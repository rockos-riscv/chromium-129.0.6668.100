// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_builder_tflite.h"

#include <cstdint>
#include <numeric>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace webnn::tflite {

namespace {

// The version number of the Schema. Ideally all changes will be backward
// compatible. If that ever changes, we must ensure that version is the first
// entry in the new tflite root so that we can see that version is not 1.
#define TFLITE_SCHEMA_VERSION (3)

// Maps a DataType to a `::tflite::TensorType`. Other `TensorTypeMap` overloads
// may be declared below as needed.
//
// Example: TensorTypeMap<uint32_t>::value -> ::tflite::TensorType_UINT32
template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
struct TensorTypeMap;

template <>
struct TensorTypeMap<float> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_FLOAT32;
};
template <>
struct TensorTypeMap<int32_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_INT32;
};
template <>
struct TensorTypeMap<uint32_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_UINT32;
};
template <>
struct TensorTypeMap<int64_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_INT64;
};

static constexpr auto kFloatDataTypes = base::MakeFixedFlatSet<OperandDataType>(
    {OperandDataType::kFloat16, OperandDataType::kFloat32});

static constexpr auto k32BitIntegerDataTypes =
    base::MakeFixedFlatSet<OperandDataType>(
        {OperandDataType::kInt32, OperandDataType::kUint32});

static constexpr auto k64BitIntegerDataTypes =
    base::MakeFixedFlatSet<OperandDataType>(
        {OperandDataType::kInt64, OperandDataType::kUint64});

// Useful for converting dimension arrays coming from mojo as uint32 to the
// int32 vectors used by TFLite.
base::expected<std::vector<int32_t>, std::string> ToSignedDimensions(
    base::span<const uint32_t> input_dimensions) {
  std::vector<int32_t> output_dimensions;
  output_dimensions.reserve(input_dimensions.size());
  for (auto dimension : input_dimensions) {
    auto checked_dimension = base::MakeCheckedNum<int32_t>(dimension);
    if (!checked_dimension.IsValid()) {
      return base::unexpected("The dimension is too large.");
    }
    output_dimensions.push_back(checked_dimension.ValueOrDie());
  }
  return output_dimensions;
}

::tflite::TensorType OperandDataTypeToTFLite(OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return ::tflite::TensorType_FLOAT32;
    case OperandDataType::kFloat16:
      return ::tflite::TensorType_FLOAT16;
    case OperandDataType::kInt32:
      return ::tflite::TensorType_INT32;
    case OperandDataType::kUint32:
      return ::tflite::TensorType_UINT32;
    case OperandDataType::kInt64:
      return ::tflite::TensorType_INT64;
    case OperandDataType::kUint64:
      return ::tflite::TensorType_UINT64;
    case OperandDataType::kInt8:
      return ::tflite::TensorType_INT8;
    case OperandDataType::kUint8:
      return ::tflite::TensorType_UINT8;
  }
}

enum class ClampRange { kRelu, kRelu1, kRelu6 };

base::expected<ClampRange, std::string> GetClampRange(
    const mojom::Clamp& clamp) {
  // TODO(crbug.com/326156496): Use RELU_0_TO_1 to support min = 0.0f and max
  // = 1.0f.
  if (clamp.min_value == -1.0f && clamp.max_value == 1.0f) {
    return ClampRange::kRelu1;
  } else if (clamp.min_value == 0.0f && clamp.max_value == 6.0f) {
    return ClampRange::kRelu6;
  } else if (clamp.min_value == 0.0f &&
             clamp.max_value == std::numeric_limits<float>::infinity()) {
    return ClampRange::kRelu;
  }

  // TODO(crbug.com/326156496): Support other range.
  return base::unexpected(
      "The range of clamp is not supported in tflite schema.");
}

::tflite::BuiltinOperator GetRecurrentNetworkActivation(
    mojom::RecurrentNetworkActivation activation) {
  switch (activation) {
    case mojom::RecurrentNetworkActivation::kRelu:
      return ::tflite::BuiltinOperator_RELU;
    case mojom::RecurrentNetworkActivation::kTanh:
      return ::tflite::BuiltinOperator_TANH;
    case mojom::RecurrentNetworkActivation::kSigmoid:
      return ::tflite::BuiltinOperator_LOGISTIC;
  }
}

struct PaddingSizes {
  uint32_t begin;
  uint32_t end;
};

// Helper to calculate the explicit padding for tflite::Padding_SAME mode with
// https://www.tensorflow.org/versions/r2.14/api_docs/python/tf/nn#notes_on_padding_2.
std::optional<PaddingSizes> CalculateExplicitPaddingForSamePaddingMode(
    uint32_t input_size,
    uint32_t filter_size,
    uint32_t stride,
    uint32_t dilation,
    bool is_transposed_conv2d) {
  auto checked_dilated_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  auto checked_input_size = base::MakeCheckedNum<uint32_t>(input_size);
  base::CheckedNumeric<uint32_t> checked_total_padding;
  if (is_transposed_conv2d) {
    // The checked_total_padding (beginningPadding + endingPadding) can be
    // calculated from the expression `outputSize = (inputSize - 1) * stride +
    // (filterSize - 1) * dilation + 1 - beginningPadding - endingPadding` that
    // is documented in the section of computing convtranspose output size:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-convtranspose2d
    checked_total_padding = (checked_input_size - 1) * stride +
                            checked_dilated_filter_size -
                            checked_input_size * stride;
  } else {
    auto checked_output_size = (checked_input_size + stride - 1) / stride;
    auto checked_needed_input_size =
        (checked_output_size - 1) * stride + checked_dilated_filter_size;
    if (!checked_needed_input_size.IsValid()) {
      return std::nullopt;
    }
    checked_total_padding = checked_needed_input_size.ValueOrDie() > input_size
                                ? checked_needed_input_size - input_size
                                : base::MakeCheckedNum<uint32_t>(0);
  }

  // Same upper padding.
  auto checked_padding_begin = checked_total_padding / 2;
  auto checked_padding_end = (checked_total_padding + 1) / 2;
  uint32_t padding_begin, padding_end;
  if (!checked_padding_begin.AssignIfValid(&padding_begin) ||
      !checked_padding_end.AssignIfValid(&padding_end)) {
    return std::nullopt;
  }
  return PaddingSizes({.begin = padding_begin, .end = padding_end});
}

struct TfLitePadding {
  ::tflite::Padding mode;
  // The explicit paddings are used to create TfLite Pad operator.
  std::optional<std::array<uint32_t, 4>> paddings;
};

// Helper to get tflite padding mode for convolution 2d or pooling 2d.
base::expected<TfLitePadding, std::string> GetTfLitePaddingMode(
    const mojom::Padding2d& padding2d,
    const webnn::Size2d<uint32_t>& input,
    const webnn::Size2d<uint32_t>& filter,
    const mojom::Size2d& stride,
    const mojom::Size2d& dilation,
    bool is_transposed_conv2d) {
  // WebNN explicit padding is in [beginning_height, ending_height,
  // beginning_width, ending_width] sequence.
  std::array<uint32_t, 4> explicit_padding = {
      padding2d.beginning->height, padding2d.ending->height,
      padding2d.beginning->width, padding2d.ending->width};
  std::array<uint32_t, 4> no_padding = {0, 0, 0, 0};
  if (explicit_padding == no_padding) {
    return TfLitePadding{.mode = ::tflite::Padding_VALID};
  }

  // Convert the explicit padding to tflite same padding mode, The TFLite PAD
  // operator need to be inserted if the calculated padding are not the same as
  // explicit padding.
  const auto padding_height = CalculateExplicitPaddingForSamePaddingMode(
      input.height, filter.height, stride.height, dilation.height,
      is_transposed_conv2d);
  const auto padding_width = CalculateExplicitPaddingForSamePaddingMode(
      input.width, filter.width, stride.width, dilation.width,
      is_transposed_conv2d);
  if (!padding_height || !padding_width) {
    return base::unexpected("Failed to calculate explicit padding.");
  }
  std::array<uint32_t, 4> upper_padding = {
      padding_height->begin, padding_height->end, padding_width->begin,
      padding_width->end};
  if (explicit_padding == upper_padding) {
    return TfLitePadding{.mode = ::tflite::Padding_SAME};
  }

  // The explicit padding are used to insert a TfLite PAD operator.
  return TfLitePadding{.mode = ::tflite::Padding_VALID,
                       .paddings = explicit_padding};
}

// Sort the indexes of the elements in the axes array based on their values and
// return the sorted index array for adding a transpose operation if needed. For
// example input shape is [2, 1, 4, 3], the shape of the scale and bias is [3,
// 1, 4] if axes is [3, 1, 2], the sorted axes would be [1, 2, 3], then the
// permutation would be (sorted indices array) [1, 2, 0].
std::vector<uint32_t> GetIndexOfSortedValue(base::span<const uint32_t> axes) {
  std::vector<uint32_t> sorted_indices(axes.size());
  std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
  base::ranges::sort(sorted_indices, base::ranges::less(),
                     [axes](uint32_t index) { return axes[index]; });
  return sorted_indices;
}

// An element in row `i` and column `j` of a matrix is in the upper-triangular
// portion if `j >= i + diagonal`. It is in the lower-triangular portion if
// `j <= i + diagonal`.
//
// This function generates an upper-triangular or lower-triangular matrix
// with the given mask value. For example, the matrices below are the upper-
// and lower-triangular [3, 3] tensors with a mask value of 1:
// [ 1, 1, 1                    [ 1, 0, 0
//   0, 1, 1,                     1, 1, 0,
//   0, 0, 1]                     1, 1, 1]
template <typename DataType>
std::vector<DataType> FillMaskTriangular(base::span<const int32_t> dimensions,
                                         bool upper,
                                         int32_t diagonal,
                                         DataType mask) {
  CHECK_EQ(dimensions.size(), 2u);
  const int32_t height = dimensions[0];
  const int32_t width = dimensions[1];
  std::vector<DataType> filled_matrix(height * width);
  for (int32_t i = 0; i < height; ++i) {
    for (int32_t j = 0; j < width; ++j) {
      // `i + diagonal` has been validated to not overflow by the caller.
      bool fill_mask_value = upper ? j >= i + diagonal : j <= i + diagonal;
      if (fill_mask_value) {
        // Get the index in the flat array with the location.
        int32_t index = (i * width) + j;
        CHECK_LT(base::checked_cast<size_t>(index), filled_matrix.size());
        filled_matrix[index] = mask;
      }
    }
  }

  return filled_matrix;
}

}  // namespace

// static
base::expected<flatbuffers::DetachedBuffer, std::string>
GraphBuilderTflite::CreateAndBuild(ContextProperties context_properties,
                                   const mojom::GraphInfo& graph_info) {
  GraphBuilderTflite builder(std::move(context_properties), graph_info);

  for (const auto& [operand_id, operand] : graph_info.id_to_operand_map) {
    RETURN_IF_ERROR(builder.SerializeOperand(operand_id, *operand));
  }

  for (const mojom::OperationPtr& operation : graph_info.operations) {
    RETURN_IF_ERROR(builder.SerializeOperation(*operation));
  }

  return builder.FinishAndTakeFlatBuffer(graph_info.input_operands,
                                         graph_info.output_operands);
}

// static
ContextProperties GraphBuilderTflite::GetContextProperties() {
  // TODO: crbug.com/345271830 - specify data types for all parameters.
  static constexpr SupportedDataTypes kFloat32{OperandDataType::kFloat32};
  static constexpr SupportedDataTypes kFloat32AndInt32To64{
      OperandDataType::kFloat32, OperandDataType::kInt32,
      OperandDataType::kInt64};
  static constexpr SupportedDataTypes kFloat32AndInts32AndInt64{
      OperandDataType::kFloat32, OperandDataType::kInt32,
      OperandDataType::kUint32, OperandDataType::kInt64};
  static constexpr SupportedDataTypes kFloat32AndInt32{
      OperandDataType::kFloat32, OperandDataType::kInt32};
  static constexpr SupportedDataTypes kFloat32AndInt32To64AndUint8{
      OperandDataType::kFloat32, OperandDataType::kInt32,
      OperandDataType::kInt64, OperandDataType::kUint8};
  static constexpr SupportedDataTypes kFloat32AndInt8To64AndUint32{
      OperandDataType::kFloat32, OperandDataType::kInt32,
      OperandDataType::kUint32, OperandDataType::kInt64,
      OperandDataType::kInt8};
  static constexpr SupportedDataTypes kFloat32AndInt8To32AndUint8{
      OperandDataType::kFloat32, OperandDataType::kInt32,
      OperandDataType::kInt8, OperandDataType::kUint8};
  static constexpr SupportedDataTypes kFloat32AndInt8To64AndUint8{
      OperandDataType::kFloat32, OperandDataType::kInt64,
      OperandDataType::kInt32, OperandDataType::kInt8, OperandDataType::kUint8};
  static constexpr SupportedDataTypes kEluSupportedDataTypes{
      OperandDataType::kFloat32, OperandDataType::kInt8};
  static constexpr SupportedDataTypes kSliceSupportedDataTypes{
      OperandDataType::kFloat32, OperandDataType::kInt64,
      OperandDataType::kInt32,   OperandDataType::kUint32,
      OperandDataType::kInt8,    OperandDataType::kUint8};
  return ContextProperties(
      InputOperandLayout::kNhwc,
      {/*input=*/SupportedDataTypes::All(),
       /*constant=*/SupportedDataTypes::All(),
       /*arg_min_max_input=*/kFloat32AndInt8To32AndUint8,
       /*arg_min_max_output=*/DataTypeConstraint::kInt32To64,
       /*concat_inputs=*/SupportedDataTypes::All(),
       /*add_input=*/kFloat32AndInt32To64,
       /*sub_input=*/kFloat32AndInt32To64,
       /*mul_input=*/kFloat32AndInts32AndInt64,
       /*div_input=*/kFloat32AndInt32,
       /*max_input=*/kFloat32AndInt32To64,
       /*min_input=*/kFloat32AndInt32To64,
       /*pow_input=*/kFloat32AndInt32,
       /*equal_input=*/kFloat32AndInt32To64AndUint8,
       /*greater_input=*/kFloat32AndInt32To64,
       /*greater_or_equal_input=*/kFloat32AndInt32To64,
       /*lesser_input=*/kFloat32AndInt32To64,
       /*lesser_or_equal_input=*/kFloat32AndInt32To64,
       /*logical_not_input=*/DataTypeConstraint::kUint8,
       /*logical_output=*/DataTypeConstraint::kUint8,
       /*abs_input=*/kFloat32AndInt32,
       /*ceil_input=*/kFloat32,
       /*cos_input=*/kFloat32,
       /*erf_input=*/kFloat32,
       /*exp_input=*/kFloat32,
       /*floor_input=*/kFloat32,
       // Identity is emulated by reshape.
       /*identity_input=*/kFloat32AndInt8To64AndUint8,
       /*log_input=*/kFloat32,
       /*neg_input=*/kFloat32AndInt32To64,
       /*reciprocal_input=*/kFloat32,
       /*sin_input=*/kFloat32,
       /*sqrt_input=*/kFloat32,
       /*tan_input=*/kFloat32,
       /*elu_input=*/kEluSupportedDataTypes,
       /*gather_input=*/kFloat32AndInt8To64AndUint8,
       /*gather_indices=*/DataTypeConstraint::kGatherIndicesSupportedDataTypes,
       /*gelu_input=*/kFloat32,
       /*leaky_relu_input=*/kFloat32,
       /*relu_input=*/kFloat32,
       /*sigmoid_input=*/kFloat32,
       /*slice_input=*/kSliceSupportedDataTypes,
       /*softmax_input=*/kFloat32,
       /*softplus_input=*/kFloat32,
       /*softsign_input=*/kFloat32,
       /*split_input=*/kFloat32AndInt8To64AndUint8,
       /*where_condition=*/DataTypeConstraint::kUint8,
       /*where_value=*/kFloat32AndInt8To64AndUint32});
}

GraphBuilderTflite::GraphBuilderTflite(ContextProperties context_properties,
                                       const mojom::GraphInfo& graph_info)
    : context_properties_(std::move(context_properties)),
      graph_info_(graph_info) {
  // TFLite requires the first entry in FlatBuffer to be an empty buffer.
  buffers_.push_back(
      ::tflite::CreateBuffer(builder_, builder_.CreateVector({})));
}

GraphBuilderTflite::~GraphBuilderTflite() = default;

base::expected<void, std::string> GraphBuilderTflite::SerializeOperand(
    uint64_t operand_id,
    const mojom::Operand& operand) {
  // The index of `tflite::Tensor` array, each `Operand` (input, constant,
  // output) will be converted and pushed back into the array, so it's increased
  // by one after each serialization in flat buffer.
  int32_t tensor_index = base::checked_cast<int32_t>(tensors_.size());
  CHECK_GE(tensor_index, 0);

  // The buffer index 0 represents input and output operand because there is no
  // data buffer associated.
  uint32_t buffer_index = 0;
  if (operand.kind == mojom::Operand::Kind::kConstant) {
    // Serialize buffer and return buffer index which starts from 1, it is
    // used to create the constant's tensor.
    buffer_index =
        SerializeBuffer(graph_info_->constant_id_to_buffer_map.at(operand_id));
  }

  // Create `Tensor` with operand shape, the index of buffer and the name.
  ASSIGN_OR_RETURN(std::vector<int32_t> signed_operand_dimensions,
                   ToSignedDimensions(operand.descriptor.shape()));
  const flatbuffers::Offset<flatbuffers::Vector<int32_t>> dimensions =
      builder_.CreateVector<int32_t>(std::move(signed_operand_dimensions));
  const auto operand_type =
      OperandDataTypeToTFLite(operand.descriptor.data_type());
  const StringOffset operand_name =
      operand.name.has_value() ? builder_.CreateString(*operand.name) : 0;
  tensors_.emplace_back(::tflite::CreateTensor(builder_, std::move(dimensions),
                                               operand_type, buffer_index,
                                               operand_name));
  operand_to_index_map_.insert({operand_id, tensor_index});
  return base::ok();
}

