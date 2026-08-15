<script setup lang="ts">
import { onMounted, ref } from 'vue'

const props = defineProps<{ code: string }>()

const rendered = ref('')
const error = ref('')

onMounted(async () => {
  try {
    const mermaid = (await import('mermaid')).default
    mermaid.initialize({
      startOnLoad: false,
      theme: 'dark',
      themeVariables: {
        darkMode: true,
        background: '#0d0d14',
        primaryColor: '#101019',
        primaryTextColor: '#c8d0dd',
        primaryBorderColor: '#3a4152',
        lineColor: '#5a6378',
        secondaryColor: '#14141f',
        tertiaryColor: '#16161f',
        fontFamily: '"IBM Plex Mono", monospace',
        fontSize: '13px',
      },
      securityLevel: 'loose',
    })
    const id = 'mermaid_' + Math.random().toString(36).slice(2)
    const { svg } = await mermaid.render(id, props.code)
    rendered.value = svg
  } catch (e) {
    error.value = String(e)
  }
})
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
  border: 1px solid var(--vp-c-border);
  border-radius: 6px;
  background: #0d0d14;
  margin: 1rem 0;
}
.mermaid-error {
  font-family: var(--vp-font-family-mono);
  color: #e06c75;
  padding: 0.75rem 1rem;
  border: 1px solid #3f2427;
  border-radius: 6px;
  background: #140d0e;
  margin: 1rem 0;
  font-size: 0.85rem;
  white-space: pre-wrap;
}
</style>
