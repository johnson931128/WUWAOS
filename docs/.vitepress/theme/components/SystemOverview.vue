<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useData, withBase } from 'vitepress'
import ArchitectureIcon from './ArchitectureIcon.vue'

const parts = [
  { id: 'kernel', title: 'WUWAOS Kernel', sub: '協調資源，也建立執行邊界', privilege: 'S-mode', badge: 'KERNEL CORE', icon: 'kernel', tone: 'indigo', desc: '核心在 RISC-V S-mode 執行，串接 trap、虛擬記憶體、裝置、TTY、檔案系統與行程。Gate / Event 提供共用的同步與等待機制。可選桌面與終端繪製也位於核心端，使用者程式則以 ecall 請求服務。', source: 'kernel/boot/main.c', href: '/CODE_GUIDE.html', chapter: '完整程式碼導覽' },
  { id: 'shell', title: 'Shell', sub: '指令與管線', privilege: 'U-mode', badge: 'USER APP', icon: 'terminal', tone: 'blue', desc: 'U-mode shell 解析指令，透過 WUWAOS 自訂 syscall ABI 建立行程、存取檔案與串接 pipe，不直接操作裝置。', source: 'user/shell/shell.c', href: '/topics/applications.html', chapter: 'Shell 與應用程式' },
  { id: 'fan', title: 'fan editor', sub: '編輯與安全儲存', privilege: 'U-mode', badge: 'USER APP', icon: 'editor', tone: 'indigo', desc: 'fan 是使用者模式的全螢幕文字編輯器。儲存透過 fs_replace 路徑，將新版檔案與 metadata 納入同一筆有界交易。', source: 'user/apps/fan/main.c', href: '/topics/journal.html', chapter: 'Journal 與安全替換' },
  { id: 'workstate', title: 'workstate', sub: '觀察 CPU 與記憶體', privilege: 'U-mode', badge: 'USER APP', icon: 'activity', tone: 'violet', desc: 'workstate 讀取核心提供的快照，呈現行程 CPU 派發時間、RSS 與 Swap 使用情況。首頁架構圖本身不是即時監測介面。', source: 'user/apps/workstate/workstate.c', href: '/topics/monitoring.html', chapter: '系統狀態監測' },
  { id: 'process', title: 'Process / Scheduler', sub: 'PCB、TCB 與 RR 排程', privilege: 'S-mode', badge: 'KERNEL MODULE', icon: 'process', tone: 'violet', desc: 'Process 保存行程資源；Thread 保存執行狀態、核心堆疊與事件等待。Ready queue 排入 Thread，透過計時器與上下文切換協調各 hart 上的執行。', source: 'kernel/proc/process.c · kernel/proc/scheduler.c', href: '/spec/03-processes-and-threads.html', chapter: '行程與排程' },
  { id: 'memory', title: 'Memory / Sv39', sub: '三級頁表、4 KiB 頁', privilege: 'S-mode', badge: 'KERNEL MODULE', icon: 'memory', tone: 'teal', desc: 'Sv39 將虛擬位址轉譯到實體頁。VM 處理合法缺頁、權限與 FIFO 置換；可用的 Swap 磁碟保存換出的使用者頁，I/O 失敗則回復原映射與 RAM。', source: 'kernel/mm/vm.c · kernel/mm/swap.c', href: '/spec/08-virtual-memory.html', chapter: '實體頁與 Sv39' },
  { id: 'trap', title: 'Trap / System Call', sub: 'ecall、例外與中斷入口', privilege: 'S-mode', badge: 'KERNEL MODULE', icon: 'gate', tone: 'indigo', desc: 'trap entry 保存暫存器並進入核心。trap_handler 分派使用者 ecall、缺頁、計時器及外部中斷；syscall_dispatch 連到自訂 ABI 的服務。', source: 'arch/riscv/trap.S · kernel/syscall/syscall.c', href: '/spec/02-traps-and-syscalls.html', chapter: '例外與系統呼叫' },
  { id: 'io', title: 'I/O / TTY', sub: 'FD、OFD、pipe 與終端', privilege: 'S-mode', badge: 'KERNEL MODULE', icon: 'io', tone: 'blue', desc: 'I/O 層將行程的 FD 連到 OFD、pipe、檔案或 TTY endpoint。等待中的 thread 透過核心事件讓出 CPU，裝置完成驗證後再通知等待者。', source: 'kernel/io/io.c · kernel/io/tty.c', href: '/spec/07-io.html', chapter: '輸入輸出與 Pipe' },
  { id: 'filesystem', title: 'WUFS / Storage', sub: 'inode、journal 與 Block', privilege: 'S-mode', badge: 'KERNEL MODULE', icon: 'storage', tone: 'amber', desc: 'WUFS 將路徑解析成 inode 與磁區，以 redo journal 管理有界交易。VirtIO block driver 提交磁碟請求，透過 IRQ 或 polling 的共用驗證路徑處理完成。', source: 'kernel/fs/fs.c · drivers/virtio/block.c', href: '/spec/09-filesystem.html', chapter: 'WUFS 檔案系統' },
  { id: 'desktop', title: 'Desktop / Terminal', sub: '可選的核心端圖形服務', privilege: 'S-mode', badge: 'KERNEL MODULE', icon: 'desktop', tone: 'violet', desc: 'ui/ 提供桌面視窗、輸入轉譯與終端繪製，經 TTY 與使用者程式溝通，再交給 VirtIO GPU 顯示。桌面服務位於 S-mode；視窗中的 shell 仍在 U-mode。', source: 'ui/desktop/desktop.c · drivers/virtio/gpu.c', href: '/topics/boot.html#桌面-terminal-與-gui-shell', chapter: '桌面與啟動流程' },
  { id: 'uart-driver', title: 'UART Driver', sub: '序列埠字元收發', privilege: 'S-mode', badge: 'DRIVER', icon: 'serial', tone: 'blue', desc: '核心的 UART driver 透過 MMIO 讀寫 UART 暫存器。輸出時先等待傳送暫存器可用，再逐位元組寫入。', source: 'drivers/uart/uart.c', href: '/spec/07-io.html', chapter: '輸入輸出與 Pipe' },
  { id: 'cpu', title: 'RISC-V CPU', sub: 'RV64 · RISC-V 虛擬處理器', privilege: 'Hardware', badge: 'QEMU VIRT', icon: 'cpu', tone: 'teal', desc: 'QEMU virt 提供 RV64 hart。各 hart 可執行核心與使用者程式，透過特權模式、trap 與頁表建立邊界，而不是每個模組各有一顆 CPU。', source: 'Makefile · arch/riscv/swtch.S', href: '/spec/01-running.html', chapter: '平台與建置' },
  { id: 'ram', title: '128 MiB RAM', sub: 'guest memory', privilege: 'Hardware', badge: 'QEMU VIRT', icon: 'ram', tone: 'violet', desc: 'QEMU 配置 128 MiB guest RAM，用於核心、頁表、堆疊、使用者頁與裝置緩衝區。這是 Makefile 的平台設定，不是瀏覽器取得的即時用量。', source: 'Makefile · kernel/mm/page.c', href: '/spec/08-virtual-memory.html', chapter: '記憶體管理' },
  { id: 'uart', title: 'UART', sub: 'QEMU 模擬序列埠', privilege: 'Hardware', badge: 'QEMU VIRT', icon: 'serial', tone: 'blue', desc: 'QEMU virt 提供 UART 裝置。WUWAOS 的 CLI 以映射在 0x10000000 的 UART 暫存器收發字元。', source: 'drivers/uart/uart.h · Makefile', href: '/spec/07-io.html', chapter: '輸入輸出與 Pipe' },
  { id: 'virtio', title: 'VirtIO Block', sub: '虛擬磁碟', privilege: 'Hardware', badge: 'QEMU VIRT', icon: 'storage', tone: 'amber', desc: '核心從 FDT 探測 VirtIO MMIO 裝置。Block driver 透過 VirtIO queue 提交磁區請求，裝置處理完成後更新 used ring。', source: 'drivers/virtio/block.c · Makefile', href: '/topics/block-async.html', chapter: 'Block I/O 互動教學' },
  { id: 'virtio-gui', title: 'VirtIO GPU', sub: 'GUI 組態', privilege: 'Hardware', badge: 'QEMU VIRT', icon: 'devices', tone: 'indigo', desc: 'GUI 組態提供 VirtIO GPU、鍵盤與指標裝置，由核心端 driver 與桌面服務負責圖像呈現和輸入。', source: 'Makefile · drivers/virtio/gpu.c', href: '/topics/boot.html#桌面-terminal-與-gui-shell', chapter: '桌面與啟動流程' },
  { id: 'opensbi', title: 'OpenSBI', sub: 'M-mode firmware', privilege: 'M-mode', badge: 'BOOT CHAIN', icon: 'firmware', tone: 'amber', desc: 'OpenSBI 提供早期機器模式初始化與 SBI 服務。在磁碟開機組態，它交接到 flash 中的 S-mode 載入器；直接開機則交接到 QEMU 預先載入的核心。', source: 'Makefile · arch/riscv/sbi.c', href: '/spec/01-running.html#從韌體交接到-shell', chapter: '開機流程' },
  { id: 'bootloader', title: 'Bootloader', sub: 'flash → WUFS → kernel.elf', privilege: 'S-mode', badge: 'BOOT CHAIN', icon: 'boot', tone: 'blue', desc: 'make run-disk 使用 flash 裡的載入器，唯讀存取 VirtIO block，從 WUFS 找到 /boot/kernel.elf，驗證並載入 ELF 後交接到核心入口。直接 -kernel 開機略過這個載入器。', source: 'bootloader/boot.c · bootloader/entry.S', href: '/spec/01-running.html#從韌體交接到-shell', chapter: '磁碟開機' },
]

