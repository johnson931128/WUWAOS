<script setup lang="ts">
import { computed, ref } from 'vue'
import DiagramFrame from './DiagramFrame.vue'
import PlaybackControls from './PlaybackControls.vue'
import { usePlayback } from '../composables/usePlayback'

type Job = { id: string; arrival: number; burst: number; color: string }
type Slice = { id: string; start: number; end: number; remaining: number }
type Frame = {
  time: number
  running: string | null
  slice: Slice | null
  ready: string[]
  remaining: Record<string, number>
  arrivals: string[]
  note: string
  gantt: Slice[]
}

const jobs: Job[] = [
  { id: 'A', arrival: 0, burst: 95, color: 'var(--os-cpu, #d68a24)' },
  { id: 'B', arrival: 25, burst: 60, color: 'var(--os-process, #7b61a8)' },
  { id: 'C', arrival: 40, burst: 35, color: 'var(--os-memory, #168b87)' },
]
const quantum = ref(50)

function simulate(q: number): Frame[] {
  const remaining: Record<string, number> = Object.fromEntries(jobs.map((job) => [job.id, job.burst]))
  const arrived = new Set<string>()
  const ready: string[] = []
  const gantt: Slice[] = []
  const frames: Frame[] = [{ time: 0, running: null, slice: null, ready: [], remaining: { ...remaining }, arrivals: [], note: '尚未派發：工作負載尚未開始。', gantt: [] }]
  let time = 0
  while (Object.values(remaining).some((amount) => amount > 0) && frames.length < 80) {
    const arrivals: string[] = []
    for (const job of jobs) {
      if (!arrived.has(job.id) && job.arrival <= time) {
        arrived.add(job.id)
        ready.push(job.id)
        arrivals.push(job.id)
      }
    }
    if (ready.length === 0) {
      const next = jobs.filter((job) => !arrived.has(job.id)).sort((a, b) => a.arrival - b.arrival)[0]
      if (!next) break
      const start = time
      time = next.arrival
      frames.push({ time, running: null, slice: null, ready: [], remaining: { ...remaining }, arrivals: [], note: `CPU 閒置 ${time - start} ms，直到工作 ${next.id} 抵達。`, gantt: [...gantt] })
      continue
    }
    const id = ready.shift()!
    const job = jobs.find((item) => item.id === id)!
    const start = time
    const duration = Math.min(q, remaining[id])
    time += duration
    remaining[id] -= duration
    const slice: Slice = { id, start, end: time, remaining: remaining[id] }
    gantt.push(slice)
    const arrivingDuringSlice: string[] = []
    for (const candidate of jobs) {
      if (!arrived.has(candidate.id) && candidate.arrival <= time) {
        arrived.add(candidate.id)
        ready.push(candidate.id)
        arrivingDuringSlice.push(candidate.id)
      }
    }
    const done = remaining[id] === 0
    if (!done) ready.push(id)
    const event = done ? `工作 ${id} 完成後離開` : `時間片到期，工作 ${id} 以剩餘 ${remaining[id]} ms 回到隊尾`
    const arrivalsText = arrivingDuringSlice.length ? `；期間工作 ${arrivingDuringSlice.join(', ')} 抵達並加入隊尾` : ''
    frames.push({ time, running: id, slice, ready: [...ready], remaining: { ...remaining }, arrivals: arrivingDuringSlice, note: `${id} 執行 ${duration} ms（${start}–${time} ms），${event}${arrivalsText}。`, gantt: [...gantt] })
  }
  return frames
}

const frames = computed(() => simulate(quantum.value))
const playback = usePlayback(computed(() => frames.value.length), 1000)
const current = computed(() => frames.value[playback.index.value] ?? frames.value[0])
const pixelsPerMs = computed(() => {
  const slices = frames.value.at(-1)?.gantt ?? []
  const shortest = Math.min(...slices.map(slice => slice.end - slice.start))
  return slices.length ? 80 / shortest : 1
})
const jobMap = Object.fromEntries(jobs.map((job) => [job.id, job]))
function reset() { playback.reset() }
</script>

