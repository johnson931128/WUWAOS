<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from 'vue'
import { withBase } from 'vitepress'

type Step = { node: number; title: string; why: string; state: string; field: string; source: string }
const common: Step[] = [
  { node: 0, title: '使用者程式執行中', why: '程式在 U-mode 執行，透過系統呼叫請求核心服務；缺頁與計時器中斷也可能讓 CPU 進入同一入口。', state: '尚未進入陷阱入口', field: '來源模式 = U-mode', source: 'user/lib/api.c' },
  { node: 1, title: '保存原本的執行狀態', why: 'CPU 記錄陷阱 CSR，trap_entry 再透過 sscratch 切到核心堆疊，保存一般暫存器與 CSR。這些保存工作並非全部由硬體完成。', state: 'U-mode → S-mode；建立 288-byte TrapFrame', field: 'sscratch → 核心 stack；核心執行期間清為 0', source: 'arch/riscv/trap.S' },
]
const routes = {
  syscall: { label: '系統呼叫', spec: '/SPEC-01-user-syscalls.html', specLabel: 'Spec 01 · 基本系統呼叫', steps: [
    { ...common[0], why: 'user/lib/api.c 的 syscall3 將呼叫編號放入 a7，參數放入 a0–a2，再執行 ecall。這是 WUWAOS 自訂 ABI。', field: 'a7 = 呼叫編號；a0–a2 = 參數' }, common[1],
    { node: 2, title: '辨識使用者 ecall', why: 'trap_handler 確認來源為使用者模式且 scause = 8。ecall 固定為 4 bytes，因此先推進返回位置，再交給 syscall_dispatch。', state: '登記核心呼叫區段；開始分派服務', field: 'SPP = 0；scause = 8；sepc += 4', source: 'arch/riscv/trap.c' },
    { node: 2, title: '服務完成後寫回結果', why: 'syscall_dispatch 依編號呼叫服務，將結果存入 frame->a0。若服務阻塞，會保留原核心呼叫堆疊，恢復後才回傳真正結果。', state: '服務完成 → thread_resume → 核心呼叫返回邊界', field: 'frame->a0 = 服務回傳值', source: 'kernel/syscall/syscall.c' },
    { node: 3, title: '還原並繼續下一條指令', why: '可正常返回的呼叫會由 trap_restore 還原 CSR 與一般暫存器，以 sret 回到 U-mode。exit 不走正常使用者返回；pending kill 也可能在返回邊界終止行程。', state: 'S-mode → U-mode（正常返回路徑）', field: '返回位置 = ecall 後的指令', source: 'arch/riscv/trap.S' },
  ] as Step[] },
  fault: { label: '合法缺頁', spec: '/SPEC-10-demand-paging.html', specLabel: 'Spec 10 · 按需分頁', steps: [
    { ...common[0], why: '使用者存取合法但尚未建立有效映射的頁面，可能觸發取指令、讀取或寫入缺頁。此情境只展示可成功補頁的路徑。', field: '取指令／讀取／寫入 → 缺頁' }, common[1],
    { node: 2, title: '檢查位址與權限', why: 'trap_handler 將頁表、stval、scause 與原 sepc 交給 vm_handle_fault，判斷是否為合法區域與允許的存取。', state: '進入缺頁處理；尚未改動返回指令位置', field: 'scause = 12 / 13 / 15；stval = 缺頁位址', source: 'arch/riscv/trap.c' },
    { node: 2, title: '建立可用映射', why: 'VM 依頁面來源載入映像、配置零頁或從 swap 換入，成功後更新映射。資源不足、非法存取或 I/O 失敗可能使此路徑無法完成，不能一律視為補頁成功。', state: 'vm_handle_fault 回傳 0 → thread_resume', field: 'sepc 保持原值；不套用 ecall 的 +4', source: 'kernel/mm/vm.c' },
    { node: 3, title: '重試同一條指令', why: '補頁成功且允許返回時，trap_restore 還原 frame，sret 回到原指令重新執行；失敗則走行程結束路徑，不畫成成功返回。', state: 'S-mode → U-mode；重試原存取', field: '返回位置 = 原 sepc', source: 'arch/riscv/trap.c' },
  ] as Step[] },
  timer: { label: '計時器中斷', spec: '/topics/scheduling.html', specLabel: '計時器與 RR 排程', steps: [
    { ...common[0], why: 'U-mode 執行期間收到監督模式計時器中斷。此情境是非同步中斷，不是使用者主動呼叫 syscall。', field: 'scause = (1 << 63) | 5' }, common[1],
    { node: 2, title: '確認並處理計時器', why: 'trap_handler 辨識 U-mode 的 supervisor timer interrupt，呼叫 timer_interrupt，再透過 thread_preempt 將目前 Thread 交回排程流程。', state: '處理計時器 pending；Running → Ready', field: 'sepc 與 a0 保持原值', source: 'arch/riscv/trap.c' },
    { node: 2, title: '等待排程器再次選中', why: '目前 Thread 讓出 CPU；thread_resume 可能暫停這條核心呼叫堆疊，待排程器再次選中後，才沿原本的呼叫返回。圖中顯示的是這條 Thread 的生命週期。', state: '排程器可先執行其他 Ready Thread', field: 'Thread 保存執行狀態；Process 持有資源', source: 'kernel/proc/process.c' },
    { node: 3, title: '恢復被中斷的執行', why: '目前 Thread 再次執行並通過返回邊界後，還原自己的 TrapFrame。中斷已保存可恢復的 sepc，不加 4，也不覆寫 a0 為 syscall 結果。', state: 'S-mode → U-mode（允許返回時）', field: '返回位置 = 保存的 sepc', source: 'arch/riscv/trap.S' },
  ] as Step[] },
}
type Route = keyof typeof routes
const route = ref<Route>('syscall')
const position = ref(0)
const playing = ref(false)
const sequence = computed(() => routes[route.value])
const step = computed(() => sequence.value.steps[position.value])
const nodes = ['使用者程式', 'trap_entry', '核心處理', '還原與 sret']
const paths = ['M5 5h14v14H5z M8 9l3 3-3 3m5 0h3', 'M12 3v18m-5-5 5 5 5-5M5 4h14', 'M8 8h8v8H8z M8 3v3m8-3v3M8 18v3m8-3v3M3 8h3m-3 8h3m12-8h3m-3 8h3', 'M4 12h16m-6-6 6 6-6 6']
let timer: ReturnType<typeof setTimeout> | undefined
function pause() { clearTimeout(timer); playing.value = false }
function select(next: number) { pause(); position.value = next }
function changeRoute(next: Route) { pause(); route.value = next; position.value = 0 }
function schedule() {
  timer = setTimeout(() => {
    if (position.value < sequence.value.steps.length - 1) position.value++
    if (position.value === sequence.value.steps.length - 1) pause()
    else schedule()
  }, 6500)
}
function toggle() {
  if (playing.value) return pause()
  if (position.value === sequence.value.steps.length - 1) position.value = 0
  playing.value = true
  schedule()
}
onBeforeUnmount(pause)
</script>

