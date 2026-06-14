// ─────────────────────────────────────────────────────────────────────────────
//  Timurlenk Satrancı — UI katmanı (render + sıra tabanlı oyun + bilgisayar rakip)
//  Bağımlılıklar (aynı script kapsamında, build sırasıyla):
//    board.mjs  → FILES, RANKS, NORMAL, mk, fileOf, rankOf, WHITE_CITADEL, BLACK_CITADEL
//    engine.mjs → makePosition, applyMove, legalMoves, legalFrom, status, inCheck,
//                 findKing, isCapture, PROMO_OPTIONS
//    ai.mjs     → chooseMove
//    (gömme)    → PIECE_IMG  { harf: {w,b} }
//  Kurallar artık tam: yasal hamle zorunlu, sıra dönüşümlü, şah/mat/pat/terfi/
//  citadel/swap. İnsan bir renk, bilgisayar diğer renk.
// ─────────────────────────────────────────────────────────────────────────────

let gBoard = null;
const cellMap = {};                 // sq -> hücre elemanı
const state = {
  pos: null,            // motor pozisyonu
  humanColor: 'w',      // insanın rengi
  selected: null,       // seçili kare (sq) | null
  legal: [],            // seçili kareden yasal hamleler
  thinking: false,      // bilgisayar düşünüyor
  over: false,          // oyun bitti
  swapArmed: false,     // "yer değiştir" modu açık
  lastMove: null,       // {from,to} — son hamle vurgusu
  pendingPromo: null,   // {from,to} — terfi seçimi bekleniyor
};
let drag = null;

const letterOf = (f) => String.fromCharCode(97 + f);
const TR = { K:'Şah', R:'Kale', Z:'Zürafa', T:'Talia', N:'At', C:'Deve',
             E:'Fil', W:'Savaş Mak.', F:'Ferz', V:'Vali', P:'Piyon' };
const AI = { maxDepth: 4, timeMs: 800 };

const $ = (id) => document.getElementById(id);
const aiColor = () => (state.humanColor === 'w' ? 'b' : 'w');
const humanTurn = () => state.pos && state.pos.stm === state.humanColor && !state.over && !state.thinking;

// ── Tahta kurulumu (112 hücre + etiketler + 2 citadel) ──────────────────────
function buildBoard() {
  gBoard = $('gBoard');
  gBoard.innerHTML = '';
  for (const k in cellMap) delete cellMap[k];

  for (let r = 0; r < RANKS; r++) {
    const lab = document.createElement('div');
    lab.className = 'g-lab';
    lab.style.gridColumn = '1';
    lab.style.gridRow = String(10 - r);
    lab.textContent = String(r + 1);
    gBoard.appendChild(lab);
  }
  for (let f = 0; f < FILES; f++) {
    const lab = document.createElement('div');
    lab.className = 'g-lab';
    lab.style.gridColumn = String(f + 3);
    lab.style.gridRow = '11';
    lab.textContent = letterOf(f);
    gBoard.appendChild(lab);
  }
  for (let r = 0; r < RANKS; r++) {
    for (let f = 0; f < FILES; f++) {
      const sq = mk(f, r);
      const cell = document.createElement('div');
      cell.className = 'g-cell ' + (((f + r) % 2 === 0) ? 'dark' : 'light');
      cell.dataset.sq = String(sq);
      cell.style.gridColumn = String(f + 3);
      cell.style.gridRow = String(10 - r);
      gBoard.appendChild(cell);
      cellMap[sq] = cell;
    }
  }
  const wc = document.createElement('div');
  wc.className = 'g-cell g-citadel';
  wc.dataset.sq = String(WHITE_CITADEL);
  wc.style.gridColumn = '14'; wc.style.gridRow = '9';
  wc.title = 'Beyaz Citadel';
  gBoard.appendChild(wc); cellMap[WHITE_CITADEL] = wc;

  const bc = document.createElement('div');
  bc.className = 'g-cell g-citadel';
  bc.dataset.sq = String(BLACK_CITADEL);
  bc.style.gridColumn = '2'; bc.style.gridRow = '2';
  bc.title = 'Siyah Citadel';
  gBoard.appendChild(bc); cellMap[BLACK_CITADEL] = bc;

  gBoard.addEventListener('pointerdown', onDown);
  gBoard.addEventListener('pointermove', onMove);
  gBoard.addEventListener('pointerup', onUp);
  gBoard.addEventListener('pointercancel', onUp);
}

