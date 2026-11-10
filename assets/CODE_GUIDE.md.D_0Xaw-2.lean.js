import{_ as a,C as s,o,c as d,a3 as t,E as c}from"./chunks/framework.B7pF4b99.js";const k=JSON.parse('{"title":"WUWAOS 程式碼與架構紀錄","description":"","frontmatter":{},"headers":[],"relativePath":"CODE_GUIDE.md","filePath":"CODE_GUIDE.md","lastUpdated":1794274556000}'),i={name:"CODE_GUIDE.md"};function l(n,e,p,u,h,m){const r=s("MermaidDiagram");return o(),d("div",null,[e[0]||(e[0]=t("",8)),c(r,{source:`flowchart LR
  subgraph U[U-mode 使用者程式]
    SH[Shell / fan / workstate]
    SC[syscall API]
    SH --> SC
  end
  subgraph K[S-mode kernel]
    SY[syscall dispatch]
    IO[core I/O / console / FD]
    OFD[OFD_TTY handle and references]
    TTY[per-endpoint queues / mode / consumer]
    EV[process loop registered poll/active hooks]
    SY --> IO --> OFD --> TTY
  end
  subgraph D[S-mode drivers]
    IN[VirtIO input driver]
    GPU[VirtIO GPU driver]
  end
  subgraph UI[S-mode UI]
    MAP[key mapping and input adapter]
    VT[ANSI VT model]
    R[terminal cell renderer]
    WIN[bounded desktop windows and composition]
  end
  SC --> SY
  IN --> MAP --> TTY
  TTY -->|registered output consumer| VT --> R --> WIN --> GPU
  WIN -->|input focus| MAP
  EV -->|poll / active| WIN
`}),e[1]||(e[1]=t("",105))])}const _=a(i,[["render",l]]);export{k as __pageData,_ as default};