const partById = Object.fromEntries(parts.map(part => [part.id, part]))

const layers = [
  {
    id: 'user',
    title: 'User Applications',
    context: 'U-mode',
    rows: [['shell', 'fan', 'workstate']],
  },
  {
    id: 'kernel',
    title: 'WUWAOS Kernel',
    context: 'S-mode',
    incoming: { down: 'ecall', up: 'sret' },
    rows: [
      ['trap', 'process', 'memory', 'io'],
      ['filesystem', 'desktop', 'uart-driver'],
    ],
  },
  {
    id: 'hardware',
    title: 'QEMU virt Hardware',
    context: '虛擬硬體',
    incoming: { down: 'MMIO', up: 'IRQ' },
    rows: [['cpu', 'ram', 'uart', 'virtio', 'virtio-gui']],
  },
]

const scenarios = {
  echo: {
    label: '追蹤 echo hello',
    steps: [
      { id: 'shell', text: 'CLI Shell 的 echo 子行程呼叫 write(1, …)，將 hello 寫到標準輸出。這裡追蹤文字的寫入，換行是另一筆 write。', source: 'user/shell/shell_exec.c#L137' },
      { id: 'trap', edge: 'ecall', text: 'write 的使用者端包裝執行 ecall，切入 S-mode。核心依系統呼叫編號 SYS_WRITE 分派到 io_write()。', source: 'kernel/syscall/syscall.c#L43' },
      { id: 'io', text: 'io_write() 驗證標準輸出的 FD 與使用者緩衝區，複製資料，再交給對應的 UART TTY endpoint。', source: 'kernel/io/io.c#L536' },
      { id: 'uart-driver', text: 'TTY 的 UART 輸出函式逐一呼叫 uart_putc_raw()，驅動程式等待 UART 的傳送暫存器可寫。', source: 'kernel/io/tty.c#L43' },
      { id: 'uart', edge: 'MMIO', text: '驅動程式以 MMIO 將位元組寫入 QEMU 模擬的 UART，終端收到 hello。此筆 write 完成後，核心把寫入的位元組數回傳給呼叫者。', source: 'drivers/uart/uart.c#L12' },
    ],
  },
  read: {
    label: '追蹤檔案讀取',
    steps: [
      { id: 'shell', text: '以 CLI 的 cat <檔名> 為例，cat 開啟檔案後對取得的描述號呼叫 read()。', source: 'user/shell/shell_exec.c#L114' },
      { id: 'trap', edge: 'ecall', text: '使用者端 read() 透過 ecall 進入核心，SYS_READ 分派至 io_read()。', source: 'kernel/syscall/syscall.c' },
      { id: 'io', text: 'I/O 層確認 FD 指向可讀的一般檔案，取得 OFD 的 inode 與目前位移。', source: 'kernel/io/io.c#L470' },
      { id: 'filesystem', text: 'WUFS 依 inode 與位移找出磁區。需要裝置資料時呼叫 block driver。', source: 'kernel/fs/fs.c#L1427' },
      { id: 'virtio', edge: 'MMIO', text: 'Block driver 透過 MMIO 通知 VirtIO 裝置處理磁區請求。完成驗證後喚醒等待者，再經 WUFS 與 I/O 將讀得的資料複製回使用者緩衝區。', source: 'drivers/virtio/block.c#L599' },
    ],
  },
}

