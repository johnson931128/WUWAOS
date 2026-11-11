<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'

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
  replay?: boolean
  warning?: string
}
type Scenario = { id: string; label: string; frames: Snapshot[] }

const replacement = 'note.txt: "OLD" → "NEW!" (4 bytes)'
const sectorTargets = 'target sectors: inode/block bitmap · new inode/data · parent directory entry'
const successFrames: Snapshot[] = [
  { stage: '建構替換交易', note: 'fs_replace 在同一 transaction 中配置新 inode，先把新資料與必要 metadata 寫進 RAM transaction_payload。舊 home sectors 尚未改動。', durable: '持久邊界：尚未開始', journal: 'RAM staging\n新 inode + 新 bytes + metadata', header: '舊 header 全零', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'staging' },
  { stage: '寫入 journal table 與 payload', note: '核心把 target sector table 和 staged payload 寫入 journal 區。此時 header 仍為零；重啟時這些資料不構成已提交交易。', durable: 'table / payload 寫入中，尚未 flush', journal: `${sectorTargets}\n${replacement}`, header: '舊 header 全零', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'journal' },
  { stage: 'flush journal payload', note: 'payload 與 target table 完整寫完後執行 block_flush，確保日誌資料先於 commit header 持久化。', durable: 'journal payload + target table 已 flush', journal: `${sectorTargets}\n${replacement}`, header: '舊 header 全零', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'journal' },
  { stage: '開始寫 commit header', note: '設定 header_attempted 後才呼叫 header sector write。從這個呼叫開始，任何 I/O 失敗都保守視為結果不確定；header 尚未 flush，commit 還未確認持久。', durable: 'payload 已 flush；commit 尚未 durable', journal: `${sectorTargets}\n${replacement}`, header: 'WUJNL002 header write', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'header', headerAttempted: true },
  { stage: 'flush commit header', note: 'header 寫入並 flush 成功後，交易已 committed。這表示核心承諾要完成更新，不代表 home sectors 已全部更新。', durable: 'commit header 已 durable · redo 必須完成', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: 'note.txt → old inode\nold bytes 尚未更新', result: '尚未返回', fs: 'online', active: 'header', headerAttempted: true, commitDurable: true },
  { stage: 'checkpoint 到 home sectors', note: '核心依 target table 將 payload 寫到真正的 home sectors；替換交易把新 inode/data 與 directory entry 一併套用。', durable: 'committed journal 可供 redo', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: 'metadata + 新 inode/data 寫入中\nnote.txt entry 切換至新 inode', result: '尚未返回', fs: 'online', active: 'home', headerAttempted: true, commitDurable: true },
  { stage: 'flush home sectors', note: 'home writes 全部成功後 flush，讓檔案系統結構持久化。header 此時仍保留，因此重啟仍可驗證並安全重播。', durable: 'journal 與 home sectors 已 flush', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: 'note.txt → new inode\nnew bytes 已落到 home', result: '尚未返回', fs: 'online', active: 'home', headerAttempted: true, commitDurable: true },
  { stage: '清除 header 並 flush', note: 'home checkpoint 確認後，核心清除 journal header 並 flush。交易完成，fs_replace 回傳新內容長度。', durable: 'home 已 durable；journal 已清除', journal: 'target table / payload 留存但無有效 header', header: '全零 · 無待 replay 交易', home: 'note.txt → new inode\nnew bytes 可讀', result: 'length = 4 bytes', fs: 'online', active: 'done', headerAttempted: true, commitDurable: true },
]

const beforeHeaderFrames: Snapshot[] = [
  { stage: '建立替換交易', note: '新資料與目標 sectors 已在 RAM staging；尚未碰 home sectors。', durable: '持久邊界：尚未開始', journal: 'RAM staging\n新 inode + 新 bytes + metadata', header: '全零', home: 'note.txt → old inode\nold bytes 可讀', result: '尚未返回', fs: 'online', active: 'staging' },
  { stage: 'payload 寫入遇到 I/O error', note: '本例假設 target table 或 payload 寫入／flush 在 header write 呼叫前失敗。header 保持為零，home 完全未被 checkpoint。核心將檔案系統標為 offline。', durable: '沒有 commit header；交易未提交', journal: 'target table / payload 可能部分寫入', header: '全零 · recovery 會忽略空 header', home: 'note.txt → old inode\nold bytes 不變', result: 'ERR_IO = -5', fs: 'offline', active: 'failed', warning: '這個情境限定在 header_attempted 尚未設為 1；header write 呼叫若已開始，回傳語意就改為 ERR_UNCERTAIN。' },
  { stage: '重新啟動檢查空 header', note: '若磁碟上的 header 確認全零，journal_recover 判定沒有已提交交易；本例的舊 home sectors 保持原樣。', durable: '沒有可 replay 的 committed header', journal: '殘留 payload 不會單獨套用', header: '全零', home: 'note.txt → old inode\nold bytes 保持舊版', result: '此次 caller 已收到 -5', fs: 'boot mount 可繼續', active: 'replay' },
]

