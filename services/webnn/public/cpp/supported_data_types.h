// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_DATA_TYPES_H_
#define SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_DATA_TYPES_H_

#include "base/containers/enum_set.h"
#include "services/webnn/public/cpp/operand_descriptor.h"

namespace webnn {

using SupportedDataTypes = base::EnumSet<OperandDataType,
                                         OperandDataType::kMinValue,
                                         OperandDataType::kMaxValue>;

namespace DataTypeConstraint {

static constexpr SupportedDataTypes kUint8 = {OperandDataType::kUint8};

static constexpr SupportedDataTypes kInt32To64 = {OperandDataType::kInt32,
                                                  OperandDataType::kInt64};

static constexpr SupportedDataTypes kFloat16To32 = {OperandDataType::kFloat16,
                                                    OperandDataType::kFloat32};

static constexpr SupportedDataTypes kFloat16To32Int8To32 = {
    OperandDataType::kFloat16, OperandDataType::kFloat32,
    OperandDataType::kInt8, OperandDataType::kInt32};

static constexpr SupportedDataTypes kGatherIndicesSupportedDataTypes = {
    OperandDataType::kInt32, OperandDataType::kUint32, OperandDataType::kInt64};

}  // namespace DataTypeConstraint

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_DATA_TYPES_H_