<template>
  <section class="scheduling-lab" aria-labelledby="scheduling-lab-title">
    <div class="lab-heading">
      <h3 id="scheduling-lab-title">就緒佇列與 CPU 時間軸</h3>
      <label class="quantum-control"><span>教學時間片</span><select v-model.number="quantum" @change="reset"><option :value="25">25 ms</option><option :value="50">50 ms</option><option :value="100">100 ms</option></select></label>
    </div>

    <div class="workload" aria-label="固定示例工作負載">
      <strong>固定工作負載</strong>
      <span v-for="job in jobs" :key="job.id" class="workload-job"><b :style="{ '--job-color': job.color }">{{ job.id }}</b> arrival {{ job.arrival }} ms · burst {{ job.burst }} ms</span>
    </div>

    <PlaybackControls :step="playback.index.value" :total="frames.length" :playing="playback.playing.value" @previous="playback.previous" @next="playback.next" @toggle="playback.togglePlay" @reset="reset" />

    <p class="live-note" aria-live="polite"><b>{{ current.time }} ms</b> · {{ current.note }}</p>
    <div class="scheduler-board">
      <DiagramFrame title="RR ready queue 與目前 CPU" description="目前執行的工作、先進先出 ready queue 與各工作的剩餘 burst。這是網站固定工作負載的計算結果。" kind="dynamic" :zoomable="false">
        <div class="queue-layout">
          <div class="lane">
            <h4>片尾 Ready queue <small>requeue 後 · head → tail</small></h4>
            <div class="queue-cards" aria-label="就緒佇列">
              <span v-if="!current.ready.length" class="empty-state">空</span>
              <span v-for="(id, index) in current.ready" :key="id" class="queue-card" :style="{ '--job-color': jobMap[id].color }"><small>{{ index === 0 ? 'HEAD' : `#${index + 1}` }}</small><b>{{ id }}</b></span>
            </div>
          </div>
          <div class="cpu" :class="{ idle: !current.running }" :style="current.running ? { '--job-color': jobMap[current.running].color } : {}">
            <small>單核心 CPU · {{ current.running ? '本步執行 · slice 結束後' : 'IDLE' }}</small><strong>{{ current.running ?? '—' }}</strong>
            <span v-if="current.slice">本步 slice {{ current.slice.start }}–{{ current.slice.end }} ms · 片尾剩餘 {{ current.slice.remaining }} ms</span><span v-else>等待下一個 arrival</span>
          </div>
        </div>
        <div class="remaining-grid" aria-label="工作剩餘時間">
          <div v-for="job in jobs" :key="job.id" class="remaining-row" :style="{ '--job-color': job.color }"><b>{{ job.id }}</b><span>{{ current.remaining[job.id] }} / {{ job.burst }} ms</span><span class="remaining-track"><i :style="{ width: `${100 * current.remaining[job.id] / job.burst}%` }" /></span></div>
        </div>
      </DiagramFrame>

      <DiagramFrame title="Gantt 執行時間軸" description="依同一個 RR 模擬結果逐步累積的 CPU 執行區段；方塊內有工作名稱，寬度依執行毫秒數縮放。" kind="dynamic" :zoomable="true">
        <div class="gantt-wrap" role="img" :aria-label="`Gantt 時間軸，已執行至 ${current.time} 毫秒`">
          <div class="gantt-axis" :style="{ width: `${current.time * pixelsPerMs}px` }"><span>0 ms</span><span v-if="current.time">{{ current.time }} ms</span></div>
          <div class="gantt-track">
            <span v-for="(slice, index) in current.gantt" :key="`${slice.id}-${slice.start}-${index}`" class="gantt-block" :style="{ width: `${(slice.end - slice.start) * pixelsPerMs}px`, '--job-color': jobMap[slice.id].color }" :title="`${slice.id}: ${slice.start}–${slice.end} ms`"><b>{{ slice.id }}</b><small>{{ slice.start }}–{{ slice.end }}</small></span>
            <span v-if="!current.gantt.length" class="empty-state">按下一步開始模擬</span>
          </div>
          <div class="gantt-legend"><span v-for="job in jobs" :key="job.id"><i :style="{ background: job.color }" />{{ job.id }}</span><span><i class="idle-key" />idle</span></div>
        </div>
      </DiagramFrame>
    </div>
    <p class="model-boundary">這個單 Hart 教學模型只計算工作到達、執行以及時間片結束後的輪轉。時間軸記錄剛完成的時間片，就緒佇列顯示該步結束時的順序。調整時間片只會重算這組範例。<strong>WUWAOS 核心實際使用 50 ms</strong>，核心態可在指定的安全檢查點搶占。</p>
  </section>
</template>

