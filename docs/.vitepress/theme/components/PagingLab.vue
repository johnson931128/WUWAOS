<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import StepTimeline from './StepTimeline.vue'

type Outcome = 'success' | 'failure'
type Frame = {
  title: string
  detail: string
  phase: string
  actor: string
  pte: string
  ram: string
  swap: string
  pinned: boolean
  revoked: boolean
  gate: string
  io: number
  failedAt?: number
  result: string
}

const makeFrames = (outcome: Outcome): Frame[] => {
  const result = outcome === 'success'
  const frames: Frame[] = [
    {
      title: '1 · FIFO 找到 eligible victim',
      detail: '全系統常駐頁按 arrival 由小到大掃描；排除 physical 為空或 pins 不為 0 的頁。讀寫命中不更新 arrival。',
      phase: 'choose', actor: 'VM · FIFO scan', pte: 'valid · R/W/U', ram: 'page B · dirty', swap: '未配置',
      pinned: false, revoked: false, gate: 'VM Gate 由 fault caller 持有', io: 0, result: '等待換出',
    },
    {
      title: '2 · 保存 PTE 與舊狀態',
      detail: '記下原始 leaf PTE、dirty 狀態與既有 slot。dirty page 沒有舊 slot 時先配置一個新 slot；此時仍不能先寫磁碟。',
      phase: 'prepare', actor: 'VM · save PTE / reserve slot', pte: 'saved: valid · R/W/U · A/D', ram: 'page B · dirty · 保留', swap: '新 slot 12 暫時保留',
      pinned: false, revoked: false, gate: 'Gate 持有；其他 VM 呼叫需等候', io: 0, result: '尚未撤銷映射',
    },
    {
      title: '3 · pin victim',
      detail: '遞增 victim pins，避免它被另一輪 FIFO 選走；同時延後取消目前 kernel call，直到狀態可安全提交或回復。',
      phase: 'pin', actor: 'VM · pin victim', pte: 'saved: valid · R/W/U · A/D', ram: 'page B · dirty · pin=1', swap: 'slot 12 暫時保留',
      pinned: true, revoked: false, gate: 'Gate 持有；原 fault caller 尚在核心', io: 0, result: '禁止換出',
    },
    {
      title: '4 · 撤銷 PTE 並 sfence.vma',
      detail: '先清除 victim PTE 並同步轉譯，再進入可能睡眠的 block I/O。原 fault caller 在 swap write 等待；其他 process 若也發生 page fault，會等待 VM Gate。',
      phase: 'revoke', actor: 'VM · revoke PTE / sfence.vma', pte: 'invalid · saved copy retained', ram: 'page B · dirty · pin=1', swap: 'slot 12 暫時保留',
      pinned: true, revoked: true, gate: 'Gate 持有 · 原 caller 睡眠 · 其他 fault 等 Gate', io: 0, result: '資料仍以 RAM 為準',
    },
  ]

  const completedSectors = result ? 8 : 2
  for (let sector = 1; sector <= completedSectors; sector += 1) {
    frames.push({
      title: `${4 + sector} · Swap 寫入磁區 ${sector}/8`,
      detail: `依序寫出 4 KiB page 的第 ${sector} 個 512-byte sector。I/O 仍由原始 fault call stack 等待完成；VM Gate 保持持有。`,
      phase: 'io', actor: 'fault caller · waits on Block', pte: 'invalid · saved copy retained', ram: 'page B · dirty · pin=1',
      swap: `slot 12 · ${sector}/8 sectors written`, pinned: true, revoked: true,
      gate: 'Gate 持有 · 原 caller 睡眠 · 其他 fault 等 Gate', io: sector,
      result: '未完成前不公布 slot',
    })
  }

  if (result) {
    frames.push({
      title: '13 · 全頁寫成功，提交換出',
      detail: '八個 sector 全部成功後才公布 page.slot=12、清除 dirty、釋放 RAM frame 並減少 resident 數。evict_one 返回外層 fault 處理；VM Gate 此時仍由外層 scope 持有。',
      phase: 'commit', actor: 'VM · publish slot / free frame', pte: 'invalid · page now in swap', ram: 'frame freed', swap: 'slot 12 · complete · published',
      pinned: false, revoked: true, gate: 'VM Gate 仍持有；原 fault caller 繼續安裝需求頁', io: 8, result: 'frame 可供原 fault caller 使用',
    })
  } else {
    frames.push({
      title: '7 · sector 3 失敗，回復 victim',
      detail: '還原 saved PTE 並 sfence.vma，保留 dirty RAM、解除 pin；釋放本次新配置的 slot。evict_one 會繼續嘗試其他 eligible victim；此示例已無其他候選，最後才回 ERR_IO。',
      phase: 'rollback', actor: 'VM · restore victim', pte: 'restored · valid · R/W/U · A/D', ram: 'page B · dirty · authoritative',
      swap: 'slot 12 · freed (new slot)', pinned: false, revoked: false,
      gate: 'VM Gate 仍持有；外層 fault 尚未完成', io: 2, failedAt: 3, result: '本候選回復；繼續掃描 FIFO',
    })
  }
  frames.push({
    title: result ? '14 · 原 fault 處理完成' : '8 · 外層 fault 回錯並離開 scope',
    detail: result
      ? '外層 vm_handle_fault 使用剛釋出的 frame 安裝需求頁並完成後，才離開 VM Gate。排隊的其他 VM 操作此時才可進入。'
      : '示例沒有其他 eligible victim，錯誤回到 vm_handle_fault；外層完成 fault 處理並離開 VM Gate，之後排隊的其他 VM 操作才可進入。',
    phase: 'done', actor: 'fault caller · finish VM scope', pte: result ? 'victim 在 slot 12' : 'victim PTE 已還原', ram: result ? 'frame 已交給 demand page' : 'victim dirty RAM 保留',
    swap: result ? 'slot 12 · published' : 'slot 12 · free', pinned: false, revoked: result,
    gate: '外層 scope 已完成並 leave；等待中的 fault 才能繼續', io: result ? 8 : 2, failedAt: result ? undefined : 3,
    result: result ? 'fault 完成後 Gate 才釋放' : 'ERR_IO 傳回後 Gate 才釋放',
  })
  return frames
}

