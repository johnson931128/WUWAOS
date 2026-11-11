<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import StepTimeline from './StepTimeline.vue'

type Snapshot = {
  stage: string
  note: string
  durable: string
  journal: string
  header: string
  home: string
  result: string
  fs: string
  active: string
  headerAttempted?: boolean
  commitDurable?: boolean
  payloadFlushed?: boolean
  homeFlushed?: boolean
  replay?: boolean
  warning?: string
}
type Scenario = { id: string; label: string; frames: Snapshot[] }

const replacement = 'note.txt: "OLD" → "NEW!" (4 bytes)'
const sectorTargets = 'target sectors: inode/block bitmap · new inode/data · parent directory entry'
const successFrames: Snapshot[] = [
  { stage: '建構替換交易', note: 'fs_replace 在同一 transaction 中配置新 inode，先把新資料與必要 metadata 寫進 RAM transaction_payload。舊 home sectors 尚未改動。', durable: '持久邊界：尚未開始', journal: 'RAM staging\n新 inode + 新 bytes + metadata', header: '舊 header 全零', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'staging' },
  { stage: '寫入 journal table 與 payload', note: '核心把 target sector table 和 staged payload 寫入 journal 區。此時 header 仍為零；重啟時這些資料不構成已提交交易。', durable: 'table / payload 寫入中，尚未 flush', journal: `${sectorTargets}\n${replacement}`, header: '舊 header 全零', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'journal' },
  { stage: 'flush journal payload', note: 'payload 與 target table 完整寫完後執行 block_flush，確保日誌資料先於 commit header 持久化。', durable: 'journal payload + target table 已 flush', journal: `${sectorTargets}\n${replacement}`, header: '舊 header 全零', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'journal', payloadFlushed: true },
  { stage: '開始寫 commit header', note: '設定 header_attempted 後才呼叫 header sector write。從這個呼叫開始，任何 I/O 失敗都保守視為結果不確定；header 尚未 flush，commit 還未確認持久。', durable: 'payload 已 flush；commit 尚未 durable', journal: `${sectorTargets}\n${replacement}`, header: 'WUJNL002 header write', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'header', headerAttempted: true, payloadFlushed: true },
  { stage: 'flush commit header', note: 'header 寫入並 flush 成功後，交易已 committed。這表示核心承諾要完成更新，不代表 home sectors 已全部更新。', durable: 'commit header 已 durable · redo 必須完成', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: 'note.txt → old inode\nold bytes 尚未更新', result: '尚未返回', fs: 'online', active: 'header', headerAttempted: true, commitDurable: true, payloadFlushed: true },
  { stage: 'checkpoint 到 home sectors', note: '核心依 target table 將 payload 寫到真正的 home sectors；替換交易把新 inode/data 與 directory entry 一併套用。', durable: 'committed journal 可供 redo', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: 'metadata + 新 inode/data 寫入中\nnote.txt entry 切換至新 inode', result: '尚未返回', fs: 'online', active: 'home', headerAttempted: true, commitDurable: true, payloadFlushed: true },
  { stage: 'flush home sectors', note: 'home writes 全部成功後 flush，讓檔案系統結構持久化。header 此時仍保留，因此重啟仍可驗證並安全重播。', durable: 'journal 與 home sectors 已 flush', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: 'note.txt → new inode\nnew bytes 已落到 home', result: '尚未返回', fs: 'online', active: 'home', headerAttempted: true, commitDurable: true, payloadFlushed: true, homeFlushed: true },
  { stage: '清除 header 並 flush', note: 'home checkpoint 確認後，核心清除 journal header 並 flush。交易完成，fs_replace 回傳新內容長度。', durable: 'home 已 durable；journal 已清除', journal: 'target table / payload 留存但無有效 header', header: '全零 · 無待 replay 交易', home: 'note.txt → new inode\nnew bytes 可讀', result: 'length = 4 bytes', fs: 'online', active: 'done', headerAttempted: true, commitDurable: true, payloadFlushed: true, homeFlushed: true },
]