type ScenarioId = keyof typeof scenarios
const selectedId = ref('memory')
const hoveredId = ref<string | null>(null)
const focusedId = ref<string | null>(null)
const scenarioId = ref<ScenarioId | null>(null)
const stepIndex = ref(0)
const scenario = computed(() => scenarioId.value ? scenarios[scenarioId.value] : null)
const step = computed(() => scenario.value?.steps[stepIndex.value])
const selected = computed(() => partById[step.value?.id ?? selectedId.value])
const previewId = computed(() => hoveredId.value ?? focusedId.value)
const detailPart = computed(() => previewId.value ? partById[previewId.value] : selected.value)
const detailStep = computed(() => previewId.value ? undefined : step.value)
const sourcePath = computed(() => detailStep.value?.source ?? detailPart.value.source)
const sourceHref = computed(() => 'https://github.com/johnson931128/WUWAOS/blob/main/' + sourcePath.value.split(' · ')[0])

function select(id: string) {
  selectedId.value = id
  scenarioId.value = null
}

function previewPointer(event: PointerEvent, id: string) {
  if (event.pointerType === 'mouse') hoveredId.value = id
}

function previewFocus(event: FocusEvent, id: string) {
  if ((event.currentTarget as HTMLElement).matches(':focus-visible')) focusedId.value = id
}