base::expected<void, std::string> GraphBuilderTflite::SerializeOperation(
    const mojom::Operation& op) {
  OperatorOffset operator_offset;
  switch (op.which()) {
    case mojom::Operation::Tag::kArgMinMax: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeArgMinMax(*op.get_arg_min_max()));
      break;
    }
    case mojom::Operation::Tag::kBatchNormalization: {
      ASSIGN_OR_RETURN(operator_offset, SerializeBatchNormalization(
                                            *op.get_batch_normalization()));
      break;
    }
    case mojom::Operation::Tag::kClamp: {
      ASSIGN_OR_RETURN(operator_offset, SerializeClamp(*op.get_clamp()));
      break;
    }
    case mojom::Operation::Tag::kConv2d: {
      ASSIGN_OR_RETURN(operator_offset, SerializeConv2d(*op.get_conv2d()));
      break;
    }
    case mojom::Operation::Tag::kConcat:
      operator_offset = SerializeConcat(*op.get_concat());
      break;
    case mojom::Operation::Tag::kElementWiseBinary: {
      operator_offset =
          SerializeElementWiseBinary(*op.get_element_wise_binary());
      break;
    }
    case mojom::Operation::Tag::kElementWiseUnary: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeElementWiseUnary(*op.get_element_wise_unary()));
      break;
    }
    case mojom::Operation::Tag::kElu: {
      ASSIGN_OR_RETURN(operator_offset, SerializeElu(*op.get_elu()));
      break;
    }
    case mojom::Operation::Tag::kExpand:
      operator_offset = SerializeExpand(*op.get_expand());
      break;
    case mojom::Operation::Tag::kGather: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGather(*op.get_gather()));
      break;
    }
    case mojom::Operation::Tag::kGelu: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGelu(*op.get_gelu()));
      break;
    }
    case mojom::Operation::Tag::kGemm: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGemm(*op.get_gemm()));
      break;
    }
    case mojom::Operation::Tag::kGru: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeRecurrentNetwork<mojom::Gru>(*op.get_gru()));
      break;
    }
    case mojom::Operation::Tag::kGruCell: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGruCell(*op.get_gru_cell()));
      break;
    }
    case mojom::Operation::Tag::kHardSigmoid:
      operator_offset = SerializeHardSigmoid(*op.get_hard_sigmoid());
      break;
    case mojom::Operation::Tag::kHardSwish:
      operator_offset = SerializeHardSwish(*op.get_hard_swish());
      break;
    case mojom::Operation::Tag::kInstanceNormalization: {
      ASSIGN_OR_RETURN(operator_offset, SerializeInstanceNormalization(
                                            *op.get_instance_normalization()));
      break;
    }
    case mojom::Operation::Tag::kLayerNormalization: {
      ASSIGN_OR_RETURN(operator_offset, SerializeLayerNormalization(
                                            *op.get_layer_normalization()));
      break;
    }
    case mojom::Operation::Tag::kLeakyRelu:
      operator_offset = SerializeLeakyRelu(*op.get_leaky_relu());
      break;
    case mojom::Operation::Tag::kLinear:
      operator_offset = SerializeLinear(*op.get_linear());
      break;
    case mojom::Operation::Tag::kLstmCell: {
      ASSIGN_OR_RETURN(operator_offset, SerializeLstmCell(*op.get_lstm_cell()));
      break;
    }
    case mojom::Operation::Tag::kLstm: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeRecurrentNetwork<mojom::Lstm>(*op.get_lstm()));
      break;
    }
    case mojom::Operation::Tag::kMatmul:
      operator_offset = SerializeMatmul(*op.get_matmul());
      break;
    case mojom::Operation::Tag::kPad: {
      ASSIGN_OR_RETURN(operator_offset, SerializePad(*op.get_pad()));
      break;
    }
    case mojom::Operation::Tag::kPool2d: {
      ASSIGN_OR_RETURN(operator_offset, SerializePool2d(*op.get_pool2d()));
      break;
    }
    case mojom::Operation::Tag::kPrelu: {
      ASSIGN_OR_RETURN(operator_offset, SerializePrelu(*op.get_prelu()));
      break;
    }
    case mojom::Operation::Tag::kReduce: {
      ASSIGN_OR_RETURN(operator_offset, SerializeReduce(*op.get_reduce()));
      break;
    }
    case mojom::Operation::Tag::kRelu:
      operator_offset = SerializeRelu(*op.get_relu());
      break;
    case mojom::Operation::Tag::kResample2d: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeResample2d(*op.get_resample2d()));
      break;
    }
    case mojom::Operation::Tag::kReshape: {
      const mojom::Reshape& reshape = *op.get_reshape();
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeReshape(reshape.input_operand_id,
                                        reshape.output_operand_id));
      break;
    }
    case mojom::Operation::Tag::kSigmoid:
      operator_offset = SerializeSigmoid(*op.get_sigmoid());
      break;
    case mojom::Operation::Tag::kSlice: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSlice(*op.get_slice()));
      break;
    }
    case mojom::Operation::Tag::kSoftmax:
      operator_offset = SerializeSoftmax(*op.get_softmax());
      break;
    case mojom::Operation::Tag::kSoftplus: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSoftplus(*op.get_softplus()));
      break;
    }
    case mojom::Operation::Tag::kSoftsign: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSoftsign(*op.get_softsign()));
      break;
    }
    case mojom::Operation::Tag::kSplit: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSplit(*op.get_split()));
      break;
    }
    case mojom::Operation::Tag::kTanh:
      operator_offset = SerializeTanh(*op.get_tanh());
      break;
    case mojom::Operation::Tag::kTranspose:
      operator_offset = SerializeTranspose(*op.get_transpose());
      break;
    case mojom::Operation::Tag::kTriangular: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeTriangular(*op.get_triangular()));
      break;
    }
    case mojom::Operation::Tag::kWhere:
      operator_offset = SerializeWhere(*op.get_where());
      break;
  }
  operators_.emplace_back(operator_offset);

  return base::ok();
}

flatbuffers::DetachedBuffer GraphBuilderTflite::FinishAndTakeFlatBuffer(
    base::span<const uint64_t> input_operands,
    base::span<const uint64_t> output_operands) {
  CHECK(!is_created_model_);

  int32_t* graph_input_ids = nullptr;
  auto graph_input_ids_index = builder_.CreateUninitializedVector<int32_t>(
      input_operands.size(), &graph_input_ids);
  base::ranges::transform(input_operands, graph_input_ids,
                          [&](uint64_t operand_id) {
                            return operand_to_index_map_.at(operand_id);
                          });

  int32_t* graph_output_ids = nullptr;
  auto graph_output_ids_index = builder_.CreateUninitializedVector<int32_t>(
      output_operands.size(), &graph_output_ids);
  base::ranges::transform(output_operands, graph_output_ids,
                          [&](uint64_t operand_id) {
                            return operand_to_index_map_.at(operand_id);
                          });

  // Create `tflite::SubGraph`, which typically represents an entire model.
  // The inputs of subgraph are the list of non-static tensors that feed into
  // the subgraph for inference. The outputs of subgraph are considered the
  // product of the subgraph's inference. The operators are in execution order.
  flatbuffers::Offset<::tflite::SubGraph> subgraph = ::tflite::CreateSubGraph(
      builder_, builder_.CreateVector(tensors_.data(), tensors_.size()),
      graph_input_ids_index, graph_output_ids_index,
      builder_.CreateVector(operators_.data(), operators_.size()));

  StringOffset description =
      builder_.CreateString("TFLite model converted from WebNN Graph");

  // The operator codes used in this model are kept in order because operators
  // carry an index into this std::vector.
  // There is only one subgraph in the model. The buffers of the model must be
  // initialized an empty buffer.
  flatbuffers::Offset<::tflite::Model> model_buffer = ::tflite::CreateModel(
      builder_, TFLITE_SCHEMA_VERSION,
      builder_.CreateVector(operator_codes_.data(), operator_codes_.size()),
      builder_.CreateVector(&subgraph, 1), description,
      builder_.CreateVector(buffers_.data(), buffers_.size()));

  ::tflite::FinishModelBuffer(builder_, model_buffer);
  is_created_model_ = true;

  return builder_.Release();
}

uint32_t GraphBuilderTflite::SerializeBuffer(
    const mojo_base::BigBuffer& constant) {
  const auto buffer_data =
      builder_.CreateVector(constant.data(), constant.size());
  const auto buffer_index = base::checked_cast<uint32_t>(buffers_.size());
  buffers_.emplace_back(::tflite::CreateBuffer(builder_, buffer_data));
  // The index of buffer is referenced by tensors.
  return buffer_index;
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
int32_t GraphBuilderTflite::SerializeTensorWithBuffer(
    base::span<const DataType> buffer,
    base::span<const int32_t> dimensions) {
  const auto buffer_index = base::checked_cast<uint32_t>(buffers_.size());
  const auto buffer_data =
      builder_.CreateVector<uint8_t>(base::as_byte_span(buffer));
  buffers_.emplace_back(::tflite::CreateBuffer(builder_, buffer_data));

  // Create `tflite::Tensor` with the dimensions and the index of buffer.
  const int32_t tensor_index = base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(dimensions),
      TensorTypeMap<DataType>::value, buffer_index));

  return tensor_index;
}

int32_t GraphBuilderTflite::SerializeTemporaryTensor(
    base::span<const int32_t> dimensions,
    ::tflite::TensorType tensor_type) {
  const int32_t temporary_tensor_index =
      base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(dimensions), tensor_type));

  return temporary_tensor_index;
}

uint32_t GraphBuilderTflite::GetOperatorCodeIndex(
    ::tflite::BuiltinOperator code,
    int32_t version) {
  // New builtin operators, whose operator code is larger than 127, can not be
  // assigned to the `deprecated_code` field. In such cases, the value of the
  // `code` field should be used for the builtin operator code, the value 127
  // will be the value of the `deprecated_code`.
  const ::tflite::BuiltinOperator deprecated_code = std::min(
      code, ::tflite::BuiltinOperator_PLACEHOLDER_FOR_GREATER_OP_CODES);

  auto operator_code_index =
      base::checked_cast<uint32_t>(operator_codes_.size());
  operator_codes_.push_back(::tflite::CreateOperatorCode(
      builder_, base::checked_cast<int8_t>(deprecated_code),
      /*custom_code=*/0, version, code));

  // The type of operation is determined by the index into the list of the valid
  // OperatorCodes.
  return operator_code_index;
}

const mojom::Operand& GraphBuilderTflite::GetOperand(
    uint64_t operand_id) const {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

auto GraphBuilderTflite::SerializeUnaryOperation(
    ::tflite::BuiltinOperator code,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    ::tflite::BuiltinOptions builtin_options_type,
    flatbuffers::Offset<void> builtin_options) -> OperatorOffset {
  CHECK_EQ(builtin_options_type == ::tflite::BuiltinOptions_NONE,
           builtin_options.IsNull());

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index = GetOperatorCodeIndex(code);
  const std::array<int32_t, 1> op_inputs = {input_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializeCastOperation(
    int32_t input_tensor_index,
    ::tflite::TensorType input_tensor_type,
    int32_t output_tensor_index,
    ::tflite::TensorType output_tensor_type) -> OperatorOffset {
  const auto cast_options = ::tflite::CreateCastOptions(
      builder_, input_tensor_type, output_tensor_type);

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_CAST);
  const std::array<int32_t, 1> op_inputs = {input_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_CastOptions, cast_options.Union());
}

auto GraphBuilderTflite::SerializeBinaryOperation(
    ::tflite::BuiltinOperator code,
    int32_t lhs_tensor_index,
    int32_t rhs_tensor_index,
    int32_t output_tensor_index) -> OperatorOffset {
  const uint32_t operator_code_index = GetOperatorCodeIndex(code);
  const std::array<int32_t, 2> op_inputs = {lhs_tensor_index, rhs_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeConcatOperation(
    base::span<const int32_t> input_tensor_indices,
    int32_t output_tensor_index,
    uint32_t axis) -> OperatorOffset {
  // Create `tflite::ConcatenationOptions` with axis.
  const auto concat_options =
      ::tflite::CreateConcatenationOptions(builder_, axis);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_CONCATENATION);
  const std::array<int32_t, 1> operator_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<int32_t>(input_tensor_indices),
      builder_.CreateVector<int32_t>(operator_outputs),
      ::tflite::BuiltinOptions_ConcatenationOptions, concat_options.Union());
}

auto GraphBuilderTflite::SerializeMatmulOperation(int32_t a_tensor_index,
                                                  int32_t b_tensor_index,
                                                  int32_t output_tensor_index)
    -> OperatorOffset {
  const auto matmul_options =
      ::tflite::CreateBatchMatMulOptions(builder_, /*adj_x=*/false,
                                         /*adj_y=*/false);
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_BATCH_MATMUL);
  const std::array<int32_t, 2> op_inputs = {a_tensor_index, b_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_BatchMatMulOptions, matmul_options.Union());
}

auto GraphBuilderTflite::SerializeLinearOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    float alpha,
    float beta) -> OperatorOffset {
  // Emulate a linear operation whose calculation follows the expression `alpha
  // * x + beta`.
  const int32_t alpha_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{alpha},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_mul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, input_tensor_index, alpha_tensor_index,
      output_tensor_index_of_mul));

  const int32_t beta_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{beta},
      /*dimensions=*/{});
  return SerializeBinaryOperation(::tflite::BuiltinOperator_ADD,
                                  beta_tensor_index, output_tensor_index_of_mul,
                                  output_tensor_index);
}

auto GraphBuilderTflite::SerializeNormalizationOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    int32_t mean_tensor_index,
    int32_t variance_tensor_index,
    float epsilon,
    std::optional<int32_t> scale_tensor_index,
    std::optional<int32_t> bias_tensor_index) -> OperatorOffset {
  // Emulate normalization follows the expression `Scale * ((Input - Mean) /
  // sqrt(Variance + Epsilon)) + Bias`
  //
  // Serialize the subtraction operation for expression `Input - Mean`.
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  const int32_t output_tensor_index_of_sub =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, input_tensor_index, mean_tensor_index,
      output_tensor_index_of_sub));

  // Serialize the subexpression `sqrt(Variance + Epsilon)`.
  const int32_t epsilon_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{epsilon},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_add =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, variance_tensor_index,
      epsilon_tensor_index, output_tensor_index_of_add));
  const int32_t output_tensor_index_of_sqrt =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(
      ::tflite::BuiltinOperator_SQRT, output_tensor_index_of_add,
      output_tensor_index_of_sqrt));

  // Serialize the intermediate expression `Scale * (output_tensor_of_sub /
  // output_tensor_of_sqrt)`.
  int32_t output_tensor_index_of_div = output_tensor_index;
  if (scale_tensor_index || bias_tensor_index) {
    output_tensor_index_of_div =
        SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  }
  OperatorOffset normalization_offset = SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, output_tensor_index_of_sub,
      output_tensor_index_of_sqrt, output_tensor_index_of_div);
  int32_t output_tensor_index_of_mul = output_tensor_index_of_div;
  if (scale_tensor_index) {
    operators_.emplace_back(normalization_offset);
    output_tensor_index_of_mul =
        bias_tensor_index
            ? SerializeTemporaryTensor(input_dimensions, input_tensor_type)
            : output_tensor_index;
    normalization_offset = SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, *scale_tensor_index,
        output_tensor_index_of_div, output_tensor_index_of_mul);
  }

  if (bias_tensor_index) {
    operators_.emplace_back(normalization_offset);
    normalization_offset = SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, output_tensor_index_of_mul,
        *bias_tensor_index, output_tensor_index);
  }

  return normalization_offset;
}

auto GraphBuilderTflite::SerializeReduceOperation(
    ::tflite::BuiltinOperator operator_code,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    base::span<const int32_t> axes,
    bool keep_dimensions) -> OperatorOffset {
  const std::array<int32_t, 1> axes_tensor_shape = {
      base::checked_cast<int32_t>(axes.size())};
  const int32_t axes_tensor_index =
      SerializeTensorWithBuffer<int32_t>(axes, axes_tensor_shape);

  const auto reduce_options =
      ::tflite::CreateReducerOptions(builder_, keep_dimensions);
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {input_tensor_index,
                                            axes_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_ReducerOptions, reduce_options.Union());
}

auto GraphBuilderTflite::SerializeReshapeOperation(
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    base::span<const int32_t> new_shape) -> OperatorOffset {
  const auto reshape_options = ::tflite::CreateReshapeOptions(
      builder_,
      /*new_shape=*/builder_.CreateVector<int32_t>(new_shape));

  return SerializeUnaryOperation(::tflite::BuiltinOperator_RESHAPE,
                                 input_tensor_index, output_tensor_index,
                                 ::tflite::BuiltinOptions_ReshapeOptions,
                                 reshape_options.Union());
}

auto GraphBuilderTflite::SerializeSliceOperation(
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    base::span<const int32_t> slice_starts,
    base::span<const int32_t> slice_sizes)
    -> base::expected<OperatorOffset, std::string> {
  CHECK_EQ(slice_starts.size(), slice_sizes.size());
  // Serialize the starting index of each input dimension.
  auto checked_number = base::MakeCheckedNum<int32_t>(slice_starts.size());
  if (!checked_number.IsValid()) {
    return base::unexpected("The number of starts and sizes is too large.");
  }
  const std::array<int32_t, 1> starts_and_sizes_shape = {
      checked_number.ValueOrDie()};
  const int32_t starts_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(slice_starts), starts_and_sizes_shape);

  // Serialize the number of elements to slice each input dimension.
  const int32_t sizes_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(slice_sizes), starts_and_sizes_shape);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SLICE);
  const std::array<int32_t, 3> op_inputs = {
      input_tensor_index, starts_tensor_index, sizes_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeTransposeOperation(
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    base::span<const uint32_t> permutation) -> OperatorOffset {
  const std::array<int32_t, 1> permutation_shape = {
      base::checked_cast<int32_t>(permutation.size())};
  const int32_t permutation_tensor_index =
      SerializeTensorWithBuffer<uint32_t>(permutation, permutation_shape);

  const auto operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_TRANSPOSE);
  const std::array<int32_t, 2> op_inputs = {input_tensor_index,
                                            permutation_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::InsertPadOperation(const mojom::Operand& input_operand,
                                            int32_t input_tensor_index,
                                            base::span<const uint32_t> paddings)
    -> base::expected<int32_t, std::string> {
  // WebNN explicit padding is in [beginning_height, ending_height,
  // beginning_width, ending_width] sequence.
  const auto padding_rank = paddings.size();
  CHECK_EQ(padding_rank, 4u);

  // Create `tflite::Tensor` for the output operand of explicit padding operator
  // with the dimensions and data type.
  CHECK_EQ(input_operand.descriptor.Rank(), 4u);
  std::vector<int32_t> output_shape;
  output_shape.reserve(padding_rank);
  for (size_t i = 0; i < padding_rank; ++i) {
    auto checked_dimension =
        base::MakeCheckedNum<int32_t>(input_operand.descriptor.shape()[i]);
    // Calculate output height with padding beginning and ending height.
    if (i == 1) {
      checked_dimension +=
          base::MakeCheckedNum<int32_t>(paddings[0]) + paddings[1];
    } else if (i == 2) {
      // Calculate output width with padding beginning and ending width.
      checked_dimension +=
          base::MakeCheckedNum<int32_t>(paddings[2]) + paddings[3];
    }
    if (!checked_dimension.IsValid()) {
      return base::unexpected("The input dimension or padding is too large.");
    }
    output_shape.push_back(checked_dimension.ValueOrDie());
  }

  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const int32_t output_tensor_index =
      base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(output_shape),
      input_tensor_type));

  // TfLite padding is a signed integer tensor array filled with pre and post
  // padding. For NHWC input layout, the sequence will be [[0, 0],
  // [beginning_height, ending_height], [beginning_width, ending_width], [0,
  // 0]].
  std::array<int32_t, 8> tflite_paddings = {};
  base::ranges::copy(paddings, tflite_paddings.begin() + 2);

  // The shape of padding is [n, 2], where n is the rank of input as described
  // here https://www.tensorflow.org/mlir/tfl_ops#tflmirror_pad_tflmirrorpadop.
  std::array<int32_t, 2> paddings_shape = {
      base::checked_cast<int32_t>(padding_rank), 2};
  const int32_t padding_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(tflite_paddings), std::move(paddings_shape));

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_PAD);
  std::array<int32_t, 2> op_inputs = {input_tensor_index, padding_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs)));

  return output_tensor_index;
}

