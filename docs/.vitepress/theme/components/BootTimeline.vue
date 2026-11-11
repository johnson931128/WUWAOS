<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from 'vue'
import { withBase } from 'vitepress'
import ArchitectureIcon from './ArchitectureIcon.vue'

const sourceRoot = 'https://github.com/johnson931128/WUWAOS/blob/main/'

const stages = [
  {
    title: 'QEMU virt', icon: 'desktop', subtitle: 'RV64 虛擬機',
    summary: '主機用 QEMU 建立單 hart、128 MiB RAM 的 virt 機器，並以預設 OpenSBI 韌體啟動。',
    detail: '磁碟開機將 boot-flash.img 接成唯讀 flash，另接一顆 VirtIO 磁碟。直接開機由 -kernel 預載 build/kernel.elf 到 RAM。',
    source: 'Makefile', reference: '/CODE_GUIDE.html', referenceLabel: '程式碼導覽',
  },
  {
    title: 'OpenSBI', icon: 'firmware', subtitle: 'M-mode 韌體',
    summary: 'OpenSBI 在 M-mode 準備 RISC-V 執行環境，再將控制權交給 S-mode。',
    detail: '磁碟路徑交給 0x20000000 的 flash 載入器；直接路徑交給 0x80200000 的核心入口。a0 帶入 hart ID，a1 帶入 FDT 位址。',
    source: 'arch/riscv/entry.S', reference: '/SPEC-01-user-syscalls.html', referenceLabel: 'Spec 01 · S-mode 與 U-mode',
  },
  {
    title: 'Flash Bootloader', icon: 'boot', subtitle: 'S-mode · 0x20000000',
    summary: '載入器準備自己的 stack 與 BSS，接著輪詢 VirtIO MMIO 插槽，尋找可讀取的 block device。',
    detail: 'boot_main 驗證 WUFS0002 的配置與 journal，定位核心檔案後檢查 ELF，再將可載入區段複製到 RAM。交接前重設 VirtIO 裝置，並保留原始 hart ID 與 FDT。',
    source: 'bootloader/boot.c', reference: '/CODE_GUIDE.html', referenceLabel: '程式碼導覽',
  },
  {
    title: '讀取 WUFS Disk', icon: 'storage', subtitle: '/boot/kernel.elf',
    summary: '這是載入器的磁碟讀取階段。WUFS0002 提供 /boot/kernel.elf，其中存放主機建置後去除符號的核心 ELF。',
    detail: '載入器讀取目錄、inode 和資料區塊。找不到核心、ELF 無效或 journal 待重播時會停止開機。待重播映像需用直接開機交由核心復原。',
    source: 'bootloader/boot.c', reference: '/SPEC-32-wufs-redo-journal.html#新磁碟與-pending-recovery-操作', referenceLabel: 'Spec 32 · WUFS journal 與復原',
  },
  {
    title: 'Kernel Entry', icon: 'ram', subtitle: '_start · 0x80200000',
    summary: '兩條路徑在同一個核心入口匯合。entry.S 先遮罩中斷，將 sp 指向啟動堆疊，再清零 BSS。',
    detail: '_start 隨後呼叫 kernel_main(hart_id, fdt)，進入 C 程式。若 kernel_main 返回，入口程式會進入 WFI 迴圈。',
    source: 'arch/riscv/entry.S', reference: '/SPEC-01-user-syscalls.html', referenceLabel: 'Spec 01 · 核心入口',
  },
  {
    title: 'Kernel Initialization', icon: 'kernel', subtitle: 'kernel_main',
    summary: '核心安裝 trap 入口、顯示 UART 開機動畫，再依 FDT 探測 PLIC 和 VirtIO 裝置，建立 Sv39 映射。',
    detail: '接著初始化 TTY，並在 block 裝置可用時嘗試掛載 WUFS。核心註冊磁碟 IRQ、啟用中斷與計時器、初始化行程後，才呼叫 process_start。',
    source: 'kernel/boot/main.c', reference: '/SPEC-04-boot-splash.html', referenceLabel: 'Spec 04 · UART 開機動畫',
  },
  {
    title: 'User Shell', icon: 'terminal', subtitle: 'U-mode · wuwaos/>',
    summary: 'process_start 切入排程器，首批可執行工作包含 PID 1 的 UART shell。Shell 進入 U-mode 後顯示 wuwaos/>。',
    detail: 'Shell 以系統呼叫存取終端與檔案。在圖形入口，桌面 Terminal 另外建立獨立 TTY 的 shell，UART shell 仍可使用。',
    source: 'kernel/proc/process.c', reference: '/SPEC-02-process-lifecycle.html', referenceLabel: 'Spec 02 · 行程生命週期',
  },
] as const