function trace(id: ScenarioId) {
  stepIndex.value = 0
  scenarioId.value = id
}

function moveStep(offset: number) {
  const next = stepIndex.value + offset
  if (scenario.value && next >= 0 && next < scenario.value.steps.length) stepIndex.value = next
}

const { isDark } = useData()
onMounted(() => {
  const appearance = localStorage.getItem('vitepress-theme-appearance')
  if (!appearance || appearance === 'auto') isDark.value = false
})
</script>

<template>
  <div class="system-overview architecture-home">
    <!-- Compact Single-Row Header so the Interactive Architecture Diagram sits Above the Fold -->
    <header class="compact-header">
      <div class="header-copy">
        <div class="title-row">
          <h1>WUWAOS</h1>
          <span class="hero-pill">RISC-V 64-BIT · SV39</span>
        </div>
        <p class="hero-lead">以 C 與 RISC-V 實作的教學作業系統——結合恐龍書理論、互動動畫與真實原始碼。</p>
      </div>
      <div class="hero-actions">
        <a class="hero-btn primary" :href="withBase('/spec/')">開始閱讀課本 (00–09)</a>
        <a class="hero-btn secondary" :href="withBase('/topics/labs.html')">互動實驗室</a>
      </div>
    </header>

    <!-- Option 1: Precision Interactive System Architecture Board + Integrated Boot Strip -->
    <section class="atlas-stage" aria-labelledby="atlas-title">
      <div class="atlas-toolbar">
        <div class="toolbar-copy">
          <h2 id="atlas-title">互動系統架構圖</h2>
          <p>滑過元件預覽說明，點選後查閱對應章節與原始碼，或直接追蹤系統呼叫路徑。</p>
        </div>
        <div class="trace-actions" role="group" aria-label="追蹤範例流程">
          <button
            type="button"
            class="trace-pill"
            :class="{ active: scenarioId === 'echo' }"
            :aria-pressed="scenarioId === 'echo'"
            @click="trace('echo')"
          >
            <ArchitectureIcon name="route" />
            <span>追蹤 <code>echo hello</code></span>
          </button>
          <button
            type="button"
            class="trace-pill"
            :class="{ active: scenarioId === 'read' }"
            :aria-pressed="scenarioId === 'read'"
            @click="trace('read')"
          >
            <span>追蹤檔案讀取</span>
          </button>
        </div>
      </div>

      <div class="atlas-layout">
        <div class="architecture-board" aria-label="WUWAOS 三層系統架構">
          <template v-for="layer in layers" :key="layer.id">
            <div v-if="layer.incoming" class="layer-connector">
              <span class="connector-pill" :class="{ connected: step?.edge === layer.incoming.down }">
                <svg class="wire" viewBox="0 0 16 24" aria-hidden="true"><path d="M8 2v19m-4-4 4 4 4-4" /></svg>
                <code>{{ layer.incoming.down }}</code>
              </span>
              <span class="connector-pill">
                <svg class="wire" viewBox="0 0 16 24" aria-hidden="true"><path d="M8 22V3m-4 4 4-4 4 4" /></svg>
                <code>{{ layer.incoming.up }}</code>
              </span>
            </div>

            <section class="stack-band" :class="layer.id" :aria-labelledby="layer.id + '-layer-title'">
              <header class="band-aside">
                <h3 :id="layer.id + '-layer-title'">{{ layer.title }}</h3>
                <span class="band-context">{{ layer.context }}</span>
              </header>

              <div class="band-rows">
                <div
                  v-for="(row, rowIdx) in layer.rows"
                  :key="rowIdx"
                  class="band-row"
                  :style="{ '--row-cols': row.length }"
                >
                  <button
                    v-for="id in row"
                    :key="id"
                    type="button"
                    class="node-card"
                    :class="[partById[id].tone, { current: step?.id === id, previewed: previewId === id }]"
                    :aria-pressed="selected.id === id"
                    @pointerenter="previewPointer($event, id)"
                    @pointerleave="hoveredId = null"
                    @focus="previewFocus($event, id)"
                    @blur="focusedId = null"
                    @click="select(id)"
                  >
                    <ArchitectureIcon :name="partById[id].icon" class="node-icon" />
                    <span class="node-title">{{ partById[id].title }}</span>
                  </button>
                </div>
              </div>
            </section>
          </template>

          <!-- Integrated Compact Boot Strip (Option 1) -->
          <div class="boot-bar" role="group" aria-label="開機交接路徑">
            <span class="boot-tag">開機路徑</span>
            <button type="button" class="boot-chip" :aria-pressed="selected.id === 'opensbi'" @click="select('opensbi')">OpenSBI</button>
            <span class="boot-sep" aria-hidden="true">→</span>
            <button type="button" class="boot-chip" :aria-pressed="selected.id === 'bootloader'" @click="select('bootloader')">Bootloader</button>
            <span class="boot-sep" aria-hidden="true">→</span>
            <button type="button" class="boot-chip" :aria-pressed="selected.id === 'kernel'" @click="select('kernel')">kernel_main</button>
          </div>
          <p class="board-caption">直接開機時由 QEMU 預載核心並略過 Bootloader；VirtIO GPU / Input 為 GUI 組態。</p>
        </div>

        <!-- Option 1 Structured Right Inspector -->
        <aside class="inspector-panel" aria-labelledby="detail-title">
          <div class="inspector-body" aria-live="polite" aria-atomic="true">
            <span class="inspector-badge">
              <template v-if="scenario && detailStep">{{ scenario.label }} · {{ stepIndex + 1 }} / {{ scenario.steps.length }}</template>
              <template v-else>{{ detailPart.badge }}</template>
            </span>
            <h3 id="detail-title">{{ detailPart.title }}</h3>
            <p class="inspector-desc">{{ detailStep?.text ?? detailPart.desc }}</p>

            <dl class="inspector-meta">
              <div><dt>層級</dt><dd>{{ detailPart.privilege }}</dd></div>
              <div><dt>原始碼</dt><dd><code>{{ sourcePath.split(' · ')[0] }}</code></dd></div>
            </dl>
          </div>

          <div v-if="scenario" class="step-controls" role="group" aria-label="追蹤步驟控制">
            <button type="button" class="step-btn" :disabled="stepIndex === 0" @click="moveStep(-1)">上一步</button>
            <button type="button" class="step-btn primary" :disabled="stepIndex === scenario.steps.length - 1" @click="moveStep(1)">下一步</button>
            <button type="button" class="step-btn ghost" @click="stepIndex = 0">重設</button>
          </div>

          <div class="inspector-footer">
            <a class="chapter-cta" :href="withBase(detailPart.href)">
              <span>閱讀：{{ detailPart.chapter }}</span>
              <span aria-hidden="true">→</span>
            </a>
            <a class="github-link" :href="sourceHref" target="_blank" rel="noopener">在 GitHub 查看原始碼 ↗</a>
          </div>
        </aside>
      </div>
    </section>

    <section class="explore-section" aria-labelledby="explore-title">
      <h2 id="explore-title">從這裡開始</h2>
      <div class="reading-paths">
        <a class="path-card" :href="withBase('/spec/')">
          <span class="path-badge blue"><ArchitectureIcon name="editor" /></span>
          <div>
            <h3>閱讀 OS 課本（00–09）</h3>
            <p>結合恐龍書理論與 WUWAOS 真實程式碼，循序理解核心設計。</p>
          </div>
        </a>
        <a class="path-card" :href="withBase('/spec/01-running.html')">
          <span class="path-badge teal"><ArchitectureIcon name="boot" /></span>
          <div>
            <h3>親手啟動</h3>
            <p>準備 QEMU 環境或 Windows 免安裝包，跑出第一個提示字元。</p>
          </div>
        </a>
        <a class="path-card" :href="withBase('/topics/labs.html')">
          <span class="path-badge indigo"><ArchitectureIcon name="route" /></span>
          <div>
            <h3>互動實驗室</h3>
            <p>逐步操作排程、Sv39 頁表、Block 非同步 I/O 與 WUFS Journal。</p>
          </div>
        </a>
      </div>

      <footer class="atlas-footer">
        <span>C 與 RISC-V 組合語言實作的小型教學作業系統。</span>
        <nav aria-label="專案參考">
          <a :href="withBase('/CODE_GUIDE.html')">程式碼導覽</a>
          <a :href="withBase('/history.html')">歷史紀錄</a>
          <a :href="withBase('/portable-windows.html')">Windows 免安裝版本</a>
          <a href="https://github.com/johnson931128/WUWAOS" target="_blank" rel="noopener">GitHub</a>
        </nav>
      </footer>
    </section>
  </div>
