# Percetto

Percetto is a minimal C wrapper for Perfetto SDK to enable app-specific
tracing.  Internally, there is a minimal implementation of `TrackEvent` data
source.

## Current Limitations

Currently, Percetto works best when statically linked once for a whole process.
When linked into a shared library, Percetto symbols should not be exported,
because percetto_init can not yet be called multiple times with different
category data.

If there are multiple instances of Percetto in a single process, the Perfetto
UI will show those as separate processes with the same PID -- they are not
merged, so even a call on the same thread will appear to be a separate track
with the same thread ID.

## Directory Structure

`perfetto-sdk` contains rules to locate and build Perfetto SDK.

`src` contains the source code of Percetto.

`examples` contains examples to demonstrate different ways of using Percetto.

## Select Examples

### [multi-category.c](examples/multi-category.c)

Shows various types of trace events and the use of multiple categories.

### [atrace.c](examples/atrace.c)

Shows how to use the Android ATRACE macro compatibility API found in
[atrace-compat.h](src/atrace-compat.h).

### [timestamps.c](examples/timestamps.c)

Shows how to manually set timestamps for events. This enables use cases where
the events are added to Percetto after they have already occurred, such as
GPU tracing.