const beforeHeaderFrames: Snapshot[] = [
  { stage: '建立替換交易', note: '新資料與目標 sectors 已在 RAM staging；尚未碰 home sectors。', durable: '持久邊界：尚未開始', journal: 'RAM staging\n新 inode + 新 bytes + metadata', header: '全零', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'staging' },
  { stage: 'payload 寫入遇到 I/O error', note: '本例假設 target table 或 payload 寫入／flush 在 header write 呼叫前失敗。header 保持為零，home 完全未被 checkpoint。核心將檔案系統標為 offline。', durable: '沒有 commit header；交易未提交', journal: 'target table / payload 可能部分寫入', header: '全零 · recovery 會忽略空 header', home: 'note.txt → old inode\nold bytes 不變', result: 'ERR_IO = -5', fs: 'offline', active: 'failed', warning: '這個情境限定在 header_attempted 尚未設為 1；header write 呼叫若已開始，回傳語意就改為 ERR_UNCERTAIN。' },
  { stage: '重新啟動檢查空 header', note: '若磁碟上的 header 確認全零，journal_recover 判定沒有已提交交易；本例的舊 home sectors 保持原樣。', durable: '沒有可 replay 的 committed header', journal: '殘留 payload 不會單獨套用', header: '全零', home: 'note.txt → old inode\nold bytes 保持舊版', result: '此次 caller 已收到 -5', fs: 'boot mount 可繼續', active: 'replay' },
]

