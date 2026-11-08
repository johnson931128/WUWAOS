import{_ as r,C as s,o as n,c as o,a3 as t,E as i}from"./chunks/framework.DERdGiAC.js";const k=JSON.parse('{"title":"11 · workstate 即時監測","description":"","frontmatter":{},"headers":[],"relativePath":"topics/monitoring.md","filePath":"topics/monitoring.md","lastUpdated":1794124004000}'),l={name:"topics/monitoring.md"};function d(p,e,c,h,m,u){const a=s("MermaidDiagram");return n(),o("div",null,[e[0]||(e[0]=t("",7)),i(a,{source:`flowchart TD
    A[派發行程：記錄硬體時間] --> B[執行使用者程式與核心服務]
    B --> C[切走前：累計本次執行時間]
    C --> D[取得兩次唯讀快照]
    D --> E[執行時間差除以經過時間差]
    E --> F[每秒顯示 CPU 與 RAM 資料]
`}),e[1]||(e[1]=t("",9))])}const _=r(l,[["render",d]]);export{k as __pageData,_ as default};
