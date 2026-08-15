import { defineConfig } from 'vitepress'
import markdownItKatex from 'markdown-it-katex'

export default defineConfig({
  title: 'HPCSim',
  description: 'A native CPU HPC simulation and visualization engine for large-scale gravitational N-body systems',
  lang: 'en-US',
  cleanUrls: true,
  lastUpdated: true,

  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          mermaid: ['mermaid'],
        },
      },
    },
  },

  head: [
    ['link', { rel: 'preconnect', href: 'https://fonts.googleapis.com' }],
    ['link', { rel: 'preconnect', href: 'https://fonts.gstatic.com', crossorigin: '' }],
    ['link', { rel: 'stylesheet', href: 'https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600&family=Inter:wght@400;500;600;700&display=swap' }],
    ['link', { rel: 'stylesheet', href: 'https://cdn.jsdelivr.net/npm/katex@0.16.21/dist/katex.min.css' }],
  ],

  markdown: {
    config(md) {
      md.use(markdownItKatex)
      const fence = md.renderer.rules.fence.bind(md.renderer.rules.fence)
      md.renderer.rules.fence = (tokens, idx, options, env, self) => {
        const token = tokens[idx]
        if (token.info.trim() === 'mermaid') {
          const code = token.content.trim()
          return `<MermaidDiagram code="${escapeAttribute(code)}"></MermaidDiagram>`
        }
        return fence(tokens, idx, options, env, self)
      }
    },
  },

  themeConfig: {
    logo: null,
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Getting Started', link: '/getting-started' },
      { text: 'Architecture', link: '/architecture' },
      { text: 'Physics', link: '/physics' },
      { text: 'Performance', link: '/performance' },
      { text: 'Instrumentation', link: '/instrumentation' },
      { text: 'SIMD', link: '/simd' },
      { text: 'NUMA', link: '/numa' },
      { text: 'Distributed (MPI)', link: '/distributed' },
      { text: 'CLI Reference', link: '/cli' },
    ],
    sidebar: {
      '/': [
        {
          text: 'Documentation',
          items: [
            { text: 'Overview', link: '/' },
            { text: 'Getting Started', link: '/getting-started' },
            { text: 'Command Line Reference', link: '/cli' },
          ],
        },
        {
          text: 'Architecture',
          items: [
            { text: 'Architecture', link: '/architecture' },
            { text: 'Physics & Integrators', link: '/physics' },
          ],
        },
        {
          text: 'Performance & Parallelism',
          items: [
            { text: 'Performance', link: '/performance' },
            { text: 'OpenMP & Threading', link: '/threading' },
            { text: 'SIMD', link: '/simd' },
            { text: 'NUMA-aware placement', link: '/numa' },
            { text: 'Distributed (MPI)', link: '/distributed' },
          ],
        },
        {
          text: 'Instrumentation',
          items: [
            { text: 'Instrumentation', link: '/instrumentation' },
          ],
        },
      ],
    },
    footer: {
      message: 'CPU-only physics. Everything is measured.',
      copyright: 'MIT License',
    },
    search: {
      provider: 'local',
      options: {
        translations: {
          button: { buttonText: 'Search docs', buttonAriaLabel: 'Search docs' },
        },
      },
    },
    outline: {
      label: 'On this page',
      level: [2, 3],
    },
    docFooter: {
      prev: 'Previous',
      next: 'Next',
    },
    lastUpdated: {
      text: 'Last updated',
      formatOptions: { dateStyle: 'short', timeStyle: 'short' },
    },
  },
})

function escapeAttribute(value) {
  return value.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;')
}