int32_t GraphBuilderTflite::InsertTransposeOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const uint32_t> permutation) {
  // Create `tflite::Tensor` for the output operand of Transpose operator with
  // the dimensions and tensor data type.
  const size_t input_rank = input_dimensions.size();
  CHECK_EQ(permutation.size(), input_rank);
  std::vector<int32_t> output_shape;
  output_shape.reserve(input_rank);
  for (size_t i = 0; i < input_rank; ++i) {
    output_shape.push_back(input_dimensions[permutation[i]]);
  }
  const int32_t output_tensor_index =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeTransposeOperation(
      input_tensor_index, output_tensor_index, permutation));

  return output_tensor_index;
}

int32_t GraphBuilderTflite::SerializeSubGraphPowMul(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    float pow_exponent,
    float mul_alpha) {
  const int32_t output_tensor_index_of_pow =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  const int32_t pow_exponent_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{pow_exponent},
      /*dimensions=*/{});
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW, input_tensor_index,
      pow_exponent_tensor_index, output_tensor_index_of_pow));

  const int32_t output_tensor_index_of_mul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  const int32_t mul_alpha_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{mul_alpha},
      /*dimensions=*/{});
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_tensor_index_of_pow,
      mul_alpha_tensor_index, output_tensor_index_of_mul));

  return output_tensor_index_of_mul;
}

auto GraphBuilderTflite::SerializeArgMinMax(const mojom::ArgMinMax& arg_min_max)
    -> base::expected<OperatorOffset, std::string> {
  // The WebNN axis option is uint32 data type, but TFLite axis needs int32
  // type, so the axis need to be validated here to not overflow.
  auto checked_axis = base::MakeCheckedNum<int32_t>(arg_min_max.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis in arg_min_max operation is too large.");
  }
  const std::array<int32_t, 1> axis_buffer = {checked_axis.ValueOrDie()};
  const std::array<int32_t, 1> axis_dimensions = {axis_buffer.size()};
  const int32_t axis_tensor_index =
      SerializeTensorWithBuffer<int32_t>(axis_buffer, axis_dimensions);

  ::tflite::BuiltinOperator operator_code;
  ::tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  const mojom::Operand& output_operand =
      GetOperand(arg_min_max.output_operand_id);

  ::tflite::TensorType output_type = ::tflite::TensorType_INT64;
  if (output_operand.descriptor.data_type() == OperandDataType::kInt32) {
    output_type = ::tflite::TensorType_INT32;
  } else {
    CHECK_EQ(output_operand.descriptor.data_type(), OperandDataType::kInt64);
  }
  switch (arg_min_max.kind) {
    case mojom::ArgMinMax::Kind::kMax: {
      operator_code = ::tflite::BuiltinOperator_ARG_MAX;
      builtin_options_type = ::tflite::BuiltinOptions_ArgMaxOptions;
      builtin_options =
          ::tflite::CreateArgMaxOptions(builder_, output_type).Union();
      break;
    }
    case mojom::ArgMinMax::Kind::kMin: {
      operator_code = ::tflite::BuiltinOperator_ARG_MIN;
      builtin_options_type = ::tflite::BuiltinOptions_ArgMinOptions;
      builtin_options =
          ::tflite::CreateArgMinOptions(builder_, output_type).Union();
      break;
    }
  }

  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(arg_min_max.input_operand_id),
      axis_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(arg_min_max.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializeBatchNormalization(
    const mojom::BatchNormalization& batch_normalization)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand =
      GetOperand(batch_normalization.input_operand_id);
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  if (input_operand.descriptor.data_type() == OperandDataType::kFloat16) {
    return base::unexpected("The 16-bit float data type is not supported.");
  }
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  CHECK_LT(batch_normalization.axis, signed_input_dimensions->size());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const int32_t dimension_on_axis =
      (*signed_input_dimensions)[batch_normalization.axis];
  std::vector<int32_t> new_shape(signed_input_dimensions->size(), 1);
  new_shape[batch_normalization.axis] = dimension_on_axis;

  // Reshape the 1-D tensor of the mean operand to the new shape.
  const mojom::Operand& mean_operand =
      GetOperand(batch_normalization.mean_operand_id);
  CHECK_EQ(mean_operand.descriptor.Rank(), 1u);
  const int32_t reshape_mean_tensor_index =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      operand_to_index_map_.at(batch_normalization.mean_operand_id),
      reshape_mean_tensor_index, new_shape));

  // Reshape the 1-D tensor of the variance operand to the new shape.
  const mojom::Operand& variance_operand =
      GetOperand(batch_normalization.variance_operand_id);
  CHECK_EQ(variance_operand.descriptor.Rank(), 1u);
  const int32_t reshape_variance_tensor_index =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      operand_to_index_map_.at(batch_normalization.variance_operand_id),
      reshape_variance_tensor_index, new_shape));

  // Reshape the 1-D tensor of the scale operand to the new shape if needed.
  std::optional<int32_t> reshape_scale_tensor_index;
  if (batch_normalization.scale_operand_id) {
    const mojom::Operand& scale_operand =
        GetOperand(*batch_normalization.scale_operand_id);
    CHECK_EQ(scale_operand.descriptor.Rank(), 1u);
    reshape_scale_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        operand_to_index_map_.at(*batch_normalization.scale_operand_id),
        *reshape_scale_tensor_index, new_shape));
  }

  // Reshape the 1-D tensor of the bias operand to the new shape if needed.
  std::optional<int32_t> reshape_bias_tensor_index;
  if (batch_normalization.bias_operand_id) {
    const mojom::Operand& bias_operand =
        GetOperand(*batch_normalization.bias_operand_id);
    CHECK_EQ(bias_operand.descriptor.Rank(), 1u);
    reshape_bias_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        operand_to_index_map_.at(*batch_normalization.bias_operand_id),
        *reshape_bias_tensor_index, new_shape));
  }

  return SerializeNormalizationOperation(
      new_shape, input_tensor_type,
      operand_to_index_map_.at(batch_normalization.input_operand_id),
      operand_to_index_map_.at(batch_normalization.output_operand_id),
      reshape_mean_tensor_index, reshape_variance_tensor_index,
      batch_normalization.epsilon, reshape_scale_tensor_index,
      reshape_bias_tensor_index);
}

auto GraphBuilderTflite::SerializeClamp(const mojom::Clamp& clamp)
    -> base::expected<OperatorOffset, std::string> {
  ASSIGN_OR_RETURN(ClampRange clamp_range, GetClampRange(clamp));
  ::tflite::BuiltinOperator code;
  switch (clamp_range) {
    case ClampRange::kRelu:
      code = ::tflite::BuiltinOperator_RELU;
      break;
    case ClampRange::kRelu1:
      code = ::tflite::BuiltinOperator_RELU_N1_TO_1;
      break;
    case ClampRange::kRelu6:
      code = ::tflite::BuiltinOperator_RELU6;
      break;
  }

  return SerializeUnaryOperation(
      code, operand_to_index_map_.at(clamp.input_operand_id),
      operand_to_index_map_.at(clamp.output_operand_id));
}

auto GraphBuilderTflite::SerializeConcat(const mojom::Concat& concat)
    -> OperatorOffset {
  std::vector<int32_t> operator_inputs_index;
  operator_inputs_index.reserve(concat.input_operand_ids.size());
  base::ranges::transform(concat.input_operand_ids,
                          std::back_inserter(operator_inputs_index),
                          [&](uint64_t operand_id) {
                            return operand_to_index_map_.at(operand_id);
                          });

  return SerializeConcatOperation(
      operator_inputs_index, operand_to_index_map_.at(concat.output_operand_id),
      concat.axis);
}

