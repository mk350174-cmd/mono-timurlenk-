// ─────────────────────────────────────────────────────────────────────────────
//  Apex Timurlenk — Monolitik Kaynak Dosyası  (v0.7.0)
//  Tüm başlık ve kaynak dosyaları tek bir translation unit'te birleştirildi.
// ─────────────────────────────────────────────────────────────────────────────
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ════════ include/types.h ════════


// ─────────────────────────────────────────────────────────────────────────────
//  APEX Timurlenk Motoru — types.h
//  Temel veri tipleri: kare, taş, renk, hamle
//
//  Tahta düzeni:
//    Normal oyun alanı: 11 sütun × 10 satır = 110 kare (indeks 0–109)
//    Beyaz citadel:     indeks 110  (tahta koordinatında: sütun 11, satır 1)
//    Siyah citadel:     indeks 111  (tahta koordinatında: sütun -1, satır 8)
//    BOARD_SIZE = 112
//
//  Koordinat sistemi:
//    file (sütun): 0–10  → a–k
//    rank (satır): 0–9   → 1–10
//    sq = rank * 11 + file   (normal kareler için)
//    sq = 110                (beyaz citadel: k2 sağında)
//    sq = 111                (siyah citadel: a9 solunda)
// ─────────────────────────────────────────────────────────────────────────────

namespace Apex {

// ── Temel tam sayı tipleri ────────────────────────────────────────────────────
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// ── Kare (Square) ────────────────────────────────────────────────────────────
// 0–109: normal kareler, 110: beyaz citadel, 111: siyah citadel, 112: geçersiz
using Square = u8;

constexpr int  BOARD_FILES = 11;
constexpr int  BOARD_RANKS = 10;
constexpr int  BOARD_NORMAL = BOARD_FILES * BOARD_RANKS; // 110
constexpr int  BOARD_SIZE   = 112;                       // 110 + 2 citadel
constexpr Square SQ_NONE    = 112;

// Citadel kareleri
constexpr Square WHITE_CITADEL = 110;  // Beyazın rakip kalesine gireceği kare
constexpr Square BLACK_CITADEL = 111;  // Siyahın rakip kalesine gireceği kare

// Citadel'in tahta komşuları (hangi normal kareye bitişik)
// Beyaz citadel: satır 1 (rank=1), sütun 10'un (k) sağında → k2 yanı
// Siyah citadel: satır 8 (rank=8), sütun 0'ın (a) solunda → a9 yanı
constexpr Square WHITE_CITADEL_NEIGHBOR = 1 * BOARD_FILES + 10; // k2 = rank1*11+file10 = 21
constexpr Square BLACK_CITADEL_NEIGHBOR = 8 * BOARD_FILES + 0;  // a9 = rank8*11+file0  = 88

inline Square make_sq(int file, int rank) {
    assert(file >= 0 && file < BOARD_FILES);
    assert(rank >= 0 && rank < BOARD_RANKS);
    return static_cast<Square>(rank * BOARD_FILES + file);
}
inline int sq_file(Square sq) { return sq % BOARD_FILES; }
inline int sq_rank(Square sq) { return sq / BOARD_FILES; }
inline bool sq_valid(Square sq) { return sq < BOARD_NORMAL; }
inline bool sq_is_citadel(Square sq) { return sq == WHITE_CITADEL || sq == BLACK_CITADEL; }

// ── Renk (Color) ─────────────────────────────────────────────────────────────
enum Color : u8 { WHITE = 0, BLACK = 1, NO_COLOR = 2 };
inline Color operator~(Color c) { return static_cast<Color>(c ^ 1); }

// ── Taş Tipi (PieceType) ──────────────────────────────────────────────────────
// Timurlenk'e özgü 11 taş tipi (tarihsel Türkçe isimler)
enum PieceType : u8 {
    NO_PIECE_TYPE = 0,
    SAH    = 1,   // K — Şah          : 8 yönde 1 kare + swap hakkı
    KALE   = 2,   // R — Kale         : yatay/dikey sınırsız kayma
    ZURAFA = 3,   // Z — Zürafa       : çapraz 1 + min 3 düz (gryphon)
    TALIA  = 4,   // T — Talia/Gözcü  : yatay/dikey tam 2 sıçrama
    AT     = 5,   // N — At           : L şekli 2+1 atlama
    DEVE   = 6,   // C — Deve         : 3+1 atlama (extended knight)
    FIL    = 7,   // E — Fil          : çapraz tam 2 sıçrama
    SAVAS  = 8,   // W — Savaş Makinesi: yatay/dikey tam 2 sıçrama
    FERZ   = 9,   // F — Ferz         : çapraz 1 kare
    VALI   = 10,  // V — Vali/Wazir   : yatay/dikey 1 kare
    PIYON  = 11,  // P — Piyon        : ileri 1, çift adım yok, çapraz yer
    PIECE_TYPE_NB = 12
};

// ── Taş (Piece) ───────────────────────────────────────────────────────────────
// Renk + tip birleşimi: bit 0 = renk, bit 1-4 = tip
// Değer aralığı: 0–23 (NO_PIECE=0)
enum Piece : u8 {
    NO_PIECE     = 0,
    W_SAH        = (SAH    << 1) | WHITE,  //  2
    W_KALE       = (KALE   << 1) | WHITE,  //  4
    W_ZURAFA     = (ZURAFA << 1) | WHITE,  //  6
    W_TALIA      = (TALIA  << 1) | WHITE,  //  8
    W_AT         = (AT     << 1) | WHITE,  // 10
    W_DEVE       = (DEVE   << 1) | WHITE,  // 12
    W_FIL        = (FIL    << 1) | WHITE,  // 14
    W_SAVAS      = (SAVAS  << 1) | WHITE,  // 16
    W_FERZ       = (FERZ   << 1) | WHITE,  // 18
    W_VALI       = (VALI   << 1) | WHITE,  // 20
    W_PIYON      = (PIYON  << 1) | WHITE,  // 22
    B_SAH        = (SAH    << 1) | BLACK,  //  3
    B_KALE       = (KALE   << 1) | BLACK,  //  5
    B_ZURAFA     = (ZURAFA << 1) | BLACK,  //  7
    B_TALIA      = (TALIA  << 1) | BLACK,  //  9
    B_AT         = (AT     << 1) | BLACK,  // 11
    B_DEVE       = (DEVE   << 1) | BLACK,  // 13
    B_FIL        = (FIL    << 1) | BLACK,  // 15
    B_SAVAS      = (SAVAS  << 1) | BLACK,  // 17
    B_FERZ       = (FERZ   << 1) | BLACK,  // 19
    B_VALI       = (VALI   << 1) | BLACK,  // 21
    B_PIYON      = (PIYON  << 1) | BLACK,  // 23
    PIECE_NB     = 24
};

inline Piece make_piece(Color c, PieceType pt) {
    return static_cast<Piece>((pt << 1) | c);
}
inline PieceType piece_type(Piece p) { return static_cast<PieceType>(p >> 1); }
inline Color     piece_color(Piece p){ return static_cast<Color>(p & 1); }

// ── Materyal Değerleri ────────────────────────────────────────────────────────
// Centipawn cinsinden (cp). Tarihsel ve pratik denge gözetilerek ayarlandı.
constexpr i32 PIECE_VALUE[PIECE_TYPE_NB] = {
    0,        // NO_PIECE_TYPE
    20000,    // SAH    — sonsuz değer (mat)
    550,      // KALE   — uzun mesafe kayma, çok güçlü 11x10'da
    480,      // ZURAFA — gryphon: çapraz+düz kombinasyonu, çok mobil
    340,      // TALIA  — 2'li sıçrama, atlama özelliği ile barikat kırıcı
    325,      // AT     — klasik at hareketi, renk bağımlı
    330,      // DEVE   — 3+1 atlama, renk bağımsız → 11x10'da At'tan biraz değerli
    290,      // FIL    — 2'li çapraz sıçrama, renk bağımlı
    260,      // SAVAS  — 2'li düz sıçrama, kontrol alanı sınırlı
    160,      // FERZ   — sadece çapraz 1, zayıf ama stratejik
    130,      // VALI   — sadece dik 1, en zayıf süvari
    100,      // PIYON  — temel birim
};

// ── Hamle (Move) ──────────────────────────────────────────────────────────────
// 16-bit kodlama:
//   bit  0– 6: kaynak kare    (0–112, 7 bit)
//   bit  7–13: hedef kare     (0–112, 7 bit)
//   bit 14–15: hamle tipi (0=normal, 1=terfi, 2=swap, 3=citadel-giriş)
// Terfi hamlesi için ek bilgi ayrı tutulur (MoveExt).
using Move = u16;
constexpr Move MOVE_NONE  = 0;
constexpr Move MOVE_NULL  = 0xFFFF;

enum MoveType : u8 {
    MT_NORMAL  = 0,
    MT_PROMO   = 1,  // Piyon terfisi
    MT_SWAP    = 2,  // Şah yer değiştirme hakkı (Timurlenk özgün kuralı)
    MT_CITADEL = 3,  // Şahın rakip kalesine girişi (beraberlik)
};

inline Move  make_move(Square from, Square to, MoveType mt = MT_NORMAL) {
    return static_cast<Move>((mt << 14) | (to << 7) | from);
}
inline Square    move_from(Move m) { return static_cast<Square>(m & 0x7F); }
inline Square    move_to  (Move m) { return static_cast<Square>((m >> 7) & 0x7F); }
inline MoveType  move_type(Move m) { return static_cast<MoveType>(m >> 14); }
inline bool      move_ok  (Move m) { return m != MOVE_NONE && m != MOVE_NULL; }

// Terfi hamlesi için genişletilmiş bilgi
struct MoveExt {
    Move      move;
    PieceType promo;    // MT_PROMO ise hangi taşa terfi
    i32       score = 0; // Sıralama skoru (score_moves tarafından doldurulur)
    MoveExt() : move(MOVE_NONE), promo(NO_PIECE_TYPE), score(0) {}
    MoveExt(Move m, PieceType p = NO_PIECE_TYPE) : move(m), promo(p), score(0) {}
};

// ── Değerlendirme skoru ───────────────────────────────────────────────────────
constexpr i32 SCORE_INFINITE =  1'000'000;
constexpr i32 SCORE_NONE     = -1'000'001;
constexpr i32 SCORE_MATE     =    900'000;
constexpr i32 SCORE_DRAW     =          0;

inline bool is_mate_score(i32 s) { return std::abs(s) > SCORE_MATE - 500; }
inline int  mate_in_plies(i32 s) {
    return s > 0 ? (SCORE_MATE - s + 1) / 2 : -(SCORE_MATE + s) / 2;
}

// ── Taş isimleri (debug/UCI için) ────────────────────────────────────────────
constexpr const char* PIECE_TYPE_STR[PIECE_TYPE_NB] = {
    ".", "K", "R", "Z", "T", "N", "C", "E", "W", "F", "V", "P"
};
constexpr const char* PIECE_TYPE_TR[PIECE_TYPE_NB] = {
    "-", "Şah", "Kale", "Zürafa", "Talia", "At", "Deve",
    "Fil", "Savaş Makinesi", "Ferz", "Vali", "Piyon"
};

} // namespace Apex

// ════════ include/tt.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  Zobrist Hashing
//  Her (kare, taş) kombinasyonu için rastgele 64-bit anahtar.
//  Artımlı güncelleme: hamle yapıldığında XOR ile çıkar/ekle.
// ─────────────────────────────────────────────────────────────────────────────
struct ZobristTable {
    // [kare][taş tipi][renk]
    u64 piece [BOARD_SIZE][PIECE_TYPE_NB][2];
    u64 side;               // Siyah'ın sırası olduğunda XOR'la
    u64 swap_used[2];       // Yer değiştirme hakkı kullanıldığında
    u64 citadel_draw;       // Citadel beraberliği hash'i (özel)

    void init(u64 seed = 0xDEADBEEF12345678ULL) {
        // xorshift64 ile deterministik üretim (her çalışmada aynı)
        auto rng = [&]() -> u64 {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            return seed;
        };
        for (int sq = 0; sq < BOARD_SIZE; sq++)
            for (int pt = 0; pt < PIECE_TYPE_NB; pt++)
                for (int c = 0; c < 2; c++)
                    piece[sq][pt][c] = rng();
        side            = rng();
        swap_used[0]    = rng();
        swap_used[1]    = rng();
        citadel_draw    = rng();
    }
};

extern ZobristTable Zobrist; // tt.cpp'de tanımlanır

// ─────────────────────────────────────────────────────────────────────────────
//  Transposition Table (TT)
//  Daha önce analiz edilen pozisyonları saklayan hash tablosu.
//  Her slot 16 byte → önbellek dostu.
// ─────────────────────────────────────────────────────────────────────────────

enum TTFlag : u8 {
    TT_NONE  = 0,
    TT_EXACT = 1,  // Kesin skor
    TT_LOWER = 2,  // Alpha kesimi (alt sınır)
    TT_UPPER = 3,  // Beta kesimi (üst sınır)
};

// TT girişi — 16 byte
struct alignas(16) TTEntry {
    u32  key16;    // hash'in üst 32 bit'i (doğrulama)
    i16  score;    // pozisyon skoru (cp)
    i16  eval;     // statik eval (quiescence için)
    Move move;     // en iyi hamle
    u8   depth;    // arama derinliği
    u8   flag;     // TTFlag
    u8   age;      // TT yaşı (ne zaman yazıldı)
    u8   padding;

    void clear() {
        key16 = 0; score = 0; eval = 0;
        move  = MOVE_NONE; depth = 0;
        flag  = TT_NONE;   age   = 0;
    }
    bool valid() const { return flag != TT_NONE; }
};

static_assert(sizeof(TTEntry) == 16, "TTEntry 16 byte olmali");

// TT boyutu: 2^N slot. Varsayılan: 2^22 = 4M slot × 16 byte = 64 MB
constexpr int TT_DEFAULT_MB = 64;

class TranspositionTable {
public:
    TranspositionTable();
    ~TranspositionTable();

    // MB cinsinden boyutu ayarla (UCI setengineoption)
    void resize(int mb);

    // Tabloyu temizle (yeni oyun başladığında)
    void clear();

    // Yaşı artır (her arama turunda)
    void new_search() { age_++; }

    // Pozisyon yaz
    void store(u64 hash, int depth, TTFlag flag, i32 score, i32 eval,
               Move best_move, int ply = 0);

    // Pozisyon oku — nullptr döndürürse hit yok
    const TTEntry* probe(u64 hash) const;

    // Doluluk oranı (promil — UCI için)
    int hashfull() const;

private:
    TTEntry* table_;
    size_t   size_;    // slot sayısı
    u8       age_;

    size_t index(u64 hash) const { return hash & (size_ - 1); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Pawn Table (piyon yapısı önbelleği)
//  Piyonlar sık değişmez → ayrı önbellek avantaj sağlar
// ─────────────────────────────────────────────────────────────────────────────
struct PawnEntry {
    u64 key;
    i32 score[2]; // [WHITE][BLACK] piyon yapısı skoru
};

constexpr size_t PAWN_TABLE_SIZE = 1 << 14; // 16K giriş

class PawnTable {
public:
    PawnTable() { clear(); }
    void clear() { entries_.fill({}); }

    PawnEntry* probe(u64 key) {
        auto& e = entries_[key & (PAWN_TABLE_SIZE - 1)];
        return (e.key == key) ? &e : nullptr;
    }
    void store(u64 key, i32 w, i32 b) {
        auto& e = entries_[key & (PAWN_TABLE_SIZE - 1)];
        e.key = key; e.score[WHITE] = w; e.score[BLACK] = b;
    }
private:
    std::array<PawnEntry, PAWN_TABLE_SIZE> entries_;
};

} // namespace Apex

// ════════ include/nnue.h ════════

/**
 * nnue.h — Apex Timurlenk NNUE Değerlendirme Ağı
 *
 * Mimari: HalfKA benzeri, Timurlenk'e özgü
 *   Giriş : 2 × (PIECE_TYPES × BOARD_SQ) = 2 × 1320 = 2640 binary özellik
 *           (her iki kral kare perspektifinden ayrı birikimleyici)
 *   L1    : 256 nöron, clamp(0,127) aktivasyon (quantize)
 *   L2    : 32 nöron, ReLU
 *   Çıkış : 1 değer → cp birimine dönüştür (× SCALE / Q_SCALE)
 *
 * Ağırlık dosyası: networks/apex_v1.bin
 *   Format: "APEX" (4B) + version(4B) + W1 + b1 + W2 + b2 + W3 + b3
 *   Tüm ağırlıklar int16 (quantize), biyaslar int32
 */


namespace Apex {

class Board; // ileri bildirim — board.h döngüsel bağımlılık önler

// ─── Ağ boyutları ────────────────────────────────────────────────────────────
// Giriş özelliği: (parça tipi - piyon değil SAH dahil) × kare
// 11 parça tipi (SAH=1..PIYON=11) × 110 kare = 1210 per renk → 2420 toplam
// Ama HalfKA: (parça tipi × kare) için her renk perspektifinden ayrı
// Basit versiyon: her renk için [ptype-1][sq] binary özelliği

static constexpr int NNUE_PIECE_TYPES = 11; // SAH..PIYON
static constexpr int NNUE_SQUARES     = BOARD_NORMAL; // 110
static constexpr int NNUE_INPUT       = NNUE_PIECE_TYPES * NNUE_SQUARES; // 1210 per side
static constexpr int NNUE_L1          = 256;
static constexpr int NNUE_L2          = 32;
static constexpr int NNUE_OUTPUT      = 1;

// Quantization sabitleri
static constexpr int Q_IN   = 127;  // giriş scale
static constexpr int Q_W1   = 64;   // L1 ağırlık scale
static constexpr int Q_W2   = 64;   // L2 ağırlık scale
static constexpr int NNUE_SCALE = 400; // cp çıkış ölçeği

// ─── Akümülatör ──────────────────────────────────────────────────────────────
// Her iki taraf için ayrı ayrı tutulur, taş ekleme/çıkarma sırasında güncellenir

struct Accumulator {
    std::array<int32_t, NNUE_L1> white{};
    std::array<int32_t, NNUE_L1> black{};

    void reset(const std::array<int32_t, NNUE_L1>& bias) {
        white = bias;
        black = bias;
    }
};

// ─── NNUE Ağı ────────────────────────────────────────────────────────────────

class NNUENetwork {
public:
    NNUENetwork();

    // Ağırlık dosyasından yükle — başarılıysa true döner
    bool load(const std::string& path);

    // Ağ yüklü mü?
    bool loaded() const { return loaded_; }

    // Tam (kaba kuvvet) değerlendirme — akümülatör olmadan
    // board: tüm taş bilgisi
    // stm: sıradaki renk (+1 → stm avantajı)
    i32 evaluate(const Board& b, Color stm) const;

    // Özellik indeksi hesapla: [color perspective][piece_type][square]
    static int feature_index(Color perspective, Color piece_color,
                             PieceType pt, Square sq);

private:
    bool loaded_ = false;

    // L1: [NNUE_INPUT × NNUE_L1] — her özelliğin L1 nöronlarına katkısı
    // Her taraf kendi perspektifinden bakarak aynı L1 ağırlıklarını kullanır
    std::array<int16_t, NNUE_INPUT * NNUE_L1> W1_{};
    std::array<int32_t, NNUE_L1>              b1_{};

    // L2: [NNUE_L1 * 2 × NNUE_L2] — iki birikimleyici birleştirilir
    std::array<int16_t, NNUE_L1 * 2 * NNUE_L2> W2_{};
    std::array<int32_t, NNUE_L2>                b2_{};

    // L3: [NNUE_L2 × NNUE_OUTPUT]
    std::array<int16_t, NNUE_L2 * NNUE_OUTPUT> W3_{};
    std::array<int32_t, NNUE_OUTPUT>            b3_{};

    // İleri besleme (quantize)
    i32 forward(const Accumulator& acc, Color stm) const;
};

} // namespace Apex

// ════════ include/persona.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  PersonaLayer — L1.5 Katman
//
//  Apex v2.0 Bilişsel Mimari'nin temel persona altyapısı.
//  ML altyapısı gerektirmeden saf C++ ile arama parametrelerini etkiler:
//
//    Machiavelli (K1–K10): Ragione di Stato — pragmatik, dar aspiration,
//                          Necessità modunda agresif zaman yönetimi.
//    Nietzsche             : Güç İstenci — geniş aspiration, Vezüv protokolü
//                          ile materyal feda uzantıları.
//
//  Psyche skaleri (ψt ∈ [-100, +100]):
//    ψ < -60  → Necessità modu (K36): dar pencere, acil arama
//    ψ > +60 (Nietzsche) → Vezüv Protokolü: geniş pencere, feda uzantısı
// ─────────────────────────────────────────────────────────────────────────────

enum class PersonaType : u8 {
    NONE        = 0,
    MACHIAVELLI = 1,
    NIETZSCHE   = 2,
    // Türk-Altay mitolojisi — v0.4.0-mythology
    ULGEN       = 3,  // Yaratıcı Tanrı: pozisyonel, sabırlı, uzun vadeli
    ERLIK       = 4,  // Yeraltı Hükümdarı: agresif, taktik, feda odaklı
    BOZKURT     = 5,  // Göktürk Totemi: dinamik, reaktif, tempo avcısı
    TENGRI      = 6,  // Gök Tanrı: saf hesap, maksimum derinlik
    DEDE_KORKUT = 7,  // Bilge Ozan: açılış odaklı, tecrübe bazlı
    UMAY        = 8,  // Ana Tanrıça: savunmacı, şah güvenliği öncelikli
};

struct PersonaLayer {
    PersonaType type   = PersonaType::NONE;
    int         psyche = 0;  // ψt ∈ [-100, +100]

    void set_persona(PersonaType t) { type = t; }
    void set_psyche(int psi)        { psyche = std::clamp(psi, -100, 100); }

    // +ψ → daha saldırgan contempt (beraberlik reddi)
    int contempt() const;

    // Aspiration window başlangıç genişliği
    // Vezüv → ×3, Necessità → ÷2, diğer → base + |ψ|/5
    int aspiration_delta() const;

    // K36 — Necessità: pozisyon kötüyse (ψ < -60) acil mod
    bool necessita_mode() const { return psyche < -60; }

    // Nietzsche Vezüv Protokolü: materyal feda fırsatlarında +1 ply uzatma
    bool vezuv_protocol() const {
        return type == PersonaType::NIETZSCHE && psyche > 60;
    }

    // Erlik Karanlık Derinlik: ψ > +60'ta mat araması modu
    bool erlik_dark_depth() const {
        return type == PersonaType::ERLIK && psyche > 60;
    }

    // Umay Koruyucu Kanat: şah güvenliği düşükse tam savunma modu
    bool umay_shield() const {
        return type == PersonaType::UMAY && psyche < -30;
    }

    // UCI info string yorumu — kısa, felsefi, Türkçe
    std::string commentary(int depth, i32 score) const;

    // Persona adı (UCI combo için)
    const char* name() const {
        switch (type) {
            case PersonaType::MACHIAVELLI: return "Machiavelli";
            case PersonaType::NIETZSCHE:   return "Nietzsche";
            case PersonaType::ULGEN:       return "Ulgen";
            case PersonaType::ERLIK:       return "Erlik";
            case PersonaType::BOZKURT:     return "Bozkurt";
            case PersonaType::TENGRI:      return "Tengri";
            case PersonaType::DEDE_KORKUT: return "DedeKorkut";
            case PersonaType::UMAY:        return "Umay";
            default:                       return "None";
        }
    }
};

} // namespace Apex

// ════════ include/book.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  Açılış Kitabı — APEX binary format v3
//
//  Dosya düzeni:
//    [0..3]  magic: "APEX"
//    [4..7]  versiyon: uint32_t = 3
//    [8..11] kayıt sayısı: uint32_t
//    [...] her kayıt: 8B hash + 1B from_sq + 1B to_sq + 1B promo + 1B pad + 1B weight
//
//  Kullanım:
//    OpeningBook book;
//    book.load("networks/timurlenk_openings.bin");
//    Move m = book.probe(board.hash());   // 0 = kitapta yok
// ─────────────────────────────────────────────────────────────────────────────

struct BookEntry {
    uint8_t from_sq;  // kaynak kare (0-111)
    uint8_t to_sq;    // hedef kare  (0-111)
    uint8_t promo;    // terfi taşı (0 = yok)
    uint8_t weight;   // ağırlık (1-255) — ağırlıklı rastgele seçim
};

class OpeningBook {
public:
    // Dosyadan yükle. Başarılı ise true döner.
    bool load(const std::string& path);

    // Pozisyon hash'ine göre hamle ara.
    // Birden fazla hamle varsa ağırlıklı rastgele seçim yapar.
    // Kitapta yoksa MOVE_NONE (0) döner.
    Move probe(uint64_t pos_hash) const;

    // Kitap yüklü ve dolu mu?
    bool loaded() const { return !entries_.empty(); }

    // Kitaptaki toplam pozisyon sayısı
    size_t size() const { return entries_.size(); }

private:
    std::unordered_map<uint64_t, std::vector<BookEntry>> entries_;
};

} // namespace Apex

// ════════ include/commentary.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  CommentaryBridge — Needle Python köprüsü
//
//  needle_bridge.py'yi tek-seferlik subprocess olarak çağırır (echo | python3).
//  Her istek ayrı bir thread'de çalışır — motor hiçbir zaman bloke olmaz.
//  Needle kurulu değilse veya hata olursa sessizce başarısız olur.
//
//  Kullanım:
//    bridge.set_script("/path/to/needle_bridge.py")
//    bridge.request_async("ERLIK", 250, 8, [](const std::string& s) {
//        std::cout << "info string [Needle] " << s << "\n";
//    });
// ─────────────────────────────────────────────────────────────────────────────

class CommentaryBridge {
public:
    using Callback = std::function<void(const std::string&)>;

    void set_script(const std::string& path) { script_path_ = path; }
    bool is_configured() const { return !script_path_.empty(); }

    // Asenkron: ayrı thread'de çalışır, sonuç callback ile döner
    void request_async(const std::string& persona, int score, int depth,
                       Callback cb);

    // Zaman yönetimi kararı: "spend_more"|"normal"|"spend_less" döner (async)
    // phase_score: 0=endgame .. 255=middlegame
    // material_balance: cp cinsinden (+ = kazanıyoruz, - = kaybediyoruz)
    void request_time_async(const std::string& persona,
                            int phase_score,
                            int material_balance,
                            Callback cb);

    // Açılış stili kararı: "aggressive"|"solid"|"gambit" döner (async)
    void request_opening_async(const std::string& persona, int move_num,
                                bool is_white, Callback cb);

    // Citadel strateji kararı: "rush_citadel"|"consolidate"|"ignore" döner (async)
    void request_citadel_async(const std::string& persona, int phase,
                                int king_dist, int material, Callback cb);

    // Dinamik contempt kararı: "raise"|"hold"|"lower" döner (async)
    void request_contempt_async(const std::string& persona, int score_trend,
                                 int move_num, Callback cb);

    // Tüm async işlerin bitmesini bekle (quit sırasında)
    void wait();

private:
    std::string       script_path_;
    std::atomic<int>  pending_{0};

    // Tek-seferlik subprocess: echo JSON | python3 bridge.py
    // Sonuç: commentary string veya "" (hata)
    static std::string call_bridge(const std::string& script,
                                   const std::string& persona,
                                   int score, int depth);

    // Zaman kararı için subprocess: "decision" alanı döner
    static std::string call_bridge_time(const std::string& script,
                                        const std::string& persona,
                                        int phase_score, int material_balance);

    // Açılış stili için subprocess: "style" alanı döner
    static std::string call_bridge_opening(const std::string& script,
                                            const std::string& persona,
                                            int move_num, bool is_white);

    // Citadel stratejisi için subprocess: "strategy" alanı döner
    static std::string call_bridge_citadel(const std::string& script,
                                            const std::string& persona,
                                            int phase, int king_dist, int material);

    // Contempt kararı için subprocess: "action" alanı döner
    static std::string call_bridge_contempt(const std::string& script,
                                             const std::string& persona,
                                             int score_trend, int move_num);
};

} // namespace Apex

// ════════ include/board.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  StateInfo — hamle yapıldığında değişen, undo için saklanması gereken bilgiler
// ─────────────────────────────────────────────────────────────────────────────
struct StateInfo {
    u64       hash;             // Zobrist hash (hamle öncesi)
    u64       pawn_hash;        // Piyon hash'i
    Square    enpassant;        // En passant hedef karesi (SQ_NONE ise yok)
    i32       material[2];      // Materyal dengesi [WHITE][BLACK]
    u8        halfmove;         // 50 hamle kuralı sayacı
    bool      swap_used[2];     // Yer değiştirme hakkı kullanıldı mı [W][B]
    Piece     captured;         // Bu hamlede yenilen taş (NO_PIECE ise yok)
    Move      last_move;        // Yapılan hamle (undo için)
    StateInfo* prev;            // Önceki durum (linked list)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Board — Timurlenk tahtası
//
//  Temsil: mailbox (düz dizi) + parça listeleri
//    mailbox[112]: her karede hangi taş var
//    pieces[renk][tip]: her taş tipinin kare kümesi (vector, max ~22 taş/yan)
//
//  112 kare düzeni:
//    [0–109]  : normal 11×10 tahta (rank*11 + file)
//    [110]    : WHITE_CITADEL (beyaz şahın girebileceği siyah citadel)
//    [111]    : BLACK_CITADEL (siyah şahın girebileceği beyaz citadel)
//
//  Citadel komşuluğu:
//    WHITE_CITADEL ↔ kare 21 (k, rank=1 → sütun 10, satır 1)
//    BLACK_CITADEL ↔ kare 88 (a, rank=8 → sütun 0,  satır 8)
// ─────────────────────────────────────────────────────────────────────────────
class Board {
public:
    Board();
    Board(const Board& other);
    Board& operator=(const Board& other);

