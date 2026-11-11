import { onBeforeUnmount, onMounted, ref, type Ref } from 'vue'

// Motion Prompt #074 Fade-in on scroll — trigger line at 85% viewport, 0.7s, 28px rise.
// Motion Prompt #145 Timeline on scroll — rail grows with scroll; dots light as the line reaches them.
// The numbers are carried into CSS (duration/easing) and here (trigger line, per-item lighting,
// rail progress). This is a presentation reveal for the reading path, not OS state.
const TRIGGER = 0.85

export interface ScrollFxOptions {
  /** Reveal selector. Elements start hidden via [data-reveal]. */
  revealSelector?: string
  /** Timeline rail element (holds --wuwa-progress). */
  rail?: Ref<HTMLElement | null> | null
  /** Dots/rows that light up as the rail tip passes them. */
  railItemsSelector?: string
}

export function useScrollFx(options: ScrollFxOptions = {}) {
  const {
    revealSelector = '[data-reveal]',
    rail = null,
    railItemsSelector = '.wuwa-rm-item',
  } = options

  const reduced = ref(false)
  let observer: IntersectionObserver | null = null
  let scrollTicking = false
  let cleanup: (() => void) | null = null

  function applyReveals(root: ParentNode) {
    root.querySelectorAll<HTMLElement>(revealSelector).forEach((el) => el.classList.add('is-revealed'))
  }

  function lightAll(root: ParentNode) {
    root.querySelectorAll<HTMLElement>(railItemsSelector).forEach((el) => el.classList.add('is-lit'))
    if (rail?.value) rail.value.style.setProperty('--wuwa-progress', '100%')
  }

  function onScroll() {
    if (scrollTicking) return
    scrollTicking = true
    requestAnimationFrame(() => {
      scrollTicking = false
      const triggerY = window.innerHeight * TRIGGER
      const items = Array.from(document.querySelectorAll<HTMLElement>(railItemsSelector))
      for (const item of items) {
        const top = item.getBoundingClientRect().top
        item.classList.toggle('is-lit', top < triggerY)
      }
      const railEl = rail?.value
      if (railEl) {
        const railTop = railEl.getBoundingClientRect().top + window.scrollY
        const railH = railEl.getBoundingClientRect().height
        const progress = railH > 0
          ? Math.max(0, Math.min(1, (triggerY - railTop) / railH))
          : 1
        railEl.style.setProperty('--wuwa-progress', `${(progress * 100).toFixed(2)}%`)
      }
    })
  }

  onMounted(() => {
    reduced.value = window.matchMedia('(prefers-reduced-motion: reduce)').matches
    if (reduced.value) {
      applyReveals(document)
      lightAll(document)
      return
    }

    // Reveal on view.
    observer = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          if (entry.isIntersecting) {
            ;(entry.target as HTMLElement).classList.add('is-revealed')
            observer?.unobserve(entry.target)
          }
        }
      },
      { rootMargin: `0px 0px ${-window.innerHeight * (1 - TRIGGER)}px 0px`, threshold: 0 },
    )
    document.querySelectorAll<HTMLElement>(revealSelector).forEach((el) => observer?.observe(el))

    // Timeline lighting + rail fill on scroll.
    const onScrollListener = onScroll
    window.addEventListener('scroll', onScrollListener, { passive: true })
    onScroll()
    cleanup = () => {
      window.removeEventListener('scroll', onScrollListener)
    }
  })

  onBeforeUnmount(() => {
    observer?.disconnect()
    cleanup?.()
    cleanup = null
  })

  return { reduced }
}
