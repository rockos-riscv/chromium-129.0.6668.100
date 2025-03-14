---
breadcrumbs:
- - /developers
  - For Developers
- - /developers/design-documents
  - Design Documents
- - /developers/design-documents/sync
  - Sync
page_name: client-tag-based-data-type-processor
title: ClientTagBasedDataTypeProcessor
---

The [`ClientTagBasedDataTypeProcessor`][ClientTagBasedDataTypeProcessor] is a
crucial piece of the USS codepath. It lives on the model thread and performs the
tracking of sync metadata for the [`DataTypeSyncBridge`][DataTypeSyncBridge]
that owns it by implementing the
[`DataTypeLocalChangeProcessor`][DataTypeLocalChangeProcessor] interface, as
well as sending commit requests to the [`DataTypeWorker`][DataTypeWorker] on the
sync thread via the [`CommitQueue`][CommitQueue] interface and receiving updates
from the same worker via the [`DataTypeProcessor`][DataTypeProcessor] interface.

This processor supports types that use a client tag, which is currently
includes all except bookmarks. This means all changes in flight (either incoming
remote changes provided via the [`DataTypeWorker`][DataTypeWorker], or local
changes reported by the [`DataTypeSyncBridge`][DataTypeSyncBridge]) must specify
a client tag, which is considered (after being hashed) the main global
identifier of a sync entity.

[ClientTagBasedDataTypeProcessor]: https://cs.chromium.org/chromium/src/components/sync/model/client_tag_based_data_type_processor.h
[DataTypeSyncBridge]: https://cs.chromium.org/chromium/src/components/sync/model/data_type_sync_bridge.h
[DataTypeLocalChangeProcessor]: https://cs.chromium.org/chromium/src/components/sync/model/data_type_local_change_processor.h
[DataTypeWorker]: https://cs.chromium.org/chromium/src/components/sync/engine/data_type_worker.h
[CommitQueue]: https://cs.chromium.org/chromium/src/components/sync/engine/commit_queue.h
[DataTypeProcessor]: https://cs.chromium.org/chromium/src/components/sync/engine/data_type_processor.h

[TOC]

## Lifetime

The bridge owns a processor object at all times and operates on the same thread
as it. If sync is disabled, the processor is destroyed but a new one is
immediately created to replace it.

## Processor State Machines

The processor sits between the model bridge and the sync engine. It has
knowledge of what state each is in based on the calls it has received and
performed. The states are not stored explicitly, but are implicit based on
state stored in the processor. Here are the states of each, with notes on their
transitions and how to determine them.

### Model States

*   `UNREADY`
    *   Waiting for `ModelReadyToStart` to be called.
    *   Determined by: `waiting_for_metadata_ && !model_error_`
*   `NEEDS_DATA`
    *   Waiting for data for pending commits to be loaded.
    *   This state is skipped if there are no pending commits.
    *   Determined by: `waiting_for_pending_data_ && !model_error_`
*   `READY`
    *   The model is completely ready to sync.
    *   Determined by: `!waiting_for_metadata_ && !waiting_for_pending_data &&
        !model_error`
*   `ERROR`
    *   Something in the model or storage broke.
    *   This state is permanent until DisableSync destroys the object.
    *   Determined by: `!!model_error_`

### Sync States

*   `DISCONNECTED`
    *   Sync for this type has not started.
    *   This state can be re-entered from any other state if Disconnect is
        called.
    *   Determined by: `!error_handler_`.
*   `STARTED`
    *   Sync has started but the model is not yet `READY` (or `ERROR`).
    *   This state is skipped if the model is ready before sync is.
    *   Determined by: `error_handler_ && start_callback_`
*   `CONNECT_PENDING`
    *   Both the model and sync are ready. The start callback has been called
        and we're waiting to connect to the sync thread.
    *   If the model was `ERROR`, the error is passed along and the callback is
        cleared; we're really waiting for DisableSync instead of connect.
    *   Determined by: `error_handler_ && !start_callback_`
*   `CONNECTED`
    *   We have a [`CommitQueue`][CommitQueue] that passes changes to the
        [`DataTypeWorker`][DataTypeWorker] on the sync thread.
    *   Determined by: `!!worker_`

### Processor States

Based on the interplay of the model and sync states, the processor effectively
progresses through 3 states worth noting:

*   `UNINITIALIZED`
    *    Metadata isn't loaded so we have no knowledge of entities.
    *   `Put` and `Delete` calls are not allowed in this state (will DCHECK).
*   `NOT_TRACKING`
    *   Indicates that not metadata is being tracked and that `Put` and `Delete`
        calls will be ignored.
    *   This state is entered if the loaded metadata shows an initial merge
        hasn't happened (`DataTypeState::initial_sync_done` is false).
    *   Exposed via `IsTrackingMetadata` for optimization, not correctness.
*   `TRACKING`
    *   Indicates that metadata is being tracked and `Put` and `Delete` calls
        must happen for entity changes.
    *   This state is entered if the loaded metadata shows an initial merge
        has happened (`DataTypeState::initial_sync_done` is true).
*   `SYNCING`
    *   Indicates that commits can be sent and updates can be received from the
        sync server. This is a superstate of `TRACKING`.
    *   If the processor was in `TRACKING`, it progresses to this state as soon
        as it gets connected to the worker.
    *   If the processor was in `NOT_TRACKING`, it progresses to this state
        after `MergeSyncData` is called and the metadata is initialized.

## Entity Tracker

The [`ProcessorEntity`][ProcessorEntity] tracks the state of individual entities
for the processor. It keeps the [`EntityMetadata`][EntityMetadata] proto in
memory, as well as any pending commit data until it gets acked by the server. It
also stores the special `commit_requested_sequence_number_`, which tracks the
sequence number of the last version that's been sent to the server.

The tracker holds the metadata in memory forever, which is needed so we know
what to update the on-disk memory with when we get a new local or remote change.
Changing this would require being able to handle updates asynchronously.

[ProcessorEntity]: https://cs.chromium.org/chromium/src/components/sync/model/processor_entity.h
[EntityMetadata]: https://cs.chromium.org/chromium/src/components/sync/protocol/entity_metadata.proto