    // Başlangıç pozisyonunu kur
    void setup();

    // Timurlenk FEN string'den pozisyon yükle
    bool from_string(const std::string& s);

    // Mevcut pozisyonu FEN string'e dönüştür
    std::string to_fen() const;

    // ── Tahta erişimi ──────────────────────────────────────────────────────
    Piece  piece_on(Square sq) const        { return mailbox_[sq]; }
    bool   empty   (Square sq) const        { return mailbox_[sq] == NO_PIECE; }
    Color  color_on(Square sq) const        { return piece_color(mailbox_[sq]); }
    Square king_sq (Color c)   const        { return king_sq_[c]; }

    // ── Durum bilgileri ────────────────────────────────────────────────────
    Color         side_to_move()  const { return stm_; }
    const StateInfo* state()      const { return st_; }
    u64           hash()          const { return st_->hash; }
    u64           pawn_hash()     const { return st_->pawn_hash; }
    Square        enpassant()     const { return st_->enpassant; }
    bool          swap_used(Color c) const { return st_->swap_used[c]; }
    int           halfmove()      const { return st_->halfmove; }
    int           fullmove()      const { return fullmove_; }
    i32           material(Color c)   const { return st_->material[c]; }

    // ── Parça erişimi ─────────────────────────────────────────────────────
    const std::vector<Square>& pieces(Color c, PieceType pt) const {
        return piece_list_[c][pt];
    }

    // ── Saldırı / Şah kontrolü ────────────────────────────────────────────
    bool is_attacked(Square sq, Color by) const;
    bool in_check()    const { return is_attacked(king_sq_[stm_], ~stm_); }
    bool gives_check(Move m) const;

    // ── Hamle yapma / geri alma ───────────────────────────────────────────
    // StateInfo dışarıdan verilir (arama stack'inde tutulur, heap alloc yok)
    void do_move  (Move m, StateInfo& new_st, PieceType promo = NO_PIECE_TYPE);
    void undo_move(Move m);

    // Null move (NMP için — sadece sırayı geçer)
    void do_null_move  (StateInfo& new_st);
    void undo_null_move();

    // ── Özel Timurlenk kuralları ──────────────────────────────────────────

    // Şah yer değiştirme (swap): şah herhangi kendi taşıyla yer değiştirir
    // (şahı tehdide sokmaması gerekir)
    bool can_swap(Color c) const { return !st_->swap_used[c]; }

    // Citadel girişi: şah rakibin kalesine girer → beraberlik
    // Kural: WHITE_CITADEL'e sadece siyah şah girebilir (beyazın "kalesine")
    // BLACK_CITADEL'e sadece beyaz şah girebilir
    bool is_citadel_move(Move m) const;

    // Citadel beraberlik kontrolü: şah rakibin kalesine girmişse true
    // Beyaz şah BLACK_CITADEL (111)'e girerse → beraberlik
    // Siyah şah WHITE_CITADEL (110)'a girerse → beraberlik
    bool king_on_citadel(Color c) const {
        if (c == WHITE) return king_sq_[WHITE] == BLACK_CITADEL;
        else            return king_sq_[BLACK] == WHITE_CITADEL;
    }

    // Oyun bitti mi?
    enum GameResult { ONGOING, WHITE_WINS, BLACK_WINS, DRAW };
    GameResult game_over() const;

    // Tekrar kontrolü
    bool is_repetition(int count = 3) const;

    // ── Debug ─────────────────────────────────────────────────────────────
    std::string to_string() const;

private:
    // ── İç veri yapıları ──────────────────────────────────────────────────
    Piece    mailbox_[BOARD_SIZE];   // Her karede hangi taş
    Square   king_sq_[2];            // Şahların konumu [WHITE][BLACK]

    // Parça listeleri: piece_list_[renk][tip] → o taşın bulunduğu kareler
    // Max: 2 kale, 2 zürafa... ama terfi ile artabilir. 32 yeterli.
    std::vector<Square> piece_list_[2][PIECE_TYPE_NB];

    Color      stm_;        // Sıra kimin
    int        fullmove_;   // Hamle sayısı (tam hamle)

    // StateInfo linked list (stack yerine dizi kullanılabilir — search.h'de)
    StateInfo  st_buf_[256]; // Arama için yeterli buffer
    StateInfo* st_;
    int        st_idx_;

    // ── İç yardımcılar ────────────────────────────────────────────────────
    void put_piece   (Piece p, Square sq);
    void remove_piece(Square sq);
    void move_piece  (Square from, Square to);
    void update_hash_piece(Square sq, Piece p);

    // Saldırı hesabı — her taş tipi için ayrı (movegen.h ile paylaşılır)
    bool attacked_by_sah   (Square sq, Color by) const;
    bool attacked_by_kale  (Square sq, Color by) const;
    bool attacked_by_zurafa(Square sq, Color by) const;
    bool attacked_by_talia (Square sq, Color by) const;
    bool attacked_by_at    (Square sq, Color by) const;
    bool attacked_by_deve  (Square sq, Color by) const;
    bool attacked_by_fil   (Square sq, Color by) const;
    bool attacked_by_savas (Square sq, Color by) const;
    bool attacked_by_ferz  (Square sq, Color by) const;
    bool attacked_by_vali  (Square sq, Color by) const;
    bool attacked_by_piyon (Square sq, Color by) const;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Başlangıç dizilişi (tarihsel — Timurlenk kurallarına göre)
//
//  Satır 0 (Beyaz alt sıra, rank=0):
//    Fil  ·  Deve  ·  SavaşM  ·  SavaşM  ·  Deve  ·  Fil
//    B  .  C    .   W     .    W     .   C    .   B
//
//  Satır 1 (Beyaz ana sıra, rank=1):
//    Kale-At-Talia-Zürafa-Ferz-Şah-Vali-Zürafa-Talia-At-Kale
//    R    N   T     Z      F    K   V    Z      T     N   R
//
//  Satır 2 (Beyaz piyonlar, rank=2):
//    P P P P P P P P P P P  (11 piyon — çift adım yok!)
//
//  Siyah: rank 9, 8, 7 — yansıtılmış
//    Siyah rank=9: Fil . Deve . SM . SM . Deve . Fil
//    Siyah rank=8: Kale-At-Talia-Zürafa-Vali-Şah-Ferz-Zürafa-Talia-At-Kale
//    (Ferz ve Vali siyahta simetrik olarak yer değiştirmiştir)
//
//  Citadel konumları:
//    WHITE_CITADEL (110): rank=1, file=11 (k sütununun sağında)
//      → Siyah şah buraya girerse beraberlik
//    BLACK_CITADEL (111): rank=8, file=-1 (a sütununun solunda)
//      → Beyaz şah buraya girerse beraberlik
// ─────────────────────────────────────────────────────────────────────────────

// Başlangıç dizilişi sabitleri (board.cpp'de setup() içinde kullanılır)
constexpr PieceType START_RANK0[BOARD_FILES] = {
    FIL, NO_PIECE_TYPE, DEVE, NO_PIECE_TYPE, SAVAS,
    NO_PIECE_TYPE,
    SAVAS, NO_PIECE_TYPE, DEVE, NO_PIECE_TYPE, FIL
};
constexpr PieceType START_RANK1_WHITE[BOARD_FILES] = {
    KALE, AT, TALIA, ZURAFA, FERZ, SAH, VALI, ZURAFA, TALIA, AT, KALE
};
// Siyah rank=8: Ferz ve Vali yer değiştirmiş (tarihsel asimetri)
constexpr PieceType START_RANK1_BLACK[BOARD_FILES] = {
    KALE, AT, TALIA, ZURAFA, VALI, SAH, FERZ, ZURAFA, TALIA, AT, KALE
};

} // namespace Apex

// ════════ include/movegen.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  MoveList — stack üzerinde yaşayan hamle listesi (heap alloc yok)
// ─────────────────────────────────────────────────────────────────────────────
constexpr int MAX_MOVES = 1024; // Timurlenk'te bir pozisyonda maksimum hamle (güvenli üst sınır)

struct MoveList {
    MoveExt moves[MAX_MOVES];
    int     count = 0;

