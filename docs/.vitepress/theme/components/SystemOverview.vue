<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useData, withBase } from 'vitepress'
import ArchitectureIcon from './ArchitectureIcon.vue'

const parts = [
{id:'kernel',title:'WUWAOS Kernel',sub:'協調資源，也建立執行邊界',icon:'kernel',tone:'teal',desc:'核心在 RISC-V S-mode 執行，串接 trap、虛擬記憶體、裝置、TTY、檔案系統與行程。Gate / Event 提供共用的同步與等待機制。可選桌面與終端繪製也位於核心端，使用者程式則以 ecall 請求服務。',source:'kernel/boot/main.c',href:'/CODE_GUIDE.html',chapter:'完整程式碼導覽'},
{id:'shell',title:'Shell',sub:'指令與管線',icon:'terminal',tone:'blue',desc:'U-mode shell 解析指令，透過 WUWAOS 自訂 syscall ABI 建立行程、存取檔案與串接 pipe，不直接操作裝置。',source:'user/shell/shell.c',href:'/topics/applications.html',chapter:'Shell 與應用程式'},
{id:'fan',title:'fan editor',sub:'編輯與安全儲存',icon:'editor',tone:'rose',desc:'fan 是使用者模式的全螢幕文字編輯器。儲存透過 fs_replace 路徑，將新版檔案與 metadata 納入同一筆有界交易。',source:'user/apps/fan/main.c',href:'/topics/journal.html',chapter:'Journal 與安全替換'},
{id:'workstate',title:'workstate',sub:'觀察 CPU 與記憶體',icon:'activity',tone:'violet',desc:'workstate 讀取核心提供的快照，呈現行程 CPU 派發時間、RSS 與 Swap 使用情況。首頁架構圖本身不是即時監測介面。',source:'user/apps/workstate/workstate.c',href:'/topics/monitoring.html',chapter:'系統狀態監測'},
{id:'process',title:'Process / Scheduler',sub:'PCB、TCB 與 RR 排程',icon:'process',tone:'blue',desc:'Process 保存行程資源；Thread 保存執行狀態、核心堆疊與事件等待。Ready queue 排入 Thread，透過計時器與上下文切換協調各 hart 上的執行。',source:'kernel/proc/process.c · kernel/proc/thread.h · kernel/proc/scheduler.c',href:'/topics/processes.html',chapter:'行程與排程'},
{id:'memory',title:'Memory / Sv39',sub:'三級頁表、4 KiB 頁',icon:'memory',tone:'violet',desc:'Sv39 將虛擬位址轉譯到實體頁。VM 處理合法缺頁、權限與 FIFO 置換；可用的 Swap 磁碟保存換出的使用者頁，I/O 失敗則回復原映射與 RAM。',source:'kernel/mm/vm.c · kernel/mm/page.c · kernel/mm/swap.c',href:'/topics/memory.html',chapter:'實體頁與 Sv39'},
{id:'trap',title:'Trap / System Call',sub:'ecall、例外與中斷入口',icon:'gate',tone:'teal',desc:'trap entry 保存暫存器並進入核心。trap_handler 分派使用者 ecall、缺頁、計時器及外部中斷；syscall_dispatch 連到自訂 ABI 的服務。',source:'arch/riscv/trap.c · arch/riscv/trap.S · kernel/syscall/syscall.c',href:'/topics/traps.html',chapter:'例外與系統呼叫'},
{id:'io',title:'I/O / TTY',sub:'FD、OFD、pipe 與終端',icon:'io',tone:'blue',desc:'I/O 層將行程的 FD 連到 OFD、pipe、檔案或 TTY endpoint。等待中的 thread 透過核心事件讓出 CPU，裝置完成驗證後再通知等待者。',source:'kernel/io/io.c · kernel/io/tty.c',href:'/topics/io.html',chapter:'輸入輸出與 Pipe'},
{id:'filesystem',title:'WUFS / Storage',sub:'inode、journal 與 Block',icon:'storage',tone:'amber',desc:'WUFS 將路徑解析成 inode 與磁區，以 redo journal 管理有界交易。VirtIO block driver 提交磁碟請求，透過 IRQ 或 polling 的共用驗證路徑處理完成。',source:'kernel/fs/fs.c · drivers/virtio/block.c',href:'/topics/filesystem.html',chapter:'WUFS 檔案系統'},
{id:'desktop',title:'Desktop / Terminal',sub:'可選的核心端圖形服務',icon:'desktop',tone:'rose',desc:'ui/ 提供桌面視窗、輸入轉譯與終端繪製，經 TTY 與使用者程式溝通，再交給 VirtIO GPU 顯示。桌面服務位於 S-mode；視窗中的 shell 仍在 U-mode。',source:'ui/desktop/desktop.c · kernel/io/tty.c · drivers/virtio/gpu.c',href:'/topics/boot.html#桌面-terminal-與-gui-shell',chapter:'桌面與啟動流程'},
{id:'cpu',title:'RISC-V CPU',sub:'RV64 · RISC-V 虛擬處理器',icon:'cpu',tone:'teal',desc:'QEMU virt 提供 RV64 hart。各 hart 可執行核心與使用者程式，透過特權模式、trap 與頁表建立邊界，而不是每個模組各有一顆 CPU。',source:'Makefile · arch/riscv/swtch.S',href:'/topics/getting-started.html',chapter:'平台與建置'},
{id:'ram',title:'RAM',sub:'128 MiB guest memory',icon:'ram',tone:'violet',desc:'QEMU 配置 128 MiB guest RAM，用於核心、頁表、堆疊、使用者頁與裝置緩衝區。這是 Makefile 的平台設定，不是瀏覽器取得的即時用量。',source:'Makefile · kernel/mm/page.c',href:'/topics/memory.html',chapter:'記憶體管理'},
{id:'uart-driver',title:'UART Driver',sub:'序列埠字元收發',icon:'serial',tone:'blue',desc:'核心的 UART driver 透過 MMIO 讀寫 UART 暫存器。輸出時先等待傳送暫存器可用，再逐位元組寫入。',source:'drivers/uart/uart.c',href:'/topics/io.html',chapter:'輸入輸出與 Pipe'},
{id:'uart',title:'UART',sub:'QEMU 模擬序列埠',icon:'serial',tone:'blue',desc:'QEMU virt 提供 UART 裝置。WUWAOS 的 CLI 以映射在 0x10000000 的 UART 暫存器收發字元。',source:'drivers/uart/uart.h · Makefile',href:'/topics/io.html',chapter:'輸入輸出與 Pipe'},
{id:'virtio',title:'VirtIO Block',sub:'虛擬磁碟',icon:'storage',tone:'amber',desc:'核心從 FDT 探測 VirtIO MMIO 裝置。Block driver 透過 VirtIO queue 提交磁區請求，裝置處理完成後更新 used ring。',source:'drivers/virtio/block.c · Makefile',href:'/topics/block-async.html',chapter:'Block I/O 互動教學'},
{id:'virtio-gui',title:'VirtIO GPU / Input',sub:'GUI 組態',icon:'devices',tone:'rose',desc:'GUI 組態提供 VirtIO GPU、鍵盤與指標裝置，由核心端 driver 與桌面服務負責圖像呈現和輸入。',source:'Makefile · drivers/virtio/gpu.c · drivers/virtio/input.c',href:'/topics/boot.html#桌面-terminal-與-gui-shell',chapter:'桌面與啟動流程'},
{id:'opensbi',title:'OpenSBI',sub:'M-mode firmware',icon:'firmware',tone:'amber',desc:'OpenSBI 提供早期機器模式初始化與 SBI 服務。在磁碟開機組態，它交接到 flash 中的 S-mode 載入器；直接開機則交接到 QEMU 預先載入的核心。',source:'Makefile · arch/riscv/sbi.c',href:'/topics/boot.html',chapter:'開機流程'},
{id:'bootloader',title:'Bootloader',sub:'flash → WUFS → kernel.elf',icon:'boot',tone:'blue',desc:'make run-disk 使用 flash 裡的載入器，唯讀存取 VirtIO block，從 WUFS 找到 /boot/kernel.elf，驗證並載入 ELF 後交接到核心入口。直接 -kernel 開機略過這個載入器。',source:'bootloader/boot.c · bootloader/entry.S',href:'/topics/boot.html',chapter:'磁碟開機'},
]
const partById = Object.fromEntries(parts.map(part => [part.id, part]))
const layers = [
  { id: 'user', title: 'User Applications', context: 'U-mode', blocks: ['shell', 'fan', 'workstate'] },
  {
    id: 'kernel', title: 'WUWAOS Kernel', context: 'S-mode',
    incoming: { down: 'ecall', up: 'sret' },
    blocks: ['trap', 'process', 'memory', 'io', 'filesystem', 'desktop', 'uart-driver'],
  },
  {
    id: 'hardware', title: 'QEMU virt', context: '虛擬硬體',
    incoming: { down: 'MMIO', up: 'IRQ' },
    blocks: ['cpu', 'ram', 'uart', 'virtio', 'virtio-gui'],
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
const selectedId = ref('kernel')
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
  <header class="atlas-intro">
    <h1>WUWAOS 系統架構</h1>
    <p class="intro-lead">從一行 Shell 指令，看核心如何使用虛擬硬體。</p>
  </header>

  <section aria-labelledby="atlas-title">
    <div class="atlas-heading">
      <div>
        <h2 id="atlas-title">System Architecture</h2>
        <p>滑過元件即可預覽說明，點選後可查閱章節與原始碼。</p>
      </div>
      <div class="trace-actions">
        <button type="button" class="atlas-control primary-control" :aria-pressed="scenarioId === 'echo'" @click="trace('echo')">
          <ArchitectureIcon name="route" />追蹤 <code>echo hello</code>
        </button>
        <button type="button" class="atlas-control" :aria-pressed="scenarioId === 'read'" @click="trace('read')">
          追蹤檔案讀取
        </button>
      </div>
    </div>

    <div class="atlas-layout">
      <div class="architecture-board" aria-label="WUWAOS 三層系統架構">
        <template v-for="layer in layers" :key="layer.id">
          <div
            v-if="layer.incoming"
            class="layer-gap"
          >
            <span class="boundary-flow" :class="{ connected: step?.edge === layer.incoming.down }">
              <svg class="wire" viewBox="0 0 16 30" aria-hidden="true"><path d="M8 2v25m-4-4 4 4 4-4" /></svg>
              <code>{{ layer.incoming.down }}</code>
            </span>
            <span class="boundary-flow">
              <svg class="wire" viewBox="0 0 16 30" aria-hidden="true"><path d="M8 28V3m-4 4 4-4 4 4" /></svg>
              <code>{{ layer.incoming.up }}</code>
            </span>
          </div>

          <section class="stack-layer" :class="layer.id" :aria-labelledby="layer.id + '-layer-title'">
            <header class="layer-heading">
              <h3 :id="layer.id + '-layer-title'">{{ layer.title }}</h3>
              <span>{{ layer.context }}</span>
            </header>

            <div class="layer-blocks">
              <button
                v-for="id in layer.blocks"
                :key="id"
                type="button"
                class="component-block"
                :class="[partById[id].tone, { current: step?.id === id, previewed: previewId === id }]"
                :aria-pressed="selected.id === id"
                @pointerenter="previewPointer($event, id)"
                @pointerleave="hoveredId = null"
                @focus="previewFocus($event, id)"
                @blur="focusedId = null"
                @click="select(id)"
              >
                <ArchitectureIcon :name="partById[id].icon" />
                <span>{{ partById[id].title }}</span>
              </button>
            </div>
          </section>
        </template>
        <p class="board-caption">箭頭標示層間介面，從左至右為向下請求與向上回傳或中斷。GPU / Input 為 GUI 組態。</p>
      </div>

      <aside class="module-detail" aria-labelledby="detail-title">
        <div class="detail-content" aria-live="polite" aria-atomic="true">
          <p class="detail-context">
            <template v-if="scenario && detailStep">{{ scenario.label }} · {{ stepIndex + 1 }} / {{ scenario.steps.length }}</template>
            <template v-else>{{ previewId !== null && (scenario || previewId !== selected.id) ? '預覽元件' : '探索元件' }}</template>
          </p>
          <div class="detail-heading">
            <ArchitectureIcon :name="detailPart.icon" :class="detailPart.tone" />
            <h3 id="detail-title">{{ detailPart.title }}</h3>
          </div>
          <p v-if="!detailStep" class="detail-sub">{{ detailPart.sub }}</p>
          <p class="detail-description">{{ detailStep?.text ?? detailPart.desc }}</p>
        </div>
        <div v-if="scenario" class="step-controls">
          <button type="button" class="atlas-control" :aria-disabled="stepIndex === 0" @click="moveStep(-1)">上一步</button>
          <button type="button" class="atlas-control primary-control" :aria-disabled="stepIndex === scenario.steps.length - 1" @click="moveStep(1)">下一步</button>
          <button type="button" class="atlas-control" @click="stepIndex = 0">重設</button>
        </div>
        <a class="chapter-link" :href="withBase(detailPart.href)">閱讀：{{ detailPart.chapter }}</a>
        <details class="source-detail">
          <summary>對照原始碼</summary>
          <code>{{ sourcePath }}</code>
          <a :href="sourceHref">在 GitHub 查看</a>
        </details>
      </aside>
    </div>
  </section>
  <section class="boot-route" aria-labelledby="boot-route-title"><div class="boot-caption"><ArchitectureIcon name="boot"/><div><h2 id="boot-route-title">從哪裡開始執行？</h2><p>磁碟開機路徑</p></div></div><div class="boot-stations"><button type="button" :aria-pressed="selected.id==='opensbi'" @click="select('opensbi')"><ArchitectureIcon name="firmware"/><span><strong>OpenSBI</strong><small>M-mode 韌體</small></span></button><span aria-hidden="true">→</span><button type="button" :aria-pressed="selected.id==='bootloader'" @click="select('bootloader')"><ArchitectureIcon name="boot"/><span><strong>Bootloader</strong><small>從 WUFS 載入 ELF</small></span></button><span aria-hidden="true">→</span><button type="button" :aria-pressed="selected.id==='kernel'" @click="select('kernel')"><ArchitectureIcon name="kernel"/><span><strong>kernel_main</strong><small>初始化與啟動行程</small></span></button></div><p class="boot-note">直接開機時，QEMU 預先載入核心，OpenSBI 直接交接給它；略過磁碟載入器。</p></section>
  <section class="explore-section" aria-labelledby="explore-title"><h2 id="explore-title">選一條路線，走進系統。</h2><div class="reading-paths"><a :href="withBase('/topics/getting-started.html')"><ArchitectureIcon name="boot"/><div><h3>親手啟動</h3><p>準備環境，從第一個提示字元開始。</p></div><span aria-hidden="true">↗</span></a><a :href="withBase('/topics/block-async.html')"><ArchitectureIcon name="route"/><div><h3>觀察資料流</h3><p>探索等待、完成與資源生命週期。</p></div><span aria-hidden="true">↗</span></a><a :href="withBase('/CODE_GUIDE.html')"><ArchitectureIcon name="editor"/><div><h3>對照原始碼</h3><p>找到每個模組的責任與實作位置。</p></div><span aria-hidden="true">↗</span></a></div><footer class="atlas-footer"><span>C 與 RISC-V 組合語言實作的小型作業系統。</span><nav aria-label="專案參考"><a :href="withBase('/history.html')">實作紀錄</a><a :href="withBase('/portable-windows.html')">Windows 免安裝版本</a><a href="https://github.com/johnson931128/WUWAOS">GitHub</a></nav></footer></section>
</div>
</template>

<style scoped>
.architecture-home {
  --a-ink: var(--os-ink);
  --a-muted: var(--os-muted);
  --a-line: var(--os-line);
  --a-paper: var(--os-panel);
  --a-soft: var(--os-soft);
  --a-teal: var(--os-accent);
  max-width: 1320px;
  margin-inline: auto;
  color: var(--a-ink);
  font-family: var(--vp-font-family-base);
}

.architecture-home :is(h1, h2, h3, p) {
  margin: 0;
  padding: 0;
  border: 0;
}

.architecture-home :is(h1, h2, h3) { color: var(--a-ink); }
.architecture-home button { font: inherit; color: inherit; cursor: pointer; }
.architecture-home a { color: var(--a-teal); text-decoration: none; }
.architecture-home :is(button, a, summary):focus-visible {
  outline: 2px solid var(--os-focus);
  outline-offset: 3px;
  scroll-margin-block: 84px;
}

.teal { --tone: var(--os-memory); }
.blue { --tone: var(--os-io); }
.violet { --tone: var(--os-process); }
.rose { --tone: var(--os-kernel); }
.amber { --tone: var(--os-storage); }

.atlas-intro { padding: .8rem 0 2.2rem; }
.atlas-intro h1 {
  font-size: clamp(2rem, 4.1vw, 4.2rem);
  letter-spacing: -.045em;
  line-height: 1.16;
  font-weight: 730;
}
.atlas-intro .intro-lead { margin-top: .8rem; color: var(--a-muted); font-size: 1.0625rem; }

.atlas-heading,
.trace-actions,
.step-controls {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: .5rem;
}

.atlas-heading { justify-content: space-between; gap: .85rem; margin-bottom: 1rem; }
.atlas-heading h2 { font-size: 1.3rem; }
.atlas-heading p { margin-top: .25rem; color: var(--a-muted); font-size: .9375rem; }

.architecture-home .atlas-control {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: .35rem;
  min-height: 44px;
  padding: .375rem .7rem;
  border: 1px solid var(--a-line);
  border-radius: var(--os-radius-control);
  background: var(--a-paper);
  font-size: .875rem;
  font-weight: 600;
}

.atlas-control code { font-size: inherit; }
.atlas-control .arch-icon { width: 19px; height: 19px; }
.architecture-home .atlas-control:is(:hover, [aria-pressed="true"]) { border-color: var(--a-teal); }
.architecture-home .primary-control { background: var(--os-accent-soft); }
.architecture-home .atlas-control[aria-disabled="true"] { opacity: .65; cursor: default; border-color: var(--a-line); }

.atlas-layout {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(250px, 29%);
  gap: 1.5rem;
  align-items: start;
}

.architecture-board {
  container-type: inline-size;
  min-width: 0;
}

.stack-layer {
  --layer-columns: 3;
  padding: clamp(.6rem, 1.5vw, 1rem);
  border: 2px solid color-mix(in srgb, var(--layer-ink) 62%, var(--a-line));
  border-radius: var(--os-radius-workspace);
  background: color-mix(in srgb, var(--layer-ink) 6%, var(--a-paper));
}

.stack-layer.user { --layer-ink: var(--os-memory); }
.stack-layer.kernel { --layer-ink: var(--os-cpu); --layer-columns: 4; }
.stack-layer.hardware { --layer-ink: var(--os-io); }

.layer-heading {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: .15rem .65rem;
  margin-bottom: .55rem;
}
.layer-heading h3 { font-size: 1.0625rem; font-weight: 700; }
.layer-heading span { color: var(--a-muted); font-size: .875rem; }

.layer-blocks {
  display: grid;
  grid-template-columns: repeat(var(--layer-columns), minmax(0, 1fr));
  gap: clamp(.4rem, 1vw, .8rem);
}

.architecture-home .component-block {
  display: grid;
  justify-items: center;
  align-content: center;
  gap: .35rem;
  min-width: 0;
  min-height: 76px;
  padding: .55rem .35rem;
  border: 1px solid color-mix(in srgb, var(--layer-ink) 58%, var(--a-line));
  border-radius: var(--os-radius-panel);
  background: var(--a-paper);
  text-align: center;
  font-size: .875rem;
  font-weight: 650;
  line-height: 1.35;
  transition: background-color var(--os-motion-fast), border-color var(--os-motion-fast);
}

.component-block span { min-width: 0; max-width: 100%; overflow-wrap: anywhere; }
.component-block .arch-icon { width: 25px; height: 25px; color: var(--tone); }
.architecture-home .component-block:is(.previewed, [aria-pressed="true"]) {
  border-color: var(--layer-ink);
  background: color-mix(in srgb, var(--layer-ink) 13%, var(--a-paper));
}
.architecture-home .component-block:is(.current, .previewed) {
  box-shadow: inset 0 0 0 1px var(--layer-ink);
}

.layer-gap {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  min-height: 44px;
  gap: .5rem;
  padding-inline: .5rem;
}

.boundary-flow {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: .3rem;
  color: var(--a-muted);
}
.boundary-flow .wire {
  width: 16px;
  height: 30px;
  flex: none;
  fill: none;
  stroke: currentColor;
  stroke-width: 1.8;
  stroke-linecap: round;
  stroke-linejoin: round;
}
.architecture-home .boundary-flow code { color: inherit; font-size: .875rem; }
.boundary-flow.connected { color: var(--a-teal); font-weight: 700; }
.boundary-flow.connected .wire { animation: emphasize-wire var(--os-motion-fast) var(--os-ease) both; stroke-width: 2.5; }
@keyframes emphasize-wire { from { opacity: .55; } }

.architecture-home .board-caption { margin-top: .75rem; color: var(--a-muted); font-size: .875rem; }
.module-detail {
  position: sticky;
  top: 88px;
  min-width: 0;
  padding: 1rem;
  border: 1px solid var(--a-line);
  border-radius: var(--os-radius-workspace);
  background: var(--a-paper);
}
.detail-content { min-height: 12.5rem; }
.detail-heading { display: flex; gap: .6rem; align-items: center; }
.detail-heading .arch-icon { color: var(--tone); }
.detail-heading h3 { font-size: 1.26rem; overflow-wrap: anywhere; }
.module-detail .detail-context { margin-bottom: .5rem; font-size: .875rem; color: var(--a-teal); }
.module-detail .detail-sub { margin-top: .4rem; font-size: .875rem; color: var(--a-muted); }
.module-detail .detail-description { margin: .5rem 0 .8rem; font-size: 1rem; line-height: 1.7; color: var(--a-muted); }
.chapter-link { display: inline-block; padding-block: .65rem; font-size: .9375rem; font-weight: 650; }
.architecture-home .source-detail { margin: 0; padding: 0 .65rem; font-size: .875rem; }
.architecture-home .source-detail summary { margin: 0; padding-block: .6rem; color: var(--a-ink); }
.source-detail > a { display: inline-block; margin-bottom: .55rem; }
.source-detail code { display: block; margin-block: .4rem; overflow-wrap: anywhere; white-space: normal; font-size: .875rem; }

.boot-route{display:grid;grid-template-columns:1fr;gap:1rem;margin-top:1.8rem;padding:1.3rem 1.4rem;border:1px solid var(--a-line);border-radius:16px;background:var(--a-paper)}.boot-caption{display:flex;gap:.7rem;align-items:center}.boot-caption>.arch-icon{width:30px;height:30px;color:#8b651e}.boot-caption h2{font-size:1rem}.boot-caption p{font-size:.875rem;color:var(--a-muted);margin-top:.15rem}.boot-stations{display:flex;align-items:center;justify-content:space-between;gap:.5rem}.boot-stations button{display:flex;align-items:center;gap:.65rem;background:var(--a-soft);border:1px solid transparent;border-radius:9px;padding:.7rem;min-width:0;text-align:left;flex:1}.boot-stations button:hover,.boot-stations button[aria-pressed=true]{border-color:#b6cec8;background:var(--os-accent-soft)}.boot-stations .arch-icon{width:29px;height:29px;flex-shrink:0;color:var(--a-teal)}.boot-stations strong{display:block;font-size:.875rem}.boot-stations small{display:block;font-size:.875rem;color:var(--a-muted);line-height:1.7}.boot-stations>span{color:#779f96}.boot-note{font-size:.875rem;color:var(--a-muted);line-height:1.8}.explore-section{margin-top:3rem}.explore-section>h2{font-size:1.5rem;margin-bottom:1.2rem}.reading-paths{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:1.1rem}.reading-paths a{display:flex;align-items:flex-start;gap:.7rem;border:1px solid var(--a-line);border-radius:12px;background:var(--a-paper);padding:1rem;min-width:0}.reading-paths a:hover{background:var(--os-accent-soft)}.reading-paths .arch-icon{width:25px;height:25px;flex-shrink:0}.reading-paths h3{font-size:.9rem;color:var(--a-ink)}.reading-paths p{font-size:1rem;line-height:1.7;color:var(--a-muted);margin-top:.4rem}.reading-paths a>span{margin-left:auto}.atlas-footer{display:flex;justify-content:space-between;gap:1rem;flex-wrap:wrap;margin-top:2rem;padding:1rem 0;font-size:.875rem;color:var(--a-muted);border-top:1px solid var(--a-line)}.atlas-footer nav{display:flex;gap:1rem;flex-wrap:wrap}
:global(body:has(.architecture-home)),:global(body:has(.architecture-home) .VPDoc){background:var(--a-paper)}:global(body:has(.architecture-home) .VPDoc .content-container),:global(body:has(.architecture-home) .VPDoc.has-sidebar .content-container){max-width:none!important}:global(body:has(.architecture-home) .VPDoc .content){padding-inline:0;max-width:none}:global(body:has(.architecture-home) .VPDoc .container){max-width:1440px}:global(body:has(.architecture-home) .VPDocFooter){display:none}

@media (max-width: 850px) {
  .atlas-layout { grid-template-columns: minmax(0, 1fr); gap: 1rem; }
  .module-detail { position: static; }
  .detail-content { min-height: 0; }
}

@media (max-width: 600px) {
  .atlas-intro { padding-bottom: 1.5rem; }
  .component-block .arch-icon { width: 20px; height: 20px; }
  .reading-paths, .boot-stations { display: grid; grid-template-columns: 1fr; }
  .boot-stations > span { justify-self: center; transform: rotate(90deg); }
  .boot-stations button { width: 100%; }
}

@container (max-width: 540px) {
  .stack-layer.kernel, .stack-layer.hardware { --layer-columns: 2; }
}

@media (prefers-reduced-motion: reduce) {
  .architecture-home .component-block { transition: none; }
  .boundary-flow.connected .wire { animation: none; }
}
</style>