</template>

<style scoped>
.architecture-home {
  max-width: 1220px;
  margin-inline: auto;
  color: var(--os-ink);
  font-family: var(--vp-font-family-base);
}

.architecture-home :is(h1, h2, h3, p) {
  margin: 0;
  padding: 0;
  border: 0;
}
.architecture-home button { font: inherit; color: inherit; cursor: pointer; }
.architecture-home a { color: var(--os-accent); text-decoration: none; }

.indigo { --tone: var(--os-kernel); }
.blue { --tone: var(--os-io); }
.teal { --tone: var(--os-memory); }
.violet { --tone: var(--os-process); }
.amber { --tone: var(--os-storage); }

/* Compact Single-Row Header */
.compact-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 1rem 1.5rem;
  padding: 0.5rem 0 1.4rem;
  border-bottom: 1px solid var(--os-line);
}
.title-row {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.75rem;
}
.hero-pill {
  display: inline-block;
  padding: 0.2rem 0.65rem;
  border-radius: 999px;
  background: var(--os-accent-soft);
  color: var(--os-accent);
  font-size: 0.875rem;
  font-weight: 700;
  letter-spacing: 0.04em;
}
.compact-header h1 {
  font-size: clamp(1.85rem, 3vw, 2.35rem);
  font-weight: 800;
  letter-spacing: -0.035em;
  line-height: 1.1;
}
.hero-lead {
  margin-top: 0.4rem !important;
  font-size: 0.96875rem;
  line-height: 1.6;
  color: var(--os-muted);
}
.hero-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 0.65rem;
}
.hero-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-height: 42px;
  padding: 0.55rem 1.15rem;
  border-radius: var(--os-radius-control);
  font-size: 0.9375rem;
  font-weight: 600;
  transition: background-color var(--os-motion-fast) var(--os-ease), border-color var(--os-motion-fast) var(--os-ease);
}
.hero-btn.primary {
  background: var(--os-accent);
  color: var(--os-on-accent) !important;
}
.hero-btn.primary:hover { background: var(--os-accent-hover); }
.hero-btn.secondary {
  background: var(--os-soft);
  color: var(--os-ink) !important;
  border: 1px solid var(--os-line);
}
.hero-btn.secondary:hover { background: var(--os-soft-hover); }

