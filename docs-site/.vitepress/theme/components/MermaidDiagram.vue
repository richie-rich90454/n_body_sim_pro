<script setup lang="ts">
import { onMounted, ref, watch } from 'vue'
import { useData } from 'vitepress'

const props = defineProps<{ code: string }>()

const { isDark } = useData()

const rendered = ref('')
const error = ref('')

let renderId = 0

async function waitForFonts() {
  try {
    if (typeof document !== 'undefined' && document.fonts) {
      // Labels are measured with the mono stack; make sure it is loaded before
      // mermaid sizes the nodes, otherwise the boxes are too small and the
      // text is clipped by the node border.
      if (document.fonts.load) {
        await Promise.race([
          document.fonts.load('14px "Noto Sans Mono"'),
          document.fonts.load('14px monospace'),
          new Promise((resolve) => setTimeout(resolve, 2000)),
        ])
      }
      await Promise.race([document.fonts.ready, new Promise((resolve) => setTimeout(resolve, 2000))])
    }
  } catch {
    // Fonts are a best-effort improvement; never block the diagram on them.
  }
}

async function render() {
  renderId += 1
  const current = renderId
  rendered.value = ''
  error.value = ''
  try {
    const mermaid = (await import('mermaid')).default
    await waitForFonts()
    const dark = isDark.value
    mermaid.initialize({
      startOnLoad: false,
      theme: dark ? 'dark' : 'neutral',
      flowchart: { htmlLabels: true, padding: 16 },
      themeVariables: dark
        ? {
            darkMode: true,
            background: '#0d0d14',
            primaryColor: '#101019',
            primaryTextColor: '#c8d0dd',
            primaryBorderColor: '#3a4152',
            lineColor: '#5a6378',
            secondaryColor: '#14141f',
            tertiaryColor: '#16161f',
            fontFamily: '"Noto Sans Mono", monospace',
            fontSize: '13px',
          }
        : {
            darkMode: false,
            background: '#ffffff',
            primaryColor: '#eef0f4',
            primaryTextColor: '#161a21',
            primaryBorderColor: '#b9c0cd',
            lineColor: '#6f7889',
            secondaryColor: '#eceef2',
            tertiaryColor: '#f4f5f7',
            fontFamily: '"Noto Sans Mono", monospace',
            fontSize: '13px',
          },
      securityLevel: 'loose',
    })
    const id = 'mermaid_' + Math.random().toString(36).slice(2)
    const { svg } = await mermaid.render(id, props.code)
    if (current === renderId) {
      rendered.value = svg
    }
  } catch (e) {
    if (current === renderId) {
      error.value = String(e)
    }
  }
}

onMounted(render)
watch(isDark, render)
</script>

<template>
  <div v-if="error" class="mermaid-error">mermaid render failed: {{ error }}</div>
  <div v-else class="mermaid-render" v-html="rendered" />
</template>

<style scoped>
.mermaid-render {
  display: flex;
  justify-content: center;
  padding: 1rem 0;
  overflow-x: auto;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  background: var(--vp-c-bg-elv);
  margin: 1rem 0;
}
.mermaid-render :deep(svg) {
  max-width: 100%;
  height: auto;
}
/* Never let node labels be clipped by their borders, whatever font metrics
 * the browser used when mermaid measured the text. */
.mermaid-render :deep(foreignObject) {
  overflow: visible;
}
.mermaid-error {
  font-family: var(--vp-font-family-mono);
  color: var(--vp-c-danger-1);
  padding: 0.75rem 1rem;
  border: 1px solid var(--vp-c-danger-1);
  border-radius: 6px;
  background: var(--nsp-code-hl);
  margin: 1rem 0;
  font-size: 0.85rem;
  white-space: pre-wrap;
}
</style>
