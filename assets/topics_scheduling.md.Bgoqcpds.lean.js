import{_ as n,C as s,o as i,c as o,a3 as a,E as r}from"./chunks/framework.BZCmpA0A.js";const _=JSON.parse('{"title":"05 · 計時器與 RR 排程","description":"","frontmatter":{},"headers":[],"relativePath":"topics/scheduling.md","filePath":"topics/scheduling.md","lastUpdated":1793085054000}'),l={name:"topics/scheduling.md"};function p(d,e,c,h,u,m){const t=s("MermaidDiagram");return i(),o("div",null,[e[0]||(e[0]=a("",5)),r(t,{source:`flowchart TD
  A["從就緒隊首取出行程"] --> B["設定時間片：現在 + 50 ms"]
  B --> C["sret：恢復暫存器，執行使用者程式"]
  C --> D["時間到：CPU 進入 trap_entry"]
  D --> E["保存 TrapFrame，辨識計時器中斷"]
  E --> F["SBI 清除 pending，Running 改為 Ready"]
  F --> G["原行程排入隊尾"]
  G --> A
`}),e[1]||(e[1]=a("",5)),r(t,{source:`flowchart TD
  A["A 執行；佇列 B → C"] -->|"50 ms 到期"| B["B 執行；佇列 C → A"]
  B -->|"50 ms 到期"| C["C 執行；佇列 A → B"]
  C -->|"50 ms 到期"| A
`}),e[2]||(e[2]=a("",14))])}const f=n(l,[["render",p]]);export{_ as __pageData,f as default};