    void push(Move m, PieceType promo = NO_PIECE_TYPE) {
        moves[count++] = MoveExt(m, promo);
    }
    MoveExt* begin() { return moves; }
    MoveExt* end()   { return moves + count; }
    const MoveExt* begin() const { return moves; }
    const MoveExt* end()   const { return moves + count; }
    int size() const { return count; }
    bool empty() const { return count == 0; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  MoveGenerator
//
//  Timurlenk'e özgü 11 taş tipi için tam hamle üreteci.
//  Tüm hamleler pseudo-legal üretilir; yasal kontrol Board::do_move sonrası
//  inCheck() ile yapılır.
//
//  ── Taş hareketleri (tarihsel) ──────────────────────────────────────────
//
//  ŞAH (K):   8 yönde 1 kare. Ek: swap hakkı (herhangi kendi taşıyla).
//             Citadel girişi (rakibin kalesine) → beraberlik hamlesi.
//
//  KALE (R):  Yatay ve dikey, sınırsız kayma. Taşlar engel.
//
//  ZÜRAFA (Z): Gryphon hareketi.
//             1 kare çapraz hareket et, SONRA en az 3 kare düz devam et.
//             Yani: (±1,±1) + (N×±1,0) veya (0,N×±1) N≥3
//             Aradaki kareler boş olmalı (atlayamaz).
//             Bu tarihsel Timurlenk zürafasıdır (modern satranç zürafasından farklı).
//
//  TALİA (T): Yatay veya dikey tam 2 kare sıçrar. Aradaki taşı ATLAYABİLİR.
//             (0,±2) veya (±2,0) — sadece hedef kareye bakılır.
//
//  AT (N):    Standart L hareketi: (±1,±2) veya (±2,±1). Atlayabilir.
//
//  DEVE (C):  Uzatılmış at: (±1,±3) veya (±3,±1). Atlayabilir.
//
//  FİL (E):   Çapraz tam 2 kare sıçrar. (±2,±2). Atlayabilir. Renk bağımlı.
//
//  SAVAŞ MAKİNESİ (W): Yatay/dikey tam 2 kare sıçrar. (±2,0) veya (0,±2).
//                       Atlayabilir. (Dabbaba)
//
//  FERZ (F):  Çapraz 1 kare. (±1,±1). En zayıf "süvari".
//
//  VALİ (V):  Yatay/dikey 1 kare. (±1,0) veya (0,±1). Wazir.
//
//  PİYON (P): İleri 1 kare. Çift adım YOK (tarihsel kural).
//             Çapraz ileri yeme. En passant YOK.
//             Terfi: son sıraya ulaşınca herhangi taşa dönüşür (şah hariç).
//
//  ── Özel kurallar ───────────────────────────────────────────────────────
//
//  SWAP (Yer Değiştirme):
//    Her oyuncu oyun boyunca sadece BİR KEZ kullanabilir.
//    Şah, kendi herhangi bir taşıyla yer değiştirir.
//    Şah yer değiştirme sonrasında tehdit altında olmamalıdır.
//    Kullanılan taşın hareketi geçerli olmak zorunda değildir.
//
//  CİTADEL (Kale Girişi):
//    Beyaz şah BLACK_CITADEL'e (111) girebilir → oyun beraberlikle biter.
//    Siyah şah WHITE_CITADEL'e (110) girebilir → oyun beraberlikle biter.
//    Citadel'e sadece şah girebilir, başka taş giremez.
//    Citadel'in komşuluğu:
//      WHITE_CITADEL ↔ kare 21 (k2, file=10, rank=1)
//      BLACK_CITADEL ↔ kare 88 (a9, file=0,  rank=8)
//    Şah, normal hareketiyle komşu karede ise citadel'e girebilir.
// ─────────────────────────────────────────────────────────────────────────────
class MoveGenerator {
public:
    // ── Ana üretim fonksiyonları ──────────────────────────────────────────

    // Tüm yasal hamleler (şaha girilmeyenler)
    static void generate_legal   (const Board& b, MoveList& ml);

    // Pseudo-legal hamleler (şah kontrolü yapılmaz, hızlı)
    static void generate_pseudo  (const Board& b, MoveList& ml);

    // Sadece yemeler + promosyonlar (quiescence search için)
    static void generate_captures(const Board& b, MoveList& ml);

    // Şahı kurtaran hamleler (şah altındayken)
    static void generate_evasions(const Board& b, MoveList& ml);

    // ── Taş bazlı üreticiler (public — evaluate/search tarafından kullanılır) ─
    static void gen_sah   (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_kale  (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_zurafa(const Board& b, Square from, Color c, MoveList& ml);
    static void gen_talia (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_at    (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_deve  (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_fil   (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_savas (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_ferz  (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_vali  (const Board& b, Square from, Color c, MoveList& ml);
    static void gen_piyon (const Board& b, Square from, Color c, MoveList& ml);

    // Swap hamlesi üretimi
    static void gen_swap  (const Board& b, Color c, MoveList& ml);

    // ── Mobilite sayısı (değerlendirici için) ──────────────────────────────
    // Yasal hamle üretmeden sadece sayar (daha hızlı)
    static int  mobility(const Board& b, Color c);

    // ── Yardımcılar ───────────────────────────────────────────────────────

    // Kayma yardımcısı: (dx,dy) yönünde engele kadar git
    // Hamleler ml'e eklenir; taş yeme dahil
    static void slide(const Board& b, Square from, Color c,
                      int dx, int dy, MoveList& ml);

    // Sıçrama yardımcısı: (dx,dy) hedefine git (varsa yiyor, yoksa geçiyor)
    static void jump(const Board& b, Square from, Color c,
                     int dx, int dy, MoveList& ml);

    // Kare geçerli mi ve erişilebilir mi (boş veya düşman)
    static bool can_go(const Board& b, Color c, int file, int rank);

    // Zürafa için: (df,dr) çapraz + (N×sf, 0) veya (0, N×sr) düz (N≥3)
    static void zurafa_ray(const Board& b, Square from, Color c,
                           int df, int dr, MoveList& ml);
};

// ─────────────────────────────────────────────────────────────────────────────
//  Hamle Sıralama (Move Ordering)
//  Alpha-Beta verimliliği için hamleler en iyiden en kötüye sıralanır.
//
//  Öncelik sırası:
//    1. TT hamlesi (önceki aramanın en iyi hamlesi)
//    2. Kazançlı yemeler (MVV-LVA: Most Valuable Victim / Least Valuable Attacker)
//    3. Şah terfileri
//    4. Killer hamleler (aynı derinlikte beta kesimi yapan hamleler)
//    5. Counter-move (önceki hamleye tarihsel en iyi yanıt)
//    6. History heuristic (geçmişte iyi beta kesimi yapan hamleler)
//    7. Kayıplı yemeler (SEE < 0)
// ─────────────────────────────────────────────────────────────────────────────
constexpr int MAX_PLY = 128;

class MoveOrderer {
public:
    MoveOrderer() { reset(); }

    void reset();

    // Hamleleri skorla ve sırala
    void score_moves(MoveList& ml, const Board& b,
                     Move tt_move, int ply, Color stm,
                     Move prev_move = MOVE_NONE) const;

    // Partial insertion sort: index'ten itibaren en yüksek skorlu hamleyi öne getirir
    MoveExt& next_move(MoveList& ml, int index) const;

    // Killer hamle ekle (beta kesimi yapan hamle)
    void add_killer(Move m, int ply);

    // History güncelle
    void update_history(Move m, int depth, bool good, Color stm);

    // Counter-move güncelle
    void update_counter(Move prev, Move response);

    // SEE (Static Exchange Evaluation) — taş değişiminin net değeri
    static i32 see(const Board& b, Move m);

private:
    // Killers: her derinlik için 2 hamle
    Move killers_[MAX_PLY][2];

    // History tablosu: [renk][kaynak][hedef] → skor
    // Timurlenk için: 2 × 112 × 112 = 25088 giriş
    i32  history_[2][BOARD_SIZE][BOARD_SIZE];

    // Counter-move: önceki hamleye karşı en iyi yanıt
    Move counter_[BOARD_SIZE][BOARD_SIZE];
};

} // namespace Apex

// ════════ include/evaluate.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  Piece-Square Tables (PST)
//  Her taş tipi için 11×10 = 110 kare pozisyon değer tablosu.
//  2 faz: middlegame (mg) ve endgame (eg).
//  Siyah için tablo dikey yansıtılır (rank = 9 - rank).
//
//  Değerler centipawn (cp) cinsinden, materyal değerine ek olarak.
// ─────────────────────────────────────────────────────────────────────────────
struct PSTTable {
    i16 mg[BOARD_NORMAL]; // middlegame — indeks: rank*11+file (beyaz perspektifi)
    i16 eg[BOARD_NORMAL]; // endgame
};

// 11 taş tipi için PST tabloları (evaluate.cpp'de tanımlanır)
extern PSTTable PST[PIECE_TYPE_NB];

// PST'yi başlat
void init_pst();

// ─────────────────────────────────────────────────────────────────────────────
//  Oyun Fazı Hesabı
//  Materyale göre [0.0, 1.0] arası faz değeri.
//  1.0 = tam middlegame, 0.0 = tam endgame.
// ─────────────────────────────────────────────────────────────────────────────
// Faz katkısı — şah ve piyon faza dahil değil
constexpr i32 PHASE_WEIGHT[PIECE_TYPE_NB] = {
    0, 0,   // NO_PIECE, SAH
    4,      // KALE
    4,      // ZURAFA
    3,      // TALIA
    3,      // AT
    3,      // DEVE
    2,      // FIL
    2,      // SAVAS
    1,      // FERZ
    1,      // VALI
    0       // PIYON
};
// Her iki tarafta tam materyal: 2×(4+4+3+3+3+2+2+1+1) × 2 taraf = 96
constexpr i32 MAX_PHASE = 96;

// ─────────────────────────────────────────────────────────────────────────────
//  Kral Güvenliği Tablosu
//  Şahın yakınındaki saldırıcı sayısına ve güçlerine göre ceza.
// ─────────────────────────────────────────────────────────────────────────────
constexpr i32 KING_ATTACK_WEIGHT[PIECE_TYPE_NB] = {
    0, 0,  // NO_PIECE, SAH (şah şahı tehdit edemez arama sırasında)
    4,     // KALE
    5,     // ZURAFA
    2,     // TALIA
    2,     // AT
    2,     // DEVE
    2,     // FIL
    2,     // SAVAS
    1,     // FERZ
    1,     // VALI
    0      // PIYON
};

// Saldırı sayısına göre ceza tablosu (0–31 arası saldırı skoru)
extern i32 KING_SAFETY_TABLE[64];

// ─────────────────────────────────────────────────────────────────────────────
//  Değerlendirme Bonusları ve Cezaları
// ─────────────────────────────────────────────────────────────────────────────

// Mobilite bonusu: [taş tipi][hamle sayısı] — ortalama hareketlilik üstü bonus
// Maksimum hamle sayısı yaklaşık değerler:
//   Kale: ~20, Zürafa: ~30, Talia: ~4, At: ~8, Deve: ~8, Fil: ~4
//   SavaşM: ~4, Ferz: ~4, Vali: ~4
constexpr i32 MOBILITY_BONUS_MG[PIECE_TYPE_NB][32] = {
    {}, // NO_PIECE_TYPE
    {}, // SAH — ayrı hesaplanır
    // KALE: 0–20 hamle arası
    { -20,-15,-10,-5,0,3,6,9,11,13,15,16,17,18,19,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20 },
    // ZURAFA: 0–30 hamle arası (çok mobil)
    { -25,-20,-15,-10,-5,0,4,8,11,14,17,19,21,23,24,25,26,27,27,28,28,29,29,29,30,30,30,30,30,30,30,30 },
    // TALIA: 0–4 hamle arası
    { -10,-5,0,5,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10 },
    // AT: 0–8 hamle arası
    { -25,-15,-5,5,12,17,20,23,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25 },
    // DEVE: 0–8 hamle arası
    { -20,-12,-4,4,10,15,18,21,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23 },
    // FİL: 0–4 hamle arası (renk bağımlı, sınırlı)
    { -15,-5,5,10,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
    // SAVAŞ MAKİNESİ: 0–4 hamle arası
    { -15,-5,3,8,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12 },
    // FERZ: 0–4 hamle arası
    { -8,-3,2,6,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9 },
    // VALİ: 0–4 hamle arası
    { -8,-3,2,5,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8 },
    {}, // PİYON — ayrı hesaplanır
};

constexpr i32 MOBILITY_BONUS_EG[PIECE_TYPE_NB][32] = {
    {}, {},
    { -18,-13,-8,-3,2,5,8,10,12,14,15,16,17,17,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18 },
    { -20,-15,-10,-5,0,4,8,11,14,16,18,20,21,22,23,24,24,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25 },
    { -8,-3,3,8,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12 },
    { -20,-12,-4,4,10,14,17,19,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21 },
    { -18,-10,-3,4,9,13,16,18,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20 },
    { -12,-4,4,9,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13 },
    { -12,-4,3,7,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10 },
    { -6,-2,3,6,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8 },
    { -6,-2,2,5,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7 },
    {},
};

// Çıplak şah cezası (bare king — tek başına kalan şah)
constexpr i32 BARE_KING_PENALTY_MG = 600;
constexpr i32 BARE_KING_PENALTY_EG = 900;

// Piyon yapısı
constexpr i32 PASSED_PAWN_BONUS[10] = { 0,10,15,25,40,60,85,120,160,0 }; // rank bazında
constexpr i32 DOUBLED_PAWN_PENALTY  = -20;
constexpr i32 ISOLATED_PAWN_PENALTY = -15;
constexpr i32 BACKWARD_PAWN_PENALTY = -10;

// Citadel tehdidi bonusu (şah citadel'e yaklaşınca)
constexpr i32 CITADEL_THREAT_BONUS = 30; // 2 kare uzakta
constexpr i32 CITADEL_ADJACENT_BONUS = 60; // 1 kare uzakta

// ─────────────────────────────────────────────────────────────────────────────
//  Persona Ağırlık Çarpanları
//  Her persona için materyal değerlendirme çarpanları.
//  1.0 = varsayılan, >1.0 = güçlendir, <1.0 = zayıflat.
// ─────────────────────────────────────────────────────────────────────────────
struct PersonaWeights {
    float rook_mult        = 1.0f;
    float pawn_mult        = 1.0f;
    float knight_mult      = 1.0f;
    float king_safety_mult = 1.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Evaluator
// ─────────────────────────────────────────────────────────────────────────────
class Evaluator {
public:
    Evaluator() : pawn_table_() {}

    // Persona ağırlık çarpanlarını ayarla (idx: PersonaType int değeri)
    void setPersona(int idx);

    // NNUE yükle — başarılıysa true
    bool load_nnue(const std::string& path) { return nnue_.load(path); }
    bool nnue_loaded() const { return nnue_.loaded(); }
    void set_use_nnue(bool v) { use_nnue_ = v; }
    bool use_nnue() const { return use_nnue_ && nnue_.loaded(); }

    // Ana değerlendirme fonksiyonu
    // side: değerlendirmeyi kimin perspektifinden yapacağız (+iyi = side iyi)
    i32 evaluate(const Board& b, Color side);

    // Statik değerlendirme (quiescence için hızlı versiyon)
    static i32 evaluate_fast(const Board& b, Color side);

private:
    PawnTable      pawn_table_;
    NNUENetwork    nnue_;
    bool           use_nnue_ = false;
    PersonaWeights pw_;         // Persona çarpanları

    // Alt fonksiyonlar
    i32 eval_material (const Board& b) const;
    i32 eval_pst      (const Board& b, Color c, int phase) const;
    i32 eval_mobility (const Board& b, Color c, int phase) const;
    i32 eval_king_safety(const Board& b, Color c, int phase) const;
    i32 eval_pawn_structure(const Board& b, Color c);
    i32 eval_citadel  (const Board& b, Color c, int phase) const;

    // Tahmini oyun fazı [0=endgame .. MAX_PHASE=middlegame]
    static int game_phase(const Board& b);

    // Taraf perspektifine göre PST değeri
    static i32 pst_value(PieceType pt, Square sq, Color c, int phase);
};

} // namespace Apex

// ════════ include/search.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  Zaman Yönetimi
// ─────────────────────────────────────────────────────────────────────────────
struct TimeManager {
    using Clock = std::chrono::steady_clock;
    using Ms    = std::chrono::milliseconds;

    Clock::time_point start;
    int64_t  hard_limit_ms = 0;  // Kesinlikle bu süre aşılamaz
    int64_t  soft_limit_ms = 0;  // İdeal hedef (iterasyon tamamlandıktan sonra dur)
    bool     infinite      = false;

    void init(int64_t wtime, int64_t btime, int64_t winc, int64_t binc,
              int movestogo, Color stm);
    void init_movetime(int64_t ms) { hard_limit_ms = ms; soft_limit_ms = ms * 9 / 10; }
    void init_infinite()           { infinite = true; }

    int64_t elapsed_ms() const {
        return std::chrono::duration_cast<Ms>(Clock::now() - start).count();
    }
    bool soft_expired() const { return !infinite && elapsed_ms() >= soft_limit_ms; }
    bool hard_expired() const { return !infinite && elapsed_ms() >= hard_limit_ms; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Arama Sınırları (UCI'dan gelir)
// ─────────────────────────────────────────────────────────────────────────────
struct SearchLimits {
    int64_t wtime    = 0;
    int64_t btime    = 0;
    int64_t winc     = 0;
    int64_t binc     = 0;
    int     movestogo = 0;
    int64_t movetime  = 0;
    int     depth     = MAX_PLY;
    int64_t nodes     = 0;
    bool    infinite  = false;
    bool    ponder    = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Arama İstatistikleri
// ─────────────────────────────────────────────────────────────────────────────
struct SearchStats {
    u64 nodes        = 0;  // Ziyaret edilen toplam düğüm
    u64 qnodes       = 0;  // Quiescence düğümleri
    u64 tt_hits      = 0;  // TT isabeti
    u64 tt_cuts      = 0;  // TT'den beta kesimi
    u64 null_cuts    = 0;  // Null move beta kesimi
    u64 lmr_count    = 0;  // LMR uygulamaları
    u64 singular_ext = 0;  // Singular extension uygulamaları
    int sel_depth    = 0;  // Seçici derinlik (quiescence dahil)

    void reset() { *this = {}; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Arama Stack çerçevesi (her derinlik için)
// ─────────────────────────────────────────────────────────────────────────────
struct SearchFrame {
    StateInfo  state;           // do_move için StateInfo (heap alloc yok)
    Move       current_move;    // Bu derinlikte denenen hamle
    Move       excluded_move;   // Singular extension — dışlanan hamle
    i32        static_eval;     // Bu pozisyonun statik değerlendirmesi
    int        ply;             // Kök'ten uzaklık
    bool       in_check;        // Şah altında mı
    bool       tt_pv;           // TT'den PV hattında mıyız
};

// ─────────────────────────────────────────────────────────────────────────────
//  PV (Principal Variation) — en iyi hamle dizisi
// ─────────────────────────────────────────────────────────────────────────────
struct PVLine {
    Move moves[MAX_PLY];
    int  length = 0;

    void clear()              { length = 0; }
    void push(Move m)         { if (length < MAX_PLY) moves[length++] = m; }
    Move best() const         { return length > 0 ? moves[0] : MOVE_NONE; }
    void copy_from(Move m, const PVLine& child) {
        moves[0] = m;
        for (int i = 0; i < child.length; i++) moves[i+1] = child.moves[i];
        length = child.length + 1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  LMR (Late Move Reductions) Tablosu
//  lmr_table[depth][move_index] → azaltma miktarı
//  Formül: max(0, floor(ln(depth) * ln(moves) / 2.0 - 0.5))
// ─────────────────────────────────────────────────────────────────────────────
extern int LMR_TABLE[MAX_PLY][MAX_MOVES];
void init_lmr_table();

// ─────────────────────────────────────────────────────────────────────────────
//  Searcher — Ana Arama Motoru
//
//  Timurlenk için adapte edilmiş APEX arama algoritması:
//
//  1. Iterative Deepening + Aspiration Windows
//  2. Alpha-Beta Negamax (fail-soft)
//  3. Transposition Table entegrasyonu
//  4. Null Move Pruning (NMP)
//  5. Late Move Reductions (LMR) — dinamik azaltma
//  6. Singular Extensions
//  7. Futility Pruning
//  8. Razoring
//  9. Probcut
// 10. Quiescence Search (yemeler + terfi)
// 11. Lazy SMP (çok çekirdek) — thread pool üzerinden
// 12. Citadel farkındalığı — beraberlik hamlelerini doğru skorla
// ─────────────────────────────────────────────────────────────────────────────
class Searcher {
public:
    Searcher(Board& board, TranspositionTable& tt);

    // Ana arama — UCI'dan çağrılır
    void start_search(const SearchLimits& limits);

    // Thread sayısını ayarla (Lazy SMP)
    void set_threads(int n) { n_threads_ = std::max(1, n); }

    // Aramayı durdur (UCI stop)
    void stop() { stopped_.store(true); }

    // Ponder pozisyon gerçekleşti — zaman yönetimini aktif et
    void ponderhit();

    // Persona katmanını güncelle
    void set_persona(const PersonaLayer& p) { persona_ = p; }

    // Sonuç erişimi
    Move  best_move()  const { return best_move_; }
    Move  ponder_move() const { return ponder_move_; }
    i32   best_score() const { return best_score_; }

    // UCI info callback (her iterasyon sonrası çağrılır)
    using InfoCallback = std::function<void(int depth, int sel_depth,
                                            i32 score, u64 nodes,
                                            int64_t ms, const PVLine&)>;
    void set_info_callback(InfoCallback cb) { info_cb_ = cb; }

    // Needle commentary callback — persona adı, skor, derinlik ile çağrılır
    using NeedleCallback = std::function<void(const std::string& persona,
                                              int score, int depth)>;
    void set_needle_callback(NeedleCallback cb) { needle_cb_ = cb; }

    // Zaman yönetimi kararı callback — async, sonuç "spend_more"|"normal"|"spend_less"
    using TimeAdjCallback = std::function<void(
        const std::string& persona,
        int phase_score,
        int material_balance,
        std::function<void(const std::string&)> result_cb)>;
    void set_time_adj_callback(TimeAdjCallback cb) { time_adj_cb_ = cb; }

    // Açılış stili kararı — "aggressive"|"solid"|"gambit"
    using OpeningCb = std::function<void(
        const std::string& persona, int move_num, bool is_white,
        std::function<void(const std::string&)> result_cb)>;
    void set_opening_callback(OpeningCb cb) { opening_cb_ = cb; }

    // Citadel strateji kararı — "rush_citadel"|"consolidate"|"ignore"
    using CitadelCb = std::function<void(
        const std::string& persona, int phase, int king_dist, int material,
        std::function<void(const std::string&)> result_cb)>;
    void set_citadel_callback(CitadelCb cb) { citadel_cb_ = cb; }

    // Dinamik contempt kararı — "raise"|"hold"|"lower"
    using ContemptCb = std::function<void(
        const std::string& persona, int score_trend, int move_num,
        std::function<void(const std::string&)> result_cb)>;
    void set_contempt_callback(ContemptCb cb) { contempt_cb_ = cb; }

    // Tekrar tablosu temizle (yeni oyun)
    void new_game();

    // Statik değerlendirme (test için)
    i32 static_eval(const Board& b);

    // NNUE erişimi (UCI setoption için)
    Evaluator& evaluator() { return evaluator_; }

private:
    Board&              board_;
    TranspositionTable& tt_;
    MoveOrderer         orderer_;
    Evaluator           evaluator_;
    TimeManager         tm_;
    SearchStats         stats_;
    SearchFrame         stack_[MAX_PLY + 4]; // +4 sentinel
    PVLine              root_pv_;
    Move                best_move_;
    Move                ponder_move_;
    i32                 best_score_;
    std::atomic<bool>   stopped_;
    InfoCallback        info_cb_;
    NeedleCallback      needle_cb_;
    TimeAdjCallback     time_adj_cb_;
    std::atomic<int>    time_adj_{0};  // 0=normal, 1=spend_more, -1=spend_less
    OpeningCb           opening_cb_;
    CitadelCb           citadel_cb_;
    ContemptCb          contempt_cb_;
    std::atomic<int>    opening_adj_{0};      // 0=solid/none, 1=aggressive, 2=gambit
    std::atomic<int>    citadel_adj_{0};      // 0=hold, 1=rush, -1=ignore
    std::atomic<int>    contempt_pending_{0}; // 0=hold, 1=raise, -1=lower
    i32                 citadel_bias_ = 0;    // citadel_score() içinde kullanılır
    PersonaLayer        persona_;

    // ── Iterative Deepening ───────────────────────────────────────────────
    void iterative_deepening(int max_depth);

    // ── Alpha-Beta (Negamax, fail-soft) ──────────────────────────────────
    // pv_node:    PV düğümü mü (alpha < beta-1)
    // cut_node:   kesim düğümü beklentisi
    i32 search(int depth, i32 alpha, i32 beta, int ply,
               bool pv_node, bool cut_node, PVLine* pv_line = nullptr);

    // ── Quiescence Search ─────────────────────────────────────────────────
    i32 qsearch(i32 alpha, i32 beta, int ply);

    // ── Budama ve Azaltma Kararları ───────────────────────────────────────

    // Razoring: çok düşük derinlikte statik eval yeterliyse
    bool try_razoring(int depth, i32 alpha, i32 static_eval,
                      bool in_check, i32& score);

    // Null Move Pruning
    bool try_null_move(int depth, i32 beta, i32 static_eval,
                       bool in_check, bool pv_node, int ply, i32& score);

    // Probcut: yüksek beta kenarında erken kesim
    bool try_probcut(int depth, i32 beta, bool in_check, int ply,
                     i32 static_eval, i32& score);

    // LMR azaltma miktarı
    int lmr_reduction(int depth, int move_idx, bool pv_node,
                      bool gives_check, bool is_capture) const;

    // Singular Extension kontrolü
    bool is_singular(Move m, int depth, i32 tt_score, int ply);

    // ── Timurlenk'e özgü kontroller ──────────────────────────────────────

    // Citadel hamlesi skoru: beraberlik = 0 (contempt ve citadel_bias ile ayarlanabilir)
    i32 citadel_score() const { return SCORE_DRAW + contempt_ + citadel_bias_; }

    // Bare king cezası (bir tarafın şah dışında taşı kalmadı)
    bool is_bare_king(Color c) const;

    // ── Yardımcılar ───────────────────────────────────────────────────────
    bool time_check();           // Zaman doldu mu (her 1024 düğümde kontrol)
    void update_stats(int depth, Move best, i32 best_score,
                      i32 alpha, i32 beta, int ply,
                      const MoveList& tried_quiets);

    i32 contempt_ = 0;  // Beraberlik için beklenti ayarı (+ = beraberlikten kaçın)
    u64 node_limit_ = 0;
    bool pondering_  = false;

    // ── Lazy SMP ─────────────────────────────────────────────────────────────
    int                        n_threads_ = 1;
    std::vector<std::thread>   helpers_;
    std::atomic<bool>*         extern_stop_ = nullptr;  // Ana thread'in durdurma sinyali

    void run_as_helper(std::atomic<bool>& parent_stop, int max_depth);
};

// ─────────────────────────────────────────────────────────────────────────────
//  Alpha-Beta Algoritması — Pseudokod (dokümantasyon)
//
//  function search(depth, alpha, beta, ply, pv_node):
//
//    // 1. Terminal kontroller
//    if time_up or nodes_limit: return 0
//    if ply >= MAX_PLY: return evaluate()
//    if is_repetition or halfmove >= 100: return DRAW
//
//    // 2. Citadel kontrolü (Timurlenk özgün)
//    if citadel_reached: return DRAW (± contempt)
//
//    // 3. Transposition Table sorgusu
//    tt_entry = tt.probe(hash)
//    if tt_entry and tt_entry.depth >= depth:
//        if tt_entry.flag == EXACT: return tt_entry.score
//        if tt_entry.flag == LOWER: alpha = max(alpha, tt_entry.score)
//        if tt_entry.flag == UPPER: beta  = min(beta,  tt_entry.score)
//        if alpha >= beta: return tt_entry.score  // TT cut
//
//    // 4. Statik değerlendirme
//    in_check = board.in_check()
//    if not in_check:
//        static_eval = evaluate()
//        // Improving: bu pozisyon 2 hamle öncesinden iyi mi?
//        improving = static_eval > stack[ply-2].static_eval
//
//    // 5. Pruning (şah altında değilken)
//    if not pv_node and not in_check:
//        // 5a. Razoring (depth <= 3)
//        if depth <= 3 and static_eval + razor_margin[depth] <= alpha:
//            qscore = qsearch(alpha, beta)
//            if qscore <= alpha: return qscore
//
//        // 5b. Null Move Pruning
//        if depth >= 3 and static_eval >= beta and board.has_pieces():
//            R = 3 + depth/6 + min(3, (static_eval-beta)/200)
//            do_null_move()
//            score = -search(depth-R, -beta, -beta+1, ply+1, false, true)
//            undo_null_move()
//            if score >= beta: return beta (null cut)
//
//        // 5c. Probcut (depth >= 5)
//        if depth >= 5:
//            probcut_beta = beta + 200
//            for each capture with SEE >= probcut_beta - static_eval:
//                score = -search(depth-4, -probcut_beta, -probcut_beta+1, ...)
//                if score >= probcut_beta: return probcut_beta
//
//    // 6. Hamle döngüsü
//    generate_moves(ml)
//    order_moves(ml, tt_move, ply)
//    best_score = -INFINITE
//    tried_quiets = []
//
//    for i, move in enumerate(ml):
//        if move == excluded_move: continue
//
//        // Singular Extension
//        extension = 0
//        if move == tt_move and depth >= 8 and tt_score != NONE:
//            singular_beta = tt_score - depth
//            score = search(depth/2, singular_beta-1, singular_beta, ..., excluded=move)
//            if score < singular_beta: extension = 1   // Tekil hamle
//            elif singular_beta >= beta: return singular_beta  // Multi-cut
//
//        do_move(move)
//
//        // Late Move Reductions
//        if i >= 2 and depth >= 3 and not in_check and not gives_check:
//            R = lmr_table[depth][i]
//            R -= pv_node ? 1 : 0
//            R -= improving ? 1 : 0
//            R = max(1, R)
//            // LMR ile önce sığ ara
//            score = -search(depth-1-R, -alpha-1, -alpha, ply+1, false, true)
//            // Umut verirse tam derinlikte yeniden ara
//            if score > alpha:
//                score = -search(depth-1, -alpha-1, -alpha, ply+1, false, !cut_node)
//        else:
//            // Normal tam arama (ilk hamle veya erken hamleler)
//            score = -search(depth-1+extension, -beta, -alpha, ply+1, pv_node, false)
//
//        undo_move(move)
//
//        if score > best_score:
//            best_score = score
//            best_move  = move
//            if score > alpha:
//                alpha = score
//                if pv_node: update_pv(move)
//                if alpha >= beta:
//                    // Beta kesimi — güncelle
//                    update_stats(move, tried_quiets, depth, ply)
//                    break  // Budama
//
//    // 7. Mat / Pat / Beraberlik kontrolü
//    if best_score == -INFINITE:
//        if in_check: return -SCORE_MATE + ply  // Mat yedik
//        else:        return SCORE_DRAW          // Pat
//
//    // 8. TT'ye yaz
//    flag = best_score >= beta   ? TT_LOWER :
//           best_score <= alpha0 ? TT_UPPER : TT_EXACT
//    tt.store(hash, depth, flag, best_score, static_eval, best_move)
//
//    return best_score
// ─────────────────────────────────────────────────────────────────────────────

} // namespace Apex

// ════════ include/uci.h ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  UCI (Universal Chess Interface) Protokol Arayüzü
//
//  Apex Timurlenk motoru standart UCI protokolüyle iletişim kurar.
//  Bu sayede Arena, Cute Chess, BankSiaQ gibi tüm GUI'larda çalışır.
//
//  Desteklenen komutlar:
//    uci            → motor kimliğini ve seçenekleri bildir
//    isready        → hazır olduğunda "readyok" yaz
//    ucinewgame     → yeni oyun, TT temizle
//    position       → pozisyonu kur (startpos veya fen)
//    go             → aramayı başlat
//    stop           → aramayı durdur
//    quit           → motoru kapat
//    setoption      → ayar değiştir
//    ponderhit      → ponder pozisyonu gerçekleşti
//    d              → tahtayı debug modda göster (standart olmayan ama yaygın)
//    eval           → statik değerlendirmeyi göster (debug)
//
//  Motor seçenekleri (setoption name X value Y):
//    Hash           → TT boyutu (MB, varsayılan: 64)
//    Threads        → İş parçacığı sayısı (varsayılan: 1, max: 128)
//    Contempt       → Beraberlik beklentisi cp cinsinden (varsayılan: 0)
//    SyzygyPath     → Syzygy tablebase dizin yolu
//    Ponder         → Ponder modu (varsayılan: false)
//    MultiPV        → Kaç hamle raporlansın (varsayılan: 1, max: 10)
//    MoveOverhead   → GUI iletişim gecikmesi ms (varsayılan: 30)
//    UCI_Chess960   → Chess960 modu (Timurlenk için kullanılmaz)
//
//  Timurlenk'e özgü seçenekler:
//    CitadelDraw    → Citadel girişi beraberlik sayılsın mı (varsayılan: true)
//    SwapAllowed    → Yer değiştirme hakkı aktif mi (varsayılan: true)
//    PieceNames     → Taş isim formatı: "tr" / "en" (varsayılan: "tr")
// ─────────────────────────────────────────────────────────────────────────────

// Motor sürüm bilgisi
constexpr const char* ENGINE_NAME    = "Apex Timurlenk";
constexpr const char* ENGINE_VERSION = "0.7.0-nnue";
constexpr const char* ENGINE_AUTHOR  = "Apex Development Team";

// ─────────────────────────────────────────────────────────────────────────────
//  Motor Seçenekleri
// ─────────────────────────────────────────────────────────────────────────────
struct EngineOptions {
    int  hash_mb        = 64;
    int  threads        = 1;
    int  contempt       = 0;
    int  multi_pv       = 1;
    int  move_overhead  = 30;  // ms
    bool ponder         = false;
    bool citadel_draw   = true;
    bool swap_allowed   = true;
    std::string syzygy_path = "";
    std::string piece_names = "tr"; // "tr" veya "en"
    // L1.5 Persona Katmanı
    PersonaLayer persona;
    // Needle commentary bridge
    bool        use_needle      = false;
    std::string needle_script   = "";   // needle_bridge.py yolu
    // NNUE
    bool        use_nnue        = false;
    std::string nnue_path       = "";   // networks/apex_v1.bin yolu
    // Açılış Kitabı
    bool        own_book        = false;
    std::string book_path       = "networks/timurlenk_openings.bin";
};

// ─────────────────────────────────────────────────────────────────────────────
//  Timurlenk FEN Formatı
//
//  Standart satranç FEN'ini Timurlenk'e genişletiyoruz:
//
//  <pozisyon> <sıra> <ek-bayraklar> <yarım-hamle> <tam-hamle>
//
//  Pozisyon: rank 9'dan rank 0'a, '/' ile ayrılmış 10 satır
//    + iki citadel durumu köşeli parantez içinde
//  Taş harfleri: K R Z T N C E W F V P (büyük=beyaz, küçük=siyah)
//  Boş kareler: sayı (1-11)
//  Citadel: [wc] beyaz citadel doluysa (olamaz normalde),
//            [bc] siyah citadel doluysa
//
//  Ek bayraklar: s veya - (swap hakkı: 'w' beyaz kullandı, 'b' siyah kullandı)
//    Örn: "-"=her iki taraf hakkını kullanmadı
//         "w"=beyaz kullandı  "b"=siyah kullandı  "wb"=her ikisi kullandı
//
//  Başlangıç FEN:
//    "e.c.w.w.c.e/RRRRRRRRRRR/PPPPPPPPPPP/11/11/11/11/ppppppppppp/rrrrrrrrrrr/E.C.W.W.C.E
//     KR1ZF1VZR1K/11/...
//
//  NOT: Gerçek başlangıç FEN'i board.cpp setup() fonksiyonunda tanımlanır.
//       Bu format gelecekteki pozisyon yükleyici için rezerve edilmiştir.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
//  UCI Arayüz Sınıfı
// ─────────────────────────────────────────────────────────────────────────────
class UCI {
public:
    UCI();
    ~UCI();

    // Ana döngü — stdin'den komut okur, stdout'a yazar
    void loop();

    // Tekil komut işle (test için)
    void process(const std::string& line);

private:
    Board               board_;
    TranspositionTable  tt_;
    Searcher            searcher_;
    EngineOptions       opts_;
    CommentaryBridge    commentary_;
    OpeningBook         book_;
    std::thread         search_thread_;
    bool                searching_  = false;
    bool                quit_       = false;
    std::vector<StateInfo> move_history_; // position komutundaki hamle geçmişi

    // ── Komut İşleyiciler ─────────────────────────────────────────────────
    void cmd_uci();
    void cmd_isready();
    void cmd_ucinewgame();
    void cmd_position(std::istringstream& ss);
    void cmd_go       (std::istringstream& ss);
    void cmd_stop();
    void cmd_quit();
    void cmd_setoption(std::istringstream& ss);
    void cmd_ponderhit();
    void cmd_debug_board();
    void cmd_debug_eval();
    void cmd_debug_moves();
    void cmd_perft(std::istringstream& ss);  // Hamle üreteci doğrulama

    // ── UCI Çıktı Oluşturucular ───────────────────────────────────────────

    // info depth X seldepth Y score cp Z nodes N nps N pv ...
    void print_info(int depth, int sel_depth, i32 score,
                    u64 nodes, int64_t ms, const PVLine& pv,
                    int multi_pv_idx = 0);

    // bestmove X ponder Y
    void print_bestmove(Move best, Move ponder);

    // ── Hamle Formatı ─────────────────────────────────────────────────────
    // Timurlenk koordinat sistemi:
    //   file: a-k (0-10), rank: 1-10 (rank 0 = "1")
    //   Normal: "e5f7", Terfi: "a9b10=Z" (Zürafa'ya terfi)
    //   Swap: "swap:e1g1" (şah e1'den g1'deki taşla yer değiştirir)
    //   Citadel beyaz: "k2-wc" (k2'den beyaz citadel'e)
    //   Citadel siyah: "a9-bc"
    static std::string move_to_str(Move m, PieceType promo = NO_PIECE_TYPE);
    static Move        str_to_move(const std::string& s, const Board& b);
    static std::string sq_to_str  (Square sq);
    static Square      str_to_sq  (const std::string& s);

    // ── Perft (hamle üreteci test) ────────────────────────────────────────
    u64 perft(int depth);

    // ── Yardımcılar ───────────────────────────────────────────────────────
    void apply_options();  // opts_ değiştiğinde motoru güncelle
    void wait_search_done();
};

// ─────────────────────────────────────────────────────────────────────────────
//  UCI Hamle Koordinat Sistemi (Referans)
//
//  Timurlenk 11×10 tahta koordinatları:
//
//    Sütunlar (file): a=0, b=1, c=2, d=3, e=4, f=5, g=6, h=7, i=8, j=9, k=10
//    Satırlar (rank): 1=rank0, 2=rank1, ..., 10=rank9
//
//    Kare indeksi: sq = (rank-1)*11 + file_index
//      Örn: e5 → file=4, rank=4 → sq = 4*11+4 = 48
//           k2 → file=10, rank=1 → sq = 1*11+10 = 21
//
//    Citadel kareleri:
//      "wc" → WHITE_CITADEL (110) — beyazın rakip kalesine giriş noktası
//      "bc" → BLACK_CITADEL (111) — siyahın rakip kalesine giriş noktası
//
//    Citadel komşusu:
//      WHITE_CITADEL ↔ k2 (sq=21): Siyah şah k2'deyken "wc"ye girebilir
//      BLACK_CITADEL ↔ a9 (sq=88): Beyaz şah a9'dayken "bc"ye girebilir
//
//  Hamle örnekleri:
//    "e1f3"     → e1'den f3'e normal hamle
//    "a9bc"     → beyaz şah a9'dan siyah citadel'e (beraberlik hamlesi)
//    "k2wc"     → siyah şah k2'den beyaz citadel'e (beraberlik hamlesi)
//    "b9b10=K"  → b9'dan b10'a piyon hamlesi, Kale'ye terfi
//    "swap e1h4" → şah e1'deki taşla h4'tekini yer değiştirir
// ─────────────────────────────────────────────────────────────────────────────

} // namespace Apex

// ════════ src/tt.cpp ════════


namespace Apex {

ZobristTable Zobrist;

// ─────────────────────────────────────────────────────────────────────────────
//  TranspositionTable
// ─────────────────────────────────────────────────────────────────────────────
TranspositionTable::TranspositionTable()
    : table_(nullptr), size_(0), age_(0)
{
    Zobrist.init();
    resize(TT_DEFAULT_MB);
}

TranspositionTable::~TranspositionTable() {
    std::free(table_);
}

void TranspositionTable::resize(int mb) {
    // 2^N slot — istenen MB'a en yakın aşağıdaki güç
    size_t bytes   = static_cast<size_t>(mb) * 1024 * 1024;
    size_t slots   = bytes / sizeof(TTEntry);

    // En yakın 2^N
    size_ = 1;
    while (size_ * 2 <= slots) size_ *= 2;

    std::free(table_);
    // aligned_alloc: önbellek hattı hizalaması (64 byte)
    table_ = static_cast<TTEntry*>(
        std::aligned_alloc(64, size_ * sizeof(TTEntry))
    );
    clear();
}

void TranspositionTable::clear() {
    std::memset(table_, 0, size_ * sizeof(TTEntry));
    age_ = 0;
}

void TranspositionTable::store(u64 hash, int depth, TTFlag flag,
                               i32 score, i32 eval, Move best_move, int ply)
{
    size_t   idx = index(hash);
    TTEntry& e   = table_[idx];

    u32 key = static_cast<u32>(hash >> 32);

    // Yazma politikası:
    // 1. Aynı pozisyon → her zaman yaz (güncel bilgi üstündür)
    // 2. Eski giriş (farklı yaş) → yaz
    // 3. Yeni derinlik eskisinden büyükse → yaz
    bool replace = (e.key16 == key)         // aynı pozisyon
                || (e.age  != age_)         // eski giriş
                || (depth + 2 > e.depth);   // daha derin

    if (!replace) return;

    // Mat skorlarını ply-bağımsız hale getir (TT'de normalize saklama)
    if (score >  SCORE_MATE - 500) score += ply;
    if (score < -SCORE_MATE + 500) score -= ply;

    e.key16 = key;
    e.score  = static_cast<i16>(score);
    e.eval   = static_cast<i16>(eval);
    e.move   = best_move;
    e.depth  = static_cast<u8>(std::max(0, depth));
    e.flag   = static_cast<u8>(flag);
    e.age    = age_;
}

const TTEntry* TranspositionTable::probe(u64 hash) const {
    size_t       idx = index(hash);
    const TTEntry& e = table_[idx];
    u32          key = static_cast<u32>(hash >> 32);

    if (e.key16 != key || !e.valid()) return nullptr;
    return &e;
}

int TranspositionTable::hashfull() const {
    // İlk 1000 slotu örnekle
    int filled = 0;
    for (int i = 0; i < 1000; i++)
        if (table_[i].valid() && table_[i].age == age_)
            filled++;
    return filled; // promil (0–1000)
}

// ─────────────────────────────────────────────────────────────────────────────
//  MoveOrderer
// ─────────────────────────────────────────────────────────────────────────────
void MoveOrderer::reset() {
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
    std::memset(counter_, 0, sizeof(counter_));
}

void MoveOrderer::add_killer(Move m, int ply) {
    if (ply >= MAX_PLY) return;
    if (m != killers_[ply][0]) {
        killers_[ply][1] = killers_[ply][0];
        killers_[ply][0] = m;
    }
}

void MoveOrderer::update_history(Move m, int depth, bool good, Color stm) {
    if (!move_ok(m)) return;
    Square from = move_from(m);
    Square to   = move_to(m);

    i32 bonus = good ? depth * depth : -(depth * depth);
    i32& h    = history_[stm][from][to];

    // Sınırlar: [-16384, +16384]
    h += bonus - h * std::abs(bonus) / 16384;
}

void MoveOrderer::update_counter(Move prev, Move response) {
    if (!move_ok(prev) || !move_ok(response)) return;
    counter_[move_from(prev)][move_to(prev)] = response;
}

// Hamle skorlama (sıralama için)
void MoveOrderer::score_moves(MoveList& ml, const Board& b,
                              Move tt_move, int ply, Color stm,
                              Move prev_move) const
{
    // MVV-LVA tablosu: saldırıcı değeri × 10 - kurban değeri
    // Küçük saldırgan, büyük kurban = yüksek skor
    auto mvv_lva = [&](Move m) -> i32 {
        Piece attacker = b.piece_on(move_from(m));
        Piece victim   = b.piece_on(move_to(m));
        if (victim == NO_PIECE) return 0;
        return PIECE_VALUE[piece_type(victim)] * 10
             - PIECE_VALUE[piece_type(attacker)];
    };

    Move killer0 = (ply < MAX_PLY) ? killers_[ply][0] : MOVE_NONE;
    Move killer1 = (ply < MAX_PLY) ? killers_[ply][1] : MOVE_NONE;

    // Counter-move: önceki hamleiyle tetiklenen yanıt hamlesi
    Move counter = MOVE_NONE;
    if (move_ok(prev_move))
        counter = counter_[move_from(prev_move)][move_to(prev_move)];

    for (auto& me : ml) {
        Move m = me.move;
        i32  s = 0;

        if (m == tt_move) {
            s = 2'000'000;  // TT hamlesi her zaman ilk
        } else {
            Piece victim = b.piece_on(move_to(m));
            if (victim != NO_PIECE) {
                // Yeme hamlesi
                i32 see_val = see(b, m);
                if (see_val >= 0)
                    s = 1'000'000 + mvv_lva(m);   // Kazançlı/nötr yeme
                else
                    s = -500'000 + mvv_lva(m);    // Kayıplı yeme (en sona)
            } else if (move_type(m) == MT_PROMO) {
                s = 900'000;  // Terfi
            } else if (m == killer0) {
                s = 800'000;
            } else if (m == killer1) {
                s = 790'000;
            } else if (counter != MOVE_NONE && m == counter) {
                s = 780'000;  // Counter-move heuristic
            } else {
                // Sakin hamle: history skoru
                s = history_[stm][move_from(m)][move_to(m)];
            }
        }

        me.score = s;
    }
}

MoveExt& MoveOrderer::next_move(MoveList& ml, int index) const {
    int best = index;
    for (int i = index + 1; i < ml.size(); i++)
        if (ml.moves[i].score > ml.moves[best].score)
            best = i;
    if (best != index)
        std::swap(ml.moves[index], ml.moves[best]);
    return ml.moves[index];
}

// Bir taşın belirli bir kareye saldırıp saldırmadığını kontrol et
// SEE'de find_lva() için kullanılır
static bool can_piece_attack(const Board& b, Square frm, PieceType pt, Square to) {
    int ff = sq_file(frm), fr = sq_rank(frm);
    int tf = sq_file(to),  tr = sq_rank(to);
    int df = tf - ff, dr = tr - fr;
    int adf = std::abs(df), adr = std::abs(dr);

    switch (pt) {
    case PIYON: {
        Color c = piece_color(b.piece_on(frm));
        int fwd = (c == WHITE) ? 1 : -1;
        return (dr == fwd && adf == 1);
    }
    case VALI:
        return (adf == 1 && adr == 0) || (adf == 0 && adr == 1);
    case FERZ:
        return (adf == 1 && adr == 1);
    case SAH:
        return (adf <= 1 && adr <= 1 && (adf + adr) > 0);
    case AT:
        return (adf == 1 && adr == 2) || (adf == 2 && adr == 1);
    case DEVE:
        return (adf == 1 && adr == 3) || (adf == 3 && adr == 1);
    case FIL:
        return (adf == 2 && adr == 2);
    case SAVAS:
    case TALIA:
        return (adf == 2 && adr == 0) || (adf == 0 && adr == 2);
    case KALE: {
        if (df != 0 && dr != 0) return false;
        int sf = (df == 0) ? 0 : (df > 0 ? 1 : -1);
        int sr = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);
        for (int f2 = ff+sf, r2 = fr+sr; !(f2 == tf && r2 == tr); f2 += sf, r2 += sr) {
            if (f2 < 0 || f2 >= BOARD_FILES || r2 < 0 || r2 >= BOARD_RANKS) return false;
            if (b.piece_on(make_sq(f2, r2)) != NO_PIECE) return false;
        }
        return true;
    }
    case ZURAFA: {
        // 1 çapraz adım (boş) + düz devam min 3 adım (1.,2. aralar boş)
        static const int ddx[] = { 1, 1,-1,-1};
        static const int ddy[] = { 1,-1, 1,-1};
        static const int sdx[] = { 1,-1, 0, 0};
        static const int sdy[] = { 0, 0, 1,-1};
        for (int d = 0; d < 4; d++) {
            int cf = ff + ddx[d], cr = fr + ddy[d];
            if (cf < 0 || cf >= BOARD_FILES || cr < 0 || cr >= BOARD_RANKS) continue;
            if (b.piece_on(make_sq(cf, cr)) != NO_PIECE) continue;
            for (int s = 0; s < 4; s++) {
                bool blocked = false;
                for (int step = 1; step <= 2; step++) {
                    int xf = cf + sdx[s]*step, xr = cr + sdy[s]*step;
                    if (xf < 0 || xf >= BOARD_FILES || xr < 0 || xr >= BOARD_RANKS)
                        { blocked = true; break; }
                    if (b.piece_on(make_sq(xf, xr)) != NO_PIECE)
                        { blocked = true; break; }
                }
                if (blocked) continue;
                for (int step = 3; ; step++) {
                    int xf = cf + sdx[s]*step, xr = cr + sdy[s]*step;
                    if (xf < 0 || xf >= BOARD_FILES || xr < 0 || xr >= BOARD_RANKS) break;
                    if (xf == tf && xr == tr) return true;
                    if (b.piece_on(make_sq(xf, xr)) != NO_PIECE) break;
                }
            }
        }
        return false;
    }
    default: return false;
    }
}

// SEE (Static Exchange Evaluation) — yeniden-yakalama zinciri
// Pozitif = kazançlı, negatif = kayıplı
i32 MoveOrderer::see(const Board& b, Move m) {
    Square to  = move_to(m);
    Square frm = move_from(m);

    Piece victim = b.piece_on(to);
    if (victim == NO_PIECE) return 0;

    // LVA sırası: en ucuz saldırıcıyı bul
    auto find_lva = [&](Color side) -> Square {
        static const PieceType order[] = {
            PIYON, VALI, FERZ, SAVAS, FIL, AT, DEVE, TALIA, KALE, ZURAFA, SAH
        };
        for (PieceType pt : order) {
            for (Square sq : b.pieces(side, pt)) {
                if (can_piece_attack(b, sq, pt, to))
                    return sq;
            }
        }
        return SQ_NONE;
    };

    i32 gain[32];
    int d = 0;

    Color side = piece_color(b.piece_on(frm));
    gain[d] = PIECE_VALUE[piece_type(victim)];
    d++;

    side = ~side;
    i32 last_attacker_val = PIECE_VALUE[piece_type(b.piece_on(frm))];

    for (int iter = 0; iter < 15 && d < 32; iter++) {
        Square lva = find_lva(side);
        if (lva == SQ_NONE) break;

        Piece lva_piece = b.piece_on(lva);
        gain[d] = last_attacker_val - gain[d-1];
        last_attacker_val = PIECE_VALUE[piece_type(lva_piece)];
        d++;
        side = ~side;
    }

    // Minimax geriye doğru hesapla
    while (--d > 0)
        gain[d-1] = std::max(-gain[d], gain[d-1]);

    return gain[0];
}

} // namespace Apex

// ════════ src/nnue.cpp ════════

/**
 * nnue.cpp — Apex Timurlenk NNUE implementasyonu
 *
 * Ağırlık dosyası formatı (networks/apex_v1.bin):
 *   [0-3]  "APEX"          — magic
 *   [4-7]  version (int32) — şu an 1
 *   [8..]  W1: NNUE_INPUT × NNUE_L1 × int16
 *          b1: NNUE_L1 × int32
 *          W2: NNUE_L1*2 × NNUE_L2 × int16
 *          b2: NNUE_L2 × int32
 *          W3: NNUE_L2 × int16
 *          b3: int32
 */


namespace Apex {

// ─── Yapıcı ──────────────────────────────────────────────────────────────────

NNUENetwork::NNUENetwork() {
    W1_.fill(0);
    b1_.fill(0);
    W2_.fill(0);
    b2_.fill(0);
    W3_.fill(0);
    b3_.fill(0);
}

// ─── Ağırlık Dosyası Yükleme ─────────────────────────────────────────────────

bool NNUENetwork::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[4];
    f.read(magic, 4);
    if (f.gcount() < 4 || std::strncmp(magic, "APEX", 4) != 0) return false;

    int32_t version;
    f.read(reinterpret_cast<char*>(&version), 4);
    if (version < 1 || version > 2) return false;

    auto read16 = [&](auto& arr) {
        f.read(reinterpret_cast<char*>(arr.data()),
               arr.size() * sizeof(int16_t));
    };
    auto read32 = [&](auto& arr) {
        f.read(reinterpret_cast<char*>(arr.data()),
               arr.size() * sizeof(int32_t));
    };

    read16(W1_); read32(b1_);
    read16(W2_); read32(b2_);
    read16(W3_); read32(b3_);

    if (!f) return false;

    // Placeholder tespiti: tüm W1_ sıfırsa ağırlık dosyası boş demektir
    bool has_nonzero = false;
    for (int i = 0; i < std::min((int)W1_.size(), 256); i++) {
        if (W1_[i] != 0) { has_nonzero = true; break; }
    }
    if (!has_nonzero) return false; // Eğitilmemiş model — NNUE bypass

    loaded_ = true;
    return true;
}

// ─── Özellik İndeksi ─────────────────────────────────────────────────────────
// perspective: hangi renkten bakıyoruz (WHITE / BLACK)
// piece_color: taşın rengi
// pt: taş tipi (SAH=1..PIYON=11, 0-tabanlı olması için -1)
// sq: kare (0..109)

int NNUENetwork::feature_index(Color perspective, Color piece_color,
                                PieceType pt, Square sq) {
    if (sq >= BOARD_NORMAL || pt == NO_PIECE_TYPE) return -1;
    // Renk perspektifine göre tahtayı çevir
    int sq_idx = (perspective == BLACK) ? (BOARD_NORMAL - 1 - sq) : sq;
    // Taş rengi: 0 = kendi, 1 = rakip
    int color_idx = (piece_color == perspective) ? 0 : 1;
    int pt_idx = static_cast<int>(pt) - 1; // 0..10
    return color_idx * NNUE_PIECE_TYPES * NNUE_SQUARES
         + pt_idx * NNUE_SQUARES
         + sq_idx;
}

// ─── İleri Besleme ───────────────────────────────────────────────────────────

i32 NNUENetwork::forward(const Accumulator& acc, Color stm) const {
    // L1 aktivasyonları: stm birikimleyicisi önce, sonra rakip
    const auto& our_acc = (stm == WHITE) ? acc.white : acc.black;
    const auto& opp_acc = (stm == WHITE) ? acc.black : acc.white;

    // Clamp(0, Q_IN) aktivasyon
    std::array<int32_t, NNUE_L1 * 2> l1{};
    for (int i = 0; i < NNUE_L1; i++) {
        l1[i]          = std::clamp(our_acc[i], 0, Q_IN);
        l1[i + NNUE_L1] = std::clamp(opp_acc[i], 0, Q_IN);
    }

    // L2: dot product
    std::array<int32_t, NNUE_L2> l2{};
    for (int o = 0; o < NNUE_L2; o++) {
        int32_t sum = b2_[o];
        for (int i = 0; i < NNUE_L1 * 2; i++)
            sum += static_cast<int32_t>(W2_[i * NNUE_L2 + o]) * l1[i];
        l2[o] = std::max(0, sum / Q_W1); // ReLU + dequantize
    }

    // L3: çıkış
    int32_t out = b3_[0];
    for (int i = 0; i < NNUE_L2; i++)
        out += static_cast<int32_t>(W3_[i]) * l2[i];

    // Centipawn'a dönüştür
    return static_cast<i32>(out * NNUE_SCALE / (Q_W1 * Q_W2 * Q_IN));
}

// ─── Tam Değerlendirme (Kaba Kuvvet) ─────────────────────────────────────────
// Akümülatör olmadan, tahta üzerinden özellikler hesaplanır

i32 NNUENetwork::evaluate(const Board& b, Color stm) const {
    if (!loaded_) return 0;

    Accumulator acc;
    acc.reset(b1_);

    // Tüm taşları tarat, her perspektife özellik ekle
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            Square sq = make_sq(f, r);
            Piece p = b.piece_on(sq);
            if (p == NO_PIECE) continue;
            PieceType pt = piece_type(p);
            Color     pc = piece_color(p);

            for (Color persp : {WHITE, BLACK}) {
                int idx = feature_index(persp, pc, pt, sq);
                if (idx < 0 || idx >= NNUE_INPUT) continue;

                auto& a = (persp == WHITE) ? acc.white : acc.black;
                for (int n = 0; n < NNUE_L1; n++)
                    a[n] += W1_[idx * NNUE_L1 + n];
            }
        }
    }

    return forward(acc, stm);
}

} // namespace Apex

// ════════ src/persona.cpp ════════


namespace Apex {

int PersonaLayer::contempt() const {
    switch (type) {
        case PersonaType::NONE:        return 0;
        case PersonaType::MACHIAVELLI: return 10 + psyche / 10;
        case PersonaType::NIETZSCHE:   return 30 + psyche / 10;
        // Türk-Altay personaları
        case PersonaType::ULGEN:       return 15 + psyche / 10;  // Zafer tanrısı — beraberliği hafif reddeder
        case PersonaType::ERLIK:       return 40 + psyche / 10;  // Karanlık güç — beraberliğe sıfır tolerans
        case PersonaType::BOZKURT:     return 20 + psyche / 10;  // Avcı — beraberliği tercih etmez
        case PersonaType::TENGRI:      return 0;                  // Tarafsız kozmik denge
        case PersonaType::DEDE_KORKUT: return  5 + psyche / 10;  // Bilge — deneyim kazancını kabul eder
        case PersonaType::UMAY:        return -10 + psyche / 10; // Koruyucu — kötü konumda beraberlik iyidir
        default:                       return 0;
    }
}

int PersonaLayer::aspiration_delta() const {
    constexpr int BASE = 25;
    // Mevcut özel modlar
    if (vezuv_protocol())   return BASE * 3;   // Nietzsche Vezüv: çok geniş
    if (erlik_dark_depth()) return BASE * 4;   // Erlik Karanlık Derinlik: mat araması
    if (necessita_mode())   return BASE / 2;   // Necessità: dar, kesin
    if (umay_shield())      return BASE / 2;   // Umay Kalkan: dar, güvenli

    switch (type) {
        case PersonaType::ULGEN:       return 20 + std::abs(psyche) / 5;  // Dar — kaçınılmaz ilerleme
        case PersonaType::ERLIK:       return 35 + std::abs(psyche) / 5;  // Geniş — saldırı kombinasyonu
        case PersonaType::BOZKURT:     return 28 + std::abs(psyche) / 5;  // Orta — reaktif tempo
        case PersonaType::TENGRI:      return BASE + std::abs(psyche) / 5; // Standart — saf hesap
        case PersonaType::DEDE_KORKUT: return 18 + std::abs(psyche) / 5;  // Dar — bilinen yolda git
        case PersonaType::UMAY:        return 15 + std::abs(psyche) / 5;  // Çok dar — güvenlik önce
        default:                       return BASE + std::abs(psyche) / 5;
    }
}

std::string PersonaLayer::commentary(int depth, i32 score) const {
    if (type == PersonaType::NONE) return "";

    switch (type) {
        case PersonaType::MACHIAVELLI:
            if (score >  150) return "Fortuna favor ediyor — ilerle.";
            if (score < -150) return "Necessità zamanı — tüm araçlar meşru.";
            if (depth >= 8)   return "Verità effettuale: gerçeği gör.";
            return "Aslan ve tilki — seç.";

        case PersonaType::NIETZSCHE:
            if (score >  150) return "Güç istenci açık — baskıla.";
            if (score < -150) return "Vezüv: her şeyi feda et, kazan.";
            if (depth >= 8)   return "Übermensch — sınırı aş.";
            return "Bengi dönüş: bu hamle ebedidir.";

        case PersonaType::ULGEN:
            if (score >  150) return "Ülgen görüyor — ışık açık, ilerle.";
            if (score < -150) return "Karanlık geçici — altın yol bulunur.";
            if (depth >= 8)   return "17. kattan baktım — hamle yazgıda.";
            return "Gök düzeni: her taşın yeri var.";

        case PersonaType::ERLIK:
            if (score >  150) return "Erlik güler — demiri çatlat, ilerle.";
            if (score < -150) return "Yeraltından bile çıkılır — saldır.";
            if (depth >= 8)   return "Demir saçlı görüyor — mat yakın.";
            return "Kaos düzeni bozar — boz ve kazan.";

        case PersonaType::BOZKURT:
            if (score >  150) return "Kurt kokuyor — av yakın, baskıya devam.";
            if (score < -150) return "Sürüden ayrılma — savun ve fırsat bekle.";
            if (depth >= 8)   return "Bozkurtun gözü: zayıf halka görünüyor.";
            return "Ötüken'den esen yel — hamle rüzgar gibi.";

        case PersonaType::TENGRI:
            if (score >  150) return "Tengri buyurdu — zafer yazgıda.";
            if (score < -150) return "Gök değişmez — yol başka yerde.";
            if (depth >= 8)   return "Kök Tengri görüyor — mavi sonsuzluktan.";
            return "Gökyüzü gibi: sınırsız, tarafsız, kesin.";

        case PersonaType::DEDE_KORKUT:
            if (score >  150) return "Dede bilirdi — bu pozisyon destanda geçer.";
            if (score < -150) return "Ozan söyledi: yenilgi de öğretir.";
            if (depth >= 8)   return "Korkut'un sazı çalıyor — derin yol açılıyor.";
            return "Her hamle bir beyit — destanı tamamla.";

        case PersonaType::UMAY:
            if (score >  150) return "Umay koruyor — taşlar yerinde, ilerliyoruz.";
            if (score < -150) return "Ana toplar — savun, yeniden dene.";
            if (depth >= 8)   return "Altın kartal görüyor: her şey yerli yerinde.";
            return "Umay'ın kanadı — taşlarını koru.";

        default: return "";
    }
}

} // namespace Apex

// ════════ src/book.cpp ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  OpeningBook::load — APEX binary format v3 okur
// ─────────────────────────────────────────────────────────────────────────────
bool OpeningBook::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    // Magic kontrolü
    char magic[4];
    f.read(magic, 4);
    if (!f || std::strncmp(magic, "APEX", 4) != 0) return false;

    // Versiyon kontrolü
    uint32_t version;
    f.read(reinterpret_cast<char*>(&version), 4);
    if (!f || version != 3) return false;

    // Kayıt sayısı
    uint32_t count;
    f.read(reinterpret_cast<char*>(&count), 4);
    if (!f) return false;

    entries_.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint64_t hash;
        f.read(reinterpret_cast<char*>(&hash), 8);
        if (!f) break;

        BookEntry e;
        uint8_t pad;
        f.read(reinterpret_cast<char*>(&e.from_sq), 1);
        f.read(reinterpret_cast<char*>(&e.to_sq),   1);
        f.read(reinterpret_cast<char*>(&e.promo),   1);
        f.read(reinterpret_cast<char*>(&pad),        1);
        f.read(reinterpret_cast<char*>(&e.weight),  1);
        if (!f) break;

        // Geçersiz kare indeksi olan kayıtları atla
        if (e.from_sq > 111 || e.to_sq > 111) continue;
        if (e.weight == 0) e.weight = 1;

        entries_[hash].push_back(e);
    }

    return !entries_.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  OpeningBook::probe — Ağırlıklı rastgele hamle seç
// ─────────────────────────────────────────────────────────────────────────────
Move OpeningBook::probe(uint64_t pos_hash) const {
    auto it = entries_.find(pos_hash);
    if (it == entries_.end()) return MOVE_NONE;

    const auto& cands = it->second;
    if (cands.empty()) return MOVE_NONE;

    // Ağırlıkları topla
    int total = 0;
    for (const auto& e : cands) total += e.weight;

    // Ağırlıklı rastgele seçim
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, total > 0 ? total - 1 : 0);
    int r = dist(rng);

    for (const auto& e : cands) {
        r -= e.weight;
        if (r < 0) {
            // from_sq ve to_sq'den basit hamle oluştur (7 bit + 7 bit)
            // Terfi varsa MT_PROMO tipini kullan
            if (e.promo != 0)
                return make_move(static_cast<Square>(e.from_sq),
                                 static_cast<Square>(e.to_sq),
                                 MT_PROMO);
            return make_move(static_cast<Square>(e.from_sq),
                             static_cast<Square>(e.to_sq));
        }
    }

    // Yedek: ilk adayı döndür
    const auto& first = cands[0];
    if (first.promo != 0)
        return make_move(static_cast<Square>(first.from_sq),
                         static_cast<Square>(first.to_sq),
                         MT_PROMO);
    return make_move(static_cast<Square>(first.from_sq),
                     static_cast<Square>(first.to_sq));
}

} // namespace Apex

// ════════ src/commentary.cpp ════════


namespace Apex {

// JSON'dan belirtilen string alanını çıkar (minimal, bağımlılıksız)
// "field": "value" ve "field":"value" her iki formatı da destekler
static std::string parse_field(const std::string& json, const std::string& field) {
    // Önce "field":"value" dene, sonra "field": "value"
    for (const auto& key : { "\"" + field + "\":\"", "\"" + field + "\": \"" }) {
        auto pos = json.find(key);
        if (pos == std::string::npos) continue;
        pos += key.size();
        auto end = json.find('"', pos);
        if (end == std::string::npos) continue;
        return json.substr(pos, end - pos);
    }
    return "";
}

static std::string parse_commentary(const std::string& json) {
    return parse_field(json, "commentary");
}

std::string CommentaryBridge::call_bridge(const std::string& script,
                                           const std::string& persona,
                                           int score, int depth) {
    // echo '{"persona":"...","score":N,"depth":N}' | python3 bridge.py
    std::ostringstream cmd;
    cmd << "echo '{\"persona\":\"" << persona
        << "\",\"score\":"         << score
        << ",\"depth\":"           << depth
        << "}' | python3 \""       << script
        << "\" 2>/dev/null";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return "";

    char buf[512] = {};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);

    return parse_commentary(output);
}

std::string CommentaryBridge::call_bridge_time(const std::string& script,
                                                const std::string& persona,
                                                int phase_score,
                                                int material_balance) {
    std::ostringstream cmd;
    cmd << "echo '{\"tool\":\"time\",\"persona\":\"" << persona
        << "\",\"phase_score\":"   << phase_score
        << ",\"material_balance\":" << material_balance
        << "}' | python3 \""        << script
        << "\" 2>/dev/null";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return "normal";

    char buf[256] = {};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);

    std::string decision = parse_field(output, "decision");
    if (decision.empty()) return "normal";
    return decision;
}

void CommentaryBridge::request_time_async(const std::string& persona,
                                           int phase_score, int material_balance,
                                           Callback cb) {
    if (script_path_.empty()) return;

    pending_.fetch_add(1);
    std::string script = script_path_;

    std::thread([this, script, persona, phase_score, material_balance,
                 cb = std::move(cb)]() {
        std::string result = call_bridge_time(script, persona,
                                              phase_score, material_balance);
        if (cb) cb(result);
        pending_.fetch_sub(1);
    }).detach();
}

std::string CommentaryBridge::call_bridge_opening(const std::string& script,
                                                   const std::string& persona,
                                                   int move_num, bool is_white) {
    std::ostringstream cmd;
    cmd << "echo '{\"tool\":\"opening\",\"persona\":\"" << persona
        << "\",\"move_num\":"  << move_num
        << ",\"color\":\""     << (is_white ? "w" : "b")
        << "\"}' | python3 \"" << script
        << "\" 2>/dev/null";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return "solid";

    char buf[256] = {};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);

    std::string style = parse_field(output, "style");
    if (style.empty()) return "solid";
    return style;
}

std::string CommentaryBridge::call_bridge_citadel(const std::string& script,
                                                   const std::string& persona,
                                                   int phase, int king_dist, int material) {
    std::ostringstream cmd;
    cmd << "echo '{\"tool\":\"citadel\",\"persona\":\"" << persona
        << "\",\"phase\":"     << phase
        << ",\"king_dist\":"   << king_dist
        << ",\"material\":"    << material
        << "}' | python3 \""   << script
        << "\" 2>/dev/null";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return "consolidate";

    char buf[256] = {};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);

    std::string strategy = parse_field(output, "strategy");
    if (strategy.empty()) return "consolidate";
    return strategy;
}

std::string CommentaryBridge::call_bridge_contempt(const std::string& script,
                                                    const std::string& persona,
                                                    int score_trend, int move_num) {
    std::ostringstream cmd;
    cmd << "echo '{\"tool\":\"contempt\",\"persona\":\"" << persona
        << "\",\"score_trend\":" << score_trend
        << ",\"move_num\":"      << move_num
        << "}' | python3 \""     << script
        << "\" 2>/dev/null";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return "hold";

    char buf[256] = {};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);

    std::string action = parse_field(output, "action");
    if (action.empty()) return "hold";
    return action;
}

void CommentaryBridge::request_opening_async(const std::string& persona,
                                              int move_num, bool is_white,
                                              Callback cb) {
    if (script_path_.empty()) return;

    pending_.fetch_add(1);
    std::string script = script_path_;

    std::thread([this, script, persona, move_num, is_white,
                 cb = std::move(cb)]() {
        std::string result = call_bridge_opening(script, persona, move_num, is_white);
        if (cb) cb(result);
        pending_.fetch_sub(1);
    }).detach();
}

void CommentaryBridge::request_citadel_async(const std::string& persona,
                                              int phase, int king_dist, int material,
                                              Callback cb) {
    if (script_path_.empty()) return;

    pending_.fetch_add(1);
    std::string script = script_path_;

    std::thread([this, script, persona, phase, king_dist, material,
                 cb = std::move(cb)]() {
        std::string result = call_bridge_citadel(script, persona, phase, king_dist, material);
        if (cb) cb(result);
        pending_.fetch_sub(1);
    }).detach();
}

void CommentaryBridge::request_contempt_async(const std::string& persona,
                                               int score_trend, int move_num,
                                               Callback cb) {
    if (script_path_.empty()) return;

    pending_.fetch_add(1);
    std::string script = script_path_;

    std::thread([this, script, persona, score_trend, move_num,
                 cb = std::move(cb)]() {
        std::string result = call_bridge_contempt(script, persona, score_trend, move_num);
        if (cb) cb(result);
        pending_.fetch_sub(1);
    }).detach();
}

void CommentaryBridge::request_async(const std::string& persona, int score,
                                      int depth, Callback cb) {
    if (script_path_.empty()) return;

    pending_.fetch_add(1);
    std::string script = script_path_;

    std::thread([this, script, persona, score, depth, cb = std::move(cb)]() {
        std::string result = call_bridge(script, persona, score, depth);
        if (!result.empty() && cb)
            cb(result);
        pending_.fetch_sub(1);
    }).detach();
}

void CommentaryBridge::wait() {
    // Async thread'lerin bitmesini bekle (en fazla 1 saniye)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (pending_.load() > 0 &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

} // namespace Apex

// ════════ src/board.cpp ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  Board — Kurucu
// ─────────────────────────────────────────────────────────────────────────────
Board::Board() : stm_(WHITE), fullmove_(1), st_(nullptr), st_idx_(0) {
    std::memset(mailbox_, 0, sizeof(mailbox_));
    std::memset(king_sq_, 0, sizeof(king_sq_));
    king_sq_[WHITE] = SQ_NONE;
    king_sq_[BLACK] = SQ_NONE;
    // StateInfo başlangıcı
    std::memset(st_buf_, 0, sizeof(st_buf_));
    st_ = &st_buf_[0];
    st_->hash       = 0;
    st_->pawn_hash  = 0;
    st_->enpassant  = SQ_NONE;
    st_->halfmove   = 0;
    st_->swap_used[WHITE] = false;
    st_->swap_used[BLACK] = false;
    st_->captured   = NO_PIECE;
    st_->last_move  = MOVE_NONE;
    st_->prev       = nullptr;
    st_->material[WHITE] = 0;
    st_->material[BLACK] = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Kopya kurucu / atama — st_ işaretçisi st_buf_'a yeniden bağlanır
// ─────────────────────────────────────────────────────────────────────────────
Board& Board::operator=(const Board& other) {
    if (this == &other) return *this;
    std::memcpy(mailbox_, other.mailbox_, sizeof(mailbox_));
    king_sq_[0] = other.king_sq_[0];
    king_sq_[1] = other.king_sq_[1];
    stm_        = other.stm_;
    fullmove_   = other.fullmove_;
    st_idx_     = other.st_idx_;
    for (int c = 0; c < 2; c++)
        for (int pt = 0; pt < PIECE_TYPE_NB; pt++)
            piece_list_[c][pt] = other.piece_list_[c][pt];
    std::memcpy(st_buf_, other.st_buf_, sizeof(st_buf_));
    // st_ işaretçisini kendi st_buf_'a yeniden bağla
    st_ = st_buf_ + (other.st_ - other.st_buf_);
    // prev zincirleri: başka Board'ın st_buf_'ını işaret etmesin
    for (int i = 0; i <= st_idx_; i++) {
        if (other.st_buf_[i].prev != nullptr) {
            ptrdiff_t offset = other.st_buf_[i].prev - other.st_buf_;
            st_buf_[i].prev = st_buf_ + offset;
        }
    }
    return *this;
}

Board::Board(const Board& other) : Board() { *this = other; }

// ─────────────────────────────────────────────────────────────────────────────
//  Yardımcı: taş koy / kaldır / taşı
// ─────────────────────────────────────────────────────────────────────────────
void Board::put_piece(Piece p, Square sq) {
    assert(sq < BOARD_SIZE);
    assert(mailbox_[sq] == NO_PIECE);
    mailbox_[sq] = p;

    PieceType pt = piece_type(p);
    Color     c  = piece_color(p);
    piece_list_[c][pt].push_back(sq);

    if (pt == SAH) king_sq_[c] = sq;

    // Materyal güncelle
    st_->material[c] += PIECE_VALUE[pt];

    // Zobrist güncelle
    st_->hash ^= Zobrist.piece[sq][pt][c];
    if (pt == PIYON) st_->pawn_hash ^= Zobrist.piece[sq][pt][c];
}

void Board::remove_piece(Square sq) {
    assert(sq < BOARD_SIZE);
    Piece p = mailbox_[sq];
    assert(p != NO_PIECE);

    PieceType pt = piece_type(p);
    Color     c  = piece_color(p);

    // piece_list'ten çıkar (swap-and-pop — O(1))
    auto& lst = piece_list_[c][pt];
    for (int i = 0; i < (int)lst.size(); i++) {
        if (lst[i] == sq) {
            lst[i] = lst.back();
            lst.pop_back();
            break;
        }
    }

    mailbox_[sq] = NO_PIECE;
    st_->material[c] -= PIECE_VALUE[pt];
    st_->hash ^= Zobrist.piece[sq][pt][c];
    if (pt == PIYON) st_->pawn_hash ^= Zobrist.piece[sq][pt][c];
}

void Board::move_piece(Square from, Square to) {
    assert(from < BOARD_SIZE && to < BOARD_SIZE);
    Piece p = mailbox_[from];
    assert(p != NO_PIECE);

    PieceType pt = piece_type(p);
    Color     c  = piece_color(p);

    // piece_list güncelle
    auto& lst = piece_list_[c][pt];
    for (auto& s : lst) {
        if (s == from) { s = to; break; }
    }

    mailbox_[from] = NO_PIECE;
    mailbox_[to]   = p;

    if (pt == SAH) king_sq_[c] = to;

    // Zobrist güncelle
    st_->hash ^= Zobrist.piece[from][pt][c];
    st_->hash ^= Zobrist.piece[to][pt][c];
    if (pt == PIYON) {
        st_->pawn_hash ^= Zobrist.piece[from][pt][c];
        st_->pawn_hash ^= Zobrist.piece[to][pt][c];
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Başlangıç Pozisyonu
//
//  Tarihsel Timurlenk düzeni (rank 0 = beyaz alt, rank 9 = siyah üst):
//
//  rank 9 (siyah ek sıra): Fil . Deve . SavaşM . SavaşM . Deve . Fil
//  rank 8 (siyah ana sıra): Kale-At-Talia-Zürafa-Vali-Şah-Ferz-Zürafa-Talia-At-Kale
//  rank 7 (siyah piyonlar): 11× Piyon
//  rank 6–3: boş
//  rank 2 (beyaz piyonlar): 11× Piyon
//  rank 1 (beyaz ana sıra): Kale-At-Talia-Zürafa-Ferz-Şah-Vali-Zürafa-Talia-At-Kale
//  rank 0 (beyaz ek sıra):  Fil . Deve . SavaşM . SavaşM . Deve . Fil
//
//  Citadel:
//    WHITE_CITADEL (110): k2 sağında (rank=1, file=11) — başlangıçta boş
//    BLACK_CITADEL (111): a9 solunda (rank=8, file=-1) — başlangıçta boş
// ─────────────────────────────────────────────────────────────────────────────
void Board::setup() {
    // Sıfırla
    std::memset(mailbox_, 0, sizeof(mailbox_));
    for (int c = 0; c < 2; c++)
        for (int pt = 0; pt < PIECE_TYPE_NB; pt++)
            piece_list_[c][pt].clear();
    king_sq_[WHITE] = king_sq_[BLACK] = SQ_NONE;
    stm_      = WHITE;
    fullmove_ = 1;
    st_idx_   = 0;
    st_       = &st_buf_[0];
    std::memset(st_buf_, 0, sizeof(st_buf_));
    st_->enpassant = SQ_NONE;
    st_->swap_used[WHITE] = st_->swap_used[BLACK] = false;

    // ── Beyaz Ek Sıra (rank 0): Fil . Deve . SavaşM . SavaşM . Deve . Fil
    // file: 0   1     2     3      4        5      6       7    8     9    10
    //       Fil  -   Deve   -    SavaşM    -    SavaşM    -   Deve   -   Fil
    static const int extra_files[]  = { 0, 2, 4, 6, 8, 10 };
    static const PieceType extra_pt[] = { FIL, DEVE, SAVAS, SAVAS, DEVE, FIL };
    for (int i = 0; i < 6; i++) {
        put_piece(make_piece(WHITE, extra_pt[i]), make_sq(extra_files[i], 0));
        put_piece(make_piece(BLACK, extra_pt[i]), make_sq(extra_files[i], 9));
    }

    // ── Beyaz Ana Sıra (rank 1)
    // Kale-At-Talia-Zürafa-Ferz-Şah-Vali-Zürafa-Talia-At-Kale
    for (int f = 0; f < BOARD_FILES; f++) {
        put_piece(make_piece(WHITE, START_RANK1_WHITE[f]), make_sq(f, 1));
    }

    // ── Siyah Ana Sıra (rank 8) — Ferz ve Vali yer değiştirmiş
    for (int f = 0; f < BOARD_FILES; f++) {
        put_piece(make_piece(BLACK, START_RANK1_BLACK[f]), make_sq(f, 8));
    }

    // ── Piyonlar (rank 2 = beyaz, rank 7 = siyah)
    for (int f = 0; f < BOARD_FILES; f++) {
        put_piece(make_piece(WHITE, PIYON), make_sq(f, 2));
        put_piece(make_piece(BLACK, PIYON), make_sq(f, 7));
    }

    // ── Zobrist: sıra beyazsa ekstra hash gerekmez (seed başlangıç değeri)
    // Siyah oynarsa Zobrist.side XOR'lanacak
}

// ─────────────────────────────────────────────────────────────────────────────
//  do_move — hamleyi uygula
// ─────────────────────────────────────────────────────────────────────────────
void Board::do_move(Move m, StateInfo& new_st, PieceType promo) {
    assert(move_ok(m));

    // StateInfo kopyala (undo için önceki durumu sakla)
    new_st        = *st_;
    new_st.prev   = st_;
    new_st.last_move = m;
    st_           = &new_st;
    st_->halfmove++;

    Square from = move_from(m);
    Square to   = move_to(m);
    MoveType mt = move_type(m);

    Piece  moving  = mailbox_[from];
    Piece  target  = (to < BOARD_NORMAL) ? mailbox_[to] : NO_PIECE;
    Color  us      = stm_;
    Color  them    = ~us;

    st_->captured = target;

    // Yakalama varsa taşı kaldır (swap hamlelerinde to'da kendi taşımız var — kaldırma!)
    if (target != NO_PIECE && mt != MT_SWAP) {
        remove_piece(to);
        st_->halfmove = 0; // Yeme → 50-hamle sayacı sıfırla
    }

    // ── Özel hamle tipleri ────────────────────────────────────────────────
    if (mt == MT_SWAP) {
        // Yer değiştirme: şah from'da, to'daki kendi taşıyla yer değiştirir
        // Swap hakkı henüz kullanılmamış olmalı
        assert(!st_->swap_used[us]);
        Piece other = mailbox_[to];
        assert(other != NO_PIECE && piece_color(other) == us);
        // Yer değiştir: şah → to, diğer taş → from
        mailbox_[from] = other;
        mailbox_[to]   = moving;
        // king_sq güncelle
        if (piece_type(moving) == SAH) king_sq_[us] = to;
        // Hash güncelle (from ve to için her iki taş)
        st_->hash ^= Zobrist.piece[from][piece_type(moving)][us];
        st_->hash ^= Zobrist.piece[to  ][piece_type(moving)][us];
        st_->hash ^= Zobrist.piece[from][piece_type(other)][us];
        st_->hash ^= Zobrist.piece[to  ][piece_type(other)][us];
        // piece_list güncelle
        for (auto& s : piece_list_[us][piece_type(moving)])
            if (s == from) { s = to;   break; }
        for (auto& s : piece_list_[us][piece_type(other)])
            if (s == to)   { s = from; break; }
        st_->swap_used[us] = true;
        st_->hash ^= Zobrist.swap_used[us];

    } else if (mt == MT_CITADEL) {
        // Citadel girişi: şah citadel'e giriyor (beraberlik)
        move_piece(from, to);

    } else if (mt == MT_PROMO) {
        PieceType pt = (promo != NO_PIECE_TYPE) ? promo : KALE;
        remove_piece(from);
        put_piece(make_piece(us, pt), to);
        st_->halfmove = 0;

    } else {
        // Normal hamle
        move_piece(from, to);
        if (piece_type(moving) == PIYON) st_->halfmove = 0;
    }

    // ── Sıra değiştir ─────────────────────────────────────────────────────
    stm_ = them;
    st_->hash ^= Zobrist.side;

    if (us == BLACK) fullmove_++;
}

// ─────────────────────────────────────────────────────────────────────────────
//  undo_move — hamleyi geri al
// ─────────────────────────────────────────────────────────────────────────────
void Board::undo_move(Move m) {
    stm_   = ~stm_;
    Color  us   = stm_;
    Square from = move_from(m);
    Square to   = move_to(m);
    MoveType mt = move_type(m);

    if (us == BLACK) fullmove_--;

    if (mt == MT_SWAP) {
        // Yer değiştirmeyi geri al
        Piece at_to   = mailbox_[to];   // şah
        Piece at_from = mailbox_[from]; // diğer taş
        mailbox_[from] = at_to;
        mailbox_[to]   = at_from;
        if (piece_type(at_to) == SAH) king_sq_[us] = from;
        for (auto& s : piece_list_[us][piece_type(at_to)])
            if (s == to) { s = from; break; }
        for (auto& s : piece_list_[us][piece_type(at_from)])
            if (s == from) { s = to; break; }

    } else if (mt == MT_PROMO) {
        // Terfiyi geri al
        remove_piece(to);
        put_piece(make_piece(us, PIYON), from);
        // Yakalanan taş varsa geri koy
        if (st_->captured != NO_PIECE)
            put_piece(st_->captured, to);

    } else {
        // Normal ve citadel hamleler
        move_piece(to, from);
        if (st_->captured != NO_PIECE)
            put_piece(st_->captured, to);
    }

    st_ = st_->prev;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Null Move (NMP için)
// ─────────────────────────────────────────────────────────────────────────────
void Board::do_null_move(StateInfo& new_st) {
    new_st        = *st_;
    new_st.prev   = st_;
    new_st.last_move = MOVE_NULL;
    new_st.captured  = NO_PIECE;
    st_           = &new_st;

    stm_          = ~stm_;
    st_->hash    ^= Zobrist.side;
    st_->halfmove++;
}

void Board::undo_null_move() {
    stm_ = ~stm_;
    st_  = st_->prev;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Saldırı Tespiti
//  is_attacked(sq, by): 'sq' karesi 'by' rengi tarafından tehdit altında mı?
// ─────────────────────────────────────────────────────────────────────────────

// Yardımcı: tahta sınırı kontrolü
static inline bool in_bounds(int f, int r) {
    return f >= 0 && f < BOARD_FILES && r >= 0 && r < BOARD_RANKS;
}

bool Board::is_attacked(Square sq, Color by) const {
    if (sq >= BOARD_NORMAL) return false; // Citadel'e saldırı yok

    // Her taş tipini sırayla kontrol et
    return attacked_by_sah   (sq, by)
        || attacked_by_kale  (sq, by)
        || attacked_by_zurafa(sq, by)
        || attacked_by_talia (sq, by)
        || attacked_by_at    (sq, by)
        || attacked_by_deve  (sq, by)
        || attacked_by_fil   (sq, by)
        || attacked_by_savas (sq, by)
        || attacked_by_ferz  (sq, by)
        || attacked_by_vali  (sq, by)
        || attacked_by_piyon (sq, by);
}

// ── Şah saldırısı: 8 yönde 1 kare ──────────────────────────────────────────
bool Board::attacked_by_sah(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static const int dy[] = {-1, 0, 1,-1, 1,-1, 0, 1};
    for (int i = 0; i < 8; i++) {
        int nf = f + dx[i], nr = r + dy[i];
        if (in_bounds(nf, nr)) {
            Square s = make_sq(nf, nr);
            Piece  p = mailbox_[s];
            if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == SAH)
                return true;
        }
    }
    return false;
}

// ── Kale saldırısı: yatay/dikey kayma ──────────────────────────────────────
bool Board::attacked_by_kale(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = { 1,-1, 0, 0};
    static const int dy[] = { 0, 0, 1,-1};
    for (int d = 0; d < 4; d++) {
        int nf = f + dx[d], nr = r + dy[d];
        while (in_bounds(nf, nr)) {
            Square s = make_sq(nf, nr);
            Piece  p = mailbox_[s];
            if (p != NO_PIECE) {
                if (piece_color(p) == by && piece_type(p) == KALE) return true;
                break; // engel
            }
            nf += dx[d]; nr += dy[d];
        }
    }
    return false;
}

// ── Zürafa saldırısı: çapraz 1 + düz min 3 ─────────────────────────────────
bool Board::attacked_by_zurafa(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int cdx[] = {1,1,-1,-1};
    static const int cdy[] = {1,-1,1,-1};
    static const int sdx[] = {1,-1,0,0};
    static const int sdy[] = {0,0,1,-1};

    for (Square zsq : piece_list_[by][ZURAFA]) {
        int zf = sq_file(zsq), zr = sq_rank(zsq);
        // Çapraz 1 adım
        for (int cd = 0; cd < 4; cd++) {
            int mf = zf + cdx[cd], mr = zr + cdy[cd];
            if (!in_bounds(mf, mr)) continue;
            if (mailbox_[make_sq(mf, mr)] != NO_PIECE) continue;
            // Düz devam
            for (int sd = 0; sd < 4; sd++) {
                bool ok = true;
                for (int step = 1; step <= 3; step++) {
                    int tf = mf + sdx[sd]*step, tr = mr + sdy[sd]*step;
                    if (!in_bounds(tf, tr)) { ok = false; break; }
                    if (step < 3 && mailbox_[make_sq(tf, tr)] != NO_PIECE) { ok = false; break; }
                    if (step == 3 && tf == f && tr == r) return true;
                }
                if (!ok) continue;
                // 3'ten fazla adım da olabilir
                for (int step = 4; ; step++) {
                    int tf = mf + sdx[sd]*step, tr = mr + sdy[sd]*step;
                    if (!in_bounds(tf, tr)) break;
                    if (tf == f && tr == r) return true;
                    if (mailbox_[make_sq(tf, tr)] != NO_PIECE) break;
                }
            }
        }
    }
    return false;
}

// ── Talia: yatay/dikey tam 2 sıçrama ────────────────────────────────────────
bool Board::attacked_by_talia(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = { 2,-2, 0, 0};
    static const int dy[] = { 0, 0, 2,-2};
    for (int i = 0; i < 4; i++) {
        int nf = f + dx[i], nr = r + dy[i];
        if (!in_bounds(nf, nr)) continue;
        Square s = make_sq(nf, nr);
        Piece  p = mailbox_[s];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == TALIA)
            return true;
    }
    return false;
}

// ── At: L şekli (2+1) ──────────────────────────────────────────────────────
bool Board::attacked_by_at(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = {2,2,-2,-2,1,1,-1,-1};
    static const int dy[] = {1,-1,1,-1,2,-2,2,-2};
    for (int i = 0; i < 8; i++) {
        int nf = f + dx[i], nr = r + dy[i];
        if (!in_bounds(nf, nr)) continue;
        Square s = make_sq(nf, nr);
        Piece  p = mailbox_[s];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == AT)
            return true;
    }
    return false;
}

// ── Deve: (1,3) atlama ──────────────────────────────────────────────────────
bool Board::attacked_by_deve(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = {3,3,-3,-3,1,1,-1,-1};
    static const int dy[] = {1,-1,1,-1,3,-3,3,-3};
    for (int i = 0; i < 8; i++) {
        int nf = f + dx[i], nr = r + dy[i];
        if (!in_bounds(nf, nr)) continue;
        Square s = make_sq(nf, nr);
        Piece  p = mailbox_[s];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == DEVE)
            return true;
    }
    return false;
}

// ── Fil: çapraz 2 sıçrama ───────────────────────────────────────────────────
bool Board::attacked_by_fil(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = { 2,-2, 2,-2};
    static const int dy[] = { 2, 2,-2,-2};
    for (int i = 0; i < 4; i++) {
        int nf = f + dx[i], nr = r + dy[i];
        if (!in_bounds(nf, nr)) continue;
        Square s = make_sq(nf, nr);
        Piece  p = mailbox_[s];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == FIL)
            return true;
    }
    return false;
}

// ── Savaş Makinesi: yatay/dikey 2 sıçrama ───────────────────────────────────
bool Board::attacked_by_savas(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = { 2,-2, 0, 0};
    static const int dy[] = { 0, 0, 2,-2};
    for (int i = 0; i < 4; i++) {
        int nf = f + dx[i], nr = r + dy[i];
        if (!in_bounds(nf, nr)) continue;
        Square s = make_sq(nf, nr);
        Piece  p = mailbox_[s];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == SAVAS)
            return true;
    }
    return false;
}

// ── Ferz: çapraz 1 kare ─────────────────────────────────────────────────────
bool Board::attacked_by_ferz(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = { 1,-1, 1,-1};
    static const int dy[] = { 1, 1,-1,-1};
    for (int i = 0; i < 4; i++) {
        int nf = f + dx[i], nr = r + dy[i];
        if (!in_bounds(nf, nr)) continue;
        Square s = make_sq(nf, nr);
        Piece  p = mailbox_[s];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == FERZ)
            return true;
    }
    return false;
}

// ── Vali: yatay/dikey 1 kare ────────────────────────────────────────────────
bool Board::attacked_by_vali(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    static const int dx[] = { 1,-1, 0, 0};
    static const int dy[] = { 0, 0, 1,-1};
    for (int i = 0; i < 4; i++) {
        int nf = f + dx[i], nr = r + dy[i];
        if (!in_bounds(nf, nr)) continue;
        Square s = make_sq(nf, nr);
        Piece  p = mailbox_[s];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == VALI)
            return true;
    }
    return false;
}

// ── Piyon saldırısı: çapraz ileri yeme ──────────────────────────────────────
bool Board::attacked_by_piyon(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);
    // Beyaz piyon: rank+1 yönünde gider, yani rank-1'den saldırır
    // Siyah piyon: rank-1 yönünde gider, yani rank+1'den saldırır
    int attack_rank = (by == WHITE) ? r - 1 : r + 1;
    if (!in_bounds(f-1, attack_rank)) {
        // sol kontrol
    } else {
        Piece p = mailbox_[make_sq(f-1, attack_rank)];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == PIYON)
            return true;
    }
    if (!in_bounds(f+1, attack_rank)) {
        // sağ kontrol
    } else {
        Piece p = mailbox_[make_sq(f+1, attack_rank)];
        if (p != NO_PIECE && piece_color(p) == by && piece_type(p) == PIYON)
            return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Özel Kontroller
// ─────────────────────────────────────────────────────────────────────────────
bool Board::is_citadel_move(Move m) const {
    return move_type(m) == MT_CITADEL;
}

bool Board::is_repetition(int count) const {
    int reps = 0;
    const StateInfo* s = st_->prev;
    u64 h = st_->hash;
    while (s && s->prev) {
        if (s->hash == h) {
            if (++reps >= count - 1) return true;
        }
        if (s->halfmove == 0) break; // yeme/piyon → tekrar mümkün değil
        s = s->prev->prev; // Her 2 hamlede bir kontrol (aynı sıra)
    }
    return false;
}

static bool has_mating_material(const Board& b, Color c) {
    // Şah + Ferz veya Vali dışında herhangi bir taş varsa mat mümkün
    for (int pt = KALE; pt <= PIYON; pt++) {
        if (pt == FERZ || pt == VALI) continue;
        if (!b.pieces(c, (PieceType)pt).empty()) return true;
    }
    // Sadece Ferz veya Vali ile mat zor ama mümkün — mat materyali var say
    if (!b.pieces(c, FERZ).empty()) return true;
    if (!b.pieces(c, VALI).empty()) return true;
    return false;
}

Board::GameResult Board::game_over() const {
    // Tekrar
    if (is_repetition(3)) return DRAW;
    // 50-hamle kuralı
    if (st_->halfmove >= 100) return DRAW;
    // Citadel beraberliği
    if (king_sq_[WHITE] == BLACK_CITADEL) return DRAW;
    if (king_sq_[BLACK] == WHITE_CITADEL) return DRAW;
    // Yetersiz materyal: her iki tarafta da sadece şah
    bool w_mat = has_mating_material(*this, WHITE);
    bool b_mat = has_mating_material(*this, BLACK);
    if (!w_mat && !b_mat) return DRAW;
    // Mat/Pat — hamle üretici gerekir, search tarafından kontrol edilir
    return ONGOING;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Debug Çıktısı
// ─────────────────────────────────────────────────────────────────────────────
std::string Board::to_string() const {
    std::ostringstream oss;
    // Taş sembolleri
    auto piece_char = [](Piece p) -> char {
        if (p == NO_PIECE) return '.';
        PieceType pt = piece_type(p);
        Color c      = piece_color(p);
        char ch = (pt == SAVAS) ? 'W' : (pt <= PIYON ? "KRZTNCEWFVP"[pt-1] : '?');
        return (c == BLACK) ? (char)(ch + 32) : ch;
    };

    oss << "\n   +";
    for (int f = 0; f < BOARD_FILES; f++) oss << "----";
    oss << "-+    [Citadel]\n";

    for (int r = BOARD_RANKS - 1; r >= 0; r--) {
        oss << std::setw(2) << (r+1) << " |";
        for (int f = 0; f < BOARD_FILES; f++) {
            Piece p = mailbox_[make_sq(f, r)];
            oss << "  " << piece_char(p) << " ";
        }
        oss << " |";
        if (r == 8) oss << "  [←BC] Black Citadel";
        if (r == 1) oss << "  White Citadel [→WC]";
        oss << "\n";
    }

    oss << "   +";
    for (int f = 0; f < BOARD_FILES; f++) oss << "----";
    oss << "-+\n     ";
    for (int f = 0; f < BOARD_FILES; f++) {
        oss << "  " << (char)('a' + f) << " ";
    }
    oss << "\n";
    oss << "Sıra: " << (stm_ == WHITE ? "Beyaz" : "Siyah");
    oss << "  |  Hash: " << std::hex << st_->hash << std::dec;
    oss << "  |  Hamle: " << fullmove_;
    oss << "  |  50-hamle: " << (int)st_->halfmove;
    oss << "\n";
    oss << "Swap: Beyaz=" << (st_->swap_used[WHITE] ? "kullandı" : "var")
        << "  Siyah=" << (st_->swap_used[BLACK] ? "kullandı" : "var") << "\n";
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  FEN Parsing — Timurlenk FEN Format:
//  <board> <renk> <swap> <ep> <halfmove> <fullmove>
//  Taş sembolleri: K=Şah R=Kale Z=Zürafa T=Talia N=At C=Deve
//                  E=Fil W=SavaşM F=Ferz V=Vali P=Piyon
//  Board: rank10/rank9/.../rank1  (boşluk=sayı)
//  Swap: "-" yok, "w" beyaz hakkı var, "b" siyah hakkı var
// ─────────────────────────────────────────────────────────────────────────────
bool Board::from_string(const std::string& s) {
    std::istringstream ss(s);
    std::string board_str, color_str, swap_str, ep_str;
    int halfmove = 0, fullmove = 1;

    if (!(ss >> board_str >> color_str >> swap_str >> ep_str >> halfmove >> fullmove))
        return false;

    // Tahtayı sıfırla
    for (int sq = 0; sq < BOARD_SIZE; sq++) mailbox_[sq] = NO_PIECE;
    for (auto& vv : piece_list_) for (auto& v : vv) v.clear();
    king_sq_[WHITE] = SQ_NONE;
    king_sq_[BLACK] = SQ_NONE;

    auto char_to_pt = [](char c) -> PieceType {
        switch (std::toupper((unsigned char)c)) {
            case 'K': return SAH;   case 'R': return KALE;  case 'Z': return ZURAFA;
            case 'T': return TALIA; case 'N': return AT;    case 'C': return DEVE;
            case 'E': return FIL;   case 'W': return SAVAS; case 'F': return FERZ;
            case 'V': return VALI;  case 'P': return PIYON;
            default:  return NO_PIECE_TYPE;
        }
    };

    // Board: rank10/rank9/.../rank1
    int rank = BOARD_RANKS - 1;
    int file = 0;
    for (char ch : board_str) {
        if (ch == '/') { rank--; file = 0; continue; }
        if (rank < 0) break;
        if (std::isdigit((unsigned char)ch)) {
            file += (ch - '0');
        } else {
            PieceType pt = char_to_pt(ch);
            if (pt == NO_PIECE_TYPE) return false;
            if (file >= BOARD_FILES) return false;
            Color c = std::isupper((unsigned char)ch) ? WHITE : BLACK;
            put_piece(make_piece(c, pt), make_sq(file, rank));
            file++;
        }
    }

    stm_ = (color_str == "b") ? BLACK : WHITE;

    st_idx_ = 0;
    st_ = &st_buf_[0];
    std::memset(st_, 0, sizeof(StateInfo));
    st_->enpassant        = SQ_NONE;
    st_->halfmove         = static_cast<u8>(halfmove);
    st_->swap_used[WHITE] = (swap_str.find('w') == std::string::npos);
    st_->swap_used[BLACK] = (swap_str.find('b') == std::string::npos);
    st_->captured         = NO_PIECE;
    st_->last_move        = MOVE_NONE;
    st_->prev             = nullptr;

    if (ep_str != "-" && ep_str.size() >= 2) {
        int ef = ep_str[0] - 'a';
        int er = std::stoi(ep_str.substr(1)) - 1;
        if (ef >= 0 && ef < BOARD_FILES && er >= 0 && er < BOARD_RANKS)
            st_->enpassant = make_sq(ef, er);
    }

    st_->material[WHITE] = st_->material[BLACK] = 0;
    for (int sq = 0; sq < BOARD_NORMAL; sq++) {
        Piece p = mailbox_[sq];
        if (p != NO_PIECE)
            st_->material[piece_color(p)] += PIECE_VALUE[piece_type(p)];
    }

    // Zobrist hash hesapla
    st_->hash = st_->pawn_hash = 0;
    for (int sq = 0; sq < BOARD_SIZE; sq++) {
        Piece p = mailbox_[sq];
        if (p != NO_PIECE) {
            Color c = piece_color(p); PieceType pt = piece_type(p);
            st_->hash ^= Zobrist.piece[sq][pt][c];
            if (pt == PIYON) st_->pawn_hash ^= Zobrist.piece[sq][pt][c];
        }
    }
    if (stm_ == BLACK)              st_->hash ^= Zobrist.side;
    if (st_->swap_used[WHITE])      st_->hash ^= Zobrist.swap_used[WHITE];
    if (st_->swap_used[BLACK])      st_->hash ^= Zobrist.swap_used[BLACK];

    fullmove_ = fullmove;
    return true;
}

std::string Board::to_fen() const {
    std::ostringstream oss;
    auto pt_to_char = [](PieceType pt, Color c) -> char {
        char ch = '?';
        switch (pt) {
            case SAH:    ch = 'K'; break; case KALE:   ch = 'R'; break;
            case ZURAFA: ch = 'Z'; break; case TALIA:  ch = 'T'; break;
            case AT:     ch = 'N'; break; case DEVE:   ch = 'C'; break;
            case FIL:    ch = 'E'; break; case SAVAS:  ch = 'W'; break;
            case FERZ:   ch = 'F'; break; case VALI:   ch = 'V'; break;
            case PIYON:  ch = 'P'; break; default:     ch = '?'; break;
        }
        return (c == BLACK) ? (char)std::tolower((unsigned char)ch) : ch;
    };

    for (int r = BOARD_RANKS - 1; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < BOARD_FILES; f++) {
            Piece p = mailbox_[make_sq(f, r)];
            if (p == NO_PIECE) { empty++; }
            else {
                if (empty > 0) { oss << empty; empty = 0; }
                oss << pt_to_char(piece_type(p), piece_color(p));
            }
        }
        if (empty > 0) oss << empty;
        if (r > 0) oss << '/';
    }

    oss << ' ' << (stm_ == WHITE ? 'w' : 'b');

    std::string sw;
    if (!st_->swap_used[WHITE]) sw += 'w';
    if (!st_->swap_used[BLACK]) sw += 'b';
    oss << ' ' << (sw.empty() ? "-" : sw);

    Square ep = st_->enpassant;
    if (ep == SQ_NONE) oss << " -";
    else oss << ' ' << (char)('a' + sq_file(ep)) << (sq_rank(ep) + 1);

    oss << ' ' << (int)st_->halfmove << ' ' << fullmove_;
    return oss.str();
}

} // namespace Apex

namespace Apex {

bool Board::gives_check(Move m) const {
    // Hamle yapıldıktan sonra rakip şahı tehdit altına alıyor mu?
    Board& self = const_cast<Board&>(*this);
    // Arama stack'inde değil, yerel buffer kullan
    static thread_local StateInfo gc_st;
    self.do_move(m, gc_st);
    Color them = self.stm_; // do_move sonrası sıra değişti
    bool check = self.is_attacked(self.king_sq(them), ~them);
    self.undo_move(m);
    return check;
}

} // namespace Apex

// ════════ src/movegen.cpp ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  Yardımcılar
// ─────────────────────────────────────────────────────────────────────────────

bool MoveGenerator::can_go(const Board& b, Color c, int file, int rank) {
    if (!in_bounds(file, rank)) return false;
    Square s = make_sq(file, rank);
    Piece  p = b.piece_on(s);
    return p == NO_PIECE || piece_color(p) != c;
}

void MoveGenerator::slide(const Board& b, Square from, Color c,
                          int dx, int dy, MoveList& ml)
{
    int f = sq_file(from) + dx;
    int r = sq_rank(from) + dy;
    while (in_bounds(f, r)) {
        Square to = make_sq(f, r);
        Piece  p  = b.piece_on(to);
        if (p == NO_PIECE) {
            ml.push(make_move(from, to));
        } else {
            if (piece_color(p) != c) ml.push(make_move(from, to)); // yeme
            break; // engel
        }
        f += dx; r += dy;
    }
}

void MoveGenerator::jump(const Board& b, Square from, Color c,
                         int dx, int dy, MoveList& ml)
{
    int f = sq_file(from) + dx;
    int r = sq_rank(from) + dy;
    if (!in_bounds(f, r)) return;
    Square to = make_sq(f, r);
    Piece  p  = b.piece_on(to);
    if (p == NO_PIECE || piece_color(p) != c)
        ml.push(make_move(from, to));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Taş Bazlı Üreticiler
// ─────────────────────────────────────────────────────────────────────────────

// ── Şah ─────────────────────────────────────────────────────────────────────
void MoveGenerator::gen_sah(const Board& b, Square from, Color c, MoveList& ml)
{
    static const int dx[] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static const int dy[] = {-1, 0, 1,-1, 1,-1, 0, 1};
    for (int i = 0; i < 8; i++)
        jump(b, from, c, dx[i], dy[i], ml);

    // Citadel girişi — rakibin kalesine komşuysa
    // Beyaz şah: BLACK_CITADEL'e girebilir (a9 = sq 88 komşusuysa)
    // Siyah şah: WHITE_CITADEL'e girebilir (k2 = sq 21 komşusuysa)
    if (c == WHITE && from == BLACK_CITADEL_NEIGHBOR) {
        ml.push(make_move(from, BLACK_CITADEL, MT_CITADEL));
    }
    if (c == BLACK && from == WHITE_CITADEL_NEIGHBOR) {
        ml.push(make_move(from, WHITE_CITADEL, MT_CITADEL));
    }
}

// ── Kale ─────────────────────────────────────────────────────────────────────
void MoveGenerator::gen_kale(const Board& b, Square from, Color c, MoveList& ml)
{
    slide(b, from, c,  1,  0, ml);
    slide(b, from, c, -1,  0, ml);
    slide(b, from, c,  0,  1, ml);
    slide(b, from, c,  0, -1, ml);
}

// ── Zürafa (Gryphon): çapraz 1 + düz min 3 ──────────────────────────────────
void MoveGenerator::zurafa_ray(const Board& b, Square from, Color c,
                                int df, int dr, MoveList& ml)
{
    int f0 = sq_file(from), r0 = sq_rank(from);

    // Çapraz 1 adım
    int cf = f0 + df, cr = r0 + dr;
    if (!in_bounds(cf, cr)) return;
    if (b.piece_on(make_sq(cf, cr)) != NO_PIECE) return; // çapraz kare dolu

    // 4 düz yön × minimum 3 adım devam
    static const int sdx[] = { 1,-1, 0, 0};
    static const int sdy[] = { 0, 0, 1,-1};

    for (int s = 0; s < 4; s++) {
        // Ara kareler (adım 1,2) boş olmalı; adım 3'ten itibaren hedef
        bool blocked = false;
        for (int step = 1; ; step++) {
            int tf = cf + sdx[s]*step, tr = cr + sdy[s]*step;
            if (!in_bounds(tf, tr)) break;
            Square to = make_sq(tf, tr);
            Piece  p  = b.piece_on(to);

            if (step < 3) {
                // Ara kare: boş olmalı
                if (p != NO_PIECE) { blocked = true; break; }
            } else {
                // Hedef kare: boş veya düşman
                if (p == NO_PIECE) {
                    ml.push(make_move(from, to));
                } else {
                    if (piece_color(p) != c) ml.push(make_move(from, to));
                    break; // engel
                }
            }
        }
        if (blocked) continue;
    }
}

void MoveGenerator::gen_zurafa(const Board& b, Square from, Color c, MoveList& ml)
{
    zurafa_ray(b, from, c,  1,  1, ml);
    zurafa_ray(b, from, c,  1, -1, ml);
    zurafa_ray(b, from, c, -1,  1, ml);
    zurafa_ray(b, from, c, -1, -1, ml);
}

// ── Talia: yatay/dikey tam 2 sıçrama (atlayabilir) ──────────────────────────
void MoveGenerator::gen_talia(const Board& b, Square from, Color c, MoveList& ml)
{
    jump(b, from, c,  2,  0, ml);
    jump(b, from, c, -2,  0, ml);
    jump(b, from, c,  0,  2, ml);
    jump(b, from, c,  0, -2, ml);
}

// ── At: L şekli 2+1 (atlayabilir) ───────────────────────────────────────────
void MoveGenerator::gen_at(const Board& b, Square from, Color c, MoveList& ml)
{
    static const int dx[] = { 2, 2,-2,-2, 1, 1,-1,-1};
    static const int dy[] = { 1,-1, 1,-1, 2,-2, 2,-2};
    for (int i = 0; i < 8; i++)
        jump(b, from, c, dx[i], dy[i], ml);
}

// ── Deve: 3+1 atlama (atlayabilir) ──────────────────────────────────────────
void MoveGenerator::gen_deve(const Board& b, Square from, Color c, MoveList& ml)
{
    static const int dx[] = { 3, 3,-3,-3, 1, 1,-1,-1};
    static const int dy[] = { 1,-1, 1,-1, 3,-3, 3,-3};
    for (int i = 0; i < 8; i++)
        jump(b, from, c, dx[i], dy[i], ml);
}

// ── Fil: çapraz 2 sıçrama (atlayabilir) ─────────────────────────────────────
void MoveGenerator::gen_fil(const Board& b, Square from, Color c, MoveList& ml)
{
    jump(b, from, c,  2,  2, ml);
    jump(b, from, c,  2, -2, ml);
    jump(b, from, c, -2,  2, ml);
    jump(b, from, c, -2, -2, ml);
}

// ── Savaş Makinesi: yatay/dikey 2 sıçrama (atlayabilir) ─────────────────────
void MoveGenerator::gen_savas(const Board& b, Square from, Color c, MoveList& ml)
{
    jump(b, from, c,  2,  0, ml);
    jump(b, from, c, -2,  0, ml);
    jump(b, from, c,  0,  2, ml);
    jump(b, from, c,  0, -2, ml);
}

// ── Ferz: çapraz 1 kare ──────────────────────────────────────────────────────
void MoveGenerator::gen_ferz(const Board& b, Square from, Color c, MoveList& ml)
{
    jump(b, from, c,  1,  1, ml);
    jump(b, from, c,  1, -1, ml);
    jump(b, from, c, -1,  1, ml);
    jump(b, from, c, -1, -1, ml);
}

// ── Vali: yatay/dikey 1 kare ─────────────────────────────────────────────────
void MoveGenerator::gen_vali(const Board& b, Square from, Color c, MoveList& ml)
{
    jump(b, from, c,  1,  0, ml);
    jump(b, from, c, -1,  0, ml);
    jump(b, from, c,  0,  1, ml);
    jump(b, from, c,  0, -1, ml);
}

// ── Piyon ────────────────────────────────────────────────────────────────────
void MoveGenerator::gen_piyon(const Board& b, Square from, Color c, MoveList& ml)
{
    int f  = sq_file(from);
    int r  = sq_rank(from);
    int dr = (c == WHITE) ? 1 : -1; // ilerleme yönü
    int promo_rank = (c == WHITE) ? BOARD_RANKS - 1 : 0;

    // İleri 1 adım (çift adım YOK — tarihsel kural)
    int nf = f, nr = r + dr;
    if (in_bounds(nf, nr)) {
        Square to = make_sq(nf, nr);
        if (b.piece_on(to) == NO_PIECE) {
            if (nr == promo_rank) {
                // Terfi: Kale, Zürafa, Talia, At, Deve, Fil, SavaşM, Ferz, Vali'ye
                static const PieceType promo_opts[] = {
                    KALE, ZURAFA, TALIA, AT, DEVE, FIL, SAVAS, FERZ, VALI
                };
                for (PieceType pt : promo_opts)
                    ml.push(make_move(from, to, MT_PROMO), pt);
            } else {
                ml.push(make_move(from, to));
            }
        }
    }

    // Çapraz yeme
    static const int captures[] = {-1, 1};
    for (int dcf : captures) {
        int cf = f + dcf, cr = r + dr;
        if (!in_bounds(cf, cr)) continue;
        Square to = make_sq(cf, cr);
        Piece  p  = b.piece_on(to);
        if (p != NO_PIECE && piece_color(p) != c) {
            if (cr == promo_rank) {
                static const PieceType promo_opts[] = {
                    KALE, ZURAFA, TALIA, AT, DEVE, FIL, SAVAS, FERZ, VALI
                };
                for (PieceType pt : promo_opts)
                    ml.push(make_move(from, to, MT_PROMO), pt);
            } else {
                ml.push(make_move(from, to));
            }
        }
    }
}

// ── Swap (Yer Değiştirme) ────────────────────────────────────────────────────
void MoveGenerator::gen_swap(const Board& b, Color c, MoveList& ml)
{
    if (b.swap_used(c)) return; // Hak kullanıldı
    if (b.in_check()) return;   // Şah altında swap yasak

    Square ksq = b.king_sq(c);
    if (ksq == SQ_NONE) return;

    // Tüm tahtayı tara
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            Square sq = make_sq(f, r);
            if (sq == ksq) continue;
            Piece p = b.piece_on(sq);
            if (p == NO_PIECE || piece_color(p) != c) continue;
            // Şah ile bu taş yer değiştirecek
            ml.push(make_move(ksq, sq, MT_SWAP));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ana Üretim Fonksiyonları
// ─────────────────────────────────────────────────────────────────────────────
void MoveGenerator::generate_pseudo(const Board& b, MoveList& ml)
{
    Color c = b.side_to_move();

    for (int rank = 0; rank < BOARD_RANKS; rank++) {
        for (int file = 0; file < BOARD_FILES; file++) {
            Square from = make_sq(file, rank);
            Piece  p    = b.piece_on(from);
            if (p == NO_PIECE || piece_color(p) != c) continue;

            switch (piece_type(p)) {
                case SAH:    gen_sah   (b, from, c, ml); break;
                case KALE:   gen_kale  (b, from, c, ml); break;
                case ZURAFA: gen_zurafa(b, from, c, ml); break;
                case TALIA:  gen_talia (b, from, c, ml); break;
                case AT:     gen_at    (b, from, c, ml); break;
                case DEVE:   gen_deve  (b, from, c, ml); break;
                case FIL:    gen_fil   (b, from, c, ml); break;
                case SAVAS:  gen_savas (b, from, c, ml); break;
                case FERZ:   gen_ferz  (b, from, c, ml); break;
                case VALI:   gen_vali  (b, from, c, ml); break;
                case PIYON:  gen_piyon (b, from, c, ml); break;
                default: break;
            }
        }
    }

    // Swap hamlesi
    gen_swap(b, c, ml);
}

void MoveGenerator::generate_legal(const Board& b, MoveList& ml)
{
    Board& board = const_cast<Board&>(b); // do_move için
    MoveList pseudo;
    generate_pseudo(b, pseudo);

    Color c = b.side_to_move();
    for (auto& me : pseudo) {
        StateInfo st;
        board.do_move(me.move, st);
        // Hamleden sonra kendi şahımız tehdit altında değilse yasal
        if (!board.is_attacked(board.king_sq(c), ~c)) {
            ml.push(me.move, me.promo);
        }
        board.undo_move(me.move);
    }
}

void MoveGenerator::generate_captures(const Board& b, MoveList& ml)
{
    Color c = b.side_to_move();

    for (int rank = 0; rank < BOARD_RANKS; rank++) {
        for (int file = 0; file < BOARD_FILES; file++) {
            Square from = make_sq(file, rank);
            Piece  p    = b.piece_on(from);
            if (p == NO_PIECE || piece_color(p) != c) continue;

            // Her taş için sadece yeme hamlelerini üret
            // (Basit yöntem: tüm hamleler üret, yeme olmayanları filtrele)
            MoveList all;
            switch (piece_type(p)) {
                case SAH:    gen_sah   (b, from, c, all); break;
                case KALE:   gen_kale  (b, from, c, all); break;
                case ZURAFA: gen_zurafa(b, from, c, all); break;
                case TALIA:  gen_talia (b, from, c, all); break;
                case AT:     gen_at    (b, from, c, all); break;
                case DEVE:   gen_deve  (b, from, c, all); break;
                case FIL:    gen_fil   (b, from, c, all); break;
                case SAVAS:  gen_savas (b, from, c, all); break;
                case FERZ:   gen_ferz  (b, from, c, all); break;
                case VALI:   gen_vali  (b, from, c, all); break;
                case PIYON:  gen_piyon (b, from, c, all); break;
                default: break;
            }
            for (auto& me : all) {
                Square to = move_to(me.move);
                if ((b.piece_on(to) != NO_PIECE && move_type(me.move) != MT_SWAP)
                    || move_type(me.move) == MT_PROMO)
                    ml.push(me.move, me.promo);
            }
        }
    }
}

void MoveGenerator::generate_evasions(const Board& b, MoveList& ml)
{
    // Şah altındayken sadece şahı kurtaran hamleler
    // 1. Şahı hareket ettir
    Square ksq = b.king_sq(b.side_to_move());
    Color  c   = b.side_to_move();
    gen_sah(b, ksq, c, ml);

    // 2. Tüm yasal hamleleri üret (şah bloğu veya saldırıyı kesme)
    // Basit implementasyon: generate_legal kullan
    MoveList all;
    generate_legal(b, all);
    for (auto& me : all)
        if (move_from(me.move) != ksq) // Şah dışı hamleler
            ml.push(me.move, me.promo);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mobilite Sayısı (değerlendirici için)
// ─────────────────────────────────────────────────────────────────────────────
int MoveGenerator::mobility(const Board& b, Color c)
{
    MoveList ml;
    // Pseudo-legal sayımı (hız için)
    Color saved = b.side_to_move();
    (void)saved;
    // Board const olduğu için doğrudan üretemiyoruz
    // Bu çağrı evaluate.cpp'den gelir — board const değil
    generate_pseudo(b, ml);
    return ml.count;
}

// ─────────────────────────────────────────────────────────────────────────────
//  LMR Tablosu
// ─────────────────────────────────────────────────────────────────────────────
int LMR_TABLE[MAX_PLY][MAX_MOVES];

void init_lmr_table() {
    for (int d = 1; d < MAX_PLY; d++) {
        for (int m = 1; m < MAX_MOVES; m++) {
            // 53 dallanma faktörü için daha agresif LMR
            double r = std::log(d) * std::log(m) / 1.25;
            LMR_TABLE[d][m] = std::max(0, static_cast<int>(r));
        }
    }
    // Derinlik 0-1 için azaltma yok
    for (int m = 0; m < MAX_MOVES; m++)
        LMR_TABLE[0][m] = LMR_TABLE[1][m] = 0;
}

} // namespace Apex

// ════════ src/evaluate.cpp ════════


namespace Apex {

PSTTable PST[PIECE_TYPE_NB];
i32 KING_SAFETY_TABLE[64];

// ── PST Başlatma ──────────────────────────────────────────────────────────
// İndeks: rank * 11 + file (beyaz perspektifi, rank 0 = beyaz başlangıç)
void init_pst() {
    std::memset(PST, 0, sizeof(PST));

    // Kral güvenlik tablosu: saldırı birimi → ceza (quadratic)
    for (int i = 0; i < 64; i++)
        KING_SAFETY_TABLE[i] = (i32)std::min(i * i * 4, 500);

    // ── Piyon PST ────────────────────────────────────────────────────────
    // Rank bonusu: ilerleme ödüllendirilir
    static const i16 pawn_rank_mg[10] = { 0, 0, 5, 10, 15, 25, 35, 50, 70, 0 };
    static const i16 pawn_rank_eg[10] = { 0, 0, 8, 15, 22, 35, 55, 80,110, 0 };
    // Merkez file bonusu (file 3-7)
    static const i16 pawn_file_mg[11] = { -5,-3, 0, 3, 5, 7, 5, 3, 0,-3,-5 };
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            PST[PIYON].mg[idx] = pawn_rank_mg[r] + pawn_file_mg[f];
            PST[PIYON].eg[idx] = pawn_rank_eg[r];
        }
    }

    // ── At PST ───────────────────────────────────────────────────────────
    // Merkez (file 3-7, rank 2-7) tercih edilir; köşeler ceza alır
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            int cf = f - 5, cr = r - 4; // Merkeze uzaklık (file 5, rank 4 merkez)
            int dist = std::abs(cf) + std::abs(cr);
            i16 val_mg = (i16)std::max(-30, 25 - dist * 8);
            i16 val_eg = (i16)std::max(-25, 20 - dist * 7);
            PST[AT].mg[idx] = val_mg;
            PST[AT].eg[idx] = val_eg;
        }
    }

    // ── Deve PST ─────────────────────────────────────────────────────────
    // Renk bağımsız; merkez tercih ama daha az yoğun
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            int cf = f - 5, cr = r - 4;
            int dist = std::abs(cf) + std::abs(cr);
            PST[DEVE].mg[idx] = (i16)std::max(-20, 18 - dist * 6);
            PST[DEVE].eg[idx] = (i16)std::max(-18, 15 - dist * 5);
        }
    }

