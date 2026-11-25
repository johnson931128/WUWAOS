<script setup lang="ts">
import { nextTick, onBeforeUnmount, ref, useId } from 'vue'
import { withBase } from 'vitepress'

defineProps<{ term: string, definition: string }>()

const id = `spec-term-${useId()}`
const host = ref<HTMLElement>()
const link = ref<HTMLAnchorElement>()
const note = ref<HTMLElement>()
const mode = ref<'hover' | 'focus' | 'tap' | null>(null)
const position = ref({ left: 0, top: 0 })
let touch = false

function place() {
  if (!mode.value || !link.value || !note.value) return
  const anchor = link.value.getBoundingClientRect()
  const { width, height } = note.value.getBoundingClientRect()
  const viewport = document.documentElement
  const edge = 12
  const left = Math.max(edge, Math.min(anchor.left, viewport.clientWidth - width - edge))
  const below = anchor.bottom + 5
  const top = below + height <= viewport.clientHeight - edge
    ? below : Math.max(edge, anchor.top - height - 5)
  position.value = { left, top }
}

function dismissOnEscape(event: KeyboardEvent) {
  if (event.key === 'Escape') close()
}

function dismissOutside(event: PointerEvent) {
  if (!host.value?.contains(event.target as Node)) close()
}

function show(source: 'hover' | 'focus' | 'tap') {
  if (!mode.value) {
    document.addEventListener('keydown', dismissOnEscape)
    document.addEventListener('pointerdown', dismissOutside)
    window.addEventListener('resize', place)
    window.addEventListener('scroll', place, { capture: true, passive: true })
  }
  mode.value = source
  void nextTick(place)
}

function close() {
  if (!mode.value) return
  mode.value = null
  document.removeEventListener('keydown', dismissOnEscape)
  document.removeEventListener('pointerdown', dismissOutside)
  window.removeEventListener('resize', place)
  window.removeEventListener('scroll', place, true)
}

function hover(event: PointerEvent) {
  if (event.pointerType === 'mouse' && !mode.value) show('hover')
}

function leave() {
  if (mode.value === 'hover') close()
}

function focus() {
  if (link.value?.matches(':focus-visible')) show('focus')
}

function click(event: MouseEvent) {
  if (!touch || event.detail === 0) return
  touch = false
  if (mode.value === 'tap') return
  event.preventDefault()
  show('tap')
}

onBeforeUnmount(close)
</script>

<template>
  <span ref="host" class="spec-term" @pointerenter="hover" @pointerleave="leave">
    <!-- Native navigation lets the first touch reveal the note before following the link. -->
    <a ref="link" class="spec-term-link" :href="withBase(`/spec/glossary.html#${term}`)"
       target="_self" :aria-describedby="id" @focus="focus" @blur="close"
       @pointerdown="touch = $event.pointerType === 'touch'" @click="click"><slot /></a>
    <span v-show="mode" :id="id" ref="note" role="tooltip" class="spec-term-note"
          :style="{ left: `${position.left}px`, top: `${position.top}px` }">{{ definition }}</span>
  </span>
</template>

<style scoped>
.vp-doc .spec-term-link {
  color: inherit;
  text-decoration: underline;
  text-decoration-style: dotted;
  text-decoration-color: var(--os-line-strong);
  text-underline-offset: 0.24em;
  text-decoration-thickness: 1.5px;
  transition: color var(--os-motion-fast) var(--os-ease), text-decoration-color var(--os-motion-fast) var(--os-ease);
}
.vp-doc .spec-term-link:hover {
  color: var(--os-accent);
  text-decoration-color: var(--os-accent);
}
.spec-term-link:focus-visible {
  outline: 2px solid var(--os-focus);
  outline-offset: 3px;
  border-radius: 3px;
}
.spec-term-note {
  position: fixed;
  z-index: 45;
  inset: auto;
  width: max-content;
  max-width: min(24rem, calc(100vw - 24px));
  padding: 0.75rem 1rem;
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-panel);
  background: var(--os-panel);
  color: var(--os-ink);
  box-shadow: 0 8px 28px rgb(15 23 42 / 12%);
  font-size: 0.9375rem;
  font-weight: 400;
  line-height: 1.6;
  white-space: normal;
  overflow-wrap: anywhere;
}
.spec-term-note::before {
  position: absolute;
  inset: -6px 0;
  content: '';
}
</style>
