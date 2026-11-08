import{_ as a,C as s,o as d,c as o,a3 as t,E as c}from"./chunks/framework.BZCmpA0A.js";const _=JSON.parse('{"title":"WUWAOS 程式碼與架構紀錄","description":"","frontmatter":{},"headers":[],"relativePath":"CODE_GUIDE.md","filePath":"CODE_GUIDE.md","lastUpdated":1794124009000}'),i={name:"CODE_GUIDE.md"};function l(n,e,p,u,h,m){const r=s("MermaidDiagram");return d(),o("div",null,[e[0]||(e[0]=t("",7)),c(r,{source:`flowchart LR
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
`}),e[1]||(e[1]=t("",103))])}const k=a(i,[["render",l]]);export{_ as __pageData,k as default};