const routes = {
  disk: {
    label: '磁碟開機',
    command: 'make run-disk',
    description: 'OpenSBI 先交給 flash 載入器。載入器從 WUFS 取得核心並載入 RAM，再跳到 _start。',
    stages: [0, 1, 2, 3, 4, 5, 6],
  },
  direct: {
    label: '直接開機',
    command: 'make run / make run-gui',
    description: 'QEMU 先用 -kernel 載入核心。OpenSBI 直接交給 _start；核心初始化時仍可掛載 WUFS。',
    stages: [0, 1, 4, 5, 6],
  },
} as const

type BootRoute = keyof typeof routes
const route = ref<BootRoute>('disk')
const position = ref(0)
const playing = ref(false)
const sequence = computed<readonly number[]>(() => routes[route.value].stages)
const stageIndex = computed(() => sequence.value[position.value])
const stage = computed(() => stages[stageIndex.value])
const last = computed(() => position.value === sequence.value.length - 1)
let playbackTimer: ReturnType<typeof setTimeout> | undefined

function pause() {
  clearTimeout(playbackTimer)
  playing.value = false
}

function advance() {
  playbackTimer = setTimeout(() => {
    position.value += 1
    if (last.value) pause()
    else advance()
  }, 3800)
}

function togglePlayback() {
  if (playing.value) {
    pause()
    return
  }
  if (last.value) position.value = 0
  playing.value = true
  advance()
}

function selectRoute(nextRoute: BootRoute) {
  if (route.value === nextRoute) return
  pause()
  route.value = nextRoute
  position.value = 0
}

function selectPosition(nextPosition: number) {
  if (nextPosition < 0 || nextPosition >= sequence.value.length) return
  pause()
  position.value = nextPosition
}

onBeforeUnmount(pause)
</script>

<template>
  <section class="boot-timeline" aria-label="RISC-V 開機路徑圖">
    <div class="boot-controls" role="group" aria-label="開機方式">
      <button
        v-for="(option, id) in routes"
        :key="id"
        type="button"
        :aria-pressed="route === id"
        @click="selectRoute(id)"
      >{{ option.label }}</button>
    </div>

    <p class="boot-route-summary">
      <code>{{ routes[route].command }}</code>
      <span>{{ routes[route].description }}</span>
    </p>

    <div class="boot-body">
      <ol class="boot-stages" aria-label="開機階段">
        <li
          v-for="(item, index) in stages"
          :key="item.title"
          :class="{
            'boot-current': index === stageIndex,
            'boot-skipped': !sequence.includes(index),
            'boot-disk-read': index === 3,
            'boot-direct-entry': route === 'direct' && index === 4,
          }"
        >
          <button
            type="button"
            :aria-current="index === stageIndex ? 'step' : undefined"
            :disabled="!sequence.includes(index)"
            @click="selectPosition(sequence.indexOf(index))"
          >
            <span class="boot-stage-icon"><ArchitectureIcon :name="item.icon" /></span>
            <span class="boot-stage-text">
              <strong>{{ item.title }}</strong>
              <span>{{ sequence.includes(index) ? item.subtitle : '直接開機略過' }}</span>
            </span>
          </button>
        </li>
      </ol>

      <aside aria-label="目前階段解說">
        <div class="boot-controls boot-playback" role="group" aria-label="流程播放">
          <button type="button" :disabled="position === 0" @click="selectPosition(position - 1)">上一步</button>
          <button type="button" class="boot-play" @click="togglePlayback">{{ playing ? '暫停' : last ? '重播' : '播放' }}</button>
          <button type="button" :disabled="last" @click="selectPosition(position + 1)">下一步</button>
        </div>
        <div class="boot-stage-detail">
          <p class="boot-step-status">步驟 {{ position + 1 }} / {{ sequence.length }}</p>
          <h3 aria-live="polite">{{ stage.title }}</h3>
          <p>{{ stage.summary }}</p>
          <p>{{ stage.detail }}</p>
          <ul class="boot-sources" aria-label="原始碼與規格">
            <li><a :href="sourceRoot + stage.source"><code>{{ stage.source }}</code></a></li>
            <li><a :href="withBase(stage.reference)">{{ stage.referenceLabel }}</a></li>
          </ul>
        </div>
      </aside>
    </div>
  </section>
</template>

<style scoped>
.boot-timeline {
  --boot-step-height: 4.5rem;
  --boot-icon-size: 3.25rem;
  margin-block: 1.5rem 2rem;
  color: var(--os-ink);
  font-size: 1rem;
  line-height: 1.7;
}

.boot-controls {
  display: flex;
  flex-wrap: wrap;
  gap: .5rem;
}

.boot-playback { margin-bottom: .75rem; }

.boot-controls button {
  min-height: 44px;
  padding: .35rem .7rem;
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-control);
  background: var(--os-panel);
  font-size: .875rem;
  font-weight: 600;
}

