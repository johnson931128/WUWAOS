# WUWAOS — curated real-world UI / motion references

**Purpose:** borrow one strong property from each mature system, not entire layouts, code, branding or color palettes. Verify examples in the browser when possible. WUWAOS is a light, readable technical textbook, not a SaaS dashboard.

| Reference | What to borrow | What NOT to borrow | WUWAOS use |
| --- | --- | --- | --- |
| [Vercel Geist](https://vercel.com/geist/introduction) | Clear design tokens, developer-friendly typography and grid, coherent icon stroke/scale | Full Vercel brand skin, React component implementation | Main design system, icons and spacing |
| [Stripe Docs](https://docs.stripe.com/) | Comfortable reading width, clear code/text hierarchy, strong layout around explanations | Stripe colors, complex API products | Chapter explanations, source links and code examples |
| [Linear Docs](https://linear.app/docs) | Restrained visual hierarchy and crisp controls, selected/focused states | Dense application dashboard or dark-only interface | Architecture selection and side inspector |
| [React Flow examples](https://reactflow.dev/examples) | Visually understandable nodes, connectors, grouped boundaries, interaction affordances | Entire React dependency or drag-and-drop as a goal | OS structure diagrams and real transitions |
| [IBM Carbon Motion](https://www.carbondesignsystem.com/building-blocks/foundations/motion/overview/) | Distinguish productive vs expressive motion, consistent timing | Over-animation and decorative motion everywhere | Control feedback and the one highlighted path |
| [Motion for Vue](https://motion.dev/docs/vue) and [layout](https://motion.dev/docs/vue-layout-animations) | Interruptible animations, shared layout patterns when truly needed | Installing motion-v for static highlights; unsupported SVG layout motion | Selected item → explanation panel transitions |
| [VitePress theme extension](https://vitepress.dev/guide/extending-default-theme) | Custom CSS variables, theme components/slots without rewriting docs | Fragile internal theme overrides without need | Existing Vue/VitePress architecture |

## Choose by page, not by trend

- **WUWAOS homepage:** Vercel's grid and icon system + Linear's restrained control clarity + React Flow's relationship cues.
- **Technical chapters:** Stripe's readable explanatory rhythm + Geist typography and code structure.
- **Process State / Scheduler diagrams:** clear textbook layout + Carbon's restrained selected-edge motion. Keep nodes fixed and highlight only an actual transition.
- **Device or File System flow diagrams:** React Flow style for visual relationships + Motion's principles of spatial continuity; keep WUWAOS source mapping.
- **Hover/click feedback:** Linear-like selected-state clarity; use Motion Prompt specific effect IDs only where they improve a real state change.

For each redesign, record: **references chosen → chosen characteristic → rejected characteristic → resulting design decision**. Two or three references per page are enough.

## WUWAOS identity

A well-crafted system architecture atlas with gentle full-surface role colors, 16–18px prose, >=14px effective data labels, coherent meaningful SVG icons, neutral fine boundaries, and one clear selected object. Visually attractive **before** adding animation. No one-sided colored stripes, generic gradient clouds or dense card farms.
