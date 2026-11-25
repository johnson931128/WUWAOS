<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'

type Thread = 'A' | 'B' | 'C'
type Frame = {
  label: string
  note: string
  queue: Thread[]
  running: (Thread | null)[]
  arrow?: 'dispatch' | 'return'
  currentSlice?: number
  completedSlices?: number
  dispatchedHart?: number
}

const props = defineProps<{ mode: 'round-robin' | 'multi-hart' }>()

// These snapshots describe a fixed teaching scenario. The playback timer is
// independent of the simulated clock and never drives the state transitions.
const roundRobin: Frame[] = [
  { label: '0 ms', note: 'A 從隊首進入 Hart 0，B 和 C 等候。', queue: ['B', 'C'], running: ['A'], arrow: 'dispatch', currentSlice: 0, completedSlices: 0 },
  { label: '50 ms', note: 'A 時間片到期，回到隊尾。', queue: ['B', 'C', 'A'], running: [null], arrow: 'return', completedSlices: 1 },
  { label: '50 ms', note: 'B 從隊首接手，A 在 C 後面等候。', queue: ['C', 'A'], running: ['B'], arrow: 'dispatch', currentSlice: 1, completedSlices: 1 },
  { label: '100 ms', note: 'B 時間片到期，回到隊尾。', queue: ['C', 'A', 'B'], running: [null], arrow: 'return', completedSlices: 2 },
  { label: '100 ms', note: 'C 從隊首進入 Hart 0。', queue: ['A', 'B'], running: ['C'], arrow: 'dispatch', currentSlice: 2, completedSlices: 2 },
  { label: '135 ms', note: 'C 提早完成，A 接著跑剩餘的 45 ms。', queue: ['B'], running: ['A'], arrow: 'dispatch', currentSlice: 3, completedSlices: 3 },
  { label: '180 ms', note: 'A 完成，B 再跑最後 10 ms。', queue: [], running: ['B'], arrow: 'dispatch', currentSlice: 4, completedSlices: 4 },
  { label: '190 ms', note: 'A、B、C 都已完成。', queue: [], running: [null], completedSlices: 5 },
]

const multiHart: Frame[] = [
  { label: '等待派發', note: 'A、B、C 已就緒，從隊首開始派發。', queue: ['A', 'B', 'C'], running: [null, null] },
  { label: '先派發 A', note: 'Hart 0 取走 A，B 成為隊首。', queue: ['B', 'C'], running: ['A', null], dispatchedHart: 0 },
  { label: '同時執行', note: 'Hart 0 跑 A，Hart 1 跑 B，C 在隊首等候。', queue: ['C'], running: ['A', 'B'], dispatchedHart: 1 },
  { label: 'B 已完成', note: 'Hart 1 空出來，Hart 0 繼續跑 A。', queue: ['C'], running: ['A', null] },
  { label: 'C 接手', note: 'Hart 1 從隊首取出 C，與 A 同時執行。', queue: [], running: ['A', 'C'], dispatchedHart: 1 },
]

const timeline: { thread: Thread; start: number; end: number }[] = [
  { thread: 'A', start: 0, end: 50 },
  { thread: 'B', start: 50, end: 100 },
  { thread: 'C', start: 100, end: 135 },
  { thread: 'A', start: 135, end: 180 },
  { thread: 'B', start: 180, end: 190 },
]
const duration = timeline[timeline.length - 1].end
const ticks = [0, 50, 100, duration]

const singleHart = computed(() => props.mode === 'round-robin')
const frames = computed(() => singleHart.value ? roundRobin : multiHart)
const stillIndex = computed(() => singleHart.value ? 1 : 2)
const step = ref<number | null>(null)
const frame = computed(() => frames.value[step.value ?? stillIndex.value])
const figure = ref<HTMLElement | null>(null)

let observer: IntersectionObserver | undefined
let motion: MediaQueryList | undefined
let timer: number | undefined
let visible = false

function stopTimer() {
  if (timer !== undefined) window.clearTimeout(timer)
  timer = undefined
}

function scheduleNext() {
  const last = frames.value.length - 1
  timer = window.setTimeout(() => {
    step.value = ((step.value ?? 0) + 1) % frames.value.length
    scheduleNext()
  }, step.value === last ? 2850 : 2100)
}

function syncPlayback() {
  stopTimer()
  if (motion?.matches) {
    step.value = null
    return
  }
  if (!visible || document.hidden) return
  step.value ??= 0
  scheduleNext()
}

onMounted(() => {
  motion = window.matchMedia('(prefers-reduced-motion: reduce)')
  motion.addEventListener('change', syncPlayback)
  document.addEventListener('visibilitychange', syncPlayback)

  observer = new IntersectionObserver(([entry]) => {
    const inView = entry.isIntersecting && entry.intersectionRatio >= 0.25
    if (visible === inView) return
    visible = inView
    syncPlayback()
  }, { threshold: 0.25 })
  if (figure.value) observer.observe(figure.value)
})

