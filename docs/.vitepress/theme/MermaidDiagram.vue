<script setup lang="ts">
import { onMounted, onBeforeUnmount, ref, watch, useId } from 'vue'
import { useData } from 'vitepress'

const props = defineProps<{ source: string }>()
const { isDark } = useData()
const markup = ref('')
const failed = ref(false)
const diagramId = `wuwaos-diagram-${useId().replace(/[^a-zA-Z0-9-]/g, '-')}`
let revision = 0
let mounted = false

async function render() {
  const current = ++revision
  failed.value = false
  try {
    const { default: mermaid } = await import('mermaid')
    if (!mounted || current !== revision) return
    mermaid.initialize({
      startOnLoad: false,
      securityLevel: 'strict',
      theme: isDark.value ? 'dark' : 'default',
      fontFamily: 'Segoe UI, Microsoft JhengHei, sans-serif',
      htmlLabels: false,
      flowchart: { htmlLabels: false },
    })
    const { svg } = await mermaid.render(`${diagramId}-${current}`, props.source)
    if (mounted && current === revision) markup.value = svg
  } catch {
    if (mounted && current === revision) {
      markup.value = ''
      failed.value = true
    }
  }
}

onMounted(() => { mounted = true; void render() })
onBeforeUnmount(() => { mounted = false; ++revision })
watch([() => props.source, isDark], () => { if (mounted) void render() })
</script>

<template>
  <figure class="mermaid-diagram" aria-label="架構流程圖">
    <div v-if="markup" class="mermaid-svg" v-html="markup" />
    <p v-else role="status">{{ failed ? '圖表暫時無法顯示，請重新整理。' : '正在載入流程圖…' }}</p>
  </figure>
</template>

<style scoped>
.mermaid-diagram { margin: 24px 0; padding: 16px; overflow-x: auto; border: 1px solid var(--vp-c-divider); border-radius: 8px; }
.mermaid-svg :deep(svg) { display: block; height: auto; margin: auto; }
.mermaid-diagram p { color: var(--vp-c-text-2); font-size: 14px; }
</style>
