# WUWAOS homepage visual experiment — Motion Prompt

Status: **experimental branch / browser QA NOT_RUN**. This is not the published website and changes **no OS implementation**.

## Reason and design intent

The previous landing page repeats visually identical bordered cards. Move toward a source-driven OS lab / technical editorial site: one focal dark terminal, confident type hierarchy, structured chapter links, and a restrained palette. Keep all existing topics, source links, verification caveats and interactive lab behavior. The terminal demonstrates `echo hello` as a **teaching visual**, not a running emulator.

## Selected Motion Prompt effects

| Source effect | WUWAOS application | Demo |
| --- | --- | --- |
| #154 Spotlight hover | Cursor-following radial light within the dark terminal sample; static fallback on touch | https://showheyohtaki.github.io/motion-prompt/#154 |
| #081 Hover lift | Homepage lab links only, 10px upward over 0.3s | https://showheyohtaki.github.io/motion-prompt/#081 |
| #086 Underline animation | Chapter-title links, 0.3s underlining | https://showheyohtaki.github.io/motion-prompt/#086 |
| #079 Scroll progress bar | 3px, scroll-proportional reading progress with CSS scroll-timeline feature detection | https://showheyohtaki.github.io/motion-prompt/#079 |

Source: https://github.com/showheyohtaki/motion-prompt (MIT), pinned upstream revision `7819d3879eb7d5a1475c11c001bedffb5cab64af`.
Review specifications at `https://showheyohtaki.github.io/motion-prompt/api/v1/en/e/ID.txt` and read `.agents/skills/motion-prompt/references/porting.md`. Attribution markers are next to adapted CSS. The original gallery animations are not embedded in production.

## Scope of this iteration

- `docs/index.md`: new editorial hero, existing material left in place.
- `docs/.vitepress/theme/components/HomeTerminal.vue`: small replayable sample terminal with pointer spotlight and motion-reduced fallback; no network calls.
- `docs/.vitepress/theme/style.css`: responsive typography and content hierarchy, chapter grid redesign, lab link feedback, reading progress.
- `.agents/skills/motion-prompt/`: upstream Codex skill + local attribution.
- `docs/AGENTS.md`: site-only Codex guidance.

This is a deliberately scoped **homepage concept**. It does not claim that the four deep labs or entire documentation theme have been rebuilt.

## Review and acceptance before merging

1. Run `npm ci && npm run docs:build` in a connected environment.
2. Browse in desktop and mobile widths (375, 768, 1280px). Compare homepage with the current main.
3. Check light/dark mode, local search, sidebar, `SystemOverview`, all 13 chapter links and the four lab links.
4. Verify terminal replay, pointer spotlight, focus indicators, reduced-motion behavior and scroll reading progress.
5. Confirm no horizontal overflow, acceptable contrast, and no animation on non-actionable text.
6. Capture browser screenshots. Report actual findings and fix visual issues before merging.
7. Do not run or claim QEMU/OS tests; this is a documentation-only experiment.

Use `$motion-prompt`, `$frontend-design`, `$web-design-guidelines` and `$accessibility` in Codex for subsequent refinements. Keep the OS source separate from site work.
