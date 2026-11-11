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
  --playback-bg: var(--os-surface, var(--vp-c-bg-soft, #f3f5f8));
  --playback-border: var(--os-border, var(--vp-c-divider, #d7dce3));
  --playback-text: var(--os-text, var(--vp-c-text-1, #202733));
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.55rem;
  color: var(--playback-text);
}
.playback-controls button {
  min-height: 2.75rem;
  padding: 0.45rem 0.75rem;
  border: 1px solid var(--playback-border);
  border-radius: var(--os-radius-control);
  background: var(--playback-bg);
  color: inherit;
  font: inherit;
  cursor: pointer;
}
.playback-controls button:disabled { opacity: 0.55; cursor: not-allowed; }
.playback-controls button:focus-visible { outline: 3px solid var(--os-focus); outline-offset: 2px; }
.playback-controls button[aria-pressed="true"] { border-color: var(--os-accent); background: var(--os-accent); color: var(--os-on-accent); }
.playback-count { min-width: 3.5rem; font-variant-numeric: tabular-nums; text-align: center; }
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