const scenario = ref<Outcome>('success')
const index = ref(0)
const playing = ref(false)
let timer: ReturnType<typeof setInterval> | undefined

const frames = computed(() => makeFrames(scenario.value))
const current = computed(() => frames.value[index.value])
const explanation = computed(() => ({
  choose: { why: 'FIFO 按 arrival 選最早且未 pin 的常駐頁；pinned 頁會略過。', changes: '選定 victim，尚未撤銷 PTE 或寫入磁碟。', source: 'vm.c · evict_one' },
  prepare: { why: '先保存 PTE 與 dirty 狀態，並預留可能需要的新 Swap slot。', changes: 'slot 暫留；RAM 與原映射仍是權威副本。', source: 'vm.c · saved_entry' },
  pin: { why: 'pin 阻止另一輪 FIFO 在 I/O 期間選走 victim。', changes: 'VmPage.pins 遞增，外層 VM scope 持續持有 Gate。', source: 'vm.c · pin' },
  revoke: { why: '撤銷映射並執行 sfence.vma 後，才能在安全狀態等待 Block I/O。', changes: 'PTE invalid；saved_entry 與 RAM 副本保留。', source: 'vm.c · sfence.vma' },
  io: { why: 'dirty 4 KiB 頁以 8 個 512-byte sector 寫到 Swap。', changes: '只記錄已完成磁區；slot 尚未公布。', source: 'swap.c · swap_write' },
  commit: { why: '只有八個 sector 全成功才提交換出。', changes: '發布 page.slot、清 dirty、釋放 frame；外層 fault scope 仍持 Gate。', source: 'vm.c · victim commit' },
  rollback: { why: '任一 sector 失敗都要還原 victim，保留唯一有效的 RAM 副本。', changes: '還原 PTE 並 sfence.vma；解除 pin、釋放新 slot。', source: 'vm.c · rollback' },
  done: { why: 'VM Gate 由外層 fault scope 擁有，整個 fault 結束後才釋放。', changes: '其他等待中的 VM 操作此時才可進入。', source: 'vm.c · vm_scope_leave' },
})[current.value.phase] ?? { why: '目前 frame 展示固定教學狀態。', changes: current.value.result, source: 'vm.c · vm_handle_fault' })
const isFirst = computed(() => index.value === 0)
const isLast = computed(() => index.value === frames.value.length - 1)
const progress = computed(() => `${index.value + 1} / ${frames.value.length}`)

