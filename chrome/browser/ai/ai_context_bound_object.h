// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_
#define CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/supports_user_data.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

// BaseClass for storing an object that shall be deleted when the
// document is gone such as AITextSession and AISummarizer.
class AIContextBoundObject {
 public:
  // The deletion_callback will remove the object from the
  // AIContextBoundObjectSet. The handler shall be called when the object is no
  // longer required for the document and should be deleted.
  virtual void SetDeletionCallback(base::OnceClosure deletion_callback) = 0;

  virtual ~AIContextBoundObject() = default;
};

#endif  // CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_
