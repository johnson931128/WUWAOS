<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'

type Descriptor = { fd: number; target: string; label?: string }
type ProcessView = { name: string; descriptors: Descriptor[] }
type OpenDescription = { id: string; kind: string; file: string; refs: number; offset: number; alive: boolean }
type Frame = {
  title: string
  note: string
  actor: string
  processes: ProcessView[]
  descriptions: OpenDescription[]
  touched?: string
}
type Scenario = { id: string; label: string; frames: Frame[] }

const sharedFrames: Frame[] = [
  { title: 'open 建立第一個 OFD', note: 'PID 12 以 fd 3 指向 OFD A。這次 open 建立新的 open-file description，初始 offset 為 0、引用數為 1。', actor: 'PID 12 · open("/home/user/story.txt", O_RDONLY)', processes: [{ name: 'PID 12', descriptors: [{ fd: 0, target: 'UART IN', label: 'stdin' }, { fd: 1, target: 'UART OUT', label: 'stdout' }, { fd: 2, target: 'UART ERR', label: 'stderr' }, { fd: 3, target: 'A', label: 'story.txt' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 1, offset: 0, alive: true }] },
  { title: 'read 取得 AB', note: 'read(fd=3, …, 2) 從 OFD A 的 offset 0 讀取兩個 byte。成功複製到使用者 buffer 後，offset 前進到 2。', actor: 'PID 12 · read(fd 3, 2 bytes)', processes: [{ name: 'PID 12', descriptors: [{ fd: 0, target: 'UART IN', label: 'stdin' }, { fd: 1, target: 'UART OUT', label: 'stdout' }, { fd: 2, target: 'UART ERR', label: 'stderr' }, { fd: 3, target: 'A', label: 'story.txt' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 1, offset: 2, alive: true }], touched: 'A' },
  { title: 'dup 建立 fd 4', note: 'dup(3) 找到最小空 FD 4，讓 fd 4 也指向 OFD A。引用數按 FD 引用計算，因此從 1 變成 2；沒有建立第二份 offset。', actor: 'PID 12 · dup(fd 3) → fd 4', processes: [{ name: 'PID 12', descriptors: [{ fd: 0, target: 'UART IN', label: 'stdin' }, { fd: 1, target: 'UART OUT', label: 'stdout' }, { fd: 2, target: 'UART ERR', label: 'stderr' }, { fd: 3, target: 'A', label: 'story.txt' }, { fd: 4, target: 'A', label: 'dup of fd 3' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 2, offset: 2, alive: true }] },
  { title: '從 fd 3 讀取 C', note: 'fd 3 與 fd 4 共用 OFD A。這次讀取由 offset 2 開始，得到 C，然後把共同 offset 推進到 3。', actor: 'PID 12 · read(fd 3, 1 byte)', processes: [{ name: 'PID 12', descriptors: [{ fd: 0, target: 'UART IN', label: 'stdin' }, { fd: 1, target: 'UART OUT', label: 'stdout' }, { fd: 2, target: 'UART ERR', label: 'stderr' }, { fd: 3, target: 'A', label: 'story.txt' }, { fd: 4, target: 'A', label: 'dup of fd 3' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 2, offset: 3, alive: true }], touched: 'A' },
  { title: '從 fd 4 讀取 D', note: '改用 fd 4 仍會從同一個 offset 3 開始，因此讀到 D，並把 A 的 offset 更新為 4。', actor: 'PID 12 · read(fd 4, 1 byte)', processes: [{ name: 'PID 12', descriptors: [{ fd: 0, target: 'UART IN', label: 'stdin' }, { fd: 1, target: 'UART OUT', label: 'stdout' }, { fd: 2, target: 'UART ERR', label: 'stderr' }, { fd: 3, target: 'A', label: 'story.txt' }, { fd: 4, target: 'A', label: 'dup of fd 3' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 2, offset: 4, alive: true }], touched: 'A' },
  { title: 'fork 繼承兩個 FD', note: 'fork 複製 FD table 的每個有效項目。子行程的 fd 3、4 各自增加一個引用，所以 A refs 由 2 變 4；父子仍共用同一個 OFD A 和 offset 4。', actor: 'PID 12 · fork() → PID 13', processes: [{ name: 'PID 12 · parent', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A', label: 'story.txt' }, { fd: 4, target: 'A', label: 'dup of fd 3' }] }, { name: 'PID 13 · child', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A', label: 'inherited' }, { fd: 4, target: 'A', label: 'inherited' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 4, offset: 4, alive: true }] },
  { title: '子行程從 fd 4 讀取 E', note: '子行程讀取後，父行程也會看到共享 OFD A 的 offset 已到 5。fork 沒有複製 OFD，也沒有建立私有 offset。', actor: 'PID 13 · read(fd 4, 1 byte)', processes: [{ name: 'PID 12 · parent', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A' }, { fd: 4, target: 'A' }] }, { name: 'PID 13 · child', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A' }, { fd: 4, target: 'A' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 4, offset: 5, alive: true }], touched: 'A' },
  { title: '第二次 open 建立 OFD B', note: '同一個路徑再次 open 會配置另一個 OFD B。PID 12 的 fd 5 指向 B；B 的 offset 從 0 開始，與 A 分開。', actor: 'PID 12 · open("/home/user/story.txt", O_RDONLY) → fd 5', processes: [{ name: 'PID 12 · parent', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A' }, { fd: 4, target: 'A' }, { fd: 5, target: 'B', label: 'story.txt · second open' }] }, { name: 'PID 13 · child', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A' }, { fd: 4, target: 'A' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 4, offset: 5, alive: true }, { id: 'B', kind: 'regular file', file: 'story.txt', refs: 1, offset: 0, alive: true }] },
  { title: 'OFD B 獨立讀取 A', note: '從 fd 5 讀取一個 byte，得到 A。B 的 offset 變成 1；OFD A 保持在 5，兩條資料位置互不影響。', actor: 'PID 12 · read(fd 5, 1 byte)', processes: [{ name: 'PID 12 · parent', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A' }, { fd: 4, target: 'A' }, { fd: 5, target: 'B' }] }, { name: 'PID 13 · child', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A' }, { fd: 4, target: 'A' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 4, offset: 5, alive: true }, { id: 'B', kind: 'regular file', file: 'story.txt', refs: 1, offset: 1, alive: true }], touched: 'B' },
  { title: '父行程 close(fd 3)', note: 'close 只移除這一個 FD 引用。A refs 從 4 變 3，因父 fd 4 與子行程 fd 3、4 仍指向 A，所以 OFD 還在。', actor: 'PID 12 · close(fd 3)', processes: [{ name: 'PID 12 · parent', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 4, target: 'A' }, { fd: 5, target: 'B' }] }, { name: 'PID 13 · child', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A' }, { fd: 4, target: 'A' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 3, offset: 5, alive: true }, { id: 'B', kind: 'regular file', file: 'story.txt', refs: 1, offset: 1, alive: true }] },
  { title: '父行程 close(fd 4)', note: '父行程對 A 的最後一個本地 FD 關閉後，A refs 為 2；子行程的兩個繼承 FD 仍保留它。', actor: 'PID 12 · close(fd 4)', processes: [{ name: 'PID 12 · parent', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 5, target: 'B' }] }, { name: 'PID 13 · child', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 3, target: 'A' }, { fd: 4, target: 'A' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 2, offset: 5, alive: true }, { id: 'B', kind: 'regular file', file: 'story.txt', refs: 1, offset: 1, alive: true }] },
  { title: '子行程 close(fd 3)', note: '再釋放一個子行程 FD，A refs 降為 1。最後還有 child fd 4 指向 A。', actor: 'PID 13 · close(fd 3)', processes: [{ name: 'PID 12 · parent', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 5, target: 'B' }] }, { name: 'PID 13 · child', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 4, target: 'A' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 1, offset: 5, alive: true }, { id: 'B', kind: 'regular file', file: 'story.txt', refs: 1, offset: 1, alive: true }] },
  { title: '最後 close 回收 OFD A', note: 'child close(fd 4) 使 A refs 歸零，核心釋放該 OFD 槽位。B 仍由父 fd 5 持有，因此 B 不受影響。', actor: 'PID 13 · close(fd 4)', processes: [{ name: 'PID 12 · parent', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }, { fd: 5, target: 'B' }] }, { name: 'PID 13 · child', descriptors: [{ fd: 0, target: 'UART IN' }, { fd: 1, target: 'UART OUT' }, { fd: 2, target: 'UART ERR' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 0, offset: 5, alive: false }, { id: 'B', kind: 'regular file', file: 'story.txt', refs: 1, offset: 1, alive: true }] },
]

const separateFrames: Frame[] = [
  { title: '第一個 open', note: 'fd 3 指向 OFD A，offset 從 0 開始。', actor: 'PID 20 · open(story.txt) → fd 3', processes: [{ name: 'PID 20', descriptors: [{ fd: 3, target: 'A', label: 'first open' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 1, offset: 0, alive: true }] },
  { title: '第二個 open', note: '同一檔案路徑再次 open，fd 4 指向新 OFD B；它有自己的 offset 0。', actor: 'PID 20 · open(story.txt) → fd 4', processes: [{ name: 'PID 20', descriptors: [{ fd: 3, target: 'A', label: 'first open' }, { fd: 4, target: 'B', label: 'second open' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 1, offset: 0, alive: true }, { id: 'B', kind: 'regular file', file: 'story.txt', refs: 1, offset: 0, alive: true }] },
  { title: '各自讀取', note: 'A 讀 AB 後 offset=2；B 讀 A 後 offset=1。它們定位同一個檔案資料，但保存兩個獨立的目前位置。', actor: 'PID 20 · read(fd 3, 2)；read(fd 4, 1)', processes: [{ name: 'PID 20', descriptors: [{ fd: 3, target: 'A', label: 'first open' }, { fd: 4, target: 'B', label: 'second open' }] }], descriptions: [{ id: 'A', kind: 'regular file', file: 'story.txt', refs: 1, offset: 2, alive: true }, { id: 'B', kind: 'regular file', file: 'story.txt', refs: 1, offset: 1, alive: true }], touched: 'A' },
]

const guiFrames: Frame[] = [
  { title: 'UART 標準 FD 使用 sentinel', note: 'UART 初始 FD 0、1、2 分別存負數 sentinel，代表固定輸入／輸出路由；它們不配置全域 OFD，也不占 16 個 OFD 槽位。', actor: 'PID 1 · fd_table 初始化', processes: [{ name: 'PID 1 · UART', descriptors: [{ fd: 0, target: 'UART IN', label: 'sentinel -1' }, { fd: 1, target: 'UART OUT', label: 'sentinel -2' }, { fd: 2, target: 'UART ERR', label: 'sentinel -3' }] }], descriptions: [] },
  { title: 'GUI Terminal 使用兩個 OFD', note: 'GUI Terminal 的 fd 0 指向可讀 input OFD；fd 1、2 共用可寫 output OFD。因此總共兩個 OFD，但 output 的 refs=2，因引用數是 FD 數。', actor: 'PID 21 · GUI TTY attach', processes: [{ name: 'PID 21 · GUI', descriptors: [{ fd: 0, target: 'IN', label: 'readable' }, { fd: 1, target: 'OUT', label: 'writable' }, { fd: 2, target: 'OUT', label: 'writable · shared' }] }], descriptions: [{ id: 'IN', kind: 'GUI TTY input', file: 'terminal endpoint', refs: 1, offset: 0, alive: true }, { id: 'OUT', kind: 'GUI TTY output', file: 'terminal endpoint', refs: 2, offset: 0, alive: true }] },
]

const scenarios: Scenario[] = [
  { id: 'shared', label: 'open、dup、fork 與 close', frames: sharedFrames },
  { id: 'separate', label: '兩次 open，位置分開', frames: separateFrames },
  { id: 'gui', label: 'UART 與 GUI Terminal', frames: guiFrames },
]
const selectedId = ref(scenarios[0].id)
const index = ref(0)
const playing = ref(false)
let timer: ReturnType<typeof setInterval> | undefined
const selected = computed(() => scenarios.find((scenario) => scenario.id === selectedId.value) ?? scenarios[0])
const frame = computed(() => selected.value.frames[index.value])
const isFirst = computed(() => index.value === 0)
const isLast = computed(() => index.value === selected.value.frames.length - 1)

function stop() {
  playing.value = false
  if (timer !== undefined) clearInterval(timer)
  timer = undefined
}
function goTo(nextIndex: number) {
  index.value = Math.max(0, Math.min(selected.value.frames.length - 1, nextIndex))
  if (isLast.value) stop()
}
function previous() { stop(); goTo(index.value - 1) }
function next() { stop(); goTo(index.value + 1) }
function reset() { stop(); index.value = 0 }
function chooseScenario(id: string) {
  stop()
  selectedId.value = id
  index.value = 0
}
function togglePlayback() {
  if (playing.value) { stop(); return }
  if (isLast.value) index.value = 0
  playing.value = true
  timer = setInterval(() => {
    if (index.value >= selected.value.frames.length - 1) stop()
    else goTo(index.value + 1)
  }, 1400)
}
watch(selectedId, stop)
onBeforeUnmount(stop)
</script>

<template>
  <section class="fd-ofd-lab" aria-labelledby="fd-ofd-lab-title">
    <div class="lab-heading">
      <div>
        <p class="eyebrow">互動示意 · 固定教學序列</p>
        <h2 id="fd-ofd-lab-title">FD 到 OFD：誰保存目前位置？</h2>
        <p class="lab-intro">每一步直接呈現 FD table、全域 open-file descriptions 與檔案 byte 位置。這是依目前 source 建立的教學模型，不是 QEMU trace。</p>
      </div>
      <div class="step-counter" aria-label="目前步驟">{{ index + 1 }} <span>/ {{ selected.frames.length }}</span></div>
    </div>

    <div class="scenario-picker" role="group" aria-label="選擇 FD/OFD 示意情境">
      <button v-for="scenario in scenarios" :key="scenario.id" type="button" :aria-pressed="selectedId === scenario.id" @click="chooseScenario(scenario.id)">{{ scenario.label }}</button>
    </div>

    <div class="controls" role="group" aria-label="互動播放控制">
      <button type="button" :disabled="isFirst" aria-label="上一步" @click="previous">← 上一步</button>
      <button type="button" :aria-label="playing ? '暫停播放' : '播放步驟'" @click="togglePlayback">{{ playing ? 'Ⅱ 暫停' : '▶ 播放' }}</button>
      <button type="button" :disabled="isLast" aria-label="下一步" @click="next">下一步 →</button>
      <button type="button" @click="reset">重設</button>
    </div>

    <div class="progress-track" role="progressbar" :aria-valuenow="index + 1" :aria-valuemin="1" :aria-valuemax="selected.frames.length" :aria-label="`教學步驟 ${index + 1} / ${selected.frames.length}`">
      <span :style="{ width: `${((index + 1) / selected.frames.length) * 100}%` }" />
    </div>

    <div class="frame-caption" aria-live="polite" aria-atomic="true">
      <div><span class="step-tag">STEP {{ String(index + 1).padStart(2, '0') }}</span><h3>{{ frame.title }}</h3></div>
      <p>{{ frame.note }}</p>
      <code>{{ frame.actor }}</code>
    </div>

    <div class="state-grid">
      <div class="panel process-panel">
        <div class="panel-title"><span class="panel-index">01</span><div><h3>各行程的 FD table</h3><p>fd 是行程本地的索引</p></div></div>
        <div class="process-list">
          <article v-for="process in frame.processes" :key="process.name" class="process-card">
            <h4>{{ process.name }}</h4>
            <div class="fd-list">
              <div v-for="descriptor in process.descriptors" :key="descriptor.fd" class="fd-row" :class="{ 'is-sentinel': descriptor.target.startsWith('UART') }">
                <span class="fd-number">fd {{ descriptor.fd }}</span><span class="fd-arrow">→</span><span class="fd-target">{{ descriptor.target }}</span><small v-if="descriptor.label">{{ descriptor.label }}</small>
              </div>
              <p v-if="process.descriptors.length === 0" class="empty-state">目前沒有指向 OFD 的 FD</p>
            </div>
          </article>
        </div>
        <p class="panel-footnote">每個 Process 最多 8 個 FD 槽位，含空槽；空槽可由 open／dup／pipe 重用。</p>
      </div>

      <div class="panel ofd-panel">
        <div class="panel-title"><span class="panel-index">02</span><div><h3>核心全域 OFD 表</h3><p>refs 按 FD 計算；一般檔案卡顯示 offset</p></div></div>
        <div v-if="frame.descriptions.length" class="ofd-list">
          <article v-for="ofd in frame.descriptions" :key="ofd.id" class="ofd-card" :class="{ 'is-touched': frame.touched === ofd.id, 'is-released': !ofd.alive }">
            <div class="ofd-card-heading"><span class="ofd-id">OFD {{ ofd.id }}</span><span class="object-kind">{{ ofd.kind }}</span></div>
            <div class="ofd-meta"><span>refs</span><strong>{{ ofd.refs }}</strong><span>offset</span><strong>{{ ofd.kind === 'regular file' ? ofd.offset : '—' }}</strong></div>
            <p>{{ ofd.alive ? `指向 ${ofd.file}` : 'refs = 0 · 槽位已回收' }}</p>
          </article>
        </div>
        <p v-else class="empty-ofd">此情境中的 UART 使用 sentinel，沒有配置 OFD。</p>
        <p class="panel-footnote">全域最多 16 個 OFD。refs 計算所有行程 FD table 中指向該 OFD 的項目。</p>
      </div>
    </div>

    <div v-if="frame.descriptions.some((ofd) => ofd.kind === 'regular file')" class="file-panel" aria-label="OFD offset 對應的檔案資料位置">
      <div class="file-panel-heading"><div><span class="panel-index">03</span><h3>同一檔案的資料位置</h3></div><p>標記顯示下一次 read 將開始的位置</p></div>
      <div class="file-row" aria-label="story.txt 的 bytes">
        <div v-for="(byte, position) in 'ABCDEFGHIJ'.split('')" :key="position" class="byte-cell" :class="{ consumed: frame.descriptions.some((ofd) => ofd.file === 'story.txt' && ofd.alive && ofd.offset > position), 'cursor-a': frame.descriptions.some((ofd) => ofd.id === 'A' && ofd.alive && ofd.offset === position), 'cursor-b': frame.descriptions.some((ofd) => ofd.id === 'B' && ofd.alive && ofd.offset === position) }">
          <span class="byte-position">{{ position }}</span><strong>{{ byte }}</strong>
          <small class="cursor-labels"><span v-if="frame.descriptions.some((ofd) => ofd.id === 'A' && ofd.alive && ofd.offset === position)">A offset</span><span v-if="frame.descriptions.some((ofd) => ofd.id === 'B' && ofd.alive && ofd.offset === position)">B offset</span></small>
        </div>
      </div>
      <p class="file-explainer"><span class="legend-dot a-dot" /> A 的下一個位置 <span class="legend-dot b-dot" /> B 的下一個位置。兩張 OFD 卡若指向同一檔案，資料相同，offset 仍各自保存。</p>
    </div>

    <p class="sr-status" aria-live="polite">{{ selected.label }}，{{ index + 1 }}／{{ selected.frames.length }}：{{ frame.title }}。{{ playing ? '正在播放' : '播放已停止' }}</p>
  </section>
</template>

<style scoped>
.fd-ofd-lab {
  --lab-border: var(--vp-c-divider, #d9dee8);
  --lab-muted: var(--vp-c-text-2, #586174);
  --lab-soft: var(--vp-c-bg-soft, #f5f7fa);
  --lab-card: var(--vp-c-bg, #fff);
  --lab-accent: var(--os-process, #8b3ca5);
  --lab-green: #267258;
  margin: 2rem 0 2.5rem;
  padding: clamp(1rem, 3vw, 1.75rem);
  border: 1px solid var(--lab-border);
  border-radius: 1.1rem;
  background: var(--lab-soft);
  color: var(--vp-c-text-1, #20242c);
}
.lab-heading, .panel-title, .file-panel-heading, .ofd-card-heading, .ofd-meta { display: flex; align-items: center; justify-content: space-between; gap: .85rem; }
.lab-heading { align-items: flex-start; }
.eyebrow { margin: 0 0 .35rem; color: var(--lab-accent); font-size: .76rem; font-weight: 700; letter-spacing: .08em; }
.lab-heading h2 { margin: 0; font-size: clamp(1.3rem, 3vw, 1.75rem); line-height: 1.25; }
.lab-intro { max-width: 66ch; margin: .55rem 0 0; color: var(--lab-muted); font-size: .92rem; }
.step-counter { flex: 0 0 auto; color: var(--lab-accent); font-size: 1.35rem; font-variant-numeric: tabular-nums; font-weight: 750; }
.step-counter span { color: var(--lab-muted); font-size: .95rem; font-weight: 500; }
.scenario-picker, .controls { display: flex; flex-wrap: wrap; gap: .55rem; margin-top: 1rem; }
button { min-height: 2.55rem; padding: .55rem .85rem; border: 1px solid var(--lab-border); border-radius: .65rem; background: var(--lab-card); color: inherit; font: inherit; font-size: .86rem; cursor: pointer; transition: border-color .16s ease, background-color .16s ease, transform .16s ease; }
button:hover:not(:disabled) { border-color: var(--lab-accent); transform: translateY(-1px); }
button:focus-visible { outline: 3px solid color-mix(in srgb, var(--lab-accent) 42%, transparent); outline-offset: 2px; }
.scenario-picker button[aria-pressed="true"], .controls button:nth-child(2) { border-color: var(--lab-accent); background: var(--lab-accent); color: var(--os-accent-ink, #fff); }
button:disabled { cursor: not-allowed; opacity: .48; }
.progress-track { height: .35rem; margin-top: .95rem; overflow: hidden; border-radius: 1rem; background: var(--lab-border); }
.progress-track span { display: block; height: 100%; border-radius: inherit; background: var(--lab-accent); transition: width .22s ease; }
.frame-caption { display: grid; grid-template-columns: minmax(12rem, .8fr) minmax(16rem, 1.4fr); gap: .5rem 1.25rem; padding: 1.1rem 0 1rem; }
.frame-caption > div { display: flex; align-items: center; gap: .65rem; }
.step-tag, .panel-index { color: var(--lab-accent); font-size: .72rem; font-weight: 750; letter-spacing: .06em; }
.frame-caption h3, .panel-title h3, .file-panel-heading h3 { margin: 0; font-size: 1.03rem; }
.frame-caption > p { margin: 0; color: var(--lab-muted); font-size: .9rem; }
.frame-caption code { grid-column: 1 / -1; width: fit-content; max-width: 100%; padding: .35rem .55rem; overflow-wrap: anywhere; border-radius: .4rem; background: var(--lab-card); color: var(--lab-accent); font-size: .78rem; }
.state-grid { display: grid; grid-template-columns: minmax(0, 1.1fr) minmax(15rem, .9fr); gap: .85rem; }
.panel, .file-panel { min-width: 0; padding: 1rem; border: 1px solid var(--lab-border); border-radius: .85rem; background: var(--lab-card); }
.panel-title { justify-content: flex-start; align-items: flex-start; margin-bottom: .8rem; }
.panel-title > div { min-width: 0; }
.panel-title p, .file-panel-heading > p { margin: .16rem 0 0; color: var(--lab-muted); font-size: .76rem; }
.process-list { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 15rem), 1fr)); gap: .65rem; }
.process-card { min-width: 0; padding: .7rem; border: 1px solid var(--lab-border); border-radius: .65rem; background: var(--lab-soft); }
.process-card h4 { margin: 0 0 .5rem; font-size: .83rem; }
.fd-list { display: grid; gap: .3rem; }
.fd-row { display: grid; grid-template-columns: 3.2rem .8rem minmax(3rem, 1fr); align-items: center; gap: .25rem; min-width: 0; padding: .33rem .4rem; border-radius: .4rem; background: var(--lab-card); font-size: .75rem; }
.fd-number { color: var(--lab-muted); font-variant-numeric: tabular-nums; }
.fd-arrow { color: var(--lab-muted); text-align: center; }
.fd-target { width: fit-content; padding: .08rem .3rem; border-radius: .3rem; background: color-mix(in srgb, var(--lab-accent) 12%, var(--lab-card)); color: var(--lab-accent); font-weight: 750; }
.fd-row small { grid-column: 1 / -1; color: var(--lab-muted); font-size: .68rem; }
.fd-row.is-sentinel .fd-target { background: color-mix(in srgb, var(--lab-green) 13%, var(--lab-card)); color: var(--lab-green); }
.panel-footnote { margin: .7rem 0 0; color: var(--lab-muted); font-size: .74rem; line-height: 1.5; }
.ofd-list { display: grid; gap: .55rem; }
.ofd-card { padding: .7rem; border: 1px solid var(--lab-border); border-left: .25rem solid var(--lab-accent); border-radius: .6rem; background: var(--lab-card); transition: border-color .16s ease, background-color .16s ease; }
.ofd-card.is-touched { background: color-mix(in srgb, var(--lab-accent) 8%, var(--lab-card)); }
.ofd-card.is-released { border-left-color: var(--lab-muted); opacity: .7; }
.ofd-id { color: var(--lab-accent); font-size: .86rem; font-weight: 800; }
.object-kind { color: var(--lab-muted); font-size: .7rem; text-align: right; }
.ofd-meta { justify-content: flex-start; gap: .4rem; margin-top: .55rem; color: var(--lab-muted); font-size: .75rem; }
.ofd-meta strong { min-width: 1.5rem; margin-right: .45rem; color: var(--vp-c-text-1, #20242c); font-size: .95rem; font-variant-numeric: tabular-nums; }
.ofd-card p { margin: .4rem 0 0; color: var(--lab-muted); font-size: .73rem; overflow-wrap: anywhere; }
.empty-ofd, .empty-state { color: var(--lab-muted); font-size: .8rem; }
.empty-ofd { padding: .8rem; border: 1px dashed var(--lab-border); border-radius: .6rem; }
.file-panel { margin-top: .85rem; }
.file-panel-heading { align-items: flex-start; }
.file-panel-heading > div { display: flex; align-items: center; gap: .6rem; }
.file-row { display: grid; grid-template-columns: repeat(10, minmax(0, 1fr)); gap: .35rem; margin-top: .8rem; }
.byte-cell { position: relative; display: flex; min-width: 0; min-height: 4rem; flex-direction: column; align-items: center; justify-content: center; border: 1px solid var(--lab-border); border-radius: .5rem; background: var(--lab-soft); transition: background-color .16s ease, border-color .16s ease; }
.byte-cell.consumed { background: color-mix(in srgb, var(--lab-accent) 8%, var(--lab-card)); }
.byte-cell.cursor-a { border: 2px solid var(--lab-accent); }
.byte-cell.cursor-b { box-shadow: inset 0 0 0 2px var(--lab-green); }
.byte-position { color: var(--lab-muted); font-size: .62rem; font-variant-numeric: tabular-nums; }
.byte-cell strong { font-size: 1rem; }
.byte-cell .cursor-labels { position: absolute; bottom: .12rem; display: flex; max-width: 100%; gap: .1rem; color: var(--lab-accent); font-size: .52rem; white-space: nowrap; }
.file-explainer { display: flex; flex-wrap: wrap; align-items: center; gap: .35rem; margin: .7rem 0 0; color: var(--lab-muted); font-size: .72rem; }
.legend-dot { width: .65rem; height: .65rem; margin-left: .35rem; border-radius: 50%; }
.a-dot { background: var(--lab-accent); }
.b-dot { border: 2px solid var(--lab-green); }
.sr-status { position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px; overflow: hidden; clip: rect(0, 0, 0, 0); white-space: nowrap; border: 0; }
@media (max-width: 700px) {
  .state-grid { grid-template-columns: 1fr; }
  .frame-caption { grid-template-columns: 1fr; gap: .55rem; }
  .frame-caption code { grid-column: 1; }
  .file-panel-heading { display: block; }
  .file-panel-heading > p { margin-left: 1.65rem; }
  .file-row { gap: .18rem; }
  .byte-cell { min-height: 3.45rem; border-radius: .35rem; }
  .byte-cell .cursor-labels { font-size: .45rem; }
}
@media (max-width: 390px) {
  .fd-ofd-lab { padding: .85rem; }
  .scenario-picker button, .controls button { flex: 1 1 auto; }
  .file-row { grid-template-columns: repeat(5, minmax(0, 1fr)); }
  .byte-cell { min-height: 3rem; }
}
.dark .fd-ofd-lab { --lab-green: #57c9a6; } /* fixed #267258 only contrasts on light; lift it for dark */
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after { scroll-behavior: auto !important; transition-duration: .01ms !important; animation-duration: .01ms !important; animation-iteration-count: 1 !important; }
}
</style>
