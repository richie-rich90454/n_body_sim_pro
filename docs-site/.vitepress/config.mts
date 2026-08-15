import { defineConfig } from 'vitepress'
import { katex } from '@mdit/plugin-katex'

/*
 * GitHub Pages serves project sites under /<repo>/; asset URLs resolve from
 * the base path. The workflow sets VITEPRESS_BASE to the deployment path;
 * local dev/build use '/' by default.
 */
const base = process.env.VITEPRESS_BASE ?? '/'

export default defineConfig({
  title: 'N-Body Sim Pro',
  description: 'A native CPU HPC simulation and visualization engine for large-scale gravitational N-body systems',
  lang: 'en-US',
  cleanUrls: true,
  lastUpdated: true,
  base,

  /*
   * Fonts are self-hosted (docs-site/public/fonts/fonts.css) so the site has
   * no external font dependency. The build strips the default theme's unused
   * bundled font assets and preload links via scripts/postbuild.mjs.
   */

  head: [
    ['link', { rel: 'stylesheet', href: `${base}fonts/fonts.css` }],
    ['link', { rel: 'icon', href: `${base}favicon.ico`, sizes: '32x32' }],
    ['link', { rel: 'icon', href: `${base}favicon.svg`, type: 'image/svg+xml' }],
    ['link', { rel: 'apple-touch-icon', href: `${base}apple-touch-icon.png` }],
  ],

  markdown: {
    config(md) {
      md.use(katex)
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
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Getting Started', link: '/getting-started' },
      {
        text: 'Architecture',
        items: [
          { text: 'Architecture Overview', link: '/architecture' },
          { text: 'Physics & Integrators', link: '/physics' },
        ],
      },
      {
        text: 'Performance',
        items: [
          { text: 'Performance Overview', link: '/performance' },
          { text: 'OpenMP & Threading', link: '/threading' },
          { text: 'SIMD', link: '/simd' },
          { text: 'NUMA-aware Placement', link: '/numa' },
          { text: 'Distributed (MPI)', link: '/distributed' },
        ],
      },
      { text: 'Instrumentation', link: '/instrumentation' },
      { text: 'CLI Reference', link: '/cli' },
    ],
    sidebar: {
      '/': [
        {
          text: 'Start',
          items: [
            { text: 'Overview', link: '/' },
            { text: 'Getting Started', link: '/getting-started' },
          ],
        },
        {
          text: 'Architecture & Physics',
          items: [
            { text: 'Architecture', link: '/architecture' },
            { text: 'Physics & Integrators', link: '/physics' },
          ],
        },
        {
          text: 'Performance & Parallelism',
          items: [
            { text: 'Performance Overview', link: '/performance' },
            { text: 'OpenMP & Threading', link: '/threading' },
            { text: 'SIMD', link: '/simd' },
            { text: 'NUMA-aware Placement', link: '/numa' },
            { text: 'Distributed (MPI)', link: '/distributed' },
          ],
        },
        {
          text: 'Reference',
          items: [
            { text: 'Instrumentation', link: '/instrumentation' },
            { text: 'CLI Reference', link: '/cli' },
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
