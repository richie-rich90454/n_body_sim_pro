import fs from 'node:fs'
import path from 'node:path'

/*
 * Post-build cleanup for the VitePress site.
 *
 * VitePress bundles its default Inter @font-face rules, Inter woff2 assets,
 * and matching <link rel="preload"> tags into the output. The theme overrides
 * the font stacks with Noto Sans / Noto Sans Mono, so those Inter artifacts
 * are dead weight and, once the woff2 files are deleted, the preload links
 * would 404. This script removes all of them after the build has finished
 * writing the site.
 */

const distDir = path.join(process.cwd(), '.vitepress', 'dist')
const assetsDir = path.join(distDir, 'assets')

if (!fs.existsSync(assetsDir)) {
  console.log('postbuild: no dist/assets found, nothing to clean')
  process.exit(0)
}

let removedFonts = 0
for (const file of fs.readdirSync(assetsDir)) {
  if (file.startsWith('inter-') && file.endsWith('.woff2')) {
    fs.rmSync(path.join(assetsDir, file), { force: true })
    removedFonts++
  }
}

let patchedCss = 0
for (const file of fs.readdirSync(assetsDir)) {
  if (!file.endsWith('.css')) continue
  const full = path.join(assetsDir, file)
  let css = fs.readFileSync(full, 'utf8')
  if (!css.includes('font-family:Inter')) continue
  css = css.replace(/@font-face\{[^}]*font-family:Inter[^}]*\}/g, '')
  css = css.replace(/"Inter"/g, '"Noto Sans"')
  fs.writeFileSync(full, css)
  patchedCss++
}

let patchedHtml = 0
for (const file of fs.readdirSync(distDir)) {
  if (!file.endsWith('.html')) continue
  const full = path.join(distDir, file)
  let html = fs.readFileSync(full, 'utf8')
  const before = html.length
  html = html.replace(/<link rel="preload"[^>]*inter-[^>]*\.woff2[^>]*>\s*/g, '')
  if (html.length !== before) {
    fs.writeFileSync(full, html)
    patchedHtml++
  }
}

console.log(
  `postbuild: removed ${removedFonts} Inter woff2, patched ${patchedCss} CSS, ` +
    `patched ${patchedHtml} HTML`
)
