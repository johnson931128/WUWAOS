<script setup>
import { computed, onBeforeUnmount, ref } from 'vue'
import StepTimeline from './StepTimeline.vue'

// Each entry is a teaching frame, not a captured execution trace.
const makeFrame = (title, active, pcb, gate, request, event, queue, buffer, narration) =>
  ({ title, active, pcb, gate, request, event, queue, buffer, narration })
const scenarios = [
  { id: 'read', title: 'Read 成功', summary: 'caller 等待 event；scheduler 輪詢完成後，caller 才將 bounce 複製到目的 buffer。', frames: [
    makeFrame('呼叫 block_read', 'caller', 'THREAD_RUNNING；尚未 WAIT_EVENT', '無 owner', 'IDLE', '尚未註冊 wait', '尚未發布 descriptor chain', 'dst 屬 caller；靜態 bounce 可重用', '同步函式呼叫開始。runtime 中 caller 必須是可等待的 RUNNING 行程。'),
    makeFrame('持有 gate 並提交', 'caller', 'THREAD_RUNNING；wait_reason=WAIT_NONE', 'owner=owner_tid；depth=1', 'IN_FLIGHT', '已 snapshot completion generation / sequence', 'header、descriptor[0..2]、avail_ring 位於 BlockDevice', 'READ 的 DMA 目的地是 target->bounce；還沒寫 dst', 'caller 正在執行 submit_request()，準備 request 並通知裝置。VirtIO 之後會寫入持久 bounce，不直接碰 caller buffer。'),
    makeFrame('caller 等待；裝置尚未完成', 'scheduler', 'THREAD_WAITING；wait_reason=WAIT_EVENT', 'caller 仍持有；跨越 wait 保留', 'IN_FLIGHT', 'KernelEventSource.completion；caller TCB.event_wait', 'used_ring 尚無新完成項；scheduler 會執行 block_poll()', 'bounce 是 DMA buffer；dst 保持原值', 'caller 的 kernel stack 與 gate 保留。poller 在 scheduler context 執行，不能阻塞。'),
    makeFrame('裝置先寫入 used ring', 'device', 'A: THREAD_WAITING / WAIT_EVENT；B 可在 THREAD_RUNNING', 'owner=owner_tid', 'IN_FLIGHT（尚未 poll 收取）', '尚未 event；裝置不直接呼叫核心 event API', '裝置更新 used_ring.index / entry；等待下一次 block_poll()', '裝置已將資料 DMA 到 bounce；dst 尚未更新', '此 polling 示意在核心驗證 used entry 前，A 仍保持等待；IRQ 路徑也需經相同驗證器。'),
    makeFrame('poller 收取並通知', 'scheduler', 'A: THREAD_READY；原 wait_reason=WAIT_EVENT', '仍由 A 持有', 'DONE', 'block_poll 驗證後 event_source_notify；TCB event_wait.result=0', 'used entry 驗證通過，consumed 前進一格', 'bounce 含 512 bytes；尚未複製到 dst', 'scheduler 下次執行 block_poll 才觀察完成、設 DONE、送 event，將 A 轉為 READY。'),
    makeFrame('collect、複製並返回', 'caller', 'THREAD_RUNNING；event_wait 清除', 'A 清理後釋放', 'IDLE', 'A 恢復後取回結果並清除 TCB event_wait', 'request slot 可重用', '成功 READ 才 copy bounce → dst', 'caller 在 submit/collect 路徑複製資料，設定 IDLE，再釋放 gate 與 cancel-defer，最後返回 0。'),
  ] },
  { id: 'write', title: 'Write 成功', summary: '提交前複製 caller 資料至靜態 bounce；裝置完成後同步呼叫才返回。', frames: [
    makeFrame('呼叫 block_write', 'caller', 'THREAD_RUNNING；尚未 WAIT_EVENT', '無 owner', 'IDLE', '尚未註冊 wait', 'queue 尚未發布', 'src 在 caller；bounce 可重用', 'block_write 是同步函式；caller 的來源資料目前有效。'),
    makeFrame('複製並提交', 'caller', 'THREAD_RUNNING；wait_reason=WAIT_NONE', 'owner=owner_tid；depth=1', 'IN_FLIGHT', 'snapshot completion generation / sequence', 'header + descriptor chain + avail ring 位於 BlockDevice', 'src 已複製到 target->bounce；DMA 不讀 caller src', 'caller 正在執行 submit_request()，driver 在通知前複製 512 bytes。descriptor 指向靜態欄位，避免 DMA 使用 caller stack buffer。'),
    makeFrame('等待裝置', 'scheduler', 'THREAD_WAITING；wait_reason=WAIT_EVENT', 'caller 持有', 'IN_FLIGHT', 'KernelEventSource.completion；caller TCB.event_wait', 'avail 已發布；等待 used ring', 'bounce 必須保留至 collect', '請求雖已提交，block_write 尚未返回。caller 等待 event，CPU 可執行其他行程。'),
    makeFrame('裝置完成，等待 poll', 'device', 'A: THREAD_WAITING / WAIT_EVENT；B 可 RUNNING', 'caller 持有', 'IN_FLIGHT（未收取）', '尚無 event notification', '裝置更新 used_ring；block_poll 尚未觀察', '裝置讀取 bounce；bounce 仍不能重用', 'VirtIO 更新 used ring 不等同 caller 已喚醒；完成由 scheduler 後續 poll 收取。'),
    makeFrame('poll、collect、返回', 'caller', 'THREAD_READY → THREAD_RUNNING；wait 清除', 'caller unwind 時釋放', 'DONE → IDLE', 'poll 驗證 status=0 後通知；caller 恢復取 result=0', 'used entry 已收取', 'bounce 可在清理後重用；不需複製回 caller', 'poller 通知只喚醒 caller；caller 恢復後收取結果、重設 state、釋放 gate，函式才返回 0。'),
  ] },
  { id: 'ioerr', title: '裝置回報 IOERR', summary: '有效 VirtIO status 錯誤完成為 DONE，collect 回 ERR_IO；不 reset。', frames: [
    makeFrame('READ 已提交並等待', 'scheduler', 'THREAD_WAITING；wait_reason=WAIT_EVENT', 'caller 持有', 'IN_FLIGHT', 'event_wait 在 caller TCB', 'avail 已發布', 'DMA 目標 bounce；dst 未更新', '此情境的 used entry 結構有效，但裝置回報 status=1 (IOERR)。'),
    makeFrame('裝置寫入完成項', 'device', 'caller 仍 THREAD_WAITING / WAIT_EVENT', 'caller 持有', 'IN_FLIGHT（尚未 poll）', '尚未通知 event', 'used_ring 更新，等待 poll 收取', '讀取資料可能不完整；dst 保持原值', '裝置回報 IOERR 不會直接執行 event notification；仍需 scheduler poll。'),
    makeFrame('poll 判定錯誤', 'scheduler', 'caller THREAD_READY；event_wait.result=ERR_IO', '仍由 caller 持有', 'DONE', 'poll 收取 status=1 並通知 ERR_IO', 'used entry 驗證通過，consumed 前進', '失敗 READ 不 copy bounce → dst', '這是有效完成的 IOERR，不是 malformed completion。狀態進 DONE，不進 reset。'),
    makeFrame('collect 回傳錯誤', 'caller', 'THREAD_RUNNING；event_wait 清除', 'caller 釋放', 'IDLE', 'caller 取得結果後清除 wait', 'slot 可重用', 'dst 不被失敗 READ 覆蓋', 'block_read 返回 ERR_IO (-5)。WRITE/FLUSH 的錯誤診斷可能標 uncertain；READ 不標 uncertain。'),
  ] },
  { id: 'reset-ok', title: 'Timeout，reset 確認', summary: 'deadline 後要求 reset；只有觀察到 status=0 才完成通知與資源收尾。', frames: [
    makeFrame('Request deadline 到期', 'scheduler', 'THREAD_WAITING；wait_reason=WAIT_EVENT', 'caller 持有', 'IN_FLIGHT', '尚無 completion notification', 'used index 未變；queue 可能仍由裝置使用', 'header、descriptor、bounce 不能回收', '單筆 request timeout 為 BLOCK_IO_TIMEOUT_MS（5000 ms）；不能直接清 state 返回。'),
    makeFrame('開始 reset', 'device', 'caller 仍 THREAD_WAITING / WAIT_EVENT', 'caller 持有', 'RESETTING', '尚未通知', '不清除 queue memory', 'DMA 相關欄位保持穩定', 'begin_reset 記錄 ERR_TIMED_OUT，設定 100 ms reset deadline，再寫裝置 status=0。'),
    makeFrame('status=0 確認 reset', 'scheduler', 'caller THREAD_READY；event_wait.result=ERR_TIMED_OUT', '仍由 caller 持有', 'DONE；裝置標 offline', 'poll_reset 通知原錯誤', '裝置不再擁有 queue；待 caller collect', 'WRITE/FLUSH uncertain；READ 不標 uncertain', 'reset 確認後標 offline 並通知 caller。寫入 timeout 不代表資料一定沒寫入。'),
    makeFrame('caller 安全收尾', 'caller', 'THREAD_RUNNING；event_wait 清除', 'caller 釋放', 'IDLE；device 維持 offline', '取回結果', 'request slot 清理', 'caller buffer 仍由 caller 管理', 'caller 返回 ERR_TIMED_OUT (-110)。裝置 offline，後續請求不會再提交。'),
  ] },
  { id: 'reset-unknown', title: 'Timeout，reset 未確認', summary: 'reset 到期仍無確認會 fail-stop；不通知、不釋放 gate，也不重用裝置緩衝區。', frames: [
    makeFrame('Request timeout，要求 reset', 'scheduler', 'caller THREAD_WAITING / WAIT_EVENT', 'caller 持有', 'RESETTING', '尚無 notification', 'queue 仍可能被裝置使用', 'header、descriptor、bounce 仍保留', 'reset 要求已發出；等待 BLOCK_RESET_TIMEOUT_MS（100 ms）內確認。'),
    makeFrame('reset 未確認', 'device', 'caller 仍 WAITING；不會 resume', '不釋放', 'RESETTING；ownership 未知', '無 event', '裝置 status 仍非 0', '裝置可能仍讀寫這些位址', 'reset deadline 到期而 status 未歸零，不能把它當普通 ERR_IO 返回。'),
    makeFrame('進入 fail-stop', 'scheduler', 'caller 不會 collect / 返回', '不執行 leave', '不轉 DONE', '不通知 caller', '不清除、不回收、不重用', '可能被裝置使用的 buffer 保持原位', 'reset_fail_stop 記錄 reset-unconfirmed，關閉 SIE 與 sie 後進 kernel_idle。這不是一般可恢復錯誤路徑。'),
  ] },
  { id: 'kill', title: '等待中 kill defer', summary: '提交後 cancel-defer 保護 request；完成並 unwind 後，kernel-call return 邊界才終止行程。', frames: [
    makeFrame('request 已提交並等待', 'caller', 'A: THREAD_WAITING / WAIT_EVENT；cancel_defer_depth=1', 'A 持有', 'IN_FLIGHT', 'event_wait 在 A 的 TCB', 'request metadata 位於 BlockDevice', 'bounce 與 caller kernel stack 都保留', 'cancel-defer 是取消保護，不是禁止 CPU 切換；A 正在 event wait，scheduler 可以執行 B。'),
    makeFrame('父行程 B 對子行程 A 呼叫 kill', 'caller', 'A 仍 WAITING；kill_pending=1', 'owner 不變', 'IN_FLIGHT', 'A 的 event wait 不取消，因 cancel_defer_depth > 0', 'request memory 穩定', 'A stack / bounce 不釋放', '本例假設 B 是 A 的直接父行程；WUWAOS 不允許任意行程互相 kill。process_kill 設 kill_pending，已提交 I/O 的取消延後以保留裝置仍使用的 buffer。'),
    makeFrame('裝置完成；poll 喚醒 A', 'device', 'A WAITING → READY；event_wait.result 由 poll 設定', '仍由 A 持有', 'DONE', '裝置先更新 used ring；scheduler poll 後才送 event', 'used entry 已驗證收取', '裝置 ownership 結束', '這裡仍是兩步：裝置寫 used ring，scheduler 下次 block_poll 才通知 event。'),
    makeFrame('A collect 並 unwind', 'caller', 'A THREAD_RUNNING；event_wait 清除', 'A 釋放 gate；defer 歸零', 'IDLE', 'A 取回結果並清 event_wait', 'queue slot 安全可重用', 'buffer 生命週期完成', '取消保護不會在 defer_leave 立即 kill；A 先完成 submit_request 清理。'),
    makeFrame('kernel-call return 邊界', 'caller', 'kernel_call_return 看見 kill_pending；A → ZOMBIE', '已釋放', 'IDLE', '沒有仍註冊的 event wait', '資源可安全回收', '無存活 DMA ownership', 'A 在自己的核心返回路徑由 kernel_call_return 呼叫 process_exit(-9)。若 reset ownership 未確認，流程先 fail-stop，不到這個邊界。'),
  ] },
]
const scenarioId = ref('read')
const frameIndex = ref(0)
const playing = ref(false)
let timer = null
const scenario = computed(() => scenarios.find((item) => item.id === scenarioId.value) ?? scenarios[0])
const frame = computed(() => scenario.value.frames[frameIndex.value])
// Explicit teaching fields: each fixed frame declares its own state instead of parsing narration.
const fieldRows = {
  read: [
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'none', depth:'0', request:'IDLE', event:'not registered', result:'—', buffer:'dst caller-owned', flow:'call', source:'block_read' },
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'generation/sequence saved', result:'pending', buffer:'READ DMA → bounce[512]', flow:'submit', source:'submit_request' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'registered', result:'pending', buffer:'bounce DMA-owned · dst unchanged', flow:'wait', source:'thread_wait_event' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'not notified', result:'pending', buffer:'device wrote bounce · dst unchanged', flow:'device-complete', source:'used ring' },
    { threadState:'THREAD_READY', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'DONE', event:'notified · result 0', result:'0', buffer:'bounce ready · dst unchanged', flow:'notify', source:'block_poll' },
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'none', depth:'0', request:'IDLE', event:'cleared', result:'0', buffer:'bounce → dst copied', flow:'collect', source:'submit_request collect' },
  ],
  write: [
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'none', depth:'0', request:'IDLE', event:'not registered', result:'—', buffer:'src caller-owned', flow:'call', source:'block_write' },
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'generation/sequence saved', result:'pending', buffer:'src → bounce copied', flow:'submit', source:'submit_request' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'registered', result:'pending', buffer:'bounce retained for DMA', flow:'wait', source:'thread_wait_event' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'not notified', result:'pending', buffer:'device reads bounce', flow:'device-complete', source:'used ring' },
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'none', depth:'0', request:'IDLE', event:'cleared · result 0', result:'0', buffer:'bounce reusable', flow:'collect', source:'submit_request collect' },
  ],
  ioerr: [
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'registered', result:'pending', buffer:'READ DMA → bounce · dst stable', flow:'wait', source:'thread_wait_event' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'not notified', result:'pending', buffer:'partial/invalid bounce · dst stable', flow:'device-complete', source:'used ring' },
    { threadState:'THREAD_READY', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'DONE', event:'notified · ERR_IO', result:'ERR_IO', buffer:'failed READ does not copy', flow:'notify', source:'poll_device' },
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'none', depth:'0', request:'IDLE', event:'cleared', result:'ERR_IO (-5)', buffer:'dst unchanged', flow:'collect', source:'submit_request collect' },
  ],
  'reset-ok': [
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'IN_FLIGHT', event:'pending', result:'pending', buffer:'queue and bounce retained', flow:'timeout', source:'poll_device timeout' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'RESETTING', event:'not notified', result:'ERR_TIMED_OUT pending', buffer:'DMA fields retained', flow:'reset', source:'begin_reset' },
    { threadState:'THREAD_READY', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'DONE', event:'notified · timeout', result:'ERR_TIMED_OUT', buffer:'device offline · safe to collect', flow:'notify', source:'poll_reset' },
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'none', depth:'0', request:'IDLE', event:'cleared', result:'ERR_TIMED_OUT (-110)', buffer:'caller buffer remains caller-owned', flow:'collect', source:'submit_request collect' },
  ],
  'reset-unknown': [
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'RESETTING', event:'pending', result:'timeout pending', buffer:'DMA storage retained', flow:'reset', source:'begin_reset' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'RESETTING', event:'not notified', result:'unknown', buffer:'device may still access memory', flow:'reset-unconfirmed', source:'poll_reset' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'caller tid', depth:'1', request:'RESETTING', event:'no notification', result:'no return', buffer:'queue and bounce never reused', flow:'fail-stop', source:'reset_fail_stop' },
  ],
  kill: [
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'A tid', depth:'1', request:'IN_FLIGHT', event:'registered', result:'pending', buffer:'bounce + kernel stack retained', flow:'wait', source:'thread_cancel_defer_enter' },
    { threadState:'THREAD_WAITING', waitReason:'WAIT_EVENT', owner:'A tid', depth:'1', request:'IN_FLIGHT', event:'wait remains registered', result:'kill_pending', buffer:'DMA storage retained', flow:'kill-deferred', source:'process_kill' },
    { threadState:'THREAD_READY', waitReason:'WAIT_EVENT', owner:'A tid', depth:'1', request:'DONE', event:'notified after poll', result:'completion result', buffer:'device ownership ended', flow:'notify', source:'poll_device' },
    { threadState:'THREAD_RUNNING', waitReason:'WAIT_NONE', owner:'none', depth:'0', request:'IDLE', event:'cleared', result:'completion result', buffer:'safe after collect', flow:'collect', source:'submit_request collect' },
    { threadState:'THREAD_ZOMBIE', waitReason:'WAIT_NONE', owner:'none', depth:'0', request:'IDLE', event:'cleared', result:'exit -9', buffer:'no active DMA ownership', flow:'exit', source:'thread_kernel_call_return' },
  ],
}
const stateExtras = {
  'reset-ok': { 2: { deviceOffline: true }, 3: { deviceOffline: true } },
  kill: {
    0: { cancelDeferDepth: '1', killPending: 'false' }, 1: { cancelDeferDepth: '1', killPending: 'true' },
    2: { cancelDeferDepth: '1', killPending: 'true' }, 3: { cancelDeferDepth: '0', killPending: 'true' },
    4: { cancelDeferDepth: '0', killPending: 'false' },
  },
}
const meta = computed(() => ({
  deviceOffline: false, cancelDeferDepth: '0', killPending: 'false',
  ...fieldRows[scenarioId.value]?.[frameIndex.value],
  ...stateExtras[scenarioId.value]?.[frameIndex.value],
}))
const copyDirection = computed(() => {
  if (meta.value.flow === 'submit' && scenarioId.value === 'write') return 'COPY：caller src → bounce[512]（通知裝置前）'
  if (meta.value.flow === 'collect' && scenarioId.value === 'read' && meta.value.result === '0') return 'COPY：bounce[512] → caller dst（成功 collect 後）'
  if (meta.value.flow === 'collect' && (scenarioId.value === 'read' || scenarioId.value === 'ioerr')) return '未 COPY：READ 失敗，caller dst 維持原值'
  if (scenarioId.value === 'read' && meta.value.flow === 'submit') return 'DMA 目的地：bounce[512]；caller dst 尚未更新'
  if (scenarioId.value === 'read' && meta.value.flow === 'device-complete') return '裝置已將資料 DMA 至 bounce[512]；尚未 collect'
  if (scenarioId.value === 'read' && meta.value.flow === 'notify') return 'bounce[512] 已就緒；等待 caller collect'
  if (meta.value.flow === 'reset-unconfirmed' || meta.value.flow === 'fail-stop') return '未 COPY：DMA ownership 未確認，queue 與 bounce 保留'
  return '此步沒有 caller buffer 複製'
})
const whyText = computed(() => ({
  call: '同步 API 從 caller thread 開始；呼叫端會等到結果收取後才返回。',
  submit: scenarioId.value === 'write' ? 'driver 先將 caller 資料複製到持久 bounce，再發布 avail ring。' : 'driver 將 descriptor 指向持久 BlockDevice 欄位，再發布 avail ring。',
  wait: '請求尚未完成，caller 透過 event wait 讓出 CPU；scheduler 可執行其他 READY thread。',
  'device-complete': '裝置先更新 used ring；核心仍須驗證完成項，caller 此時尚未收到通知。',
  notify: 'poller 驗證 used entry 與 status 後，才通知 completion event。',
  collect: scenarioId.value === 'read' && meta.value.result === '0' ? '成功 READ 由恢復執行的 caller 在 collect 階段複製 bounce 到目的 buffer。' : 'caller 恢復後收取結果並安全釋放 request 與 gate。',
  timeout: 'deadline 到期表示 request 超時，不能證明裝置已停止使用 DMA 記憶體。',
  reset: 'driver 要求裝置 reset；只有觀察到 status=0 才能確認 DMA ownership 結束。',
  'reset-unconfirmed': '裝置 status 尚未歸零，queue 與 bounce 仍可能被裝置使用。',
  'fail-stop': 'reset 未確認時裝置仍可能存取 DMA 記憶體；核心停機且不通知、不釋放資源。',
  'kill-deferred': 'cancel-defer 只延後 kill 處理，不阻止 scheduler 切換；已提交 I/O 必須先安全完成。',
  exit: 'request 收尾後，延後的 kill 才會在 kernel-call return 邊界處理。',
}[meta.value.flow] ?? '此步依固定教學模型呈現 request 與 caller 的狀態。'))
const changesText = computed(() => ({
  call: '尚未建立裝置 request。', submit: scenarioId.value === 'write' ? 'src 已複製到 bounce，avail ring 已發布。' : 'avail ring 已發布；READ 目的地仍是 bounce。',
  wait: 'caller TCB 登記 event_wait 並進入 THREAD_WAITING。', 'device-complete': 'used ring 已更新；核心尚未收取或通知 caller。',
  notify: '完成項通過驗證；completion event 通知 caller，狀態轉為 READY。',
  collect: meta.value.result === '0' ? 'caller 收取成功結果，request 回到 IDLE，gate 與 cancel-defer 釋放。' : 'caller 收取錯誤結果；失敗 READ 不會複製到目的 buffer。',
  timeout: 'request 進入 reset 流程；queue 與 DMA 欄位繼續保留。', reset: '等待 reset 確認；目前不通知 caller，也不回收 DMA 欄位。',
  'reset-unconfirmed': '沒有 completion notification；caller 不會 collect，裝置欄位不重用。',
  'fail-stop': '關閉 SIE 並進入 kernel_idle；Thread 不會被通知或 collect。',
  'kill-deferred': '設定 kill_pending；既有 event wait 不取消。', exit: 'Thread 結束；此時已無存活 DMA ownership。',
}[meta.value.flow] ?? '狀態依目前固定教學步驟更新。'))
const eventText = computed(() => ({
  'not registered': '尚未登記', registered: '已登記', 'generation/sequence saved': 'generation / sequence 已保存',
  pending: '等待中', 'not notified': '尚未通知', 'notified · result 0': '已通知 · result 0',
  'cleared · result 0': '已清除 · result 0', cleared: '已清除', 'event_wait remains registered': 'event_wait 仍登記',
  'notified · ERR_IO': '已通知 · ERR_IO', 'notified · timeout': '已通知 · timeout', 'no notification': '沒有通知',
  'no return': '不返回', 'wait remains registered': 'event_wait 仍登記', 'notified after poll': 'poll 後已通知',
  'completion result': '完成結果', 'exit -9': 'exit -9', 'event_wait cleared': 'event_wait 已清除',
}[meta.value.event] ?? meta.value.event))
const resultText = computed(() => ({ pending: '等待中', 'completion result': '完成結果', unknown: '未知', 'no return': '不返回' }[meta.value.result] ?? meta.value.result))
const ownerText = computed(() => ({ none: '無', 'caller tid': 'caller thread ID', 'A tid': 'A thread ID' }[meta.value.owner] ?? meta.value.owner))
const sourceHref = computed(() => {
  if (['thread_wait_event', 'process_kill', 'thread_cancel_defer_enter', 'thread_kernel_call_return'].includes(meta.value.source)) return 'https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c'
  if (meta.value.source === 'event_source_notify') return 'https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/event.c'
  if (meta.value.source === 'kernel_gate_enter' || meta.value.source === 'kernel_gate_leave') return 'https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/gate.c'
  return 'https://github.com/johnson931128/WUWAOS/blob/main/drivers/virtio/block.c'
})
const isFirst = computed(() => frameIndex.value === 0)
const isLast = computed(() => frameIndex.value === scenario.value.frames.length - 1)
function stop() { if (timer !== null) clearInterval(timer); timer = null; playing.value = false }
function selectScenario(event) { stop(); scenarioId.value = event.target.value; frameIndex.value = 0 }
function next() { stop(); if (isLast.value) return; frameIndex.value += 1 }
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
      <button type="button" :aria-pressed="playing" :aria-label="playing ? '暫停播放' : '播放步驟'" @click="togglePlay">{{ playing ? '暫停' : '播放' }}</button>
      <button type="button" @click="reset">重設</button>
      <button type="button" :disabled="isLast" aria-label="下一步" @click="next">下一步 →</button>
      <span class="step-count">{{ frameIndex + 1 }} / {{ scenario.frames.length }}</span>
    </div>
    <div class="frame-status" aria-live="polite" aria-atomic="true"><strong>{{ frame.title }}</strong></div>
    <div class="workbench">
      <section class="system-view" aria-labelledby="block-map-title">
        <h3 id="block-map-title">請求所有權與資料路徑</h3>
        <div class="flow-grid" role="group" aria-label="Block request 的核心物件與狀態路徑">
          <article class="object-node caller-node" :class="{ focused: ['call','submit','wait','collect','kill-deferred','exit'].includes(meta.flow) }">
            <h4>Caller thread · TCB</h4><p><code>state</code> {{ meta.threadState }} · <code>wait_reason</code> {{ meta.waitReason }}</p>
            <div class="object-field"><code aria-label="Thread TCB.event_wait">event_wait</code><span>{{ eventText }} · {{ resultText }}</span></div>
            <div v-if="scenarioId === 'kill'" class="object-field"><code aria-label="Thread.cancel_defer_depth">cancel_defer_depth</code><span>{{ meta.cancelDeferDepth }}</span></div>
            <div v-if="scenarioId === 'kill'" class="object-field"><code aria-label="Thread.kill_pending">kill_pending</code><span>{{ meta.killPending }}</span></div>
          </article>
          <div class="relation submit-relation" :class="{ active: meta.flow === 'submit' }"><b aria-hidden="true">→</b><span>submit_request()</span></div>
          <article class="object-node device-node" :class="{ focused: ['device-complete','reset','reset-unconfirmed','fail-stop'].includes(meta.flow) }">
            <h4>VirtIO block device</h4><p>DMA 存取以 BlockDevice 持久欄位為界</p>
            <code>{{ meta.flow === 'device-complete' ? 'used ring 已更新' : meta.flow === 'reset-unconfirmed' || meta.flow === 'fail-stop' ? 'DMA ownership 未確認' : meta.deviceOffline ? 'status=0 · offline' : 'header · descriptor · status' }}</code>
            <div class="object-field"><code aria-label="BlockDevice.offline">offline</code><span>{{ meta.deviceOffline ? 'true · 已確認 reset' : meta.flow === 'reset-unconfirmed' || meta.flow === 'fail-stop' ? 'false · ownership 未確認' : 'false' }}</span></div>
            <div class="object-field"><code aria-label="KernelGate.owner_tid">KernelGate.owner_tid</code><span>{{ ownerText }} · depth={{ meta.depth }}</span></div>
          </article>
          <div class="relation wake-relation" :class="{ active: meta.flow === 'notify' }">
            <span class="event-source"><code>BlockDevice.completion</code><small>type: KernelEventSource</small><small>{{ meta.flow === 'notify' ? 'event_source_notify → caller TCB.event_wait' : 'caller TCB.event_wait · 尚未通知' }}</small></span><b aria-hidden="true">↑</b>
          </div>
          <div class="relation reset-relation" :class="{ active: ['reset','reset-unconfirmed','fail-stop'].includes(meta.flow) }" :aria-hidden="!['reset','reset-unconfirmed','fail-stop'].includes(meta.flow)"><b aria-hidden="true">→</b><span>poller 要求 reset</span></div>
          <div class="relation complete-relation" :class="{ active: meta.flow === 'device-complete' }"><span>裝置寫入完成項</span><b aria-hidden="true">↓</b></div>
          <article class="object-node scheduler-node" :class="{ focused: ['notify','device-complete','timeout','reset','reset-unconfirmed','fail-stop'].includes(meta.flow) }">
            <h4>Scheduler / poller</h4><p><code>block_poll()</code> → <code>poll_device()</code></p>
            <code>{{ meta.flow === 'fail-stop' ? 'reset_fail_stop()' : '驗證 used entry / status' }}</code>
            <strong v-if="meta.flow === 'fail-stop'" class="system-state">FAIL-STOP · 不通知、不返回</strong>
          </article>
          <div class="relation poll-relation" :class="{ active: meta.flow === 'notify' }"><b aria-hidden="true">←</b><span>驗證 used entry</span></div>
          <article class="object-node ring-node" :class="{ focused: meta.flow === 'device-complete' || meta.flow === 'notify' }"><h4>BlockDevice · request / rings</h4><p><code>request_state</code> {{ meta.request }}</p><code>avail_ring · used_ring · consumed</code></article>
        </div>
        <div class="data-object" aria-label="Caller buffer 與 BlockDevice bounce buffer">
          <div class="buffer-object"><small>Caller buffer</small><code>{{ scenarioId === 'write' ? 'src' : scenarioId === 'read' || scenarioId === 'ioerr' ? 'dst' : 'src / dst' }}</code><span>由 caller 持有</span></div>
          <div class="copy-relation" :class="{ copied: copyDirection.startsWith('COPY：') }"><strong>{{ copyDirection }}</strong></div>
          <div class="buffer-object bounce-object"><small>BlockDevice data field</small><code>bounce[512]</code><span>16-byte aligned · DMA 欄位保留至安全收尾</span></div>
        </div>
        <p class="model-note">此固定序列展示 scheduler polling 路徑，非 QEMU trace。啟用 IRQ 時 service_block_irq 也會 ACK 後呼叫相同的 poll_device 驗證器；IRQ handler 不 collect、釋放 DMA 欄位或切換 caller。</p>
      </section>
      <aside class="inspector" aria-label="目前核心狀態欄位">
        <h3>核心欄位快照</h3>
        <section class="inspector-group" aria-labelledby="thread-fields"><h4 id="thread-fields">Thread TCB</h4><dl>
          <div class="field-row"><dt><code aria-label="Thread.state">state</code></dt><dd>{{ meta.threadState }}</dd></div>
          <div class="field-row"><dt><code aria-label="Thread.wait_reason">wait_reason</code></dt><dd>{{ meta.waitReason }}</dd></div>
          <div class="field-row"><dt><code aria-label="Thread.event_wait.result">event_wait.<wbr>result</code></dt><dd>{{ eventText }} · {{ resultText }}</dd></div>
          <div v-if="scenarioId === 'kill'" class="field-row"><dt><code aria-label="Thread.cancel_defer_depth">cancel_defer_depth</code></dt><dd>{{ meta.cancelDeferDepth }}</dd></div>
          <div v-if="scenarioId === 'kill'" class="field-row"><dt><code aria-label="Thread.kill_pending">kill_pending</code></dt><dd>{{ meta.killPending }}</dd></div>
        </dl></section>
        <section class="inspector-group" aria-labelledby="gate-fields"><h4 id="gate-fields">KernelGate</h4><dl>
          <div class="field-row"><dt><code aria-label="KernelGate.owner_tid">owner_tid</code></dt><dd>{{ ownerText }}</dd></div>
          <div class="field-row"><dt><code aria-label="KernelGate.depth">depth</code></dt><dd>{{ meta.depth }}</dd></div>
        </dl></section>
        <section class="inspector-group" aria-labelledby="device-fields"><h4 id="device-fields">BlockDevice</h4><dl>
          <div class="field-row"><dt><code aria-label="BlockDevice.request_state">request_state</code></dt><dd>{{ meta.request }}</dd></div>
          <div class="field-row"><dt><code aria-label="BlockDevice.offline">offline</code></dt><dd>{{ meta.deviceOffline ? 'true' : meta.flow === 'reset-unconfirmed' || meta.flow === 'fail-stop' ? 'false · reset 未確認' : 'false' }}</dd></div>
          <div class="field-row"><dt><code aria-label="BlockDevice.bounce[512]">bounce[512]</code></dt><dd>{{ copyDirection }}</dd></div>
        </dl></section>
      </aside>
    </div>
    <section class="explanation"><h3>{{ frame.title }}</h3><div class="explanation-grid"><div><h4>目前操作</h4><p>{{ frame.narration }}</p></div><div><h4>為什麼會發生</h4><p>{{ whyText }}</p></div><div><h4>狀態變化</h4><p>{{ changesText }}</p></div></div><p><strong>結果：</strong><code>{{ resultText }}</code> · <strong>資料路徑：</strong>{{ copyDirection }}</p><p class="source-links"><strong>原始碼：</strong><a :href="sourceHref" target="_blank" rel="noreferrer">{{ meta.source }}</a></p></section>
    <StepTimeline :frames="scenario.frames" :current="frameIndex" label="執行時間線" @seek="(i) => { stop(); frameIndex = i }" />
    <p class="model-note">方向鍵以外可用時間線按鈕跳轉；播放僅推進固定教學步驟，不表示真實時間比例。</p>
  </section>
