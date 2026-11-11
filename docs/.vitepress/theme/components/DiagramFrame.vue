<script setup lang="ts">
import { computed, ref, useId } from 'vue'

const props = withDefaults(defineProps<{
  title: string
  description?: string
  kind?: string
  zoomable?: boolean
}>(), {
  description: undefined,
  kind: undefined,
  zoomable: false,
})

const zoom = ref(1)
const frameId = `diagram-${useId().replace(/[^a-zA-Z0-9-]/g, '-')}`
const zoomLabel = computed(() => `${Math.round(zoom.value * 100)}%`)
const canZoomOut = computed(() => zoom.value > 0.75)
const canZoomIn = computed(() => zoom.value < 1.75)

function changeZoom(amount: number) {
  zoom.value = Math.min(1.75, Math.max(0.75, Math.round((zoom.value + amount) * 100) / 100))
}

function resetZoom() {
  zoom.value = 1
}
</script>

<template>
  <figure class="diagram-frame" :aria-labelledby="`${frameId}-title`" :aria-describedby="description ? `${frameId}-description` : undefined">
    <header class="diagram-heading">
      <div class="diagram-heading-copy">
        <span v-if="kind" class="diagram-kind">{{ kind }}</span>
        <h3 :id="`${frameId}-title`">{{ title }}</h3>
        <p v-if="description" :id="`${frameId}-description`" class="diagram-description">{{ description }}</p>
      </div>
      <div v-if="zoomable" class="zoom-controls" role="group" :aria-label="`${title} 圖解縮放控制`">
        <button type="button" :disabled="!canZoomOut" aria-label="縮小圖解" @click="changeZoom(-0.25)">−</button>
        <span aria-live="polite" aria-atomic="true">{{ zoomLabel }}</span>
        <button type="button" :disabled="!canZoomIn" aria-label="放大圖解" @click="changeZoom(0.25)">＋</button>
        <button type="button" :disabled="zoom === 1" aria-label="還原圖解大小" @click="resetZoom">還原</button>
      </div>
    </header>
    <div
      class="diagram-viewport"
      :role="zoomable ? 'region' : undefined"
      :class="{ zoomable }"
      :aria-label="zoomable ? `${title} 圖解；可在此區域捲動檢視放大內容` : undefined"
      :tabindex="zoomable ? 0 : undefined"
      :style="{ '--diagram-scale': zoom }"
    >
      <div class="diagram-content"><slot /></div>
    </div>
    <figcaption class="diagram-caption">
      <slot name="caption">{{ description || title }}</slot>
    </figcaption>
  </figure>
</template>

<style scoped>
.diagram-frame {
  --diagram-bg: var(--os-surface, var(--vp-c-bg-soft, #f3f5f8));
  --diagram-border: var(--os-border, var(--vp-c-divider, #d7dce3));
  --diagram-text: var(--os-text, var(--vp-c-text-1, #202733));
  --diagram-muted: var(--os-muted, var(--vp-c-text-2, #596273));
  --diagram-accent: var(--os-accent, #476de0);
  margin: 1.5rem 0;
  padding: clamp(0.9rem, 2.5vw, 1.4rem);
  border: 1px solid var(--diagram-border);
  border-radius: var(--os-radius-workspace);
  background: var(--diagram-bg);
  color: var(--diagram-text);
  min-width: 0;
}
.diagram-heading {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  gap: 1rem;
  margin-bottom: 1rem;
}
.diagram-heading h3 { margin: 0.25rem 0; color: inherit; font-size: 1.1rem; }
.diagram-kind { color: var(--diagram-accent); font-size: 0.78rem; font-weight: 700; letter-spacing: 0.04em; }
.diagram-description, .diagram-caption { margin: 0.3rem 0 0; color: var(--diagram-muted); font-size: 0.9rem; line-height: 1.6; }
.diagram-viewport { min-width: 0; max-width: 100%; }
.diagram-viewport.zoomable {
  max-height: min(72vh, 48rem);
  overflow: auto;
  overscroll-behavior: contain;
  border: 1px solid color-mix(in srgb, var(--diagram-border) 70%, transparent);
  border-radius: 0.65rem;
  scrollbar-color: var(--diagram-muted) transparent;
}
.diagram-viewport.zoomable:focus-visible { outline: 3px solid var(--os-focus); outline-offset: 2px; }
.diagram-content { min-width: 0; transform: scale(var(--diagram-scale, 1)); transform-origin: top left; }
.diagram-viewport.zoomable .diagram-content { width: 100%; }
.diagram-content :deep(svg) { display: block; max-width: 100%; height: auto; }
.diagram-viewport.zoomable .diagram-content :deep(svg) { max-width: none; }
.diagram-caption { margin-top: 0.75rem; }
.zoom-controls { display: flex; flex: 0 0 auto; align-items: center; gap: 0.35rem; }
.zoom-controls button {
  min-width: 2.75rem;
  min-height: 2.75rem;
  padding: 0.35rem 0.55rem;
  border: 1px solid var(--diagram-border);
  border-radius: 0.45rem;
  background: var(--os-control-surface, var(--vp-c-bg, #fff));
  color: inherit;
  font: inherit;
  cursor: pointer;
}
.zoom-controls button:disabled { opacity: 0.55; cursor: not-allowed; }
.zoom-controls button:focus-visible { outline: 3px solid var(--os-focus); outline-offset: 2px; }
.zoom-controls span { min-width: 3rem; color: var(--diagram-muted); font-size: 0.82rem; text-align: center; font-variant-numeric: tabular-nums; }
@media (max-width: 640px) {
  .diagram-heading { flex-direction: column; }
  .zoom-controls { max-width: 100%; flex-wrap: wrap; }
}
@media (prefers-reduced-motion: reduce) {
  .diagram-frame, .diagram-frame * { scroll-behavior: auto; transition: none; }
}
</style>
