// ─────────────────────────────────────────────────────────────────────────────
//  Timurlenk Satrancı — bilgisayar rakip (saf JS, DOM'dan bağımsız)
//  apex_timur.cpp'nin "NNUE kapalı" değerlendirmesini taban alır:
//    • Materyal farkı (evaluate_fast → b.material(WHITE) - b.material(BLACK))
//    • Negamax + alfa-beta budama, MVV-LVA hamle sıralaması
//    • Sessizlik (quiescence) araması: yatay yanılgıyı azaltmak için yemeler
//    • Yinelemeli derinleşme (iterative deepening) + zaman sınırı (mobil için akıcı)
//  Swap hamleleri AI'da üretilmez (materyal değişmez → değersiz, dallanmayı şişirir;
//  ayrıca şah altında zaten yasak, kaçış sağlamaz). Citadel = beraberlik (skor 0).
// ─────────────────────────────────────────────────────────────────────────────

import {
  PIECE_VALUE, legalMoves, applyMove, inCheck, isCapture,
} from './engine.mjs';

const MATE = 900000;       // apex SCORE_MATE
const INF = 1000000;

// Materyal değerlendirme — sıradaki tarafın perspektifinden (centipawn)
export function evaluate(pos) {
  let s = 0;
  for (const p of pos.board) {
    if (!p) continue;
    s += (p.c === 'w') ? PIECE_VALUE[p.t] : -PIECE_VALUE[p.t];
  }
  return (pos.stm === 'w') ? s : -s;
}

// MVV-LVA: değerli kurbanı ucuz saldırganla almak öncelikli (apex score_moves)
function moveScore(pos, m) {
  let s = 0;
  if (isCapture(pos, m)) {
    const victim = pos.board[m.to], attacker = pos.board[m.from];
    s += 10 * PIECE_VALUE[victim.t] - PIECE_VALUE[attacker.t];
  }
  if (m.type === 'promo') s += 800;
  return s;
}
function order(pos, moves) {
  for (const m of moves) m._s = moveScore(pos, m);
  moves.sort((a, b) => b._s - a._s);
  return moves;
}

const AI_OPTS = { includeSwap: false, promoAll: false };

// Sessizlik araması: yalnız yemeler/terfiler — durağan pozisyona inene dek
function quiesce(pos, alpha, beta, deadline) {
  const standPat = evaluate(pos);
  if (standPat >= beta) return beta;
  if (standPat > alpha) alpha = standPat;
  if (Date.now() > deadline) return alpha;

  const caps = order(pos, legalMoves(pos, AI_OPTS).filter(
    (m) => isCapture(pos, m) || m.type === 'promo'));
  for (const m of caps) {
    const sc = -quiesce(applyMove(pos, m), -beta, -alpha, deadline);
    if (sc >= beta) return beta;
    if (sc > alpha) alpha = sc;
  }
  return alpha;
}

// Negamax + alfa-beta. ply = kökten uzaklık (mat skorunu yakına çekmek için)
function negamax(pos, depth, alpha, beta, ply, deadline, info) {
  if (pos.drawn) return 0;                       // citadel beraberliği
  if (Date.now() > deadline) { info.timeout = true; return evaluate(pos); }
  if (depth <= 0) return quiesce(pos, alpha, beta, deadline);

  const moves = legalMoves(pos, AI_OPTS);
  if (moves.length === 0)
    return inCheck(pos, pos.stm) ? -MATE + ply : 0; // mat (uzaklıkça) / pat=0

  order(pos, moves);
  let best = -INF;
  for (const m of moves) {
    const sc = -negamax(applyMove(pos, m), depth - 1, -beta, -alpha, ply + 1, deadline, info);
    if (sc > best) best = sc;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;                    // beta kesme
    if (info.timeout) break;
  }
  return best;
}

// Kök: yinelemeli derinleşme + zaman sınırı. En iyi hamleyi döndürür.
export function chooseMove(pos, { maxDepth = 4, timeMs = 800 } = {}) {
  const root = legalMoves(pos, AI_OPTS);
  if (root.length === 0) return null;
  if (root.length === 1) return root[0];

  const deadline = Date.now() + timeMs;
  let best = root[0], bestScore = -INF;

  for (let d = 1; d <= maxDepth; d++) {
    const info = { timeout: false };
    let localBest = null, localScore = -INF, alpha = -INF;
    // Önceki en iyi hamleyi başa al (PV) → daha iyi budama
    const ordered = order(pos, root.slice());
    const bi = ordered.indexOf(best);
    if (bi > 0) { ordered.splice(bi, 1); ordered.unshift(best); }

    for (const m of ordered) {
      const sc = -negamax(applyMove(pos, m), d - 1, -INF, -alpha, 1, deadline, info);
      if (sc > localScore) { localScore = sc; localBest = m; }
      if (localScore > alpha) alpha = localScore;
      if (info.timeout) break;
    }
    if (localBest && !info.timeout) { best = localBest; bestScore = localScore; }
    if (info.timeout || Date.now() > deadline) break;
    if (bestScore >= MATE - 1000) break;          // mat bulundu, derinleşme gereksiz
  }
  // sıralama için eklenen geçici alanı temizle
  delete best._s;
  return best;
}
