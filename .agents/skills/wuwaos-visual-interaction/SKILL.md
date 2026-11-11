---
name: wuwaos-visual-interaction
description: Improve WUWAOS visual quality and interaction smoothness together. Use for website redesign, motion polish, typography, icon systems, explanation panels, microinteractions, responsive behavior, and accessible UI reviews in VitePress/Vue.
---

# WUWAOS — Visual quality and interaction craft

**Order of priorities: (1) visual quality and readability, (2) interaction clarity, (3) smoothness, (4) embellishment.** A beautiful static page that reads well beats a messy animated dashboard. Follow root `AGENTS.md` and limit implementation to the requested page.

## Use Skills, don't just mention them

- **Always first read** `.agents/skills/frontend-design/SKILL.md` for distinctive composition, typography, layout, and palette.
- **Then read** this skill's `references/design-review.md`, `references/site-patterns.md`, and the relevant section of `references/ui-ux-pro-max-quick-reference.md` for visual quality and microinteraction design.
- Read `.agents/skills/web-design-guidelines/SKILL.md` to review navigation and web conventions.
- Read `.agents/skills/accessibility/SKILL.md` for keyboard, focus, understandable labels, and reduced motion.
- Use `.agents/skills/motion-prompt/SKILL.md` if installed and only if a motion pattern clarifies behavior; use the publisher's exact spec instead of inventing timing.
- Use `.agents/skills/performance/SKILL.md` for profiling when the user says the page is laggy, sticky, jumps, or slow. Never treat a build as evidence of browser smoothness.
- For OS-specific stateful teaching components read `.agents/skills/wuwaos-interactive-labs/SKILL.md`. The actual source is authoritative. Do not transform simple educational diagrams into multi-form simulators without a request.

## 1. Appearance pass (required, before animations)

1. Pick **two or three visual references** from `references/site-patterns.md`. Extract a specific quality from each (e.g. Vercel Geist for grid/typography, Stripe for reading rhythm, Linear for restrained control density); specify why others are not used. No full-site cloning.
2. Draft one cohesive visual direction: light-background palette, 16–18px body, >=14px effective labels incl. rendered SVG text, primary and neutral surfaces, type scale, icon stroke style, section rhythm, meaningful spacing, restrained elevation.
3. Create one visual hero, use diagrams and purposeful SVG icons as protagonists. Do not fill unused space with repetitive cards. Preserve strong hierarchy at a glance: what the system is → what is selected → why it matters → where to read more.
4. Express roles through soft **full-surface** tints, clear labels and iconography; prohibit ornamental left/top colored stripes, pseudo-element stripes and garish gradients.
5. Examine representative screenshots at actual viewport sizes. If text is small or cropped, **change the layout** before changing the font size. If a diagram is busy, remove redundant decoration before adding more labels.

## 2. Interaction pass (make every action feel good)

For each clickable element specify **trigger → immediate feedback → content transition → end/focus state**.

- Hover/tap/keyboard must reveal an actionable affordance with clear active/selected/disabled state. Maintain focus when swapping explanations; avoid jumping the viewport or losing the selected item.
- Keep selected diagram objects fixed spatially. Update one compact explanation panel with matching label, icon, source path and current key detail. Use modest color transition, not huge layout movement.
- Main SVG arrows are for **real architecture relations**, not decoration. Animated paths illustrate only current data/state change; others remain visually quiet.
- Use perceptually short and consistent durations (baseline 120–180ms input feedback; 180–300ms content transition; 300–500ms simple educational path movement). These are starting guidance, not hard limits: honor selected Motion Prompt specs when porting and verify actual feel.
- Prefer `opacity`, `transform`, and SVG stroke attributes to frequent size/layout reflow. No endless idle animations, scroll hijacking, obligatory preloaders or strobing.
- Native Vue transitions/CSS are preferred for simple interaction. Consider `motion-v` only **after** a real need for interruptible shared-layout, gestures or choreography is demonstrated; no mandatory dependency addition. Review Motion's SVG limitation: SVG layout animation is not supported; animate vector attributes instead.
- Respect `prefers-reduced-motion` and disable or shorten movement while preserving understandable state cues.

## 3. Review

- Read `references/design-review.md` and inspect the actual page rather than relying on CSS inspection alone.
- After each requested page, validate content hierarchy, visual rhythm, color contrast, SVG icons, pointer/keyboard/touch states and a few real interaction sequences. Compare screenshots before/after where possible.
- Check `npm run docs:build`. If browser or field performance data is unavailable, state `NOT_RUN` and do not claim a measured frame rate, LCP, INP, or CLS.
- Leave `main` as the sole development branch; no self-created PRs/branches/worktrees. Do not change Kernel, drivers or user programs for UI improvements.

## References

- `references/site-patterns.md`: curated reference sites, what to borrow, what to avoid
- `references/design-review.md`: concise visual + interaction + smoothness acceptance rubric
- `references/ui-ux-pro-max-quick-reference.md`: credited upstream UI/UX Pro Max rule reference (static excerpt; not its executable search skill)
- `references/UI-UX-PRO-MAX-LICENSE`: original license
