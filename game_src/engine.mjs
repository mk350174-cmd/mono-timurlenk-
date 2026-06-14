// ─────────────────────────────────────────────────────────────────────────────
//  Timurlenk Satrancı — kural motoru (saf mantık, DOM'dan bağımsız)
//  apex_timur.cpp'nin birebir JS karşılığı:
//    • Hamle üretimi  (movegen.cpp: gen_* / slide / jump / zurafa_ray)
//    • Saldırı tespiti (board.cpp: is_attacked / attacked_by_*)
//    • Yasallık süzme (generate_legal: hamleden sonra şah tehdit altında olmamalı)
//  Taş gösterimi: kare = null | {t:'K'|'R'|…, c:'w'|'b'} (board.mjs ile aynı)
//  Hamle: { from, to, type:'normal'|'promo'|'swap'|'citadel', promo:harf|null }
// ─────────────────────────────────────────────────────────────────────────────

import {
  FILES, RANKS, NORMAL, WHITE_CITADEL, BLACK_CITADEL,
  mk, fileOf, rankOf, setupBoard,
} from './board.mjs';

// Citadel komşuları (apex_timur.cpp:77–78)
export const WHITE_CITADEL_NEIGHBOR = 1 * FILES + 10; // k2 = 21
export const BLACK_CITADEL_NEIGHBOR = 8 * FILES + 0;  // a9 = 88

// Materyal değerleri — centipawn (apex_timur.cpp:150)
export const PIECE_VALUE = {
  K: 20000, R: 550, Z: 480, T: 340, N: 325, C: 330,
  E: 290, W: 260, F: 160, V: 130, P: 100,
};

// Piyon terfi seçenekleri (Şah ve Piyon hariç 9 taş — apex_timur.cpp:3766)
export const PROMO_OPTIONS = ['R', 'Z', 'T', 'N', 'C', 'E', 'W', 'F', 'V'];

const inB = (f, r) => f >= 0 && f < FILES && r >= 0 && r < RANKS;
const other = (c) => (c === 'w' ? 'b' : 'w');

