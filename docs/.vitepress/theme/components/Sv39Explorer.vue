<script setup lang="ts">
import { computed, ref } from 'vue'
import DiagramFrame from './DiagramFrame.vue'
import PlaybackControls from './PlaybackControls.vue'
import { usePlayback } from '../composables/usePlayback'

const presets = [
  { label: '程式碼頁 · RXU', value: '0x00401234' },
  { label: '使用者堆疊 · RWU', value: '0x00800ABC' },
  { label: 'Heap 教學頁 · RWU', value: '0x0100007A' },
  { label: '未映射教學情境', value: '0x20000000' },
  { label: '非 canonical 邊界', value: '0x4000000000' },
  { label: 'canonical high-half', value: '0xFFFFFFC000000000' },
  { label: '超出 64-bit', value: '0x10000000000000000' },
]
const addressText = ref(presets[0].value)
const stepCount = computed(() => 5)
const playback = usePlayback(stepCount, 1300)
const currentStep = computed(() => playback.index.value)
const parsed = computed(() => {
  const text = addressText.value.trim().replaceAll('_', '')
  if (!/^0x[0-9a-f]+$/i.test(text)) return { error: '格式錯誤：請輸入 0x 開頭的十六進位位址。' } as const
  try { return { address: BigInt(text) } as const } catch { return { error: '無法解析這個十六進位位址。' } as const }
})
const mapEntries = [
  { page: 0x00400000n, frame: 0x81000000n, region: '使用者程式碼', permissions: 'R-X-U', flags: { V: 1, R: 1, W: 0, X: 1, U: 1, G: 0, A: 1, D: 1 } },
  { page: 0x00800000n, frame: 0x81234000n, region: '使用者堆疊', permissions: 'RW--U', flags: { V: 1, R: 1, W: 1, X: 0, U: 1, G: 0, A: 1, D: 1 } },
  { page: 0x01000000n, frame: 0x81345000n, region: 'Heap 示意頁', permissions: 'RW--U', flags: { V: 1, R: 1, W: 1, X: 0, U: 1, G: 0, A: 1, D: 1 } },
]
const walk = computed(() => {
  if ('error' in parsed.value) return { error: parsed.value.error } as const
  const va = parsed.value.address
  if (va > 0xffffffffffffffffn) return { error: '超出 64-bit 虛擬位址範圍。' } as const
  if (va >= (1n << 38n) && va < 0xffffffc000000000n) return { error: '不是 canonical Sv39 位址：bit 38 以上必須正確 sign-extend。' } as const
  if (va >= 0xffffffc000000000n) return { error: 'canonical Sv39 high-half 位址不在 WUWAOS 的 user address limit（user VA 必須低於 2^38）。' } as const
  const offset = va & 0xfffn
  const vpn0 = (va >> 12n) & 0x1ffn
  const vpn1 = (va >> 21n) & 0x1ffn
  const vpn2 = (va >> 30n) & 0x1ffn
  const page = va & ~0xfffn
  const mapping = mapEntries.find((entry) => entry.page === page)
  if (!mapping) return { va, vpn2, vpn1, vpn0, offset, page, error: '位址在示意 user 範圍內，但不在本頁三筆固定教學 mapping。' } as const
  const pte = ((mapping.frame >> 12n) << 10n) | BigInt((mapping.flags.V << 0) | (mapping.flags.R << 1) | (mapping.flags.W << 2) | (mapping.flags.X << 3) | (mapping.flags.U << 4) | (mapping.flags.G << 5) | (mapping.flags.A << 6) | (mapping.flags.D << 7))
  return { va, vpn2, vpn1, vpn0, offset, page, mapping, pte, physical: mapping.frame + offset } as const
})
const canWalk = computed(() => 'mapping' in walk.value)
const stageNames = ['拆解 VA', 'L2 index', 'L1 index', 'L0 leaf PTE', '組合 PA']
const currentDescription = computed(() => {
  if (currentStep.value === 0) return '把輸入的 VA 拆成 VPN[2]、VPN[1]、VPN[0] 與 12-bit page offset。'
  if (currentStep.value === 1) return '根表以 VPN[2] 選出下一層表；示意節點只表示查表關係。'
  if (currentStep.value === 2) return '第二層以 VPN[1] 選出 L0 表；WUWAOS 的 vm_walk 不接受中途 leaf。'
  if (currentStep.value === 3) return 'L0 index 找到有效 leaf PTE，解碼權限與實體 frame。'
  return '保留原 VA 的 12-bit offset，將它加到 leaf PTE 的 frame base 得到 PA。'
})
function choosePreset(value: string) {
  addressText.value = value
  playback.reset()
}
</script>

