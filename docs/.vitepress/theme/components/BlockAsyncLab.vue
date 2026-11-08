<script setup>
import { computed, onBeforeUnmount, ref } from 'vue'

// Each entry is a teaching frame, not a captured execution trace.
const actorLabels = { caller: 'Caller', scheduler: 'Scheduler / poller', device: 'VirtIO device lane' }
const makeFrame = (title, active, pcb, gate, request, event, queue, buffer, narration) =>
  ({ title, active, pcb, gate, request, event, queue, buffer, narration })
const scenarios = [
  { id: 'read', title: 'Read 成功', summary: 'caller 等待 event；scheduler 輪詢完成後，caller 才將 bounce 複製到目的 buffer。', frames: [
    makeFrame('呼叫 block_read', 'caller', 'PROCESS_RUNNING；尚未 WAIT_EVENT', '無 owner', 'IDLE', '尚未註冊 wait', '尚未發布 descriptor chain', 'dst 屬 caller；靜態 bounce 可重用', '同步函式呼叫開始。runtime 中 caller 必須是可等待的 RUNNING 行程。'),
    makeFrame('持有 gate 並提交', 'device', 'PROCESS_RUNNING；wait_reason=WAIT_NONE', 'owner=caller PID；depth=1', 'IN_FLIGHT', '已 snapshot completion generation / sequence', 'header、descriptor[0..2]、avail_ring 位於 BlockDevice', 'READ 的 DMA 目的地是 target->bounce；還沒寫 dst', 'driver 將 request metadata 放在 BlockDevice。VirtIO 寫入持久 bounce，不直接碰 caller buffer。'),
    makeFrame('caller 等待；裝置尚未完成', 'scheduler', 'PROCESS_WAITING；wait_reason=WAIT_EVENT', 'caller 仍持有；跨越 wait 保留', 'IN_FLIGHT', 'KernelEventWait 位於 caller PCB', 'used_ring 尚無新完成項；scheduler 會執行 block_poll()', 'bounce 是 DMA buffer；dst 保持原值', 'caller 的 kernel stack 與 gate 保留。poller 在 scheduler context 執行，不能阻塞。'),
    makeFrame('裝置先寫入 used ring', 'device', 'A: PROCESS_WAITING / WAIT_EVENT；B 可在 PROCESS_RUNNING', 'owner=caller PID', 'IN_FLIGHT（尚未 poll 收取）', '尚未 event；裝置不直接呼叫核心 event API', '裝置更新 used_ring.index / entry；等待下一次 block_poll()', '裝置已將資料 DMA 到 bounce；dst 尚未更新', '裝置完成與 caller 被喚醒是兩個步驟：此 frame 只表示 used ring 已更新。沒有 block 硬體中斷直接喚醒 A。'),
    makeFrame('poller 收取並通知', 'scheduler', 'A: PROCESS_READY；原 wait_reason=WAIT_EVENT', '仍由 A 持有', 'DONE', 'block_poll 驗證後 event_source_notify；PCB wait.result=0', 'used entry 驗證通過，consumed 前進一格', 'bounce 含 512 bytes；尚未複製到 dst', 'scheduler 下次執行 block_poll 才觀察完成、設 DONE、送 event，將 A 轉為 READY。'),
    makeFrame('collect、複製並返回', 'caller', 'PROCESS_RUNNING；wait record 清除', 'A 清理後釋放', 'IDLE', 'A 恢復後取回結果並清除 PCB wait', 'request slot 可重用', '成功 READ 才 copy bounce → dst', 'caller 在 submit/collect 路徑複製資料，設定 IDLE，再釋放 gate 與 cancel-defer，最後返回 0。'),
  ] },
  { id: 'write', title: 'Write 成功', summary: '提交前複製 caller 資料至靜態 bounce；裝置完成後同步呼叫才返回。', frames: [
    makeFrame('呼叫 block_write', 'caller', 'PROCESS_RUNNING；尚未 WAIT_EVENT', '無 owner', 'IDLE', '尚未註冊 wait', 'queue 尚未發布', 'src 在 caller；bounce 可重用', 'block_write 是同步函式；caller 的來源資料目前有效。'),
    makeFrame('複製並提交', 'device', 'PROCESS_RUNNING；wait_reason=WAIT_NONE', 'owner=caller PID；depth=1', 'IN_FLIGHT', 'snapshot completion generation / sequence', 'header + descriptor chain + avail ring 位於 BlockDevice', 'src 已複製到 target->bounce；DMA 不讀 caller src', 'driver 在通知前複製 512 bytes。descriptor 指向靜態欄位，避免 DMA 使用 caller stack buffer。'),
    makeFrame('等待裝置', 'scheduler', 'PROCESS_WAITING；wait_reason=WAIT_EVENT', 'caller 持有', 'IN_FLIGHT', 'KernelEventWait 位於 caller PCB', 'avail 已發布；等待 used ring', 'bounce 必須保留至 collect', '請求雖已提交，block_write 尚未返回。caller 等待 event，CPU 可執行其他行程。'),
    makeFrame('裝置完成，等待 poll', 'device', 'A: PROCESS_WAITING / WAIT_EVENT；B 可 RUNNING', 'caller 持有', 'IN_FLIGHT（未收取）', '尚無 event notification', '裝置更新 used_ring；block_poll 尚未觀察', '裝置讀取 bounce；bounce 仍不能重用', 'VirtIO 更新 used ring 不等同 caller 已喚醒；完成由 scheduler 後續 poll 收取。'),
    makeFrame('poll、collect、返回', 'caller', 'PROCESS_READY → PROCESS_RUNNING；wait 清除', 'caller unwind 時釋放', 'DONE → IDLE', 'poll 驗證 status=0 後通知；caller 恢復取 result=0', 'used entry 已收取', 'bounce 可在清理後重用；不需複製回 caller', 'poller 通知只喚醒 caller；caller 恢復後收取結果、重設 state、釋放 gate，函式才返回 0。'),
  ] },
  { id: 'ioerr', title: '裝置回報 IOERR', summary: '有效 VirtIO status 錯誤完成為 DONE，collect 回 ERR_IO；不 reset。', frames: [
    makeFrame('READ 已提交並等待', 'scheduler', 'PROCESS_WAITING；wait_reason=WAIT_EVENT', 'caller 持有', 'IN_FLIGHT', 'wait record 在 caller PCB', 'avail 已發布', 'DMA 目標 bounce；dst 未更新', '此情境的 used entry 結構有效，但裝置回報 status=1 (IOERR)。'),
    makeFrame('裝置寫入完成項', 'device', 'caller 仍 PROCESS_WAITING / WAIT_EVENT', 'caller 持有', 'IN_FLIGHT（尚未 poll）', '尚未通知 event', 'used_ring 更新，等待 poll 收取', '讀取資料可能不完整；dst 保持原值', '裝置回報 IOERR 不會直接執行 event notification；仍需 scheduler poll。'),
    makeFrame('poll 判定錯誤', 'scheduler', 'caller PROCESS_READY；wait.result=ERR_IO', '仍由 caller 持有', 'DONE', 'poll 收取 status=1 並通知 ERR_IO', 'used entry 驗證通過，consumed 前進', '失敗 READ 不 copy bounce → dst', '這是有效完成的 IOERR，不是 malformed completion。狀態進 DONE，不進 reset。'),
    makeFrame('collect 回傳錯誤', 'caller', 'PROCESS_RUNNING；wait record 清除', 'caller 釋放', 'IDLE', 'caller 取得結果後清除 wait', 'slot 可重用', 'dst 不被失敗 READ 覆蓋', 'block_read 返回 ERR_IO (-5)。WRITE/FLUSH 的錯誤診斷可能標 uncertain；READ 不標 uncertain。'),
  ] },
  { id: 'reset-ok', title: 'Timeout，reset 確認', summary: 'deadline 後要求 reset；只有觀察到 status=0 才完成通知與資源收尾。', frames: [
    makeFrame('Request deadline 到期', 'scheduler', 'PROCESS_WAITING；wait_reason=WAIT_EVENT', 'caller 持有', 'IN_FLIGHT', '尚無 completion notification', 'used index 未變；queue 可能仍由裝置使用', 'header、descriptor、bounce 不能回收', '單筆 request timeout 為 BLOCK_IO_TIMEOUT_MS（5000 ms）；不能直接清 state 返回。'),
    makeFrame('開始 reset', 'device', 'caller 仍 PROCESS_WAITING / WAIT_EVENT', 'caller 持有', 'RESETTING', '尚未通知', '不清除 queue memory', 'DMA 相關欄位保持穩定', 'begin_reset 記錄 ERR_TIMED_OUT，設定 100 ms reset deadline，再寫裝置 status=0。'),
    makeFrame('status=0 確認 reset', 'scheduler', 'caller PROCESS_READY；wait.result=ERR_TIMED_OUT', '仍由 caller 持有', 'DONE；裝置標 offline', 'poll_reset 通知原錯誤', '裝置不再擁有 queue；待 caller collect', 'WRITE/FLUSH uncertain；READ 不標 uncertain', 'reset 確認後標 offline 並通知 caller。寫入 timeout 不代表資料一定沒寫入。'),
    makeFrame('caller 安全收尾', 'caller', 'PROCESS_RUNNING；wait record 清除', 'caller 釋放', 'IDLE；device 維持 offline', '取回結果', 'request slot 清理', 'caller buffer 仍由 caller 管理', 'caller 返回 ERR_TIMED_OUT (-110)。裝置 offline，後續請求不會再提交。'),
  ] },
  { id: 'reset-unknown', title: 'Timeout，reset 未確認', summary: 'reset 到期仍無確認會 fail-stop；不通知、不釋放 gate，也不重用裝置緩衝區。', frames: [
    makeFrame('Request timeout，要求 reset', 'scheduler', 'caller PROCESS_WAITING / WAIT_EVENT', 'caller 持有', 'RESETTING', '尚無 notification', 'queue 仍可能被裝置使用', 'header、descriptor、bounce 仍保留', 'reset 要求已發出；等待 BLOCK_RESET_TIMEOUT_MS（100 ms）內確認。'),
    makeFrame('reset 未確認', 'device', 'caller 仍 WAITING；不會 resume', '不釋放', 'RESETTING；ownership 未知', '無 event', '裝置 status 仍非 0', '裝置可能仍讀寫這些位址', 'reset deadline 到期而 status 未歸零，不能把它當普通 ERR_IO 返回。'),
    makeFrame('進入 fail-stop', 'scheduler', 'caller 不會 collect / 返回', '不執行 leave', '不轉 DONE', '不通知 caller', '不清除、不回收、不重用', '可能被裝置使用的 buffer 保持原位', 'reset_fail_stop 記錄 reset-unconfirmed，關閉 SIE 與 sie 後進 kernel_idle。這不是一般可恢復錯誤路徑。'),
  ] },
  { id: 'kill', title: '等待中 kill defer', summary: '提交後 cancel-defer 保護 request；完成並 unwind 後，kernel-call return 邊界才終止行程。', frames: [
    makeFrame('request 已提交並等待', 'caller', 'A: PROCESS_WAITING / WAIT_EVENT；cancel_defer_depth=1', 'A 持有', 'IN_FLIGHT', 'wait record 在 A 的 PCB', 'request metadata 位於 BlockDevice', 'bounce 與 caller kernel stack 都保留', 'cancel-defer 是取消保護，不是禁止 CPU 切換；A 正在 event wait，scheduler 可以執行 B。'),
    makeFrame('B 對 A 呼叫 kill', 'scheduler', 'A 仍 WAITING；kill_pending=1', 'owner 不變', 'IN_FLIGHT', 'A 的 event wait 不取消，因 cancel_defer_depth > 0', 'request memory 穩定', 'A stack / bounce 不釋放', 'process_kill 設 kill_pending。已持有 Block gate 並進入 defer 後，取消延後以避免裝置仍使用 buffer 時中止 caller。'),
    makeFrame('裝置完成；poll 喚醒 A', 'device', 'A WAITING → READY；wait.result 由 poll 設定', '仍由 A 持有', 'DONE', '裝置先更新 used ring；scheduler poll 後才送 event', 'used entry 已驗證收取', '裝置 ownership 結束', '這裡仍是兩步：裝置寫 used ring，scheduler 下次 block_poll 才通知 event。'),
    makeFrame('A collect 並 unwind', 'caller', 'A PROCESS_RUNNING；wait record 清除', 'A 釋放 gate；defer 歸零', 'IDLE', 'A 取回結果並清 wait record', 'queue slot 安全可重用', 'buffer 生命週期完成', '取消保護不會在 defer_leave 立即 kill；A 先完成 submit_request 清理。'),
    makeFrame('kernel-call return 邊界', 'scheduler', 'kernel_call_return 看見 kill_pending；A → ZOMBIE', '已釋放', 'IDLE', '沒有仍註冊的 event wait', '資源可安全回收', '無存活 DMA ownership', 'kernel_call_return 才呼叫 process_exit(-9)。若 reset ownership 未確認，流程先 fail-stop，不到這個邊界。'),
  ] },
]
const scenarioId = ref('read')
const frameIndex = ref(0)
const playing = ref(false)
let timer = null
const scenario = computed(() => scenarios.find((item) => item.id === scenarioId.value) ?? scenarios[0])
const frame = computed(() => scenario.value.frames[frameIndex.value])
const isFirst = computed(() => frameIndex.value === 0)
const isLast = computed(() => frameIndex.value === scenario.value.frames.length - 1)
function stop() { if (timer !== null) clearInterval(timer); timer = null; playing.value = false }
function selectScenario(event) { stop(); scenarioId.value = event.target.value; frameIndex.value = 0 }
function next() { if (isLast.value) { stop(); return } frameIndex.value += 1; if (isLast.value) stop() }
function previous() { stop(); if (!isFirst.value) frameIndex.value -= 1 }
function reset() { stop(); frameIndex.value = 0 }
function togglePlay() {
  if (playing.value) { stop(); return }
  if (isLast.value) frameIndex.value = 0
  playing.value = true
  timer = setInterval(() => { if (isLast.value) { stop(); return } frameIndex.value += 1; if (isLast.value) stop() }, 1800)
}
onBeforeUnmount(stop)
</script>

