import DefaultTheme from 'vitepress/theme'
import MermaidDiagram from './MermaidDiagram.vue'
import SpecTerm from './components/SpecTerm.vue'
import './style.css'
export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('MermaidDiagram', MermaidDiagram)
    app.component('SpecTerm', SpecTerm)
  },
}
