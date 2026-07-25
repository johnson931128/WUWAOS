# Domain Docs

WUWAOS uses a single-context domain documentation layout:

- `CONTEXT.md` at the repo root for shared project language.
- `docs/adr/` for architectural decision records when a decision is hard to reverse, surprising without context, and the result of a real trade-off.

## Before exploring

Read `CONTEXT.md`, then read any relevant ADRs in `docs/adr/`.

If an ADR does not exist for the current topic, proceed with the existing architecture documents instead of inventing one.

## WUWAOS-specific rules

- Keep WUWAOS described as a terminal-first, Linux-like OS simulator.
- Do not describe WUWAOS as a bootable OS.
- Keep shell commands separate from host machine commands.
- Keep the simulated kernel separate from the host operating system.
- Preserve the small-step development rule from `AGENTS.md`.
- Keep `docs/ARCHITECTURE.md` and `docs/PROGRESS.md` as project documentation, not replacements for `CONTEXT.md`.
