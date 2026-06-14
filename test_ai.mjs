// ─────────────────────────────────────────────────────────────────────────────
//  test_ai.mjs — bilgisayar rakip doğrulaması
//  Çalıştırma:  node test_ai.mjs
// ─────────────────────────────────────────────────────────────────────────────
import assert from 'node:assert';
import { chooseMove, evaluate } from './game_src/ai.mjs';
import { applyMove, status, makePosition, legalMoves } from './game_src/engine.mjs';
import { mk } from './game_src/board.mjs';

function pos(pieces, stm = 'w') {
  const board = new Array(112).fill(null);
  for (const [sq, t, c] of pieces) board[sq] = { t, c };
  return { board, stm, swapUsed: { w: false, b: false }, drawn: false };
}

// ── 1) Bedava taşı alır (Kale'yi piyonla yer) ───────────────────────────────
{
  // Beyaz piyon c4 (mk2,3); siyah Kale d5 (mk3,4) bedava — piyon çapraz alır
  const p = pos([
    [mk(2, 3), 'P', 'w'], [mk(0, 0), 'K', 'w'],
    [mk(3, 4), 'R', 'b'], [mk(10, 9), 'K', 'b'],
  ], 'w');
  const m = chooseMove(p, { maxDepth: 3, timeMs: 500 });
  assert.ok(m, 'hamle bulundu');
  assert.strictEqual(m.to, mk(3, 4), 'AI bedava kaleyi alır');
}

// ── 2) Mat-in-1 bulur ───────────────────────────────────────────────────────
{
  // Siyah şah k10 (mk10,9) köşede, sırası beyazda, şu an şah altında değil.
  //  • Beyaz kale a9 (mk0,8): 9. satırı (rank8) tutar → k9(mk10,8), j9(mk9,8) kapalı.
  //  • Beyaz kale b1 (mk1,0): b-sütunu boş; b1→b10 (mk1,9) ile 10. satıra çıkar →
  //    k10 şahını yatay tehdit; j10(mk9,9) de bu kale tarafından kapanır → MAT.
  //  (a9 kalesi b-sütununu engellemez; iki kale farklı sütunda → tıkanma yok.)
  const p = pos([
    [mk(0, 0), 'K', 'w'],
    [mk(0, 8), 'R', 'w'],   // a9 — rank8 (9. satır) kontrolü
    [mk(1, 0), 'R', 'w'],   // b1 — b10'a çıkıp mat edecek
    [mk(10, 9), 'K', 'b'],  // k10 siyah şah
  ], 'w');
  assert.ok(legalMoves(p, { includeSwap: false }).some(
    (x) => x.from === mk(1, 0) && x.to === mk(1, 9)), 'Rb10 yasal hamle olmalı');
  const m = chooseMove(p, { maxDepth: 3, timeMs: 1000 });
  const np = applyMove(p, m);
  const st = status(np);
  assert.strictEqual(st.result, 'checkmate', `AI mat etmeli (hamle ${JSON.stringify(m)})`);
  assert.strictEqual(st.winner, 'w', 'beyaz kazanır');
}

// ── 3) Materyal değerlendirme perspektifi ───────────────────────────────────
{
  // Beyaz fazladan Kale → beyaz sırada pozitif, siyah sırada negatif
  const pw = pos([[mk(0, 0), 'K', 'w'], [mk(5, 0), 'R', 'w'], [mk(10, 9), 'K', 'b']], 'w');
  const pb = pos([[mk(0, 0), 'K', 'w'], [mk(5, 0), 'R', 'w'], [mk(10, 9), 'K', 'b']], 'b');
  assert.ok(evaluate(pw) > 0, 'beyaz sırada beyaz Kale avantajı pozitif');
  assert.ok(evaluate(pb) < 0, 'siyah sırada (aynı tahta) negatif');
}

// ── 4) Başlangıçtan hamle döndürür ve zaman sınırına uyar ───────────────────
{
  const p = makePosition();
  const t0 = Date.now();
  const m = chooseMove(p, { maxDepth: 4, timeMs: 700 });
  const dt = Date.now() - t0;
  assert.ok(m, 'başlangıçta hamle döner');
  assert.ok(legalMoves(p, { includeSwap: false }).some(
    (x) => x.from === m.from && x.to === m.to), 'dönen hamle yasal');
  assert.ok(dt < 2500, `zaman sınırına yakın kalmalı (ölçülen ${dt}ms)`);
}

// ── 5) Asılı taşı bırakmaz (savunma): tehdit altındaki kaleyi kurtarır ───────
{
  // Beyaz Kale d4(mk3,3) siyah Kale d10(mk3,9) tarafından tehdit; beyaz sırada.
  // AI kaleyi güvenli kareye taşımalı (ya da rakibi almalı). Burada karşılıklı tehdit:
  // beyaz kale d-sütununda siyah kaleyi de tehdit ediyor → en iyi: siyah kaleyi al (d10).
  const p = pos([
    [mk(0, 0), 'K', 'w'], [mk(3, 3), 'R', 'w'],
    [mk(3, 9), 'R', 'b'], [mk(10, 0), 'K', 'b'],
  ], 'w');
  const m = chooseMove(p, { maxDepth: 3, timeMs: 600 });
  assert.strictEqual(m.to, mk(3, 9), 'AI karşı kaleyi alır (asılı taşı bedavaya vermez)');
}

console.log('OK — AI: bedava taş alma, mat-in-1, materyal perspektifi, zaman sınırı, savunma.');
