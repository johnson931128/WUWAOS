# WUWAOS docs design guidance for Codex

When modifying homepage visual design or interactive labs, read `.agents/skills/motion-prompt/SKILL.md` and `.agents/skills/frontend-design/SKILL.md`. Use motion-prompt IDs that match the interaction; read the original element spec rather than guessing the effect.

For stateful OS labs also read `.agents/skills/wuwaos-interactive-labs/SKILL.md`. Effects must communicate real transitions, not simulate nonexistent kernel behavior. Preserve source mapping and distinguish teaching visuals from QEMU runtime traces.

Changes to `docs/` must maintain accessible focus, legible light/dark palettes, keyboard navigation, reduced-motion fallback, responsive layouts and working VitePress links. Browser and build checks must be actually executed before marking verified. Do not change OS `arch/`, `kernel/`, `drivers/`, or `user/` during design tasks.
