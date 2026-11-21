<template>
  <figure class="cow-flow" aria-labelledby="cow-flow-caption">
    <ol class="cow-steps" role="list">
      <li>
        <strong>fork 建立子頁表</strong>
        <div class="cow-pair">
          <span>父行程 PTE</span>
          <span>子行程 PTE</span>
          <span class="cow-arrow" aria-hidden="true">↘</span>
          <span class="cow-arrow" aria-hidden="true">↙</span>
        </div>
        <div class="cow-frame">兩個 PTE 指向同一個 4 KiB 頁框</div>
      </li>
      <li>
        <span class="cow-arrow" aria-hidden="true">↓</span>
        <strong class="cow-fault">一方寫入，觸發 Store Page Fault</strong>
      </li>
      <li>
        <span class="cow-arrow" aria-hidden="true">↓</span>
        <strong>核心檢查 COW 標記、原權限和引用數</strong>
        <ul class="cow-pair cow-branches" role="list">
          <li><strong>引用 &gt; 1</strong>複製 4 KiB，寫入方改用私有頁框。</li>
          <li><strong>引用 = 1</strong>保留原頁框，直接恢復寫入權限。</li>
        </ul>
      </li>
    </ol>
    <figcaption id="cow-flow-caption">COW 寫入路徑示意。更新 PTE 並同步 TLB 後，CPU 重試原指令。</figcaption>
  </figure>
</template>

<style scoped>
.cow-flow {
  max-width: 44rem;
  margin: 1.25rem 0;
  padding: clamp(.75rem, 3vw, 1.5rem);
  border: 1px solid var(--os-line);
  border-radius: var(--os-radius-panel);
  background: var(--os-panel);
  color: var(--os-ink);
  font-size: 1rem;
  line-height: 1.65;
}
.cow-steps, .cow-branches {
  margin: 0;
  padding: 0;
  list-style: none;
}
.cow-steps { text-align: center; }
.cow-steps li + li { margin-top: .5rem; }
.cow-pair {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: .5rem;
  max-width: 32rem;
  margin: .5rem auto 0;
}
.cow-pair > span:first-child, .cow-pair > span:nth-child(2), .cow-frame, .cow-branches > li {
  padding: .5rem .75rem;
  border-radius: var(--os-radius-control);
}
.cow-pair > span:first-child, .cow-pair > span:nth-child(2), .cow-branches > li { background: var(--os-soft); }
.cow-frame { max-width: 32rem; margin: 0 auto; background: var(--os-active-soft); color: var(--os-memory); }
.cow-arrow { display: block; color: var(--os-muted); }
.cow-fault { color: var(--os-fault); }
.cow-branches > li + li { margin-top: 0; }
.cow-branches strong { display: block; }
figcaption { margin-top: 1rem; color: var(--os-muted); font-size: .9375rem; }

@media (max-width: 500px) {
  .cow-branches { grid-template-columns: minmax(0, 1fr); }
}
</style>