onBeforeUnmount(() => {
  stopTimer()
  observer?.disconnect()
  motion?.removeEventListener('change', syncPlayback)
  document.removeEventListener('visibilitychange', syncPlayback)
})
</script>

<template>
  <figure ref="figure" class="scheduling-demo" :class="{ 'single-hart': singleHart }" :data-scheduling-demo="mode">
    <div class="demo-board">
      <div class="demo-event">
        <strong>{{ frame.label }}</strong>
        <span>{{ frame.note }}</span>
      </div>

      <div class="demo-topology">
        <div class="demo-queue">
          <span class="diagram-label">{{ singleHart ? '就緒佇列' : '共用就緒佇列' }} · 隊首在左</span>
          <div class="queue-slots" role="group" :aria-label="frame.queue.join('、') || '佇列為空'">
            <span
              v-for="slot in 3"
              :key="slot"
              class="queue-slot"
              :data-thread="frame.queue[slot - 1]"
              aria-hidden="true"
            >{{ frame.queue[slot - 1] ?? '–' }}</span>
          </div>
        </div>

        <svg
          v-if="singleHart"
          class="queue-arrow"
          :class="{ active: frame.arrow, returning: frame.arrow === 'return' }"
          viewBox="0 0 48 16"
          aria-hidden="true"
        >
          <path d="M4 8h38m-6-5 6 5-6 5" />
        </svg>
        <svg v-else class="hart-links" viewBox="0 0 240 30" preserveAspectRatio="none" aria-hidden="true">
          <path d="M120 0v12H2v16m-5-5 5 5 5-5" :class="{ active: frame.dispatchedHart === 0 }" />
          <path d="M120 0v12h118v16m-5-5 5 5 5-5" :class="{ active: frame.dispatchedHart === 1 }" />
        </svg>

        <div class="demo-harts">
          <div v-for="(thread, hart) in frame.running" :key="hart" class="hart-slot" :class="{ idle: !thread }">
            <span class="hart-label">Hart {{ hart }}</span>
            <div class="hart-state">
              <span class="thread-tag" :data-thread="thread ?? undefined">{{ thread ?? '–' }}</span>
              <span>{{ thread ? '執行中' : frame.completedSlices === timeline.length ? '閒置' : '等待派發' }}</span>
            </div>
          </div>
        </div>
      </div>

      <div v-if="singleHart" class="demo-timeline">
        <span class="diagram-label">執行時間軸</span>
        <div class="time-track">
          <span
            v-for="(slice, index) in timeline"
            :key="slice.start"
            class="time-slice"
            :class="{ current: index === frame.currentSlice }"
            :data-thread="index < (frame.completedSlices ?? 0) || index === frame.currentSlice ? slice.thread : undefined"
            :style="{ flexGrow: slice.end - slice.start }"
            :aria-hidden="index >= (frame.completedSlices ?? 0) && index !== frame.currentSlice"
          >{{ index < (frame.completedSlices ?? 0) || index === frame.currentSlice ? slice.thread : '' }}</span>
        </div>
        <div class="time-axis" aria-hidden="true">
          <span v-for="tick in ticks" :key="tick" :style="{ left: `${tick / duration * 100}%` }">
            {{ tick === duration ? `${tick} ms` : tick }}
          </span>
        </div>
      </div>
    </div>

    <figcaption v-if="singleHart">
      固定 50 ms 時間片。執行順序為 A → B → C → A → B，外框標示目前的執行區段。
    </figcaption>
    <figcaption v-else>
      B 先完成時，Hart 1 從隊首取出 C。Hart 0 繼續執行 A。
    </figcaption>
  </figure>
</template>

