<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from 'vue'
import { withBase } from 'vitepress'

type State = 'NEW' | 'READY' | 'RUNNING' | 'WAITING' | 'ZOMBIE' | 'UNUSED'
type Frame = { title: string; why: string; state: State; queue: string[]; cpu: string; waiting: string[]; edge: string; field: string; source: string }
const scenarios = {
  rr: { label: '時間片輪轉', intro: 'A、B、C 都可執行。排程器照隊列順序取出執行緒，用完時間片就排回隊尾。', frames: [
    { title: 'A 正在使用 hart 0', why: '排程器先選中 A，給它 50 ms 的時間片。B 和 C 留在就緒佇列，等 CPU 空出來。', state: 'RUNNING', queue: ['B', 'C'], cpu: 'A', waiting: [], edge: '', field: 'current = A；Ready head = B', source: 'kernel/proc/process.c' },
    { title: 'A 的時間片到期，排回隊尾', why: '計時器在 A 執行使用者程式時發出中斷。核心保存 A 的執行進度，將它排到隊尾，再交還控制權給排程器。', state: 'READY', queue: ['B', 'C', 'A'], cpu: '', waiting: [], edge: 'preempt', field: 'A.preemptions++；A.state = THREAD_READY', source: 'arch/riscv/trap.c' },
    { title: '隊首 B 取得 CPU', why: '排程器從隊首取出 B，啟用它所屬行程的頁表，恢復它的執行狀態並重設計時器。', state: 'READY', queue: ['C', 'A'], cpu: 'B', waiting: [], edge: 'dispatch', field: 'B.state = THREAD_RUNNING；quantum = 50 ms', source: 'kernel/proc/scheduler.c' },
    { title: 'B 到期，也排回隊尾', why: 'B 在使用者模式用完時間片後也回到隊尾。C 現在排在最前面。', state: 'READY', queue: ['C', 'A', 'B'], cpu: '', waiting: [], edge: 'preempt', field: 'Ready queue = C → A → B', source: 'kernel/proc/process.c' },
    { title: 'C 執行，A 留在下一個位置', why: 'C 開始執行，A 排在 B 前面。如果佇列只剩一條執行緒，排程器會再次選中它。', state: 'READY', queue: ['A', 'B'], cpu: 'C', waiting: [], edge: 'dispatch', field: 'current = C；Ready queue = A → B', source: 'kernel/proc/process.c' },
  ] as Frame[] },
  io: { label: 'I/O 等待', intro: 'A 讀檔時要等磁碟，B 可以先跑。讀取完成後，A 回到就緒佇列，等待下一次派發。', frames: [
    { title: 'A 準備讀取資料', why: 'A 呼叫 read，核心接手處理這次讀取，並使用 A 的核心堆疊保存函式資料。這次需要等裝置提供資料。', state: 'RUNNING', queue: ['B'], cpu: 'A', waiting: [], edge: '', field: 'A.kernel_call_active = 1', source: 'kernel/io/io.c' },
    { title: '登記等待，A 讓出 CPU', why: '核心記錄 A 在等哪一次裝置事件，把 A 標為等待中，並保留這次讀取的核心堆疊。', state: 'WAITING', queue: ['B'], cpu: '', waiting: ['A'], edge: 'block', field: 'A.wait_reason = WAIT_EVENT；event_wait.active = 1', source: 'kernel/proc/process.c' },
    { title: 'A 等待期間，B 繼續前進', why: '排程器選中 B。A 已離開就緒佇列，在裝置完成前不會占用 CPU 反覆檢查結果。', state: 'WAITING', queue: [], cpu: 'B', waiting: ['A'], edge: 'dispatch', field: 'current = B；A.state = THREAD_WAITING', source: 'kernel/proc/scheduler.c' },
    { title: '事件完成，A 回到隊尾', why: '核心可透過裝置中斷或主動檢查得知結果，接著把 A 排回就緒佇列。B 此時仍在執行。', state: 'READY', queue: ['A'], cpu: 'B', waiting: [], edge: 'wake', field: 'A.event_wait.completed = 1；A.state = THREAD_READY', source: 'kernel/proc/process.c' },
    { title: 'B 讓出後，才輪到 A', why: 'B 呼叫 yield 主動讓出 CPU，排到 A 後面。A 現在位於隊首。', state: 'READY', queue: ['A', 'B'], cpu: '', waiting: [], edge: 'preempt', field: 'Ready queue = A → B', source: 'kernel/proc/process.c' },
    { title: '恢復 A 原本的 read 呼叫', why: '排程器恢復 A 的核心堆疊，讓原本的 read 接著處理裝置回報的結果。核心完成後，A 回到使用者程式。', state: 'RUNNING', queue: ['B'], cpu: 'A', waiting: [], edge: 'dispatch', field: 'A.wait_reason = WAIT_NONE；原 kernel stack 繼續', source: 'arch/riscv/swtch.S' },
  ] as Frame[] },
  exit: { label: '退出與回收', intro: 'A 和 B 屬於同一行程。A 結束後，B 可以用 thread_join 取得 A 的退出值並回收它。', frames: [
    { title: '同一 Process，兩條獨立 Thread', why: 'A 是額外建立的執行緒，有自己的 TCB、使用者堆疊和核心堆疊。它和 B 共用同一行程的記憶體映射與開啟檔案表。', state: 'RUNNING', queue: ['B'], cpu: 'A', waiting: [], edge: '', field: 'A.process = B.process；A.tid ≠ B.tid', source: 'kernel/proc/thread.h' },
    { title: 'A 結束，先保留 Zombie', why: 'A 的入口函式回傳 7，啟動用的組合語言程式接手呼叫 thread_exit。核心記下退出值，切回排程器，暫時保留 A 的 TCB 和堆疊。', state: 'ZOMBIE', queue: ['B'], cpu: '', waiting: [], edge: '', field: 'A.exit_status = 7；A.state = THREAD_ZOMBIE', source: 'kernel/proc/process.c' },
    { title: 'B 繼續使用共享資源', why: 'B 接著執行，仍可使用行程的開啟檔案表與頁表。若呼叫的是行程的 exit，核心就會要求同一行程的所有執行緒結束。', state: 'ZOMBIE', queue: [], cpu: 'B', waiting: [], edge: 'dispatch', field: 'Process.state = PROCESS_LIVE', source: 'kernel/proc/process.c' },
    { title: 'B 取得回收 A 的資格', why: 'B 呼叫 thread_join，核心先保留回收資格，避免兩條執行緒回收同一個目標。A 已經退出，核心可以開始把退出值寫回 B 提供的位置。', state: 'ZOMBIE', queue: [], cpu: 'B', waiting: [], edge: '', field: 'A.join_claimed = 1；A.joiner_tid = B.tid', source: 'kernel/proc/process.c' },
    { title: 'A 的 TCB 回收，B 繼續執行', why: '核心把 7 寫到 B 提供的變數，回收 A 的額外堆疊和 TCB，最後回傳 A 的 TID。B 現在可以讀取退出值。', state: 'UNUSED', queue: [], cpu: 'B', waiting: [], edge: '', field: 'A 的 TCB slot = THREAD_UNUSED；join status = 7', source: 'kernel/proc/thread.c' },
  ] as Frame[] },
}
type Scenario = keyof typeof scenarios
const scenario = ref<Scenario>('rr')
const index = ref(0)
const playing = ref(false)
const sequence = computed(() => scenarios[scenario.value])
const frame = computed(() => sequence.value.frames[index.value])
// Stack the reading frames in one grid cell so the longest text reserves space
// at every viewport width. Hidden frames cannot receive keyboard focus.
const readingFrames = Object.entries(scenarios).flatMap(([key, item]) =>
  item.frames.map((content, position) => ({ key, label: item.label, position, ...content })))