<template>
  <section class="sv39-explorer" aria-labelledby="sv39-title">
    <div class="explorer-heading"><div><p class="eyebrow">BigInt 位址解碼 · 固定教學 mapping</p><h3 id="sv39-title">Sv39 三級頁表逐步查找</h3></div>
      <label class="address-input"><span>虛擬位址（hex）</span><input v-model="addressText" inputmode="text" spellcheck="false" aria-label="輸入十六進位虛擬位址" placeholder="0x00401234" @input="playback.reset()"></label>
    </div>
    <div class="presets" aria-label="固定教學位址 preset"><button v-for="preset in presets" :key="preset.value" type="button" :aria-pressed="addressText.toUpperCase() === preset.value.toUpperCase()" @click="choosePreset(preset.value)">{{ preset.label }}</button></div>
    <PlaybackControls :step="playback.index.value" :total="stepCount" :playing="playback.playing.value" @previous="playback.previous" @next="playback.next" @toggle="playback.togglePlay" @reset="playback.reset" />

    <div class="walk-caption" aria-live="polite"><b>第 {{ currentStep + 1 }} 步：{{ stageNames[currentStep] }}</b><span>{{ currentDescription }}</span></div>
    <DiagramFrame title="Sv39 VA 位元欄位與三級頁表路徑" description="輸入位址拆成 VPN[2]、VPN[1]、VPN[0]、offset；按步驟高亮根表至 L0 leaf，再組合 physical address。表節點與 frame 數值是固定教學示意。" kind="walk" :zoomable="true">
      <div class="walk-diagram">
        <div class="bit-address" role="region" tabindex="0" aria-label="可水平捲動的 Sv39 位址欄位：VPN[2]、VPN[1]、VPN[0] 與 offset">
          <div class="bit-labels"><span>VPN[2] · 9 bits</span><span>VPN[1] · 9 bits</span><span>VPN[0] · 9 bits</span><span>page offset · 12 bits</span></div>
          <div class="bit-values"><b :class="{ active: currentStep >= 0 }">{{ 'vpn2' in walk ? `0x${walk.vpn2.toString(16).padStart(3, '0')}` : '—' }}</b><b :class="{ active: currentStep >= 0 }">{{ 'vpn1' in walk ? `0x${walk.vpn1.toString(16).padStart(3, '0')}` : '—' }}</b><b :class="{ active: currentStep >= 0 }">{{ 'vpn0' in walk ? `0x${walk.vpn0.toString(16).padStart(3, '0')}` : '—' }}</b><b :class="{ active: currentStep >= 0 }">{{ 'offset' in walk ? `0x${walk.offset.toString(16).padStart(3, '0')}` : '—' }}</b></div>
        </div>
        <div class="table-path">
          <div class="table-node" :class="{ highlighted: currentStep >= 1 }"><small>satp root</small><b>L2 page table</b><span v-if="'vpn2' in walk">entry {{ walk.vpn2.toString() }}</span><span v-else>讀取 VPN[2]</span></div>
          <i class="path-arrow" :class="{ lit: currentStep >= 2 }" aria-hidden="true">→</i>
          <div class="table-node" :class="{ highlighted: currentStep >= 2 }"><small>non-leaf PTE</small><b>L1 page table</b><span v-if="'vpn1' in walk">entry {{ walk.vpn1.toString() }}</span><span v-else>讀取 VPN[1]</span></div>
          <i class="path-arrow" :class="{ lit: currentStep >= 3 }" aria-hidden="true">→</i>
          <div class="table-node leaf" :class="{ highlighted: currentStep >= 3, invalid: !canWalk }"><small>leaf lookup</small><b>L0 PTE</b><span v-if="'vpn0' in walk">entry {{ walk.vpn0.toString() }}</span><span v-else>讀取 VPN[0]</span></div>
          <i class="path-arrow" :class="{ lit: currentStep >= 4 && canWalk }" aria-hidden="true">→</i>
          <div class="table-node frame-node" :class="{ highlighted: currentStep >= 4 && canWalk, invalid: !canWalk }"><small>physical frame</small><b>{{ 'mapping' in walk ? `0x${walk.mapping.frame.toString(16)}` : '無 leaf' }}</b><span>+ 原 offset</span></div>
        </div>
        <div v-if="'error' in walk" class="walk-error" role="status"><strong>無法完成這筆轉譯</strong><span>{{ walk.error }}</span></div>
        <div v-else class="pte-result" :class="{ revealed: currentStep >= 3 }">
          <div class="pte-head"><strong>{{ walk.mapping.region }}</strong><span>固定教學 mapping · 非 RAM 讀取</span></div>
          <div class="pte-fields"><div><small>leaf PTE</small><code>0x{{ walk.pte.toString(16).padStart(16, '0') }}</code></div><div><small>PPN → frame base</small><code>0x{{ walk.mapping.frame.toString(16) }}</code></div><div><small>flags · V R W X U G A D</small><code>{{ Object.entries(walk.mapping.flags).map(([key, value]) => `${key}=${value}`).join(' ') }}</code></div></div>
          <div class="physical-result" :class="{ revealed: currentStep >= 4 }"><span>PA = frame base + offset</span><strong>{{ currentStep >= 4 ? `0x${walk.physical.toString(16)}` : '逐步查找後顯示' }}</strong><small>offset 0x{{ walk.offset.toString(16).padStart(3, '0') }} 原樣保留</small></div>
        </div>
      </div>
    </DiagramFrame>
    <p class="model-boundary"><strong>模型邊界：</strong>三筆 mapping 與 frame base 是人工指定的固定教學範例，用於展示 PTE／offset 計算，不是 WUWAOS 真實 page table 或 RAM。僅對已列出的低位 user 頁面進行示意；高位/超範圍與未映射情境清楚回錯。WUWAOS 使用 4 KiB leaf，vm_walk 不支援大頁；本頁不模擬任意 kernel mapping 或 TLB。</p>
  </section>