const checkpointFrames: Snapshot[] = [
  { stage: 'commit header 已 flush', note: '本情境從已成功持久化的 committed header 開始；target table 與 payload 已先 flush。', durable: '有效 commit header + payload 已 durable', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: 'note.txt → old inode\nhome 尚未全數更新', result: '尚未返回', fs: 'online', active: 'header' },
  { stage: 'home checkpoint 寫入失敗', note: 'header 已開始寫入且已 flush；home sector write 或 home flush 失敗。fs offline，caller 收到 ERR_UNCERTAIN。這不是「一定沒寫入」，也不是「一定已完成」。', durable: '有效 committed journal 仍待處理', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header 保留', home: '部分 home sectors 可能已更新\n整筆結果尚不明', result: 'ERR_UNCERTAIN = -200', fs: 'offline', active: 'failed', headerAttempted: true, commitDurable: true, warning: '只有 reboot 時 header、target table、payload 都有效且 CRC 驗證通過，才會進入以下 replay。header 若撕裂／無效，核心拒絕掛載。' },
  { stage: '重啟：先驗證 journal', note: '假設本例 committed header、target table、payload 完整且 CRC 正確。recovery 先檢查所有記錄、sector 範圍與重複 target，再開始任何 home write。', durable: 'CRC / targets 已全部驗證', journal: `${sectorTargets}\n${replacement}`, header: '有效 committed header', home: '讀取目前各 target 狀態\n允許 redo 整組資料', result: '原 caller 已收到 -200', fs: 'boot recovery', active: 'replay', headerAttempted: true, commitDurable: true, replay: true },
  { stage: 'redo home sectors', note: '驗證完成後，journal_recover 將完整 payload 重寫到所有 target sectors，再 flush 並清除 header。有效交易被補完為新版本。', durable: 'redo 後 home 已 flush', journal: 'redo source 已套用', header: 'replay 完成後清零', home: 'note.txt → new inode\nnew bytes 已完成', result: '新啟動的 FS 可掛載', fs: 'online after mount validation', active: 'done', headerAttempted: true, commitDurable: true, replay: true },
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

    <div class="frame-copy" aria-live="polite" aria-atomic="true">
      <div class="frame-title"><span>STEP {{ String(index + 1).padStart(2, '0') }}</span><h3>{{ frame.stage }}</h3></div>
      <p>{{ frame.note }}</p>
      <p v-if="frame.warning" class="warning-note"><strong>條件：</strong>{{ frame.warning }}</p>
    </div>

    <div class="boundary" :class="{ 'is-committed': frame.commitDurable }">
      <span class="boundary-icon" aria-hidden="true">{{ frame.commitDurable ? '◆' : frame.headerAttempted ? '!' : '◇' }}</span>
      <div><strong>Durable boundary</strong><p>{{ frame.durable }}</p></div>
      <span class="boundary-label">{{ frame.commitDurable ? 'commit 已 durable' : frame.headerAttempted ? 'header attempt 已開始' : '尚未 committed' }}</span>
    </div>

    <div class="storage-grid">
      <article class="storage-card staging" :class="{ active: frame.active === 'staging' }">
        <div class="card-heading"><span class="card-mark">RAM</span><div><h4>Transaction staging</h4><p>暫存 sector payload</p></div></div>
        <pre>{{ frame.journal.includes('RAM staging') ? frame.journal : 'write_sector() 將目標扇區暫存於核心 RAM' }}</pre>
      </article>
      <article class="storage-card journal" :class="{ active: frame.active === 'journal' || frame.replay }">
        <div class="card-heading"><span class="card-mark">LOG</span><div><h4>Journal table + payload</h4><p>先寫資料，再 flush</p></div></div>
        <pre>{{ frame.active === 'staging' ? '尚未寫入 journal' : frame.journal }}</pre>
      </article>
      <article class="storage-card header" :class="{ active: frame.active === 'header' }">
        <div class="card-heading"><span class="card-mark">H</span><div><h4>Commit header</h4><p>write attempt 是回錯界線</p></div></div>
        <pre>{{ frame.header }}</pre>
      </article>
      <article class="storage-card home" :class="{ active: frame.active === 'home' || frame.active === 'done' }">
        <div class="card-heading"><span class="card-mark">DISK</span><div><h4>Home sectors</h4><p>實際檔案系統結構</p></div></div>
        <pre>{{ frame.home }}</pre>
      </article>
    </div>

    <div class="result-row" aria-live="polite" aria-atomic="true">
      <div><span>FS 狀態</span><strong :class="{ offline: frame.fs.includes('offline'), online: frame.fs.includes('online') }">{{ frame.fs }}</strong></div>
      <div><span>Caller 結果</span><strong :class="{ error: frame.result.includes('-'), success: frame.result.includes('bytes') || frame.result.includes('掛載') }">{{ frame.result }}</strong></div>
    </div>
    <p class="lab-footnote">commit header 一旦開始寫入，後續錯誤保守回 ERR_UNCERTAIN。有效 committed journal 可在 reboot redo；撕裂或無效 header 會拒絕掛載，不保證每個 uncertain 都能 replay。</p>
    <p class="sr-status" aria-live="polite">{{ selected.label }}，步驟 {{ index + 1 }}／{{ selected.frames.length }}：{{ frame.stage }}。{{ playing ? '正在播放' : '播放已停止' }}</p>
  </section>
</template>

<style scoped>
.journal-lab {
  --j-border: var(--vp-c-divider, #d9dee8);
  --j-muted: var(--vp-c-text-2, #586174);
  --j-soft: var(--vp-c-bg-soft, #f5f7fa);
  --j-card: var(--vp-c-bg, #fff);
  --j-accent: var(--os-storage, #865500);
  --j-green: #267258;
  --j-red: #aa433c;
  margin: 2rem 0 2.5rem;
  padding: clamp(1rem, 3vw, 1.75rem);
  border: 1px solid var(--j-border);
  border-radius: 1.1rem;
  background: var(--j-soft);
  color: var(--vp-c-text-1, #20242c);
}
.lab-heading, .card-heading, .result-row, .boundary { display: flex; align-items: center; justify-content: space-between; gap: .8rem; }
.lab-heading { align-items: flex-start; }
.eyebrow { margin: 0 0 .35rem; color: var(--j-accent); font-size: .76rem; font-weight: 750; letter-spacing: .08em; }
.lab-heading h2 { margin: 0; font-size: clamp(1.3rem, 3vw, 1.75rem); line-height: 1.25; }
.intro { max-width: 68ch; margin: .55rem 0 0; color: var(--j-muted); font-size: .92rem; }
.counter { flex: 0 0 auto; color: var(--j-accent); font-size: 1.35rem; font-variant-numeric: tabular-nums; font-weight: 750; }
.counter span { color: var(--j-muted); font-size: .95rem; font-weight: 500; }
.scenario-picker, .controls { display: flex; flex-wrap: wrap; gap: .55rem; margin-top: 1rem; }
button { min-height: 2.55rem; padding: .55rem .85rem; border: 1px solid var(--j-border); border-radius: .65rem; background: var(--j-card); color: inherit; font: inherit; font-size: .86rem; cursor: pointer; transition: border-color .16s ease, background-color .16s ease, transform .16s ease; }
button:hover:not(:disabled) { border-color: var(--j-accent); transform: translateY(-1px); }
button:focus-visible { outline: 3px solid color-mix(in srgb, var(--j-accent) 42%, transparent); outline-offset: 2px; }
.scenario-picker button[aria-pressed="true"], .controls button:nth-child(2) { border-color: var(--j-accent); background: var(--j-accent); color: var(--os-accent-ink, #fff); }
button:disabled { cursor: not-allowed; opacity: .48; }
.progress { height: .35rem; margin-top: .95rem; overflow: hidden; border-radius: 1rem; background: var(--j-border); }
.progress span { display: block; height: 100%; border-radius: inherit; background: var(--j-accent); transition: width .22s ease; }
.frame-copy { display: grid; grid-template-columns: minmax(13rem, .8fr) minmax(16rem, 1.4fr); gap: .55rem 1.2rem; padding: 1.1rem 0; }
.frame-title { display: flex; align-items: center; gap: .65rem; }
.frame-title > span { color: var(--j-accent); font-size: .72rem; font-weight: 750; letter-spacing: .06em; }
.frame-title h3 { margin: 0; font-size: 1.03rem; }
.frame-copy > p { margin: 0; color: var(--j-muted); font-size: .9rem; }
.frame-copy .warning-note { grid-column: 1 / -1; padding: .65rem .8rem; border-left: .25rem solid #bc8036; border-radius: .35rem; background: color-mix(in srgb, #bc8036 10%, var(--j-card)); color: var(--vp-c-text-1, #20242c); }
.boundary { justify-content: flex-start; margin: 0 0 .85rem; padding: .7rem .85rem; border: 1px solid var(--j-border); border-radius: .7rem; background: var(--j-card); }
.boundary.is-committed { border-color: color-mix(in srgb, var(--j-green) 55%, var(--j-border)); }
.boundary-icon { color: var(--j-muted); font-size: 1.2rem; }
.boundary.is-committed .boundary-icon { color: var(--j-green); }
.boundary > div { min-width: 0; flex: 1; }
.boundary strong { font-size: .82rem; }
.boundary p { margin: .12rem 0 0; color: var(--j-muted); font-size: .77rem; }
.boundary-label { flex: 0 0 auto; padding: .22rem .45rem; border-radius: .4rem; background: var(--j-soft); color: var(--j-muted); font-size: .68rem; font-weight: 700; }
.boundary.is-committed .boundary-label { background: color-mix(in srgb, var(--j-green) 12%, var(--j-card)); color: var(--j-green); }
.storage-grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: .65rem; }
.storage-card { min-width: 0; padding: .75rem; border: 1px solid var(--j-border); border-top: .22rem solid var(--j-border); border-radius: .7rem; background: var(--j-card); transition: border-color .16s ease, background-color .16s ease; }
.storage-card.active { border-top-color: var(--j-accent); background: color-mix(in srgb, var(--j-accent) 6%, var(--j-card)); }
.storage-card.journal.active { border-top-color: #99702a; }
.storage-card.header.active { border-top-color: #a75d33; }
.storage-card.home.active { border-top-color: var(--j-green); }
.card-heading { justify-content: flex-start; align-items: flex-start; }
.card-mark { display: grid; width: 2.1rem; height: 2.1rem; flex: 0 0 auto; place-items: center; border: 1px solid var(--j-border); border-radius: .5rem; color: var(--j-accent); font-size: .62rem; font-weight: 800; }
.card-heading h4 { margin: 0; font-size: .8rem; line-height: 1.3; }
.card-heading p { margin: .16rem 0 0; color: var(--j-muted); font-size: .68rem; }
.storage-card pre { min-height: 5.4rem; margin: .65rem 0 0; padding: .55rem; overflow: auto; border-radius: .45rem; background: var(--j-soft); color: var(--vp-c-text-1, #20242c); font: .7rem/1.55 ui-monospace, SFMono-Regular, Consolas, monospace; white-space: pre-wrap; overflow-wrap: anywhere; }
.result-row { justify-content: flex-start; flex-wrap: wrap; margin-top: .85rem; padding: .75rem .85rem; border: 1px solid var(--j-border); border-radius: .7rem; background: var(--j-card); }
.result-row > div { display: flex; align-items: center; gap: .5rem; }
.result-row > div + div { padding-left: .8rem; border-left: 1px solid var(--j-border); }
.result-row span { color: var(--j-muted); font-size: .76rem; }
.result-row strong { font-size: .8rem; overflow-wrap: anywhere; }
.result-row .offline, .result-row .error { color: var(--j-red); }
.result-row .online, .result-row .success { color: var(--j-green); }
.lab-footnote { margin: .7rem 0 0; color: var(--j-muted); font-size: .75rem; line-height: 1.55; }
.sr-status { position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px; overflow: hidden; clip: rect(0, 0, 0, 0); white-space: nowrap; border: 0; }
@media (max-width: 850px) { .storage-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
@media (max-width: 680px) {
  .storage-grid { grid-template-columns: 1fr; }
  .storage-card pre { min-height: 0; }
  .frame-copy { grid-template-columns: 1fr; gap: .55rem; }
  .frame-copy .warning-note { grid-column: 1; }
  .boundary { align-items: flex-start; }
  .boundary-label { align-self: center; }
  .result-row { display: grid; grid-template-columns: 1fr; }
  .result-row > div + div { padding: .5rem 0 0; border-top: 1px solid var(--j-border); border-left: 0; }
  .scenario-picker button, .controls button { flex: 1 1 auto; }
}
.dark .journal-lab { --j-green: #57c9a6; --j-red: #ff9995; } /* fixed light-mode greens/reds only contrast on light; lift for dark */
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after { scroll-behavior: auto !important; transition-duration: .01ms !important; animation-duration: .01ms !important; animation-iteration-count: 1 !important; }
}
</style>
