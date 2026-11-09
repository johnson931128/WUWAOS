<script setup lang="ts">
import { withBase } from 'vitepress'
import { computed, ref } from 'vue'
import DiagramFrame from './DiagramFrame.vue'

const modules = [
  { id: 'cpu', label: 'CPU', eyebrow: 'RV64 · 單核心', x: 42, y: 42, w: 205, h: 94, role: 'cpu', definition: 'QEMU virt 提供一顆 RV64 CPU。CPU 執行 S-mode 核心與 U-mode 使用者程式；它是硬體資源，不是 kernel 的子模組。', source: 'Makefile：-machine virt -smp 1', href: '/topics/getting-started.html', chapter: '平台與建置' },
  { id: 'memory', label: '實體記憶體', eyebrow: 'RAM · 128 MiB', x: 272, y: 42, w: 235, h: 94, role: 'memory', definition: 'QEMU 配置 128 MiB RAM。核心管理 4 KiB 實體頁，Sv39 為行程提供虛擬位址轉譯。', source: 'kernel/mm/page.c · kernel/mm/vm.c', href: '/topics/memory.html', chapter: 'Sv39 與實體頁' },
  { id: 'io', label: 'VirtIO 裝置', eyebrow: 'GPU · input · block', x: 532, y: 42, w: 235, h: 94, role: 'io', definition: 'QEMU virt 可提供 VirtIO GPU、tablet／keyboard 與 block 裝置。核心透過 FDT 探測 MMIO，再由對應 driver 管理。', source: 'drivers/virtio · kernel/boot/main.c', href: '/topics/io.html', chapter: '輸入輸出與 Pipe' },
  { id: 'storage', label: '持久磁碟與 Swap', eyebrow: 'VirtIO block · 512-byte sectors', x: 792, y: 42, w: 256, h: 94, role: 'storage', definition: 'WUFS 磁碟管理檔案與 redo journal。直接開機可以附加獨立 Swap 磁碟，保存被換出的使用者頁。磁碟開機使用一顆 WUFS 磁碟。', source: 'kernel/fs/fs.c · kernel/mm/swap.c · drivers/virtio/block.c', href: '/topics/filesystem.html', chapter: 'WUFS 檔案系統' },
  { id: 'opensbi', label: 'OpenSBI 與開機載入', eyebrow: 'M-mode 韌體 · S-mode 載入器', x: 350, y: 177, w: 390, h: 62, role: 'kernel', definition: 'OpenSBI 完成機器模式初始化。磁碟開機時，OpenSBI 先交接給 flash 裡的載入器，由它從 WUFS 讀取核心。直接開機時，QEMU 預先載入核心，OpenSBI 直接交接給它。', source: 'Makefile · bootloader/boot.c · arch/riscv/entry.S', href: '/topics/boot.html', chapter: '開機流程' },
  { id: 'kernel', label: 'WUWAOS 核心與桌面服務', eyebrow: 'S-mode · 靜態連結核心', x: 204, y: 278, w: 680, h: 127, role: 'kernel', definition: '核心在 S-mode 執行。kernel_main 安裝 trap、探測裝置、初始化 VM／TTY／FS／timer／process，最後啟動行程。可選圖形桌面、renderer 與 TTY service 也由核心端的 ui/ 與 kernel/ 程式提供，不是 U-mode 桌面應用。', source: 'kernel/boot/main.c · ui/desktop/desktop.c · kernel/io/tty.c', href: '/CODE_GUIDE.html#ui-與-kernel-的資料通道', chapter: '程式碼架構' },
  { id: 'process', label: '使用者行程', eyebrow: 'U-mode · shell / app / fan / workstate', x: 311, y: 455, w: 467, h: 91, role: 'process', definition: 'shell、demo app、fan 與 workstate 是獨立的 U-mode 映像。它們透過 ecall 進入自訂 syscall ABI，由核心代辦操作。', source: 'kernel/exec/user_image.S · user/lib/api.c · kernel/syscall/syscall.c', href: '/topics/applications.html', chapter: 'Shell 與 fan 編輯器' },
  { id: 'filesystem', label: 'WUFS 檔案系統', eyebrow: 'KERNEL SUBSYSTEM · STORAGE', x: 642, y: 354, w: 94, h: 31, role: 'storage', definition: 'WUFS 是 kernel/fs/ 內的檔案系統實作，將路徑解析到 inode 與磁區，並以 redo journal 支援有界交易。block driver 才負責 VirtIO 磁碟 I/O。', source: 'kernel/fs/fs.c · kernel/fs/path.c · drivers/virtio/block.c', href: '/topics/filesystem.html', chapter: 'WUFS 檔案系統' },
]
const selectedId = ref('kernel')
const selected = computed(() => modules.find((module) => module.id === selectedId.value) ?? modules[0])
function choose(id: string) { selectedId.value = id }
function onKey(event: KeyboardEvent, id: string) {
  if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); choose(id) }
}
</script>

