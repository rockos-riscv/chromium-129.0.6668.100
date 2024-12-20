// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_impl_tflite.h"

#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"
#include "services/webnn/tflite/buffer_impl_tflite.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/tflite/graph_impl_tflite.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::tflite {

ContextImplTflite::ContextImplTflite(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GraphBuilderTflite::GetContextProperties(),
                       std::move(options)) {}

ContextImplTflite::~ContextImplTflite() = default;

base::WeakPtr<WebNNContextImpl> ContextImplTflite::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplTflite::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    CreateGraphImplCallback callback) {
  std::move(callback).Run(GraphImplTflite::CreateAndBuild(
      std::move(graph_info), std::move(compute_resource_info), this));
}

void ContextImplTflite::CreateBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    CreateBufferImplCallback callback) {
  std::move(callback).Run(BufferImplTflite::Create(std::move(receiver), this,
                                                   std::move(buffer_info)));
}

}  // namespace webnn::tflite