<template>
  <section class="block-lab" aria-labelledby="block-lab-title">
    <header class="lab-heading">
      <div><p class="eyebrow">互動示意 · 非即時 trace</p><h2 id="block-lab-title">Block Async I/O：從提交到安全收尾</h2></div>
      <label class="scenario-picker"><span>選擇情境</span><select :value="scenarioId" @change="selectScenario"><option v-for="item in scenarios" :key="item.id" :value="item.id">{{ item.title }}</option></select></label>
    </header>
    <p class="lab-summary">{{ scenario.summary }}</p>
    <div class="controls" aria-label="互動步驟控制">
      <button type="button" :disabled="isFirst" aria-label="前一步" @click="previous">← 前一步</button>
      <button type="button" :aria-label="playing ? '暫停播放' : '播放步驟'" @click="togglePlay">{{ playing ? '暫停' : '播放' }}</button>
      <button type="button" @click="reset">重設</button>
      <button type="button" :disabled="isLast" aria-label="下一步" @click="next">下一步 →</button>
      <span class="step-count">{{ frameIndex + 1 }} / {{ scenario.frames.length }}</span>
    </div>
    <div class="frame-status" aria-live="polite" aria-atomic="true"><strong>{{ frame.title }}</strong><span>{{ frame.narration }}</span></div>
    <div class="lanes" aria-label="目前請求路徑">
      <section class="lane" :class="{ active: frame.active === 'caller' }" aria-labelledby="lane-caller"><h3 id="lane-caller">Caller</h3><p>{{ actorLabels.caller }} · {{ frame.pcb }}</p><dl><dt>Gate owner</dt><dd>{{ frame.gate }}</dd></dl></section>
      <section class="lane" :class="{ active: frame.active === 'scheduler' }" aria-labelledby="lane-scheduler"><h3 id="lane-scheduler">Scheduler / poller</h3><p>{{ actorLabels.scheduler }}</p><dl><dt>Event / PCB</dt><dd>{{ frame.event }}</dd></dl></section>
      <section class="lane" :class="{ active: frame.active === 'device' }" aria-labelledby="lane-device"><h3 id="lane-device">VirtIO device lane</h3><p>{{ actorLabels.device }}</p><dl><dt>RequestState</dt><dd><code>{{ frame.request }}</code></dd></dl></section>
    </div>
    <div class="storage-grid">
      <section class="storage-card"><h3>Queue 與 request 資料位置</h3><p>{{ frame.queue }}</p><small>實際欄位在 BlockDevice：header、descriptor[]、avail_ring、used_ring、status、consumed。</small></section>
      <section class="storage-card"><h3>資料與 buffer 生命週期</h3><p>{{ frame.buffer }}</p><small>每個 BlockDevice 有長期存在的 512-byte 對齊 bounce buffer。</small></section>
    </div>
    <p class="model-note">這些固定 frame 依原始碼行為設計，只呈現順序，不代表真實時間比例；不是 QEMU trace 或即時 OS 模擬。</p>
  </section>