// ── Pozisyon ──────────────────────────────────────────────────────────────────
export function makePosition() {
  return { board: setupBoard(), stm: 'w', swapUsed: { w: false, b: false }, drawn: false };
}
export function clonePosition(pos) {
  return {
    board: pos.board.slice(),
    stm: pos.stm,
    swapUsed: { w: pos.swapUsed.w, b: pos.swapUsed.b },
    drawn: pos.drawn,
  };
}
export function findKing(board, color) {
  for (let s = 0; s < board.length; s++) {
    const p = board[s];
    if (p && p.t === 'K' && p.c === color) return s;
  }
  return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Saldırı tespiti — apex_timur.cpp Board::is_attacked / attacked_by_*
//  "by" rengindeki bir taş sq karesini tehdit ediyor mu?
// ─────────────────────────────────────────────────────────────────────────────
function isType(board, f, r, by, t) {
  if (!inB(f, r)) return false;
  const p = board[mk(f, r)];
  return !!p && p.c === by && p.t === t;
}

// Zürafa saldırısı: çapraz 1 (boş) + düz min 3 (ara kareler boş) — generate ile tutarlı
function zurafaAttacks(board, zsq, tf0, tr0) {
  const zf = fileOf(zsq), zr = rankOf(zsq);
  const cdx = [1, 1, -1, -1], cdy = [1, -1, 1, -1];
  const sdx = [1, -1, 0, 0],  sdy = [0, 0, 1, -1];
  for (let cd = 0; cd < 4; cd++) {
    const mf = zf + cdx[cd], mr = zr + cdy[cd];
    if (!inB(mf, mr) || board[mk(mf, mr)] != null) continue; // çapraz kare dolu/dışı
    for (let sd = 0; sd < 4; sd++) {
      for (let step = 1; ; step++) {
        const tf = mf + sdx[sd] * step, tr = mr + sdy[sd] * step;
        if (!inB(tf, tr)) break;
        const occ = board[mk(tf, tr)] != null;
        if (step < 3) { if (occ) break; continue; }
        if (tf === tf0 && tr === tr0) return true; // adım ≥3: hedefe ulaştı
        if (occ) break;
      }
    }
  }
  return false;
}

export function isAttacked(board, sq, by) {
  if (sq >= NORMAL) return false; // Citadel'e saldırı yok (apex:3082)
  const f = fileOf(sq), r = rankOf(sq);

  // Şah (8 yön 1 kare)
  for (let dx = -1; dx <= 1; dx++)
    for (let dy = -1; dy <= 1; dy++)
      if ((dx || dy) && isType(board, f + dx, r + dy, by, 'K')) return true;

  // Kale (yatay/dikey kayma)
  {
    const dx = [1, -1, 0, 0], dy = [0, 0, 1, -1];
    for (let d = 0; d < 4; d++) {
      let nf = f + dx[d], nr = r + dy[d];
      while (inB(nf, nr)) {
        const p = board[mk(nf, nr)];
        if (p) { if (p.c === by && p.t === 'R') return true; break; }
        nf += dx[d]; nr += dy[d];
      }
    }
  }

  // Sıçrayıcılar — geometri tablosu
  const leapers = [
    ['T', [[2, 0], [-2, 0], [0, 2], [0, -2]]],          // Talia
    ['W', [[2, 0], [-2, 0], [0, 2], [0, -2]]],          // Savaş (Talia ile aynı şekil)
    ['N', [[2, 1], [2, -1], [-2, 1], [-2, -1], [1, 2], [1, -2], [-1, 2], [-1, -2]]],
    ['C', [[3, 1], [3, -1], [-3, 1], [-3, -1], [1, 3], [1, -3], [-1, 3], [-1, -3]]],
    ['E', [[2, 2], [2, -2], [-2, 2], [-2, -2]]],          // Fil
    ['F', [[1, 1], [1, -1], [-1, 1], [-1, -1]]],          // Ferz
    ['V', [[1, 0], [-1, 0], [0, 1], [0, -1]]],            // Vali
  ];
  for (const [t, offs] of leapers)
    for (const [dx, dy] of offs)
      if (isType(board, f + dx, r + dy, by, t)) return true;

  // Piyon (çapraz ileri yeme — saldıran kare ters yönde) (apex:3286)
  const ar = (by === 'w') ? r - 1 : r + 1;
  if (isType(board, f - 1, ar, by, 'P') || isType(board, f + 1, ar, by, 'P')) return true;

  // Zürafa
  for (let s = 0; s < NORMAL; s++) {
    const p = board[s];
    if (p && p.c === by && p.t === 'Z' && zurafaAttacks(board, s, f, r)) return true;
  }
  return false;
}

export function inCheck(pos, color) {
  const ksq = findKing(pos.board, color);
  if (ksq < 0 || ksq >= NORMAL) return false; // şah yoksa/citadel'deyse tehdit yok
  return isAttacked(pos.board, ksq, other(color));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hamle üretimi — apex_timur.cpp MoveGenerator
// ─────────────────────────────────────────────────────────────────────────────
const M = (from, to, type = 'normal', promo = null) => ({ from, to, type, promo });

function slide(board, from, c, dx, dy, out) {
  let f = fileOf(from) + dx, r = rankOf(from) + dy;
  while (inB(f, r)) {
    const to = mk(f, r), p = board[to];
    if (!p) out.push(M(from, to));
    else { if (p.c !== c) out.push(M(from, to)); break; }
    f += dx; r += dy;
  }
}
function jump(board, from, c, dx, dy, out) {
  const f = fileOf(from) + dx, r = rankOf(from) + dy;
  if (!inB(f, r)) return;
  const to = mk(f, r), p = board[to];
  if (!p || p.c !== c) out.push(M(from, to));
}
function zurafaRay(board, from, c, df, dr, out) {
  const f0 = fileOf(from), r0 = rankOf(from);
  const cf = f0 + df, cr = r0 + dr;
  if (!inB(cf, cr) || board[mk(cf, cr)] != null) return; // çapraz kare boş olmalı
  const sdx = [1, -1, 0, 0], sdy = [0, 0, 1, -1];
  for (let s = 0; s < 4; s++) {
    for (let step = 1; ; step++) {
      const tf = cf + sdx[s] * step, tr = cr + sdy[s] * step;
      if (!inB(tf, tr)) break;
      const to = mk(tf, tr), p = board[to];
      if (step < 3) { if (p) break; continue; } // ara kare dolu → engel
      if (!p) out.push(M(from, to));
      else { if (p.c !== c) out.push(M(from, to)); break; }
    }
  }
}

function genPawn(board, from, c, out, promoAll) {
  const f = fileOf(from), r = rankOf(from);
  const dr = (c === 'w') ? 1 : -1;
  const promoRank = (c === 'w') ? RANKS - 1 : 0;
  const pushTo = (to, rankReached) => {
    if (rankReached === promoRank) {
      if (promoAll) for (const pt of PROMO_OPTIONS) out.push(M(from, to, 'promo', pt));
      else out.push(M(from, to, 'promo', 'R')); // AI: en değerli taşa (Kale)
    } else out.push(M(from, to));
  };
  // ileri 1 (çift adım yok)
  const nr = r + dr;
  if (inB(f, nr) && !board[mk(f, nr)]) pushTo(mk(f, nr), nr);
  // çapraz yeme
  for (const dcf of [-1, 1]) {
    const cf = f + dcf, cr = r + dr;
    if (!inB(cf, cr)) continue;
    const p = board[mk(cf, cr)];
    if (p && p.c !== c) pushTo(mk(cf, cr), cr);
  }
}

function genKing(board, from, c, out) {
  for (let dx = -1; dx <= 1; dx++)
    for (let dy = -1; dy <= 1; dy++)
      if (dx || dy) jump(board, from, c, dx, dy, out);
  // Citadel girişi (apex:3619): beyaz a9'dan siyah citadel'e, siyah k2'den beyaz citadel'e
  if (c === 'w' && from === BLACK_CITADEL_NEIGHBOR) out.push(M(from, BLACK_CITADEL, 'citadel'));
  if (c === 'b' && from === WHITE_CITADEL_NEIGHBOR) out.push(M(from, WHITE_CITADEL, 'citadel'));
}

// Bir taşın pseudo-legal hamleleri
function genPiece(board, from, c, t, out, promoAll) {
  switch (t) {
    case 'K': genKing(board, from, c, out); break;
    case 'R': slide(board, from, c, 1, 0, out); slide(board, from, c, -1, 0, out);
              slide(board, from, c, 0, 1, out); slide(board, from, c, 0, -1, out); break;
    case 'Z': zurafaRay(board, from, c, 1, 1, out); zurafaRay(board, from, c, 1, -1, out);
              zurafaRay(board, from, c, -1, 1, out); zurafaRay(board, from, c, -1, -1, out); break;
    case 'T': jump(board, from, c, 2, 0, out); jump(board, from, c, -2, 0, out);
              jump(board, from, c, 0, 2, out); jump(board, from, c, 0, -2, out); break;
    case 'N': for (const [dx, dy] of [[2,1],[2,-1],[-2,1],[-2,-1],[1,2],[1,-2],[-1,2],[-1,-2]])
                jump(board, from, c, dx, dy, out); break;
    case 'C': for (const [dx, dy] of [[3,1],[3,-1],[-3,1],[-3,-1],[1,3],[1,-3],[-1,3],[-1,-3]])
                jump(board, from, c, dx, dy, out); break;
    case 'E': for (const [dx, dy] of [[2,2],[2,-2],[-2,2],[-2,-2]]) jump(board, from, c, dx, dy, out); break;
    case 'W': jump(board, from, c, 2, 0, out); jump(board, from, c, -2, 0, out);
              jump(board, from, c, 0, 2, out); jump(board, from, c, 0, -2, out); break;
    case 'F': for (const [dx, dy] of [[1,1],[1,-1],[-1,1],[-1,-1]]) jump(board, from, c, dx, dy, out); break;
    case 'V': jump(board, from, c, 1, 0, out); jump(board, from, c, -1, 0, out);
              jump(board, from, c, 0, 1, out); jump(board, from, c, 0, -1, out); break;
    case 'P': genPawn(board, from, c, out, promoAll); break;
  }
}

// Swap: şah ile kendi taşlarından biri yer değiştirir (1 hak, şah altında yasak — apex:3799)
function genSwap(pos, out) {
  const c = pos.stm;
  if (pos.swapUsed[c]) return;
  if (inCheck(pos, c)) return;
  const ksq = findKing(pos.board, c);
  if (ksq < 0) return;
  for (let s = 0; s < NORMAL; s++) {
    if (s === ksq) continue;
    const p = pos.board[s];
    if (p && p.c === c) out.push(M(ksq, s, 'swap'));
  }
}

export function genPseudo(pos, { includeSwap = true, promoAll = true } = {}) {
  const out = [];
  const c = pos.stm;
  for (let s = 0; s < NORMAL; s++) {
    const p = pos.board[s];
    if (p && p.c === c) genPiece(pos.board, s, c, p.t, out, promoAll);
  }
  if (includeSwap) genSwap(pos, out);
  return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hamle uygula (immutable) — yeni pozisyon döndürür
// ─────────────────────────────────────────────────────────────────────────────
export function applyMove(pos, m) {
  const np = clonePosition(pos);
  const c = pos.stm;
  const b = np.board;
  if (m.type === 'swap') {
    const tmp = b[m.from]; b[m.from] = b[m.to]; b[m.to] = tmp;
    np.swapUsed[c] = true;
  } else if (m.type === 'citadel') {
    b[m.to] = b[m.from]; b[m.from] = null;
    np.drawn = true; // şah rakip citadel'e girdi → beraberlik
  } else if (m.type === 'promo') {
    b[m.to] = { t: m.promo, c }; b[m.from] = null;
  } else {
    b[m.to] = b[m.from]; b[m.from] = null;
  }
  np.stm = other(c);
  return np;
}

// Yasal hamleler: pseudo üret, uygula, kendi şahı tehdit altında değilse koru
export function legalMoves(pos, opts = {}) {
  const c = pos.stm;
  const out = [];
  for (const m of genPseudo(pos, opts)) {
    const np = applyMove(pos, m);
    if (!inCheck(np, c)) out.push(m); // np.stm karşıya geçti; biz c'nin şahına bakarız
  }
  return out;
}

// Belirli bir kareden çıkan yasal hamleler (UI seçim için)
export function legalFrom(pos, from, opts = {}) {
  return legalMoves(pos, opts).filter((m) => m.from === from);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Oyun durumu
//  'ongoing' | 'checkmate' (kazanan stm'in rakibi) | 'stalemate' | 'draw'
// ─────────────────────────────────────────────────────────────────────────────
export function status(pos) {
  if (pos.drawn) return { over: true, result: 'draw', reason: 'citadel' };
  const moves = legalMoves(pos, { includeSwap: true, promoAll: false });
  if (moves.length === 0) {
    if (inCheck(pos, pos.stm))
      return { over: true, result: 'checkmate', winner: other(pos.stm), reason: 'mat' };
    return { over: true, result: 'stalemate', reason: 'pat' };
  }
  return { over: false, result: 'ongoing', check: inCheck(pos, pos.stm) };
}

export function isCapture(pos, m) {
  return m.type !== 'swap' && m.type !== 'citadel' && pos.board[m.to] != null;
}
