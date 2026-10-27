import{_ as n,C as p,o as r,c as t,a3 as a,E as l}from"./chunks/framework.BZCmpA0A.js";const _=JSON.parse('{"title":"04 · 行程管理","description":"","frontmatter":{},"headers":[],"relativePath":"topics/processes.md","filePath":"topics/processes.md","lastUpdated":1793085054000}'),o={name:"topics/processes.md"};function i(c,e,d,b,h,m){const s=p("MermaidDiagram");return r(),t("div",null,[e[0]||(e[0]=a("",9)),l(s,{source:`flowchart TD
    A[行程 A：核心中的 sleep 或阻塞 I/O] --> B[設 Waiting，保存核心上下文]
    B --> C[切到排程器的獨立堆疊]
    C --> D[派發其他就緒行程]
    D --> E[A 的事件完成，改回 Ready]
    E --> F[再次選中 A，恢復其核心堆疊]
    F --> G[從原函式繼續，正常回傳 syscall 結果]
`}),e[1]||(e[1]=a("",18))])}const k=n(o,[["render",i]]);export{_ as __pageData,k as default};