const states: State[] = ['UNUSED', 'NEW', 'READY', 'RUNNING', 'WAITING', 'ZOMBIE']
let timer: ReturnType<typeof setTimeout> | undefined
function pause() { clearTimeout(timer); playing.value = false }
function select(position: number) { pause(); index.value = position }
function change(next: Scenario) { pause(); scenario.value = next; index.value = 0 }
function schedule() {
  timer = setTimeout(() => {
    index.value++
    if (index.value === sequence.value.frames.length - 1) pause()
    else schedule()
  }, 6500)
}
function toggle() {
  if (playing.value) return pause()
  if (index.value === sequence.value.frames.length - 1) index.value = 0
  playing.value = true
  schedule()
}
onBeforeUnmount(pause)
</script>

<template>
  <section class="thread-flow" aria-label="Thread 狀態與排程解析">
    <div class="scenario-picker" role="group" aria-label="選擇 Thread 情境">
      <button v-for="(item, key) in scenarios" :key="key" :aria-pressed="scenario === key" @click="change(key)">{{ item.label }}</button>
    </div>
    <p class="scenario-intro">{{ sequence.intro }}</p>
    <p class="model-note">A、B、C 代表教學情境中的執行緒。每一步顯示一個狀態變化。</p>
    <div class="map-container">
      <div class="system-map">
        <p class="return-label">時間片到期／yield：Running → Ready 隊尾</p>
        <svg class="connections" viewBox="0 0 1000 420" preserveAspectRatio="none" aria-hidden="true">
          <path d="M300 210H360" :class="{ selected: frame.edge === 'dispatch' }" />
          <path d="M350 203L360 210L350 217" :class="{ selected: frame.edge === 'dispatch' }" />
          <path d="M640 210H700M690 203L700 210L690 217" :class="{ selected: frame.edge === 'block' }" />
          <path d="M500 110V42H160V110M153 100L160 110L167 100" :class="{ selected: frame.edge === 'preempt' }" />
          <path d="M840 310V378H160V310M153 320L160 310L167 320" :class="{ selected: frame.edge === 'wake' }" />
        </svg>
        <div class="map-node ready-node" :class="{ active: frame.state === 'READY' }">
          <svg viewBox="0 0 32 32" width="32" height="32" aria-hidden="true"><path d="M5 8h22M5 16h22M5 24h22M9 5v6m0 2v6m0 2v6" /></svg>
          <h3>就緒佇列</h3><p>依隊首到隊尾排列</p>
          <ol aria-label="Ready queue 順序"><li v-for="name in frame.queue" :key="name">{{ name }}</li></ol>
          <span v-if="!frame.queue.length" class="empty">目前沒有就緒的執行緒</span>
        </div>
        <p class="mobile-connection">隊首出隊 ↓</p>
        <div class="map-node cpu-node" :class="{ active: frame.cpu === 'A' }">
          <svg viewBox="0 0 32 32" width="32" height="32" aria-hidden="true"><path d="M9 9h14v14H9zM13 13h6v6h-6zM12 4v5m8-5v5M12 23v5m8-5v5M4 12h5m-5 8h5m14-8h5m-5 8h5" /></svg>
          <h3>hart 0</h3><p>單一執行位置</p>
          <strong class="cpu-value">{{ frame.cpu ? `Thread ${frame.cpu}` : '排程切換中' }}</strong>
          <span>派發時間片 50 ms</span>
        </div>
        <p class="mobile-connection">阻塞時讓出 CPU ↓</p>
        <div class="map-node wait-node" :class="{ active: frame.state === 'WAITING' }">
          <svg viewBox="0 0 32 32" width="32" height="32" aria-hidden="true"><circle cx="16" cy="16" r="11" /><path d="M16 9v8l5 3" /></svg>
          <h3>等待中</h3><p>等待裝置或其他事件</p>
          <strong>{{ frame.waiting.length ? `Thread ${frame.waiting.join('、')}` : '目前沒有等待者' }}</strong>
          <span>保留原核心堆疊</span>
        </div>
        <p class="wake-label">事件完成／逾時／取消：Waiting → Ready 隊尾</p>
      </div>
    </div>
    <div class="state-strip" aria-label="目前追蹤 A 的 TCB 狀態">
      <span class="state-caption">A 的狀態</span>
      <span v-for="state in states" :key="state" class="state-tag" :class="{ chosen: frame.state === state }" :aria-current="frame.state === state ? 'true' : undefined">{{ frame.state === state ? '● ' : '' }}{{ state }}</span>
    </div>
    <p class="lifecycle-note">建立執行緒時，核心先保留空的 TCB，準備完成後再排入就緒佇列。退出後先保留結果，離開原核心堆疊後才可回收。</p>
    <div class="controls" role="group" aria-label="Thread 步驟控制">
      <button :disabled="index === 0" @click="select(index - 1)">上一步</button>
      <button :aria-pressed="playing" @click="toggle">{{ playing ? '暫停' : '播放' }}</button>
      <button :disabled="index === sequence.frames.length - 1" @click="select(index + 1)">下一步</button>
      <button @click="select(0)">重設</button>
      <span>步驟 {{ index + 1 }} / {{ sequence.frames.length }}</span>
    </div>
    <div class="explanation">
      <div v-for="reading in readingFrames" :key="`${reading.key}-${reading.position}`" class="explanation-frame" :class="{ 'is-current': reading.key === scenario && reading.position === index }" :aria-hidden="reading.key !== scenario || reading.position !== index" :inert="reading.key !== scenario || reading.position !== index">
        <p class="step-label">{{ reading.label }} · A：{{ reading.state }}</p>
        <h3>{{ reading.title }}</h3>
        <p>{{ reading.why }}</p>
        <dl><dt>關鍵欄位</dt><dd><code>{{ reading.field }}</code></dd></dl>
        <div class="source-links"><a :href="'https://github.com/johnson931128/WUWAOS/blob/main/' + reading.source">原始碼：{{ reading.source }}</a><a :href="withBase('/SPEC-34-threads.html')">Spec 34 · 狀態轉移與生命週期</a></div>
      </div>
    </div>
    <p class="model-note" role="status">{{ sequence.label }}，步驟 {{ index + 1 }}／{{ sequence.frames.length }}：{{ frame.title }}。</p>
    <p class="model-note">播放時每步停留 6.5 秒，可隨時暫停。</p>
  </section>