</template>

<style scoped>
.sv39-explorer{margin:1.6rem 0;padding:1.25rem;border:1px solid var(--vp-c-divider);border-radius:var(--os-radius-workspace);background:var(--os-panel);color:var(--vp-c-text-1)}.explorer-heading{display:flex;justify-content:space-between;align-items:flex-end;gap:1rem}.eyebrow{margin:0;color:var(--os-memory,#087c78);font-size:.76rem;font-weight:700;letter-spacing:.04em}.explorer-heading h3{margin:.25rem 0 0;font-size:1.2rem}.address-input{display:grid;gap:.35rem;min-width:min(17rem,100%);font-size:.8rem}.address-input input{width:100%;padding:.5rem .65rem;border:1px solid var(--vp-c-divider);border-radius:8px;background:var(--vp-c-bg);color:inherit;font: .88rem ui-monospace,monospace}.presets{display:flex;flex-wrap:wrap;gap:.4rem;margin:1rem 0}.presets button{padding:.38rem .55rem;border:1px solid var(--vp-c-divider);border-radius:8px;background:var(--vp-c-bg);color:inherit;font-size:.7rem;cursor:pointer}.presets button[aria-pressed=true]{border-color:var(--os-memory,#168b87);box-shadow:none;background:var(--os-active-soft);color:var(--os-ink)}.walk-caption{display:grid;gap:.2rem;min-height:3.3rem;margin:.8rem 0;padding:.65rem .8rem;border:1px solid var(--os-line);border-radius:var(--os-radius-control);background:var(--os-active-soft);color:var(--os-ink);font-size:.82rem}.walk-caption span{color:var(--vp-c-text-2)}.walk-diagram{display:grid;gap:1rem}.bit-address{min-width:480px}.bit-address:focus-visible{outline:3px solid var(--os-focus);outline-offset:2px}.bit-labels,.bit-values{display:grid;grid-template-columns:9fr 9fr 9fr 12fr;gap:3px}.bit-labels span{text-align:center;color:var(--vp-c-text-2);font-size:.66rem}.bit-values b{padding:.55rem .2rem;border:1px solid var(--vp-c-divider);border-radius:7px;background:var(--vp-c-bg-soft);text-align:center;font:700 .8rem ui-monospace,monospace}.bit-values b.active{border-color:var(--os-memory,#168b87);background:color-mix(in srgb,var(--os-memory,#168b87) 14%,var(--vp-c-bg))}.table-path{display:grid;grid-template-columns:minmax(105px,1fr) auto minmax(105px,1fr) auto minmax(105px,1fr) auto minmax(125px,1.15fr);align-items:center;gap:.45rem}.table-node{display:grid;gap:.24rem;min-height:5rem;align-content:center;padding:.55rem;border:1px solid var(--vp-c-divider);border-radius:10px;background:var(--vp-c-bg);transition:border-color var(--os-motion-fast) var(--os-ease),background var(--os-motion-fast) var(--os-ease)}.table-node small,.table-node span{color:var(--vp-c-text-2);font-size:.64rem}.table-node b{font-size:.8rem}.table-node.highlighted{border-color:var(--os-memory,#168b87);background:color-mix(in srgb,var(--os-memory,#168b87) 13%,var(--vp-c-bg));box-shadow:0 0 0 1px var(--os-memory,#168b87)}.table-node.invalid{opacity:1;color:var(--vp-c-text-2);background:var(--vp-c-bg-alt)}.frame-node b{overflow-wrap:anywhere;font-family:ui-monospace,monospace}.path-arrow{color:var(--vp-c-text-3);font-size:1.2rem;font-style:normal}.path-arrow.lit{color:var(--os-memory,#168b87)}.walk-error{display:grid;gap:.3rem;padding:.8rem;border:1px solid var(--os-error,#c44);border-radius:10px;background:color-mix(in srgb,var(--os-error,#c44) 10%,var(--vp-c-bg));font-size:.82rem}.pte-result{display:grid;gap:.75rem;padding:.85rem;border:1px solid var(--vp-c-divider);border-radius:12px;background:var(--vp-c-bg);opacity:.65}.pte-result.revealed{opacity:1}.pte-head{display:flex;justify-content:space-between;gap:.7rem}.pte-head span,.pte-fields small{color:var(--vp-c-text-2);font-size:.68rem}.pte-fields{display:grid;grid-template-columns:1.15fr 1fr 1.5fr;gap:.6rem}.pte-fields>div{display:grid;gap:.25rem}.pte-fields code{overflow-wrap:anywhere;font-size:.72rem}.physical-result{display:flex;flex-wrap:wrap;align-items:center;gap:.6rem;padding:.65rem;border-radius:8px;background:var(--vp-c-bg-soft)}.physical-result span,.physical-result small{font-size:.72rem;color:var(--vp-c-text-2)}.physical-result strong{font:700 .95rem ui-monospace,monospace}.physical-result:not(.revealed) strong{color:var(--vp-c-text-3)}.model-boundary{margin:1rem 0 0;padding:.8rem;border:1px solid var(--os-line);background:var(--os-active-soft);border-radius:var(--os-radius-panel);font-size:.8rem;line-height:1.55}
@media(max-width:720px){.sv39-explorer{padding:.9rem}.explorer-heading{align-items:flex-start;flex-direction:column}.address-input{min-width:0;width:100%}.table-path{grid-template-columns:1fr;gap:.4rem}.path-arrow{justify-self:center;transform:rotate(90deg)}.pte-fields{grid-template-columns:1fr}.pte-head{flex-direction:column}.bit-address{min-width:0;overflow-x:auto}.bit-labels,.bit-values{min-width:470px}}
@media(prefers-reduced-motion:reduce){.table-node{transition:none}}
</style>
