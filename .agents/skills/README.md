# Codex repository skills for WUWAOS

Codex discovers repository-local skills under **`.agents/skills/<skill>/SKILL.md`**. This is the supported project location (not `.codex/skills`). These skills are instructions, **not installed executables**, and do not run until Codex selects them or a developer invokes them.

## Skill catalog

| Skill | Purpose | Invoke |
| --- | --- | --- |
| `frontend-design` | Strong visual identity, layout, type, hierarchy, purposeful motion | `$frontend-design` |
| `web-design-guidelines` | Audit site UX, layout, interaction, accessibility | `$web-design-guidelines` |
| `accessibility` | WCAG-oriented keyboard, screen-reader, focus, motion design checks | `$accessibility` |
| `wuwaos-interactive-labs` | State-driven, source-grounded simulations of the four OS teaching labs | `$wuwaos-interactive-labs` |

For WUWAOS UI redesigns, start with `$wuwaos-interactive-labs`, use `$frontend-design` for visual direction, and review with `$web-design-guidelines` and `$accessibility`.

### Provenance (retrieved 2026-11-10)

- `frontend-design/SKILL.md` and its `LICENSE.txt`: [Anthropic skills](https://github.com/anthropics/skills/tree/2c5958ecac7cee2b583967a2c4981d0f4a75c579/skills/frontend-design), Apache 2.0, copied unmodified.
- `web-design-guidelines/SKILL.md`: [Vercel Labs agent-skills](https://github.com/vercel-labs/agent-skills/tree/1a2c79d4e6c4a2283dc2c614b6538cbe1ed5ca62/skills/web-design-guidelines), copied unmodified; this source references a [separately hosted current guidelines document](https://github.com/vercel-labs/web-interface-guidelines/blob/main/command.md). It requires network access for live refresh.
- `accessibility/SKILL.md`, `references/` and `LICENSE`: [Addy Osmani web-quality-skills](https://github.com/addyosmani/web-quality-skills/tree/afa8da942115f2961fdbfa80807ea0b232ff6c00/skills/accessibility), MIT. The only local adaptation is replacing a link to a not-vendored sibling audit skill with its upstream URL.
- `wuwaos-interactive-labs/`: locally authored WUWAOS-specific guidance; grounded in current repository docs and source paths. No copied external instructional text.

Upstream instructions are included for inspection and use; source authors' styles and recommendations are not guarantees of correctness. The Vercel audit fetches fresh guidelines at run time; if unavailable, report the audit as incomplete. No third-party install scripts were executed.

## Scope and verification

All listed skills are for the **website under `docs/`**. They do not grant permission to modify OS kernel/device implementation, and do not constitute runtime tests. For the four labs, validate with real browser actions when available, and report unavailable checks as `NOT_RUN`.

For changes to these skills, review source and attribution before replacing vendored text. Restart Codex if the new skills do not show in the picker.