function stop() {
  playing.value = false
  if (timer !== undefined) clearInterval(timer)
  timer = undefined
}

function reset() {
  stop()
  index.value = 0
}

function next() {
  stop()
  if (index.value < frames.value.length - 1) index.value += 1
}

function previous() {
  stop()
  if (index.value > 0) index.value -= 1
}

function play() {
  if (isLast.value) index.value = 0
  if (playing.value) return
  playing.value = true
  timer = setInterval(() => {
    if (index.value >= frames.value.length - 1) {
      stop()
      return
    }
    index.value += 1
    if (index.value >= frames.value.length - 1) stop()
  }, 1400)
}

watch(scenario, reset)
onBeforeUnmount(stop)
</script>

<template>
  <section class="paging-lab" aria-labelledby="paging-lab-title">
    <div class="lab-heading">
      <div>
        <p class="eyebrow">可單步操作 · 教學示意</p>
        <h3 id="paging-lab-title">FIFO victim 與 Swap 交易</h3>
      </div>
      <label class="scenario-select">
        <span>情境</span>
        <select v-model="scenario" aria-label="選擇換出情境">
          <option value="success">Swap 寫入成功</option>
          <option value="failure">I/O 失敗並 rollback</option>
        </select>
      </label>
    </div>

    <div class="lab-controls" aria-label="互動播放控制">
      <button type="button" :disabled="isFirst" aria-label="回到上一步" @click="previous">上一步</button>
      <button type="button" :disabled="isLast" aria-label="前進一步" @click="next">下一步</button>
      <button type="button" :disabled="playing" aria-label="播放步驟" @click="play">播放</button>
      <button type="button" :disabled="!playing" aria-label="暫停播放" @click="stop">暫停</button>
      <button type="button" aria-label="重設情境" @click="reset">重設</button>
      <span class="step-count" aria-label="目前步數">{{ progress }}</span>
    </div>

    <div class="lab-layout" :data-phase="current.phase">
      <div class="system-panel">
        <div class="diagram-heading"><div><span class="phase-label">{{ current.phase }} · {{ current.actor }}</span><h4>{{ current.title }}</h4></div><span class="diagram-result">{{ current.result }}</span></div>
        <p class="diagram-caption">{{ current.detail }}</p>
        <div class="memory-map" role="img" :aria-label="`頁面狀態：PTE ${current.pte}，RAM ${current.ram}，Swap ${current.swap}`">
          <div class="memory-card pte-node" :class="{ active: !current.revoked, locked: current.pinned }"><span>虛擬頁 · leaf PTE</span><strong>{{ current.pte }}</strong><small>saved_entry 保留原始映射</small></div>
          <span class="map-link" :class="{ muted: current.revoked }"><b>maps</b><i aria-hidden="true">→</i></span>
          <div class="memory-card ram-node" :class="{ active: !current.revoked, locked: current.pinned }"><span>實體記憶體 · frame</span><strong>{{ current.ram }}</strong><small>pin={{ current.pinned ? 1 : 0 }} · dirty={{ current.phase === 'commit' || current.phase === 'done' && scenario === 'success' ? '0' : '1' }}</small></div>
          <span class="map-link" :class="{ active: current.io > 0 || current.phase === 'commit' }"><b>8 × 512 B</b><i aria-hidden="true">→</i></span>
          <div class="memory-card swap-node" :class="{ written: current.io > 0 || current.phase === 'commit' || current.phase === 'done' && scenario === 'success', released: current.phase === 'rollback' }"><span>Swap · slot 12</span><strong>{{ current.swap }}</strong><small>{{ current.io }} / 8 sectors written</small></div>
        </div>
        <div class="candidate-panel">
          <div class="candidate-heading"><h4>FIFO arrival order · selection snapshot</h4><p>Oldest eligible wins; pinned pages are skipped.</p></div>
          <div class="candidate-list" role="list" aria-label="FIFO 候選頁">
            <div class="candidate chosen" role="listitem" :aria-current="current.phase === 'choose' ? 'step' : undefined"><span class="page-name">41 · P1 page B</span><b>victim</b><span class="eligibility">dirty · pins 0 · selected</span></div>
            <div class="candidate pinned" role="listitem"><span class="page-name">42 · P2 page C</span><b>skip</b><span class="eligibility">dirty · pins 1 · pinned</span></div>
            <div class="candidate pinned" role="listitem"><span class="page-name">43 · P2 page A</span><b>skip</b><span class="eligibility">clean · pins 1 · pinned</span></div>
          </div>
        </div>
      </div>

      <aside class="inspector-panel" aria-label="目前步驟說明與欄位">
        <div class="state-topline">
          <span class="inspector-kicker">目前操作</span>
          <span>{{ current.phase }}</span>
        </div>
        <section class="explanation-block"><h4>為什麼會發生</h4><p>{{ explanation.why }}</p></section>
        <section class="explanation-block"><h4>狀態變化</h4><p>{{ explanation.changes }}</p></section>
        <dl class="field-inspector"><div><dt>VmPage.arrival</dt><dd>41 · 最早 eligible</dd></div><div><dt>VmPage.physical</dt><dd>{{ current.phase === 'commit' || current.phase === 'done' && scenario === 'success' ? 'released' : 'RAM frame B' }}</dd></div><div><dt>VmPage.slot</dt><dd>{{ current.phase === 'commit' || current.phase === 'done' && scenario === 'success' ? '12 · published' : '0 · no published slot' }}</dd></div><div><dt>reserved slot</dt><dd>{{ ['prepare','pin','revoke','io'].includes(current.phase) ? '12 · local only' : current.phase === 'rollback' ? '12 · freed after failure' : current.phase === 'commit' || current.phase === 'done' && scenario === 'success' ? 'transferred to VmPage.slot' : 'none' }}</dd></div><div><dt>VmPage.pins</dt><dd>{{ current.pinned ? '1 · pinned' : '0 · unpinned' }}</dd></div><div><dt>VmPage.dirty</dt><dd>{{ scenario === 'success' && ['commit','done'].includes(current.phase) ? 'false · clean' : scenario === 'failure' && current.phase === 'rollback' ? 'true · RAM retained' : 'true · dirty victim' }}</dd></div><div><dt>saved_entry</dt><dd>{{ ['prepare','pin','revoke','io'].includes(current.phase) || (scenario === 'failure' && current.phase === 'rollback') ? 'available for rollback' : current.phase === 'choose' ? 'captured next' : current.phase === 'commit' || current.phase === 'done' ? 'not needed' : 'restored' }}</dd></div><div><dt>Swap I/O</dt><dd>{{ current.io }} / 8 sectors<span v-if="current.failedAt"> · sector {{ current.failedAt }} failed</span></dd></div><div><dt>VM Gate</dt><dd>{{ current.gate }}</dd></div></dl>
        <div class="io-progress" role="img" :aria-label="`Swap 寫入 ${current.io} 個磁區，共 8 個`">
          <span v-for="sector in 8" :key="sector" class="sector" :class="{ done: sector <= current.io, failed: sector === current.failedAt }" :aria-label="sector === current.failedAt ? 'sector ' + sector + ' failed' : 'sector ' + sector + (sector <= current.io ? ' written' : ' pending')">{{ sector }}</span>
        </div>
        <div class="source-links"><a class="source-link" href="https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c">vm.c · {{ explanation.source }}</a><a class="source-link" href="https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/swap.c">swap.c · swap_write</a></div>
      </aside>
    </div>
    <p class="sr-status" aria-live="polite" aria-atomic="true">步驟 {{ progress }}：{{ current.title }}。{{ current.result }}</p>
    <p class="lab-footnote">固定教學資料，不是即時 QEMU trace。FIFO 掃描範圍包括最多 16 個 process 與 exec 備用空間；核心頁及 page table 不置換。</p>
    <StepTimeline :frames="frames" :current="index" label="換頁流程時間線" @seek="(i) => { stop(); index = i }" />
  </section>
