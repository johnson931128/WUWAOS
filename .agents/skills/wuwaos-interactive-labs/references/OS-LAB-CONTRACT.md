# WUWAOS Interactive Labs: Source-Mapped Contract

This reference helps design teaching models. All claims must be cross-checked against current `main` source before implementation. Simulators are **not** live QEMU traces.

## 1. Block Async I/O

Read `drivers/virtio/block.c`, `kernel/proc/process.c`, and `docs/topics/block-async.md`.

- Visual lanes: process caller, scheduler/poller, VirtIO block device. Show BlockDevice gate, persistent request metadata, descriptor/avail/used rings, and the 512-byte bounce buffer.
- Normal path: prepare and submit -> caller waits without discarding its C stack -> device updates used ring -> scheduler's `block_poll` detects completion and notifies event -> caller resumes and collects -> ownership released.
- A completed device operation does **not** directly wake the caller via a VirtIO interrupt in this model. Separate device completion, poll, and wakeup visually.
- Read data is copied from bounce to caller destination **only on success**; writes copy caller source to bounce **before** submit.
- Scenarios: read/write, reported IOERR, timeout with confirmed reset and offline transition, unconfirmed reset/fail-stop, and cancellation deferred while the device might own DMA memory.
- Disable actions that violate gate ownership, request lifetime, or offline status.

## 2. FD/OFD

Read `kernel/io/io.c`, `docs/topics/file-descriptors.md`, and corresponding user API.

- Show process-local FD tables, independently allocated global OFDs, per-OFD offset, file bytes, and incoming arrows for every reference.
- `open` creates an OFD (when applicable); `dup` points another FD to the **same** OFD; `fork` copies live FD references and increments refs; a separate `open` yields an independent offset.
- An ordinary read moves the referenced OFD's offset by bytes actually read. `close` drops a reference and may release the OFD at zero.
- Preserve WUWAOS limits and special UART/TTY endpoint handling. Do not invent POSIX PTYs or claim all console descriptors allocate regular-file OFDs.
- Suggested first interaction: `open` -> `read` -> `dup` -> `read` via second FD -> `fork` -> `close`. Each action updates the live graph.

## 3. Paging and Swap

Read `kernel/mm/vm.c`, `kernel/mm/swap.c`, `docs/topics/memory.md`, and `docs/topics/paging.md`.

- WUWAOS uses RV64 Sv39, three levels of 9-bit VPN indexes, 12-bit page offset, and 4 KiB leaf pages; distinguish teaching addresses from actual runtime addresses.
- Make page accesses explicit. Show virtual pages, PTE flags, physical frames, FIFO arrival order, pin counts, dirty state, and swap slots.
- FIFO chooses the oldest eligible resident page; pinned pages are skipped. Do not silently substitute LRU.
- For a dirty eviction, preserve the PTE, revoke the mapping and fence, pin the victim, issue swap I/O, then either commit swap metadata/free the frame or restore original RAM/PTE and dirty state on failure.
- A 4-KiB page uses 8 sector writes of 512 bytes each in the teaching scenario. A failed write may stop before the remaining sectors.
- Explicitly distinguish page fault, eviction attempt, successful swap-out, retry, and rollback.

## 4. WUFS Journal and atomic replacement

Read `kernel/fs/fs.c`, `docs/topics/journal.md`, and `docs/SPEC-32-wufs-redo-journal.md`.

- Show RAM staging, target sector table, redo payload, commit header, and home sectors as distinct storage locations.
- Ordered transaction: stage -> write target table/payload -> FLUSH -> commit header attempt -> FLUSH -> home writes -> FLUSH -> clear header -> FLUSH.
- A failed pre-header write may abort with old home sectors intact; after header write is attempted, errors may return `ERR_UNCERTAIN` and force FS offline. Distinguish header attempt from verified durable commit.
- On reboot, replay requires a complete valid committed journal, including header and payload validation. A torn or invalid header must not be shown as an automatically recoverable transaction.
- `fs_replace` stages new inode/data, updates the directory entry and frees old data in one bounded transaction; do not claim multi-call shell redirection is atomic.
- Make failure-injection selection meaningful: pick phase/sector and show resulting disk state, caller result, and whether replay is permitted. Prefer illustrative deterministic fault states over false random hardware behavior.

## Shared acceptance checklist

- Initial user goal and visible operating instructions are clear without reading all explanatory prose.
- Each click/keyboard action changes **model state**, not merely a paragraph title.
- Same source-of-truth drives diagram, labels, counters, history, and explanation.
- All restrictions (capacity, permissions, ownership, failed operations) produce explicit feedback.
- Keyboard, focus-visible, zoom, mobile, dark/light, reduced-motion and screen reader narration work.
- Code references lead to current paths; verified browser behavior is separated from build-only checks.