<template>
  <section class="trap-flow" aria-label="WUWAOS 陷阱流程解析">
    <div class="route-picker" role="group" aria-label="選擇解析情境">
      <button v-for="(item, key) in routes" :key="key" :aria-pressed="route === key" @click="changeRoute(key)">{{ item.label }}</button>
    </div>
    <p class="diagram-note">依原始碼整理的教學示意；不是即時 OS 狀態或 QEMU Trace。</p>
    <ol class="flow-map" aria-label="固定陷阱架構">
      <li v-for="(name, i) in nodes" :key="name" :class="{ active: step.node === i }" :aria-current="step.node === i ? 'step' : undefined">
        <svg viewBox="0 0 24 24" width="32" height="32" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path :d="paths[i]" /></svg>
        <strong>{{ name }}</strong><span>{{ ['U-mode', 'S-mode · 保存 frame', 'S-mode · 分派原因', '允許返回時進入 U-mode'][i] }}</span>
        <span v-if="step.node === i" class="current-label">目前階段</span>
      </li>
    </ol>
    <p class="return-path">返回關係：還原與 sret → 使用者程式；系統呼叫繼續下一條指令，成功補頁重試原指令。</p>
    <div class="playback" role="group" aria-label="步驟控制">
      <button :disabled="position === 0" @click="select(position - 1)">上一步</button>
      <button :aria-pressed="playing" @click="toggle">{{ playing ? '暫停' : '播放' }}</button>
      <button :disabled="position === sequence.steps.length - 1" @click="select(position + 1)">下一步</button>
      <button @click="select(0)">重設</button>
      <span>步驟 {{ position + 1 }} / {{ sequence.steps.length }}</span>
    </div>
    <div class="step-reading">
      <p class="step-heading">{{ sequence.label }} · 步驟 {{ position + 1 }}</p>
      <h3>{{ step.title }}</h3>
      <p>{{ step.why }}</p>
      <dl><div><dt>狀態變化</dt><dd>{{ step.state }}</dd></div><div><dt>關鍵欄位</dt><dd><code>{{ step.field }}</code></dd></div></dl>
      <div class="references"><a :href="'https://github.com/johnson931128/WUWAOS/blob/main/' + step.source">原始碼：{{ step.source }}</a><a :href="withBase(sequence.spec)">{{ sequence.specLabel }}</a></div>
    </div>
    <p class="diagram-note">每步停留 6.5 秒，可隨時暫停或單步閱讀。外部 IRQ 與錯誤終止路徑見下方正文；本圖不將它們合併成正常返回。</p>
  </section>
