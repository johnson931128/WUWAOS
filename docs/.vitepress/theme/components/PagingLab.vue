<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'

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

    <p class="step-announcement" aria-live="polite" aria-atomic="true">
      {{ current.title }}。{{ current.detail }}
    </p>

    <div class="lab-layout">
      <div class="candidate-panel">
        <h4>FIFO 掃描快照：本次選擇前</h4>
        <p class="panel-note">arrival 由舊到新；命中不改 arrival。下表固定展示選擇前欄位，動態 pin 與換出狀態看右側。</p>
        <div class="candidate-list" role="list" aria-label="FIFO 候選頁">
          <div class="candidate chosen" role="listitem" :aria-current="current.phase === 'choose' ? 'step' : undefined">
            <span class="page-name">P1 · page B <b>victim</b></span>
            <span>arrival <strong>41</strong></span>
            <span>pins <strong>0</strong></span>
            <span>dirty <strong>yes</strong></span>
            <span class="eligibility">最早未固定 · 本次 victim</span>
          </div>
          <div class="candidate pinned">
            <span class="page-name">P2 · page C</span>
            <span>arrival <strong>42</strong></span>
            <span>pins <strong>1</strong></span>
            <span>dirty <strong>yes</strong></span>
            <span class="eligibility">略過 · pinned</span>
          </div>
          <div class="candidate">
            <span class="page-name">P2 · page A</span>
            <span>arrival <strong>43</strong></span>
            <span>pins <strong>1</strong></span>
            <span>dirty <strong>no</strong></span>
            <span class="eligibility">略過 · pinned</span>
          </div>
        </div>
        <p class="panel-note">全域掃描範圍為最多 16 個 process 加一個 exec 備用空間；核心頁與 page table 不置換。</p>
      </div>

      <div class="state-panel" :data-phase="current.phase">
        <div class="state-topline">
          <span class="phase-label">{{ current.phase }} · {{ current.actor }}</span>
          <span>{{ current.result }}</span>
        </div>
        <h4>{{ current.title }}</h4>
        <p>{{ current.detail }}</p>
        <div class="memory-map" aria-label="victim 頁資料位置">
          <div class="memory-card" :class="{ active: !current.revoked, locked: current.pinned }">
            <span>victim PTE</span>
            <strong>{{ current.pte }}</strong>
          </div>
          <div class="memory-card" :class="{ active: !current.revoked, locked: current.pinned }">
            <span>RAM frame</span>
            <strong>{{ current.ram }}</strong>
          </div>
          <div class="memory-card" :class="{ written: current.io > 0 || current.phase === 'commit', released: current.phase === 'rollback' }">
            <span>Swap location</span>
            <strong>{{ current.swap }}</strong>
          </div>
        </div>
        <div class="io-progress" :aria-label="`Swap 寫入 ${current.io} 個磁區，共 8 個`">
          <span v-for="sector in 8" :key="sector" class="sector" :class="{ done: sector <= current.io, failed: sector === current.failedAt }" :aria-label="sector === current.failedAt ? 'sector ' + sector + ' failed' : 'sector ' + sector + (sector <= current.io ? ' written' : ' pending')">{{ sector }}</span>
        </div>
        <div class="gate-status">
          <span class="gate-icon" aria-hidden="true">G</span>
          <span><b>VM Gate / caller</b>{{ current.gate }}</span>
        </div>
      </div>
    </div>
    <p class="lab-footnote">頁號、arrival、slot 與 sector 結果都是固定的教學資料，不是即時 QEMU trace。播放預設停止；鍵盤與螢幕閱讀器可操作所有控制。</p>
  </section>
</template>