// ── Render: taşlar + vurgular + durum satırı ────────────────────────────────
function render() {
  const b = state.pos.board;
  const targetSet = {};      // sq -> 'move' | 'cap'
  for (const m of state.legal) targetSet[m.to] = isCapture(state.pos, m) ? 'cap' : 'move';

  const swapCands = {};
  if (state.swapArmed) {
    const ksq = findKing(b, state.humanColor);
    for (let s = 0; s < NORMAL; s++) {
      const p = b[s];
      if (p && p.c === state.humanColor && s !== ksq) swapCands[s] = true;
    }
  }

  let chkSq = -1;
  if (!state.over && inCheck(state.pos, state.pos.stm)) chkSq = findKing(b, state.pos.stm);

  for (const sqKey in cellMap) {
    const sq = +sqKey;
    const cell = cellMap[sq];
    const p = b[sq];
    let img = cell.querySelector('img.pc');
    if (p) {
      if (!img) {
        img = document.createElement('img');
        img.className = 'pc'; img.draggable = false;
        cell.appendChild(img);
      }
      const src = (PIECE_IMG[p.t] || {})[p.c] || '';
      if (img.getAttribute('src') !== src) img.setAttribute('src', src);
      img.alt = p.t;
      img.style.visibility = '';
    } else if (img) {
      img.remove();
    }
    cell.classList.toggle('sel', state.selected === sq);
    cell.classList.toggle('tgt', targetSet[sq] === 'move');
    cell.classList.toggle('cap', targetSet[sq] === 'cap');
    cell.classList.toggle('swapcand', !!swapCands[sq]);
    cell.classList.toggle('chk', sq === chkSq);
    cell.classList.toggle('last', !!state.lastMove &&
      (sq === state.lastMove.from || sq === state.lastMove.to));
  }
  renderStatus();
  syncControls();
}

function renderStatus() {
  const el = $('gStatus');
  if (!el) return;
  let msg;
  if (state.over) {
    const st = status(state.pos);
    if (st.result === 'checkmate')
      msg = (st.winner === state.humanColor ? '🏆 Mat! Kazandın.' : 'Mat — bilgisayar kazandı.');
    else if (st.result === 'stalemate') msg = 'Pat — berabere.';
    else if (st.result === 'draw') msg = 'Beraberlik (citadel).';
    else msg = 'Oyun bitti.';
  } else if (state.thinking) {
    msg = 'Bilgisayar düşünüyor…';
  } else if (state.swapArmed) {
    msg = 'Yer değiştir: şahla takas edeceğin taşı seç.';
  } else {
    const turn = state.pos.stm === state.humanColor ? 'Sıra sende' : 'Sıra bilgisayarda';
    const chk = inCheck(state.pos, state.pos.stm) ? ' — ŞAH!' : '';
    msg = turn + chk;
  }
  el.textContent = msg;
}

// Kontrolleri (swap düğmesi, renk seçimi) durumla eşitle
function syncControls() {
  const sw = $('gSwap');
  if (sw) {
    const can = humanTurn() && !state.pos.swapUsed[state.humanColor]
      && !inCheck(state.pos, state.humanColor);
    sw.disabled = !can;
    sw.classList.toggle('armed', state.swapArmed);
    sw.textContent = state.pos.swapUsed[state.humanColor] ? 'Yer Değiştir (kullanıldı)' : 'Yer Değiştir';
  }
  const sel = $('gSide');
  if (sel) sel.value = state.humanColor;
}

// ── Etkileşim ────────────────────────────────────────────────────────────────
function cellSqFromPoint(x, y) {
  const el = document.elementFromPoint(x, y);
  const cell = el && el.closest('.g-cell');
  return cell ? +cell.dataset.sq : null;
}

