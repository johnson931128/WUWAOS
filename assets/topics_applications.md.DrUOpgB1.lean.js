import{_ as a,C as o,o as c,c as l,a3 as t,E as r}from"./chunks/framework.BZCmpA0A.js";const f=JSON.parse('{"title":"10 · Shell 與 fan 編輯器","description":"","frontmatter":{},"headers":[],"relativePath":"topics/applications.md","filePath":"topics/applications.md","lastUpdated":1793166439000}'),s={name:"topics/applications.md"};function n(h,e,i,p,b,m){const d=o("MermaidDiagram");return c(),l("div",null,[e[0]||(e[0]=t("",10)),r(d,{source:`flowchart TD
  A[鍵盤輸入 cat note.txt 後按 Enter] --> B[QEMU UART 序列埠收到字元]
  B --> C[核心 I/O 收集一行並交給 shell 的 read]
  C --> D[shell tokenize 切出 cat 與 note.txt]
  D --> E[command 依名稱和參數數量選擇 cat]
  E --> F[使用者 API 執行 ecall 系統呼叫指令]
  F --> G[trap 例外入口保存狀態並進入核心]
  G --> H[syscall_dispatch 依系統呼叫編號分派]
  H --> I[核心結合 PCB 的目前目錄查詢 WUFS]
  I --> J[讀取內容回傳 shell，經 write 輸出]
`}),e[1]||(e[1]=t("",22))])}const _=a(s,[["render",n]]);export{f as __pageData,_ as default};
