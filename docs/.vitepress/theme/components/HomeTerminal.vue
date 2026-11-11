<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from 'vue'

// The console is a teaching visual of one shell command crossing the privilege
// boundary. It is NOT a live QEMU trace. Motion: Motion Prompt #006 (typewriter)
// for the typed command, #154 (spotlight) for the pointer light.
const CMD = 'echo hello'
const OUT = 'hello'

// Motion Prompt #006 numbers: first char at 0.3s, 0.24s per char, scale settle 1.35 -> 1.
const TYPE_START = 0.3
const PER_CHAR = 0.24
const OUT_AFTER = 0.4  // output appears after typing settles
const STAGE_AFTER = 0.45 // caption/stages after output

const typedCount = ref(0)
const phase = ref<'idle' | 'typing' | 'output' | 'stages'>('idle')
const reduced = ref(false)
const spotlight = ref({ x: '50%', y: '32%' })
let timeouts: ReturnType<typeof setTimeout>[] = []
let played = false

function clearTimers() {
  timeouts.forEach(clearTimeout)
  timeouts = []
}

function play() {
  clearTimers()
  if (reduced.value) {
    typedCount.value = CMD.length
    phase.value = 'stages'
    return
  }
  typedCount.value = 0
  phase.value = 'idle'
  const typeEnd = TYPE_START + CMD.length * PER_CHAR
  for (let i = 1; i <= CMD.length; i += 1) {
    timeouts.push(setTimeout(() => { typedCount.value = i; phase.value = 'typing' }, (TYPE_START + i * PER_CHAR) * 1000))
  }
  timeouts.push(setTimeout(() => { phase.value = 'output' }, (typeEnd + OUT_AFTER) * 1000))
  timeouts.push(setTimeout(() => { phase.value = 'stages' }, (typeEnd + OUT_AFTER + STAGE_AFTER) * 1000))
}

function point(event: PointerEvent) {
  const element = event.currentTarget as HTMLElement | null
  if (!element) return
  const rect = element.getBoundingClientRect()
  spotlight.value = { x: `${event.clientX - rect.left}px`, y: `${event.clientY - rect.top}px` }
}
function resetPoint() { spotlight.value = { x: '50%', y: '32%' } }

onMounted(() => {
  reduced.value = window.matchMedia('(prefers-reduced-motion: reduce)').matches
  played = true
  play()
})
onBeforeUnmount(clearTimers)
</script>

<template>
  <div class="wuwa-console" aria-label="echo hello 教學示意控制台（非即時 QEMU 追蹤）"
    :style="{ '--spot-x': spotlight.x, '--spot-y': spotlight.y }"
    @pointermove="point" @pointerleave="resetPoint">
    <span class="wuwa-sr" aria-hidden="false">教學示意：在 wuwaos/> 提示輸入 echo hello，作業系統輸出 hello。此為靜態教學圖，非即時執行追蹤。</span>
    <div class="console-header" aria-hidden="true">
      <div class="console-lights"><i></i><i></i><i></i></div>
      <span>WUWAOS / UART SESSION</span>
      <span class="console-status"><b></b> SAMPLE</span>
    </div>
    <div class="console-demo" aria-hidden="true">
      <div class="console-prompt"><span class="wuwa-ps1">wuwaos/&gt;</span><span v-for="(ch, i) in CMD.slice(0, typedCount)" :key="i" class="wuwa-ch">{{ ch === ' ' ? ' ' : ch }}</span><span v-if="typedCount < CMD.length" class="wuwa-caret"></span><span v-else class="wuwa-caret wuwa-caret-blink"></span></div>
      <div class="console-output" :class="{ 'is-shown': phase === 'output' || phase === 'stages' }">{{ OUT }}</div>
      <div class="console-rule"></div>
      <p class="console-caption" :class="{ 'is-shown': phase === 'stages' }">指令資料如何穿越執行權限邊界？</p>
      <div class="console-stages" :class="{ 'is-shown': phase === 'stages' }">
        <span>U-mode</span><span>&rarr;</span><span>ecall</span>
        <span>&rarr;</span><span>S-mode</span><span>&rarr;</span><span>UART</span>
      </div>
    </div>
    <div class="console-footer">
      <span>DESIGN PREVIEW / NOT A LIVE TRACE</span>
      <button type="button" @click="play" aria-label="重新播放控制台入場動畫">&#8635; 重播</button>
    </div>
  </div>
</template>

<style scoped>
/* Motion Prompt #006 · MIT License (c) 2026 Shohei Ohtaki · https://showheyohtaki.github.io/motion-prompt/#006
   Adapted: 0.24s/char typing with a 1.35 -> 1 scale settle on each glyph; the send-button
   half of the demo is replaced by the WUWAOS "hello" output. Terminal content is original. */
.wuwa-sr { position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px; overflow: hidden; clip: rect(0 0 0 0); white-space: nowrap; border: 0; }
.wuwa-ch { display: inline-block; animation: wuwa-type-pop .28s var(--wuwa-ease-out) both; }
@keyframes wuwa-type-pop { from { transform: scale(1.35); } to { transform: scale(1); } }
.console-output { opacity: 0; transform: translateY(8px); transition: opacity .5s var(--wuwa-ease-out), transform .5s var(--wuwa-ease-out); }
.console-output.is-shown { opacity: 1; transform: none; }
.console-caption, .console-stages { opacity: 0; transform: translateY(8px); transition: opacity .6s var(--wuwa-ease-out), transform .6s var(--wuwa-ease-out); }
.console-caption.is-shown, .console-stages.is-shown { opacity: 1; transform: none; }
.console-caret { display: inline-block; width: 2px; height: 1.12em; background: #5fd6b4; vertical-align: -0.2em; margin-left: .3rem; }
.wuwa-caret-blink { animation: wuwa-caret-blink .94s step-end infinite; }
@keyframes wuwa-caret-blink { 0%, 53% { opacity: 1; } 54%, 100% { opacity: 0; } }
@media (prefers-reduced-motion: reduce) {
  .wuwa-ch { animation: none; }
  .console-output, .console-caption, .console-stages { opacity: 1; transform: none; transition: none; }
  .wuwa-caret-blink { animation: none; }
}
</style>