<template>
  <section class="system-overview" aria-labelledby="system-overview-title">
    <div class="overview-heading">
      <div>
        <p class="overview-kicker">SYSTEM MAP · SOURCE-BASED</p>
        <h3 id="system-overview-title">從虛擬硬體到使用者程式</h3>
      </div>
      <p>選取圖中的模組，查看它的角色、實際來源與對應章節。</p>
    </div>
    <DiagramFrame
      title="WUWAOS QEMU 系統分層圖"
      description="選取圖中模組查看定義、實際來源與對應章節。箭頭呈現韌體交接與 ecall 邊界，不代表一次 QEMU 執行追蹤。"
      kind="SYSTEM MAP · SOURCE-BASED"
      :zoomable="true"
    >
      <svg class="system-svg" viewBox="0 0 1090 580" role="group" aria-labelledby="system-svg-title system-svg-desc">
        <title id="system-svg-title">WUWAOS QEMU 系統分層架構</title>
        <desc id="system-svg-desc">QEMU 虛擬硬體提供 CPU、RAM、VirtIO 裝置和磁碟。OpenSBI 交接給 S-mode 的磁碟載入器，或直接交接給核心。核心內含桌面服務，並啟動以 ecall 使用核心服務的 U-mode 行程。</desc>
        <defs>
          <marker id="system-arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 z" fill="currentColor" /></marker>
        </defs>
        <rect class="qemu-boundary" x="16" y="15" width="1058" height="139" rx="18" />
        <text class="boundary-label" x="35" y="35">QEMU VIRT · 虛擬硬體</text>
        <g v-for="item in modules.slice(0, 4)" :key="item.id" class="module" :class="[`role-${item.role}`, { selected: selectedId === item.id }]" role="button" tabindex="0" :aria-label="`${item.label}：查看定義與來源`" :aria-pressed="selectedId === item.id" @click="choose(item.id)" @keydown="onKey($event, item.id)">
          <rect class="module-box" :x="item.x" :y="item.y" :width="item.w" :height="item.h" rx="12" />
          <text class="module-eyebrow" :x="item.x + 15" :y="item.y + 29">{{ item.eyebrow }}</text>
          <text class="module-title" :x="item.x + 15" :y="item.y + 59">{{ item.label }}</text>
          <text class="module-mark" :x="item.x + item.w - 22" :y="item.y + 78">↗</text>
        </g>
        <path class="flow flow-boot" d="M 545 155 V 169" marker-end="url(#system-arrow)" />
        <text class="flow-label" x="558" y="167">韌體交接</text>
        <g class="module role-kernel" :class="{ selected: selectedId === 'opensbi' }" role="button" tabindex="0" aria-label="OpenSBI：查看定義與來源" :aria-pressed="selectedId === 'opensbi'" @click="choose('opensbi')" @keydown="onKey($event, 'opensbi')">
          <rect class="module-box" x="350" y="177" width="390" height="62" rx="12" />
          <text class="module-eyebrow" x="368" y="202">M-MODE · FIRMWARE</text><text class="module-title" x="368" y="225">OpenSBI</text>
        </g>
        <path class="flow flow-boot" d="M 545 240 V 266" marker-end="url(#system-arrow)" />
        <text class="flow-label" x="558" y="259">_start → kernel_main</text>
        <g class="module role-kernel" :class="{ selected: selectedId === 'kernel' }" role="button" tabindex="0" aria-label="WUWAOS 核心與桌面服務：查看定義與來源" :aria-pressed="selectedId === 'kernel'" @click="choose('kernel')" @keydown="onKey($event, 'kernel')">
          <rect class="module-box kernel-box" x="204" y="278" width="680" height="127" rx="14" />
          <text class="module-eyebrow" x="226" y="305">SUPERVISOR · S-MODE</text><text class="module-title" x="226" y="337">WUWAOS 核心</text>
          <rect class="kernel-chip" x="226" y="354" width="137" height="31" rx="7"/><text class="chip-label" x="237" y="375">VM · Sv39 · Swap</text>
          <rect class="kernel-chip" x="373" y="354" width="116" height="31" rx="7"/><text class="chip-label" x="384" y="375">Process · RR</text>
          <rect class="kernel-chip" x="499" y="354" width="132" height="31" rx="7"/><text class="chip-label" x="509" y="375">syscall · I/O</text>
          <rect class="kernel-chip" x="744" y="354" width="120" height="31" rx="7"/><text class="chip-label" x="754" y="375">UI · desktop</text>
        </g>
        <g class="module role-storage" :class="{ selected: selectedId === 'filesystem' }" role="button" tabindex="0" aria-label="WUFS 檔案系統：查看定義與來源" :aria-pressed="selectedId === 'filesystem'" @click.stop="choose('filesystem')" @keydown.stop="onKey($event, 'filesystem')">
          <rect class="fs-chip" x="642" y="354" width="94" height="31" rx="7"/><text class="chip-label" x="650" y="375">FS · WUFS</text>
        </g>
        <path class="flow flow-call" d="M 545 407 V 444" marker-end="url(#system-arrow)" />
        <text class="flow-label" x="559" y="432">process_start → U-mode</text>
        <g class="module role-process" :class="{ selected: selectedId === 'process' }" role="button" tabindex="0" aria-label="使用者行程：查看定義與來源" :aria-pressed="selectedId === 'process'" @click="choose('process')" @keydown="onKey($event, 'process')">
          <rect class="module-box" x="311" y="455" width="467" height="91" rx="12" />
          <text class="module-eyebrow" x="331" y="482">USER APPLICATIONS · U-MODE</text><text class="module-title" x="331" y="513">shell　 demo app　 fan　 workstate</text>
          <text class="module-subtitle" x="331" y="534">ecall → trap entry → syscall dispatch</text>
        </g>
        <path class="flow flow-call return" d="M 792 468 C 908 452, 917 378, 872 378" marker-end="url(#system-arrow)" />
        <text class="flow-label return-label" x="915" y="423" transform="rotate(-90 915 423)">系統呼叫進入核心</text>
        <text class="diagram-note" x="23" y="573">圖示為依目前 source 繪製的靜態分層；箭頭表示啟動交接與 syscall 邊界，不代表執行追蹤。</text>
      </svg>
      <template #caption>硬體、韌體、S-mode 核心與 U-mode 程序依目前 source 分層；可在圖框中局部放大／縮小，不會放大整頁。</template>
    </DiagramFrame>
    <div class="module-detail" aria-live="polite" aria-atomic="true">
      <div class="detail-role" :class="`text-${selected.role}`">已選取 · {{ selected.label }}</div>
      <p>{{ selected.definition }}</p>
      <div class="detail-source"><span>Source</span><code>{{ selected.source }}</code></div>
      <a :href="withBase(selected.href)">閱讀{{ selected.chapter }} →</a>
    </div>
    <p class="system-caption">分層依 WUWAOS 目前實作整理；QEMU 的 1280×720 指可選 VirtIO GPU 桌面解析度，並非 QEMU RAM 或 CPU 規格。</p>
  </section>