function selectSquare(sq) {
  state.selected = sq;
  state.legal = legalFrom(state.pos, sq, { includeSwap: false, promoAll: true });
}
function clearSelection() { state.selected = null; state.legal = []; }

// from→to için yasal hamle(ler); terfi ise seçim açar, değilse uygular
function tryMoveTo(toSq) {
  const matches = state.legal.filter((m) => m.to === toSq);
  if (matches.length === 0) { clearSelection(); render(); return false; }
  if (matches[0].type === 'promo') { openPromo(state.selected, toSq); return true; }
  doMove(matches[0]);
  return true;
}

function doMove(move) {
  state.pos = applyMove(state.pos, move);
  state.lastMove = { from: move.from, to: move.to };
  clearSelection();
  state.swapArmed = false;
  state.pendingPromo = null;
  checkEnd();
  render();
  if (!state.over && state.pos.stm === aiColor()) scheduleAI();
}

function checkEnd() {
  const st = status(state.pos);
  state.over = st.over;
  if (st.over) showOver();
  return st.over;
}

function scheduleAI() {
  state.thinking = true;
  renderStatus();
  // UI'nin "düşünüyor" durumunu çizebilmesi için bir tık geciktir
  setTimeout(() => {
    // Normalde swap'sız arar; tek yasal hamle swap ise ona düş (kilitlenmeyi önle)
    let m = chooseMove(state.pos, AI);
    if (!m) { const all = legalMoves(state.pos, { includeSwap: true }); m = all[0] || null; }
    state.thinking = false;
    if (!m) { checkEnd(); render(); return; }
    state.pos = applyMove(state.pos, m);
    state.lastMove = { from: m.from, to: m.to };
    checkEnd();
    render();
  }, 40);
}

function onDown(e) {
  if (!humanTurn()) return;
  const cell = e.target.closest('.g-cell');
  if (!cell) return;
  e.preventDefault();
  const sq = +cell.dataset.sq;
  const p = state.pos.board[sq];

  // Swap modu: kendi (şah olmayan) taşına dokun → takas
  if (state.swapArmed) {
    if (p && p.c === state.humanColor && p.t !== 'K') {
      const ksq = findKing(state.pos.board, state.humanColor);
      const sw = legalMoves(state.pos, { includeSwap: true })
        .find((m) => m.type === 'swap' && m.from === ksq && m.to === sq);
      if (sw) { doMove(sw); return; }
    }
    state.swapArmed = false; render(); return;
  }

  try { gBoard.setPointerCapture(e.pointerId); } catch (_) {}

  // Seçiliyken yasal hedefe dokunma → taşı (tap-to-move)
  if (state.selected != null && state.selected !== sq) {
    if (state.legal.some((m) => m.to === sq)) { tryMoveTo(sq); return; }
  }
  // Kendi taşını seç
  if (p && p.c === state.humanColor) {
    selectSquare(sq);
    drag = { fromSq: sq, moved: false, x: e.clientX, y: e.clientY, ghost: null };
    render();
  } else {
    clearSelection(); render();
  }
}

function onMove(e) {
  if (!drag) return;
  if (!drag.moved && Math.hypot(e.clientX - drag.x, e.clientY - drag.y) > 6) {
    drag.moved = true;
    const p = state.pos.board[drag.fromSq];
    if (!p) return;
    const ghost = document.createElement('img');
    ghost.className = 'pc-ghost';
    ghost.src = (PIECE_IMG[p.t] || {})[p.c] || '';
    ghost.draggable = false;
    document.body.appendChild(ghost);
    const rect = cellMap[drag.fromSq].getBoundingClientRect();
    ghost.style.width = rect.width + 'px';
    ghost.style.height = rect.height + 'px';
    drag.ghost = ghost;
    const oi = cellMap[drag.fromSq].querySelector('img.pc');
    if (oi) oi.style.visibility = 'hidden';
  }
  if (drag.moved && drag.ghost) {
    drag.ghost.style.left = e.clientX + 'px';
    drag.ghost.style.top = e.clientY + 'px';
  }
}