<style scoped>
.scheduling-lab{margin:1.6rem 0;padding:1.25rem;border:1px solid var(--vp-c-divider);border-radius:var(--os-radius-workspace);background:var(--os-panel);color:var(--vp-c-text-1)}
.lab-heading{display:flex;justify-content:space-between;align-items:flex-end;gap:1rem}.eyebrow{margin:0;color:var(--os-cpu,#a7680c);font-size:.76rem;font-weight:700;letter-spacing:.04em}.lab-heading h3{margin:.25rem 0 0;font-size:1.2rem}.quantum-control{display:grid;gap:.35rem;font-size:.8rem}.quantum-control select{padding:.45rem .65rem;border:1px solid var(--vp-c-divider);border-radius:8px;background:var(--vp-c-bg);color:inherit}
.workload{display:flex;flex-wrap:wrap;align-items:center;gap:.5rem .9rem;margin:1rem 0;padding:.8rem;border-radius:12px;background:var(--vp-c-bg)}.workload-job{font-size:.78rem}.workload-job b{display:inline-grid;place-items:center;width:1.35rem;height:1.35rem;border-radius:5px;background:var(--job-color);color:var(--os-on-accent)}
.live-note{min-height:2.5rem;padding:.7rem .85rem;border:1px solid var(--os-line);border-radius:var(--os-radius-control);background:var(--os-warning-soft);color:var(--os-ink);font-size:.9rem}.scheduler-board{display:grid;gap:1rem}.queue-layout{display:grid;grid-template-columns:minmax(0,1fr) minmax(140px,.45fr);align-items:center;gap:1rem}.lane h4{margin:0 0 .55rem;font-size:.9rem}.lane h4 small{color:var(--vp-c-text-2);font-weight:400}.queue-cards{display:flex;flex-wrap:wrap;gap:.5rem;min-height:3.6rem;align-items:center}.queue-card{display:grid;justify-items:center;min-width:3rem;padding:.35rem;border:1px solid var(--job-color);border-radius:10px;background:color-mix(in srgb,var(--job-color) 15%,var(--vp-c-bg));}.queue-card b{font-size:1.15rem}.queue-card small{font-size:.58rem;color:var(--vp-c-text-2)}.empty-state{color:var(--vp-c-text-2);font-size:.8rem}.cpu{display:grid;justify-items:center;gap:.25rem;padding:.8rem;border:2px solid var(--job-color,var(--vp-c-divider));border-radius:14px;background:color-mix(in srgb,var(--job-color,var(--vp-c-bg)) 15%,var(--vp-c-bg));text-align:center}.cpu.idle{border-style:dashed}.cpu strong{font-size:1.7rem}.cpu small,.cpu span{font-size:.68rem}.remaining-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:.5rem;margin-top:1rem}.remaining-row{display:grid;grid-template-columns:auto auto;gap:.1rem .45rem;align-items:center;padding:.5rem;border-radius:9px;background:var(--vp-c-bg);font-size:.75rem}.remaining-row>b{color:var(--job-color)}.remaining-track{grid-column:1/-1;height:6px;overflow:hidden;border-radius:9px;background:var(--vp-c-bg-alt)}.remaining-track i{display:block;height:100%;background:var(--job-color);transition:width var(--os-motion-fast) var(--os-ease)}.gantt-wrap{overflow-x:auto;padding:.2rem .1rem .4rem}.gantt-axis{display:flex;justify-content:space-between;font-size:.7rem;color:var(--vp-c-text-2)}.gantt-track{display:flex;min-width:260px;min-height:3.4rem;align-items:stretch;overflow:hidden;border:1px solid var(--vp-c-divider);border-radius:9px;background:var(--vp-c-bg)}.gantt-block{display:grid;align-content:center;justify-items:center;min-width:3rem;overflow:hidden;border-right:2px solid var(--vp-c-bg);background:var(--job-color);color:var(--os-on-accent)}.gantt-block b{font-size:.88rem}.gantt-block small{font-size:.58rem;white-space:nowrap}.gantt-legend{display:flex;gap:.85rem;margin-top:.5rem;font-size:.7rem}.gantt-legend span{display:flex;align-items:center;gap:.3rem}.gantt-legend i{width:.7rem;height:.7rem;border-radius:3px}.idle-key{border:1px dashed var(--vp-c-text-2)}.model-boundary{margin:1rem 0 0;padding:.8rem;border:1px solid var(--os-line);background:var(--os-warning-soft);border-radius:var(--os-radius-panel);font-size:.8rem;line-height:1.55}
@media(max-width:640px){.scheduling-lab{padding:.9rem}.lab-heading{align-items:flex-start;flex-direction:column}.quantum-control{grid-template-columns:auto 1fr;align-items:center}.queue-layout{grid-template-columns:1fr}.cpu{justify-self:stretch}.remaining-grid{grid-template-columns:1fr}.gantt-block{min-width:2.7rem}}
.quantum-control, .workload-job, .queue-card small, .empty-state,
.cpu small, .cpu span, .remaining-row, .gantt-axis, .gantt-legend,
.gantt-block small, .model-boundary { font-size: .875rem; }
.gantt-axis { min-width: 4.75rem; }
.gantt-track { width: max-content; min-width: 0; }
.gantt-block { flex-shrink: 0; min-width: 0; padding-inline: .2rem; }
.gantt-track .empty-state { padding-inline: .65rem; }
@media(prefers-reduced-motion:reduce){.remaining-track i{transition:none}}
</style>
