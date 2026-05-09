# NS-LIB-HID documentation

Per-file licensing for this library is summarized in [**../LICENSING.md**](../LICENSING.md).

Markdown guides for integrating **NS-LIB-HID** live in this folder.

| Document | Purpose |
|----------|---------|
| [Implementation guide](./implementation-guide.md) | End-to-end integration flow: init, host OUT enqueue, FIFO command processing, and periodic report generation |
| [HD rumble LUTs & index helpers](./hd-rumble-implementation-guide.md) | **`ns_lib_haptics_build_raw_tables`**, turning frequency/amplitude **indices** into **float Hz / gain**, and how decoded packets map to those tables |
