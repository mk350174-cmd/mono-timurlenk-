// ─────────────────────────────────────────────────────────────────────────────
//  build_game.mjs — Timurlenk oyununu timurlenk.html içine GÖMER (authoring aracı)
//  Yaptıkları:
//   1) chess_assets_256_bundle (1).zip içindeki manifest.json'dan 22 base64 taş
//      görselini çekip motor harfine göre PIECE_IMG nesnesi kurar.
//   2) game_src/{game.css, board.mjs, ui.js} kaynaklarını okur.
//   3) timurlenk.html'e: </style> öncesine oyun CSS'i, </body> öncesine #game
//      ekranı + tek <script> (PIECE_IMG + model + UI) enjekte eder.
//  İşaretçiler (marker) sayesinde tekrar çalıştırmak güvenlidir (idempotent).
//  Çalıştırma:  node build_game.mjs
// ─────────────────────────────────────────────────────────────────────────────
import { readFileSync, writeFileSync } from 'node:fs';
import { execSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const ROOT = dirname(fileURLToPath(import.meta.url));
const HTML = join(ROOT, 'timurlenk.html');
const ZIP = join(ROOT, 'chess_assets_256_bundle (1).zip');

// Motor harfi (apex_timur.cpp PieceType) -> manifest slug
const LETTER_SLUG = {
  K: 'sah', R: 'kale', Z: 'zurafa', T: 'talia_gozcu', N: 'at',
  C: 'deve', E: 'fil', W: 'kurt', F: 'vezir', V: 'piyon', P: 'general',
};

// 1) manifest.json -> PIECE_IMG
const manifestRaw = execSync(`unzip -p "${ZIP}" manifest.json`, { maxBuffer: 1 << 28 }).toString();
const manifest = JSON.parse(manifestRaw);
const bySlug = {};
for (const code in manifest.pieces) {
  const p = manifest.pieces[code];
  bySlug[p.slug] = p;
}
const PIECE_IMG = {};
for (const L in LETTER_SLUG) {
  const slug = LETTER_SLUG[L];
  const p = bySlug[slug];
  if (!p) throw new Error(`manifest'te slug bulunamadı: ${slug}`);
  if (!p.white?.dataUrl || !p.black?.dataUrl) throw new Error(`dataUrl eksik: ${slug}`);
  PIECE_IMG[L] = { w: p.white.dataUrl, b: p.black.dataUrl };
}

// 2) kaynaklar
// ES modül sözdizimini tek kapsamda çalışacak şekilde temizle:
//   - "import { … } from '…';" satırlarını sil (her şey aynı script kapsamında)
//   - "export " anahtarını sil
const stripModule = (src) => src
  .replace(/^\s*import\s+[\s\S]*?from\s*['"][^'"]+['"];?\s*$/gm, '')
  .replace(/\bexport\s+/g, '');
const css    = readFileSync(join(ROOT, 'game_src', 'game.css'), 'utf8');
const board  = stripModule(readFileSync(join(ROOT, 'game_src', 'board.mjs'), 'utf8'));
const engine = stripModule(readFileSync(join(ROOT, 'game_src', 'engine.mjs'), 'utf8'));
const ai     = stripModule(readFileSync(join(ROOT, 'game_src', 'ai.mjs'), 'utf8'));
const ui     = readFileSync(join(ROOT, 'game_src', 'ui.js'), 'utf8');

// HTML'i oku (tahta doku çıkarımı + enjeksiyon için)
let html = readFileSync(HTML, 'utf8');

// Tahta doku görsellerini .tt seçici kutularından çıkar (ilki aktif tema)
const texMatches = [...html.matchAll(/class="tt[^"]*"\s+onclick="selT\(this\)"><img\s+src="(data:[^"]+)"/g)];
const boardTextures = texMatches.map(m => m[1]);
if (!boardTextures.length) console.warn('Uyarı: tahta doku görselleri bulunamadı — --g-board-tex ayarlanmadı');
const boardTexCss = boardTextures.length
  ? `:root{--g-board-tex:url("${boardTextures[0]}")}\n`
  : '';

// 3) enjekte edilecek bloklar
const gameDom = `<div id="game">
  <div class="g-top">
    <button class="g-back" onclick="exitGame()">&#8249; Menü</button>
    <div class="g-title">Timurlenk Satrancı</div>
    <button class="g-new" onclick="newGame()">Yeni Oyun</button>
  </div>
  <div class="g-bar">
    <div class="g-status" id="gStatus">—</div>
    <div class="g-actions">
      <label class="g-side">Renk
        <select id="gSide" onchange="setSide(this.value)">
          <option value="w">Beyaz</option>
          <option value="b">Siyah</option>
        </select>
      </label>
      <button class="g-swap" id="gSwap" onclick="toggleSwap()">Yer Değiştir</button>
    </div>
  </div>
  <div class="g-stage"><div class="g-board" id="gBoard"></div></div>
  <div class="g-over" id="gOver"><div class="g-over-card">
    <div id="gOverMsg"></div><button onclick="newGame()">Yeni Oyun</button>
  </div></div>
  <div class="g-promo" id="gPromo"><div class="g-promo-card">
    <div class="g-promo-t">Piyon terfisi — taş seç</div>
    <div class="g-promo-row" id="gPromoRow"></div>
  </div></div>
</div>`;

// CSS, mevcut <style> öğesinin İÇİNE ham olarak girer (yeni <style> açmaz → iç içe
// geçme yok). İşaretçiler CSS yorumu biçiminde.
const styleBlock =
  `\n/*TC_GAME_CSS_START*/\n${boardTexCss}${css}\n/*TC_GAME_CSS_END*/\n`;

const scriptBlock =
  `\n<!--TC_GAME_START-->\n${gameDom}\n<script>\n(function(){\n` +
  `const PIECE_IMG=${JSON.stringify(PIECE_IMG)};\n` +
  `${board}\n${engine}\n${ai}\n${ui}\n})();\n</script>\n<!--TC_GAME_END-->\n`;

// güvenlik: gömülecek içerikte script tag'ini erken kapatacak bir şey olmamalı
if (/<\/script>/i.test(PIECE_IMG.K.w + board + engine + ai + ui)) {
  throw new Error('Beklenmedik </script> içeriği — enjeksiyon güvenli değil');
}

// önceki enjeksiyonları temizle (idempotent)
html = html.replace(/\n?\/\*TC_GAME_CSS_START\*\/[\s\S]*?\/\*TC_GAME_CSS_END\*\/\n?/g, '');
html = html.replace(/\n?<!--TC_GAME_START-->[\s\S]*?<!--TC_GAME_END-->\n?/g, '');

// tekil anchor kontrolü
const styleClose = (html.match(/<\/style>/g) || []).length;
const bodyClose = (html.match(/<\/body>/g) || []).length;
if (styleClose !== 1) throw new Error(`</style> sayısı 1 değil: ${styleClose}`);
if (bodyClose !== 1) throw new Error(`</body> sayısı 1 değil: ${bodyClose}`);

// enjekte et ($ özel karakter sorununu önlemek için fonksiyon replacer)
html = html.replace('</style>', () => styleBlock + '</style>');
html = html.replace('</body>', () => scriptBlock + '</body>');

writeFileSync(HTML, html);

const kb = (s) => (Buffer.byteLength(s) / 1024).toFixed(0) + ' KB';
console.log('✓ Gömme tamam.');
console.log('  Taş görselleri:', Object.keys(PIECE_IMG).length, '× {w,b}');
console.log('  CSS bloğu     :', kb(styleBlock));
console.log('  Script bloğu  :', kb(scriptBlock));
console.log('  Yeni HTML     :', kb(html));