</template>

<style scoped>
.block-lab { margin: 1.5rem 0; padding: 1.25rem; border: 1px solid var(--vp-c-divider); border-radius: 14px; background: var(--vp-c-bg-soft); color: var(--vp-c-text-1); }
.lab-heading { display: flex; align-items: flex-start; justify-content: space-between; gap: 1rem; }
.lab-heading h2 { margin: 0; font-size: 1.35rem; line-height: 1.3; }
.eyebrow { margin: 0 0 .35rem; color: var(--vp-c-brand-1); font-size: .78rem; font-weight: 700; letter-spacing: .04em; }
.scenario-picker { display: grid; gap: .3rem; min-width: 12rem; font-size: .85rem; font-weight: 600; }
select, button { color: var(--vp-c-text-1); font: inherit; }
select { min-height: 2.5rem; padding: .4rem .65rem; border: 1px solid var(--vp-c-divider); border-radius: 8px; background: var(--vp-c-bg); }
.lab-summary { margin: .9rem 0; color: var(--vp-c-text-2); }
.controls { display: flex; flex-wrap: wrap; align-items: center; gap: .5rem; margin: .75rem 0; }
.controls button { min-height: 2.5rem; padding: .4rem .75rem; border: 1px solid var(--vp-c-divider); border-radius: 8px; background: var(--vp-c-bg); cursor: pointer; }
.controls button:hover:not(:disabled) { border-color: var(--vp-c-brand-1); }
.controls button:focus-visible, select:focus-visible { outline: 3px solid var(--vp-c-brand-1); outline-offset: 2px; }
.controls button:disabled { cursor: not-allowed; opacity: .48; }
.step-count { margin-left: auto; color: var(--vp-c-text-2); font-variant-numeric: tabular-nums; }
.frame-status { display: grid; gap: .25rem; margin: .85rem 0; padding: .85rem 1rem; border-left: 4px solid var(--vp-c-brand-1); border-radius: 8px; background: var(--vp-c-bg); }
.frame-status strong { font-size: 1rem; }
.frame-status span { line-height: 1.6; }
.lanes { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: .75rem; }
.lane, .storage-card { min-width: 0; padding: .85rem; border: 1px solid var(--vp-c-divider); border-radius: 10px; background: var(--vp-c-bg); }
.lane { border-top: 3px solid var(--vp-c-divider); }
.lane.active { border-top-color: var(--vp-c-brand-1); }
.lane h3, .storage-card h3 { margin: 0 0 .45rem; font-size: .98rem; }
.lane p, .storage-card p { margin: 0; line-height: 1.55; overflow-wrap: anywhere; }
dl { margin: .75rem 0 0; padding-top: .55rem; border-top: 1px solid var(--vp-c-divider); }
dt { color: var(--vp-c-text-2); font-size: .78rem; font-weight: 650; }
dd { margin: .2rem 0 0; line-height: 1.5; overflow-wrap: anywhere; }
.storage-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: .75rem; margin-top: .75rem; }
.storage-card small { display: block; margin-top: .65rem; color: var(--vp-c-text-2); line-height: 1.5; }
.model-note { margin: .85rem 0 0; color: var(--vp-c-text-2); font-size: .82rem; line-height: 1.5; }
@media (max-width: 700px) { .block-lab { padding: .9rem; } .lab-heading { display: grid; } .scenario-picker { min-width: 0; width: 100%; } .lanes, .storage-grid { grid-template-columns: minmax(0, 1fr); } .step-count { width: 100%; margin-left: 0; } }
@media (prefers-reduced-motion: reduce) { *, *::before, *::after { scroll-behavior: auto !important; transition-duration: .01ms !important; animation-duration: .01ms !important; animation-iteration-count: 1 !important; } }
</style>