    // ── Fil PST ──────────────────────────────────────────────────────────
    // Renk bağımlı; tek renk kare kalır; açık pozisyon tercih
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            int cf = f - 5, cr = r - 4;
            int dist = std::abs(cf) + std::abs(cr);
            PST[FIL].mg[idx] = (i16)std::max(-15, 12 - dist * 5);
            PST[FIL].eg[idx] = (i16)std::max(-12, 10 - dist * 4);
        }
    }

    // ── Savaş Makinesi PST ────────────────────────────────────────────────
    // Atlayan taş; merkezi tercih eder
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            int cf = f - 5, cr = r - 4;
            int dist = std::abs(cf) + std::abs(cr);
            PST[SAVAS].mg[idx] = (i16)std::max(-12, 10 - dist * 4);
            PST[SAVAS].eg[idx] = (i16)std::max(-10,  8 - dist * 3);
        }
    }

    // ── Ferz PST ─────────────────────────────────────────────────────────
    // Küçük taş; kenar ceza, orta hafif bonus
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            bool edge = (f == 0 || f == 10 || r == 0 || r == 9);
            PST[FERZ].mg[idx] = edge ? -5 : 3;
            PST[FERZ].eg[idx] = edge ? -3 : 2;
        }
    }

    // ── Vali PST ─────────────────────────────────────────────────────────
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            bool edge = (f == 0 || f == 10 || r == 0 || r == 9);
            PST[VALI].mg[idx] = edge ? -4 : 2;
            PST[VALI].eg[idx] = edge ? -3 : 2;
        }
    }

    // ── Talia PST ────────────────────────────────────────────────────────
    // Atlayan taş (talia), merkez tercih
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            int cf = f - 5, cr = r - 4;
            int dist = std::abs(cf) + std::abs(cr);
            PST[TALIA].mg[idx] = (i16)std::max(-10, 8 - dist * 3);
            PST[TALIA].eg[idx] = (i16)std::max( -8, 6 - dist * 3);
        }
    }

    // ── Kale PST ─────────────────────────────────────────────────────────
    // Açık hat + rakip yarı bonus
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            i16 rank_bonus = (r >= 7) ? 20 : (r >= 5) ? 10 : 0; // rakip yarı
            i16 file_bonus = (f == 5 || f == 6) ? 3 : 0;         // merkez dosya
            PST[KALE].mg[idx] = rank_bonus + file_bonus;
            PST[KALE].eg[idx] = (i16)(rank_bonus * 4 / 5);
        }
    }

    // ── Zürafa PST ───────────────────────────────────────────────────────
    // Uzun menzilli; açık merkez köşegenlerde güçlü, köşelerden uzak dur
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            // Köşeye mesafe (4 köşeden en yakınına uzaklık)
            int corner_dist = std::min({f, 10-f, r, 9-r});
            PST[ZURAFA].mg[idx] = (i16)std::max(-20, corner_dist * 5 - 5);
            PST[ZURAFA].eg[idx] = (i16)std::max(-18, corner_dist * 4 - 4);
        }
    }

    // ── Şah PST ──────────────────────────────────────────────────────────
    // MG: güvenli köşe/kanat tercih; merkez tehlikeli
    // EG: merkeze yaklaş, rakip piyonları avla
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            int idx = r * BOARD_FILES + f;
            // MG: rank 0-1 güvenli (kendi tarafı), file uç güvenli
            bool safe_mg = (r <= 1 && (f <= 2 || f >= 8));
            int center_dist = std::abs(f-5) + std::abs(r-4);
            PST[SAH].mg[idx] = safe_mg ? 15 : (i16)std::max(-40, 5 - center_dist * 6);
            PST[SAH].eg[idx] = (i16)std::max(-10, 30 - center_dist * 5);
        }
    }
}

