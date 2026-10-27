import{_ as l,C as i,o,c as d,j as a,a as t,E as n,a3 as s}from"./chunks/framework.BZCmpA0A.js";const _=JSON.parse('{"title":"05 · 計時器與 RR 排程","description":"","frontmatter":{},"headers":[],"relativePath":"topics/scheduling.md","filePath":"topics/scheduling.md","lastUpdated":1793082474000}'),p={name:"topics/scheduling.md"};function c(h,e,u,m,b,f){const r=i("MermaidDiagram");return o(),d("div",null,[e[0]||(e[0]=a("h1",{id:"_05-·-計時器與-rr-排程",tabindex:"-1"},[t("05 · 計時器與 RR 排程 "),a("a",{class:"header-anchor",href:"#_05-·-計時器與-rr-排程","aria-label":'Permalink to "05 · 計時器與 RR 排程"'},"​")],-1)),e[1]||(e[1]=a("p",null,[a("strong",null,"目前是 50 ms 時間片的 RR（Round Robin，輪詢排程）"),t("。就緒行程仍以 FIFO（先進先出）順序排隊，但執行中的程式即使不主動 yield，也會在時間片到期時被核心換下。")],-1)),e[2]||(e[2]=a("h2",{id:"硬體如何讓程式暫停",tabindex:"-1"},[t("硬體如何讓程式暫停 "),a("a",{class:"header-anchor",href:"#硬體如何讓程式暫停","aria-label":'Permalink to "硬體如何讓程式暫停"'},"​")],-1)),e[3]||(e[3]=a("p",null,"核心透過 OpenSBI TIME（韌體計時服務）設定「目前硬體時間 + 50 ms」。到期後，CPU 收到 Supervisor timer interrupt（監督模式計時器中斷），跳到 stvec（監督模式陷阱入口暫存器）指定的入口。",-1)),n(r,{source:`flowchart TD
  A["從就緒隊首取出行程"] --> B["設定時間片：現在 + 50 ms"]
  B --> C["sret：恢復暫存器，執行使用者程式"]
  C --> D["時間到：CPU 進入 trap_entry"]
  D --> E["保存 TrapFrame，辨識計時器中斷"]
  E --> F["SBI 清除 pending，Running 改為 Ready"]
  F --> G["原行程排入隊尾"]
  G --> A
`}),e[4]||(e[4]=s("",5)),n(r,{source:`flowchart TD
  A["A 執行；佇列 B → C"] -->|"50 ms 到期"| B["B 執行；佇列 C → A"]
  B -->|"50 ms 到期"| C["C 執行；佇列 A → B"]
  C -->|"50 ms 到期"| A
`}),e[5]||(e[5]=s("",14))])}const k=l(p,[["render",c]]);export{_ as __pageData,k as default};