auto GraphBuilderTflite::SerializeConv2d(const mojom::Conv2d& conv2d)
    -> base::expected<OperatorOffset, std::string> {
  // TFLite schema doesn't support dilations and groups, they are being
  // discussed in the issues
  // https://github.com/tensorflow/tensorflow/issues/70031
  // https://github.com/tensorflow/tensorflow/issues/69201
  if (conv2d.kind == mojom::Conv2d::Kind::kTransposed &&
      (conv2d.dilations->height != 1 || conv2d.dilations->width != 1 ||
       conv2d.groups != 1)) {
    return base::unexpected(
        "convTranspose2d doesn't support dilations and groups.");
  }

  const mojom::Operand& input_operand = GetOperand(conv2d.input_operand_id);
  // TODO(crbug.com/328733319): Support other tensor data types.
  if (input_operand.descriptor.data_type() != OperandDataType::kFloat32) {
    return base::unexpected("The data type of input is not supported.");
  }

  // Get tflite padding mode with the size2d of input, filter, dilation.
  const auto& input_shape = input_operand.descriptor.shape();
  CHECK_EQ(input_shape.size(), 4u);
  const uint32_t input_channels = input_shape[3];
  const mojom::Operand& output_operand = GetOperand(conv2d.output_operand_id);
  const auto& output_shape = output_operand.descriptor.shape();
  CHECK_EQ(output_shape.size(), 4u);
  const uint32_t output_channels = output_shape[3];
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_shape[1],
                                                .width = input_shape[2]};
  // For nhwc input layout, the default filter layout is ohwi for
  // regular/transpose conv2d and ihwo for depthwise conv2d.
  const mojom::Operand& filter_operand = GetOperand(conv2d.filter_operand_id);
  CHECK_EQ(filter_operand.descriptor.Rank(), 4u);
  const auto& filter_shape = filter_operand.descriptor.shape();
  CHECK_EQ(filter_shape.size(), 4u);
  const webnn::Size2d<uint32_t> filter_size2d = {.height = filter_shape[1],
                                                 .width = filter_shape[2]};
  ASSIGN_OR_RETURN(
      TfLitePadding padding_mode,
      GetTfLitePaddingMode(*conv2d.padding, input_size2d, filter_size2d,
                           *conv2d.strides, *conv2d.dilations,
                           conv2d.kind == mojom::Conv2d::Kind::kTransposed));
  const int32_t input_index = operand_to_index_map_.at(conv2d.input_operand_id);
  // Insert a Pad operator before TfLite Conv2d if needed for explicit padding.
  std::optional<int32_t> explicit_pad_index;
  if (padding_mode.paddings) {
    ASSIGN_OR_RETURN(explicit_pad_index,
                     InsertPadOperation(input_operand, input_index,
                                        padding_mode.paddings.value()));
  }

  // If there is no bias operand, serialize a empty buffer with the size of
  // output channel.
  int32_t bias_index;
  if (conv2d.bias_operand_id) {
    bias_index = operand_to_index_map_.at(conv2d.bias_operand_id.value());
  } else {
    const std::array<int32_t, 1> bias_shape = {
        base::checked_cast<int32_t>(output_channels)};
    bias_index = SerializeTensorWithBuffer<float>(
        std::vector<float>(output_channels), std::move(bias_shape));
  }

  // TODO(crbug.com/344633746): Consider fusing Conv2D activations when
  // possible.

  std::vector<int32_t> op_inputs;
  ::tflite::BuiltinOperator operator_kind;
  ::tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  if (conv2d.kind == mojom::Conv2d::Kind::kDirect) {
    op_inputs = {explicit_pad_index ? explicit_pad_index.value() : input_index,
                 operand_to_index_map_.at(conv2d.filter_operand_id),
                 bias_index};
    if (webnn::IsDepthwiseConv2d(input_channels, output_channels,
                                 conv2d.groups)) {
      operator_kind = ::tflite::BuiltinOperator_DEPTHWISE_CONV_2D;
      builtin_options = ::tflite::CreateDepthwiseConv2DOptions(
                            builder_, padding_mode.mode, conv2d.strides->width,
                            conv2d.strides->height, /*depth_multiplier=*/1,
                            ::tflite::ActivationFunctionType_NONE,
                            conv2d.dilations->width, conv2d.dilations->height)
                            .Union();
      builtin_options_type = ::tflite::BuiltinOptions_DepthwiseConv2DOptions;
    } else {
      operator_kind = ::tflite::BuiltinOperator_CONV_2D;
      builtin_options =
          ::tflite::CreateConv2DOptions(
              builder_, padding_mode.mode, conv2d.strides->width,
              conv2d.strides->height, ::tflite::ActivationFunctionType_NONE,
              conv2d.dilations->width, conv2d.dilations->height)
              .Union();
      builtin_options_type = ::tflite::BuiltinOptions_Conv2DOptions;
    }
  } else {
    const auto signed_output_dimensions = ToSignedDimensions(output_shape);
    CHECK(signed_output_dimensions.has_value());
    const std::array<int32_t, 1> output_tensor_shape = {
        base::checked_cast<int32_t>(output_shape.size())};
    const int32_t output_shape_tensor_index =
        SerializeTensorWithBuffer<int32_t>(*signed_output_dimensions,
                                           output_tensor_shape);
    op_inputs = {output_shape_tensor_index,
                 operand_to_index_map_.at(conv2d.filter_operand_id),
                 explicit_pad_index ? explicit_pad_index.value() : input_index,
                 bias_index};
    operator_kind = ::tflite::BuiltinOperator_TRANSPOSE_CONV;
    builtin_options =
        ::tflite::CreateTransposeConvOptions(
            builder_, padding_mode.mode, conv2d.strides->width,
            conv2d.strides->height, ::tflite::ActivationFunctionType_NONE)
            .Union();
    builtin_options_type = ::tflite::BuiltinOptions_TransposeConvOptions;
  }
  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(operator_kind);
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(conv2d.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializeElementWiseBinary(
    const mojom::ElementWiseBinary& op) -> OperatorOffset {
  const OperandDataType input_data_type =
      GetOperand(op.lhs_operand_id).descriptor.data_type();
  ::tflite::BuiltinOperator code;
  switch (op.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      CHECK(
          context_properties_.data_type_limits.add_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_ADD;
      break;
    case mojom::ElementWiseBinary::Kind::kSub:
      CHECK(
          context_properties_.data_type_limits.sub_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_SUB;
      break;
    case mojom::ElementWiseBinary::Kind::kMul:
      CHECK(
          context_properties_.data_type_limits.mul_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_MUL;
      break;
    case mojom::ElementWiseBinary::Kind::kDiv:
      CHECK(
          context_properties_.data_type_limits.div_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_DIV;
      break;
    case mojom::ElementWiseBinary::Kind::kMax:
      CHECK(
          context_properties_.data_type_limits.max_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_MAXIMUM;
      break;
    case mojom::ElementWiseBinary::Kind::kMin:
      CHECK(
          context_properties_.data_type_limits.min_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_MINIMUM;
      break;
    case mojom::ElementWiseBinary::Kind::kPow:
      CHECK(
          context_properties_.data_type_limits.pow_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_POW;
      break;
    case mojom::ElementWiseBinary::Kind::kEqual:
      CHECK(context_properties_.data_type_limits.equal_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kGreater:
      CHECK(context_properties_.data_type_limits.greater_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_GREATER;
      break;
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      CHECK(context_properties_.data_type_limits.greater_or_equal_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_GREATER_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kLesser:
      CHECK(context_properties_.data_type_limits.lesser_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_LESS;
      break;
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
      CHECK(context_properties_.data_type_limits.lesser_or_equal_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_LESS_EQUAL;
      break;
  }

  return SerializeBinaryOperation(
      code, operand_to_index_map_.at(op.lhs_operand_id),
      operand_to_index_map_.at(op.rhs_operand_id),
      operand_to_index_map_.at(op.output_operand_id));
}

auto GraphBuilderTflite::SerializeElementWiseUnary(
    const mojom::ElementWiseUnary& op)
    -> base::expected<OperatorOffset, std::string> {
  const int32_t input_tensor_index =
      operand_to_index_map_.at(op.input_operand_id);
  const int32_t output_tensor_index =
      operand_to_index_map_.at(op.output_operand_id);
  const OperandDataType input_data_type =
      GetOperand(op.input_operand_id).descriptor.data_type();
  switch (op.kind) {
    case mojom::ElementWiseUnary::Kind::kAbs: {
      CHECK(
          context_properties_.data_type_limits.abs_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kCast: {
      return SerializeCastOperation(
          input_tensor_index,
          OperandDataTypeToTFLite(
              GetOperand(op.input_operand_id).descriptor.data_type()),
          output_tensor_index,
          OperandDataTypeToTFLite(
              GetOperand(op.output_operand_id).descriptor.data_type()));
    }
    case mojom::ElementWiseUnary::Kind::kCeil: {
      CHECK(
          context_properties_.data_type_limits.ceil_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_CEIL,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kCos: {
      CHECK(
          context_properties_.data_type_limits.cos_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_COS,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kExp: {
      CHECK(
          context_properties_.data_type_limits.exp_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kFloor: {
      CHECK(context_properties_.data_type_limits.floor_input.Has(
          input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_FLOOR,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kIdentity: {
      CHECK(context_properties_.data_type_limits.identity_input.Has(
          input_data_type));
      // Implement WebNN identity operation with TFLite reshape operator, the
      // output shape is the same as input.
      // TODO(crbug.com/336399247): Skip identity implementation with
      // redirecting output tensor to input.
      return SerializeReshape(op.input_operand_id, op.output_operand_id);
    }
    case mojom::ElementWiseUnary::Kind::kLog: {
      CHECK(
          context_properties_.data_type_limits.log_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_LOG,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kLogicalNot: {
      CHECK(context_properties_.data_type_limits.logical_not_input.Has(
          input_data_type));
      return SerializeLogicalNot(op);
    }
    case mojom::ElementWiseUnary::Kind::kNeg: {
      CHECK(
          context_properties_.data_type_limits.neg_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_NEG,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kReciprocal: {
      CHECK(context_properties_.data_type_limits.reciprocal_input.Has(
          input_data_type));
      return SerializeReciprocal(op);
    }
    case mojom::ElementWiseUnary::Kind::kSin: {
      CHECK(
          context_properties_.data_type_limits.sin_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SIN,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kSqrt: {
      CHECK(
          context_properties_.data_type_limits.sqrt_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SQRT,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kTan: {
      CHECK(
          context_properties_.data_type_limits.tan_input.Has(input_data_type));
      return SerializeTan(op);
    }
    case mojom::ElementWiseUnary::Kind::kErf: {
      CHECK(
          context_properties_.data_type_limits.erf_input.Has(input_data_type));
      return SerializeErf(op);
    }
  }
}

auto GraphBuilderTflite::SerializeElu(const mojom::Elu& elu)
    -> base::expected<OperatorOffset, std::string> {
  if (elu.alpha != 1.0) {
    // TODO: crbug.com/328736354 - Support custom alpha values.
    return base::unexpected(
        "Setting a custom alpha is not supported in tflite schema.");
  }
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_ELU,
      operand_to_index_map_.at(elu.input_operand_id),
      operand_to_index_map_.at(elu.output_operand_id));
}

auto GraphBuilderTflite::SerializeErf(const mojom::ElementWiseUnary& erf)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand = GetOperand(erf.input_operand_id);
  const OperandDataType input_data_type = input_operand.descriptor.data_type();
  CHECK(context_properties_.data_type_limits.erf_input.Has(input_data_type));
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_data_type);

  // Emulate the erf operation with the expression `erf(x) = sign(x) * (1 - (a1
  // * t + a2 * pow(t, 2) + ... + a5 * pow(t, 5)) * exp(-pow(x, 2)))`, the `t`
  // is the subexpression `1 / (1 + p * |x|)` as documented here:
  // https://en.wikipedia.org/wiki/Error_function
  const std::array<float, 5> constants = {/*a1*/ 0.254829592,
                                          /*a2*/ -0.284496736,
                                          /*a3*/ 1.421413741,
                                          /*a4*/ -1.453152027,
                                          /*a5*/ 1.061405429};
  const float p = 0.3275911;

  // Compute the subexpression `t = 1 / (1 + p * |x|)`.
  const int32_t input_tensor_index =
      operand_to_index_map_.at(erf.input_operand_id);
  const int32_t output_tensor_index_of_abs =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                                  input_tensor_index,
                                                  output_tensor_index_of_abs));
  const int32_t output_tensor_index_of_line =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeLinearOperation(
      *signed_input_dimensions, input_tensor_type, output_tensor_index_of_abs,
      output_tensor_index_of_line, p, 1.0));
  const int32_t constant_one_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1.0},
      /*dimensions=*/{});
  const int32_t t_expression_tensor_index =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, constant_one_tensor_index,
      output_tensor_index_of_line, t_expression_tensor_index));

  // Compute subexpression `(a1 * t + a2 * pow(t, 2) + ... + a5 * pow(t, 5))`.
  std::optional<int32_t> sum_pow_mul_tensor_index;
  for (size_t i = 0; i < constants.size(); ++i) {
    const int32_t output_tensor_index_of_pow_mul = SerializeSubGraphPowMul(
        *signed_input_dimensions, input_tensor_type, t_expression_tensor_index,
        /*pow_exponent=*/i + 1,
        /*mul_alpha=*/constants[i]);
    if (sum_pow_mul_tensor_index) {
      const int32_t output_tensor_index_of_add =
          SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
      operators_.emplace_back(SerializeBinaryOperation(
          ::tflite::BuiltinOperator_ADD, output_tensor_index_of_pow_mul,
          *sum_pow_mul_tensor_index, output_tensor_index_of_add));
      sum_pow_mul_tensor_index = output_tensor_index_of_add;
    } else {
      sum_pow_mul_tensor_index = output_tensor_index_of_pow_mul;
    }
  }

  // Compute the subexpression `exp(-pow(x, 2))`.
  const int32_t output_tensor_index_of_pow =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  const int32_t pow_exponent_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{2.0},
      /*dimensions=*/{});
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW, input_tensor_index,
      pow_exponent_tensor_index, output_tensor_index_of_pow));
  const int32_t output_tensor_index_of_neg =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_NEG,
                                                  output_tensor_index_of_pow,
                                                  output_tensor_index_of_neg));
  const int32_t output_tensor_index_of_exp =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                                  output_tensor_index_of_neg,
                                                  output_tensor_index_of_exp));

  // Compute `1 - (the sum of pow mul subexpression) * (the pow exp
  // subexpression)`.
  const int32_t output_tensor_index_of_mul =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_tensor_index_of_exp,
      *sum_pow_mul_tensor_index, output_tensor_index_of_mul));
  const int32_t output_tensor_index_of_sub =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, constant_one_tensor_index,
      output_tensor_index_of_mul, output_tensor_index_of_sub));

  // Compute the subexpression `sign = sign(x)`
  const int32_t output_tensor_index_of_sign =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(
      SerializeUnaryOperation(::tflite::BuiltinOperator_SIGN,
                              input_tensor_index, output_tensor_index_of_sign));

  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_tensor_index_of_sign,
      output_tensor_index_of_sub,
      operand_to_index_map_.at(erf.output_operand_id));
}

auto GraphBuilderTflite::SerializeExpand(const mojom::Expand& expand)
    -> OperatorOffset {
  // Serialize the expanded shape to tflite tensor with output dimensions.
  const mojom::Operand& output_operand = GetOperand(expand.output_operand_id);
  // The output shape has been validated to not overflow before creating tensor.
  const auto signed_output_dimensions =
      ToSignedDimensions(output_operand.descriptor.shape());
  CHECK(signed_output_dimensions.has_value());
  const int32_t output_rank =
      base::checked_cast<int32_t>(signed_output_dimensions->size());
  const int32_t new_shape_tensor_index = SerializeTensorWithBuffer<int32_t>(
      *signed_output_dimensions, std::array<int32_t, 1>{output_rank});

  const uint32_t operator_code_index = GetOperatorCodeIndex(
      ::tflite::BuiltinOperator_BROADCAST_TO, /*version=*/2);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(expand.input_operand_id),
      new_shape_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(expand.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeGather(const mojom::Gather& gather)
    -> base::expected<OperatorOffset, std::string> {
  // The WebNN indices must be one of type uint32, int32, int64, but TFLite
  // indices need int32 or int64 type, so a cast operation need to be inserted
  // before Gather if indices data type is uint32.
  int32_t indices_tensor_index =
      operand_to_index_map_.at(gather.indices_operand_id);
  const mojom::Operand& indices_operand = GetOperand(gather.indices_operand_id);
  if (indices_operand.descriptor.data_type() == OperandDataType::kUint32) {
    ASSIGN_OR_RETURN(const std::vector<int32_t> signed_indices_dimensions,
                     ToSignedDimensions(indices_operand.descriptor.shape()));
    indices_tensor_index = SerializeTemporaryTensor(signed_indices_dimensions,
                                                    ::tflite::TensorType_INT64);

    operators_.emplace_back(SerializeCastOperation(
        operand_to_index_map_.at(gather.indices_operand_id),
        /*input_tensor_type=*/::tflite::TensorType_UINT32, indices_tensor_index,
        /*output_tensor_type=*/::tflite::TensorType_INT64));
  } else {
    CHECK(indices_operand.descriptor.data_type() == OperandDataType::kInt64 ||
          indices_operand.descriptor.data_type() == OperandDataType::kInt32);
  }

  // The WebNN axis option is uint32 data type, but TFLite axis needs int32
  // type, so the axis need to be validated here to not overflow.
  auto checked_axis = base::MakeCheckedNum<int32_t>(gather.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis in gather operation is too large.");
  }
  const auto gather_options =
      ::tflite::CreateGatherOptions(builder_, checked_axis.ValueOrDie());

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_GATHER);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(gather.input_operand_id), indices_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(gather.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_GatherOptions, gather_options.Union());
}

auto GraphBuilderTflite::SerializeGelu(const mojom::Gelu& gelu)
    -> base::expected<OperatorOffset, std::string> {
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  CHECK_EQ(GetOperand(gelu.input_operand_id).descriptor.data_type(),
           OperandDataType::kFloat32);

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_GELU,
      operand_to_index_map_.at(gelu.input_operand_id),
      operand_to_index_map_.at(gelu.output_operand_id));
}

auto GraphBuilderTflite::SerializeGemm(const mojom::Gemm& gemm)
    -> base::expected<OperatorOffset, std::string> {
  // Check for unsupported inputs.
  const mojom::Operand& output_operand = GetOperand(gemm.output_operand_id);
  CHECK_EQ(output_operand.descriptor.Rank(), 2u);
  CHECK(kFloatDataTypes.contains(output_operand.descriptor.data_type()));
  const uint32_t output_channels = output_operand.descriptor.shape()[1];
  if (gemm.c_operand_id.has_value()) {
    // The TFLite fully connected operator only supports a 1-D bias tensor with
    // `output_channels` dimensions.
    const mojom::Operand& bias_operand = GetOperand(*gemm.c_operand_id);
    if (bias_operand.descriptor.Rank() != 1u ||
        bias_operand.descriptor.shape()[0] != output_channels) {
      // TODO(crbug.com/328652105): Support the bias with other dimensions by
      // element-wise addition operator.
      return base::unexpected(base::StringPrintf(
          "The dimensions of bias must be [%u].", output_channels));
    }
  }
  if (gemm.alpha != 1.0f) {
    // TODO(crbug.com/328652105): Support alpha by using element-wise
    // multiplication operator.
    return base::unexpected("gemm doesn't support alpha option.");
  }
  if (gemm.beta != 1.0f) {
    // TODO(crbug.com/328652105): Support beta by using element-wise
    // multiplication operator.
    return base::unexpected("gemm doesn't support beta option.");
  }
  if (gemm.a_transpose) {
    // TODO(crbug.com/328652105): Support aTranspose by using transpose
    // operator.
    return base::unexpected("gemm doesn't support aTranspose option.");
  }

  // The WebNN Gemm follows the expression `alpha * A * B + beta * C`, where
  // A is a 2-D tensor with shape [M, K], B is a 2-D tensor with shape [K,
  // N] by default options, but Tflite Fully Connected's input and filter
  // shapes are [batch, input_channels] and [output_channels,
  // input_channels], so the Transpose operator need to be inserted before
  // Gemm When bTranspose option is false.
  std::optional<int32_t> transposed_filter_index;
  const uint32_t filter_operand_id = gemm.b_operand_id;
  const int32_t filter_index = operand_to_index_map_.at(filter_operand_id);
  if (!gemm.b_transpose) {
    const mojom::Operand& filter_operand = GetOperand(filter_operand_id);
    CHECK_EQ(filter_operand.descriptor.Rank(), 2u);
    // The shape has been validated to not overflow before creating tensor.
    const auto filter_dimensions =
        ToSignedDimensions(filter_operand.descriptor.shape());
    CHECK(filter_dimensions.has_value());

    const std::array<uint32_t, 2> permutation = {1u, 0u};
    transposed_filter_index = InsertTransposeOperation(
        *filter_dimensions,
        OperandDataTypeToTFLite(filter_operand.descriptor.data_type()),
        filter_index, permutation);
  }

  std::vector<int32_t> op_inputs = {operand_to_index_map_.at(gemm.a_operand_id),
                                    transposed_filter_index.has_value()
                                        ? *transposed_filter_index
                                        : filter_index};
  if (gemm.c_operand_id.has_value()) {
    op_inputs.push_back(operand_to_index_map_.at(*gemm.c_operand_id));
  }

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_FULLY_CONNECTED);
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(gemm.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

// Serialize a sub graph (input * weight + bias) for gru cell.
//
//     [input]   [weight]
//         \        /
//           Matmul   [bias]
//             \        /
//                 add
//                  |
//              [output]
int32_t GraphBuilderTflite::SerializeSubGraphMatmulAdd(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t weight_tensor_index,
    std::optional<int32_t> bias_tensor_index) {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  const int32_t output_tensor_index_of_matmul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeMatmulOperation(
      input_tensor_index, weight_tensor_index, output_tensor_index_of_matmul));

  int32_t output_tensor_index = output_tensor_index_of_matmul;
  if (bias_tensor_index) {
    output_tensor_index =
        SerializeTemporaryTensor(input_dimensions, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, output_tensor_index_of_matmul,
        *bias_tensor_index, output_tensor_index));
  }

  return output_tensor_index;
}

// Serialize a sub graph (slice appending transpose operation) for gru cell.
//
//     [input]
//        |
//      slice
//        |
//     transpose
//        |
//     [output]
auto GraphBuilderTflite::SerializeSubGraphSliceTranspose(
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> slice_starts,
    base::span<const int32_t> slice_sizes)
    -> base::expected<int32_t, std::string> {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  // The output is a tensor the same as the specified slice sizes.
  const int32_t output_tensor_index_of_slice =
      SerializeTemporaryTensor(slice_sizes, input_tensor_type);
  ASSIGN_OR_RETURN(
      OperatorOffset operator_offset,
      SerializeSliceOperation(input_tensor_index, output_tensor_index_of_slice,
                              slice_starts, slice_sizes));
  operators_.emplace_back(operator_offset);

  const int32_t output_tensor_index =
      SerializeTemporaryTensor(slice_sizes, input_tensor_type);
  std::vector<uint32_t> permutation(slice_sizes.size());
  std::iota(permutation.rbegin(), permutation.rend(), 0);
  operators_.emplace_back(SerializeTransposeOperation(
      output_tensor_index_of_slice, output_tensor_index, permutation));

  return output_tensor_index;
}

auto GraphBuilderTflite::SerializeGruGate(
    const GruCellOperation& gru_cell,
    GruGateType type,
    std::optional<int32_t> reset_gate_tensor_index)
    -> base::expected<int32_t, std::string> {
  CHECK_EQ(gru_cell.input_dimensions.size(), 2u);
  const int32_t hidden_size = gru_cell.hidden_size;
  const std::array<int32_t, 2> output_shape = {gru_cell.input_dimensions[0],
                                               hidden_size};
  const int32_t input_size = gru_cell.input_dimensions[1];

  std::vector<::tflite::BuiltinOperator> activation_operator_codes;
  activation_operator_codes.reserve(gru_cell.activations.size());
  for (mojom::RecurrentNetworkActivation activation : gru_cell.activations) {
    activation_operator_codes.push_back(
        GetRecurrentNetworkActivation(activation));
  }

  ::tflite::BuiltinOperator activation_code;
  int32_t slice_start;
  switch (type) {
    case GruGateType::kUpdate: {
      activation_code = activation_operator_codes[0];
      slice_start =
          gru_cell.layout == mojom::GruWeightLayout::kRzn ? hidden_size : 0;
      break;
    }
    case GruGateType::kReset: {
      activation_code = activation_operator_codes[0];
      slice_start =
          gru_cell.layout == mojom::GruWeightLayout::kZrn ? hidden_size : 0;
      break;
    }
    case GruGateType::kNew: {
      activation_code = activation_operator_codes[1];
      slice_start = 2 * hidden_size;
      break;
    }
  }

  // input * weight + bias.
  ::tflite::TensorType input_tensor_type = gru_cell.input_tensor_type;
  std::optional<int32_t> bias_tensor_index;
  if (gru_cell.bias_tensor_index) {
    bias_tensor_index = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*gru_cell.bias_tensor_index, *bias_tensor_index,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);
  }
  ASSIGN_OR_RETURN(const int32_t weight_tensor_index,
                   SerializeSubGraphSliceTranspose(
                       input_tensor_type, gru_cell.weight_tensor_index,
                       std::array<int32_t, 2>({slice_start, 0}),
                       std::array<int32_t, 2>({hidden_size, input_size})));
  const int32_t output_tensor_index_of_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, gru_cell.input_tensor_index,
      weight_tensor_index, bias_tensor_index);

  // hiddenState * recurrentWeight + recurrentBias.
  std::optional<int32_t> recurrent_bias_tensor_index;
  if (gru_cell.recurrent_bias_tensor_index) {
    recurrent_bias_tensor_index = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*gru_cell.recurrent_bias_tensor_index,
                                *recurrent_bias_tensor_index,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);
  }
  ASSIGN_OR_RETURN(
      const int32_t recurrent_weight_tensor_index,
      SerializeSubGraphSliceTranspose(
          input_tensor_type, gru_cell.recurrent_weight_tensor_index,
          std::array<int32_t, 2>({slice_start, 0}),
          std::array<int32_t, 2>({hidden_size, hidden_size})));
  int32_t hidden_state_tensor_index = gru_cell.hidden_state_tensor_index;
  // Apply the reset gate before matrix multiplication for new gate if needed.
  if (type == GruGateType::kNew && !gru_cell.reset_after) {
    const int32_t output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, *reset_gate_tensor_index,
        hidden_state_tensor_index, output_tensor_index_of_mul));
    hidden_state_tensor_index = output_tensor_index_of_mul;
  }
  int32_t output_tensor_index_of_recurrent_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, hidden_state_tensor_index,
      recurrent_weight_tensor_index, recurrent_bias_tensor_index);
  // Apply the reset gate after matrix multiplication for new gate if needed.
  if (type == GruGateType::kNew && gru_cell.reset_after) {
    const int32_t output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, *reset_gate_tensor_index,
        output_tensor_index_of_recurrent_weight, output_tensor_index_of_mul));
    output_tensor_index_of_recurrent_weight = output_tensor_index_of_mul;
  }

  // Add the result of the above two expressions (element-wise multiplication
  // between the input / hiddenState and the respective weights / recurrent
  // weights).
  const int32_t output_tensor_index_of_add =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, output_tensor_index_of_weight,
      output_tensor_index_of_recurrent_weight, output_tensor_index_of_add));

  // Apply first activation for the update and reset gate, the second activation
  // for the new gate.
  const int32_t output_tensor_index_of_gate =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(activation_code,
                                                  output_tensor_index_of_add,
                                                  output_tensor_index_of_gate));

  return output_tensor_index_of_gate;
}

GraphBuilderTflite::RecurrentNetworkBase::RecurrentNetworkBase(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t weight_tensor_index,
    int32_t recurrent_weight_tensor_index,
    std::optional<int32_t> bias_tensor_index,
    std::optional<int32_t> recurrent_bias_tensor_index,
    int32_t hidden_state_tensor_index,
    int32_t hidden_size,
    base::span<const mojom::RecurrentNetworkActivation> activations)
    : input_dimensions(input_dimensions),
      input_tensor_type(input_tensor_type),
      input_tensor_index(input_tensor_index),
      weight_tensor_index(weight_tensor_index),
      recurrent_weight_tensor_index(recurrent_weight_tensor_index),
      bias_tensor_index(bias_tensor_index),
      recurrent_bias_tensor_index(recurrent_bias_tensor_index),
      hidden_state_tensor_index(hidden_state_tensor_index),
      hidden_size(hidden_size),
      activations(activations) {}

GraphBuilderTflite::RecurrentNetworkBase::~RecurrentNetworkBase() = default;

GraphBuilderTflite::GruCellOperation::GruCellOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    int32_t weight_tensor_index,
    int32_t recurrent_weight_tensor_index,
    std::optional<int32_t> bias_tensor_index,
    std::optional<int32_t> recurrent_bias_tensor_index,
    int32_t hidden_state_tensor_index,
    int32_t hidden_size,
    bool reset_after,
    mojom::GruWeightLayout layout,
    base::span<const mojom::RecurrentNetworkActivation> activations)
    : RecurrentNetworkBase(input_dimensions,
                           input_tensor_type,
                           input_tensor_index,
                           weight_tensor_index,
                           recurrent_weight_tensor_index,
                           bias_tensor_index,
                           recurrent_bias_tensor_index,
                           hidden_state_tensor_index,
                           hidden_size,
                           activations),
      output_tensor_index(output_tensor_index),
      reset_after(reset_after),
      layout(layout) {}

GraphBuilderTflite::GruCellOperation::~GruCellOperation() = default;

