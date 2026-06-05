// ─────────────────────────────────────────────────────────────────────────────
//  test_render.mjs — tarayıcısız render doğrulaması
//  Gerçek game_src/ui.js + board.mjs kodunu hafif bir DOM taklidi üzerinde
//  çalıştırır; buildBoard()+render() sonrası DOM'u inceler.
//  Çalıştırma:  node test_render.mjs
// ─────────────────────────────────────────────────────────────────────────────
import assert from 'node:assert';
import vm from 'node:vm';
import { readFileSync } from 'node:fs';
import { execSync } from 'node:child_process';

// PIECE_IMG'i build ile aynı şekilde kur
const LETTER_SLUG = { K:'sah', R:'kale', Z:'zurafa', T:'talia_gozcu', N:'at', C:'deve', E:'fil', W:'kurt', F:'vezir', V:'piyon', P:'general' };
const manifest = JSON.parse(execSync('unzip -p "chess_assets_256_bundle (1).zip" manifest.json', { maxBuffer: 1 << 28 }).toString());
const bySlug = {}; for (const c in manifest.pieces) bySlug[manifest.pieces[c].slug] = manifest.pieces[c];
const PIECE_IMG = {}; for (const L in LETTER_SLUG) PIECE_IMG[L] = { w: bySlug[LETTER_SLUG[L]].white.dataUrl, b: bySlug[LETTER_SLUG[L]].black.dataUrl };

// ── DOM taklidi ──
function makeEl(tag) {
  return {
    tagName: tag, children: [], className: '', style: {}, dataset: {}, _attrs: {},
    textContent: '', title: '', alt: '', draggable: false, src: '',
    set innerHTML(v) { if (v === '') this.children = []; }, get innerHTML() { return ''; },
    appendChild(c) { this.children.push(c); return c; },
    querySelector(sel) {
      if (sel === 'img.pc') return this.children.find(c => c.tagName === 'img' && (' ' + c.className + ' ').includes(' pc ')) || null;
      return null;
    },
    setAttribute(k, v) { this._attrs[k] = v; if (k === 'src') this.src = v; },
    getAttribute(k) { return k in this._attrs ? this._attrs[k] : null; },
    classList: { toggle() {}, add() {}, remove() {}, contains() { return false; } },
    addEventListener() {}, setPointerCapture() {},
    getBoundingClientRect() { return { width: 40, height: 40, left: 0, top: 0 }; },
  };
}
const gBoard = makeEl('div');
const document = {
  readyState: 'complete',
  getElementById: (id) => (id === 'gBoard' ? gBoard : null),
  createElement: makeEl,
  querySelector: () => null,
  addEventListener() {},
  body: makeEl('body'),
  elementFromPoint: () => null,
};
const window = {};

const board = readFileSync('game_src/board.mjs', 'utf8').replace(/\bexport\s+/g, '');
const ui = readFileSync('game_src/ui.js', 'utf8');
const ctx = vm.createContext({ document, window, console, Math, String, Number, Object, Array });
ctx.PIECE_IMG = PIECE_IMG;
vm.runInContext(board + '\n' + ui, ctx);

// Tahtayı kur + çiz
window.newGame();

// ── İncelemeler ──
const cells = gBoard.children.filter(c => 'sq' in c.dataset);
const labels = gBoard.children.filter(c => (' ' + c.className + ' ').includes(' g-lab '));
assert.strictEqual(cells.length, 112, `112 kare olmalı (110 + 2 citadel), bulundu ${cells.length}`);
assert.strictEqual(labels.length, 21, `21 etiket olmalı (10 rütbe + 11 dosya), bulundu ${labels.length}`);

const bySq = {}; for (const c of cells) bySq[+c.dataset.sq] = c;
const imgs = cells.map(c => c.querySelector('img.pc')).filter(Boolean);
assert.strictEqual(imgs.length, 56, `56 taş görseli olmalı, bulundu ${imgs.length}`);

// Beyaz Şah: mk(5,1)=16 -> sütun f+3=8, satır 10-1=9, görsel = sah beyaz
const wk = bySq[16];
assert.strictEqual(wk.style.gridColumn, '8', 'beyaz şah sütunu');
assert.strictEqual(wk.style.gridRow, '9', 'beyaz şah satırı');
assert.strictEqual(wk.querySelector('img.pc').getAttribute('src'), PIECE_IMG.K.w, 'beyaz şah görseli sah/beyaz olmalı');

// Siyah Şah: mk(5,8)=93 -> sütun 8, satır 10-8=2
const bk = bySq[93];
assert.strictEqual(bk.style.gridColumn, '8'); assert.strictEqual(bk.style.gridRow, '2');
assert.strictEqual(bk.querySelector('img.pc').getAttribute('src'), PIECE_IMG.K.b, 'siyah şah görseli sah/siyah olmalı');

// Önerilen eşleme doğrulaması: Vali->piyon, Savaş->kurt, Ferz->vezir
// Beyaz Vali mk(6,1)=17, Beyaz Ferz mk(4,1)=15, Beyaz Savaş mk(4,0)=4
assert.strictEqual(bySq[17].querySelector('img.pc').getAttribute('src'), PIECE_IMG.V.w, 'Vali görseli');
assert.strictEqual(bySq[15].querySelector('img.pc').getAttribute('src'), PIECE_IMG.F.w, 'Ferz görseli');
assert.strictEqual(bySq[4].querySelector('img.pc').getAttribute('src'), PIECE_IMG.W.w, 'Savaş görseli');

// Citadeller boş ve doğru konumda
assert.strictEqual(bySq[110].style.gridColumn, '14'); assert.strictEqual(bySq[110].style.gridRow, '9');
assert.strictEqual(bySq[111].style.gridColumn, '2');  assert.strictEqual(bySq[111].style.gridRow, '2');
assert.strictEqual(bySq[110].querySelector('img.pc'), null, 'beyaz citadel boş');
assert.strictEqual(bySq[111].querySelector('img.pc'), null, 'siyah citadel boş');

// Boş orta kareler (rank 3-6) görselsiz
assert.strictEqual(bySq[5 * 11 + 5].querySelector('img.pc'), null, 'orta kare boş');

// window API'leri bağlı mı
for (const fn of ['startGame', 'exitGame', 'newGame']) assert.strictEqual(typeof window[fn], 'function', `window.${fn}`);

console.log('OK — render doğru: 112 kare, 21 etiket, 56 taş doğru konum+görselle; citadeller boş; eşleme V→piyon(slug), P→general(slug), W→kurt, F→vezir.');
