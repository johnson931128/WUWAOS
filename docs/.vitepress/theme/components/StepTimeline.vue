<script setup lang="ts">
import { onUpdated, ref } from 'vue'
const props = defineProps<{ frames: Array<{ title?: string; stage?: string }>; current: number; label: string }>()
const emit = defineEmits<{ seek: [index: number] }>()
const strip = ref<HTMLElement | null>(null)
const items = ref<HTMLButtonElement[]>([])
onUpdated(() => {
  const nav = strip.value
  const current = items.value[props.current]
  if (!nav || !current) return
  const left = current.offsetLeft - nav.offsetLeft
  if (left < nav.scrollLeft) nav.scrollLeft = left
  else if (left + current.offsetWidth > nav.scrollLeft + nav.clientWidth) nav.scrollLeft = left + current.offsetWidth - nav.clientWidth
})
</script>

<template>
  <nav ref="strip" class="step-timeline" :aria-label="label">
    <button v-for="(frame, index) in frames" :key="index" ref="items" :data-timeline-index="index" type="button" :aria-current="index === current ? 'step' : undefined" @click="emit('seek', index)">
      <span>{{ index < current ? '已經過' : index === current ? '目前' : '之後' }}</span>
      <strong>{{ frame.title ?? frame.stage }}</strong>
    </button>
  </nav>
</template>

<style scoped>
.step-timeline { display: flex; gap: .4rem; margin-top: 1rem; padding: .2rem .1rem .55rem; overflow-x: auto; overscroll-behavior-inline: contain; scroll-snap-type: x proximity; }
.step-timeline button { min-width: 9rem; max-width: 14rem; min-height: 3.5rem; flex: 1 0 9rem; padding: .55rem; text-align: left; border: 1px solid var(--os-line,var(--vp-c-divider)); border-radius: 8px; background: var(--os-panel,var(--vp-c-bg)); color: var(--os-ink,var(--vp-c-text-1)); cursor: pointer; scroll-snap-align: start; }
.step-timeline button span, .step-timeline button strong { display: block; }
.step-timeline button span { color: var(--os-muted,var(--vp-c-text-2)); font-size: .8rem; }
.step-timeline button[aria-current="step"] { border-color: var(--os-accent,var(--vp-c-brand-1)); background: color-mix(in srgb,var(--os-accent,var(--vp-c-brand-1)) 8%,var(--os-panel,var(--vp-c-bg))); }
.step-timeline button[aria-current="step"] span { color: var(--os-accent,var(--vp-c-brand-1)); font-weight: 700; }
.step-timeline button:focus-visible { outline: 3px solid var(--os-accent,var(--vp-c-brand-1)); outline-offset: 2px; }
@media (prefers-reduced-motion: reduce) { .step-timeline *, .step-timeline *::before, .step-timeline *::after { transition: none !important; animation: none !important; } }
</style>
