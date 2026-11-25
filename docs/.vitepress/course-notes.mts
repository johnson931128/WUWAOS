import katex from 'katex'

const noteTone: [RegExp, string][] = [
  [/\p{Extended_Pictographic}\uFE0F?\s*/gu, ''],
  [/考試絕對重點|考試重點|考點整理/g, '重點'],
  [/萬無一失的公式/g, '公式的適用條件'],
  [/三大鐵律/g, '三項要求'],
  [/致命的侷限性/g, '使用上的限制'],
  [/致命代價/g, '額外成本'],
  [/災難的開端/g, '問題從何而來'],
  [/災難推演/g, '執行順序推演'],
  [/災難/g, '問題'],
  [/\*\*==([^=\n]+)==\*\*|==([^=\n]+)==/g, '**$1$2**'],
]

function normalizeProse(line: string) {
  return line.split(/(`[^`]*`)/).map((part, index) => {
    if (index % 2) return part
    for (const [pattern, replacement] of noteTone) part = part.replace(pattern, replacement)
    return part.replace(/<(?!\/?(?:br|sub|sup)\s*\/?>|https?:\/\/)(?=[A-Za-z/!])/g, '&lt;')
  }).join('')
}

export function normalizeCourseNotes(source: string) {
  return source.replace(/<!-- course-note:start -->[\r\n]+([\s\S]*?)<!-- course-note:end -->/g, (_, body: string) => {
    let fence: { character: string; length: number } | undefined
    let section = '課堂筆記'
    return body.split(/\r?\n/).map(line => {
      const delimiter = line.match(/^(\s*)(`{3,}|~{3,})(.*)$/)
      if (fence) {
        if (delimiter?.[2][0] === fence.character && delimiter[2].length >= fence.length && !delimiter[3].trim()) {
          fence = undefined
        }
        return line
      }
      if (delimiter) {
        const [, indent, marker, info] = delimiter
        const language = info.trim().replace(/^=/, '').split(/\s/)[0]
        const aliases: Record<string, string> = { 'c++': 'cpp', Bash: 'bash', txt: 'text' }
        fence = { character: marker[0], length: marker.length }
        return indent + marker + (aliases[language] ?? language)
      }

      const heading = line.match(/^(#{1,6})[ \t]+(.+)$/)
      if (heading) {
        const title = normalizeProse(heading[2].trim())
        section = title.replace(/[\[\]*`]/g, '').replace(/^\d+[.、]\s*/, '')
        return '#'.repeat(Math.min(heading[1].length + 1, 6)) + ' ' + title
      }
      line = normalizeProse(line)
      return line.replace(/!\[(?:image|圖片)?\](?=\(https:\/\/hackmd\.io\/)/gi,
        '![' + section.slice(0, 85).replace(/[\\[\]]/g, '') + '插圖]')
    }).join('\n')
  })
}

export function renderCourseFormula(formula: string, display: boolean, escape: (value: string) => string) {
  try {
    return katex.renderToString(formula, {
      displayMode: display,
      output: 'mathml',
      strict: 'ignore',
    })
  } catch {
    return '<code>' + escape(formula) + '</code>'
  }
}