.boot-controls button[aria-pressed="true"] {
  border-color: var(--os-accent);
  background: var(--os-accent-soft);
  color: var(--os-accent);
}

.boot-controls .boot-play {
  border-color: var(--os-accent);
  background: var(--os-accent);
  color: var(--os-on-accent);
}

.boot-controls button:not(:disabled):hover { box-shadow: inset 0 0 0 1px var(--os-accent); }
.boot-controls button:disabled { color: var(--os-muted); border-style: dashed; cursor: default; }
.boot-timeline button:focus-visible { outline: 2px solid var(--os-focus); outline-offset: 2px; }

.boot-timeline .boot-route-summary {
  display: flex;
  flex-wrap: wrap;
  align-items: baseline;
  gap: .35rem .8rem;
  margin: .85rem 0 1.25rem;
  color: var(--os-muted);
  font-size: .9375rem;
}

.boot-timeline code { font-size: .875rem; overflow-wrap: anywhere; }

.boot-body {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1.2fr);
  align-items: start;
  gap: 1.15rem;
}

.boot-timeline .boot-stages {
  isolation: isolate;
  margin: 0;
  padding: 0;
  list-style: none;
}

.boot-stages > li {
  position: relative;
  margin: 0;
  padding-inline-start: .75rem;
}

.boot-stages > li + li::before {
  content: "";
  position: absolute;
  inset-block: -50% 50%;
  inset-inline-start: calc(.75rem + var(--boot-icon-size) / 2);
  width: 2px;
  background: var(--os-line);
}

.boot-stages > li + li.boot-current::before {
  background: var(--os-accent);
  transform-origin: center top;
  animation: boot-connect 450ms cubic-bezier(.2, 0, .38, .9) both;
}

.boot-stages > li.boot-direct-entry::before {
  inset-block-start: calc(50% - var(--boot-step-height) * 3);
  inset-inline-start: .2rem;
  width: calc(.55rem + var(--boot-icon-size) / 2);
  border: solid var(--os-line);
  border-width: 2px 0 2px 2px;
  border-radius: 5px 0 0 5px;
  background: transparent;
}

.boot-stages > li.boot-direct-entry.boot-current::before {
  border-color: var(--os-accent);
  background: transparent;
}

.boot-stages > li.boot-skipped::before { content: none; }

.boot-stages button {
  position: relative;
  z-index: 1;
  display: grid;
  grid-template-columns: var(--boot-icon-size) minmax(0, 1fr);
  align-items: center;
  gap: .7rem;
  width: 100%;
  min-height: var(--boot-step-height);
  padding: .35rem .4rem .35rem 0;
  border-radius: var(--os-radius-panel);
  text-align: start;
  line-height: 1.45;
}

.boot-stage-icon {
  display: grid;
  place-items: center;
  width: var(--boot-icon-size);
  height: var(--boot-icon-size);
  border: 1px solid var(--os-line);
  border-radius: 12px;
  background: var(--os-panel);
  color: var(--os-accent);
}

.boot-stage-icon :deep(.arch-icon) { width: 34px; height: 34px; }
.boot-stage-text { min-width: 0; overflow-wrap: anywhere; }
.boot-stage-text strong { display: block; font-weight: 650; }
.boot-stage-text span { color: var(--os-muted); font-size: .875rem; }
.boot-stages button:not(:disabled):hover { background: var(--os-soft); }
.boot-current button, .boot-current button:not(:disabled):hover { background: var(--os-accent-soft); }
.boot-current .boot-stage-icon { border-color: var(--os-accent); }
.boot-skipped .boot-stage-icon { color: var(--os-muted); border-style: dashed; }
.boot-skipped button { color: var(--os-muted); cursor: default; }
.boot-disk-read .boot-stage-icon { border-style: dashed; }

.boot-stage-detail {
  min-width: 0;
  padding: 1.05rem;
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-panel);
  background: var(--os-panel);
}

.boot-stage-detail .boot-step-status {
  margin: 0 0 .4rem;
  color: var(--os-accent);
  font-size: .875rem;
  font-weight: 650;
}

.boot-stage-detail h3 { margin: 0; font-size: 1.25rem; }
.boot-stage-detail p { margin: .65rem 0 0; }
.boot-stage-detail .boot-sources {
  display: flex;
  flex-wrap: wrap;
  gap: .35rem 1rem;
  list-style: none;
  margin: .9rem 0 0;
  padding: .75rem 0 0;
  border-top: 1px solid var(--os-line);
  font-size: .875rem;
}
.boot-sources li { min-width: 0; margin: 0; }

@keyframes boot-connect {
  from { transform: scaleY(0); }
  to { transform: scaleY(1); }
}

@media (max-width: 700px) {
  .boot-body { grid-template-columns: minmax(0, 1fr); }
}

@media (prefers-reduced-motion: reduce) {
  .boot-stages > li + li.boot-current::before { animation: none; }
}
</style>