auto GraphBuilderTflite::SerializeGruCell(const mojom::GruCell& gru_cell)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand = GetOperand(gru_cell.input_operand_id);
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  if (input_operand.descriptor.data_type() == OperandDataType::kFloat16) {
    return base::unexpected("The 16-bit float data type is not supported.");
  }
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  CHECK_EQ(signed_input_dimensions->size(), 2u);
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const auto checked_hidden_size =
      base::MakeCheckedNum<int32_t>(gru_cell.hidden_size);
  if (!checked_hidden_size.IsValid()) {
    return base::unexpected("The hidden size is too large.");
  }

  std::optional<int32_t> bias_tensor_index;
  if (gru_cell.bias_operand_id) {
    bias_tensor_index = operand_to_index_map_.at(*gru_cell.bias_operand_id);
  }
  std::optional<int32_t> recurrent_bias_tensor_index;
  if (gru_cell.recurrent_bias_operand_id) {
    recurrent_bias_tensor_index =
        operand_to_index_map_.at(*gru_cell.recurrent_bias_operand_id);
  }

  GruCellOperation gru_cell_operation(
      *signed_input_dimensions, input_tensor_type,
      operand_to_index_map_.at(gru_cell.input_operand_id),
      operand_to_index_map_.at(gru_cell.output_operand_id),
      operand_to_index_map_.at(gru_cell.weight_operand_id),
      operand_to_index_map_.at(gru_cell.recurrent_weight_operand_id),
      bias_tensor_index, recurrent_bias_tensor_index,
      operand_to_index_map_.at(gru_cell.hidden_state_operand_id),
      checked_hidden_size.ValueOrDie(), gru_cell.reset_after, gru_cell.layout,
      gru_cell.activations);

  return SerializeGruCellOperation(gru_cell_operation);
}

auto GraphBuilderTflite::SerializeGruCellOperation(
    const GruCellOperation& gru_cell)
    -> base::expected<OperatorOffset, std::string> {
  // Compute the update gate.
  ASSIGN_OR_RETURN(const int32_t update_gate_tensor_index,
                   SerializeGruGate(gru_cell, GruGateType::kUpdate));

  // Compute the reset gate.
  ASSIGN_OR_RETURN(const int32_t reset_gate_tensor_index,
                   SerializeGruGate(gru_cell, GruGateType::kReset));

  // Compute the new gate.
  ASSIGN_OR_RETURN(
      const int32_t new_gate_tensor_index,
      SerializeGruGate(gru_cell, GruGateType::kNew, reset_gate_tensor_index));

  const std::array<int32_t, 2> output_shape = {gru_cell.input_dimensions[0],
                                               gru_cell.hidden_size};
  // Compute mul(newGate, sub(one, updateGate)).
  const int32_t scalar_one_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1.0},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_sub =
      SerializeTemporaryTensor(output_shape, gru_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, scalar_one_tensor_index,
      update_gate_tensor_index, output_tensor_index_of_sub));
  const int32_t new_gate_tensor_index_of_mul =
      SerializeTemporaryTensor(output_shape, gru_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, new_gate_tensor_index,
      output_tensor_index_of_sub, new_gate_tensor_index_of_mul));

  // Compute mul(updateGate, hiddenState).
  const int32_t update_gate_tensor_index_of_mul =
      SerializeTemporaryTensor(output_shape, gru_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, update_gate_tensor_index,
      gru_cell.hidden_state_tensor_index, update_gate_tensor_index_of_mul));

  // Compute the new hidden state with adding the new gate and update gate.
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, new_gate_tensor_index_of_mul,
      update_gate_tensor_index_of_mul, gru_cell.output_tensor_index);
}

GraphBuilderTflite::LstmCellOperation::LstmCellOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> output_tensor_indices,
    int32_t weight_tensor_index,
    int32_t recurrent_weight_tensor_index,
    std::optional<int32_t> bias_tensor_index,
    std::optional<int32_t> recurrent_bias_tensor_index,
    int32_t hidden_state_tensor_index,
    int32_t hidden_size,
    int32_t cell_state_tensor_index,
    std::optional<int32_t> peephole_weight_tensor_index,
    mojom::LstmWeightLayout layout,
    base::span<const mojom::RecurrentNetworkActivation> activations)
    : RecurrentNetworkBase(input_dimensions,
                           input_tensor_type,
                           input_tensor_index,
                           weight_tensor_index,
                           recurrent_weight_tensor_index,
                           bias_tensor_index,
                           recurrent_bias_tensor_index,
                           hidden_state_tensor_index,
                           hidden_size,
                           activations),
      output_tensor_indices(output_tensor_indices),
      cell_state_tensor_index(cell_state_tensor_index),
      peephole_weight_tensor_index(peephole_weight_tensor_index),
      layout(layout) {}

GraphBuilderTflite::LstmCellOperation::~LstmCellOperation() = default;

base::expected<int32_t, std::string> GraphBuilderTflite::SerializeLstmGate(
    const LstmCellOperation& lstm_cell,
    LstmGateType type) {
  CHECK_EQ(lstm_cell.input_dimensions.size(), 2u);
  const int32_t hidden_size = lstm_cell.hidden_size;
  const std::array<int32_t, 2> output_shape = {lstm_cell.input_dimensions[0],
                                               hidden_size};
  const int32_t input_size = lstm_cell.input_dimensions[1];

  std::vector<::tflite::BuiltinOperator> activation_operator_codes;
  activation_operator_codes.reserve(lstm_cell.activations.size());
  for (mojom::RecurrentNetworkActivation activation : lstm_cell.activations) {
    activation_operator_codes.push_back(
        GetRecurrentNetworkActivation(activation));
  }

  CHECK(lstm_cell.layout == mojom::LstmWeightLayout::kIofg ||
        lstm_cell.layout == mojom::LstmWeightLayout::kIfgo);
  ::tflite::BuiltinOperator activation_code;
  int32_t slice_start;
  switch (type) {
    case LstmGateType::kInput: {
      activation_code = activation_operator_codes[0];
      slice_start = 0;
      break;
    }
    case LstmGateType::kForget: {
      activation_code = activation_operator_codes[0];
      slice_start = lstm_cell.layout == mojom::LstmWeightLayout::kIofg
                        ? 2 * hidden_size
                        : hidden_size;
      break;
    }
    case LstmGateType::kCell: {
      activation_code = activation_operator_codes[1];
      slice_start = lstm_cell.layout == mojom::LstmWeightLayout::kIofg
                        ? 3 * hidden_size
                        : 2 * hidden_size;
      break;
    }
    case LstmGateType::kOutput: {
      activation_code = activation_operator_codes[0];
      slice_start = lstm_cell.layout == mojom::LstmWeightLayout::kIofg
                        ? hidden_size
                        : 3 * hidden_size;
      break;
    }
  }

  // input * weight + bias.
  const ::tflite::TensorType input_tensor_type = lstm_cell.input_tensor_type;
  std::optional<int32_t> bias_tensor_index;
  if (lstm_cell.bias_tensor_index) {
    bias_tensor_index = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                     SerializeSliceOperation(
                         *lstm_cell.bias_tensor_index, *bias_tensor_index,
                         std::array<int32_t, 1>({slice_start}),
                         std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);
  }
  ASSIGN_OR_RETURN(const int32_t weight_tensor_index,
                   SerializeSubGraphSliceTranspose(
                       input_tensor_type, lstm_cell.weight_tensor_index,
                       std::array<int32_t, 2>({slice_start, 0}),
                       std::array<int32_t, 2>({hidden_size, input_size})));
  const int32_t output_tensor_index_of_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, lstm_cell.input_tensor_index,
      weight_tensor_index, bias_tensor_index);

  // hiddenState * recurrentWeight + recurrentBias.
  std::optional<int32_t> recurrent_bias_tensor_index;
  if (lstm_cell.recurrent_bias_tensor_index) {
    recurrent_bias_tensor_index = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*lstm_cell.recurrent_bias_tensor_index,
                                *recurrent_bias_tensor_index,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);
  }
  ASSIGN_OR_RETURN(
      const int32_t recurrent_weight_tensor_index,
      SerializeSubGraphSliceTranspose(
          input_tensor_type, lstm_cell.recurrent_weight_tensor_index,
          std::array<int32_t, 2>({slice_start, 0}),
          std::array<int32_t, 2>({hidden_size, hidden_size})));
  int32_t output_tensor_index_of_recurrent_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, lstm_cell.hidden_state_tensor_index,
      recurrent_weight_tensor_index, recurrent_bias_tensor_index);

  // Add the result of the above two expressions (element-wise multiplication
  // between the input / hiddenState and the respective weights / recurrent
  // weights).
  int32_t updated_state_tensor_index =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, output_tensor_index_of_weight,
      output_tensor_index_of_recurrent_weight, updated_state_tensor_index));

  // mul(cellState, peepholeWeight) + updatedState.
  if (lstm_cell.peephole_weight_tensor_index && type != LstmGateType::kCell) {
    int32_t output_tensor_index_of_slice = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*lstm_cell.peephole_weight_tensor_index,
                                output_tensor_index_of_slice,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);

    const int32_t output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, lstm_cell.cell_state_tensor_index,
        output_tensor_index_of_slice, output_tensor_index_of_mul));

    const int32_t output_tensor_index_of_add =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, output_tensor_index_of_mul,
        updated_state_tensor_index, output_tensor_index_of_add));
    updated_state_tensor_index = output_tensor_index_of_add;
  }

  // Apply first activation for the input, forget and output gate, the second
  // activation for the cell gate.
  const int32_t output_tensor_index_of_gate =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(activation_code,
                                                  updated_state_tensor_index,
                                                  output_tensor_index_of_gate));

  return output_tensor_index_of_gate;
}

auto GraphBuilderTflite::SerializeLstmCellOperation(
    const LstmCellOperation& lstm_cell)
    -> base::expected<OperatorOffset, std::string> {
  // Compute the input gate.
  ASSIGN_OR_RETURN(const int32_t input_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kInput));

  // Compute the forget gate.
  ASSIGN_OR_RETURN(const int32_t forgat_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kForget));

  // Compute the cell gate.
  ASSIGN_OR_RETURN(const int32_t cell_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kCell));

  // Compute the output gate.
  ASSIGN_OR_RETURN(const int32_t output_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kOutput));

  const std::array<int32_t, 2> output_shape = {lstm_cell.input_dimensions[0],
                                               lstm_cell.hidden_size};
  // Compute add(mul(forgetGete, cellState), mul(cellGate, inputGate).
  const int32_t forget_cell_gete_tensor_index =
      SerializeTemporaryTensor(output_shape, lstm_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, forgat_gate_tensor_index,
      lstm_cell.cell_state_tensor_index, forget_cell_gete_tensor_index));
  const int32_t input_cell_gete_tensor_index =
      SerializeTemporaryTensor(output_shape, lstm_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, cell_gate_tensor_index,
      input_gate_tensor_index, input_cell_gete_tensor_index));
  CHECK_EQ(lstm_cell.output_tensor_indices.size(), 2u);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, forget_cell_gete_tensor_index,
      input_cell_gete_tensor_index, lstm_cell.output_tensor_indices[1]));

  // Compute the new hidden state with adding the output gate and the new cell
  // state.
  const int32_t activation_tensor_index =
      SerializeTemporaryTensor(output_shape, lstm_cell.input_tensor_type);
  CHECK_EQ(lstm_cell.activations.size(), 3u);
  operators_.emplace_back(SerializeUnaryOperation(
      GetRecurrentNetworkActivation(lstm_cell.activations[2]),
      lstm_cell.output_tensor_indices[1], activation_tensor_index));
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_gate_tensor_index,
      activation_tensor_index, lstm_cell.output_tensor_indices[0]);
}

// Serialize a sub graph (slice appending squeeze operation) for gru.
//
//     [input]
//        |
//      slice
//        |
//     squeeze
//        |
//     [output]
auto GraphBuilderTflite::SerializeSubGraphSliceSqueeze(
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> slice_starts,
    base::span<const int32_t> slice_sizes,
    int32_t squeeze_axis) -> base::expected<int32_t, std::string> {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  // The output is a tensor the same as the specified slice sizes.
  const int32_t output_tensor_index_of_slice =
      SerializeTemporaryTensor(slice_sizes, input_tensor_type);
  ASSIGN_OR_RETURN(
      OperatorOffset operator_offset,
      SerializeSliceOperation(input_tensor_index, output_tensor_index_of_slice,
                              slice_starts, slice_sizes));
  operators_.emplace_back(operator_offset);

  std::vector<int32_t> squeeze_output_shape;
  squeeze_output_shape.reserve(slice_sizes.size());
  for (size_t i = 0; i < slice_sizes.size(); ++i) {
    if (base::checked_cast<size_t>(squeeze_axis) != i) {
      squeeze_output_shape.push_back(slice_sizes[i]);
    }
  }
  const int32_t output_tensor_index =
      SerializeTemporaryTensor(squeeze_output_shape, input_tensor_type);
  flatbuffers::Offset<void> builtin_options =
      ::tflite::CreateSqueezeOptions(
          builder_, builder_.CreateVector<int32_t>({squeeze_axis}))
          .Union();
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SQUEEZE);
  const std::array<int32_t, 1> op_inputs = {output_tensor_index_of_slice};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_SqueezeOptions, builtin_options));

  return output_tensor_index;
}