// ── Persona Ağırlık Çarpanları ────────────────────────────────────────────
// idx: PersonaType int değeri (0=None, 1=Machiavelli, 4=Erlik, 5=Bozkurt...)
// Görev tanımı: idx 0 → Machiavelli ayarları, idx 4 → Bozkurt ayarları
void Evaluator::setPersona(int idx) {
    pw_ = PersonaWeights{}; // Tümünü 1.0'a sıfırla
    switch (idx) {
        case 0: // Machiavelli: kaleye güven, piyonu küçümse
            pw_.rook_mult = 1.15f;
            pw_.pawn_mult = 0.90f;
            break;
        case 4: // Bozkurt: ata ağırlık ver, şah güvenliğini öne çıkar
            pw_.knight_mult      = 1.20f;
            pw_.king_safety_mult = 1.30f;
            break;
        default: // Diğerleri: tüm çarpanlar varsayılan 1.0
            break;
    }
}

// ── Oyun Fazı ────────────────────────────────────────────────────────────
int Evaluator::game_phase(const Board& b) {
    int phase = 0;
    for (int r = 0; r < BOARD_RANKS; r++)
        for (int f = 0; f < BOARD_FILES; f++) {
            Square sq = make_sq(f, r);
            Piece  p  = b.piece_on(sq);
            if (p == NO_PIECE) continue;
            phase += PHASE_WEIGHT[piece_type(p)];
        }
    return std::min(phase, MAX_PHASE);
}