</template>

<style scoped>
.paging-lab { container-type:inline-size; --line:var(--os-line); --panel:var(--os-panel); --soft:var(--os-soft); --ink:var(--os-ink); --muted:var(--os-muted); --accent:var(--os-accent); margin:1.5rem 0 2rem; padding:1.25rem; border:1px solid var(--line); border-radius:var(--os-radius-panel); background:var(--panel); color:var(--ink); }
.lab-heading,.lab-controls,.state-topline { display:flex; align-items:center; justify-content:space-between; gap:.75rem; }
.lab-heading { margin-bottom:1rem; }.lab-heading h3,.system-panel h4,.candidate-panel h4 { margin:0; }.eyebrow { margin:0 0 .2rem; color:var(--muted); font-size:var(--os-font-sm); }
.scenario-select { display:grid; gap:.25rem; color:var(--muted); font-size:var(--os-font-sm); }.scenario-select select,.lab-controls button { min-height:40px; border:1px solid var(--line); border-radius:var(--os-radius-control); background:var(--panel); color:var(--ink); font:inherit; }.scenario-select select { padding:.4rem 2rem .4rem .65rem; color-scheme:light dark; }
.lab-controls { justify-content:flex-start; flex-wrap:wrap; margin-bottom:1rem; }.lab-controls button { padding:.45rem .8rem; cursor:pointer; }.lab-controls button:hover:not(:disabled) { background:var(--soft); }.lab-controls button:focus-visible,.scenario-select select:focus-visible,.source-link:focus-visible { outline:2px solid var(--os-focus); outline-offset:2px; }.lab-controls button:disabled { cursor:not-allowed; opacity:.58; }.step-count { margin-left:auto; color:var(--muted); font-variant-numeric:tabular-nums; }
.lab-layout { display:grid; grid-template-columns:minmax(0,1.85fr) minmax(18rem,1fr); gap:1rem; align-items:start; }.system-panel,.inspector-panel { min-width:0; border:1px solid var(--line); border-radius:var(--os-radius-workspace); background:var(--soft); }.system-panel { padding:1.1rem; }.inspector-panel { padding:1rem; background:var(--panel); }.diagram-heading { display:flex; align-items:flex-start; justify-content:space-between; gap:.8rem; }.diagram-heading h4 { margin:.55rem 0 0; font-size:1.08rem; }.phase-label { display:inline-flex; padding:.2rem .55rem; border-radius:999px; background:var(--panel); color:var(--muted); font-size:var(--os-font-sm); }.diagram-result { color:var(--muted); font-size:var(--os-font-sm); text-align:right; }.diagram-caption { max-width:72ch; margin:.6rem 0 1rem; color:var(--muted); line-height:1.55; }
.memory-map { display:grid; grid-template-columns:minmax(0,1fr) minmax(3.4rem,.35fr) minmax(0,1fr) minmax(3.8rem,.4fr) minmax(0,1fr); align-items:center; gap:.55rem; padding:1rem; border:1px solid var(--line); border-radius:var(--os-radius-workspace); background:var(--panel); }.memory-card { display:grid; align-content:start; gap:.45rem; min-height:9rem; padding:.9rem; border:1px solid var(--line); border-radius:var(--os-radius-panel); background:var(--soft); }.memory-card > span { color:var(--muted); font-size:var(--os-font-sm); }.memory-card strong { font-size:var(--os-font-base); line-height:1.4; overflow-wrap:anywhere; }.memory-card small { color:var(--muted); font-size:var(--os-font-sm); line-height:1.4; }.memory-card.active { background:var(--os-active-soft); }.memory-card.locked { outline:2px solid var(--os-active); outline-offset:2px; }.memory-card.written { background:var(--os-info-soft); }.memory-card.released { background:var(--os-warning-soft); }.map-link { display:grid; justify-items:center; gap:.25rem; color:var(--muted); text-align:center; }.map-link b { font-size:var(--os-font-xs); font-weight:600; }.map-link i { color:var(--os-info); font-size:1.7rem; font-style:normal; line-height:1; }.map-link.active i { color:var(--os-active); }.map-link.muted i { color:var(--muted); }
.candidate-panel { margin-top:1rem; padding-top:.9rem; border-top:1px solid var(--line); }.candidate-heading { display:flex; align-items:baseline; justify-content:space-between; gap:.7rem; margin-bottom:.6rem; }.candidate-heading p { margin:0; color:var(--muted); font-size:var(--os-font-sm); }.candidate-list { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:.55rem; }.candidate { display:grid; grid-template-columns:minmax(0,1fr) auto; gap:.3rem .45rem; align-items:center; min-width:0; padding:.65rem; border:1px solid var(--line); border-radius:var(--os-radius-control); background:var(--panel); font-size:var(--os-font-sm); }.candidate b { padding:.12rem .4rem; border-radius:999px; background:var(--os-active-soft); color:var(--os-active); font-size:var(--os-font-xs); font-weight:650; }.candidate.pinned b { background:var(--os-warning-soft); color:var(--os-ink); }.page-name { font-weight:650; }.eligibility { grid-column:1/-1; color:var(--muted); }
.state-topline { padding-bottom:.7rem; border-bottom:1px solid var(--line); color:var(--muted); font-size:var(--os-font-sm); }.inspector-kicker { color:var(--ink); font-weight:650; }.explanation-block { padding:.8rem 0; border-bottom:1px solid var(--line); }.explanation-block h4 { margin:0 0 .3rem; font-size:var(--os-font-base); }.explanation-block p { margin:0; color:var(--muted); font-size:var(--os-font-sm); line-height:1.55; }.field-inspector { display:grid; grid-template-columns:minmax(0,1fr); margin:.4rem 0; }.field-inspector>div { display:grid; grid-template-columns:minmax(7rem,.9fr) minmax(0,1.2fr); gap:.5rem; padding:.48rem 0; border-bottom:1px solid var(--line); }.field-inspector dt { color:var(--muted); font:var(--os-font-sm) var(--vp-font-family-mono); }.field-inspector dd { margin:0; font-size:var(--os-font-sm); line-height:1.4; overflow-wrap:anywhere; }.io-progress { display:grid; grid-template-columns:repeat(8,minmax(0,1fr)); gap:.3rem; margin:.8rem 0; }.sector { display:grid; place-items:center; min-width:0; min-height:28px; border:1px solid var(--line); border-radius:var(--os-radius-control); color:var(--muted); font-size:var(--os-font-sm); }.sector.done { border-color:var(--os-active); background:var(--os-active); color:var(--os-on-accent); }.sector.failed { border-color:var(--os-error); background:var(--os-error); color:var(--os-on-accent); }.source-links { display:grid; gap:.3rem; margin-top:.6rem; }.source-link { display:inline-block; color:var(--os-info); font-size:var(--os-font-sm); }.lab-footnote { margin:.9rem 0 0; color:var(--muted); font-size:var(--os-font-sm); }.sr-status { position:absolute; width:1px; height:1px; padding:0; margin:-1px; overflow:hidden; clip:rect(0,0,0,0); white-space:nowrap; border:0; }
@container (max-width: 900px) { .lab-layout { grid-template-columns:1fr; } .inspector-panel { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:0 1rem; } .state-topline,.field-inspector,.source-links { grid-column:1/-1; } }
@media (max-width:760px) { .lab-layout { grid-template-columns:1fr; }.inspector-panel { display:block; }.memory-map { grid-template-columns:1fr; }.map-link { grid-template-columns:1fr auto 1fr; justify-items:center; }.map-link i { transform:rotate(90deg); }.candidate-list { grid-template-columns:1fr; }.lab-heading { align-items:flex-start; flex-direction:column; }.scenario-select,.scenario-select select { width:100%; }.step-count { margin-left:0; } }
@media (max-width:440px) { .paging-lab { padding:.8rem; }.system-panel,.inspector-panel { padding:.8rem; }.candidate-heading { align-items:flex-start; flex-direction:column; }.field-inspector>div { grid-template-columns:minmax(0,1fr); gap:.2rem; } }
@media (prefers-reduced-motion:reduce) { *,*::before,*::after { scroll-behavior:auto; } }
</style>
