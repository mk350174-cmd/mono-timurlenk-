// ─────────────────────────────────────────────────────────────────────────────
//  test_board.mjs — başlangıç dizilişinin apex_timur.cpp setup() ile uyumu
//  Çalıştırma:  node test_board.mjs
// ─────────────────────────────────────────────────────────────────────────────
import assert from 'node:assert';
import {
  setupBoard, serialize, mk,
  START_W_MAIN, START_B_MAIN,
} from './game_src/board.mjs';

const b = setupBoard();

// 1) Tam diziliş (rank 9 → rank 0)
const E = '.'.repeat(11);
const expected = [
  'e.c.w.w.c.e', // rank 9 (siyah ek)
  'rntzvkfztnr', // rank 8 (siyah ana — dosya4=v, dosya6=f)
  'ppppppppppp', // rank 7 (siyah piyon)
  E, E, E, E,    // rank 6–3 boş
  'PPPPPPPPPPP', // rank 2 (beyaz piyon)
  'RNTZFKVZTNR', // rank 1 (beyaz ana — dosya4=F, dosya6=V)
  'E.C.W.W.C.E', // rank 0 (beyaz ek)
].join('/');
const got = serialize(b);
assert.strictEqual(got, expected, `Diziliş hatalı.\nbeklenen: ${expected}\nbulunan : ${got}`);

// 2) Toplam ve taraf başına sayım
let total = 0, w = 0, bl = 0;
const cnt = {};
for (const p of b) {
  if (!p) continue;
  total++;
  p.c === 'w' ? w++ : bl++;
  const key = p.c === 'w' ? p.t : p.t.toLowerCase();
  cnt[key] = (cnt[key] || 0) + 1;
}
assert.strictEqual(total, 56, `toplam taş 56 olmalı, bulundu ${total}`);
assert.strictEqual(w, 28, `beyaz 28 olmalı, bulundu ${w}`);
assert.strictEqual(bl, 28, `siyah 28 olmalı, bulundu ${bl}`);

// 3) Tip sayıları (beyaz büyük harf)
const want = { K: 1, R: 2, N: 2, T: 2, Z: 2, F: 1, V: 1, E: 2, C: 2, W: 2, P: 11 };
for (const L in want) {
  assert.strictEqual(cnt[L], want[L], `beyaz ${L} sayısı ${want[L]} olmalı, bulundu ${cnt[L]}`);
}

// 4) Tarihsel asimetri (Ferz/Vali)
assert.strictEqual(START_W_MAIN[4], 'F', 'beyaz dosya4 = Ferz');
assert.strictEqual(START_W_MAIN[6], 'V', 'beyaz dosya6 = Vali');
assert.strictEqual(START_B_MAIN[4], 'V', 'siyah dosya4 = Vali');
assert.strictEqual(START_B_MAIN[6], 'F', 'siyah dosya6 = Ferz');

// 5) Şah konumları
assert.deepStrictEqual(b[mk(5, 1)], { t: 'K', c: 'w' }, 'beyaz şah f1(=mk5,1)');
assert.deepStrictEqual(b[mk(5, 8)], { t: 'K', c: 'b' }, 'siyah şah f8(=mk5,8)');
// Savaş makineleri (ek sırada dosya 4 ve 6)
assert.deepStrictEqual(b[mk(4, 0)], { t: 'W', c: 'w' });
assert.deepStrictEqual(b[mk(6, 0)], { t: 'W', c: 'w' });

console.log('OK — diziliş ve sayımlar apex_timur.cpp ile uyumlu.');
console.log(got.split('/').join('\n'));
