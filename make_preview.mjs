// Başlangıç tahtasının kendine yeten SVG önizlemesi (oyunla aynı veri kaynağı).
// Çıktı: argümanla verilen yol (vars. /tmp/board_preview.svg)
import { setupBoard, mk, FILES, RANKS } from './game_src/board.mjs';
import { writeFileSync } from 'node:fs';
import { execSync } from 'node:child_process';

const LETTER_SLUG = { K:'sah', R:'kale', Z:'zurafa', T:'talia_gozcu', N:'at', C:'deve', E:'fil', W:'kurt', F:'vezir', V:'piyon', P:'general' };
const manifest = JSON.parse(execSync('unzip -p "chess_assets_256_bundle (1).zip" manifest.json', { maxBuffer: 1 << 28 }).toString());
const bySlug = {}; for (const c in manifest.pieces) bySlug[manifest.pieces[c].slug] = manifest.pieces[c];
const IMG = {}; for (const L in LETTER_SLUG) IMG[L] = { w: bySlug[LETTER_SLUG[L]].white.dataUrl, b: bySlug[LETTER_SLUG[L]].black.dataUrl };

const CELL = 66, PAD = 18, LBL = 26, CIT = CELL;
const boardLeft = PAD + LBL + CIT;
const W = boardLeft + FILES * CELL + CIT + PAD;
const H = PAD + RANKS * CELL + LBL + PAD;
const xF = (f) => boardLeft + f * CELL;
const yR = (r) => PAD + (9 - r) * CELL;       // rank9 üstte
const letter = (f) => String.fromCharCode(97 + f);

const b = setupBoard();
const out = [];
out.push(`<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}" font-family="Georgia, serif">`);
out.push(`<rect width="${W}" height="${H}" fill="#0a0704"/>`);
out.push(`<rect x="${PAD/2}" y="${PAD/2}" width="${W-PAD}" height="${H-PAD}" fill="#1a1208" stroke="#8a6a1e" stroke-width="2" rx="6"/>`);

// defs: kullanılan taş görselleri (256 doğal boyut)
out.push('<defs>');
for (const L in IMG) for (const c of ['w', 'b'])
  out.push(`<symbol id="p_${L}${c}" viewBox="0 0 256 256"><image width="256" height="256" href="${IMG[L][c]}"/></symbol>`);
out.push('</defs>');

// kareler
for (let r = 0; r < RANKS; r++) for (let f = 0; f < FILES; f++) {
  const dark = (f + r) % 2 === 0;
  out.push(`<rect x="${xF(f)}" y="${yR(r)}" width="${CELL}" height="${CELL}" fill="${dark ? '#7a5526' : '#e8d5a0'}"/>`);
}
// citadeller
const cit = (x, y) => out.push(`<rect x="${x}" y="${y}" width="${CELL}" height="${CELL}" fill="#34250f" stroke="#8a6a1e"/><text x="${x+CELL/2}" y="${y+CELL/2+10}" fill="#f0d08055" font-size="30" text-anchor="middle">&#8962;</text>`);
cit(xF(11), yR(1)); // beyaz citadel: k2 sağı
cit(boardLeft - CIT, yR(8)); // siyah citadel: a9 solu

// etiketler
for (let r = 0; r < RANKS; r++)
  out.push(`<text x="${PAD + LBL/2}" y="${yR(r) + CELL/2 + 5}" fill="#c9a84c" font-size="15" text-anchor="middle">${r + 1}</text>`);
for (let f = 0; f < FILES; f++)
  out.push(`<text x="${xF(f) + CELL/2}" y="${PAD + RANKS*CELL + 18}" fill="#c9a84c" font-size="15" text-anchor="middle">${letter(f)}</text>`);

// taşlar (kare %90)
const inset = CELL * 0.05, size = CELL * 0.9;
for (let r = 0; r < RANKS; r++) for (let f = 0; f < FILES; f++) {
  const p = b[mk(f, r)];
  if (!p) continue;
  out.push(`<use href="#p_${p.t}${p.c}" x="${xF(f) + inset}" y="${yR(r) + inset}" width="${size}" height="${size}"/>`);
}
out.push(`<text x="${W/2}" y="${H - 4}" fill="#8a6a1e" font-size="12" text-anchor="middle">Timurlenk Satrancı — başlangıç dizilişi (11×10 + 2 citadel)</text>`);
out.push('</svg>');

const dest = process.argv[2] || '/tmp/board_preview.svg';
writeFileSync(dest, out.join('\n'));
console.log('✓ önizleme yazıldı:', dest);