// `RecurrentNetworkType` must be `mojom::Gru` or `mojom::Lstm`.
template <typename RecurrentNetworkType>
auto GraphBuilderTflite::SerializeRecurrentNetwork(
    const RecurrentNetworkType& recurrent_network)
    -> base::expected<OperatorOffset, std::string> {
  static_assert(std::is_same<RecurrentNetworkType, mojom::Gru>::value ||
                std::is_same<RecurrentNetworkType, mojom::Lstm>::value);
  const mojom::Operand& input_operand =
      GetOperand(recurrent_network.input_operand_id);
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  if (input_operand.descriptor.data_type() == OperandDataType::kFloat16) {
    return base::unexpected("The 16-bit float data type is not supported.");
  }
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  CHECK_EQ(signed_input_dimensions->size(), 3u);
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const auto checked_hidden_size =
      base::MakeCheckedNum<int32_t>(recurrent_network.hidden_size);
  if (!checked_hidden_size.IsValid()) {
    return base::unexpected("The hidden size is too large.");
  }
  const auto checked_steps =
      base::MakeCheckedNum<int32_t>(recurrent_network.steps);
  if (!checked_steps.IsValid()) {
    return base::unexpected("The steps size is too large.");
  }
  const int32_t num_directions =
      recurrent_network.direction == mojom::RecurrentNetworkDirection::kBoth
          ? 2
          : 1;
  const int32_t batch_size = (*signed_input_dimensions)[1];
  const int32_t input_size = (*signed_input_dimensions)[2];
  const int32_t hidden_size = checked_hidden_size.ValueOrDie();
  const int32_t recurrent_steps = checked_steps.ValueOrDie();

  const std::array<int32_t, 3> initial_hidden_cell_state_shape = {
      num_directions, batch_size, hidden_size};
  int32_t hidden_state_tensor_index = GetInitialHiddenAndCellState(
      recurrent_network.initial_hidden_state_operand_id,
      initial_hidden_cell_state_shape);
  // The cell state tensor index only be used by lstm.
  int32_t lstm_cell_state_tensor_index;
  if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
    lstm_cell_state_tensor_index = GetInitialHiddenAndCellState(
        recurrent_network.initial_cell_state_operand_id,
        initial_hidden_cell_state_shape);
  }

  std::vector<int32_t> cell_weight_tensor_indices;
  cell_weight_tensor_indices.reserve(num_directions);
  std::vector<int32_t> cell_recurrent_weight_tensor_indices;
  cell_recurrent_weight_tensor_indices.reserve(num_directions);
  std::vector<int32_t> cell_bias_tensor_indices;
  cell_bias_tensor_indices.reserve(num_directions);
  std::vector<int32_t> cell_recurrent_bias_tensor_indices;
  cell_recurrent_bias_tensor_indices.reserve(num_directions);
  // The cell peephole weight tensor indices only be used by lstm.
  std::vector<int32_t> lstm_cell_peephole_weight_tensor_indices;
  if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
    lstm_cell_peephole_weight_tensor_indices.reserve(num_directions);
  }
  for (int32_t slot = 0; slot < num_directions; ++slot) {
    int32_t slice_height;
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Gru>::value) {
      slice_height = 3 * hidden_size;
    } else /* `RecurrentNetworkType` is `mojom::Lstm` */ {
      slice_height = 4 * hidden_size;
    }
    ASSIGN_OR_RETURN(
        const int32_t cell_weight_tensor_index,
        SerializeSubGraphSliceSqueeze(
            input_tensor_type,
            operand_to_index_map_.at(recurrent_network.weight_operand_id),
            /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
            /*slice_sizes=*/
            std::array<int32_t, 3>({1, slice_height, input_size}),
            /*squeeze_axis=*/0));
    cell_weight_tensor_indices.push_back(cell_weight_tensor_index);

    ASSIGN_OR_RETURN(const int32_t cell_recurrent_weight_tensor_index,
                     SerializeSubGraphSliceSqueeze(
                         input_tensor_type,
                         operand_to_index_map_.at(
                             recurrent_network.recurrent_weight_operand_id),
                         /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
                         /*slice_sizes=*/
                         std::array<int32_t, 3>({1, slice_height, hidden_size}),
                         /*squeeze_axis=*/0));
    cell_recurrent_weight_tensor_indices.push_back(
        cell_recurrent_weight_tensor_index);

    if (recurrent_network.bias_operand_id) {
      ASSIGN_OR_RETURN(
          const int32_t cell_bias_tensor_index,
          SerializeSubGraphSliceSqueeze(
              input_tensor_type,
              operand_to_index_map_.at(*recurrent_network.bias_operand_id),
              /*slice_starts=*/std::array<int32_t, 2>({slot, 0}),
              /*slice_sizes=*/
              std::array<int32_t, 2>({1, slice_height}),
              /*squeeze_axis=*/0));
      cell_bias_tensor_indices.push_back(cell_bias_tensor_index);
    }

    if (recurrent_network.recurrent_bias_operand_id) {
      ASSIGN_OR_RETURN(const int32_t cell_recurrent_bias_tensor_index,
                       SerializeSubGraphSliceSqueeze(
                           input_tensor_type,
                           operand_to_index_map_.at(
                               *recurrent_network.recurrent_bias_operand_id),
                           /*slice_starts=*/std::array<int32_t, 2>({slot, 0}),
                           /*slice_sizes=*/
                           std::array<int32_t, 2>({1, slice_height}),
                           /*squeeze_axis=*/0));
      cell_recurrent_bias_tensor_indices.push_back(
          cell_recurrent_bias_tensor_index);
    }

    if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
      if (recurrent_network.peephole_weight_operand_id) {
        ASSIGN_OR_RETURN(const int32_t peephole_weight_tensor_index,
                         SerializeSubGraphSliceSqueeze(
                             input_tensor_type,
                             operand_to_index_map_.at(
                                 *recurrent_network.peephole_weight_operand_id),
                             /*slice_starts=*/std::array<int32_t, 2>({slot, 0}),
                             /*slice_sizes=*/
                             std::array<int32_t, 2>({1, 3 * hidden_size}),
                             /*squeeze_axis=*/0));
        lstm_cell_peephole_weight_tensor_indices.push_back(
            peephole_weight_tensor_index);
      }
    }
  }

  std::optional<int32_t> sequence_tensor_index;
  for (int32_t step = 0; step < recurrent_steps; ++step) {
    std::vector<int32_t> current_hidden_tensor_indices;
    current_hidden_tensor_indices.reserve(num_directions);
    std::vector<int32_t> lstm_current_cell_tensor_indices;
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
      lstm_current_cell_tensor_indices.reserve(num_directions);
    }
    for (int32_t slot = 0; slot < num_directions; ++slot) {
      ASSIGN_OR_RETURN(
          const int32_t cell_hidden_tensor_index,
          SerializeSubGraphSliceSqueeze(
              input_tensor_type, hidden_state_tensor_index,
              /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
              /*slice_sizes=*/
              std::array<int32_t, 3>({1, batch_size, hidden_size}),
              /*squeeze_axis=*/0));
      current_hidden_tensor_indices.push_back(cell_hidden_tensor_index);

      if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
        ASSIGN_OR_RETURN(
            const int32_t lstm_cell_tensor_index,
            SerializeSubGraphSliceSqueeze(
                input_tensor_type, lstm_cell_state_tensor_index,
                /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
                /*slice_sizes=*/
                std::array<int32_t, 3>({1, batch_size, hidden_size}),
                /*squeeze_axis=*/0));
        lstm_current_cell_tensor_indices.push_back(lstm_cell_tensor_index);
      }
    }

    std::optional<int32_t> next_hidden_tensor_index;
    std::optional<int32_t> lstm_next_cell_tensor_index;
    for (int32_t slot = 0; slot < num_directions; ++slot) {
      const int32_t slice_start =
          (slot == 1 || recurrent_network.direction ==
                            mojom::RecurrentNetworkDirection::kBackward
               ? recurrent_steps - step - 1
               : step);
      ASSIGN_OR_RETURN(
          const int32_t cell_input_tensor_index,
          SerializeSubGraphSliceSqueeze(
              input_tensor_type,
              operand_to_index_map_.at(recurrent_network.input_operand_id),
              /*slice_starts=*/std::array<int32_t, 3>({slice_start, 0, 0}),
              /*slice_sizes=*/
              std::array<int32_t, 3>({1, batch_size, input_size}),
              /*squeeze_axis=*/0));

      const std::array<int32_t, 2> cell_input_dimensions = {batch_size,
                                                            input_size};
      const std::array<int32_t, 2> cell_output_dimensions = {batch_size,
                                                             hidden_size};
      const int32_t output_hidden_state_tensor_index =
          SerializeTemporaryTensor(cell_output_dimensions, input_tensor_type);
      // The output cell state tensor index is only used by lstm.
      int32_t lstm_output_cell_state_tensor_index;

      std::optional<int32_t> bias_tensor_index;
      if (recurrent_network.bias_operand_id) {
        bias_tensor_index = cell_bias_tensor_indices[slot];
      }
      std::optional<int32_t> recurrent_bias_tensor_index;
      if (recurrent_network.recurrent_bias_operand_id) {
        recurrent_bias_tensor_index = cell_recurrent_bias_tensor_indices[slot];
      }

      if constexpr (std::is_same<RecurrentNetworkType, mojom::Gru>::value) {
        GruCellOperation gru_cell_operation(
            cell_input_dimensions, input_tensor_type, cell_input_tensor_index,
            output_hidden_state_tensor_index, cell_weight_tensor_indices[slot],
            cell_recurrent_weight_tensor_indices[slot], bias_tensor_index,
            recurrent_bias_tensor_index, current_hidden_tensor_indices[slot],
            hidden_size, recurrent_network.reset_after,
            recurrent_network.layout, recurrent_network.activations);

        ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                         SerializeGruCellOperation(gru_cell_operation));
        operators_.emplace_back(operator_offset);
      } else /* `RecurrentNetworkType` is `mojom::Lstm` */ {
        std::optional<int32_t> lstm_peephole_weight_tensor_index;
        if (recurrent_network.peephole_weight_operand_id) {
          lstm_peephole_weight_tensor_index =
              lstm_cell_peephole_weight_tensor_indices[slot];
        }
        lstm_output_cell_state_tensor_index =
            SerializeTemporaryTensor(cell_output_dimensions, input_tensor_type);
        const std::array<int32_t, 2> output_tensor_indices = {
            output_hidden_state_tensor_index,
            lstm_output_cell_state_tensor_index};
        LstmCellOperation lstm_cell_operation(
            cell_input_dimensions, input_tensor_type, cell_input_tensor_index,
            output_tensor_indices, cell_weight_tensor_indices[slot],
            cell_recurrent_weight_tensor_indices[slot], bias_tensor_index,
            recurrent_bias_tensor_index, current_hidden_tensor_indices[slot],
            hidden_size, lstm_current_cell_tensor_indices[slot],
            lstm_peephole_weight_tensor_index, recurrent_network.layout,
            recurrent_network.activations);

        ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                         SerializeLstmCellOperation(lstm_cell_operation));
        operators_.emplace_back(operator_offset);
      }

      // Reshape the output hidden state tensor of gru / lstm cell to the 3-D
      // tensor [1, batchSize, hiddenSize].
      const std::array<int32_t, 3> new_shape = {1, batch_size, hidden_size};
      const std::array<int32_t, 3> concat_output_shape = {slot + 1, batch_size,
                                                          hidden_size};
      next_hidden_tensor_index = ReshapeHiddenAndCellState(
          input_tensor_type, output_hidden_state_tensor_index, new_shape,
          next_hidden_tensor_index, concat_output_shape);

      if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
        // Reshape the output cell state tensor of lstm cell to the 3-D tensor
        // [1, batchSize, hiddenSize].
        lstm_next_cell_tensor_index = ReshapeHiddenAndCellState(
            input_tensor_type, lstm_output_cell_state_tensor_index, new_shape,
            lstm_next_cell_tensor_index, concat_output_shape);
      }
    }

    // Update hidden state.
    hidden_state_tensor_index = *next_hidden_tensor_index;
    // Update cell state for Lstm.
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
      lstm_cell_state_tensor_index = *lstm_next_cell_tensor_index;
    }

    if (recurrent_network.return_sequence) {
      // Reshape the output tensor of gru cell to the 3-D tensor [1,
      // num_directions, batchSize, hiddenSize].
      const std::array<int32_t, 4> new_shape = {1, num_directions, batch_size,
                                                hidden_size};
      const int32_t reshape_cells_tensor_index =
          SerializeTemporaryTensor(new_shape, input_tensor_type);
      operators_.emplace_back(SerializeReshapeOperation(
          *next_hidden_tensor_index, reshape_cells_tensor_index, new_shape));

      if (sequence_tensor_index) {
        const std::array<int32_t, 4> concat_output_shape = {
            step + 1, num_directions, batch_size, hidden_size};
        const int32_t concat_tensor_index =
            SerializeTemporaryTensor(concat_output_shape, input_tensor_type);
        operators_.emplace_back(SerializeConcatOperation(
            std::array<int32_t, 2>(
                {*sequence_tensor_index, reshape_cells_tensor_index}),
            concat_tensor_index, 0));
        sequence_tensor_index = concat_tensor_index;
      } else {
        sequence_tensor_index = reshape_cells_tensor_index;
      }
    }
  }

  if (recurrent_network.return_sequence) {
    int32_t output_sequence_tensor_index;
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Gru>::value) {
      CHECK_EQ(recurrent_network.output_operand_ids.size(), 2u);
      output_sequence_tensor_index =
          operand_to_index_map_.at(recurrent_network.output_operand_ids[1]);
    } else /* `RecurrentNetworkType` is `mojom::Lstm` */ {
      CHECK_EQ(recurrent_network.output_operand_ids.size(), 3u);
      output_sequence_tensor_index =
          operand_to_index_map_.at(recurrent_network.output_operand_ids[2]);
    }
    const std::array<int32_t, 4> new_shape = {recurrent_steps, num_directions,
                                              batch_size, hidden_size};
    operators_.emplace_back(SerializeReshapeOperation(
        *sequence_tensor_index, output_sequence_tensor_index, new_shape));
  }

  // Return cell state for Lstm.
  if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
    CHECK_GE(recurrent_network.output_operand_ids.size(), 2u);
    operators_.emplace_back(SerializeReshapeOperation(
        lstm_cell_state_tensor_index,
        operand_to_index_map_.at(recurrent_network.output_operand_ids[1]),
        std::array<int32_t, 3>({num_directions, batch_size, hidden_size})));
  }

  return SerializeReshapeOperation(
      hidden_state_tensor_index,
      operand_to_index_map_.at(recurrent_network.output_operand_ids[0]),
      std::array<int32_t, 3>({num_directions, batch_size, hidden_size}));
}

auto GraphBuilderTflite::SerializeHardSigmoid(
    const mojom::HardSigmoid& hard_sigmoid) -> OperatorOffset {
  // Emulate the hardSigmoid operation with function `y = max(0, min(1, alpha *
  // x + beta))` that is applied to the input tensor element-wise.
  //
  // The subexpression `alpha * x + beta` is considered a linear operation.
  const mojom::Operand& input_operand =
      GetOperand(hard_sigmoid.input_operand_id);
  CHECK(input_operand.descriptor.data_type() == OperandDataType::kFloat16 ||
        input_operand.descriptor.data_type() == OperandDataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const int32_t output_tensor_index_of_linear =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeLinearOperation(
      *signed_input_dimensions, input_tensor_type,
      operand_to_index_map_.at(hard_sigmoid.input_operand_id),
      output_tensor_index_of_linear, hard_sigmoid.alpha, hard_sigmoid.beta));

  // The expression `max(0, min(1, linear))` can be implemented with TFLite
  // RELU_0_TO_1 operator.
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_RELU_0_TO_1, output_tensor_index_of_linear,
      operand_to_index_map_.at(hard_sigmoid.output_operand_id));
}

auto GraphBuilderTflite::SerializeHardSwish(const mojom::HardSwish& hard_swish)
    -> OperatorOffset {
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_HARD_SWISH,
      operand_to_index_map_.at(hard_swish.input_operand_id),
      operand_to_index_map_.at(hard_swish.output_operand_id));
}

std::tuple<int32_t, int32_t>
GraphBuilderTflite::ComputeMeanAndVarianceForNormalization(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> spatial_dimensions) {
  // Get mean values with reduceMean over the spatial dimensions of the input.
  std::vector<int32_t> reduce_dimensions(input_dimensions.begin(),
                                         input_dimensions.end());
  for (auto dimension : spatial_dimensions) {
    reduce_dimensions[dimension] = 1;
  }
  const int32_t mean_tensor_index =
      SerializeTemporaryTensor(reduce_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeReduceOperation(
      ::tflite::BuiltinOperator_MEAN, input_tensor_index, mean_tensor_index,
      spatial_dimensions, /*keep_dimensions=*/true));

  // Get variance with expression `Variance = ReduceMean(Pow(Input - Mean, 2))`
  // over the spatial dimensions of the input.
  const int32_t output_tensor_index_of_sub =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, input_tensor_index, mean_tensor_index,
      output_tensor_index_of_sub));
  const int32_t pow_constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{2.0},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_pow =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW, output_tensor_index_of_sub,
      pow_constant_tensor_index, output_tensor_index_of_pow));
  const int32_t variance_tensor_index =
      SerializeTemporaryTensor(reduce_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeReduceOperation(
      ::tflite::BuiltinOperator_MEAN, output_tensor_index_of_pow,
      variance_tensor_index, spatial_dimensions, /*keep_dimensions=*/true));

  return std::make_tuple(mean_tensor_index, variance_tensor_index);
}

int32_t GraphBuilderTflite::TransposeAndReshapeLayerNormalizationScaleBias(
    base::span<const int32_t> input_dimensions,
    uint64_t scale_or_bias_operand_id,
    base::span<const uint32_t> axes) {
  const mojom::Operand& scale_or_bias_operand =
      GetOperand(scale_or_bias_operand_id);
  // The shape has been validated to not overflow before creating tensor.
  const auto scale_or_bias_dimensions =
      ToSignedDimensions(scale_or_bias_operand.descriptor.shape());
  CHECK(scale_or_bias_dimensions.has_value());
  const ::tflite::TensorType scale_or_bias_tensor_type =
      OperandDataTypeToTFLite(scale_or_bias_operand.descriptor.data_type());
  const int32_t scale_or_bias_tensor_index =
      operand_to_index_map_.at(scale_or_bias_operand_id);
  std::vector<int32_t> compatible_shape(input_dimensions.size(), 1);
  for (auto axis : axes) {
    compatible_shape[axis] = input_dimensions[axis];
  }

  // The shape of the scale and bias tensors is determined by the axes selected
  // from the input tensor. These tensors need to be reshaped and/or transposed
  // so that they can be element-wise multiplied (for scale) or added (for bias)
  // during normalization.
  //
  // For example, if the input shape is [2, 1, 4, 3] and the axes are [3, 1, 2]
  // then the scale shape will be [3, 1, 4]. The scale shape needs to be
  // transposed to [1, 4, 3] and then reshaped to [1, 1, 4, 3].
  std::optional<int32_t> transpose_tensor_index;
  const std::vector<uint32_t> sorted_indices = GetIndexOfSortedValue(axes);
  if (!base::ranges::is_sorted(sorted_indices)) {
    transpose_tensor_index = InsertTransposeOperation(
        *scale_or_bias_dimensions, scale_or_bias_tensor_type,
        scale_or_bias_tensor_index, sorted_indices);
  }

  const int32_t reshape_tensor_index =
      SerializeTemporaryTensor(compatible_shape, scale_or_bias_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      transpose_tensor_index ? *transpose_tensor_index
                             : scale_or_bias_tensor_index,
      reshape_tensor_index, compatible_shape));

  return reshape_tensor_index;
}

auto GraphBuilderTflite::SerializeInstanceNormalization(
    const mojom::InstanceNormalization& instance_normalization)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand =
      GetOperand(instance_normalization.input_operand_id);
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  if (input_operand.descriptor.data_type() == OperandDataType::kFloat16) {
    return base::unexpected("The 16-bit float data type is not supported.");
  }
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  CHECK_EQ(signed_input_dimensions->size(), 4u);
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  std::array<int32_t, 2> spatial_dimensions;
  uint32_t channel_axis;
  switch (instance_normalization.layout) {
    case mojom::InputOperandLayout::kChannelsFirst: {
      spatial_dimensions = {2, 3};
      channel_axis = 1;
      break;
    }
    case mojom::InputOperandLayout::kChannelsLast:
      spatial_dimensions = {1, 2};
      channel_axis = 3;
      break;
  }
  std::vector<int32_t> new_shape(signed_input_dimensions->size(), 1);
  new_shape[channel_axis] = (*signed_input_dimensions)[channel_axis];

  const int32_t input_tensor_index =
      operand_to_index_map_.at(instance_normalization.input_operand_id);
  const auto [mean_tensor_index, variance_tensor_index] =
      ComputeMeanAndVarianceForNormalization(
          *signed_input_dimensions, input_tensor_type, input_tensor_index,
          spatial_dimensions);

  // Reshape the 1-D tensor of the scale operand to the new shape if needed.
  std::optional<int32_t> reshape_scale_tensor_index;
  if (instance_normalization.scale_operand_id) {
    const mojom::Operand& scale_operand =
        GetOperand(*instance_normalization.scale_operand_id);
    CHECK_EQ(scale_operand.descriptor.Rank(), 1u);
    reshape_scale_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        operand_to_index_map_.at(*instance_normalization.scale_operand_id),
        *reshape_scale_tensor_index, new_shape));
  }

  // Reshape the 1-D tensor of the bias operand to the new shape if needed.
  std::optional<int32_t> reshape_bias_tensor_index;
  if (instance_normalization.bias_operand_id) {
    const mojom::Operand& bias_operand =
        GetOperand(*instance_normalization.bias_operand_id);
    CHECK_EQ(bias_operand.descriptor.Rank(), 1u);
    reshape_bias_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        operand_to_index_map_.at(*instance_normalization.bias_operand_id),
        *reshape_bias_tensor_index, new_shape));
  }

  return SerializeNormalizationOperation(
      *signed_input_dimensions, input_tensor_type, input_tensor_index,
      operand_to_index_map_.at(instance_normalization.output_operand_id),
      mean_tensor_index, variance_tensor_index, instance_normalization.epsilon,
      reshape_scale_tensor_index, reshape_bias_tensor_index);
}

auto GraphBuilderTflite::SerializeLayerNormalization(
    const mojom::LayerNormalization& layer_normalization)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand =
      GetOperand(layer_normalization.input_operand_id);
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  if (input_operand.descriptor.data_type() == OperandDataType::kFloat16) {
    return base::unexpected("The 16-bit float data type is not supported.");
  }
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());

  // Get mean and variance values with reduceMean on the fly across all the
  // input features of each individual sample in the batch.
  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_axes,
                   ToSignedDimensions(layer_normalization.axes));
  const int32_t input_tensor_index =
      operand_to_index_map_.at(layer_normalization.input_operand_id);
  const auto [mean_tensor_index, variance_tensor_index] =
      ComputeMeanAndVarianceForNormalization(*signed_input_dimensions,
                                             input_tensor_type,
                                             input_tensor_index, signed_axes);

  std::optional<int32_t> scale_tensor_index;
  if (layer_normalization.scale_operand_id) {
    scale_tensor_index = TransposeAndReshapeLayerNormalizationScaleBias(
        *signed_input_dimensions, *layer_normalization.scale_operand_id,
        layer_normalization.axes);
  }

  std::optional<int32_t> bias_tensor_index;
  if (layer_normalization.bias_operand_id) {
    bias_tensor_index = TransposeAndReshapeLayerNormalizationScaleBias(
        *signed_input_dimensions, *layer_normalization.bias_operand_id,
        layer_normalization.axes);
  }

  return SerializeNormalizationOperation(
      *signed_input_dimensions, input_tensor_type, input_tensor_index,
      operand_to_index_map_.at(layer_normalization.output_operand_id),
      mean_tensor_index, variance_tensor_index, layer_normalization.epsilon,
      scale_tensor_index, bias_tensor_index);
}

auto GraphBuilderTflite::SerializeLeakyRelu(const mojom::LeakyRelu& leaky_relu)
    -> OperatorOffset {
  const auto leaky_rely_options =
      ::tflite::CreateLeakyReluOptions(builder_, leaky_relu.alpha);

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_LEAKY_RELU,
      operand_to_index_map_.at(leaky_relu.input_operand_id),
      operand_to_index_map_.at(leaky_relu.output_operand_id),
      ::tflite::BuiltinOptions_LeakyReluOptions, leaky_rely_options.Union());
}

auto GraphBuilderTflite::SerializeLinear(const mojom::Linear& linear)
    -> OperatorOffset {
  const auto& input_operand = GetOperand(linear.input_operand_id);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  return SerializeLinearOperation(
      *signed_input_dimensions,
      OperandDataTypeToTFLite(input_operand.descriptor.data_type()),
      operand_to_index_map_.at(linear.input_operand_id),
      operand_to_index_map_.at(linear.output_operand_id), linear.alpha,
      linear.beta);
}