// ── Mobilite Değerlendirmesi ──────────────────────────────────────────────
i32 Evaluator::eval_mobility(const Board& b, Color c, int phase) const {
    i32 score = 0;

    // Piyon ve şah hariç her taş tipi için mobilite say
    for (int pt_int = KALE; pt_int <= VALI; pt_int++) {
        PieceType pt = (PieceType)pt_int;
        for (Square sq : b.pieces(c, pt)) {
            MoveList ml;
            // Taş tipine göre hamle üret
            switch (pt) {
                case KALE:   MoveGenerator::gen_kale  (b, sq, c, ml); break;
                case ZURAFA: MoveGenerator::gen_zurafa(b, sq, c, ml); break;
                case TALIA:  MoveGenerator::gen_talia (b, sq, c, ml); break;
                case AT:     MoveGenerator::gen_at    (b, sq, c, ml); break;
                case DEVE:   MoveGenerator::gen_deve  (b, sq, c, ml); break;
                case FIL:    MoveGenerator::gen_fil   (b, sq, c, ml); break;
                case SAVAS:  MoveGenerator::gen_savas (b, sq, c, ml); break;
                case FERZ:   MoveGenerator::gen_ferz  (b, sq, c, ml); break;
                case VALI:   MoveGenerator::gen_vali  (b, sq, c, ml); break;
                default: break;
            }
            int mob = std::clamp((int)ml.size(), 0, 31);
            i32 mg = MOBILITY_BONUS_MG[pt_int][mob];
            i32 eg = MOBILITY_BONUS_EG[pt_int][mob];
            score += (mg * phase + eg * (MAX_PHASE - phase)) / MAX_PHASE;
        }
    }
    return score;
}

// ── Şah Bölgesi Saldırı Yardımcısı ──────────────────────────────────────
// Verilen sq'daki taşın ksq etrafındaki 3×3 + 1 dış halkayı tehdit edip etmediği
static bool attacks_king_zone(const Board& b, Square attacker_sq, Square ksq) {
    int af = sq_file(attacker_sq), ar = sq_rank(attacker_sq);
    int kf = sq_file(ksq),         kr = sq_rank(ksq);
    // Şah çevresindeki 5×5 kare (genişletilmiş bölge)
    return (std::abs(af - kf) <= 2 && std::abs(ar - kr) <= 2);
}

// ── Kral Güvenliği Değerlendirmesi ────────────────────────────────────────
i32 Evaluator::eval_king_safety(const Board& b, Color c, int phase) const {
    Square ksq = b.king_sq(c);
    if (ksq == SQ_NONE || !sq_valid(ksq)) return 0;

    Color them = ~c;
    // Endgame'de kral güvenliği daha az kritik
    if (phase < MAX_PHASE / 3) return 0;

    int attack_units = 0;
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            Square sq = make_sq(f, r);
            Piece p = b.piece_on(sq);
            if (p == NO_PIECE || piece_color(p) != them) continue;
            PieceType pt = piece_type(p);
            if (attacks_king_zone(b, sq, ksq))
                attack_units += KING_ATTACK_WEIGHT[pt];
        }
    }
    attack_units = std::min(attack_units, 63);
    i32 penalty = KING_SAFETY_TABLE[attack_units];
    // Persona çarpanı: şah güvenliği ağırlığını ayarla
    penalty = (i32)(penalty * pw_.king_safety_mult);
    // Midgame'de daha ağırlıklı
    return -(penalty * phase / MAX_PHASE);
}

// ── Piyon Yardımcıları ────────────────────────────────────────────────────
static bool is_passed(const Board& b, Square sq, Color c) {
    int f = sq_file(sq), r = sq_rank(sq);
    Color them = ~c;
    int dir = (c == WHITE) ? 1 : -1;
    // Önünde ve komşu dosyalarda rakip piyon var mı?
    for (int r2 = r + dir; r2 >= 0 && r2 < BOARD_RANKS; r2 += dir) {
        for (int df = -1; df <= 1; df++) {
            int f2 = f + df;
            if (f2 < 0 || f2 >= BOARD_FILES) continue;
            Square s2 = make_sq(f2, r2);
            Piece p = b.piece_on(s2);
            if (p != NO_PIECE && piece_color(p) == them && piece_type(p) == PIYON)
                return false;
        }
    }
    return true;
}

static bool is_doubled(const Board& b, Square sq, Color c) {
    int f = sq_file(sq), r = sq_rank(sq);
    for (int r2 = 0; r2 < BOARD_RANKS; r2++) {
        if (r2 == r) continue;
        Square s2 = make_sq(f, r2);
        Piece p = b.piece_on(s2);
        if (p != NO_PIECE && piece_color(p) == c && piece_type(p) == PIYON)
            return true;
    }
    return false;
}

static bool is_isolated(const Board& b, Square sq, Color c) {
    int f = sq_file(sq);
    for (int df : {-1, 1}) {
        int f2 = f + df;
        if (f2 < 0 || f2 >= BOARD_FILES) continue;
        for (int r2 = 0; r2 < BOARD_RANKS; r2++) {
            Piece p = b.piece_on(make_sq(f2, r2));
            if (p != NO_PIECE && piece_color(p) == c && piece_type(p) == PIYON)
                return false;
        }
    }
    return true;
}

// ── Piyon Yapısı Değerlendirmesi ─────────────────────────────────────────
i32 Evaluator::eval_pawn_structure(const Board& b, Color c) {
    // PawnTable cache kontrolü
    PawnEntry* pe = pawn_table_.probe(b.pawn_hash());
    if (pe) return pe->score[c];

    // Her iki taraf için hesapla, sonra cache'e yaz
    i32 scores[2] = {0, 0};
    for (int ci = 0; ci < 2; ci++) {
        Color cc = (Color)ci;
        for (Square sq : b.pieces(cc, PIYON)) {
            int rank = sq_rank(sq);
            int prank = (cc == WHITE) ? rank : (9 - rank); // 0=başlangıç, 9=son sıra
            if (is_passed(b, sq, cc))
                scores[ci] += PASSED_PAWN_BONUS[prank];
            if (is_doubled(b, sq, cc))
                scores[ci] += DOUBLED_PAWN_PENALTY;
            if (is_isolated(b, sq, cc))
                scores[ci] += ISOLATED_PAWN_PENALTY;
        }
    }
    pawn_table_.store(b.pawn_hash(), scores[WHITE], scores[BLACK]);
    return scores[c];
}

// ── Citadel Değerlendirmesi ───────────────────────────────────────────────
i32 Evaluator::eval_citadel(const Board& b, Color c, int phase) const {
    Square ksq    = b.king_sq(c);
    Square target = (c == WHITE) ? BLACK_CITADEL_NEIGHBOR : WHITE_CITADEL_NEIGHBOR;
    if (ksq == SQ_NONE || ksq >= BOARD_NORMAL) return 0;

    int kf = sq_file(ksq), kr = sq_rank(ksq);
    int tf = sq_file(target), tr = sq_rank(target);
    int dist = std::abs(kf-tf) + std::abs(kr-tr);

    i32 score = 0;

    // Yakınlık bonusu — endgame'de daha değerli
    int eg_weight = MAX_PHASE - phase; // endgame'de daha yüksek

    if (dist <= 1) {
        score = CITADEL_ADJACENT_BONUS + eg_weight / 4;
    } else if (dist <= 2) {
        score = CITADEL_THREAT_BONUS + eg_weight / 8;
    } else if (dist <= 4) {
        score = 10; // Uzaktan hafif bonus
    }

    // Rakibin citadel savunması: rakip şah citadel'e yakınsa ceza
    Color them = ~c;
    Square their_target = (them == WHITE) ? BLACK_CITADEL_NEIGHBOR : WHITE_CITADEL_NEIGHBOR;
    Square their_ksq = b.king_sq(them);
    if (their_ksq != SQ_NONE && their_ksq < BOARD_NORMAL) {
        int their_dist = std::abs(sq_file(their_ksq) - sq_file(their_target))
                       + std::abs(sq_rank(their_ksq) - sq_rank(their_target));
        if (their_dist <= 1) score -= 20; // Rakip de yakında, rekabet
    }

    return score;
}

i32 Evaluator::eval_material(const Board& b) const {
    return b.material(WHITE) - b.material(BLACK);
}

// ── Ana Değerlendirme ─────────────────────────────────────────────────────
i32 Evaluator::evaluate(const Board& b, Color side) {
    int   phase = game_phase(b);
    float t     = static_cast<float>(phase) / MAX_PHASE; // 1.0=midgame

    i32 score = 0;

    // Materyal + PST (beyaz perspektifi)
    for (int r = 0; r < BOARD_RANKS; r++) {
        for (int f = 0; f < BOARD_FILES; f++) {
            Square sq = make_sq(f, r);
            Piece  p  = b.piece_on(sq);
            if (p == NO_PIECE) continue;

            PieceType pt = piece_type(p);
            Color     c  = piece_color(p);
            i32       val = PIECE_VALUE[pt];

            // Persona çarpanları: kale ve at materyal değerine uygulanır
            if (pt == KALE) val = (i32)(val * pw_.rook_mult);
            if (pt == AT)   val = (i32)(val * pw_.knight_mult);

            // PST bonusu (renk için doğru yönelim)
            int pst_sq = (c == WHITE) ? sq : ((BOARD_RANKS-1-r)*BOARD_FILES + f);
            if (pst_sq < BOARD_NORMAL) {
                i32 mg_bonus = PST[pt].mg[pst_sq];
                i32 eg_bonus = PST[pt].eg[pst_sq];
                val += (i32)(t * mg_bonus + (1.0f-t) * eg_bonus);
            }

            score += (c == WHITE ? +val : -val);
        }
    }

    // Mobilite
    score += eval_mobility(b, WHITE, phase);
    score -= eval_mobility(b, BLACK, phase);

    // Kral güvenliği
    score += eval_king_safety(b, WHITE, phase);
    score -= eval_king_safety(b, BLACK, phase);

    // Piyon yapısı
    score += eval_pawn_structure(b, WHITE);
    score -= eval_pawn_structure(b, BLACK);

    // Citadel yakınlık
    score += eval_citadel(b, WHITE, phase);
    score -= eval_citadel(b, BLACK, phase);

    // Bare king cezası
    auto bare_king = [&](Color c2) -> bool {
        return b.pieces(c2, KALE).empty() && b.pieces(c2, ZURAFA).empty()
            && b.pieces(c2, TALIA).empty() && b.pieces(c2, AT).empty()
            && b.pieces(c2, DEVE).empty()  && b.pieces(c2, FIL).empty()
            && b.pieces(c2, SAVAS).empty() && b.pieces(c2, FERZ).empty()
            && b.pieces(c2, VALI).empty()  && b.pieces(c2, PIYON).empty();
    };
    if (bare_king(WHITE)) score -= (i32)(t * BARE_KING_PENALTY_MG + (1.0f-t) * BARE_KING_PENALTY_EG);
    if (bare_king(BLACK)) score += (i32)(t * BARE_KING_PENALTY_MG + (1.0f-t) * BARE_KING_PENALTY_EG);

    i32 hce_score = (side == WHITE) ? score : -score;

    // NNUE blend: faz bazlı ağırlıklı ortalama
    // Erken eğitim aşaması: NNUE ağırlığı düşük tutulur (HCE'nin güvenilirliği yüksek)
    // phase=0(endgame): %30 NNUE, phase=MAX_PHASE(opening): %20 NNUE
    if (use_nnue()) {
        i32 nnue_score = nnue_.evaluate(b, side);
        int nnue_w = 20 + (MAX_PHASE - phase) * 10 / MAX_PHASE; // 20..30
        int hce_w  = 100 - nnue_w;
        return (nnue_score * nnue_w + hce_score * hce_w) / 100;
    }

    return hce_score;
}

