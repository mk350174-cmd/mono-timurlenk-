// ─────────────────────────────────────────────────────────────────────────────
//  Timurlenk Satrancı — tahta modeli (saf mantık, DOM'dan bağımsız)
//  Kaynak referans: apex_timur.cpp
//    - Tahta: 11 dosya (a–k, 0–10) × 10 rütbe (1–10, 0–9) = 110 kare + 2 citadel
//    - sq = rank * 11 + file ; idx 110 = WHITE_CITADEL, 111 = BLACK_CITADEL
//    - Başlangıç dizilişi: START_RANK0 / START_RANK1_WHITE / START_RANK1_BLACK (852–868)
//  Harfler (apex FEN şeması): K Şah, R Kale, Z Zürafa, T Talia, N At,
//                              C Deve, E Fil, W Savaş, F Ferz, V Vali, P Piyon
// ─────────────────────────────────────────────────────────────────────────────

export const FILES = 11;
export const RANKS = 10;
export const NORMAL = 110;
export const WHITE_CITADEL = 110;
export const BLACK_CITADEL = 111;

export const mk = (f, r) => r * FILES + f;
export const fileOf = (sq) => sq % FILES;
export const rankOf = (sq) => Math.floor(sq / FILES);

// Ek sıra (beyaz rank 0 / siyah rank 9): dosya 0,2,4,6,8,10
export const START_EXTRA  = ['E', null, 'C', null, 'W', null, 'W', null, 'C', null, 'E'];
// Beyaz ana sıra (rank 1)
export const START_W_MAIN = ['R', 'N', 'T', 'Z', 'F', 'K', 'V', 'Z', 'T', 'N', 'R'];
// Siyah ana sıra (rank 8) — Ferz/Vali yer değiştirmiş (tarihsel asimetri)
export const START_B_MAIN = ['R', 'N', 'T', 'Z', 'V', 'K', 'F', 'Z', 'T', 'N', 'R'];

// apex_timur.cpp Board::setup() birebir aynası
export function setupBoard() {
  const b = new Array(112).fill(null);
  for (let f = 0; f < FILES; f++) {
    if (START_EXTRA[f]) {
      b[mk(f, 0)] = { t: START_EXTRA[f], c: 'w' };
      b[mk(f, 9)] = { t: START_EXTRA[f], c: 'b' };
    }
    b[mk(f, 1)] = { t: START_W_MAIN[f], c: 'w' };
    b[mk(f, 8)] = { t: START_B_MAIN[f], c: 'b' };
    b[mk(f, 2)] = { t: 'P', c: 'w' };
    b[mk(f, 7)] = { t: 'P', c: 'b' };
  }
  return b;
}

// Teşhis/test için FEN benzeri seri (rank 9 → rank 0, satırlar '/' ile;
// büyük harf = beyaz, küçük = siyah, boş kare = '.')
export function serialize(b) {
  const rows = [];
  for (let r = RANKS - 1; r >= 0; r--) {
    let row = '';
    for (let f = 0; f < FILES; f++) {
      const p = b[mk(f, r)];
      row += p ? (p.c === 'w' ? p.t : p.t.toLowerCase()) : '.';
    }
    rows.push(row);
  }
  return rows.join('/');
}