function onUp(e) {
  if (!drag) return;
  const d = drag; drag = null;
  if (d.ghost) d.ghost.remove();
  const fromCell = cellMap[d.fromSq];
  const oi = fromCell && fromCell.querySelector('img.pc');
  if (oi) oi.style.visibility = '';
  if (d.moved) {
    const toSq = cellSqFromPoint(e.clientX, e.clientY);
    if (toSq != null && state.selected === d.fromSq) tryMoveTo(toSq);
    else render();
  }
  // tap (moved=false): seçim onDown'da yapıldı; ek iş yok
}

// ── Terfi seçimi ─────────────────────────────────────────────────────────────
function openPromo(from, to) {
  state.pendingPromo = { from, to };
  const row = $('gPromoRow');
  const overlay = $('gPromo');
  if (!row || !overlay) { // DOM yoksa (test) varsayılan Kale
    doMove({ from, to, type: 'promo', promo: 'R' }); return;
  }
  row.innerHTML = '';
  for (const pt of PROMO_OPTIONS) {
    const btn = document.createElement('button');
    btn.className = 'g-promo-item';
    btn.title = TR[pt];
    const img = document.createElement('img');
    img.src = (PIECE_IMG[pt] || {})[state.humanColor] || '';
    img.alt = pt; img.draggable = false;
    btn.appendChild(img);
    btn.addEventListener('click', () => choosePromo(pt));
    row.appendChild(btn);
  }
  overlay.classList.add('vis');
}
function choosePromo(pt) {
  const overlay = $('gPromo');
  if (overlay) overlay.classList.remove('vis');
  const pp = state.pendingPromo;
  if (!pp) return;
  doMove({ from: pp.from, to: pp.to, type: 'promo', promo: pt });
}

// ── Oyun sonu afişi ──────────────────────────────────────────────────────────
function showOver() {
  const overlay = $('gOver'); const msg = $('gOverMsg');
  if (!overlay || !msg) return;
  const st = status(state.pos);
  if (st.result === 'checkmate')
    msg.textContent = st.winner === state.humanColor ? 'Mat! Kazandın 🏆' : 'Mat — bilgisayar kazandı.';
  else if (st.result === 'stalemate') msg.textContent = 'Pat — berabere.';
  else if (st.result === 'draw') msg.textContent = 'Beraberlik (citadel).';
  else msg.textContent = 'Oyun bitti.';
  overlay.classList.add('vis');
}

// ── Kontrol fonksiyonları (HTML onclick) ────────────────────────────────────
function toggleSwap() {
  if (!humanTurn()) return;
  if (state.pos.swapUsed[state.humanColor] || inCheck(state.pos, state.humanColor)) return;
  state.swapArmed = !state.swapArmed;
  clearSelection();
  render();
}

function setSide(color) {
  if (color !== 'w' && color !== 'b') return;
  state.humanColor = color;
  newGame();
}

function newGame() {
  state.pos = makePosition();
  state.selected = null; state.legal = [];
  state.thinking = false; state.over = false;
  state.swapArmed = false; state.lastMove = null; state.pendingPromo = null;
  const ov = $('gOver'); if (ov) ov.classList.remove('vis');
  const pr = $('gPromo'); if (pr) pr.classList.remove('vis');
  if (!gBoard) buildBoard();
  render();
  if (state.pos.stm === aiColor()) scheduleAI(); // insan siyahsa bilgisayar (beyaz) başlar
}

function startGame() {
  const menu = $('menu');
  const game = $('game');
  if (menu) menu.classList.remove('vis');
  if (game) game.classList.add('vis');
  newGame();
}

function exitGame() {
  const menu = $('menu');
  const game = $('game');
  if (game) game.classList.remove('vis');
  if (menu) menu.classList.add('vis');
}

// HTML onclick / menü için global'e bağla
window.startGame = startGame;
window.exitGame = exitGame;
window.newGame = newGame;
window.setSide = setSide;
window.toggleSwap = toggleSwap;
window.choosePromo = choosePromo;
window.__tc = { state, makePosition, applyMove, legalMoves, status }; // konsol teşhis

function tcInit() {
  const playBtn = document.querySelector('.pl-btn');
  if (playBtn) playBtn.addEventListener('click', startGame);
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', tcInit);
else tcInit();
