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
.step-timeline {
  display: flex;
  gap: 0.5rem;
  margin-top: 1rem;
  padding: 0.25rem 0.1rem 0.55rem;
  overflow-x: auto;
  overscroll-behavior-inline: contain;
  scroll-snap-type: x proximity;
}
.step-timeline button {
  min-width: 10rem;
  max-width: 15rem;
  min-height: 3.75rem;
  flex: 1 0 10rem;
  padding: 0.6rem 0.8rem;
  text-align: left;
  border: 1px solid transparent;
  border-radius: var(--os-radius-panel);
  background: var(--os-panel);
  box-shadow: var(--os-shadow-panel);
  color: var(--os-ink);
  cursor: pointer;
  scroll-snap-align: start;
  transition: border-color var(--os-motion-fast) var(--os-ease), background-color var(--os-motion-fast) var(--os-ease);
}
.step-timeline button:hover { border-color: color-mix(in srgb, var(--os-accent) 45%, transparent); }
.step-timeline button span, .step-timeline button strong { display: block; }
.step-timeline button span { color: var(--os-muted); font-size: 0.875rem; }
.step-timeline button strong { margin-top: 0.15rem; font-size: 0.875rem; font-weight: 650; line-height: 1.4; }
.step-timeline button[aria-current="step"] {
  border-color: var(--os-accent);
  background: var(--os-accent-soft);
  box-shadow: 0 0 0 1px var(--os-accent);
}
.step-timeline button[aria-current="step"] span { color: var(--os-accent); font-weight: 700; }
.step-timeline button:focus-visible { outline: 2px solid var(--os-focus); outline-offset: 2px; }
@media (prefers-reduced-motion: reduce) { .step-timeline *, .step-timeline *::before, .step-timeline *::after { transition: none !important; animation: none !important; } }
</style>