const checkpointFrames: Snapshot[] = [
  { stage: 'commit header 已 flush', note: '本情境從已成功持久化的 committed header 開始；target table 與 payload 已先 flush。', durable: '有效 commit header + payload 已 durable', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: 'note.txt → old inode\nhome 尚未全數更新', result: '尚未返回', fs: 'online', active: 'header', headerAttempted: true, commitDurable: true, payloadFlushed: true },
  { stage: 'home checkpoint 寫入失敗', note: 'header 已開始寫入且已 flush；home sector write 或 home flush 失敗。fs offline，caller 收到 ERR_UNCERTAIN。這不是「一定沒寫入」，也不是「一定已完成」。', durable: '有效 committed journal 仍待處理', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header 保留', home: '部分 home sectors 可能已更新\n整筆結果尚不明', result: 'ERR_UNCERTAIN = -200', fs: 'offline', active: 'failed', headerAttempted: true, commitDurable: true, payloadFlushed: true, warning: '只有 reboot 時 header、target table、payload 都有效且 CRC 驗證通過，才會進入以下 replay。header 若撕裂／無效，核心拒絕掛載。' },
  { stage: '重啟：先驗證 journal', note: '假設本例 committed header、target table、payload 完整且 CRC 正確。recovery 先檢查所有記錄、sector 範圍與重複 target，再開始任何 home write。', durable: 'CRC / targets 已全部驗證', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: '讀取目前各 target 狀態\n允許 redo 整組資料', result: '原 caller 已收到 -200', fs: 'boot recovery', active: 'replay', headerAttempted: true, commitDurable: true, payloadFlushed: true, replay: true },
  { stage: 'redo home sectors', note: '驗證完成後，journal_recover 將完整 payload 重寫到所有 target sectors，再 flush 並清除 header。有效交易被補完為新版本。', durable: 'redo 後 home 已 flush', journal: 'redo source 已套用', header: 'replay 完成後清零', home: 'note.txt → new inode\nnew bytes 已完成', result: '新啟動的 FS 可掛載', fs: 'online after mount validation', active: 'done', headerAttempted: true, commitDurable: true, payloadFlushed: true, homeFlushed: true, replay: true },
]

const scenarios: Scenario[] = [
  { id: 'success', label: '完整成功 commit', frames: successFrames },
  { id: 'before-header', label: 'header 前 I/O error', frames: beforeHeaderFrames },
  { id: 'checkpoint', label: 'checkpoint 失敗與條件式 replay', frames: checkpointFrames },
]
const selectedId = ref(scenarios[0].id)
const index = ref(0)
const playing = ref(false)
let timer: ReturnType<typeof setInterval> | undefined
const selected = computed(() => scenarios.find((scenario) => scenario.id === selectedId.value) ?? scenarios[0])
const frame = computed(() => selected.value.frames[index.value])
const first = computed(() => index.value === 0)
const last = computed(() => index.value === selected.value.frames.length - 1)

function stop() {
  playing.value = false
  if (timer !== undefined) clearInterval(timer)
  timer = undefined
}
function goTo(nextIndex: number) {
  index.value = Math.max(0, Math.min(selected.value.frames.length - 1, nextIndex))
  if (last.value) stop()
}
function previous() { stop(); goTo(index.value - 1) }
function next() { stop(); goTo(index.value + 1) }
function reset() { stop(); index.value = 0 }
function chooseScenario(id: string) { stop(); selectedId.value = id; index.value = 0 }
function togglePlayback() {
  if (playing.value) { stop(); return }
  if (last.value) index.value = 0
  playing.value = true
  timer = setInterval(() => {
    if (index.value >= selected.value.frames.length - 1) stop()
    else goTo(index.value + 1)
  }, 1450)
}
watch(selectedId, stop)
onBeforeUnmount(stop)
</script>

<template>
  <section class="journal-lab" aria-labelledby="journal-lab-title">
    <div class="lab-heading">
      <div><p class="eyebrow">互動示意 · redo journal</p><h2 id="journal-lab-title">一筆更新何時算 committed？</h2><p class="intro">追蹤 RAM staging、journal、commit header 與 home sectors。每個 frame 都標出 durable boundary 和 caller 結果。</p></div>
      <div class="counter">{{ index + 1 }} <span>/ {{ selected.frames.length }}</span></div>
    </div>
    <div class="scenario-picker" role="group" aria-label="選擇 journal 示意情境">
      <button v-for="scenario in scenarios" :key="scenario.id" type="button" :aria-pressed="selectedId === scenario.id" @click="chooseScenario(scenario.id)">{{ scenario.label }}</button>
    </div>
    <div class="controls" role="group" aria-label="transaction 播放控制">
      <button type="button" :disabled="first" aria-label="上一步" @click="previous">← 上一步</button>
      <button type="button" :aria-label="playing ? '暫停播放' : '播放步驟'" @click="togglePlayback">{{ playing ? 'Ⅱ 暫停' : '▶ 播放' }}</button>
      <button type="button" :disabled="last" aria-label="下一步" @click="next">下一步 →</button>
      <button type="button" @click="reset">重設</button>
    </div>
    <div class="progress" role="progressbar" :aria-valuenow="index + 1" :aria-valuemin="1" :aria-valuemax="selected.frames.length" :aria-label="`交易步驟 ${index + 1} / ${selected.frames.length}`"><span :style="{ width: `${((index + 1) / selected.frames.length) * 100}%` }" /></div>

    <div class="journal-workbench" :data-phase="frame.active">
      <div class="system-panel">
        <div class="diagram-heading"><div><span class="phase-label">Step {{ String(index + 1).padStart(2, '0') }} · {{ frame.active }}</span><h3>{{ frame.stage }}</h3></div><span class="boundary-label" :class="{ committed: frame.commitDurable, uncertain: frame.headerAttempted && !frame.commitDurable }">{{ frame.commitDurable ? 'commit durable' : frame.headerAttempted ? 'header attempted' : 'not committed' }}</span></div>
        <p class="diagram-caption">{{ frame.note }}</p>
        <div class="journal-flow" role="img" :aria-label="`Redo path: RAM staging; target table and payload; commit header; home sectors. Current step: ${frame.stage}`">
          <article class="flow-ram" :class="{ active: frame.active === 'staging' }"><span>RAM staging</span><strong>Staged data</strong><small>transaction_payload · new inode and bytes</small></article>
          <b class="flow-to-journal" :class="{ active: ['journal','header','home','done'].includes(frame.active) }" aria-hidden="true">→</b>
          <article class="flow-journal" :class="{ active: frame.active === 'journal' || frame.active === 'replay' }"><span>Redo journal</span><strong>Target table + payload</strong><small>flush before header</small></article>
          <b class="flow-to-header" :class="{ active: ['header','home','done'].includes(frame.active) }" aria-hidden="true">↓</b>
          <article class="flow-header" :class="{ active: frame.active === 'header' }"><span>Commit header</span><strong>Commit record</strong><small>{{ frame.header }} · attempt differs from durable commit</small></article>
          <b class="flow-to-home" :class="{ active: ['home','done'].includes(frame.active) }" aria-hidden="true">←</b>
          <article class="flow-home" :class="{ active: frame.active === 'home' || frame.active === 'done' || frame.replay }"><span>Home sectors</span><strong>{{ frame.home.includes('部分') ? 'Checkpoint partly applied' : frame.home.includes('new inode') ? 'New inode on disk' : frame.home.includes('old inode') ? 'Old inode remains' : 'Home update in progress' }}</strong><small>checkpoint, flush, then clear header</small></article>
        </div>
        <div class="durability-track" aria-label="已確認持久化的邊界"><span :class="{ reached: frame.payloadFlushed }">1 · journal {{ frame.payloadFlushed ? '已確認 flush' : '待 flush' }}</span><i aria-hidden="true">→</i><span :class="{ reached: frame.commitDurable }">2 · commit header {{ frame.commitDurable ? '已確認 flush' : '待 flush' }}</span><i aria-hidden="true">→</i><span :class="{ reached: frame.homeFlushed }">3 · home {{ frame.homeFlushed ? '已確認 flush' : '待 flush' }}</span></div>
        <div v-if="frame.warning" class="warning-note"><strong>Recovery condition:</strong> {{ frame.warning }}</div>
      </div>
      <aside class="inspector-panel" aria-label="Transaction frame details">
        <section class="explanation-block"><h4>目前操作</h4><p>{{ frame.note }}</p></section>
        <section class="explanation-block"><h4>為什麼會發生</h4><p>{{ frame.durable }}</p></section>
        <section class="explanation-block"><h4>狀態變化</h4><div class="status-pills"><span :class="{ error: frame.fs.includes('offline') }">檔案系統：{{ frame.fs }}</span><span :class="{ error: frame.result.includes('-'), success: frame.result.includes('bytes') || frame.result.includes('掛載') }">呼叫結果：{{ frame.result }}</span><span>Recovery：{{ frame.replay ? 'journal 驗證通過' : '未執行' }}</span></div></section>
        <dl class="field-inspector"><div><dt>header_attempted</dt><dd>{{ frame.headerAttempted ? 'true' : 'false' }}</dd></div><div><dt>commitDurable · model</dt><dd>{{ frame.commitDurable ? 'true' : 'false' }}</dd></div><div><dt>journal header</dt><dd>{{ frame.header }}</dd></div><div><dt>table + payload</dt><dd>{{ frame.journal }}</dd></div><div><dt>home sectors</dt><dd>{{ frame.home }}</dd></div><div><dt>replay requires</dt><dd>{{ frame.replay ? 'valid header + table + payload CRC' : 'valid committed record' }}</dd></div></dl>
        <a class="source-link" href="https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c#L108-L220">Source: transaction_commit · journal_recover</a>
      </aside>
    </div>
    <p class="lab-footnote">commit header 一旦開始寫入，後續錯誤保守回 ERR_UNCERTAIN。有效 committed journal 可在 reboot redo；撕裂或無效 header 會拒絕掛載，不保證每個 uncertain 都能 replay。header_attempted 對應核心欄位；commitDurable 是此教學模型依 commit 後狀態推導的標記。</p>
    <p class="sr-status" aria-live="polite">{{ selected.label }}，步驟 {{ index + 1 }}／{{ selected.frames.length }}：{{ frame.stage }}。{{ playing ? '正在播放' : '播放已停止' }}</p>
    <StepTimeline :frames="selected.frames" :current="index" label="Journal transaction timeline" @seek="(i) => { stop(); index = i }" />
  </section>
</template>

<style scoped>
.journal-lab { container-type:inline-size; --line:var(--os-line); --panel:var(--os-panel); --soft:var(--os-soft); --ink:var(--os-ink); --muted:var(--os-muted); --accent:var(--os-accent); margin:1.5rem 0 2rem; padding:1.25rem; border:1px solid var(--line); border-radius:var(--os-radius-panel); background:var(--panel); color:var(--ink); }
.lab-heading,.result-row { display:flex; align-items:center; justify-content:space-between; gap:.8rem; }.lab-heading { align-items:flex-start; }.eyebrow { margin:0 0 .35rem; color:var(--muted); font-size:var(--os-font-sm); }.lab-heading h2 { margin:0; font-size:clamp(1.3rem,3vw,1.75rem); line-height:1.25; }.intro { max-width:68ch; margin:.55rem 0 0; color:var(--muted); font-size:var(--os-font-base); }.counter { flex:0 0 auto; color:var(--accent); font-size:1.35rem; font-variant-numeric:tabular-nums; font-weight:700; }.counter span { color:var(--muted); font-size:var(--os-font-base); font-weight:500; }
.scenario-picker,.controls { display:flex; flex-wrap:wrap; gap:.55rem; margin-top:1rem; }button { min-height:2.55rem; padding:.55rem .85rem; border:1px solid var(--line); border-radius:var(--os-radius-control); background:var(--panel); color:inherit; font:inherit; font-size:var(--os-font-sm); cursor:pointer; }button:hover:not(:disabled) { background:var(--soft); }button:focus-visible { outline:2px solid var(--os-focus); outline-offset:2px; }.scenario-picker button[aria-pressed="true"],.controls button:nth-child(2) { background:var(--os-active-soft); color:var(--ink); }button:disabled { cursor:not-allowed; opacity:.58; }.progress { height:.35rem; margin-top:.95rem; overflow:hidden; border-radius:1rem; background:var(--line); }.progress span { display:block; height:100%; border-radius:inherit; background:var(--accent); }
.journal-workbench { display:grid; grid-template-columns:minmax(0,1.85fr) minmax(18rem,1fr); gap:1rem; align-items:start; margin-top:1rem; }.system-panel,.inspector-panel { min-width:0; border:1px solid var(--line); border-radius:var(--os-radius-workspace); background:var(--soft); }.system-panel { padding:1.1rem; }.inspector-panel { padding:1rem; background:var(--panel); }.diagram-heading { display:flex; align-items:flex-start; justify-content:space-between; gap:.8rem; }.diagram-heading h3 { margin:.55rem 0 0; font-size:1.08rem; }.phase-label { display:inline-flex; padding:.2rem .55rem; border-radius:999px; background:var(--panel); color:var(--muted); font-size:var(--os-font-sm); }.diagram-caption { max-width:72ch; margin:.65rem 0 1rem; color:var(--muted); line-height:1.55; }.boundary-label { flex:0 0 auto; padding:.35rem .55rem; border-radius:var(--os-radius-control); background:var(--soft); color:var(--muted); font-size:var(--os-font-sm); font-weight:650; }.boundary-label.committed { background:var(--os-active-soft); color:var(--os-active); }.boundary-label.uncertain { background:var(--os-warning-soft); color:var(--ink); }
.journal-flow { display:grid; grid-template-columns:minmax(0,1fr) 2rem minmax(0,1fr); grid-template-rows:minmax(8rem,auto) 1.6rem minmax(8rem,auto); grid-template-areas:"ram right journal" ". . down" "home left header"; align-items:stretch; gap:.45rem; padding:1rem; border:1px solid var(--line); border-radius:var(--os-radius-workspace); background:var(--panel); }.journal-flow article { display:grid; align-content:start; gap:.55rem; min-height:0; min-width:0; padding:.85rem; border:1px solid var(--line); border-radius:var(--os-radius-panel); background:var(--soft); }.journal-flow article.active { background:var(--os-active-soft); }.journal-flow article span,.journal-flow article small { color:var(--muted); font-size:var(--os-font-sm); line-height:1.4; }.journal-flow article strong { font-size:var(--os-font-base); line-height:1.45; overflow-wrap:anywhere; }.flow-ram { grid-area:ram; }.flow-journal { grid-area:journal; }.flow-header { grid-area:header; }.flow-home { grid-area:home; }.journal-flow > b { align-self:center; justify-self:center; color:var(--muted); font-size:1.35rem; }.journal-flow > b.active { color:var(--os-active); }.flow-to-journal { grid-area:right; }.flow-to-header { grid-area:down; }.flow-to-home { grid-area:left; }.durability-track { display:grid; grid-template-columns:minmax(0,1fr) auto minmax(0,1fr) auto minmax(0,1fr); align-items:center; gap:.5rem; margin-top:.8rem; padding:.65rem .8rem; border:1px solid var(--line); border-radius:var(--os-radius-control); background:var(--panel); }.durability-track span { color:var(--muted); font-size:var(--os-font-sm); text-align:center; }.durability-track span.reached { color:var(--os-active); font-weight:650; }.durability-track i { color:var(--muted); font-style:normal; }.warning-note { margin:.8rem 0 0; padding:.7rem .8rem; border:1px solid var(--line); border-radius:var(--os-radius-control); background:var(--os-warning-soft); color:var(--ink); line-height:1.5; }
.explanation-block { padding:.8rem 0; border-bottom:1px solid var(--line); }.explanation-block:first-child { padding-top:0; }.explanation-block h4 { margin:0 0 .35rem; font-size:var(--os-font-base); }.explanation-block p { margin:0; color:var(--muted); font-size:var(--os-font-sm); line-height:1.55; }.status-pills { display:flex; flex-wrap:wrap; gap:.4rem; }.status-pills span { padding:.25rem .45rem; border-radius:999px; background:var(--soft); font-size:var(--os-font-sm); }.status-pills .error { background:var(--os-error-soft); color:var(--os-ink); }.status-pills .success { background:var(--os-active-soft); color:var(--os-ink); }.field-inspector { display:grid; grid-template-columns:1fr; margin:.4rem 0; }.field-inspector>div { display:grid; grid-template-columns:minmax(7rem,.85fr) minmax(0,1.2fr); gap:.5rem; padding:.48rem 0; border-bottom:1px solid var(--line); }.field-inspector dt { color:var(--muted); font:var(--os-font-sm) var(--vp-font-family-mono); }.field-inspector dd { margin:0; font-size:var(--os-font-sm); line-height:1.45; overflow-wrap:anywhere; }.source-link { display:inline-block; margin-top:.7rem; color:var(--os-info); font-size:var(--os-font-sm); }.source-link:focus-visible { outline:2px solid var(--os-focus); outline-offset:2px; }.lab-footnote { margin:.7rem 0 0; color:var(--muted); font-size:var(--os-font-sm); line-height:1.55; }.sr-status { position:absolute; width:1px; height:1px; padding:0; margin:-1px; overflow:hidden; clip:rect(0,0,0,0); white-space:nowrap; border:0; }
@container (max-width:900px) { .journal-workbench { grid-template-columns:1fr; }.inspector-panel { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:0 1rem; }.explanation-block:first-child,.field-inspector,.source-link { grid-column:1/-1; } }
@media (max-width:760px) { .journal-workbench { grid-template-columns:1fr; }.inspector-panel { display:block; }.journal-flow { grid-template-columns:minmax(0,1fr); grid-template-rows:none; grid-auto-rows:auto; grid-template-areas:"ram" "right" "journal" "down" "header" "left" "home"; gap:.25rem; padding:.7rem; }.journal-flow article { min-height:0; }.journal-flow > b { font-size:1.1rem; }.flow-to-journal { transform:rotate(90deg); }.flow-to-header { transform:none; }.flow-to-home { transform:rotate(-90deg); }.durability-track { grid-template-columns:1fr; }.durability-track i { transform:rotate(90deg); justify-self:center; }.scenario-picker button,.controls button { flex:1 1 auto; } }
@media (max-width:440px) { .journal-lab { padding:.8rem; }.system-panel,.inspector-panel { padding:.8rem; }.diagram-heading { flex-direction:column; }.boundary-label { align-self:flex-start; }.field-inspector>div { grid-template-columns:minmax(0,1fr); gap:.2rem; } }
@media (prefers-reduced-motion:reduce) { *,*::before,*::after { scroll-behavior:auto; } }
</style>
