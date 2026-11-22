import { defineConfig } from 'vitepress'
import { readFileSync } from 'node:fs'

const publicUrl = 'https://johnson931128.github.io/WUWAOS/'
const readSpec = (file: string) => readFileSync(new URL(`../spec/${file}`, import.meta.url), 'utf8')

// The Markdown contents page is also the source of the primary navigation.
const chapters = Array.from(readSpec('index.md').matchAll(/^\| \[(\d{2} · [^\]]+)\]\(\.\/([\w-]+)\.md\) \|/gm),
  ([, text, slug]) => ({ text, slug, link: `/spec/${slug}` }))
if (!chapters.length || chapters.some(({ slug, text }, index) =>
  !text.startsWith(`${String(index).padStart(2, '0')} · `) ||
  readSpec(`${slug}.md`).match(/^# (.+)$/m)?.[1].trim() !== text)) {
  throw new Error('章節目錄的順序與標題必須和現行規格一致。')
}

const glossary = new Map(Array.from(readSpec('glossary.md').matchAll(/^## .+ \{#([\w-]+)\}\r?\n\r?\n([^\r\n]+)/gm),
  ([, id, text]) => [id, text.replace(/\[([^\]]+)\]\([^)]+\)/g, '$1').replace(/`([^`]+)`/g, '$1')]))

export default defineConfig({
  lang: 'zh-Hant',
  title: 'WUWAOS',
  description: '視覺化探索 WUWAOS：以 C 與 RISC-V RV64 實作、可在 QEMU 啟動的小型作業系統。',
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
    // Repository-authored Vue labs are embedded in the Markdown chapters.
    lineNumbers: true, html: true, languageAlias: { h: 'c', S: 'asm' },
    config(md) {
      md.core.ruler.after('inline', 'spec-term-hints', (state) => {
        for (const token of state.tokens) {
          if (!token.children) continue
          let inTerm = false
          for (const child of token.children) {
            if (child.type === 'link_open') {
              // Match terms in spec Markdown without duplicating glossary prose in Vue.
              const id = child.attrGet('href')?.match(/^\.\/glossary\.md#([\w-]+)$/)?.[1]
              const definition = id && glossary.get(id)
              if (!definition) continue
              child.type = 'html_inline'
              child.content = `<SpecTerm term="${id}" definition="${md.utils.escapeHtml(definition)}">`
              inTerm = true
            } else if (child.type === 'link_close' && inTerm) {
              child.type = 'html_inline'
              child.content = '</SpecTerm>'
              inTerm = false
            }
          }
        }
      })
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
    nav: [{ text: '現行規格', link: '/spec/', activeMatch: '^/spec/' }, { text: '互動教學', link: '/topics/labs' }, { text: '歷史紀錄', link: '/history' }],
    sidebar: [
      { text: '現行規格', link: '/spec/', items: chapters.map(({ text, link }) => ({ text, link })) },
      { text: '教材與參考', items: [{ text: '互動教學', link: '/topics/labs' }, { text: '名詞表', link: '/spec/glossary' }, { text: '原始規格與驗收紀錄', link: '/history' }, { text: '程式碼導覽', link: '/CODE_GUIDE' }] },
      { text: '網站維護', collapsed: true, items: [{ text: '維護文件網站', link: '/topics/writing' }, { text: '視覺教材設計', link: '/visual-system' }, { text: '研究與來源對照', link: '/redesign' }] },
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