/* Interactive Atlas Stage */
.atlas-stage {
  padding-top: 1.5rem;
  scroll-margin-top: 84px;
}
.atlas-toolbar {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
  flex-wrap: wrap;
  gap: 1rem;
  margin-bottom: 1.25rem;
}
.toolbar-copy h2 {
  font-size: 1.45rem;
  font-weight: 750;
  letter-spacing: -0.02em;
  scroll-margin-top: 90px;
}
.toolbar-copy p {
  margin-top: 0.3rem !important;
  font-size: 0.9375rem;
  color: var(--os-muted);
}
.trace-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
}
.trace-pill {
  display: inline-flex;
  align-items: center;
  gap: 0.45rem;
  min-height: 40px;
  padding: 0.4rem 0.9rem;
  border: 1px solid var(--os-line);
  border-radius: 999px;
  background: var(--os-panel);
  font-size: 0.875rem;
  font-weight: 600;
  transition: background-color var(--os-motion-fast) var(--os-ease), border-color var(--os-motion-fast) var(--os-ease), color var(--os-motion-fast) var(--os-ease);
}
.trace-pill .arch-icon { width: 17px; height: 17px; color: var(--os-accent); }
.trace-pill code { font-size: 0.875rem; background: transparent; border: 0; padding: 0; color: inherit; }
.trace-pill:hover { background: var(--os-soft); border-color: var(--os-line-strong); }
.trace-pill.active { background: var(--os-accent); border-color: var(--os-accent); color: var(--os-on-accent); }
.trace-pill.active .arch-icon { color: var(--os-on-accent); }

.atlas-layout {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(290px, 325px);
  gap: 1.5rem;
  align-items: start;
}
.architecture-board { min-width: 0; }