i32 Evaluator::evaluate_fast(const Board& b, Color side) {
    i32 score = b.material(WHITE) - b.material(BLACK);
    return (side == WHITE) ? score : -score;
}

// eval_pst: ana evaluate() içinde satır içi yapıldı, bağımsız stub
i32 Evaluator::eval_pst(const Board&, Color, int) const { return 0; }

} // namespace Apex

// ════════ src/search.cpp ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  Zaman Yönetimi
// ─────────────────────────────────────────────────────────────────────────────
void TimeManager::init(int64_t wtime, int64_t btime, int64_t winc, int64_t binc,
                       int movestogo, Color stm)
{
    start = Clock::now();
    int64_t my_time = (stm == WHITE) ? wtime : btime;
    int64_t my_inc  = (stm == WHITE) ? winc  : binc;

    if (movestogo > 0) {
        // Belirli hamle sayısı için zaman
        soft_limit_ms = (my_time / movestogo) + my_inc * 3 / 4;
    } else {
        // Genel formül: kalan sürenin ~%5'i + artış
        soft_limit_ms = my_time / 20 + my_inc * 3 / 4;
    }

    // Hard limit: soft'un 3×'i veya toplam sürenin %80'i
    hard_limit_ms = std::min(soft_limit_ms * 3, my_time * 4 / 5);

    // Güvenlik marjı
    hard_limit_ms = std::max(hard_limit_ms - 50, (int64_t)10);
    soft_limit_ms = std::max(soft_limit_ms - 30, (int64_t)5);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Searcher — Kurucu
// ─────────────────────────────────────────────────────────────────────────────
Searcher::Searcher(Board& board, TranspositionTable& tt)
    : board_(board), tt_(tt), orderer_(), evaluator_(),
      best_move_(MOVE_NONE), ponder_move_(MOVE_NONE),
      best_score_(SCORE_NONE), stopped_(false)
{}

void Searcher::new_game() {
    tt_.clear();
    orderer_.reset();
    stats_.reset();
    best_move_  = MOVE_NONE;
    best_score_ = SCORE_NONE;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Zaman Kontrolü (her ~1024 düğümde)
// ─────────────────────────────────────────────────────────────────────────────
bool Searcher::time_check() {
    if (stopped_.load()) return true;
    if (extern_stop_ && extern_stop_->load()) {
        stopped_.store(true);
        return true;
    }
    if ((stats_.nodes & 1023) != 0) return false;
    if (node_limit_ > 0 && stats_.nodes >= node_limit_) {
        stopped_.store(true);
        return true;
    }
    if (tm_.hard_expired()) {
        stopped_.store(true);
        return true;
    }
    return false;
}

void Searcher::ponderhit() {
    pondering_ = false;
    tm_.infinite = false; // ponder'dan normal zamanlı aramaya geç
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ana Arama Başlatıcı
// ─────────────────────────────────────────────────────────────────────────────
void Searcher::start_search(const SearchLimits& lim) {
    stopped_.store(false);
    stats_.reset();
    node_limit_ = lim.nodes;

    Color stm = board_.side_to_move();

    // Zaman yöneticisini ayarla
    tm_.start = TimeManager::Clock::now();
    pondering_ = lim.ponder;
    if (lim.infinite || lim.ponder) {
        tm_.init_infinite();
    } else if (lim.movetime > 0) {
        tm_.init_movetime(lim.movetime);
    } else if (lim.wtime > 0 || lim.btime > 0) {
        tm_.init(lim.wtime, lim.btime, lim.winc, lim.binc, lim.movestogo, stm);
    } else {
        // Sadece depth limiti (go depth N) — sonsuz gibi davran
        tm_.init_infinite();
    }

    tt_.new_search();
    orderer_.reset();
    best_move_  = MOVE_NONE;
    best_score_ = SCORE_NONE;
    root_pv_.clear();
    time_adj_.store(0);

    // Contempt'i persona'dan başlat
    contempt_ = persona_.contempt();

    // Açılış stili — sadece ilk 10 hamlede (fullmove <= 10)
    opening_adj_.store(0);
    if (board_.fullmove() <= 10 && opening_cb_) {
        bool is_white = (board_.side_to_move() == WHITE);
        opening_cb_(persona_.name(), board_.fullmove(), is_white,
            [this](const std::string& style) {
                if (style == "aggressive") opening_adj_.store(1);
                else if (style == "gambit") opening_adj_.store(2);
            });
    }

    // Citadel stratejisi — az taş kaldığında (<16 taş)
    citadel_adj_.store(0);
    citadel_bias_ = 0;
    if (citadel_cb_) {
        int npieces_citadel = 0;
        for (int c = 0; c <= 1; c++)
            for (int pt = 2; pt < PIECE_TYPE_NB; pt++)
                npieces_citadel += static_cast<int>(
                    board_.pieces(static_cast<Color>(c),
                                  static_cast<PieceType>(pt)).size());
        if (npieces_citadel < 16) {
            Color stm_cit = board_.side_to_move();
            Square ksq      = board_.king_sq(stm_cit);
            Square neighbor = (stm_cit == WHITE) ? BLACK_CITADEL_NEIGHBOR
                                                 : WHITE_CITADEL_NEIGHBOR;
            int dist = std::abs(sq_rank(ksq) - sq_rank(neighbor))
                     + std::abs(sq_file(ksq) - sq_file(neighbor));
            int mat = static_cast<int>(best_score_ != SCORE_NONE ? best_score_ : 0);
            int phase_approx_cit = std::min(255, npieces_citadel * 9);
            citadel_cb_(persona_.name(), phase_approx_cit, dist, mat,
                [this](const std::string& strategy) {
                    if (strategy == "rush_citadel") citadel_adj_.store(1);
                    else if (strategy == "ignore")   citadel_adj_.store(-1);
                });
        }
    }

    // Contempt başlangıç değerini persona'dan al (zaten contempt_ = persona_.contempt() yukarıda)
    contempt_pending_.store(0);

    // Needle zaman kararı — sadece wtime modunda ve callback ayarlıysa
    if (!tm_.infinite && time_adj_cb_) {
        // Faz tahmini: taş sayısına göre (0=endgame .. 255=middlegame)
        int npieces = 0;
        for (int c = 0; c <= 1; c++)
            for (int pt = 2; pt < PIECE_TYPE_NB; pt++)
                npieces += static_cast<int>(
                    board_.pieces(static_cast<Color>(c),
                                  static_cast<PieceType>(pt)).size());
        int phase_approx = std::min(255, npieces * 9);

        // Materyal dengesi: son bilinen skor (yeni oyunda 0)
        int mat_balance = static_cast<int>(best_score_ != SCORE_NONE ? best_score_ : 0);

        time_adj_cb_(persona_.name(), phase_approx, mat_balance,
            [this](const std::string& decision) {
                if (decision == "spend_more")
                    time_adj_.store(1);
                else if (decision == "spend_less")
                    time_adj_.store(-1);
                // "normal" → 0 zaten
            });
    }

    int max_depth = std::min(lim.depth, MAX_PLY - 1);

    // Lazy SMP: n_threads_-1 helper thread başlat
    for (int t = 1; t < n_threads_; t++) {
        helpers_.emplace_back([this, max_depth]() {
            run_as_helper(stopped_, max_depth);
        });
    }

    iterative_deepening(max_depth);

    // Ana iterasyon bitti, helper'ları durdur ve bekle
    stopped_.store(true);
    for (auto& h : helpers_) h.join();
    helpers_.clear();
}

void Searcher::run_as_helper(std::atomic<bool>& parent_stop, int max_depth) {
    // Helper kendi Board kopyasını alır (TT paylaşımlı)
    Board local_board = board_;
    Searcher helper(local_board, tt_);
    helper.extern_stop_ = &parent_stop;
    helper.n_threads_   = 1;
    helper.tm_          = tm_;
    helper.node_limit_  = 0;
    helper.stopped_.store(false);
    helper.stats_.reset();
    helper.orderer_.reset();

    // Biraz farklı derinlik sırası (TT çeşitliliği için)
    for (int depth = 1; depth <= max_depth && !parent_stop.load(); depth++) {
        helper.stack_[0].ply = 0;
        helper.search(depth, -SCORE_INFINITE, SCORE_INFINITE, 0, true, false, nullptr);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Iterative Deepening
// ─────────────────────────────────────────────────────────────────────────────
void Searcher::iterative_deepening(int max_depth) {
    i32 prev_score = SCORE_NONE;
    i32 delta      = persona_.aspiration_delta();
    bool time_adj_applied    = false;
    bool opening_adj_applied = false;
    bool citadel_adj_applied = false;

    for (int depth = 1; depth <= max_depth; depth++) {
        if (stopped_.load()) break;

        // Contempt dinamik ayarını uygula (önceki derinlikten gelen karar)
        {
            int cpend = contempt_pending_.exchange(0);
            if (cpend == 1)       contempt_ = std::min(contempt_ + 5, 100);
            else if (cpend == -1) contempt_ = std::max(contempt_ - 5, -100);
        }

        // Aspiration Windows
        i32 alpha, beta;
        if (depth >= 5 && prev_score != SCORE_NONE) {
            alpha = prev_score - delta;
            beta  = prev_score + delta;
        } else {
            alpha = -SCORE_INFINITE;
            beta  =  SCORE_INFINITE;
        }

        i32 score;
        while (true) {
            stack_[0].ply = 0;
            score = search(depth, alpha, beta, 0, true, false, nullptr);

            if (stopped_.load()) break;

            if (score <= alpha) {
                // Alpha kaçtı: pencereyi aşağı genişlet
                alpha = std::max(-SCORE_INFINITE, alpha - delta);
                delta *= 2;
            } else if (score >= beta) {
                // Beta kaçtı: pencereyi yukarı genişlet
                beta = std::min(SCORE_INFINITE, beta + delta);
                delta *= 2;
            } else {
                break; // Pencere içinde kaldı
            }
            if (alpha <= -SCORE_INFINITE && beta >= SCORE_INFINITE) break;
        }

        if (stopped_.load() && best_move_ != MOVE_NONE) break;

        // Needle zaman kararını ilk geldiğinde uygula (bir kez)
        // Python subprocess ~200-500ms sürer; her derinlikte kontrol ederek
        // karar gelir gelmez uygularız
        if (!time_adj_applied && !tm_.infinite) {
            int adj = time_adj_.load();
            if (adj != 0) {
                time_adj_applied = true;
                time_adj_.store(0);
                if (adj == 1) {
                    tm_.soft_limit_ms = tm_.soft_limit_ms * 13 / 10;  // +%30
                    std::cout << "info string [NeedleTime] spend_more → soft_limit +%30"
                                 " (depth " << depth << ")\n" << std::flush;
                } else {
                    tm_.soft_limit_ms = tm_.soft_limit_ms * 7 / 10;   // -%30
                    std::cout << "info string [NeedleTime] spend_less → soft_limit -%30"
                                 " (depth " << depth << ")\n" << std::flush;
                }
                // hard_limit_ms değişmez — güvenli üst sınır sabit
            }
        }

        // Açılış stili — yanıt gelir gelmez uygula (bir kez)
        if (!opening_adj_applied) {
            int oadj = opening_adj_.load();
            if (oadj != 0) {
                opening_adj_applied = true;
                opening_adj_.store(0);
                if (oadj == 1) {
                    delta = std::max(10, delta * 7 / 10);
                    contempt_ = std::min(contempt_ + 10, 100);
                    std::cout << "info string [NeedleOpening] aggressive → dar aspiration"
                                 " (depth " << depth << ")\n" << std::flush;
                } else if (oadj == 2) {
                    contempt_ = std::max(contempt_ - 15, -100);
                    std::cout << "info string [NeedleOpening] gambit → feda toleransi"
                                 " (depth " << depth << ")\n" << std::flush;
                }
            }
        }

        // Citadel stratejisi — yanıt gelir gelmez uygula (bir kez)
        if (!citadel_adj_applied) {
            int cadj = citadel_adj_.load();
            if (cadj != 0) {
                citadel_adj_applied = true;
                citadel_adj_.store(0);
                if (cadj == 1) {
                    citadel_bias_ = 30;
                    std::cout << "info string [NeedleCitadel] rush_citadel → cazibe +30"
                                 " (depth " << depth << ")\n" << std::flush;
                } else {
                    citadel_bias_ = -30;
                    std::cout << "info string [NeedleCitadel] ignore → cazibe -30"
                                 " (depth " << depth << ")\n" << std::flush;
                }
            }
        }

        // Contempt dinamik — her 5 derinlikte bir Needle çağrısı yap
        if (depth >= 5 && depth % 5 == 0 && contempt_cb_) {
            int trend = static_cast<int>(prev_score != SCORE_NONE ? prev_score : 0);
            contempt_cb_(persona_.name(), trend, board_.fullmove(),
                [this](const std::string& action) {
                    if      (action == "raise") contempt_pending_.store(1);
                    else if (action == "lower") contempt_pending_.store(-1);
                    else                        contempt_pending_.store(0);
                    if (action != "hold" && !action.empty())
                        std::cout << "info string [NeedleContempt] " << action
                                  << " → contempt ayarlaniyor\n" << std::flush;
                });
        }

        prev_score = score;
        best_score_ = score;
        if (root_pv_.length > 0) {
            best_move_   = root_pv_.moves[0];
            ponder_move_ = (root_pv_.length > 1) ? root_pv_.moves[1] : MOVE_NONE;
        }
        delta = persona_.aspiration_delta(); // Sıfırla

        // UCI bilgi çıktısı
        int64_t ms = tm_.elapsed_ms();
        u64 nps    = ms > 0 ? (stats_.nodes + stats_.qnodes) * 1000 / ms : 0;
        (void)nps;
        if (info_cb_) {
            info_cb_(depth, stats_.sel_depth, score,
                     stats_.nodes + stats_.qnodes, ms, root_pv_);
        }
        // Persona yorumu — sadece derin aramalarda
        if (depth >= 4) {
            std::string comment = persona_.commentary(depth, score);
            if (!comment.empty())
                std::cout << "info string [" << persona_.name() << "] " << comment << "\n";
            // Needle bridge isteği (asenkron — search bloke olmaz)
            if (needle_cb_)
                needle_cb_(persona_.name(), static_cast<int>(score), depth);
        }

        // Zaman yönetimi: derinlik tamamlandı — devam edilmeli mi?
        if (!tm_.infinite && tm_.soft_expired()) break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Alpha-Beta Negamax (fail-soft)
// ─────────────────────────────────────────────────────────────────────────────
i32 Searcher::search(int depth, i32 alpha, i32 beta, int ply,
                     bool pv_node, bool cut_node, PVLine* pv_line)
{
    if (time_check()) return SCORE_NONE;

    // ── Terminal kontroller ────────────────────────────────────────────────
    if (ply >= MAX_PLY) return evaluator_.evaluate(board_, board_.side_to_move());
    if (board_.is_repetition(2)) return citadel_score(); // beraberlik
    if (board_.halfmove() >= 100) return citadel_score();

    // Citadel beraberlik: şah rakibin kalesine girmişse draw
    if (board_.king_on_citadel(board_.side_to_move())) return 0;
    if (board_.king_on_citadel(~board_.side_to_move())) return 0;

    // Citadel'e girilmişse beraberlik
    auto gr = board_.game_over();
    if (gr == Board::DRAW) return citadel_score();

    // Quiescence'a geç
    if (depth <= 0) return qsearch(alpha, beta, ply);

    stats_.nodes++;
    if (ply > stats_.sel_depth) stats_.sel_depth = ply;

    bool  root    = (ply == 0);
    bool  in_check= board_.in_check();
    i32   alpha0  = alpha; // Orijinal alpha (TT flag için)

    // Şah altında qsearch'e düşmeyi engelle (uzatmadan kaçın)
    if (in_check && depth <= 0) depth = 1;

    // ── Transposition Table sorgusu ───────────────────────────────────────
    u64            hash = board_.hash();
    const TTEntry* tte  = tt_.probe(hash);
    Move           tt_move = MOVE_NONE;
    i32            tt_score = SCORE_NONE;

    if (tte) {
        stats_.tt_hits++;
        tt_move  = tte->move;
        tt_score = tte->score;
        // Mat skorunu ply'ya normalize et (saklanan skor ply-bağımsız)
        if (tt_score >  SCORE_MATE - 500) tt_score -= ply;
        if (tt_score < -SCORE_MATE + 500) tt_score += ply;

        if (!pv_node && tte->depth >= depth) {
            if (tte->flag == TT_EXACT) {
                return tt_score;
            }
            if (tte->flag == TT_LOWER && tt_score >= beta) {
                stats_.tt_cuts++;
                return tt_score;
            }
            if (tte->flag == TT_UPPER && tt_score <= alpha) {
                return tt_score;
            }
        }
    }

    // ── Statik Değerlendirme ───────────────────────────────────────────────
    i32 static_eval = SCORE_NONE;
    if (!in_check) {
        static_eval = tte ? tte->eval : evaluator_.evaluate(board_, board_.side_to_move());
        stack_[ply].static_eval = static_eval;
    }

    // Improving: bu pozisyon 2 hamle öncesinden iyi mi?
    bool improving = false;
    if (!in_check && ply >= 2) {
        improving = (static_eval > stack_[ply-2].static_eval);
    }

    // ── Pruning (şah altında değilken, PV düğümünde değilken) ─────────────
    if (!pv_node && !in_check && static_eval != SCORE_NONE) {

        // Ters futility (statik değerlendirme beta'nın üstündeyse kes)
        if (depth <= 7 && static_eval - 100 * depth >= beta)
            return static_eval;

        // Razoring (depth <= 3)
        if (depth <= 3) {
            i32 razor_score;
            if (try_razoring(depth, alpha, static_eval, in_check, razor_score))
                return razor_score;
        }

        // Null Move Pruning (depth >= 3)
        if (depth >= 3) {
            i32 null_score;
            if (try_null_move(depth, beta, static_eval, in_check, pv_node, ply, null_score))
                return null_score;
        }

        // Probcut (depth >= 5)
        if (depth >= 5) {
            i32 pc_score;
            if (try_probcut(depth, beta, in_check, ply, static_eval, pc_score))
                return pc_score;
        }
    }

    // ── Hamle döngüsü ──────────────────────────────────────────────────────
    MoveList ml;
    if (in_check)
        MoveGenerator::generate_evasions(board_, ml);
    else
        MoveGenerator::generate_pseudo(board_, ml);

    Move prev_move = (ply > 0) ? stack_[ply - 1].current_move : MOVE_NONE;
    orderer_.score_moves(ml, board_, tt_move, ply, board_.side_to_move(), prev_move);

    i32  best_score = -SCORE_INFINITE;
    Move best_move  = MOVE_NONE;
    int  tried      = 0;
    MoveList tried_quiets; // Killers/history güncelleme için

    PVLine child_pv;
    child_pv.clear();

    for (int mi = 0; mi < ml.size(); mi++) {
        auto& me = orderer_.next_move(ml, mi);
        Move m = me.move;
        if (m == stack_[ply].excluded_move) continue;

        Square to   = move_to(m);
        bool is_capture = (board_.piece_on(to) != NO_PIECE);

        // ── Singular Extension ──────────────────────────────────────────
        int extension = 0;
        if (!root && depth >= 8 && m == tt_move
            && tt_score != SCORE_NONE && !is_mate_score(tt_score)
            && tte && tte->flag == TT_LOWER && tte->depth >= depth - 3)
        {
            if (is_singular(m, depth, tt_score, ply))
                extension = 1;
        }

        // ── Hamleyi uygula ──────────────────────────────────────────────
        Color us = board_.side_to_move(); // Hamleyi yapan (do_move ÖNCE)
        StateInfo new_st;
        board_.do_move(m, new_st, me.promo);
        tried++;

        // Şah altına girdik mi? (yasal kontrol)
        // us: hamleyi yapan taraf; do_move sonrası stm = rakip
        Square ks = board_.king_sq(us);
        if (ks == SQ_NONE || board_.is_attacked(ks, board_.side_to_move())) {
            board_.undo_move(m);
            tried--;
            continue; // Yasadışı hamle
        }

        // Hamle rakip şahı tehdit ediyor mu? (LMR kararı için)
        // do_move sonrası stm = rakip; attacker = us
        bool gives_check = board_.is_attacked(board_.king_sq(board_.side_to_move()), us);

        // Futility: sığ sessiz hamleler için erken kes
        // Statik değerlendirme + marj < alpha → hamle umut vermez
        if (depth <= 5 && !pv_node && !in_check && !gives_check
            && !is_capture && move_type(m) != MT_PROMO
            && tried >= 2 && static_eval != SCORE_NONE
            && static_eval + 80 * depth < alpha)
        {
            board_.undo_move(m);
            tried--;
            continue;
        }

        i32 score;

        // ── Late Move Reductions (LMR) ──────────────────────────────────
        if (tried >= 3 && depth >= 3 && !in_check && !gives_check
            && !is_capture && move_type(m) != MT_PROMO)
        {
            int R = lmr_reduction(depth, tried, pv_node, gives_check, is_capture);
            R -= improving ? 1 : 0;  // İyileşme: daha az azalt
            R  = std::max(1, R);

            // LMR ile sığ arama
            score = -search(depth - 1 - R, -alpha - 1, -alpha,
                            ply + 1, false, true);
            stats_.lmr_count++;

            // Umut verirse tam derinlikte tekrar ara
            if (score > alpha && R > 0)
                score = -search(depth - 1 + extension, -alpha - 1, -alpha,
                                ply + 1, false, !cut_node);
        } else {
            // İlk hamle veya erken hamleler — tam arama
            if (tried == 1 || pv_node) {
                score = -search(depth - 1 + extension, -beta, -alpha,
                                ply + 1, pv_node, false, pv_node ? &child_pv : nullptr);
            } else {
                // Zero window
                score = -search(depth - 1 + extension, -alpha - 1, -alpha,
                                ply + 1, false, !cut_node);
                if (score > alpha && score < beta)
                    score = -search(depth - 1 + extension, -beta, -alpha,
                                    ply + 1, true, false, &child_pv);
            }
        }

        board_.undo_move(m);
        if (stopped_.load()) return SCORE_NONE;

        if (!is_capture) tried_quiets.push(m);

        // ── Skor güncelle ───────────────────────────────────────────────
        if (score > best_score) {
            best_score = score;
            best_move  = m;

            if (score > alpha) {
                alpha = score;
                if (pv_node) {
                    if (root) {
                        root_pv_.copy_from(m, child_pv);
                    } else if (pv_line) {
                        pv_line->copy_from(m, child_pv);
                    }
                }
                if (alpha >= beta) {
                    // Beta kesimi
                    update_stats(depth, m, score, alpha0, beta, ply, tried_quiets);
                    break;
                }
            }
        }
    } // hamle döngüsü sonu

    // ── Mat / Pat ──────────────────────────────────────────────────────────
    if (best_score == -SCORE_INFINITE) {
        // Hiç yasal hamle yok
        if (in_check)
            return -SCORE_MATE + ply; // Mat yedik
        else
            return SCORE_DRAW;        // Pat
    }

    // ── TT'ye yaz ─────────────────────────────────────────────────────────
    TTFlag flag = (best_score >= beta)   ? TT_LOWER :
                  (best_score <= alpha0) ? TT_UPPER : TT_EXACT;

    tt_.store(hash, depth, flag, best_score,
              static_eval != SCORE_NONE ? static_eval : 0, best_move, ply);

    return best_score;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Quiescence Search
// ─────────────────────────────────────────────────────────────────────────────
i32 Searcher::qsearch(i32 alpha, i32 beta, int ply) {
    if (time_check()) return SCORE_NONE;
    if (ply >= MAX_PLY) return evaluator_.evaluate(board_, board_.side_to_move());

    // Citadel beraberlik kontrolü (qsearch'te de)
    if (board_.king_on_citadel(board_.side_to_move())) return 0;
    if (board_.king_on_citadel(~board_.side_to_move())) return 0;

    stats_.qnodes++;

    bool in_check = board_.in_check();
    i32 alpha0 = alpha;

    // TT sorgusu
    u64 qhash = board_.hash();
    const TTEntry* qtte = tt_.probe(qhash);
    Move qtt_move = MOVE_NONE;
    if (qtte) {
        stats_.tt_hits++;
        i32 qtt_score = qtte->score;
        if (qtt_score >  SCORE_MATE - 500) qtt_score -= ply;
        if (qtt_score < -SCORE_MATE + 500) qtt_score += ply;
        qtt_move = qtte->move;
        if (qtte->depth >= 0) { // qsearch depth = 0
            if (qtte->flag == TT_EXACT)                          return qtt_score;
            if (qtte->flag == TT_LOWER && qtt_score >= beta)     return qtt_score;
            if (qtte->flag == TT_UPPER && qtt_score <= alpha)    return qtt_score;
        }
    }

    // Şah altında değilse stand-pat ile alt sınır belirle
    i32 stand_pat = 0;
    if (!in_check) {
        stand_pat = qtte ? (i32)qtte->eval : evaluator_.evaluate(board_, board_.side_to_move());
        if (stand_pat >= beta)  return stand_pat;
        if (stand_pat > alpha)  alpha = stand_pat;
    }

    // Şah altında → tüm kaçış hamlelerini üret; değilse → sadece yemeler
    MoveList ml;
    if (in_check)
        MoveGenerator::generate_evasions(board_, ml);
    else
        MoveGenerator::generate_captures(board_, ml);

    // MVV-LVA sıralama (sadece yeme modunda)
    if (!in_check) {
        for (auto& me : ml) {
            Piece victim   = board_.piece_on(move_to(me.move));
            Piece attacker = board_.piece_on(move_from(me.move));
            me.score = (victim != NO_PIECE)
                ? PIECE_VALUE[piece_type(victim)] * 10 - PIECE_VALUE[piece_type(attacker)]
                : 0;
        }
    }

    int legal = 0;
    for (int qi = 0; qi < ml.size(); qi++) {
        auto& me = orderer_.next_move(ml, qi);
        Move m = me.move;
        Square to = move_to(m);

        // Delta pruning: sadece şah altında değilken
        if (!in_check) {
            Piece victim = board_.piece_on(to);
            if (victim != NO_PIECE) {
                i32 delta = stand_pat + PIECE_VALUE[piece_type(victim)] + 200;
                if (delta < alpha) continue;
            }
        }

        Color us = board_.side_to_move();
        StateInfo new_st;
        board_.do_move(m, new_st, me.promo);

        if (board_.is_attacked(board_.king_sq(us), board_.side_to_move())) {
            board_.undo_move(m);
            continue;
        }
        legal++;

        i32 score = -qsearch(-beta, -alpha, ply + 1);
        board_.undo_move(m);
        if (stopped_.load()) return SCORE_NONE;

        if (score > alpha) {
            alpha = score;
            if (alpha >= beta) return alpha;
        }
    }

    // Şah altında yasal hamle yoksa → mat
    if (in_check && legal == 0) return -SCORE_MATE + ply;

    // TT'ye yaz (depth=0 ile qsearch sonucu)
    TTFlag qflag = (alpha >= beta)   ? TT_LOWER :
                   (alpha <= alpha0) ? TT_UPPER : TT_EXACT;
    tt_.store(qhash, 0, qflag, alpha,
              in_check ? 0 : stand_pat, qtt_move, ply);

    return alpha;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pruning Yardımcıları
// ─────────────────────────────────────────────────────────────────────────────

bool Searcher::try_razoring(int depth, i32 alpha, i32 static_eval,
                             bool in_check, i32& score)
{
    if (in_check) return false;
    static const i32 margins[] = {0, 300, 500, 750};
    if (depth > 3 || static_eval + margins[depth] > alpha) return false;

    score = qsearch(alpha - 1, alpha, 0);
    return score < alpha;
}

bool Searcher::try_null_move(int depth, i32 beta, i32 static_eval,
                              bool in_check, bool pv_node, int ply, i32& score)
{
    if (in_check || pv_node) return false;
    if (static_eval < beta) return false;

    // Sadece süvari taşlar varsa NMP uygula (zugzwang riski azalır)
    // Timurlenk'te özellikle: savaş makinesi, talia gibi sıçrayan taşlar
    // ve piyonlar olan taraflarda zugzwang daha az olası

    // R değeri: depth ve materyal farkına göre adaptif
    i32 R = 3 + depth / 6 + std::min((i32)3, (static_eval - beta) / 200);

    StateInfo new_st;
    board_.do_null_move(new_st);
    score = -search(depth - R, -beta, -beta + 1, ply + 1, false, !false);
    board_.undo_null_move();

    if (stopped_.load()) return false;

    if (score >= beta) {
        stats_.null_cuts++;
        // Mate skorunu döndürme (verifikasyon gerekebilir)
        return !is_mate_score(score);
    }
    return false;
}

bool Searcher::try_probcut(int depth, i32 beta, bool in_check,
                            int ply, i32 static_eval, i32& score)
{
    if (in_check) return false;
    i32 pc_beta = beta + 200;

    MoveList ml;
    MoveGenerator::generate_captures(board_, ml);

    for (auto& me : ml) {
        Move m = me.move;
        if (MoveOrderer::see(board_, m) < pc_beta - static_eval) continue;

        StateInfo new_st;
        board_.do_move(m, new_st, me.promo);
        Color us = ~board_.side_to_move();
        if (board_.is_attacked(board_.king_sq(us), board_.side_to_move())) {
            board_.undo_move(m);
            continue;
        }

        score = -search(depth - 4, -pc_beta, -pc_beta + 1, ply + 1, false, true);
        board_.undo_move(m);

        if (stopped_.load()) return false;
        if (score >= pc_beta) return true;
    }
    return false;
}

int Searcher::lmr_reduction(int depth, int move_idx, bool pv_node,
                             bool gives_check, bool is_capture) const
{
    if (gives_check || is_capture) return 0;
    int d = std::min(depth, MAX_PLY - 1);
    int m = std::min(move_idx, MAX_MOVES - 1);
    int R = LMR_TABLE[d][m];
    if (pv_node) R--;
    return std::max(0, R);
}

bool Searcher::is_singular(Move m, int depth, i32 tt_score, int ply) {
    i32 s_beta = tt_score - depth;
    stack_[ply].excluded_move = m;
    i32 score = search(depth / 2, s_beta - 1, s_beta, ply, false, true);
    stack_[ply].excluded_move = MOVE_NONE;
    if (stopped_.load()) return false;
    stats_.singular_ext++;
    return score < s_beta;
}

// ─────────────────────────────────────────────────────────────────────────────
//  İstatistik Güncelleme (beta kesimi sonrası)
// ─────────────────────────────────────────────────────────────────────────────
void Searcher::update_stats(int depth, Move best, i32 best_score,
                             i32 alpha0, i32 beta, int ply,
                             const MoveList& tried_quiets)
{
    Piece  bp = board_.piece_on(move_from(best));
    bool   best_is_capture = (board_.piece_on(move_to(best)) != NO_PIECE);

    if (!best_is_capture) {
        // Killer güncelle
        orderer_.add_killer(best, ply);

        // History güncelle: iyi hamle +, denenen ama kötü hamleler -
        orderer_.update_history(best, depth, true, board_.side_to_move());
        for (auto& me : tried_quiets) {
            if (me.move != best)
                orderer_.update_history(me.move, depth, false, board_.side_to_move());
        }

        // Counter-move güncelle
        if (ply > 0) {
            Move prev = stack_[ply-1].current_move;
            orderer_.update_counter(prev, best);
        }
    }
    (void)best_score; (void)alpha0; (void)beta; (void)bp;
}

bool Searcher::is_bare_king(Color c) const {
    // Şah dışında taş kalmadı mı?
    for (int pt = KALE; pt < PIECE_TYPE_NB; pt++) {
        // piece_list erişimi için Board'a getter gerekir
        // Şimdilik tahtayı tara
        for (int r = 0; r < BOARD_RANKS; r++)
            for (int f = 0; f < BOARD_FILES; f++) {
                Square sq = make_sq(f, r);
                Piece  p  = board_.piece_on(sq);
                if (p != NO_PIECE && piece_color(p) == c && piece_type(p) != SAH)
                    return false;
            }
    }
    return true;
}

i32 Searcher::static_eval(const Board& b) {
    return evaluator_.evaluate(b, b.side_to_move());
}

} // namespace Apex

// ════════ src/uci.cpp ════════


namespace Apex {

// ─────────────────────────────────────────────────────────────────────────────
//  Koordinat → String dönüşümleri
// ─────────────────────────────────────────────────────────────────────────────
std::string UCI::sq_to_str(Square sq) {
    if (sq == WHITE_CITADEL) return "wc";
    if (sq == BLACK_CITADEL) return "bc";
    if (sq >= BOARD_NORMAL)  return "??";
    char file = 'a' + sq_file(sq);
    int  rank =  1  + sq_rank(sq);
    return std::string(1, file) + std::to_string(rank);
}

Square UCI::str_to_sq(const std::string& s) {
    if (s == "wc") return WHITE_CITADEL;
    if (s == "bc") return BLACK_CITADEL;
    if (s.size() < 2) return SQ_NONE;
    int f = s[0] - 'a';
    int r = std::stoi(s.substr(1)) - 1;
    if (f < 0 || f >= BOARD_FILES || r < 0 || r >= BOARD_RANKS) return SQ_NONE;
    return make_sq(f, r);
}

std::string UCI::move_to_str(Move m, PieceType promo) {
    if (m == MOVE_NONE) return "0000";
    if (m == MOVE_NULL) return "null";

    Square from = move_from(m);
    Square to   = move_to(m);
    MoveType mt = move_type(m);

    std::string s = sq_to_str(from) + sq_to_str(to);

    if (mt == MT_PROMO && promo != NO_PIECE_TYPE) {
        s += '=';
        s += PIECE_TYPE_STR[promo];
    } else if (mt == MT_SWAP) {
        s = "swap:" + sq_to_str(from) + sq_to_str(to);
    }
    return s;
}

Move UCI::str_to_move(const std::string& s, const Board& b) {
    if (s == "0000" || s == "(none)") return MOVE_NONE;

    // Swap hamlesi: "swap:e1h4"
    if (s.substr(0, 5) == "swap:") {
        std::string rest = s.substr(5);
        // from: ilk 2 karakter, to: kalan
        std::string from_s = rest.substr(0, 2);
        std::string to_s   = rest.substr(2);
        // Rank 2 haneliyse düzelt
        if (rest.size() >= 3 && rest[2] == '1' && rest.size() > 3)
            to_s = rest.substr(2);
        Square from = str_to_sq(from_s);
        Square to   = str_to_sq(to_s);
        if (from == SQ_NONE || to == SQ_NONE) return MOVE_NONE;
        return make_move(from, to, MT_SWAP);
    }

    // Citadel: "a9bc" veya "k2wc"
    if (s.size() >= 4) {
        std::string to_s = s.substr(s.size() - 2);
        if (to_s == "wc" || to_s == "bc") {
            std::string from_s = s.substr(0, s.size() - 2);
            Square from = str_to_sq(from_s);
            Square to   = str_to_sq(to_s);
            if (from != SQ_NONE && to != SQ_NONE)
                return make_move(from, to, MT_CITADEL);
        }
    }

    // Normal / terfi: "e5f7" veya "b9b10=K"
    // From: ilk 2-3 karakter (rank 10 için 3)
    // Eşittir işaretinden önce to, sonra promo
    std::string move_s = s;
    PieceType promo_pt = NO_PIECE_TYPE;

    auto eq_pos = move_s.find('=');
    if (eq_pos != std::string::npos) {
        std::string promo_str = move_s.substr(eq_pos + 1);
        move_s = move_s.substr(0, eq_pos);
        for (int pt = 1; pt < PIECE_TYPE_NB; pt++) {
            if (promo_str == PIECE_TYPE_STR[pt]) {
                promo_pt = static_cast<PieceType>(pt);
                break;
            }
        }
    }

    // from ve to: dosya harfi + rank sayısı
    // Rank 10 için to kısmı 3 karakter (b10)
    Square from = SQ_NONE, to = SQ_NONE;
    if (move_s.size() >= 4) {
        // from: a-k + 1-10
        size_t i = 1;
        while (i < move_s.size() && std::isdigit(move_s[i])) i++;
        from = str_to_sq(move_s.substr(0, i));
        to   = str_to_sq(move_s.substr(i));
    }

    if (from == SQ_NONE || to == SQ_NONE) return MOVE_NONE;
    if (promo_pt != NO_PIECE_TYPE)
        return make_move(from, to, MT_PROMO);
    return make_move(from, to);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UCI — Kurucu / Yıkıcı
// ─────────────────────────────────────────────────────────────────────────────
UCI::UCI()
    : board_(), tt_(), searcher_(board_, tt_)
{
    board_.setup();

    // Search sonuçlarını UCI formatında yaz
    searcher_.set_info_callback(
        [this](int depth, int sd, i32 score, u64 nodes, int64_t ms, const PVLine& pv) {
            print_info(depth, sd, score, nodes, ms, pv);
        }
    );
}

UCI::~UCI() {
    wait_search_done();
}

void UCI::wait_search_done() {
    if (search_thread_.joinable()) {
        searcher_.stop();
        search_thread_.join();
    }
    searching_ = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ana Döngü
// ─────────────────────────────────────────────────────────────────────────────
void UCI::loop() {
    std::string line;
    while (!quit_ && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        process(line);
    }
}

void UCI::process(const std::string& line) {
    std::istringstream ss(line);
    std::string token;
    ss >> token;

    if      (token == "uci")        cmd_uci();
    else if (token == "isready")    cmd_isready();
    else if (token == "ucinewgame") cmd_ucinewgame();
    else if (token == "position")   cmd_position(ss);
    else if (token == "go")         cmd_go(ss);
    else if (token == "stop")       cmd_stop();
    else if (token == "quit")       cmd_quit();
    else if (token == "setoption")  cmd_setoption(ss);
    else if (token == "ponderhit")  cmd_ponderhit();
    else if (token == "d")          cmd_debug_board();
    else if (token == "eval")       cmd_debug_eval();
    else if (token == "moves")      cmd_debug_moves();
    else if (token == "perft")      cmd_perft(ss);
    // Bilinmeyen komutlar sessizce yoksayılır (UCI standardı)
}

// ─────────────────────────────────────────────────────────────────────────────
//  Komut İşleyiciler
// ─────────────────────────────────────────────────────────────────────────────

void UCI::cmd_uci() {
    std::cout << "id name "   << ENGINE_NAME << " " << ENGINE_VERSION << "\n";
    std::cout << "id author " << ENGINE_AUTHOR << "\n";
    std::cout << "\n";

    // Seçenekler
    std::cout << "option name Hash type spin default 64 min 1 max 65536\n";
    std::cout << "option name Threads type spin default 1 min 1 max 128\n";
    std::cout << "option name Contempt type spin default 0 min -100 max 100\n";
    std::cout << "option name MultiPV type spin default 1 min 1 max 10\n";
    std::cout << "option name MoveOverhead type spin default 30 min 0 max 5000\n";
    std::cout << "option name Ponder type check default false\n";
    std::cout << "option name SyzygyPath type string default <empty>\n";

    // Timurlenk'e özgü
    std::cout << "option name CitadelDraw type check default true\n";
    std::cout << "option name SwapAllowed type check default true\n";
    std::cout << "option name PieceNames type combo default tr var tr var en\n";

    // L1.5 Persona Katmanı
    std::cout << "option name Persona type combo default None var None var Machiavelli var Nietzsche var Ulgen var Erlik var Bozkurt var Tengri var DedeKorkut var Umay\n";
    std::cout << "option name Psyche type spin default 0 min -100 max 100\n";

    // Needle commentary bridge
    std::cout << "option name UseNeedle type check default false\n";
    std::cout << "option name NeedleScript type string default <empty>\n";

    // NNUE
    std::cout << "option name UseNNUE type check default false\n";
    std::cout << "option name NNUEPath type string default <empty>\n";

    // Açılış Kitabı
    std::cout << "option name OwnBook type check default false\n";
    std::cout << "option name BookFile type string default networks/timurlenk_openings.bin\n";

    std::cout << "uciok\n";
}

void UCI::cmd_isready() {
    // Motor zaten hazır — sadece onay ver
    std::cout << "readyok\n";
}

void UCI::cmd_ucinewgame() {
    wait_search_done();
    searcher_.new_game();
    board_.setup();
    move_history_.clear();
}

void UCI::cmd_position(std::istringstream& ss) {
    wait_search_done();
    move_history_.clear();

    std::string token;
    ss >> token;

    if (token == "startpos") {
        board_.setup();
        ss >> token; // "moves" kelimesini oku
    } else if (token == "fen") {
        std::string fen;
        int parts = 0;
        while (parts < 6 && ss >> token && token != "moves") {
            if (!fen.empty()) fen += ' ';
            fen += token;
            parts++;
        }
        // Eğer token "moves" değilse bir sonraki token'ı oku
        if (token != "moves") {
            if (!(ss >> token)) token.clear();
        }
        if (!board_.from_string(fen))
            board_.setup();
    }

    // Hamleler
    if (token == "moves") {
        std::string move_str;
        while (ss >> move_str) {
            Move m = str_to_move(move_str, board_);
            if (!move_ok(m)) {
                std::cerr << "info string Geçersiz hamle: " << move_str << "\n";
                continue;
            }
            move_history_.push_back({});
            board_.do_move(m, move_history_.back());
        }
    }
}

void UCI::cmd_go(std::istringstream& ss) {
    wait_search_done();

    SearchLimits lim;
    std::string token;

    while (ss >> token) {
        if      (token == "wtime")     ss >> lim.wtime;
        else if (token == "btime")     ss >> lim.btime;
        else if (token == "winc")      ss >> lim.winc;
        else if (token == "binc")      ss >> lim.binc;
        else if (token == "movestogo") ss >> lim.movestogo;
        else if (token == "movetime")  ss >> lim.movetime;
        else if (token == "depth")     ss >> lim.depth;
        else if (token == "nodes")     ss >> lim.nodes;
        else if (token == "infinite")  lim.infinite = true;
        else if (token == "ponder")    lim.ponder   = true;
    }

    // ── Açılış kitabı kontrolü — kitapta hamle varsa aramayı atla ────────────
    if (opts_.own_book && book_.loaded()) {
        Move bm = book_.probe(board_.hash());
        if (bm != MOVE_NONE) {
            Square from = move_from(bm);
            Square to   = move_to(bm);
            // UCI koordinat string'i: "e1f3" formatı
            static const char* FILES = "abcdefghijk";
            char buf[12];
            int from_file = sq_file(from);
            int from_rank = sq_rank(from) + 1;
            int to_file   = sq_file(to);
            int to_rank   = sq_rank(to)   + 1;
            snprintf(buf, sizeof(buf), "%c%d%c%d",
                     FILES[from_file], from_rank,
                     FILES[to_file],   to_rank);
            std::cout << "bestmove " << buf << "\n" << std::flush;
            return;
        }
    }

    searcher_.set_persona(opts_.persona);
    searcher_.set_threads(opts_.threads);

    // Needle bridge callback — Needle aktifse async yorum isteği gönder
    if (opts_.use_needle && commentary_.is_configured()) {
        searcher_.set_needle_callback([this](const std::string& persona,
                                             int score, int depth) {
            commentary_.request_async(persona, score, depth,
                [persona](const std::string& text) {
                    std::cout << "info string [" << persona << "/Needle] "
                              << text << "\n" << std::flush;
                });
        });
        // Zaman yönetimi kararı callback
        searcher_.set_time_adj_callback([this](const std::string& persona,
                                               int phase_score, int mat_balance,
                                               std::function<void(const std::string&)> result_cb) {
            commentary_.request_time_async(persona, phase_score, mat_balance,
                std::move(result_cb));
        });
        // Açılış stili callback
        searcher_.set_opening_callback([this](const std::string& persona,
                                              int move_num, bool is_white,
                                              std::function<void(const std::string&)> result_cb) {
            commentary_.request_opening_async(persona, move_num, is_white,
                std::move(result_cb));
        });
        // Citadel strateji callback
        searcher_.set_citadel_callback([this](const std::string& persona,
                                              int phase, int king_dist, int material,
                                              std::function<void(const std::string&)> result_cb) {
            commentary_.request_citadel_async(persona, phase, king_dist, material,
                std::move(result_cb));
        });
        // Dinamik contempt callback
        searcher_.set_contempt_callback([this](const std::string& persona,
                                               int score_trend, int move_num,
                                               std::function<void(const std::string&)> result_cb) {
            commentary_.request_contempt_async(persona, score_trend, move_num,
                std::move(result_cb));
        });
    } else {
        searcher_.set_needle_callback(nullptr);
        searcher_.set_time_adj_callback(nullptr);
        searcher_.set_opening_callback(nullptr);
        searcher_.set_citadel_callback(nullptr);
        searcher_.set_contempt_callback(nullptr);
    }

    searching_ = true;
    search_thread_ = std::thread([this, lim]() {
        searcher_.start_search(lim);
        Move best   = searcher_.best_move();
        Move ponder = searcher_.ponder_move();
        print_bestmove(best, ponder);
        searching_ = false;
    });
}

void UCI::cmd_stop() {
    searcher_.stop();
    wait_search_done();
}

void UCI::cmd_quit() {
    cmd_stop();
    commentary_.wait();
    quit_ = true;
}

void UCI::cmd_setoption(std::istringstream& ss) {
    std::string token, name, value;
    ss >> token; // "name"
    while (ss >> token && token != "value") name += (name.empty() ? "" : " ") + token;
    while (ss >> token) value += (value.empty() ? "" : " ") + token;

    // Küçük harf karşılaştırma
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };
    std::string lname = lower(name);

    if      (lname == "hash")         { opts_.hash_mb = std::stoi(value); tt_.resize(opts_.hash_mb); }
    else if (lname == "threads")      { opts_.threads = std::stoi(value); }
    else if (lname == "contempt")     { opts_.contempt = std::stoi(value); }
    else if (lname == "multipv")      { opts_.multi_pv = std::stoi(value); }
    else if (lname == "moveoverhead") { opts_.move_overhead = std::stoi(value); }
    else if (lname == "ponder")       { opts_.ponder = (lower(value) == "true"); }
    else if (lname == "syzygypath")   { opts_.syzygy_path = value; }
    else if (lname == "citadeldraw")  { opts_.citadel_draw = (lower(value) == "true"); }
    else if (lname == "swapallowed")  { opts_.swap_allowed = (lower(value) == "true"); }
    else if (lname == "piecenames")   { opts_.piece_names = value; }
    else if (lname == "persona") {
        std::string lval = lower(value);
        // Sayısal index desteği: "0"=None, "1"=Machiavelli, "5"=Bozkurt vb.
        bool is_numeric = !lval.empty() && std::all_of(lval.begin(), lval.end(), ::isdigit);
        if (is_numeric) {
            int idx = std::stoi(lval);
            opts_.persona.set_persona(static_cast<PersonaType>(idx));
        } else if (lval == "machiavelli") opts_.persona.set_persona(PersonaType::MACHIAVELLI);
        else if (lval == "nietzsche")   opts_.persona.set_persona(PersonaType::NIETZSCHE);
        else if (lval == "ulgen")       opts_.persona.set_persona(PersonaType::ULGEN);
        else if (lval == "erlik")       opts_.persona.set_persona(PersonaType::ERLIK);
        else if (lval == "bozkurt")     opts_.persona.set_persona(PersonaType::BOZKURT);
        else if (lval == "tengri")      opts_.persona.set_persona(PersonaType::TENGRI);
        else if (lval == "dedekorkut")  opts_.persona.set_persona(PersonaType::DEDE_KORKUT);
        else if (lval == "umay")        opts_.persona.set_persona(PersonaType::UMAY);
        else                            opts_.persona.set_persona(PersonaType::NONE);
        // Evaluator persona ağırlık çarpanlarını güncelle
        searcher_.evaluator().setPersona(static_cast<int>(opts_.persona.type));
        std::cout << "info string Persona: " << opts_.persona.name() << "\n";
    }
    else if (lname == "psyche") {
        opts_.persona.set_psyche(std::stoi(value));
        std::cout << "info string Psyche (ψ): " << opts_.persona.psyche << "\n";
    }
    else if (lname == "useneedle") {
        opts_.use_needle = (lower(value) == "true");
        if (opts_.use_needle && !opts_.needle_script.empty()) {
            commentary_.set_script(opts_.needle_script);
            std::cout << "info string Needle bridge aktif: " << opts_.needle_script << "\n";
        } else if (opts_.use_needle) {
            std::cout << "info string Needle: NeedleScript yolu ayarlanmamış\n";
        } else {
            std::cout << "info string Needle bridge devre dışı\n";
        }
    }
    else if (lname == "needlescript") {
        opts_.needle_script = value;
        if (opts_.use_needle) {
            commentary_.set_script(value);
            std::cout << "info string Needle script: " << value << "\n";
        }
    }
    else if (lname == "usennue") {
        opts_.use_nnue = (lower(value) == "true");
        if (opts_.use_nnue && !opts_.nnue_path.empty())
            searcher_.evaluator().load_nnue(opts_.nnue_path);
        searcher_.evaluator().set_use_nnue(opts_.use_nnue);
        std::cout << "info string NNUE: " << (opts_.use_nnue ? "aktif" : "devre dışı") << "\n";
    }
    else if (lname == "nnuepath") {
        opts_.nnue_path = value;
        if (opts_.use_nnue) {
            bool ok = searcher_.evaluator().load_nnue(value);
            std::cout << "info string NNUE model: " << value
                      << (ok ? " [yüklendi]" : " [hata — dosya bulunamadı]") << "\n";
        }
    }
    else if (lname == "ownbook") {
        opts_.own_book = (lower(value) == "true");
        if (opts_.own_book && !opts_.book_path.empty() && !book_.loaded()) {
            bool ok = book_.load(opts_.book_path);
            std::cout << "info string OwnBook: " << (ok ? "kitap yüklendi" : "kitap bulunamadı — normal arama kullanılacak")
                      << " (" << opts_.book_path << ")\n";
        } else {
            std::cout << "info string OwnBook: " << (opts_.own_book ? "aktif" : "devre dışı") << "\n";
        }
    }
    else if (lname == "bookfile") {
        opts_.book_path = value;
        if (opts_.own_book) {
            bool ok = book_.load(value);
            std::cout << "info string BookFile: " << value
                      << (ok ? " [yüklendi, " + std::to_string(book_.size()) + " pozisyon]"
                             : " [hata — dosya bulunamadı]") << "\n";
        }
    }
    else {
        std::cout << "info string Bilinmeyen seçenek: " << name << "\n";
    }
}

void UCI::cmd_ponderhit() {
    searcher_.ponderhit();
}

void UCI::cmd_debug_board() {
    std::cout << board_.to_string();
    std::cout << "info string Şah kontrolü: "
              << (board_.in_check() ? "VAR" : "yok") << "\n";
}

void UCI::cmd_debug_eval() {
    Evaluator ev;
    i32 score = ev.evaluate(board_, board_.side_to_move());
    std::cout << "info string Değerlendirme: " << score << " cp ("
              << (board_.side_to_move() == WHITE ? "Beyaz" : "Siyah")
              << " perspektifi)\n";
    i32 mat_w = board_.material(WHITE);
    i32 mat_b = board_.material(BLACK);
    std::cout << "info string Materyal — Beyaz: " << mat_w
              << "  Siyah: " << mat_b
              << "  Fark: " << (mat_w - mat_b) << " cp\n";
}

void UCI::cmd_debug_moves() {
    MoveList ml;
    MoveGenerator::generate_legal(board_, ml);
    std::cout << "info string Yasal hamle sayısı: " << ml.count << "\n";
    for (auto& me : ml) {
        std::cout << "  " << move_to_str(me.move, me.promo);
        Piece victim = board_.piece_on(move_to(me.move));
        if (victim != NO_PIECE)
            std::cout << " x" << PIECE_TYPE_TR[piece_type(victim)];
        if (move_type(me.move) == MT_SWAP)   std::cout << " [swap]";
        if (move_type(me.move) == MT_CITADEL) std::cout << " [citadel=beraberlik]";
        if (move_type(me.move) == MT_PROMO)   std::cout << " [terfi=" << PIECE_TYPE_TR[me.promo] << "]";
        std::cout << "\n";
    }
}

void UCI::cmd_perft(std::istringstream& ss) {
    int depth = 1;
    ss >> depth;

    auto t_start = std::chrono::steady_clock::now();
    u64 nodes = perft(depth);
    auto t_end = std::chrono::steady_clock::now();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    u64 nps    = ms > 0 ? nodes * 1000 / ms : nodes;

    std::cout << "Perft(" << depth << ") = " << nodes
              << "  süre: " << ms << "ms"
              << "  NPS: " << nps << "\n";
}

u64 UCI::perft(int depth) {
    if (depth == 0) return 1;

    MoveList ml;
    MoveGenerator::generate_legal(board_, ml);
    if (depth == 1) return ml.count;

    u64 nodes = 0;
    for (auto& me : ml) {
        StateInfo st;
        board_.do_move(me.move, st);
        nodes += perft(depth - 1);
        board_.undo_move(me.move);
    }
    return nodes;
}

// ─────────────────────────────────────────────────────────────────────────────
//  UCI Çıktı Oluşturucular
// ─────────────────────────────────────────────────────────────────────────────
void UCI::print_info(int depth, int sel_depth, i32 score,
                     u64 nodes, int64_t ms, const PVLine& pv,
                     int multi_pv_idx)
{
    std::cout << "info depth " << depth
              << " seldepth " << sel_depth;

    if (multi_pv_idx > 0)
        std::cout << " multipv " << multi_pv_idx;

    // Skor: mat mı yoksa cp mi?
    if (is_mate_score(score)) {
        int plies = mate_in_plies(score);
        std::cout << " score mate " << plies;
    } else {
        std::cout << " score cp " << score;
    }

    u64 nps = (ms > 0) ? nodes * 1000 / ms : 0;
    std::cout << " nodes " << nodes
              << " nps "   << nps
              << " time "  << ms
              << " hashfull " << tt_.hashfull();

    // PV hamleler
    if (pv.length > 0) {
        std::cout << " pv";
        for (int i = 0; i < pv.length; i++)
            std::cout << " " << move_to_str(pv.moves[i]);
    }

    std::cout << "\n";

    // Türkçe taş isimli PV açıklaması (piece_names=tr seçiliyse)
    if (opts_.piece_names == "tr" && pv.length > 0 && depth <= 3) {
        std::cout << "info string PV:";
        for (int i = 0; i < pv.length; i++) {
            Move mv = pv.moves[i];
            Square from = move_from(mv);
            Piece p = board_.piece_on(from);
            if (p != NO_PIECE) {
                PieceType pt = piece_type(p);
                std::cout << " " << PIECE_TYPE_TR[pt]
                          << "(" << move_to_str(mv) << ")";
            } else {
                std::cout << " " << move_to_str(mv);
            }
        }
        std::cout << "\n";
    }
}

void UCI::print_bestmove(Move best, Move ponder) {
    std::cout << "bestmove " << move_to_str(best);
    if (ponder != MOVE_NONE && opts_.ponder)
        std::cout << " ponder " << move_to_str(ponder);
    std::cout << "\n";
}

void UCI::apply_options() {
    // Seçenekler değiştiğinde motoru güncelle
    // (thread sayısı, contempt vb. — Faz 2'de genişletilecek)
}

} // namespace Apex

// ════════ src/main.cpp ════════


// Timurlenk benchmark pozisyonları (başlangıç + çeşitli orta oyun pozisyonları)
static const char* BENCH_FENS[] = {
    // Başlangıç pozisyonu
    "RE2WNCZTFRK/ZT1PNWECVKR/PPPPPPPPPPP/11/11/11/11/ppppppppppp/zt1pnwecvkr/re2wncztfrk w wb - 0 1",
    // Beyaz açılış hamlelerinden sonra
    "RE2WNCZTFRK/ZT1PNWECVKR/PPPPPP1PPPP/11/11/6P4/11/ppppppppppp/zt1pnwecvkr/re2wncztfrk b wb - 0 2",
    // Orta oyun pozisyonu
    "R3WN1TFRK/ZT2PNEC1KR/PPP2PPPPPP/11/3pp5/3PP5/11/pp1pp1ppppp/zt2pnec1kr/r3wn1tfrk w - - 4 8",
    // Piyon yapısı testi
    "R4N1TFRK/ZT3NECVKR/PPPPPP1PPPP/11/11/5p5/11/pppppp1pppp/zt3necvkr/r4n1tfrk w wb - 0 5",
    // Kale açık hat
    "R4NZTFRK/ZT4NECVKR/PPPPPP1PPPP/11/11/11/11/pp3pppppp/zt4necvkr/r4nztfrk w w - 2 10",
};
static const int BENCH_DEPTH = 6;
static const int BENCH_COUNT = 5;

int main(int argc, char* argv[]) {
    Apex::Zobrist.init();
    Apex::init_lmr_table();
    Apex::init_pst();

    std::cout.setf(std::ios::unitbuf);
    std::cin.setf(std::ios::unitbuf);

    if (argc > 1 && std::string(argv[1]) == "bench") {
        std::cout << "Apex Timurlenk " << Apex::ENGINE_VERSION
                  << " — Benchmark (depth " << BENCH_DEPTH << ")\n";

        Apex::TranspositionTable tt;
        tt.resize(64);
        Apex::Board board;
        board.setup();
        Apex::Searcher searcher(board, tt);

        Apex::u64 total_nodes = 0;
        auto t0 = std::chrono::steady_clock::now();

        for (int pos = 0; pos < BENCH_COUNT; pos++) {
            if (!board.from_string(BENCH_FENS[pos])) {
                board.setup(); // FEN geçersizse başlangıç pozisyonu
            }
            std::cout << "Position " << (pos+1) << "/" << BENCH_COUNT << "\n";

            Apex::SearchLimits lim;
            lim.depth = BENCH_DEPTH;

            searcher.set_info_callback([&](int depth, int, Apex::i32 score,
                                           Apex::u64 nodes, int64_t ms, const Apex::PVLine&) {
                total_nodes = nodes;
                std::cout << "info depth " << depth
                          << " score cp " << score
                          << " nodes " << nodes
                          << " time " << ms << "\n";
            });
            searcher.start_search(lim);
        }

        auto t1 = std::chrono::steady_clock::now();
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        int64_t nps = ms > 0 ? (total_nodes * 1000 / ms) : 0;

        std::cout << "\nBenchmark tamamlandı:\n"
                  << "  Toplam node: " << total_nodes << "\n"
                  << "  Toplam süre: " << ms << " ms\n"
                  << "  NPS:         " << nps << "\n";
        return 0;
    }

    Apex::UCI uci;
    uci.loop();
    return 0;
}