</template>

<style scoped>
.trap-flow { margin: 28px 0 40px; padding: 24px; border: 1px solid var(--vp-c-divider); border-radius: 18px; background: var(--vp-c-bg-soft); }
.route-picker, .playback, .references { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; }
button { min-height: 44px; padding: 8px 14px; border: 1px solid var(--vp-c-divider); border-radius: 9px; background: var(--vp-c-bg); color: var(--vp-c-text-1); font: inherit; font-size: 16px; cursor: pointer; transition: background-color 150ms, border-color 150ms; }
button:hover:not(:disabled) { border-color: var(--vp-c-brand-1); }
button[aria-pressed="true"] { background: var(--vp-c-brand-soft); border-color: var(--vp-c-brand-1); font-weight: 600; }
button:disabled { opacity: .5; cursor: default; }
button:focus-visible, a:focus-visible { outline: 2px solid var(--vp-c-brand-1); outline-offset: 4px; }
.trap-flow .diagram-note, .trap-flow .return-path { font-size: 14px; line-height: 1.7; color: var(--vp-c-text-2); }
.trap-flow .flow-map { list-style: none; padding: 0; margin: 24px 0 12px; display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 20px; }
.flow-map li { position: relative; margin: 0; min-height: 168px; padding: 20px 12px; display: flex; flex-direction: column; align-items: center; gap: 8px; text-align: center; border: 1px solid var(--vp-c-divider); border-radius: 12px; background: var(--vp-c-bg); }
.flow-map li:not(:last-child)::after { content: '→'; position: absolute; right: -19px; top: 65px; color: var(--vp-c-text-2); font-size: 20px; }
.flow-map li.active { background: var(--vp-c-brand-soft); border-color: var(--vp-c-brand-1); }
.flow-map strong { font-size: 16px; }
.flow-map span { font-size: 14px; line-height: 1.5; }
.current-label { font-weight: 600; }
.playback { margin: 24px 0; }
.playback span { font-size: 14px; font-variant-numeric: tabular-nums; }
.step-reading { padding: 24px; border-radius: 12px; background: var(--vp-c-bg); min-height: 350px; }
.trap-flow .step-heading { margin: 0; font-size: 14px; color: var(--vp-c-text-2); }
.trap-flow h3 { margin: 8px 0 16px; font-size: 22px; line-height: 1.5; }
.step-reading p, .step-reading dl { font-size: 16px; line-height: 1.8; }
dl div { display: grid; grid-template-columns: 5em minmax(0, 1fr); gap: 12px; margin: 12px 0; }
dt { font-weight: 600; } dd { margin: 0; overflow-wrap: anywhere; }
.step-reading code { font-size: 14px; white-space: normal; }
.references { margin-top: 20px; align-items: flex-start; flex-direction: column; font-size: 14px; line-height: 1.7; overflow-wrap: anywhere; }
@media (max-width: 760px) { .trap-flow .flow-map { grid-template-columns: 1fr; gap: 24px; } .flow-map li { min-height: 130px; } .flow-map li:not(:last-child)::after { content: '↓'; right: calc(50% - 10px); top: auto; bottom: -25px; } }
@media (max-width: 480px) { .trap-flow { padding: 16px; } .step-reading { padding: 16px; } dl div { grid-template-columns: 1fr; gap: 4px; } }
@media (prefers-reduced-motion: reduce) { button { transition: none; } }
</style>
