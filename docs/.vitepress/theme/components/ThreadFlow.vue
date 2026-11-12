<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from 'vue'
import { withBase } from 'vitepress'

type State = 'NEW' | 'READY' | 'RUNNING' | 'WAITING' | 'ZOMBIE' | 'UNUSED'
type Frame = { title: string; why: string; state: State; queue: string[]; cpu: string; waiting: string[]; edge: string; field: string; source: string }
const scenarios = {
  rr: { label: '時間片輪轉', intro: 'A、B、C 都可執行。每條 Thread 各自參與 FIFO 輪轉，不依 Process 平分時間片。', frames: [
    { title: 'A 正在使用 hart 0', why: 'A 已從隊首出隊並取得 50 ms 時間片。B、C 保持 Ready，等待之後派發。', state: 'RUNNING', queue: ['B', 'C'], cpu: 'A', waiting: [], edge: '', field: 'current = A；Ready head = B', source: 'kernel/proc/process.c' },
    { title: 'A 的時間片到期，排回隊尾', why: 'U-mode timer trap 清除 pending，thread_preempt 增加搶占次數並將 A 改成 Ready；保存核心上下文後切回 scheduler。此畫面停在派發下一條之前。', state: 'READY', queue: ['B', 'C', 'A'], cpu: '', waiting: [], edge: 'preempt', field: 'A.preemptions++；A.state = THREAD_READY', source: 'arch/riscv/trap.c' },
    { title: '隊首 B 取得 CPU', why: 'scheduler_next 取出 B，scheduler 設定 B 的頁表與新時間片，恢復 B 自己的核心上下文。喚醒或排隊本身不會跳過這一步。', state: 'READY', queue: ['C', 'A'], cpu: 'B', waiting: [], edge: 'dispatch', field: 'B.state = THREAD_RUNNING；quantum = 50 ms', source: 'kernel/proc/scheduler.c' },
    { title: 'B 到期，也排回隊尾', why: 'B 在 U-mode 用完時間片，依相同規則回到隊尾。A 仍在等候，下一個輪到 C。50 ms 不是核心內強制搶占的硬上限。', state: 'READY', queue: ['C', 'A', 'B'], cpu: '', waiting: [], edge: 'preempt', field: 'Ready queue = C → A → B', source: 'kernel/proc/process.c' },
    { title: 'C 執行，A 留在下一個位置', why: 'C 出隊後開始執行，A、B 仍依序等待。若只有一條 Ready Thread，排程器也可能再次選到剛用完時間片的同一條。', state: 'READY', queue: ['A', 'B'], cpu: 'C', waiting: [], edge: 'dispatch', field: 'current = C；Ready queue = A → B', source: 'kernel/proc/process.c' },
  ] as Frame[] },
  io: { label: 'I/O 等待', intro: 'A 呼叫 read 後等待事件，B 可繼續執行；事件完成只讓 A Ready，不會立即搶走 CPU。', frames: [
    { title: 'A 準備讀取資料', why: 'A 在 U-mode 呼叫 read，進入自己的核心 stack。此範例展示需要等待裝置完成的路徑，不代表每次 read 都必定阻塞。', state: 'RUNNING', queue: ['B'], cpu: 'A', waiting: [], edge: '', field: 'A.kernel_call_active = 1', source: 'kernel/io/io.c' },
    { title: '登記等待，A 讓出 CPU', why: '確認事件來源的 generation／sequence，登記等待後將 A 改成 Waiting。A 的核心呼叫堆疊仍保留，排程器不會替 A 完成讀取。', state: 'WAITING', queue: ['B'], cpu: '', waiting: ['A'], edge: 'block', field: 'A.wait_reason = WAIT_EVENT；event_wait.active = 1', source: 'kernel/proc/process.c' },
    { title: 'A 等待期間，B 繼續前進', why: 'B 從 Ready queue 出隊。A 不在 Ready queue，因此不會被反覆派發空轉；同一時刻仍只有 hart 0 上的一條 Thread 執行。', state: 'WAITING', queue: [], cpu: 'B', waiting: ['A'], edge: 'dispatch', field: 'current = B；A.state = THREAD_WAITING', source: 'kernel/proc/scheduler.c' },
    { title: '事件完成，A 回到隊尾', why: '裝置完成可由 IRQ 或 poll 路徑通知。A 的等待結果被記錄，狀態變成 Ready。此教學步驟停在通知後，B 仍是目前 Thread。', state: 'READY', queue: ['A'], cpu: 'B', waiting: [], edge: 'wake', field: 'A.event_wait.completed = 1；A.state = THREAD_READY', source: 'kernel/proc/process.c' },
    { title: 'B 讓出後，才輪到 A', why: '此例假設 B 以 yield 主動讓出 CPU，排在 A 後面；不是事件通知直接切換 Thread。取消或逾時也會先喚醒，再由原 stack 處理結果。', state: 'READY', queue: ['A', 'B'], cpu: '', waiting: [], edge: 'preempt', field: 'Ready queue = A → B', source: 'kernel/proc/process.c' },
    { title: '恢復 A 原本的 read 呼叫', why: 'A 再次獲選，恢復 KernelContext 與原核心 stack，從等待函式返回、處理完成結果，再還原 TrapFrame 回到使用者。整個程式沒有重新啟動。', state: 'RUNNING', queue: ['B'], cpu: 'A', waiting: [], edge: 'dispatch', field: 'A.wait_reason = WAIT_NONE；原 kernel stack 繼續', source: 'arch/riscv/swtch.S' },
  ] as Frame[] },
  exit: { label: '退出與回收', intro: 'A、B 屬於同一 Process。A 單獨 thread_exit，B 仍可執行，再以 join 取得結果並回收 A。', frames: [
    { title: '同一 Process，兩條獨立 Thread', why: 'A、B 共用位址空間與 FD table，各自擁有 TCB、使用者 stack 與核心 stack。圖中的 A 是使用額外 stack slot 的 Thread。', state: 'RUNNING', queue: ['B'], cpu: 'A', waiting: [], edge: '', field: 'A.process = B.process；A.tid ≠ B.tid', source: 'kernel/proc/thread.h' },
    { title: 'A 結束，先保留 Zombie', why: 'A 的函式 return 經 trampoline 呼叫 thread_exit。釋放自己的同步／raw owner 後記錄結果，切到 scheduler；Zombie 仍占 TCB slot，不能在原 stack 上回收自己。', state: 'ZOMBIE', queue: ['B'], cpu: '', waiting: [], edge: '', field: 'A.exit_status = 7；A.state = THREAD_ZOMBIE', source: 'kernel/proc/process.c' },
    { title: 'B 繼續使用共享資源', why: 'A 的 thread_exit 不會關閉整份 FD table 或銷毀 Process 頁表。B 仍存在，所以 Process 保持 Live。既有 exit(status) 則會要求整個 Process 結束。', state: 'ZOMBIE', queue: [], cpu: 'B', waiting: [], edge: 'dispatch', field: 'Process.state = PROCESS_LIVE', source: 'kernel/proc/process.c' },
    { title: 'B 取得 A 的 join claim', why: '同一 Process 的 B 呼叫 thread_join(A)。本例 A 已為 Zombie，不需等待其退出；status 成功 copyout、額外 stack 回收完成後，才能消耗 Zombie。', state: 'ZOMBIE', queue: [], cpu: 'B', waiting: [], edge: '', field: 'A.join_claimed = 1；A.joiner_tid = B.tid', source: 'kernel/proc/process.c' },
    { title: 'A 的 slot 釋放，B 繼續執行', why: '成功 join 回傳 A 的 TID 與結果 7，釋放 A 的額外 user stack／swap 資源及 TCB slot。整個 Process 的退出與父行程 wait 是另一層生命週期。', state: 'UNUSED', queue: [], cpu: 'B', waiting: [], edge: '', field: 'A 的 TCB slot = THREAD_UNUSED；join status = 7', source: 'kernel/proc/thread.c' },
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
    <p class="model-note">A／B／C 是教學代號，非即時 TID。步驟不是實際耗時，亦非 QEMU Trace。</p>
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
          <h3>Ready queue</h3><p>隊首 → 隊尾</p>
          <ol aria-label="Ready queue 順序"><li v-for="name in frame.queue" :key="name">{{ name }}</li></ol>
          <span v-if="!frame.queue.length" class="empty">目前沒有 Ready Thread</span>
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
          <h3>Waiting</h3><p>等待條件完成</p>
          <strong>{{ frame.waiting.length ? `Thread ${frame.waiting.join('、')}` : '目前沒有等待者' }}</strong>
          <span>保留原核心 stack</span>
        </div>
        <p class="wake-label">事件完成／逾時／取消：Waiting → Ready 隊尾</p>
      </div>
    </div>
    <div class="state-strip" aria-label="目前追蹤 A 的 TCB 狀態">
      <span class="state-caption">A 的狀態</span>
      <span v-for="state in states" :key="state" class="state-tag" :class="{ chosen: frame.state === state }" :aria-current="frame.state === state ? 'true' : undefined">{{ frame.state === state ? '● ' : '' }}{{ state }}</span>
    </div>
    <p class="lifecycle-note">建立：Unused → New → Ready。退出：Running → Zombie → 回收為 Unused；必須先離開原核心 stack。</p>
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
    <p class="model-note">預設不播放；播放每步停留 6.5 秒，可隨時暫停。核心內不強制搶占，50 ms 不是回應時間上限。</p>
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