</template>

<style scoped>
.thread-flow { --flow-active: #eaf1fb; --flow-ink: #244e85; container-type: inline-size; margin: 28px 0 40px; padding: 24px; border: 1px solid var(--vp-c-divider); border-radius: 18px; background: var(--vp-c-bg-soft); }
.dark .thread-flow { --flow-active: #23364e; --flow-ink: #bdd8ff; }
.scenario-picker, .controls { display: flex; flex-wrap: wrap; gap: 10px; align-items: center; }
button { min-height: 44px; padding: 9px 14px; border: 1px solid var(--vp-c-divider); border-radius: 9px; background: var(--vp-c-bg); color: var(--vp-c-text-1); font: inherit; font-size: 16px; cursor: pointer; transition: background-color 150ms, border-color 150ms; }
button:hover:not(:disabled) { border-color: var(--flow-ink); }
button[aria-pressed="true"] { background: var(--flow-active); border-color: var(--flow-ink); font-weight: 600; }
button:disabled { opacity: .5; cursor: default; }
button:focus-visible, a:focus-visible { outline: 2px solid var(--vp-c-brand-1); outline-offset: 4px; }
.thread-flow p { font-size: 16px; line-height: 1.8; }
.scenario-intro { min-height: 58px; }
.thread-flow .model-note, .thread-flow .lifecycle-note { font-size: 14px; line-height: 1.7; color: var(--vp-c-text-2); }
.system-map { position: relative; display: grid; gap: 12px; margin: 24px 0; }
.connections { display: none; }
.map-node { position: relative; min-width: 0; min-height: 210px; padding: 20px 12px; border: 1px solid var(--vp-c-divider); border-radius: 12px; background: var(--vp-c-bg); display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 8px; text-align: center; }
.map-node.active { background: var(--flow-active); border-color: var(--flow-ink); }
.map-node svg { fill: none; stroke: var(--flow-ink); stroke-width: 1.5; stroke-linecap: round; stroke-linejoin: round; }
.thread-flow .map-node h3 { margin: 0; font-size: 18px; line-height: 1.5; }
.thread-flow .map-node p { margin: 0; font-size: 14px; }
.map-node span, .map-node strong { font-size: 14px; line-height: 1.7; }
.map-node .cpu-value { font-size: 18px; }
.thread-flow .map-node ol { display: flex; flex-wrap: wrap; justify-content: center; gap: 6px; margin: 0; padding: 0; list-style: none; min-height: 34px; }
.map-node li { margin: 0; padding: 3px 10px; border: 1px solid var(--vp-c-divider); border-radius: 6px; font-size: 16px; background: var(--vp-c-bg); }
.empty { min-height: 34px; }
.thread-flow .return-label, .thread-flow .wake-label, .thread-flow .mobile-connection { margin: 0; font-size: 14px; line-height: 1.7; text-align: center; color: var(--vp-c-text-2); }
.state-strip { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
.state-caption { width: 100%; font-size: 14px; font-weight: 600; }
.state-tag { padding: 5px 8px; font-size: 14px; line-height: 1.6; border-radius: 6px; border: 1px solid var(--vp-c-divider); }
.state-tag.chosen { color: var(--flow-ink); background: var(--flow-active); border-color: var(--flow-ink); font-weight: 600; }
.controls { margin: 24px 0; }
.controls span { font-size: 14px; font-variant-numeric: tabular-nums; }
.explanation { display: grid; min-height: 330px; padding: 24px; border-radius: 12px; background: var(--vp-c-bg); }
.explanation-frame { grid-area: 1 / 1; min-width: 0; visibility: hidden; }
.explanation-frame.is-current { visibility: visible; }
.thread-flow .step-label { margin: 0; font-size: 14px; color: var(--vp-c-text-2); }
.thread-flow .explanation h3 { margin: 8px 0 16px; font-size: 22px; line-height: 1.5; }
dl { margin: 20px 0; } dt { font-size: 14px; font-weight: 600; } dd { margin: 6px 0 0; overflow-wrap: anywhere; } code { font-size: 14px; white-space: normal; }
.source-links { display: flex; flex-direction: column; align-items: flex-start; gap: 10px; font-size: 14px; line-height: 1.7; overflow-wrap: anywhere; }
@container (min-width: 560px) {
  .system-map { grid-template-columns: 28% 28% 28%; justify-content: space-between; gap: 0; padding: 95px 2%; }
  .connections { display: block; position: absolute; inset: 0; width: 100%; height: 100%; fill: none; stroke: var(--vp-c-text-2); stroke-width: 1.5; }
  .connections .selected { stroke: var(--flow-ink); stroke-width: 3; }
  .mobile-connection { display: none; }
  .thread-flow .return-label { position: absolute; top: 0; left: 0; right: 0; }
  .thread-flow .wake-label { position: absolute; bottom: 0; left: 0; right: 0; }
}
@media (max-width: 480px) { .thread-flow { padding: 16px; } .explanation { padding: 16px; } }
@media (prefers-reduced-motion: reduce) { button { transition: none; } }
</style>
