// ─────────────────────────────────────────────────────────────────────────────
//  Timurlenk Satrancı — UI katmanı (render + serbest sürükleme)
//  NOT: Bu sürümde hamle kuralı / sıra / şah-mat denetimi YOKTUR. Taşlar serbestçe
//  taşınır; dolu kareye bırakınca üstündeki yenir (değiştirilir). Hareket kuralları,
//  yasal hamle ve AI sonraki faza bırakıldı (model DOM'dan ayrık tutuldu).
//  Bağımlılıklar (aynı script kapsamında): PIECE_IMG, FILES, RANKS, mk,
//  WHITE_CITADEL, BLACK_CITADEL, setupBoard, serialize.
// ─────────────────────────────────────────────────────────────────────────────

let gBoard = null;
const cellMap = {};                 // sq -> hücre elemanı
const state = { board: null, selected: null };
let drag = null;

const letterOf = (f) => String.fromCharCode(97 + f); // 0..10 -> a..k

function buildBoard() {
  gBoard = document.getElementById('gBoard');
  gBoard.innerHTML = '';
  for (const k in cellMap) delete cellMap[k];

  // Rütbe etiketleri (sol sütun 1, 1–10) — ekran satırı = 10 - rank
  for (let r = 0; r < RANKS; r++) {
    const lab = document.createElement('div');
    lab.className = 'g-lab';
    lab.style.gridColumn = '1';
    lab.style.gridRow = String(10 - r);
    lab.textContent = String(r + 1);
    gBoard.appendChild(lab);
  }
  // Dosya etiketleri (alt satır 11, a–k)
  for (let f = 0; f < FILES; f++) {
    const lab = document.createElement('div');
    lab.className = 'g-lab';
    lab.style.gridColumn = String(f + 3);
    lab.style.gridRow = '11';
    lab.textContent = letterOf(f);
    gBoard.appendChild(lab);
  }
  // 110 normal kare (dosya f -> sütun f+3 ; rütbe r -> satır 10-r)
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
  // Citadel kareleri: beyaz = k2 sağı (sütun 14, rank1=satır9), siyah = a9 solu (sütun 2, rank8=satır2)
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

function render() {
  for (const sqKey in cellMap) {
    const sq = +sqKey;
    const cell = cellMap[sq];
    const p = state.board[sq];
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
  }
}

function cellSqFromPoint(x, y) {
  const el = document.elementFromPoint(x, y);
  const cell = el && el.closest('.g-cell');
  return cell ? +cell.dataset.sq : null;
}

function movePiece(from, to) {
  if (from === to || from == null || to == null) return;
  if (!state.board[from]) return;
  state.board[to] = state.board[from];
  state.board[from] = null;
}

function onDown(e) {
  const cell = e.target.closest('.g-cell');
  if (!cell) return;
  e.preventDefault();
  const sq = +cell.dataset.sq;
  try { gBoard.setPointerCapture(e.pointerId); } catch (_) {}
  drag = { fromSq: sq, hasPiece: !!state.board[sq], moved: false, x: e.clientX, y: e.clientY, ghost: null };
}

function onMove(e) {
  if (!drag || !drag.hasPiece) return;
  if (!drag.moved && Math.hypot(e.clientX - drag.x, e.clientY - drag.y) > 6) {
    drag.moved = true;
    const p = state.board[drag.fromSq];
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

  if (d.moved && d.hasPiece) {
    // sürükle-bırak
    const toSq = cellSqFromPoint(e.clientX, e.clientY);
    if (toSq != null) movePiece(d.fromSq, toSq);
    state.selected = null;
  } else {
    // dokunma (tap): tıkla-seç / tıkla-taşı
    if (d.hasPiece) {
      if (state.selected == null) state.selected = d.fromSq;
      else if (state.selected === d.fromSq) state.selected = null;
      else { movePiece(state.selected, d.fromSq); state.selected = null; }
    } else if (state.selected != null) {
      movePiece(state.selected, d.fromSq);
      state.selected = null;
    }
  }
  render();
}

function newGame() {
  state.board = setupBoard();
  state.selected = null;
  if (!gBoard) buildBoard();
  render();
}

function startGame() {
  const menu = document.getElementById('menu');
  const game = document.getElementById('game');
  if (menu) menu.classList.remove('vis');
  if (game) game.classList.add('vis');
  newGame();
}

function exitGame() {
  const menu = document.getElementById('menu');
  const game = document.getElementById('game');
  if (game) game.classList.remove('vis');
  if (menu) menu.classList.add('vis');
}

// HTML onclick / menü için global'e bağla
window.startGame = startGame;
window.exitGame = exitGame;
window.newGame = newGame;
window.__tc = { state, setupBoard, serialize }; // tarayıcı konsolundan teşhis

function tcInit() {
  const playBtn = document.querySelector('.pl-btn');
  if (playBtn) playBtn.addEventListener('click', startGame);
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', tcInit);
else tcInit();
