import { computed, onBeforeUnmount, ref, watch, type ComputedRef, type Ref } from 'vue'

export interface Playback {
  index: Ref<number>
  playing: Ref<boolean>
  previous: () => void
  next: () => void
  reset: () => void
  togglePlay: () => void
  stop: () => void
}

/** Owns a teaching animation's step and timer; it does not represent a live OS trace. */
export function usePlayback(count: ComputedRef<number>, delay = 1400): Playback {
  const index = ref(0)
  const playing = ref(false)
  const total = computed(() => Math.max(0, Math.floor(count.value)))
  const intervalDelay = Number.isFinite(delay) ? Math.max(1, delay) : 1400
  let timer: ReturnType<typeof setInterval> | undefined

  function stop() {
    playing.value = false
    if (timer !== undefined) clearInterval(timer)
    timer = undefined
  }

  function reset() {
    stop()
    index.value = 0
  }

  function previous() {
    stop()
    if (index.value > 0) index.value -= 1
  }

  function next() {
    stop()
    if (index.value < total.value - 1) index.value += 1
  }

  function togglePlay() {
    if (playing.value) {
      stop()
      return
    }
    if (total.value < 2) return
    if (index.value >= total.value - 1) index.value = 0

    playing.value = true
    timer = setInterval(() => {
      if (index.value >= total.value - 1) {
        stop()
        return
      }
      index.value += 1
      if (index.value >= total.value - 1) stop()
    }, intervalDelay)
  }

  watch(total, reset)
  onBeforeUnmount(stop)

  return { index, playing, previous, next, reset, togglePlay, stop }
}
