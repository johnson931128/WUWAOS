# WUWAOS design and motion review (one-page practical checklist)

## A. Appearance (highest priority)

- [ ] Page has one obvious focal point; headings and whitespace establish reading order.
- [ ] Default look is light; surfaces and colors have deliberate roles, not random patches.
- [ ] Prose is 16–18px; necessary labels and actual rendered SVG text are >=14px wherever practical. Resize the diagram/layout instead of shrinking fonts.
- [ ] One coherent icon family: predictable size/stroke, meaningful metaphors, labels beside icons.
- [ ] Avoid repeated generic bordered cards, decorative accent stripes, big empty panels and loud glowing backgrounds.
- [ ] Content zones are easy to distinguish: diagram, current explanation, source references; annotations don't overlap edges.
- [ ] UI remains visually balanced when nothing is selected and when the longest description is selected.

## B. Interaction

- [ ] Selected and unselected states are instantly distinguishable (also without relying solely on color).
- [ ] Hover, keyboard focus and press states are purposeful, not three contradictory styles.
- [ ] Click/tap updates the right explanation and source link; no stale data, text jump or disorienting scroll.
- [ ] Feedback occurs promptly, selected nodes remain in place, and the diagram's arrows retain semantic meaning.
- [ ] Buttons have discoverable targets and clear labels; no inaccessible hover-only information.
- [ ] Educational animation only emphasizes the current step; no fake hardware traces or unrelated autoplay.

## C. Smoothness

- [ ] No layout shift when selecting another diagram component, loading icons, fonts or switching chapter.
- [ ] Prefer transform/opacity/color transitions; avoid repeatedly updating measured layout in a pointermove/scroll handler.
- [ ] Repeated rapid clicks leave the interface in the correct final state; any animation can be interrupted without visual glitches.
- [ ] Animations have an end state and `prefers-reduced-motion` fallback.
- [ ] Do not claim 60fps, good INP/CLS/LCP or deployed quality unless actually measured; for genuine lag use performance skill.

## D. Minimal test, no test theatre

1. Build with `npm run docs:build`.
2. If a browser is available: inspect one desktop and one narrow layout, hover/click/keyboard select, a source link, and reduced motion.
3. Capture actual screenshots if available and note the visible improvement or remaining roughness.
4. Report `PASS`, `FAIL`, or `NOT_RUN` **only** for steps actually exercised.