<style scoped>
.scheduling-demo {
  --diagram-ink: var(--os-muted);
  margin: 1rem 0 1.25rem;
  font-size: 0.9375rem;
  line-height: 1.5;
}
.demo-board {
  padding: clamp(0.75rem, 2.5vw, 1.125rem);
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-workspace);
  background: color-mix(in srgb, var(--os-canvas) 78%, var(--os-panel));
}
.demo-event {
  display: flex;
  gap: 0.7rem;
  align-items: baseline;
  min-height: 2.9rem;
  margin-bottom: 0.9rem;
  padding: 0.55rem 0.7rem;
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-panel);
  background: var(--os-panel);
}
.demo-event strong {
  flex: none;
  color: var(--os-cpu);
  font-variant-numeric: tabular-nums;
}
.single-hart .demo-topology {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 3.5rem minmax(0, 1.1fr);
  align-items: center;
}
.diagram-label {
  display: block;
  margin-bottom: 0.3rem;
  color: var(--diagram-ink);
  font-weight: 650;
}
.diagram-label, .hart-label, .hart-state, .time-axis, figcaption {
  font-size: 0.875rem;
}
.queue-slots {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 2.625rem));
  gap: 0.375rem;
}
.queue-slot, .thread-tag, .time-slice {
  display: grid;
  place-items: center;
  border-radius: var(--os-radius-control);
  background: var(--thread-bg, var(--os-soft));
  color: var(--thread-ink, var(--diagram-ink));
  font-family: var(--vp-font-family-mono);
  font-weight: 700;
  transition: background-color 220ms ease, color 220ms ease;
}
[data-thread="A"] {
  --thread-ink: var(--os-kernel);
  --thread-bg: color-mix(in srgb, var(--thread-ink) 13%, var(--os-panel));
}
[data-thread="B"] {
  --thread-ink: var(--os-io);
  --thread-bg: color-mix(in srgb, var(--thread-ink) 13%, var(--os-panel));
}
[data-thread="C"] {
  --thread-ink: var(--os-memory);
  --thread-bg: color-mix(in srgb, var(--thread-ink) 13%, var(--os-panel));
}
.queue-slot {
  min-height: 2.5rem;
  font-size: 1rem;
}
.queue-slot:not([data-thread]) {
  border: 1px dashed var(--os-line);
  background: var(--os-panel);
}
.queue-arrow, .hart-links {
  display: block;
  margin-inline: auto;
  color: var(--diagram-ink);
}
.queue-arrow {
  width: 75%;
  margin-top: 1rem;
}
.queue-arrow.returning { transform: scaleX(-1); }
.queue-arrow path, .hart-links path {
  fill: none;
  stroke: currentColor;
  stroke-width: 1.6;
  vector-effect: non-scaling-stroke;
  stroke-linecap: round;
  stroke-linejoin: round;
}
.queue-arrow.active, .hart-links .active { color: var(--os-cpu); }
.hart-links {
  width: 50%;
  height: 1.7rem;
  overflow: visible;
}
.scheduling-demo:not(.single-hart) .demo-queue { text-align: center; }
.scheduling-demo:not(.single-hart) .queue-slots { justify-content: center; }
.scheduling-demo:not(.single-hart) .demo-harts {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 0.7rem;
}
.hart-slot {
  display: grid;
  align-content: center;
  gap: 0.3rem;
  min-height: 4.875rem;
  padding: 0.55rem 0.7rem;
  border: 1px solid color-mix(in srgb, var(--os-cpu) 27%, var(--os-line));
  border-radius: var(--os-radius-panel);
  background: color-mix(in srgb, var(--os-cpu) 9%, var(--os-panel));
  transition: background-color 220ms ease, border-color 220ms ease;
}
.hart-slot.idle {
  border-color: var(--os-line);
  background: var(--os-panel);
}
.hart-label {
  color: var(--os-cpu);
  font-family: var(--vp-font-family-mono);
  font-weight: 700;
}
.hart-state {
  display: flex;
  flex-wrap: wrap;
  gap: 0.38rem;
  align-items: center;
  color: var(--diagram-ink);
}
.thread-tag {
  min-width: 1.65rem;
  height: 1.65rem;
  font-size: 1rem;
}
.demo-timeline { margin-top: 0.9rem; }
.time-track {
  display: flex;
  gap: 2px;
  min-height: 1.75rem;
}
.time-slice {
  flex-basis: 0;
  min-width: 0;
  font-size: 0.875rem;
}
.time-slice.current { box-shadow: inset 0 0 0 1px var(--thread-ink); }
.time-axis {
  position: relative;
  height: 1.4rem;
  color: var(--diagram-ink);
  font-variant-numeric: tabular-nums;
}
.time-axis span {
  position: absolute;
  transform: translateX(-50%);
}
.time-axis span:first-child { transform: none; }
.time-axis span:last-child { transform: translateX(-100%); white-space: nowrap; }
figcaption {
  margin-top: 0.4rem;
  color: var(--diagram-ink);
}
@media (min-width: 481px) {
  .demo-event { min-height: 3.9rem; align-items: center; }
}
@media (max-width: 480px) {
  .demo-event { display: block; min-height: 5.45rem; }
  .demo-event strong { display: block; }
  .single-hart .demo-topology {
    grid-template-columns: minmax(0, 1fr) 1.85rem minmax(0, 0.85fr);
  }
  .queue-slots {
    grid-template-columns: repeat(3, minmax(0, 1.95rem));
    gap: 0.25rem;
  }
  .queue-slot { min-height: 2.1rem; }
  .hart-slot { padding-inline: 0.45rem; }
}
@media (prefers-reduced-motion: reduce) {
  .queue-slot, .thread-tag, .time-slice, .hart-slot { transition: none; }
}
</style>
