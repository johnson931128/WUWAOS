---
name: wuwaos-interactive-labs
description: Design or rebuild state-driven Vue/VitePress interactive learning labs for WUWAOS, including Block Async I/O, FD/OFD, Paging/Swap, and WUFS Journal. Use for UX redesign, simulator logic, diagrams, user interactions, or lab verification; not for kernel features.
---

# WUWAOS interactive lab design and implementation

## Scope and evidence

- Scope changes to `docs/` unless the task explicitly requires OS source changes. Do not silently alter `kernel/`, `arch/`, `drivers/`, or `user/`.
- Before implementation, read the relevant `docs/topics/*.md`, `docs/CODE_GUIDE.md`, and real OS code; consult `plan_current.md` for verified versus NOT_RUN boundaries.
- Use `references/OS-LAB-CONTRACT.md` as the checklist for the four labs. This is a design contract, not proof of kernel behavior; resolve discrepancies in favor of the current source, and document them.
- Distinguish simulated teaching states, source-derived behavior, and actual QEMU trace. Never portray a model as live telemetry.

## Interaction model before visuals

1. Define learning outcome and one realistic user task, not a slideshow objective.
2. Specify model types, input actions, guards, pure transitions, invariants, outputs, and error conditions.
3. Prefer state-driven interactions: a user action changes model data; the diagram, tables, explanation, and action history derive from the same state.
4. Preserve Back/Next playback only for genuinely chronological events (hardware completion, poll, wakeup, journal ordering). If playback is used, all panels derive from one frame state; avoid disconnected descriptions.
5. Keep deterministic scenarios and a resettable seed. Clearly mark simplified assumptions and bounds. Avoid fictional general-purpose POSIX/Linux features.
6. Provide explanatory feedback after every action: which structure changed, why, and which WUWAOS function implements the behavior.

## Visual hierarchy

- One primary **system diagram** per lab: actual objects, ownership, arrows, and data destinations; organize by caller/kernel/device or process/OFD/file where appropriate.
- Animate only transitions with meaning (buffer copy, reference changes, page movement, commit boundaries). Include instant/static alternatives for reduced motion.
- Use spatial continuity: keep objects in predictable positions across steps and highlight only changed objects. Do not make text-heavy cards the main visual.
- A compact inspector may show current function, associated source path, key fields, and before/after differences; keep exhaustive explanation in the existing chapter text.
- Follow `frontend-design` for deliberate visual composition and `web-design-guidelines` for UX review; use `accessibility` for keyboard, focus, contrast, and motion checks.

## WUWAOS implementation pointers

- Lab components: `docs/.vitepress/theme/components/{BlockAsyncLab,FdOfdLab,PagingLab,JournalLab}.vue`.
- Chapters: `docs/topics/{block-async,file-descriptors,paging,journal}.md`.
- WUWAOS core sources: `drivers/virtio/block.c`, `kernel/io/io.c`, `kernel/mm/vm.c`, `kernel/mm/swap.c`, `kernel/fs/fs.c`.
- Avoid claiming that `npm run docs:build` or a VitePress build proves the simulator's behavior.

## Verification

- Inspect the actual package scripts before running a command. Build docs, then run browser interactions if browser capability is available.
- Verify every enabled action and guard, reset, stepping, failure path, source links, keyboard controls, focus, mobile widths, light/dark themes, and reduced-motion.
- Take screenshots to compare actual rendered states if a browser is available; if no browser is available, report visual/runtime verification as NOT_RUN rather than inventing a result.
- Report changed files, scenarios actually exercised, known limitations, and invariants not yet tested.