/* Clean Full-Width Band Layout — top header so cards get 100% width and never chop words */
.stack-band {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  padding: 1.1rem 1.25rem;
  border-radius: var(--os-radius-workspace);
}
.stack-band.user { background: var(--os-band-user); }
.stack-band.kernel { background: var(--os-band-kernel); }
.stack-band.hardware { background: var(--os-band-hardware); }

.band-aside {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
}
.band-aside h3 {
  font-size: 0.96875rem;
  font-weight: 700;
  line-height: 1.3;
}
.band-context {
  padding: 0.14rem 0.6rem;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.75);
  font-size: 0.875rem;
  font-weight: 500;
  color: var(--os-muted);
}
.dark .band-context {
  background: rgba(15, 23, 42, 0.55);
}

.band-rows { display: grid; gap: 0.6rem; }
.band-row {
  display: grid;
  grid-template-columns: repeat(var(--row-cols, 3), minmax(0, 1fr));
  gap: 0.6rem;
}

.node-card {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 0.4rem;
  min-width: 0;
  min-height: 76px;
  padding: 0.65rem 0.5rem;
  border: 1px solid transparent;
  border-radius: var(--os-radius-panel);
  background: var(--os-panel);
  box-shadow: var(--os-shadow-panel);
  text-align: center;
  transition: transform var(--os-motion-fast) var(--os-ease), box-shadow var(--os-motion-fast) var(--os-ease), border-color var(--os-motion-fast) var(--os-ease), background-color var(--os-motion-fast) var(--os-ease);
}
.node-icon { width: 22px; height: 22px; color: var(--tone); flex-shrink: 0; }
.node-title {
  font-size: 0.875rem;
  font-weight: 600;
  line-height: 1.35;
  color: var(--os-ink);
  word-break: keep-all;
  overflow-wrap: normal;
}
.node-card:hover {
  transform: translateY(-1px);
  box-shadow: var(--os-shadow-hover);
  border-color: color-mix(in srgb, var(--os-accent) 45%, transparent);
}
.node-card[aria-pressed="true"],
.node-card.current {
  border-color: var(--os-accent);
  background: var(--os-accent-soft);
  box-shadow: 0 0 0 1.5px var(--os-accent);
}
.node-card.previewed:not([aria-pressed="true"]) {
  border-color: color-mix(in srgb, var(--os-accent) 55%, var(--os-line));
}

.layer-connector {
  display: flex;
  justify-content: center;
  gap: 3.5rem;
  padding: 0.45rem 0;
}
.connector-pill {
  display: inline-flex;
  align-items: center;
  gap: 0.35rem;
  padding: 0.18rem 0.65rem;
  border-radius: 999px;
  background: var(--os-soft);
  color: var(--os-muted);
  font-size: 0.875rem;
}
.connector-pill code { background: transparent; border: 0; padding: 0; font-size: 0.875rem; color: inherit; }
.connector-pill .wire { width: 14px; height: 18px; fill: none; stroke: currentColor; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }
.connector-pill.connected { background: var(--os-accent); color: var(--os-on-accent); font-weight: 650; }

/* Integrated Compact Boot Bar */
.boot-bar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: center;
  gap: 0.55rem;
  margin-top: 0.9rem;
  padding: 0.65rem 1rem;
  border-radius: var(--os-radius-panel);
  background: var(--os-soft);
}
.boot-tag {
  margin-right: 0.35rem;
  font-size: 0.875rem;
  font-weight: 700;
  color: var(--os-muted);
}
.boot-chip {
  min-height: 36px;
  padding: 0.3rem 0.8rem;
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-control);
  background: var(--os-panel);
  font-size: 0.875rem;
  font-weight: 600;
  transition: border-color var(--os-motion-fast) var(--os-ease), background-color var(--os-motion-fast) var(--os-ease);
}
.boot-chip:hover { border-color: var(--os-accent); }
.boot-chip[aria-pressed="true"] {
  border-color: var(--os-accent);
  background: var(--os-accent);
  color: var(--os-on-accent);
}
.boot-sep { color: var(--os-subtle); font-weight: 600; }

.board-caption {
  margin-top: 0.65rem !important;
  font-size: 0.875rem;
  color: var(--os-muted);
}

