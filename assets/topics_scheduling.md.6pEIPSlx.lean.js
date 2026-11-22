import{S as i}from"./chunks/SchedulingLab.CS8ngyIV.js";import{C as s,o as d,c as l,j as a,a as r,E as t,a6 as n}from"./chunks/framework.CIQdaWiD.js";import"./chunks/usePlayback.DoA8ctHb.js";const g=JSON.parse('{"title":"計時器與 RR 排程","description":"","frontmatter":{},"headers":[],"relativePath":"topics/scheduling.md","filePath":"topics/scheduling.md","lastUpdated":1795330291000}'),c={name:"topics/scheduling.md"},f=Object.assign(c,{setup(h){return(p,e)=>{const o=s("MermaidDiagram");return d(),l("div",null,[e[0]||(e[0]=a("h1",{id:"_05-·-計時器與-rr-排程",tabindex:"-1"},[r("計時器與 RR 排程 "),a("a",{class:"header-anchor",href:"#_05-·-計時器與-rr-排程","aria-label":'Permalink to "計時器與 RR 排程 {#_05-·-計時器與-rr-排程}"'},"​")],-1)),e[1]||(e[1]=a("p",null,[r("對應現行規格："),a("a",{href:"./../spec/04-scheduling.html"},"〈04 · 誰先跑：排程、狀態與多 Hart〉"),r("。")],-1)),t(i),e[2]||(e[2]=n("",8)),t(o,{source:`flowchart TD
  A[從就緒隊首取出 Thread] --> B[設定時間片：現在 + 50 ms]
  B --> C[sret：恢復暫存器，執行使用者程式]
  C --> D[時間到：CPU 進入 trap_entry]
  D --> E[保存 TrapFrame，辨識計時器中斷]
  E --> F[SBI 清除 pending，Running 改為 Ready]
  F --> G[原 Thread 排入隊尾]
  G --> A
`}),e[3]||(e[3]=n("",3)),t(o,{source:`flowchart TD
  A[A 執行；佇列 B → C] -->|50 ms 到期| B[B 執行；佇列 C → A]
  B -->|50 ms 到期| C[C 執行；佇列 A → B]
  C -->|50 ms 到期| A
`}),e[4]||(e[4]=n("",27))])}}});export{g as __pageData,f as default};
