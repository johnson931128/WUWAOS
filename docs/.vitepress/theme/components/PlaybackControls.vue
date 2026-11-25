<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps<{
  step: number
  total: number
  playing: boolean
}>()

const emit = defineEmits<{
  previous: []
  next: []
  toggle: []
  reset: []
}>()

const boundedTotal = computed(() => Math.max(0, Math.floor(props.total)))
const boundedStep = computed(() => boundedTotal.value === 0
  ? 0
  : Math.min(boundedTotal.value - 1, Math.max(0, Math.floor(props.step))))
const isFirst = computed(() => boundedStep.value <= 0)
const isLast = computed(() => boundedTotal.value === 0 || boundedStep.value >= boundedTotal.value - 1)
const status = computed(() => {
  if (boundedTotal.value === 0) return '目前沒有可播放的步驟'
  return `第 ${boundedStep.value + 1} 步，共 ${boundedTotal.value} 步；${props.playing ? '播放中' : '已暫停'}`
})
</script>

<template>
  <div class="playback-controls" role="group" aria-label="圖解步驟控制">
    <button type="button" :disabled="isFirst" aria-label="前一步" @click="emit('previous')">← 前一步</button>
    <button
      type="button"
      :disabled="boundedTotal < 2"
      :aria-pressed="playing"
      :aria-label="playing ? '暫停播放' : '播放步驟'"
      @click="emit('toggle')"
    >{{ playing ? '暫停' : '播放' }}</button>
    <button type="button" :disabled="boundedTotal === 0" aria-label="重設至第一步" @click="emit('reset')">重設</button>
    <button type="button" :disabled="isLast" aria-label="下一步" @click="emit('next')">下一步 →</button>
    <span class="playback-count" aria-hidden="true">{{ boundedTotal ? `${boundedStep + 1} / ${boundedTotal}` : '0 / 0' }}</span>
    <span class="visually-hidden" aria-live="polite" aria-atomic="true">{{ status }}</span>
  </div>
</template>

<style scoped>
.playback-controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem;
  color: var(--os-ink);
}
.playback-controls button {
  min-height: 40px;
  padding: 0.4rem 0.85rem;
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-control);
  background: var(--os-panel);
  color: var(--os-ink);
  font: inherit;
  font-size: 0.875rem;
  font-weight: 600;
  cursor: pointer;
  transition: background-color var(--os-motion-fast) var(--os-ease), border-color var(--os-motion-fast) var(--os-ease), color var(--os-motion-fast) var(--os-ease);
}
.playback-controls button:hover:not(:disabled) {
  border-color: var(--os-accent);
  background: var(--os-accent-soft);
  color: var(--os-accent);
}
.playback-controls button:nth-of-type(4):not(:disabled) {
  background: var(--os-accent);
  border-color: var(--os-accent);
  color: var(--os-on-accent);
}
.playback-controls button:nth-of-type(4):hover:not(:disabled) {
  background: var(--os-accent-hover);
  color: var(--os-on-accent);
}
.playback-controls button:disabled { opacity: 0.45; cursor: not-allowed; }
.playback-controls button:focus-visible { outline: 2px solid var(--os-focus); outline-offset: 2px; }
.playback-controls button[aria-pressed="true"] {
  border-color: var(--os-accent);
  background: var(--os-accent-soft);
  color: var(--os-accent);
}
.playback-count {
  margin-left: auto;
  padding: 0.25rem 0.65rem;
  border-radius: 999px;
  background: var(--os-panel);
  color: var(--os-muted);
  font-size: 0.875rem;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
}
.visually-hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border: 0;
}
@media (prefers-reduced-motion: reduce) {
  .playback-controls, .playback-controls * { scroll-behavior: auto; transition: none; }
}
</style>