/* Structured Right Inspector (Option 1) */
.inspector-panel {
  position: sticky;
  top: calc(var(--vp-nav-height, 64px) + 24px);
  padding: 1.4rem;
  border-radius: var(--os-radius-workspace);
  background: var(--os-soft);
  display: flex;
  flex-direction: column;
  gap: 1.1rem;
}
.inspector-badge {
  display: inline-block;
  padding: 0.18rem 0.6rem;
  border-radius: 999px;
  background: var(--os-accent);
  color: var(--os-on-accent);
  font-size: 0.875rem;
  font-weight: 700;
  letter-spacing: 0.02em;
}
.inspector-panel h3 {
  margin-top: 0.75rem !important;
  font-size: 1.3rem;
  font-weight: 750;
  line-height: 1.25;
}
.inspector-desc {
  margin-top: 0.65rem !important;
  font-size: 0.9375rem;
  line-height: 1.7;
  color: var(--os-muted);
}
.inspector-meta {
  margin: 1rem 0 0;
  border-top: 1px solid var(--os-line);
}
.inspector-meta > div {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  gap: 0.75rem;
  padding: 0.55rem 0;
  border-bottom: 1px solid var(--os-line);
  font-size: 0.875rem;
}
.inspector-meta dt { color: var(--os-muted); font-weight: 600; flex-shrink: 0; }
.inspector-meta dd { margin: 0; text-align: right; color: var(--os-ink); font-weight: 600; overflow-wrap: anywhere; }
.inspector-meta code { font-size: 0.875rem; background: transparent; border: 0; padding: 0; }

.step-controls { display: flex; flex-wrap: wrap; gap: 0.45rem; }
.step-btn {
  min-height: 38px;
  padding: 0.35rem 0.8rem;
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-control);
  background: var(--os-panel);
  font-size: 0.875rem;
  font-weight: 600;
}
.step-btn.primary { background: var(--os-accent); border-color: var(--os-accent); color: var(--os-on-accent); }
.step-btn.ghost { background: transparent; border-color: transparent; color: var(--os-muted); }
.step-btn:disabled { opacity: 0.45; cursor: not-allowed; }

.inspector-footer {
  display: flex;
  flex-direction: column;
  gap: 0.6rem;
}
.chapter-cta {
  display: inline-flex;
  align-items: center;
  justify-content: space-between;
  padding: 0.65rem 0.95rem;
  border-radius: var(--os-radius-control);
  background: var(--os-panel);
  color: var(--os-accent);
  font-size: 0.9375rem;
  font-weight: 650;
  box-shadow: var(--os-shadow-panel);
  transition: background-color var(--os-motion-fast) var(--os-ease);
}
.chapter-cta:hover { background: var(--os-accent-soft); }
.github-link {
  font-size: 0.875rem;
  color: var(--os-muted) !important;
  font-weight: 500;
}
.github-link:hover { color: var(--os-accent) !important; }

/* Explore / Start Reading */
.explore-section { margin-top: 3rem; }
.explore-section > h2 { font-size: 1.45rem; font-weight: 750; margin-bottom: 1.25rem !important; }
.reading-paths { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 1.25rem; }
.path-card {
  display: flex;
  flex-direction: column;
  gap: 0.9rem;
  padding: 1.35rem;
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-workspace);
  background: var(--os-panel);
  transition: transform var(--os-motion-fast) var(--os-ease), border-color var(--os-motion-fast) var(--os-ease), box-shadow var(--os-motion-fast) var(--os-ease);
}
.path-card:hover { transform: translateY(-2px); border-color: color-mix(in srgb, var(--os-accent) 45%, var(--os-line)); box-shadow: var(--os-shadow-hover); }
.path-badge { display: grid; place-items: center; width: 42px; height: 42px; border-radius: 10px; background: var(--os-soft); color: var(--tone); }
.path-badge .arch-icon { width: 22px; height: 22px; }
.path-card h3 { font-size: 1.0625rem; font-weight: 700; color: var(--os-ink); }
.path-card p { margin-top: 0.4rem !important; font-size: 0.9375rem; line-height: 1.65; color: var(--os-muted); }

.atlas-footer {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 1rem;
  margin-top: 3rem;
  padding-top: 1.5rem;
  border-top: 1px solid var(--os-line);
  font-size: 0.875rem;
  color: var(--os-muted);
}
.atlas-footer nav { display: flex; flex-wrap: wrap; gap: 1.25rem; }
.atlas-footer nav a { color: var(--os-muted); font-weight: 500; }
.atlas-footer nav a:hover { color: var(--os-accent); }

@media (max-width: 960px) {
  .split-hero { grid-template-columns: 1fr; }
  .atlas-layout { grid-template-columns: 1fr; }
  .inspector-panel { position: static; }
}
@media (max-width: 760px) {
  .stack-band { grid-template-columns: 1fr; gap: 0.65rem; }
  .band-row { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  .reading-paths { grid-template-columns: 1fr; }
}
@media (prefers-reduced-motion: reduce) {
  .node-card, .path-card, .hero-btn { transition: none; transform: none; }
}
</style>