auto GraphBuilderTflite::SerializeLogicalNot(
    const mojom::ElementWiseUnary& logical_not) -> OperatorOffset {
  // The data type of WebNN LogicalNot operation is uint8, but TFLite LogicalNot
  // builtin operation needs bool type, so a cast operation need to be inserted
  // before LogicalNot to convert uint8 to bool for input tensor and a cast
  // operation after LogicalNot to convert bool to uint8 for output tensor.
  //
  // Create two temporary tensors with bool type for TFLite LogicalNot.
  std::array<int32_t, 2> bool_tensor_indexes;
  const auto& input_operand = GetOperand(logical_not.input_operand_id);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  for (auto& bool_tensor_index : bool_tensor_indexes) {
    bool_tensor_index = SerializeTemporaryTensor(*signed_input_dimensions,
                                                 ::tflite::TensorType_BOOL);
  }

  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kUint8);
  operators_.emplace_back(SerializeCastOperation(
      operand_to_index_map_.at(logical_not.input_operand_id),
      /*input_tensor_type=*/::tflite::TensorType_UINT8, bool_tensor_indexes[0],
      /*output_tensor_type=*/::tflite::TensorType_BOOL));

  // Serialize TFLite LogicalNot operation.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_LOGICAL_NOT);
  const std::array<int32_t, 1> op_inputs = {bool_tensor_indexes[0]};
  const std::array<int32_t, 1> op_outputs = {bool_tensor_indexes[1]};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs)));

  return SerializeCastOperation(
      bool_tensor_indexes[1],
      /*input_tensor_type=*/::tflite::TensorType_BOOL,
      operand_to_index_map_.at(logical_not.output_operand_id),
      /*output_tensor_type=*/::tflite::TensorType_UINT8);
}

auto GraphBuilderTflite::SerializeLstmCell(const mojom::LstmCell& lstm_cell)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand = GetOperand(lstm_cell.input_operand_id);
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  if (input_operand.descriptor.data_type() == OperandDataType::kFloat16) {
    return base::unexpected("The 16-bit float data type is not supported.");
  }
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  CHECK_EQ(signed_input_dimensions->size(), 2u);
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const auto checked_hidden_size =
      base::MakeCheckedNum<int32_t>(lstm_cell.hidden_size);
  if (!checked_hidden_size.IsValid()) {
    return base::unexpected("The hidden size is too large.");
  }

  std::optional<int32_t> bias_tensor_index;
  if (lstm_cell.bias_operand_id) {
    bias_tensor_index = operand_to_index_map_.at(*lstm_cell.bias_operand_id);
  }
  std::optional<int32_t> recurrent_bias_tensor_index;
  if (lstm_cell.recurrent_bias_operand_id) {
    recurrent_bias_tensor_index =
        operand_to_index_map_.at(*lstm_cell.recurrent_bias_operand_id);
  }
  std::optional<int32_t> peephole_weight_tensor_index;
  if (lstm_cell.peephole_weight_operand_id) {
    peephole_weight_tensor_index =
        operand_to_index_map_.at(*lstm_cell.peephole_weight_operand_id);
  }

  std::vector<int32_t> output_tensor_indices;
  output_tensor_indices.reserve(lstm_cell.output_operand_ids.size());
  base::ranges::transform(lstm_cell.output_operand_ids,
                          std::back_inserter(output_tensor_indices),
                          [&](uint64_t operand_id) {
                            return operand_to_index_map_.at(operand_id);
                          });

  LstmCellOperation lstm_cell_operation(
      *signed_input_dimensions, input_tensor_type,
      operand_to_index_map_.at(lstm_cell.input_operand_id),
      output_tensor_indices,
      operand_to_index_map_.at(lstm_cell.weight_operand_id),
      operand_to_index_map_.at(lstm_cell.recurrent_weight_operand_id),
      bias_tensor_index, recurrent_bias_tensor_index,
      operand_to_index_map_.at(lstm_cell.hidden_state_operand_id),
      checked_hidden_size.ValueOrDie(),
      operand_to_index_map_.at(lstm_cell.cell_state_operand_id),
      peephole_weight_tensor_index, lstm_cell.layout, lstm_cell.activations);

  return SerializeLstmCellOperation(lstm_cell_operation);
}

int32_t GraphBuilderTflite::GetInitialHiddenAndCellState(
    std::optional<uint64_t> state_operand_id,
    base::span<const int32_t> state_dimensions) {
  int32_t state_tensor_index;
  if (state_operand_id) {
    state_tensor_index = operand_to_index_map_.at(*state_operand_id);
  } else {
    CHECK_EQ(state_dimensions.size(), 3u);
    const std::vector<float> initial_hidden_state_value(
        std::accumulate(state_dimensions.begin(), state_dimensions.end(), 1,
                        std::multiplies()));
    state_tensor_index = SerializeTensorWithBuffer<float>(
        initial_hidden_state_value, state_dimensions);
  }
  return state_tensor_index;
}

int32_t GraphBuilderTflite::ReshapeHiddenAndCellState(
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> new_shape,
    std::optional<int32_t> concat_input_tensor_index,
    base::span<const int32_t> concat_output_shape) {
  // Reshape the output hidden or cell state tensor of gru / lstm cell to the
  // 3-D tensor [1, batchSize, hiddenSize].
  const int32_t out_tensor_index_of_shape =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      input_tensor_index, out_tensor_index_of_shape, new_shape));
  if (concat_input_tensor_index) {
    const int32_t concat_output_tensor_index =
        SerializeTemporaryTensor(concat_output_shape, input_tensor_type);
    operators_.emplace_back(SerializeConcatOperation(
        std::array<int32_t, 2>(
            {*concat_input_tensor_index, out_tensor_index_of_shape}),
        concat_output_tensor_index, /*axis=*/0));
    return concat_output_tensor_index;
  }

  return out_tensor_index_of_shape;
}

auto GraphBuilderTflite::SerializeMatmul(const mojom::Matmul& matmul)
    -> OperatorOffset {
  const OperandDataType a_operand_data_type =
      GetOperand(matmul.a_operand_id).descriptor.data_type();
  CHECK(kFloatDataTypes.contains(a_operand_data_type));

  return SerializeMatmulOperation(
      operand_to_index_map_.at(matmul.a_operand_id),
      operand_to_index_map_.at(matmul.b_operand_id),
      operand_to_index_map_.at(matmul.output_operand_id));
}

auto GraphBuilderTflite::SerializePad(const mojom::Pad& pad)
    -> base::expected<OperatorOffset, std::string> {
  CHECK_EQ(pad.beginning_padding.size(), pad.ending_padding.size());

  std::vector<int32_t> paddings;
  paddings.resize(pad.beginning_padding.size() * 2);
  for (size_t i = 0; i < pad.beginning_padding.size(); ++i) {
    auto checked_pre_padding =
        base::MakeCheckedNum<int32_t>(pad.beginning_padding[i]);
    auto checked_post_padding =
        base::MakeCheckedNum<int32_t>(pad.ending_padding[i]);
    if (!checked_pre_padding.IsValid() || !checked_post_padding.IsValid()) {
      return base::unexpected("The padding is too large.");
    }
    paddings[i * 2] = checked_pre_padding.ValueOrDie();
    paddings[i * 2 + 1] = checked_post_padding.ValueOrDie();
  }

  // The shape of padding is [n, 2], where n is the rank of input as described
  // here https://www.tensorflow.org/mlir/tfl_ops#tflmirror_pad_tflmirrorpadop.
  std::array<int32_t, 2> paddings_shape{
      {base::checked_cast<int32_t>(pad.beginning_padding.size()), 2}};
  const int32_t paddings_index =
      SerializeTensorWithBuffer<int32_t>(paddings, paddings_shape);

  std::vector<int32_t> op_inputs = {
      operand_to_index_map_.at(pad.input_operand_id), paddings_index};

  ::tflite::BuiltinOptions builtin_options_type =
      ::tflite::BuiltinOptions::BuiltinOptions_NONE;
  flatbuffers::Offset<void> builtin_options;
  ::tflite::BuiltinOperator operator_code;
  switch (pad.mode->which()) {
    case mojom::PaddingMode::Tag::kConstant: {
      operator_code = ::tflite::BuiltinOperator::BuiltinOperator_PADV2;
      builtin_options_type =
          ::tflite::BuiltinOptions::BuiltinOptions_PadV2Options;
      builtin_options = ::tflite::CreatePadV2Options(builder_).Union();

      // Add the padding value as an input.
      //
      // TODO: crbug.com/328567884 - This is not correct to always use floats,
      // though for now WebNN only supports passing a float32 constant value.
      // https://www.tensorflow.org/mlir/tfl_ops#tflpadv2_tflpadv2op specifies
      // that this constant value should match the type of the input operand.
      const std::array<float, 1> padding_value_buffer = {
          pad.mode->get_constant()->value};
      const std::array<int32_t, 1> padding_value_dimensions = {1};
      const int32_t padding_value_index = SerializeTensorWithBuffer<float>(
          padding_value_buffer, padding_value_dimensions);
      op_inputs.push_back(padding_value_index);
      break;
    }
    case mojom::PaddingMode::Tag::kEdge:
      // TODO: crbug.com/328547551 - Support the edge padding mode.
      return base::unexpected(
          "The edge padding mode is not supported in tflite schema.");
    case mojom::PaddingMode::Tag::kReflection: {
      operator_code = ::tflite::BuiltinOperator::BuiltinOperator_MIRROR_PAD;
      builtin_options_type =
          ::tflite::BuiltinOptions::BuiltinOptions_MirrorPadOptions;
      builtin_options = ::tflite::CreateMirrorPadOptions(
                            builder_, ::tflite::MirrorPadMode_REFLECT)
                            .Union();
      break;
    }
    case mojom::PaddingMode::Tag::kSymmetric: {
      operator_code = ::tflite::BuiltinOperator::BuiltinOperator_MIRROR_PAD;
      builtin_options_type =
          ::tflite::BuiltinOptions::BuiltinOptions_MirrorPadOptions;
      builtin_options = ::tflite::CreateMirrorPadOptions(
                            builder_, ::tflite::MirrorPadMode_SYMMETRIC)
                            .Union();
      break;
    }
  }

  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(pad.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializePool2d(const mojom::Pool2d& pool2d)
    -> base::expected<OperatorOffset, std::string> {
  // The dilations are not supported in tflite schema.
  if (pool2d.dilations->height != 1 || pool2d.dilations->width != 1) {
    return base::unexpected("Pool2d in tflite doesn't support dilations.");
  }

  const mojom::Operand& input_operand = GetOperand(pool2d.input_operand_id);
  const auto& input_shape = input_operand.descriptor.shape();
  CHECK_EQ(input_shape.size(), 4u);
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_shape[1],
                                                .width = input_shape[2]};
  webnn::Size2d<uint32_t> filter_size2d = {
      .height = pool2d.window_dimensions->height,
      .width = pool2d.window_dimensions->width};
  ASSIGN_OR_RETURN(
      TfLitePadding padding_mode,
      GetTfLitePaddingMode(*pool2d.padding, input_size2d, filter_size2d,
                           *pool2d.strides, *pool2d.dilations,
                           /*is_transposed_conv2d=*/false));
  // Insert a Pad operator before TfLite Pool2d if needed for explicit padding.
  std::optional<int32_t> explicit_pad_index;
  const int32_t input_index = operand_to_index_map_.at(pool2d.input_operand_id);
  if (padding_mode.paddings) {
    ASSIGN_OR_RETURN(explicit_pad_index,
                     InsertPadOperation(input_operand, input_index,
                                        padding_mode.paddings.value()));
  }

  ::tflite::BuiltinOperator operator_code;
  switch (pool2d.kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()));
      operator_code = ::tflite::BuiltinOperator_AVERAGE_POOL_2D;
      break;
    case mojom::Pool2d::Kind::kMaxPool2d:
      operator_code = ::tflite::BuiltinOperator_MAX_POOL_2D;
      break;
    case mojom::Pool2d::Kind::kL2Pool2d:
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()));
      return base::unexpected("L2Pool2d is not supported in tflite.");
  }

  const auto pool_2d_options = ::tflite::CreatePool2DOptions(
      builder_, padding_mode.mode, pool2d.strides->width,
      pool2d.strides->height, filter_size2d.width, filter_size2d.height,
      ::tflite::ActivationFunctionType_NONE);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 1> op_inputs = {
      explicit_pad_index ? explicit_pad_index.value() : input_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(pool2d.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_Pool2DOptions, pool_2d_options.Union());
}

auto GraphBuilderTflite::SerializePrelu(const mojom::Prelu& prelu)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand = GetOperand(prelu.input_operand_id);
  CHECK(input_operand.descriptor.data_type() == OperandDataType::kFloat32 ||
        input_operand.descriptor.data_type() == OperandDataType::kFloat16 ||
        input_operand.descriptor.data_type() == OperandDataType::kInt32 ||
        input_operand.descriptor.data_type() == OperandDataType::kInt8);
  const mojom::Operand& slope_operand = GetOperand(prelu.slope_operand_id);
  // `ValidatePreluAndInferOutput` function has checked broadcastable shapes
  // between input and slope operand, but TFLite XNNPACK delegate doesn't
  // support to broadcast last dimension.
  // TODO(crbug.com/335517470): Support last dimension broadcastable.
  if (input_operand.descriptor.Rank() != 0 &&
      slope_operand.descriptor.Rank() != 0 &&
      input_operand.descriptor.shape().back() !=
          slope_operand.descriptor.shape().back()) {
    return base::unexpected(
        "The input and slope should have the same last dimension.");
  }

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_PRELU);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(prelu.input_operand_id),
      operand_to_index_map_.at(prelu.slope_operand_id)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(prelu.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeReciprocal(
    const mojom::ElementWiseUnary& reciprocal)
    -> base::expected<OperatorOffset, std::string> {
  // Emulate the reciprocal operation whose calculation follows the expression
  // `1 / x`.
  //
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  const mojom::Operand& input_operand = GetOperand(reciprocal.input_operand_id);
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);

  const int32_t constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1.0},
      /*dimensions=*/{});

  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, constant_tensor_index,
      operand_to_index_map_.at(reciprocal.input_operand_id),
      operand_to_index_map_.at(reciprocal.output_operand_id));
}

auto GraphBuilderTflite::SerializeReduce(const mojom::Reduce& reduce)
    -> base::expected<OperatorOffset, std::string> {
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  const mojom::Operand& input_operand = GetOperand(reduce.input_operand_id);
  if (input_operand.descriptor.data_type() == OperandDataType::kFloat16) {
    return base::unexpected("The 16-bit float data type isn't supported.");
  }

  // Serialize the axes tensor to reduce input tensor.
  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_axes,
                   ToSignedDimensions(reduce.axes));

  ::tflite::BuiltinOperator operator_code;
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  int32_t input_tensor_index =
      operand_to_index_map_.at(reduce.input_operand_id);
  switch (reduce.kind) {
    case mojom::Reduce::Kind::kMax:
      operator_code = ::tflite::BuiltinOperator_REDUCE_MAX;
      break;
    case mojom::Reduce::Kind::kMean:
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()));
      operator_code = ::tflite::BuiltinOperator_MEAN;
      break;
    case mojom::Reduce::Kind::kMin:
      operator_code = ::tflite::BuiltinOperator_REDUCE_MIN;
      break;
    case mojom::Reduce::Kind::kProduct:
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()) ||
            k32BitIntegerDataTypes.contains(
                input_operand.descriptor.data_type()) ||
            k64BitIntegerDataTypes.contains(
                input_operand.descriptor.data_type()));
      operator_code = ::tflite::BuiltinOperator_REDUCE_PROD;
      break;
    case mojom::Reduce::Kind::kSum:
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()) ||
            k32BitIntegerDataTypes.contains(
                input_operand.descriptor.data_type()) ||
            k64BitIntegerDataTypes.contains(
                input_operand.descriptor.data_type()));
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    case mojom::Reduce::Kind::kLogSum:
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()));
      // The reduceLogSum can be emulated with appending log operation after
      // reduceSum.
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    case mojom::Reduce::Kind::kLogSumExp: {
      // The reduceLogSumExp can be emulated with adding exp operation before
      // reduceSum and appending log operation after the reduceSum.
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()));
      const int32_t output_tensor_index_of_exp =
          SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
      operators_.emplace_back(SerializeUnaryOperation(
          ::tflite::BuiltinOperator_EXP, input_tensor_index,
          output_tensor_index_of_exp));
      input_tensor_index = output_tensor_index_of_exp;
      // A log operation will be appended after the reduce sum.
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    }
    case mojom::Reduce::Kind::kL2: {
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()));
      // The reduceL2 can be emulated with appending pow(x, 0.5) operation after
      // reduceSumSquare.
      const int32_t output_tensor_index_of_sum =
          SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
      ASSIGN_OR_RETURN(
          OperatorOffset operator_offset,
          SerializeReduceSumSquare(reduce, output_tensor_index_of_sum));
      operators_.emplace_back(operator_offset);
      CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
      const int32_t pow_constant_tensor_index =
          SerializeTensorWithBuffer<float>(
              /*buffer=*/std::array<float, 1>{0.5},
              /*dimensions=*/{});
      return SerializeBinaryOperation(
          ::tflite::BuiltinOperator_POW, output_tensor_index_of_sum,
          pow_constant_tensor_index,
          operand_to_index_map_.at(reduce.output_operand_id));
    }
    case mojom::Reduce::Kind::kSumSquare: {
      // The reduceSumSquare can be emulated with adding pow operation before
      // reduceSum.
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()) ||
            k32BitIntegerDataTypes.contains(
                input_operand.descriptor.data_type()) ||
            k64BitIntegerDataTypes.contains(
                input_operand.descriptor.data_type()));
      return SerializeReduceSumSquare(
          reduce, operand_to_index_map_.at(reduce.output_operand_id));
    }
    case mojom::Reduce::Kind::kL1: {
      CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()) ||
            k32BitIntegerDataTypes.contains(
                input_operand.descriptor.data_type()) ||
            k64BitIntegerDataTypes.contains(
                input_operand.descriptor.data_type()));
      if (input_operand.descriptor.data_type() == OperandDataType::kUint32 ||
          input_operand.descriptor.data_type() == OperandDataType::kUint64) {
        return base::unexpected(base::StrCat(
            {DataTypeToString(input_operand.descriptor.data_type()),
             " is not supported."}));
      }
      // The reduceL1 can be emulated with adding abs operation before
      // reduceSum.
      const int32_t output_tensor_index_of_abs =
          SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
      operators_.emplace_back(SerializeUnaryOperation(
          ::tflite::BuiltinOperator_ABS, input_tensor_index,
          output_tensor_index_of_abs));
      input_tensor_index = output_tensor_index_of_abs;
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    }
  }

  if (reduce.kind == mojom::Reduce::Kind::kLogSum ||
      reduce.kind == mojom::Reduce::Kind::kLogSumExp) {
    const int32_t output_tensor_index_of_sum =
        SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
    operators_.emplace_back(SerializeReduceOperation(
        operator_code, input_tensor_index, output_tensor_index_of_sum,
        signed_axes, reduce.keep_dimensions));
    return SerializeUnaryOperation(
        ::tflite::BuiltinOperator_LOG, output_tensor_index_of_sum,
        operand_to_index_map_.at(reduce.output_operand_id));
  }

  return SerializeReduceOperation(
      operator_code, input_tensor_index,
      operand_to_index_map_.at(reduce.output_operand_id), signed_axes,
      reduce.keep_dimensions);
}