<style scoped>
.paging-lab {
  --lab-border: var(--vp-c-divider);
  --lab-surface: var(--vp-c-bg-soft);
  --lab-muted: var(--vp-c-text-2);
  --lab-accent: var(--os-memory);
  margin: 1.5rem 0 2rem;
  padding: 1.25rem;
  border: 1px solid var(--lab-border);
  border-radius: 16px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
}
.lab-heading, .lab-controls, .state-topline { display: flex; align-items: center; justify-content: space-between; gap: .75rem; }
.lab-heading { margin-bottom: 1rem; }
.lab-heading h3, .state-panel h4, .candidate-panel h4 { margin: 0; }
.eyebrow { margin: 0 0 .2rem; color: var(--lab-muted); font-size: .78rem; letter-spacing: .04em; }
.scenario-select { display: grid; gap: .25rem; color: var(--lab-muted); font-size: .82rem; }
.scenario-select select, .lab-controls button {
  min-height: 40px;
  border: 1px solid var(--lab-border);
  border-radius: 8px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  font: inherit;
}
.scenario-select select { padding: .4rem 2rem .4rem .65rem; }
.lab-controls { justify-content: flex-start; flex-wrap: wrap; margin-bottom: .85rem; }
.lab-controls button { padding: .45rem .8rem; cursor: pointer; }
.lab-controls button:hover:not(:disabled) { border-color: var(--lab-accent); }
.lab-controls button:focus-visible, .scenario-select select:focus-visible { outline: 2px solid var(--lab-accent); outline-offset: 2px; }
.lab-controls button:disabled { cursor: not-allowed; opacity: .48; }
.step-count { margin-left: auto; color: var(--lab-muted); font-variant-numeric: tabular-nums; }
.step-announcement { min-height: 3.2em; margin: 0 0 1rem; padding: .7rem .85rem; border-left: 3px solid var(--lab-accent); background: var(--lab-surface); }
.lab-layout { display: grid; grid-template-columns: minmax(250px, .9fr) minmax(0, 1.4fr); gap: 1rem; align-items: stretch; }
.candidate-panel, .state-panel { min-width: 0; padding: 1rem; border: 1px solid var(--lab-border); border-radius: 12px; background: var(--lab-surface); }
.candidate-panel h4 { font-size: .98rem; }
.panel-note { margin: .4rem 0 .8rem; color: var(--lab-muted); font-size: .82rem; }
.candidate-list { display: grid; gap: .55rem; }
.candidate { display: grid; grid-template-columns: 1fr auto auto auto; gap: .25rem .6rem; align-items: center; padding: .65rem; border: 1px solid var(--lab-border); border-radius: 9px; background: var(--vp-c-bg); font-size: .78rem; }
.candidate.chosen { border-color: var(--lab-accent); }
.candidate.pinned { opacity: .76; }
.page-name { grid-column: 1 / -1; font-size: .84rem; }
.page-name b { margin-left: .4rem; color: var(--lab-accent); font-size: .72rem; }
.eligibility { grid-column: 1 / -1; color: var(--lab-muted); }
.state-panel { transition: border-color .2s ease, box-shadow .2s ease; }
.state-panel[data-phase='io'] { border-color: var(--lab-accent); box-shadow: inset 0 3px 0 var(--lab-accent); }
.state-panel[data-phase='rollback'] { border-color: var(--vp-c-danger-1, #c44); }
.phase-label { padding: .18rem .55rem; border-radius: 999px; background: var(--vp-c-bg); color: var(--lab-muted); font-size: .75rem; }
.state-topline > span:last-child { color: var(--lab-muted); font-size: .8rem; text-align: right; }
.state-panel h4 { margin: .8rem 0 .35rem; font-size: 1rem; }
.state-panel > p { min-height: 3.5em; margin: 0 0 .85rem; color: var(--lab-muted); font-size: .86rem; }
.memory-map { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: .5rem; }
.memory-card { display: grid; align-content: start; gap: .3rem; min-height: 90px; padding: .65rem; border: 1px solid var(--lab-border); border-radius: 9px; background: var(--vp-c-bg); transition: border-color .2s ease, transform .2s ease; }
.memory-card span { color: var(--lab-muted); font-size: .73rem; }
.memory-card strong { overflow-wrap: anywhere; font-size: .8rem; }
.memory-card.active, .memory-card.locked { border-color: var(--lab-accent); }
.memory-card.locked { transform: translateY(-2px); }
.memory-card.written { border-color: var(--vp-c-green-1, #398); }
.memory-card.released { border-style: dashed; }
.io-progress { display: grid; grid-template-columns: repeat(8, minmax(22px, 1fr)); gap: .3rem; margin: .8rem 0; }
.sector { display: grid; place-items: center; min-height: 25px; border: 1px solid var(--lab-border); border-radius: 5px; color: var(--lab-muted); font-size: .7rem; }
.sector.done { border-color: var(--lab-accent); background: var(--lab-accent); color: var(--vp-c-bg); }
.sector.failed { border-color: var(--vp-c-danger-1, #c44); background: var(--vp-c-danger-1, #c44); color: var(--vp-c-bg); }
.gate-status { display: flex; gap: .65rem; align-items: center; padding: .65rem; border-radius: 9px; background: var(--vp-c-bg); font-size: .8rem; }
.gate-status > span:last-child { display: grid; gap: .15rem; }
.gate-status b { font-size: .75rem; }
.gate-icon { display: grid; flex: 0 0 30px; place-items: center; width: 30px; height: 30px; border: 1px solid var(--lab-accent); border-radius: 50%; color: var(--lab-accent); font-weight: 700; }
.lab-footnote { margin: .9rem 0 0; color: var(--lab-muted); font-size: .78rem; }
@media (max-width: 760px) {
  .lab-layout { grid-template-columns: 1fr; }
  .lab-heading { align-items: flex-start; flex-direction: column; }
  .scenario-select { width: 100%; }
  .scenario-select select { width: 100%; }
  .step-count { margin-left: 0; }
}
@media (max-width: 440px) {
  .paging-lab { padding: .8rem; }
  .candidate { grid-template-columns: repeat(3, minmax(0, 1fr)); }
  .memory-map { grid-template-columns: 1fr; }
  .memory-card { min-height: 0; }
}
@media (prefers-reduced-motion: reduce) {
  .state-panel, .memory-card { transition: none; }
}
</style>