</template>

<style scoped>
.block-lab { container: block-lab / inline-size; margin: 1.5rem 0; padding: 1.25rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-workspace); background: var(--os-canvas); color: var(--os-ink); }
.lab-heading { display: flex; align-items: center; justify-content: space-between; gap: 1rem; }
.lab-heading h2 { margin: 0; padding: 0; border: 0; font-size: var(--os-font-lg); line-height: 1.3; }
.eyebrow { margin: 0 0 .25rem; color: var(--os-accent); font-size: var(--os-font-xs); font-weight: 650; }
.scenario-picker { display: grid; gap: .3rem; min-width: 12rem; font-size: var(--os-font-sm); font-weight: 600; }
select, button { color: var(--os-ink); font: inherit; }
select { min-height: 2.75rem; padding: .4rem .65rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-control); background: var(--os-control-surface); color: var(--os-ink); }
.lab-summary { margin: .65rem 0; color: var(--os-muted); }
.controls { display: flex; flex-wrap: wrap; align-items: center; gap: .5rem; margin: .65rem 0; }
.controls button { min-height: 2.75rem; padding: .4rem .75rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-control); background: var(--os-control-surface); cursor: pointer; }
.controls button:hover:not(:disabled) { border-color: var(--os-accent); }
.controls button:focus-visible, select:focus-visible { outline: 3px solid var(--os-focus); outline-offset: 2px; }
.controls button[aria-pressed="true"] { border-color: var(--os-accent); background: var(--os-accent); color: var(--os-on-accent); }
.controls button:disabled { cursor: not-allowed; }
.step-count { margin-left: auto; color: var(--os-muted); font-variant-numeric: tabular-nums; }
.frame-status { margin: .65rem 0; padding: .65rem .85rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-panel); background: var(--os-panel); }
.frame-status strong { font-size: var(--os-font-base); }
.workbench { display: grid; grid-template-columns: minmax(0, 1.8fr) minmax(17rem, 1fr); gap: 1rem; align-items: start; }
.system-view, .inspector, .explanation { min-width: 0; padding: 1rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-panel); background: var(--os-panel); }
.system-view h3, .inspector h3, .explanation h3 { margin: 0 0 .6rem; font-size: var(--os-font-base); }
.flow-grid { display: grid; grid-template-columns: minmax(0,1fr) minmax(6.5rem,7rem) minmax(0,1fr); grid-template-areas: 'caller submit device' 'wake reset complete' 'scheduler poll rings'; gap: .55rem; align-items: stretch; }
.caller-node { grid-area: caller; background: var(--os-info-soft); }
.submit-relation { grid-area: submit; }
.device-node { grid-area: device; background: var(--os-warning-soft); }
.wake-relation { grid-area: wake; }
.reset-relation { grid-area: reset; }
.complete-relation { grid-area: complete; }
.scheduler-node { grid-area: scheduler; background: var(--os-active-soft); }
.poll-relation { grid-area: poll; }
.ring-node { grid-area: rings; }
.object-node { min-width: 0; padding: .75rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-panel); color: var(--os-ink); }
.object-node.focused { outline: 2px solid var(--os-focus); outline-offset: 1px; }
.object-node h4 { margin: 0 0 .4rem; font-size: var(--os-font-md); }
.object-node p { margin: 0 0 .4rem; line-height: 1.45; overflow-wrap: anywhere; }
.object-node > code, .data-object code { display: block; overflow-wrap: anywhere; font-family: var(--vp-font-family-mono); }
.object-node p code { display: inline; }
.object-field { display: grid; grid-template-columns: minmax(0,auto) minmax(0,1fr); gap: .35rem; align-items: baseline; margin-top: .4rem; font-size: var(--os-font-xs); }
.object-field code { color: var(--os-muted); }
.object-field span { overflow-wrap: anywhere; }
.system-state { display: block; margin-top: .45rem; color: var(--os-error); }
.object-node, .relation { font-size: max(var(--os-font-sm),13px); }
.relation { display: flex; min-width: 0; align-items: center; justify-content: center; gap: .3rem; color: var(--os-muted); text-align: center; line-height: 1.3; overflow-wrap: anywhere; }
.relation span { min-width: 0; overflow-wrap: anywhere; word-break: break-word; }
.relation b { flex: 0 0 auto; color: currentColor; font-size: 1.25rem; }
.relation.active { color: var(--os-active); font-weight: 650; }
.submit-relation.active { color: var(--os-info); }
.complete-relation.active, .reset-relation.active { color: var(--os-warning); }
.event-source { display: grid; gap: .2rem; justify-items: center; padding: .35rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-control); background: var(--os-panel); font-weight: 400; }
.event-source code { font-size: var(--os-font-xs); overflow-wrap: anywhere; }
.event-source small { font-size: var(--os-font-xs); }
.reset-relation:not(.active) { visibility: hidden; }
.data-object { display: grid; grid-template-columns: minmax(7rem,.8fr) minmax(10rem,1.1fr) minmax(10rem,1.1fr); align-items: stretch; gap: .6rem; margin-top: .8rem; padding: .7rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-panel); background: var(--os-soft); }
.buffer-object { display: grid; align-content: center; gap: .2rem; min-width: 0; padding: .55rem; border: 1px solid var(--os-line); border-radius: var(--os-radius-control); background: var(--os-panel); }
.buffer-object small, .buffer-object span { color: var(--os-muted); font-size: var(--os-font-xs); overflow-wrap: anywhere; }
.buffer-object code { font-size: var(--os-font-sm); }
.copy-relation { display: grid; place-items: center; min-width: 0; color: var(--os-muted); text-align: center; }
.copy-relation strong { font-size: var(--os-font-sm); font-weight: 500; overflow-wrap: anywhere; }
.copy-relation.copied { color: var(--os-active); font-weight: 700; }
.inspector h3 { margin-bottom: .5rem; }
.inspector-group { margin-top: .6rem; padding-top: .55rem; border-top: 1px solid var(--os-line); }
.inspector-group h4 { margin: 0 0 .35rem; color: var(--os-muted); font-size: var(--os-font-sm); font-weight: 650; }
.inspector-group dl { display: grid; gap: .25rem; margin: 0; }
.field-row { display: grid; grid-template-columns: minmax(0,1.1fr) minmax(0,1fr); align-items: start; gap: .45rem; padding: .25rem 0; }
.field-row dt, .field-row dd { min-width: 0; margin: 0; font-size: max(var(--os-font-xs),13px); line-height: 1.45; overflow-wrap: anywhere; }
.field-row dt code { font-family: var(--vp-font-family-mono); font-size: inherit; color: var(--os-muted); overflow-wrap: normal; word-break: normal; }
.field-row dd { font-family: var(--vp-font-family-mono); color: var(--os-ink); }
.explanation { margin-top: .8rem; }
.explanation p { max-width: 80ch; margin: .45rem 0; line-height: 1.55; }
.explanation-grid { display: grid; grid-template-columns: repeat(3,minmax(0,1fr)); gap: .8rem; }
.explanation-grid h4 { margin: 0 0 .3rem; font-size: var(--os-font-md); }
.explanation-grid p, .source-links { margin: 0; line-height: 1.55; overflow-wrap: anywhere; }
.source-links { margin-top: .7rem; }
.model-note { margin: .75rem 0 0; color: var(--os-muted); font-size: var(--os-font-xs); line-height: 1.5; }
@container block-lab (max-width: 940px) { .workbench { grid-template-columns: minmax(0,1fr); } }
@container block-lab (max-width: 650px) {
  .block-lab { padding: .85rem; }
  .lab-heading { display: grid; align-items: start; }
  .scenario-picker { min-width: 0; width: 100%; }
  .flow-grid { grid-template-columns: minmax(0,1fr); grid-template-areas: 'caller' 'submit' 'device' 'complete' 'rings' 'poll' 'scheduler' 'wake' 'reset'; }
  .relation { min-height: 2.75rem; }
  .relation b { transform: rotate(90deg); }
  .wake-relation { flex-direction: column; }
  .wake-relation b { transform: none; }
  .event-source { width: 100%; max-width: 100%; box-sizing: border-box; }
  .event-source code { max-width: 100%; overflow-wrap: anywhere; word-break: normal; }
  .data-object { grid-template-columns: minmax(0,1fr); }
  .explanation-grid { grid-template-columns: minmax(0,1fr); }
  .step-count { width: 100%; margin-left: 0; }
  .field-row { grid-template-columns: minmax(0,1fr); gap: .15rem; }
}
@media (prefers-reduced-motion: reduce) { *, *::before, *::after { scroll-behavior: auto !important; transition: none !important; animation: none !important; } }
</style>