auto GraphBuilderTflite::SerializeReduceSumSquare(const mojom::Reduce& reduce,
                                                  int32_t output_tensor_index)
    -> base::expected<OperatorOffset, std::string> {
  // The reduceSumSquare can be emulated with adding pow operation before
  // reduceSum.
  // Serialize the axes tensor to reduce input tensor.
  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_axes,
                   ToSignedDimensions(reduce.axes));

  // The input shape has been validated to not overflow before creating tensor.
  const mojom::Operand& input_operand = GetOperand(reduce.input_operand_id);
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  int32_t pow_constant_tensor_index;
  if (input_operand.descriptor.data_type() == OperandDataType::kFloat32) {
    pow_constant_tensor_index = SerializeTensorWithBuffer<float>(
        /*buffer=*/std::array<float, 1>{2.0},
        /*dimensions=*/{});
  } else if (input_operand.descriptor.data_type() == OperandDataType::kInt32) {
    pow_constant_tensor_index = SerializeTensorWithBuffer<int32_t>(
        /*buffer=*/std::array<int32_t, 1>{2},
        /*dimensions=*/{});
  } else {
    return base::unexpected(
        base::StrCat({DataTypeToString(input_operand.descriptor.data_type()),
                      " is not supported."}));
  }
  const int32_t output_tensor_index_of_pow =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW,
      operand_to_index_map_.at(reduce.input_operand_id),
      pow_constant_tensor_index, output_tensor_index_of_pow));

  return SerializeReduceOperation(
      ::tflite::BuiltinOperator_SUM, output_tensor_index_of_pow,
      output_tensor_index, signed_axes, reduce.keep_dimensions);
}

auto GraphBuilderTflite::SerializeRelu(const mojom::Relu& relu)
    -> OperatorOffset {
  // TODO(crbug.com/354625677): Support 32-bit signed integer with
  // TFL::MaximumOp
  // https://www.tensorflow.org/mlir/tfl_ops#tflmaximum_tflmaximumop.
  CHECK_EQ(GetOperand(relu.input_operand_id).descriptor.data_type(),
           OperandDataType::kFloat32);

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator::BuiltinOperator_RELU,
      operand_to_index_map_.at(relu.input_operand_id),
      operand_to_index_map_.at(relu.output_operand_id));
}

auto GraphBuilderTflite::SerializeResample2d(
    const mojom::Resample2d& resample2d)
    -> base::expected<OperatorOffset, std::string> {
  // TODO: crbug.com/329543543 - `resample2d.scales` is dropped on the floor.

  const mojom::Operand& input_operand = GetOperand(resample2d.input_operand_id);
  CHECK(kFloatDataTypes.contains(input_operand.descriptor.data_type()));
  const std::array<uint32_t, 2> supported_axes = {1, 2};
  if (!base::ranges::equal(resample2d.axes, supported_axes)) {
    // TODO: crbug.com/329658123: Support axes of {0, 1} and {2, 3}.
    return base::unexpected(
        "Resample2d only supports axes = {1, 2} in tflite schema.");
  }

  // Create tflite builtin options for resize mode that is align_corner = false
  // and half_pixel_center = true by default. WebNN will support coordinate
  // transformation modes for Resample2d and it's tracked by the issue:
  // https://github.com/webmachinelearning/webnn/issues/270.
  ::tflite::BuiltinOperator operator_code;
  ::tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  switch (resample2d.mode) {
    case mojom::Resample2d::InterpolationMode::kNearestNeighbor:
      operator_code = ::tflite::BuiltinOperator_RESIZE_NEAREST_NEIGHBOR;
      builtin_options_type =
          ::tflite::BuiltinOptions_ResizeNearestNeighborOptions;
      builtin_options = ::tflite::CreateResizeNearestNeighborOptions(
                            builder_, /*align_corners=*/false,
                            /*half_pixel_centers=*/true)
                            .Union();
      break;
    case mojom::Resample2d::InterpolationMode::kLinear:
      operator_code = ::tflite::BuiltinOperator_RESIZE_BILINEAR;
      builtin_options_type = ::tflite::BuiltinOptions_ResizeBilinearOptions;
      builtin_options = ::tflite::CreateResizeBilinearOptions(
                            builder_, /*align_corners=*/false,
                            /*half_pixel_centers=*/true)
                            .Union();
      break;
  }

  // Serialize the target sizes for the dimensions [OutputHeight, OutputWidth].
  ASSIGN_OR_RETURN(
      std::vector<int32_t> signed_output_dimensions,
      ToSignedDimensions(
          GetOperand(resample2d.output_operand_id).descriptor.shape()));
  CHECK_EQ(signed_output_dimensions.size(), 4u);

  int32_t output_height = signed_output_dimensions[resample2d.axes[0]];
  int32_t output_width = signed_output_dimensions[resample2d.axes[1]];

  const std::array<int32_t, 2> resize_data = {output_height, output_width};
  const std::array<int32_t, 1> resize_shape = {resize_data.size()};
  const int32_t resize_tensor_index =
      SerializeTensorWithBuffer<int32_t>(resize_data, resize_shape);

  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(resample2d.input_operand_id),
      resize_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(resample2d.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializeReshape(uint64_t input_operand_id,
                                          uint64_t output_operand_id)
    -> base::expected<OperatorOffset, std::string> {
  // Get the shape of the output tensor, such that this operator can reshape the
  // input to it.
  const mojom::Operand& output_operand = GetOperand(output_operand_id);
  ASSIGN_OR_RETURN(std::vector<int32_t> signed_output_dimensions,
                   ToSignedDimensions(output_operand.descriptor.shape()));

  return SerializeReshapeOperation(operand_to_index_map_.at(input_operand_id),
                                   operand_to_index_map_.at(output_operand_id),
                                   std::move(signed_output_dimensions));
}

auto GraphBuilderTflite::SerializeSigmoid(const mojom::Sigmoid& sigmoid)
    -> OperatorOffset {
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_LOGISTIC,
      operand_to_index_map_.at(sigmoid.input_operand_id),
      operand_to_index_map_.at(sigmoid.output_operand_id));
}

auto GraphBuilderTflite::SerializeSlice(const mojom::Slice& slice)
    -> base::expected<OperatorOffset, std::string> {
  // The number of starts and sizes are the same as input rank that is verified
  // in ValidateSliceAndInferOutput() function.
  std::vector<int32_t> slice_starts;
  slice_starts.reserve(slice.starts_and_sizes.size());
  std::vector<int32_t> slice_sizes;
  slice_sizes.reserve(slice.starts_and_sizes.size());
  for (auto& start_and_size : slice.starts_and_sizes) {
    auto checked_start = base::MakeCheckedNum<int32_t>(start_and_size->start);
    auto checked_size = base::MakeCheckedNum<int32_t>(start_and_size->size);
    if (!checked_start.IsValid() || !checked_size.IsValid()) {
      return base::unexpected("The start or size of slice is too large.");
    }
    slice_starts.push_back(checked_start.ValueOrDie());
    slice_sizes.push_back(checked_size.ValueOrDie());
  }

  return SerializeSliceOperation(
      operand_to_index_map_.at(slice.input_operand_id),
      operand_to_index_map_.at(slice.output_operand_id), slice_starts,
      slice_sizes);
}

auto GraphBuilderTflite::SerializeSoftmax(const mojom::Softmax& softmax)
    -> OperatorOffset {
  const auto& input_operand = GetOperand(softmax.input_operand_id);
  // The input shape has been validated to not overflow before creating tensor.
  auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const size_t input_rank = signed_input_dimensions->size();

  const auto softmax_options =
      ::tflite::CreateSoftmaxOptions(builder_, /*beta=*/1.0);
  if (softmax.axis == input_rank - 1) {
    // The axis is the last dimension, so the softmax operation can be directly
    // serialized.
    return SerializeUnaryOperation(
        ::tflite::BuiltinOperator_SOFTMAX,
        operand_to_index_map_.at(softmax.input_operand_id),
        operand_to_index_map_.at(softmax.output_operand_id),
        ::tflite::BuiltinOptions_SoftmaxOptions, softmax_options.Union());
  }
  // Transpose the input tensor to make the axis to be the last dimension.
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  std::vector<uint32_t> permutation(input_rank);
  std::iota(permutation.begin(), permutation.end(), 0);
  std::swap(permutation[softmax.axis], permutation[input_rank - 1]);
  std::vector<int32_t> transpose_dimensions = *signed_input_dimensions;
  std::swap(transpose_dimensions[softmax.axis],
            transpose_dimensions[input_rank - 1]);

  const int32_t output_tensor_index_of_transpose =
      SerializeTemporaryTensor(transpose_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeTransposeOperation(
      operand_to_index_map_.at(softmax.input_operand_id),
      output_tensor_index_of_transpose, permutation));

  // Perform softmax.
  const int32_t output_tensor_index_of_softmax =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(
      ::tflite::BuiltinOperator_SOFTMAX, output_tensor_index_of_transpose,
      output_tensor_index_of_softmax, ::tflite::BuiltinOptions_SoftmaxOptions,
      softmax_options.Union()));

  // Transpose the last dimension back to the original axis.
  return SerializeTransposeOperation(
      output_tensor_index_of_softmax,
      operand_to_index_map_.at(softmax.output_operand_id), permutation);
}

auto GraphBuilderTflite::SerializeSoftplus(const mojom::Softplus& softplus)
    -> base::expected<OperatorOffset, std::string> {
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  const mojom::Operand& input_operand = GetOperand(softplus.input_operand_id);
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);

  // Emulate the softplus operation whose calculation follows the expression
  // `ln(1 + exp(x))`.
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const int32_t output_tensor_index_of_exp =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(
      ::tflite::BuiltinOperator_EXP,
      operand_to_index_map_.at(softplus.input_operand_id),
      output_tensor_index_of_exp));

  // Add constant value `1` to the output tensor of element-wise exp operation.
  // TODO(crbug.com/339654398): Convert the 32-bit floating-point data to 16-bit
  // floating-point data with fp16_ieee_from_fp32_value function if some
  // delegates support 16-bit float inference.
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
  const int32_t constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_add =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, constant_tensor_index,
      output_tensor_index_of_exp, output_tensor_index_of_add));

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_LOG, output_tensor_index_of_add,
      operand_to_index_map_.at(softplus.output_operand_id));
}

auto GraphBuilderTflite::SerializeSoftsign(const mojom::Softsign& softsign)
    -> base::expected<OperatorOffset, std::string> {
  // TODO(crbug.com/339654398): Support 16-bit float with dequantize operator
  // https://www.tensorflow.org/mlir/tfl_ops#tfldequantize_tfldequantizeop.
  const mojom::Operand& input_operand = GetOperand(softsign.input_operand_id);
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);

  // Emulate the softsign operation whose calculation follows the expression
  // `x / (1 + |x|)`.
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const int32_t output_tensor_index_of_abs =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  const int32_t input_tensor_index =
      operand_to_index_map_.at(softsign.input_operand_id);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                                  input_tensor_index,
                                                  output_tensor_index_of_abs));

  // Add constant value `1` to the output tensor of element-wise abs operation.
  // TODO(crbug.com/339654398): Convert the 32-bit floating-point data to 16-bit
  // floating-point data with fp16_ieee_from_fp32_value function if some
  // delegates support 16-bit float inference.
  CHECK_EQ(input_operand.descriptor.data_type(), OperandDataType::kFloat32);
  const int32_t constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_add =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, constant_tensor_index,
      output_tensor_index_of_abs, output_tensor_index_of_add));

  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, input_tensor_index,
      output_tensor_index_of_add,
      operand_to_index_map_.at(softsign.output_operand_id));
}

auto GraphBuilderTflite::SerializeSplit(const mojom::Split& split)
    -> base::expected<OperatorOffset, std::string> {
  // Serialize the axis tensor to split input tensor along it.
  const auto checked_axis = base::MakeCheckedNum<int32_t>(split.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis is too large.");
  }
  const int32_t axis_tensor_index = SerializeTensorWithBuffer<int32_t>(
      /*buffer=*/std::array<int32_t, 1>{checked_axis.ValueOrDie()},
      /*dimensions=*/{});

  // Serialize the split sizes tensor that specifies the sizes of each output
  // tensor along the axis.
  const size_t outputs_size = split.output_operand_ids.size();
  std::vector<int32_t> split_sizes;
  split_sizes.reserve(outputs_size);
  std::vector<int32_t> op_outputs;
  op_outputs.reserve(outputs_size);
  for (uint64_t output_id : split.output_operand_ids) {
    // The output shape has been validated to not overflow before creating
    // tensor.
    const std::vector<uint32_t>& output_shape =
        GetOperand(output_id).descriptor.shape();
    CHECK_LT(split.axis, output_shape.size());
    split_sizes.push_back(output_shape[split.axis]);

    op_outputs.push_back(operand_to_index_map_.at(output_id));
  }
  const auto checked_split_size =
      base::MakeCheckedNum<int32_t>(split_sizes.size());
  if (!checked_split_size.IsValid()) {
    return base::unexpected("The split size is too large.");
  }
  const std::array<int32_t, 1> split_sizes_shape = {
      checked_split_size.ValueOrDie()};
  const int32_t sizes_tensor_index =
      SerializeTensorWithBuffer<int32_t>(split_sizes, split_sizes_shape);

  // Create `tflite::SplitOptions` with the split size.
  const auto split_options = ::tflite::CreateSplitOptions(
      builder_, /*num_splits=*/checked_split_size.ValueOrDie());

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SPLIT_V);
  // The order of inputs is input, split sizes tensor and then axis tensor as
  // the described https://www.tensorflow.org/mlir/tfl_ops#operands_130.
  const std::array<int32_t, 3> op_inputs = {
      operand_to_index_map_.at(split.input_operand_id), sizes_tensor_index,
      axis_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_SplitVOptions, split_options.Union());
}

auto GraphBuilderTflite::SerializeTan(const mojom::ElementWiseUnary& tan)
    -> OperatorOffset {
  // The tangent operation defines the expression `opposite side / adjacent
  // side` to a right triangle as the described here
  // https://www.mathworks.com/help/matlab/ref/tan.html, it can be emulated with
  // `sin(x)/cos(x)` element-wise.
  const mojom::Operand& input_operand = GetOperand(tan.input_operand_id);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      OperandDataTypeToTFLite(input_operand.descriptor.data_type());
  const int32_t output_tensor_index_of_sin =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  const int32_t input_tensor_index =
      operand_to_index_map_.at(tan.input_operand_id);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_SIN,
                                                  input_tensor_index,
                                                  output_tensor_index_of_sin));

  const int32_t output_tensor_index_of_cos =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_COS,
                                                  input_tensor_index,
                                                  output_tensor_index_of_cos));
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, output_tensor_index_of_sin,
      output_tensor_index_of_cos,
      operand_to_index_map_.at(tan.output_operand_id));
}

auto GraphBuilderTflite::SerializeTanh(const mojom::Tanh& tanh)
    -> OperatorOffset {
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_TANH,
      operand_to_index_map_.at(tanh.input_operand_id),
      operand_to_index_map_.at(tanh.output_operand_id));
}

auto GraphBuilderTflite::SerializeTriangular(
    const mojom::Triangular& triangular)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand = GetOperand(triangular.input_operand_id);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.descriptor.shape());
  CHECK(signed_input_dimensions.has_value());
  auto input_rank = signed_input_dimensions->size();
  CHECK_GE(input_rank, 2u);
  const int32_t height = (*signed_input_dimensions)[input_rank - 2];
  const int32_t width = (*signed_input_dimensions)[input_rank - 1];
  const std::array<int32_t, 2> mask_dimensions = {height, width};
  auto checked_diagonal = base::MakeCheckedNum<int32_t>(triangular.diagonal) +
                          std::max(height, width);
  if (!checked_diagonal.IsValid()) {
    return base::unexpected("The diagonal is too large.");
  }

  // Mask the input with an element-wise multiplication. For example:
  // [ 2, 3                           [0, 0,           [0, 0,
  //   4, 5,   element-wise mul        1, 0,      =>    4, 0,
  //   6, 7]                           1, 1]            6, 7]
  //
  // TODO(crbug.com/359729258): Save GPU memory consumption.
  int32_t mask_tensor_index;
  OperandDataType data_type = input_operand.descriptor.data_type();
  switch (data_type) {
    case OperandDataType::kFloat32:
      mask_tensor_index = SerializeTensorWithBuffer<float>(
          /*buffer=*/FillMaskTriangular<float>(
              mask_dimensions, triangular.upper, triangular.diagonal, 1.0),
          /*dimensions=*/mask_dimensions);
      break;
    case OperandDataType::kInt32:
      mask_tensor_index = SerializeTensorWithBuffer<int32_t>(
          /*buffer=*/FillMaskTriangular<int32_t>(
              mask_dimensions, triangular.upper, triangular.diagonal, 1),
          /*dimensions=*/mask_dimensions);
      break;
    case OperandDataType::kUint32:
      mask_tensor_index = SerializeTensorWithBuffer<uint32_t>(
          /*buffer=*/FillMaskTriangular<uint32_t>(
              mask_dimensions, triangular.upper, triangular.diagonal, 1u),
          /*dimensions=*/mask_dimensions);
      break;
    case OperandDataType::kInt64:
      mask_tensor_index = SerializeTensorWithBuffer<int64_t>(
          /*buffer=*/FillMaskTriangular<int64_t>(
              mask_dimensions, triangular.upper, triangular.diagonal, 1),
          /*dimensions=*/mask_dimensions);
      break;
    case OperandDataType::kInt8:
    case OperandDataType::kUint8:
    case OperandDataType::kFloat16:
    case OperandDataType::kUint64:
      // TODO(crbug.com/359758892): Add the unsupported data types in
      // opSupportLimits.
      return base::unexpected(base::StrCat(
          {DataTypeToString(data_type), " is not supported for triangular."}));
  }

  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL,
      operand_to_index_map_.at(triangular.input_operand_id), mask_tensor_index,
      operand_to_index_map_.at(triangular.output_operand_id));
}

auto GraphBuilderTflite::SerializeTranspose(const mojom::Transpose& transpose)
    -> OperatorOffset {
  return SerializeTransposeOperation(
      operand_to_index_map_.at(transpose.input_operand_id),
      operand_to_index_map_.at(transpose.output_operand_id),
      transpose.permutation);
}

auto GraphBuilderTflite::SerializeWhere(const mojom::Where& where)
    -> OperatorOffset {
  // The data type of WebNN condition operand is uint8, but TFLite requires the
  // condition operand to be of type bool, so a cast operation need to be
  // inserted before the operation to convert uint8 to bool for the condition
  // operand.
  const mojom::Operand& condition_operand =
      GetOperand(where.condition_operand_id);
  // The shape of condition operand has been validated to not overflow before
  // creating tensor.
  const auto signed_condition_dimensions =
      ToSignedDimensions(condition_operand.descriptor.shape());
  CHECK(signed_condition_dimensions.has_value());
  const int32_t condition_bool_tensor_index = SerializeTemporaryTensor(
      *signed_condition_dimensions, ::tflite::TensorType_BOOL);

  CHECK_EQ(condition_operand.descriptor.data_type(), OperandDataType::kUint8);
  operators_.emplace_back(SerializeCastOperation(
      operand_to_index_map_.at(where.condition_operand_id),
      /*input_tensor_type=*/::tflite::TensorType_UINT8,
      condition_bool_tensor_index,
      /*output_tensor_type=*/::tflite::TensorType_BOOL));

  // TFLite SELECT_V2 builtin operator supports broadcastable shapes between
  // `condition`, `true` and `false` operand.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SELECT_V2);
  const std::array<int32_t, 3> op_inputs = {
      condition_bool_tensor_index,
      operand_to_index_map_.at(where.true_value_operand_id),
      operand_to_index_map_.at(where.false_value_operand_id)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(where.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

}  // namespace webnn::tflite