</template>

<style scoped>
.system-overview { --role: var(--os-kernel); border: 1px solid var(--os-line); border-radius: 1rem; padding: clamp(1rem, 2.5vw, 1.6rem); background: var(--os-canvas); }
.overview-heading { display: flex; align-items: end; justify-content: space-between; gap: 1rem; margin-bottom: .8rem; }
.overview-heading h3 { margin: .2rem 0; font-size: 1.35rem; }
.overview-heading p { max-width: 40ch; margin: 0; color: var(--os-muted); font-size: .9rem; }
.overview-kicker { margin: 0; color: var(--os-memory); font: 700 .7rem var(--vp-font-family-mono); letter-spacing: .12em; }
.system-svg { display: block; width: 100%; min-width: 760px; height: auto; color: var(--os-ink); }
.qemu-boundary { fill: var(--os-soft); stroke: var(--os-line); stroke-width: 1.5; stroke-dasharray: 6 5; }
.boundary-label, .module-eyebrow, .flow-label, .diagram-note { fill: var(--os-muted); font-family: var(--vp-font-family-mono); }
.boundary-label { font-size: 11px; font-weight: 700; }
.module { cursor: pointer; outline: none; }
.module-box { fill: var(--os-panel); stroke: var(--role); stroke-width: 2; transition: stroke-width .15s, fill .15s; }
.role-cpu { --role: var(--os-cpu); }.role-memory { --role: var(--os-memory); }.role-kernel { --role: var(--os-kernel); }.role-process { --role: var(--os-process); }.role-io { --role: var(--os-io); }.role-storage { --role: var(--os-storage); }
.module:hover .module-box, .module:focus-visible .module-box, .module.selected .module-box { stroke-width: 4; fill: color-mix(in srgb, var(--role) 10%, var(--os-panel)); }
.module-eyebrow { font-size: 10px; }.module-title { fill: var(--os-ink); font-size: 17px; font-weight: 700; }.module-subtitle { fill: var(--os-muted); font-size: 12px; }.module-mark { fill: var(--role); font-size: 18px; font-weight: 700; }
.kernel-chip { fill: color-mix(in srgb, var(--os-kernel) 12%, var(--os-panel)); stroke: color-mix(in srgb, var(--os-kernel) 42%, var(--os-line)); }
.fs-chip { fill: color-mix(in srgb, var(--os-storage) 12%, var(--os-panel)); stroke: var(--os-storage); stroke-width: 1.5; }
.chip-label { fill: var(--os-ink); font-size: 12px; }
.flow { fill: none; stroke: var(--os-muted); stroke-width: 2; }.flow-boot { stroke: var(--os-kernel); }.flow-call { stroke: var(--os-process); stroke-dasharray: 5 4; }.flow-label { font-size: 10px; }.return-label { font-size: 10px; }
.diagram-note { font-size: 10px; }.module-detail { display: grid; grid-template-columns: minmax(130px, .35fr) minmax(0, 1.65fr); gap: .2rem 1rem; margin-top: .8rem; padding: 1rem; border: 1px solid var(--os-line); border-radius: .8rem; background: var(--os-panel); }
.detail-role { grid-row: span 2; align-self: start; font-weight: 700; }.text-cpu { color: var(--os-cpu); }.text-memory { color: var(--os-memory); }.text-kernel { color: var(--os-kernel); }.text-process { color: var(--os-process); }.text-io { color: var(--os-io); }.text-storage { color: var(--os-storage); }
.module-detail p { margin: 0; }.detail-source { display: flex; flex-wrap: wrap; align-items: center; gap: .5rem; color: var(--os-muted); font-size: .85rem; }.detail-source code { color: var(--os-ink); overflow-wrap: anywhere; }.module-detail a { grid-column: 2; width: fit-content; }
.system-caption { margin: .6rem 0 0; color: var(--os-muted); font-size: .8rem; }
@media(max-width: 680px) { .overview-heading { display: block; }.overview-heading p { margin-top: .4rem; }.module-detail { grid-template-columns: 1fr; }.detail-role { grid-row: auto; }.module-detail a { grid-column: 1; } }
@media(prefers-reduced-motion: reduce) { .module-box { transition: none; } }
</style>
