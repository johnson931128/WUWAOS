import{_ as r,C as s,o as n,c as o,a3 as e,E as i}from"./chunks/framework.BZCmpA0A.js";const _=JSON.parse('{"title":"11 · workstate 即時監測","description":"","frontmatter":{},"headers":[],"relativePath":"topics/monitoring.md","filePath":"topics/monitoring.md","lastUpdated":1793159406000}'),l={name:"topics/monitoring.md"};function d(p,t,c,h,m,u){const a=s("MermaidDiagram");return n(),o("div",null,[t[0]||(t[0]=e("",6)),i(a,{source:`flowchart TD
    A[派發行程：記錄硬體時間] --> B[執行使用者程式與核心服務]
    B --> C[切走前：累計本次執行時間]
    C --> D[取得兩次唯讀快照]
    D --> E[執行時間差除以經過時間差]
    E --> F[每秒顯示 CPU 與 RAM 資料]
`}),t[1]||(t[1]=e("",9))])}const k=r(l,[["render",d]]);export{_ as __pageData,k as default};
