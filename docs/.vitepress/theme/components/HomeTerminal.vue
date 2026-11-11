<script setup lang="ts">
import { ref } from 'vue'

const iteration = ref(0)
const spotlight = ref({ x: '50%', y: '30%' })

function point(event: PointerEvent) {
  const element = event.currentTarget as HTMLElement | null
  if (!element) return
  const rect = element.getBoundingClientRect()
  spotlight.value = {
    x: `${event.clientX - rect.left}px`,
    y: `${event.clientY - rect.top}px`,
  }
}

function resetPoint() {
  spotlight.value = { x: '50%', y: '30%' }
}

function replay() {
  iteration.value += 1
}
</script>

<template>
  <div class="wuwa-console" aria-label="echo hello 教學示意控制台"
    :style="{ '--spot-x': spotlight.x, '--spot-y': spotlight.y }"
    @pointermove="point" @pointerleave="resetPoint">
    <div class="console-header">
      <div class="console-lights" aria-hidden="true"><i></i><i></i><i></i></div>
      <span>WUWAOS / UART SESSION</span>
      <span class="console-status"><b aria-hidden="true"></b> SAMPLE</span>
    </div>
    <div :key="iteration" class="console-demo">
      <div class="console-prompt"><span>wuwaos/&gt;</span> echo hello<span class="console-caret" aria-hidden="true"></span></div>
      <div class="console-output">hello</div>
      <div class="console-rule" aria-hidden="true"></div>
      <p class="console-caption">指令資料如何穿越執行權限邊界？</p>
      <div class="console-stages" aria-label="指令輸出路徑概念圖">
        <span>U-mode</span><span aria-hidden="true">→</span><span>ecall</span>
        <span aria-hidden="true">→</span><span>S-mode</span><span aria-hidden="true">→</span><span>UART</span>
      </div>
    </div>
    <div class="console-footer">
      <span>DESIGN PREVIEW / NOT A LIVE TRACE</span>
      <button type="button" @click="replay" aria-label="重新播放控制台入場動畫">↻ 重播</button>
    </div>
  </div>
</template>

<style scoped>
/* Motion Prompt #154 · MIT License (c) 2026 Shohei Ohtaki · https://showheyohtaki.github.io/motion-prompt/#154
   Adapted as pointer-driven 115px radial surface light; terminal content and UI are WUWAOS originals. */
.wuwa-console {
  position: relative; isolation: isolate; overflow: hidden;
  min-width: 0; align-self: center;
  color: #e8f5f1; background: #101d28;
  border: 1px solid rgba(111, 226, 203, .28);
  border-radius: 1rem;
  box-shadow: 0 28px 85px rgba(1, 8, 20, .4), inset 0 1px rgba(255,255,255,.07);
}
.wuwa-console::before {
  content: ""; position: absolute; inset: 0; pointer-events: none; z-index: 0;
  background: radial-gradient(115px circle at var(--spot-x) var(--spot-y), rgba(255,255,255,.15), transparent);
}
.wuwa-console > * { position: relative; z-index: 1; }
.console-header, .console-footer {
  display: flex; align-items: center; justify-content: space-between; gap: .65rem;
  padding: .8rem 1rem; font: 600 .65rem/1.4 var(--vp-font-family-mono);
  letter-spacing: .075em; color: #93afb7;
}
.console-header { border-bottom: 1px solid rgba(187,218,221,.14); background: rgba(255,255,255,.035); }
.console-lights { display: flex; gap: .3rem; }
.console-lights i { width: .52rem; height: .52rem; border-radius: 50%; background: #ec7e72; }
.console-lights i:nth-child(2) { background: #e9bf70; }
.console-lights i:nth-child(3) { background: #64cbb0; }
.console-status { color: #8decc5; white-space: nowrap; }
.console-status b { display:inline-block;width:.38rem;height:.38rem;border-radius:50%;background:currentColor;margin-right:.3rem; }
.console-demo { padding: clamp(1.5rem,3.2vw,2.5rem) clamp(1.1rem,2.6vw,2rem); min-height: 265px; font-family: var(--vp-font-family-mono); }
.console-prompt { color: #dfe7e9; font-size: clamp(.83rem, 1.1vw, 1rem); white-space: nowrap; }
.console-prompt > span:first-child { color: #6de2bc; }
.console-caret { display:inline-block;width:2px;height:1.15em;background:#75dfbd;vertical-align:-.2em;margin-left:.35rem; }
.console-output { color: #f1dcb7; font-size:clamp(1.5rem,2.6vw,2.35rem); margin: .85rem 0 1.3rem; letter-spacing:-.03em; animation: console-appear .55s .52s both; }
.console-rule { height: 1px; background: linear-gradient(90deg,rgba(109,226,188,.5),rgba(255,255,255,.05)); }
.console-caption { margin: 1rem 0 .65rem; font: .78rem/1.5 var(--vp-font-family-base); color: #a6bdc1; }
.console-stages { display:flex;gap:.45rem;align-items:center;flex-wrap:wrap;font-size:.69rem;color:#81e2c1;animation:console-appear .6s .95s both; }
.console-stages span:nth-child(even) { color:#7e979f; }
.console-footer { border-top:1px solid rgba(187,218,221,.14); }
.console-footer button { appearance:none; background:transparent; border:1px solid rgba(174,229,213,.32);border-radius:.45rem;min-height:2rem;padding:.25rem .55rem;color:#d1e9e2;cursor:pointer;font:inherit;letter-spacing:0; }
.console-footer button:hover { background:rgba(109,226,188,.1); }
.console-footer button:focus-visible { outline:2px solid #6de2bc;outline-offset:2px; }
@keyframes console-appear { from { opacity:0; transform:translateY(10px); } to { opacity:1; transform:translateY(0); } }
@media (max-width: 420px) { .console-header, .console-footer { font-size: .55rem; }.console-demo { min-height: 235px; } }
@media (prefers-reduced-motion: reduce) {
 .console-output, .console-stages { animation:none; }
 .console-caret { display:none; }
}
</style>
