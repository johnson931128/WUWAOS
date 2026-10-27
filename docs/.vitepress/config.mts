import { defineConfig } from 'vitepress'

const publicUrl = 'https://johnson931128.github.io/WUWAOS/'

const chapters = [
  ['開始使用', 'getting-started'], ['開機流程', 'boot'],
  ['例外與系統呼叫', 'traps'], ['行程管理', 'processes'],
  ['計時器與 RR 排程', 'scheduling'], ['實體頁與 Sv39', 'memory'],
  ['按需分頁、置換與 Swap', 'paging'], ['輸入輸出與 Pipe', 'io'],
  ['WUFS 檔案系統', 'filesystem'], ['Shell 與 fan 編輯器', 'applications'],
  ['維護文件網站', 'writing'],
]

export default defineConfig({
  lang: 'zh-Hant',
  title: 'WUWAOS',
  description: '從開機到使用者程式，理解一個 RISC-V 小型作業系統的實作。',
  base: process.env.DOCS_BASE || '/',
  sitemap: { hostname: publicUrl },
  head: [['meta', { name: 'robots', content: 'index, follow' }]],
  transformHead({ pageData }) {
    if (pageData.relativePath === '404.md') return []
    const path = pageData.relativePath.replace(/(^|\/)index\.md$/, '$1').replace(/\.md$/, '.html')
    const canonical = new URL(path, publicUrl).href
    return [
      ['link', { rel: 'canonical', href: canonical }],
      ['meta', { property: 'og:url', content: canonical }],
      ['meta', { property: 'og:title', content: `${pageData.title || '實作紀錄'} | WUWAOS` }],
      ['meta', { property: 'og:description', content: pageData.description || '以 C 與 RISC-V 實作的小型作業系統：開機、行程、分頁、Swap、WUFS 與 fan 編輯器。' }],
    ]
  },
  lastUpdated: true,
  markdown: {
    lineNumbers: true, html: false, languageAlias: { h: 'c', S: 'asm' },
    config(md) {
      const fence = md.renderer.rules.fence!
      md.renderer.rules.fence = (tokens, index, options, env, self) => {
        if (tokens[index].info.trim() === 'mermaid') {
          return `<MermaidDiagram source="${md.utils.escapeHtml(tokens[index].content)}" />\n`
        }
        return fence(tokens, index, options, env, self)
      }
    },
  },
  themeConfig: {
    siteTitle: 'WUWAOS 實作紀錄',
    nav: [{ text: '閱讀章節', link: '/topics/getting-started' }, { text: '規格紀錄', link: '/history' }],
    sidebar: [
      { text: 'WUWAOS', items: [{ text: '00 · 專案總覽', link: '/' }] },
      { text: '從硬體到應用程式', items: chapters.map(([text, slug], i) => ({ text: `${String(i + 1).padStart(2, '0')} · ${text}`, link: `/topics/${slug}` })) },
      { text: '規格與參考', items: [{ text: '原始規格與驗收紀錄', link: '/history' }, { text: '完整程式碼導覽', link: '/CODE_GUIDE' }] },
    ],
    outline: { level: [2, 3], label: '本頁內容' },
    search: { provider: 'local', options: { locales: { root: { translations: { button: { buttonText: '搜尋文件', buttonAriaLabel: '搜尋文件' }, modal: { noResultsText: '找不到相關內容', resetButtonTitle: '清除搜尋', footer: { selectText: '選取', navigateText: '切換', closeText: '關閉' } } } } } } },
    socialLinks: [{ icon: 'github', link: 'https://github.com/johnson931128/WUWAOS' }],
    docFooter: { prev: '上一章', next: '下一章' },
    lastUpdated: { text: '最後提交更新', formatOptions: { dateStyle: 'medium' } },
    returnToTopLabel: '回到頂端', sidebarMenuLabel: '章節目錄',
    darkModeSwitchLabel: '外觀', lightModeSwitchTitle: '切換淺色模式', darkModeSwitchTitle: '切換深色模式',
  },
})
