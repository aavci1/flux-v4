#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace flux;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kCardW = 124.f;
constexpr float kCardH = 177.f;
constexpr float kColGap = 18.f;
constexpr float kFanOpen = 28.f;
constexpr float kFanClosed = 14.f;
constexpr float kTopToTableauGap = 36.f;
constexpr float kMinBoardW = kCardW * 7.f + kColGap * 6.f;
constexpr float kBoardH = 720.f;
constexpr float kBoardTop = 28.f;
constexpr float kBoardHorizontalInset = 40.f;
constexpr std::int64_t kDropFlyDurationNanos = 340'000'000;

enum class Suit : std::uint8_t { Spades, Hearts, Diamonds, Clubs };
enum class PileKind : std::uint8_t { None, Stock, Waste, Tableau, Foundation };

struct Card {
  int id = 0;
  int rank = 1;
  Suit suit = Suit::Spades;
  bool faceUp = false;

  bool operator==(Card const&) const = default;
};

struct Board {
  std::array<std::vector<Card>, 7> tableau;
  std::vector<Card> stock;
  std::vector<Card> waste;
  std::array<std::vector<Card>, 4> foundations;

  bool operator==(Board const&) const = default;
};

struct HistoryEntry {
  Board board;
  int moves = 0;
  int score = 0;

  bool operator==(HistoryEntry const&) const = default;
};

struct Source {
  PileKind kind = PileKind::None;
  int index = -1;
  int row = -1;

  bool operator==(Source const&) const = default;
};

struct Selection {
  Source source;
  int count = 0;

  bool active() const { return source.kind != PileKind::None && count > 0; }
  bool operator==(Selection const&) const = default;
};

struct Hint {
  Source source;

  bool active() const { return source.kind != PileKind::None; }
  bool operator==(Hint const&) const = default;
};

struct FlyAnimation {
  std::vector<Card> cards;
  std::vector<Rect> fromRects;
  std::vector<Rect> toRects;
  std::int64_t startNanos = 0;
  std::int64_t durationNanos = 260'000'000;

  bool operator==(FlyAnimation const&) const = default;
};

struct PeekState {
  Source source;
  Card card;
  Rect rect;
  std::int64_t expiresNanos = 0;

  bool active(std::int64_t now) const {
    return source.kind != PileKind::None && expiresNanos > now;
  }
  bool operator==(PeekState const&) const = default;
};

struct DragState {
  Source source;
  int count = 0;
  std::vector<Card> cards;
  Point startPoint;
  Point currentPoint;
  Point grabOffset;
  bool moved = false;

  bool active() const { return source.kind != PileKind::None && count > 0 && !cards.empty(); }
  bool operator==(DragState const&) const = default;
};

struct SolitaireState {
  Board board;
  std::vector<HistoryEntry> history;
  std::vector<FlyAnimation> animations;
  Selection selection;
  Hint hint;
  PeekState peek;
  DragState drag;
  int moves = 0;
  int score = 0;
  int elapsedSeconds = 0;
  std::uint32_t seed = 1;
  std::int64_t startedNanos = 0;
  std::int64_t frameNanos = 0;

  bool operator==(SolitaireState const&) const = default;
};

struct CardPosition {
  Card card;
  Source source;
  Rect rect;
  int count = 1;
  bool draggable = false;

  bool operator==(CardPosition const&) const = default;
};

struct BoardGeometry {
  Point origin;
  float scale = 1.f;
  float layoutWidth = kMinBoardW;
  float layoutHeight = kBoardH;
  float columnGap = kColGap;
  Size viewport;
};

Color colorHex(std::uint32_t hex, float alpha = 1.f) {
  Color c = Color::hex(hex);
  c.a = alpha;
  return c;
}

Color withAlpha(Color c, float alpha) {
  c.a = alpha;
  return c;
}

std::int64_t nowNanos() {
  auto const nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch());
  return nanos.count();
}

std::uint32_t randomSeed() {
  static std::random_device randomDevice;
  std::uint64_t const nanos = static_cast<std::uint64_t>(nowNanos());
  std::uint32_t seed = randomDevice();
  seed ^= static_cast<std::uint32_t>(nanos);
  seed ^= static_cast<std::uint32_t>(nanos >> 32);
  return seed == 0 ? 1u : seed;
}

template<typename Fn>
void mutate(Signal<SolitaireState> const& state, Fn&& fn) {
  SolitaireState next = state();
  fn(next);
  state = std::move(next);
}

std::string rankLabel(int rank) {
  switch (rank) {
  case 1:
    return "A";
  case 11:
    return "J";
  case 12:
    return "Q";
  case 13:
    return "K";
  default:
    return std::to_string(rank);
  }
}

std::string_view suitGlyph(Suit suit) {
  switch (suit) {
  case Suit::Spades:
    return "♠︎";
  case Suit::Hearts:
    return "♥︎";
  case Suit::Diamonds:
    return "♦︎";
  case Suit::Clubs:
    return "♣︎";
  }
  return "♠︎";
}

Color suitColor(Suit suit) {
  return (suit == Suit::Hearts || suit == Suit::Diamonds) ? Color::hex(0xD94040) : Color::hex(0x1A1A22);
}

bool redSuit(Suit suit) {
  return suit == Suit::Hearts || suit == Suit::Diamonds;
}

int foundationIndex(Suit suit) {
  return static_cast<int>(suit);
}

int rankValue(Card const& card) {
  return card.rank;
}

bool canPlaceOnTableau(Card const& moving, Card const* top) {
  if (!top) {
    return moving.rank == 13;
  }
  if (!top->faceUp) {
    return false;
  }
  return redSuit(moving.suit) != redSuit(top->suit) && rankValue(moving) == rankValue(*top) - 1;
}

bool canPlaceOnFoundation(Card const& moving, Card const* top) {
  if (!top) {
    return moving.rank == 1;
  }
  return moving.suit == top->suit && rankValue(moving) == rankValue(*top) + 1;
}

Board dealBoard(std::uint32_t seed) {
  std::vector<Card> deck;
  deck.reserve(52);
  int id = 0;
  for (Suit suit : {Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs}) {
    for (int rank = 1; rank <= 13; ++rank) {
      deck.push_back(Card{.id = id++, .rank = rank, .suit = suit, .faceUp = false});
    }
  }

  std::mt19937 rng(seed);
  std::shuffle(deck.begin(), deck.end(), rng);

  Board board;
  std::size_t cursor = 0;
  for (int col = 0; col < 7; ++col) {
    for (int row = 0; row <= col; ++row) {
      Card card = deck[cursor++];
      card.faceUp = row == col;
      board.tableau[static_cast<std::size_t>(col)].push_back(card);
    }
  }
  for (; cursor < deck.size(); ++cursor) {
    board.stock.push_back(deck[cursor]);
  }
  return board;
}

SolitaireState makeState(std::uint32_t seed) {
  return SolitaireState{
      .board = dealBoard(seed),
      .seed = seed,
      .startedNanos = nowNanos(),
  };
}

std::string formatTime(int seconds) {
  int const minutes = seconds / 60;
  int const secs = seconds % 60;
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
  return buffer;
}

void pushHistory(SolitaireState& state) {
  if (state.history.size() >= 50) {
    state.history.erase(state.history.begin());
  }
  state.history.push_back(HistoryEntry{
      .board = state.board,
      .moves = state.moves,
      .score = state.score,
  });
}

std::vector<Card> cardsFromSource(Board const& board, Source source, int count) {
  std::vector<Card> cards;
  if (count <= 0) {
    return cards;
  }
  switch (source.kind) {
  case PileKind::Waste:
    if (!board.waste.empty()) {
      cards.push_back(board.waste.back());
    }
    break;
  case PileKind::Tableau: {
    if (source.index < 0 || source.index >= 7) {
      break;
    }
    auto const& column = board.tableau[static_cast<std::size_t>(source.index)];
    if (source.row < 0 || source.row >= static_cast<int>(column.size())) {
      break;
    }
    int const end = static_cast<int>(column.size());
    int const start = std::max(source.row, end - count);
    for (int i = start; i < end; ++i) {
      cards.push_back(column[static_cast<std::size_t>(i)]);
    }
    break;
  }
  case PileKind::Foundation:
    if (source.index >= 0 && source.index < 4) {
      auto const& pile = board.foundations[static_cast<std::size_t>(source.index)];
      if (!pile.empty()) {
        cards.push_back(pile.back());
      }
    }
    break;
  default:
    break;
  }
  return cards;
}

bool takeCards(Board& board, Source source, int count, std::vector<Card>& out) {
  out = cardsFromSource(board, source, count);
  if (out.empty()) {
    return false;
  }

  switch (source.kind) {
  case PileKind::Waste:
    board.waste.pop_back();
    return true;
  case PileKind::Tableau: {
    auto& column = board.tableau[static_cast<std::size_t>(source.index)];
    auto const start = column.end() - static_cast<std::ptrdiff_t>(out.size());
    column.erase(start, column.end());
    return true;
  }
  case PileKind::Foundation:
    board.foundations[static_cast<std::size_t>(source.index)].pop_back();
    return true;
  default:
    break;
  }
  return false;
}

bool canMove(Board const& board, Source source, int count, Source dest) {
  if (source.kind == PileKind::None || dest.kind == PileKind::None || source == dest) {
    return false;
  }
  std::vector<Card> moving = cardsFromSource(board, source, count);
  if (moving.empty()) {
    return false;
  }
  Card const& first = moving.front();
  if (dest.kind == PileKind::Tableau) {
    if (dest.index < 0 || dest.index >= 7) {
      return false;
    }
    auto const& column = board.tableau[static_cast<std::size_t>(dest.index)];
    Card const* top = column.empty() ? nullptr : &column.back();
    return canPlaceOnTableau(first, top);
  }
  if (dest.kind == PileKind::Foundation) {
    if (moving.size() != 1 || dest.index < 0 || dest.index >= 4) {
      return false;
    }
    auto const& pile = board.foundations[static_cast<std::size_t>(dest.index)];
    Card const* top = pile.empty() ? nullptr : &pile.back();
    return canPlaceOnFoundation(first, top);
  }
  return false;
}

bool performMove(SolitaireState& state, Source source, Source dest, int count) {
  if (!canMove(state.board, source, count, dest)) {
    return false;
  }

  pushHistory(state);
  std::vector<Card> moving;
  if (!takeCards(state.board, source, count, moving)) {
    state.history.pop_back();
    return false;
  }

  if (dest.kind == PileKind::Tableau) {
    auto& column = state.board.tableau[static_cast<std::size_t>(dest.index)];
    column.insert(column.end(), moving.begin(), moving.end());
  } else if (dest.kind == PileKind::Foundation) {
    auto& pile = state.board.foundations[static_cast<std::size_t>(dest.index)];
    pile.push_back(moving.front());
  }

  if (source.kind == PileKind::Tableau) {
    auto& column = state.board.tableau[static_cast<std::size_t>(source.index)];
    if (!column.empty() && !column.back().faceUp) {
      column.back().faceUp = true;
      state.score += 5;
    }
  }

  if (dest.kind == PileKind::Foundation) {
    state.score += 10;
  }
  if (source.kind == PileKind::Foundation && dest.kind == PileKind::Tableau) {
    state.score = std::max(0, state.score - 15);
  }
  if (source.kind == PileKind::Waste && dest.kind == PileKind::Tableau) {
    state.score += 5;
  }

  state.selection = {};
  state.hint = {};
  state.peek = {};
  state.drag = {};
  ++state.moves;
  return true;
}

std::optional<int> findFoundationFor(Board const& board, Card const& card) {
  int const preferred = foundationIndex(card.suit);
  auto const& preferredPile = board.foundations[static_cast<std::size_t>(preferred)];
  Card const* preferredTop = preferredPile.empty() ? nullptr : &preferredPile.back();
  if (canPlaceOnFoundation(card, preferredTop)) {
    return preferred;
  }
  for (int i = 0; i < 4; ++i) {
    auto const& pile = board.foundations[static_cast<std::size_t>(i)];
    Card const* top = pile.empty() ? nullptr : &pile.back();
    if (canPlaceOnFoundation(card, top)) {
      return i;
    }
  }
  return std::nullopt;
}

bool autoMove(SolitaireState& state, Source source, int count) {
  std::vector<Card> cards = cardsFromSource(state.board, source, count);
  if (cards.size() != 1) {
    return false;
  }
  std::optional<int> dest = findFoundationFor(state.board, cards.front());
  if (!dest) {
    return false;
  }
  return performMove(state, source, Source{.kind = PileKind::Foundation, .index = *dest}, 1);
}

void drawStock(SolitaireState& state, int drawCount) {
  pushHistory(state);
  if (state.board.stock.empty()) {
    while (!state.board.waste.empty()) {
      Card card = state.board.waste.back();
      state.board.waste.pop_back();
      card.faceUp = false;
      state.board.stock.push_back(card);
    }
    if (drawCount == 1) {
      state.score = std::max(0, state.score - 100);
    }
  } else {
    int const n = std::min(drawCount, static_cast<int>(state.board.stock.size()));
    for (int i = 0; i < n; ++i) {
      Card card = state.board.stock.back();
      state.board.stock.pop_back();
      card.faceUp = true;
      state.board.waste.push_back(card);
    }
  }
  state.selection = {};
  state.hint = {};
  state.peek = {};
  state.drag = {};
  ++state.moves;
}

float columnX(BoardGeometry const& geometry, int column) {
  return (kCardW + geometry.columnGap) * static_cast<float>(column);
}

bool cardIdIn(std::vector<int> const& cardIds, int cardId) {
  return std::find(cardIds.begin(), cardIds.end(), cardId) != cardIds.end();
}

std::vector<CardPosition> buildCardPositions(Board const& board, int drawCount, BoardGeometry const& geometry,
                                             std::vector<int> const& hiddenCardIds = {}) {
  std::vector<CardPosition> positions;

  for (auto it = board.stock.rbegin(); it != board.stock.rend(); ++it) {
    if (cardIdIn(hiddenCardIds, it->id)) {
      continue;
    }
    positions.push_back(CardPosition{
        .card = *it,
        .source = Source{.kind = PileKind::Stock},
        .rect = Rect::sharp(0.f, 0.f, kCardW, kCardH),
        .draggable = false,
    });
    break;
  }

  int const wasteShow = drawCount == 3 ? 3 : 1;
  int const wasteVisible = std::min(wasteShow, static_cast<int>(board.waste.size()));
  int const firstWasteIndex = static_cast<int>(board.waste.size()) - wasteVisible;
  int displayIndex = 0;
  for (int i = 0; i < wasteVisible; ++i) {
    int const sourceIndex = firstWasteIndex + i;
    Card card = board.waste[static_cast<std::size_t>(sourceIndex)];
    if (cardIdIn(hiddenCardIds, card.id)) {
      continue;
    }
    float const x = columnX(geometry, 1) + static_cast<float>(displayIndex) * 22.f;
    bool const top = sourceIndex == static_cast<int>(board.waste.size()) - 1;
    positions.push_back(CardPosition{
        .card = card,
        .source = Source{.kind = PileKind::Waste},
        .rect = Rect::sharp(x, 0.f, kCardW, kCardH),
        .count = 1,
        .draggable = top,
    });
    ++displayIndex;
  }
  if (displayIndex == 0 && firstWasteIndex > 0) {
    for (int sourceIndex = firstWasteIndex - 1; sourceIndex >= 0; --sourceIndex) {
      Card card = board.waste[static_cast<std::size_t>(sourceIndex)];
      if (cardIdIn(hiddenCardIds, card.id)) {
        continue;
      }
      positions.push_back(CardPosition{
          .card = card,
          .source = Source{.kind = PileKind::Waste},
          .rect = Rect::sharp(columnX(geometry, 1), 0.f, kCardW, kCardH),
          .count = 1,
          .draggable = false,
      });
      break;
    }
  }

  for (int i = 0; i < 4; ++i) {
    auto const& pile = board.foundations[static_cast<std::size_t>(i)];
    for (int cardIndex = static_cast<int>(pile.size()) - 1; cardIndex >= 0; --cardIndex) {
      Card card = pile[static_cast<std::size_t>(cardIndex)];
      if (cardIdIn(hiddenCardIds, card.id)) {
        continue;
      }
      bool const top = cardIndex == static_cast<int>(pile.size()) - 1;
      positions.push_back(CardPosition{
          .card = card,
          .source = Source{.kind = PileKind::Foundation, .index = i},
          .rect = Rect::sharp(columnX(geometry, 3 + i), 0.f, kCardW, kCardH),
          .count = 1,
          .draggable = top,
      });
      break;
    }
  }

  float const tableauTop = kCardH + kTopToTableauGap;
  for (int col = 0; col < 7; ++col) {
    auto const& column = board.tableau[static_cast<std::size_t>(col)];
    float y = tableauTop;
    for (int row = 0; row < static_cast<int>(column.size()); ++row) {
      Card const card = column[static_cast<std::size_t>(row)];
      if (!cardIdIn(hiddenCardIds, card.id)) {
        positions.push_back(CardPosition{
            .card = card,
            .source = Source{.kind = PileKind::Tableau, .index = col, .row = row},
            .rect = Rect::sharp(columnX(geometry, col), y, kCardW, kCardH),
            .count = static_cast<int>(column.size()) - row,
            .draggable = card.faceUp,
        });
      }
      y += card.faceUp ? kFanOpen : kFanClosed;
    }
  }

  return positions;
}

BoardGeometry boardGeometry(Size viewport) {
  float const inset = std::min(kBoardHorizontalInset, std::max(16.f, viewport.width * 0.08f));
  float const availableW = std::max(1.f, viewport.width - inset);
  float const availableH = std::max(1.f, viewport.height - 32.f);
  float const scale = std::max(0.01f, std::min(availableW / kMinBoardW, availableH / (kBoardTop + kBoardH)));
  return BoardGeometry{
      .origin = Point{(viewport.width - kMinBoardW * scale) * 0.5f, kBoardTop * scale},
      .scale = scale,
      .layoutWidth = kMinBoardW,
      .layoutHeight = kBoardH,
      .columnGap = kColGap,
      .viewport = viewport,
  };
}

Point toBoardPoint(Point local, BoardGeometry geometry) {
  return Point{
      (local.x - geometry.origin.x) / geometry.scale,
      (local.y - geometry.origin.y) / geometry.scale,
  };
}

Rect slotRect(PileKind kind, int index, BoardGeometry const& geometry) {
  if (kind == PileKind::Stock) {
    return Rect::sharp(0.f, 0.f, kCardW, kCardH);
  }
  if (kind == PileKind::Waste) {
    return Rect::sharp(columnX(geometry, 1), 0.f, kCardW, kCardH);
  }
  if (kind == PileKind::Foundation) {
    return Rect::sharp(columnX(geometry, 3 + index), 0.f, kCardW, kCardH);
  }
  if (kind == PileKind::Tableau) {
    return Rect::sharp(columnX(geometry, index), kCardH + kTopToTableauGap, kCardW, kCardH);
  }
  return {};
}

std::optional<CardPosition> hitCard(Board const& board, int drawCount, BoardGeometry const& geometry, Point p) {
  std::vector<CardPosition> positions = buildCardPositions(board, drawCount, geometry);
  for (auto it = positions.rbegin(); it != positions.rend(); ++it) {
    if (it->rect.contains(p) && it->draggable && it->card.faceUp) {
      return *it;
    }
  }
  return std::nullopt;
}

std::optional<Source> hitDestination(BoardGeometry const& geometry, Point p) {
  for (int i = 0; i < 4; ++i) {
    if (slotRect(PileKind::Foundation, i, geometry).contains(p)) {
      return Source{.kind = PileKind::Foundation, .index = i};
    }
  }
  for (int i = 0; i < 7; ++i) {
    Rect const column = Rect::sharp(columnX(geometry, i), kCardH + kTopToTableauGap,
                                   kCardW, geometry.layoutHeight - kCardH - kTopToTableauGap);
    if (column.contains(p)) {
      return Source{.kind = PileKind::Tableau, .index = i};
    }
  }
  return std::nullopt;
}

std::optional<Rect> rectForCard(std::vector<CardPosition> const& positions, int cardId) {
  for (CardPosition const& position : positions) {
    if (position.card.id == cardId) {
      return position.rect;
    }
  }
  return std::nullopt;
}

bool dragMoved(DragState const& drag, Point currentPoint, float threshold) {
  float const dx = currentPoint.x - drag.startPoint.x;
  float const dy = currentPoint.y - drag.startPoint.y;
  return dx * dx + dy * dy > threshold * threshold;
}

Rect dragCardRect(DragState const& drag, std::size_t index) {
  Point const origin = Point{drag.currentPoint.x - drag.grabOffset.x,
                             drag.currentPoint.y - drag.grabOffset.y};
  return Rect::sharp(origin.x, origin.y + static_cast<float>(index) * kFanOpen, kCardW, kCardH);
}

Point dragDropPoint(DragState const& drag) {
  return dragCardRect(drag, 0).center();
}

std::optional<Source> dropDestinationForDrag(BoardGeometry const& geometry, DragState const& drag) {
  if (!drag.active()) {
    return std::nullopt;
  }
  return hitDestination(geometry, dragDropPoint(drag));
}

bool moveWithAnimation(SolitaireState& state, Source source, Source dest, int count,
                       int drawCount, BoardGeometry const& geometry, std::int64_t startNanos) {
  std::vector<Card> moving = cardsFromSource(state.board, source, count);
  if (moving.empty()) {
    return false;
  }

  std::vector<CardPosition> const before = buildCardPositions(state.board, drawCount, geometry);
  std::vector<Rect> fromRects;
  fromRects.reserve(moving.size());
  for (Card const& card : moving) {
    fromRects.push_back(rectForCard(before, card.id).value_or(slotRect(source.kind, source.index, geometry)));
  }

  if (!performMove(state, source, dest, count)) {
    return false;
  }

  std::vector<CardPosition> const after = buildCardPositions(state.board, drawCount, geometry);
  std::vector<Rect> toRects;
  toRects.reserve(moving.size());
  for (Card const& card : moving) {
    toRects.push_back(rectForCard(after, card.id).value_or(slotRect(dest.kind, dest.index, geometry)));
  }

  state.animations.push_back(FlyAnimation{
      .cards = std::move(moving),
      .fromRects = std::move(fromRects),
      .toRects = std::move(toRects),
      .startNanos = startNanos,
  });
  state.frameNanos = startNanos;
  return true;
}

bool autoMoveWithAnimation(SolitaireState& state, Source source, int count, int drawCount,
                           BoardGeometry const& geometry, std::int64_t startNanos) {
  std::vector<Card> cards = cardsFromSource(state.board, source, count);
  if (cards.size() != 1) {
    return false;
  }
  std::optional<int> dest = findFoundationFor(state.board, cards.front());
  if (!dest) {
    return false;
  }
  return moveWithAnimation(state, source, Source{.kind = PileKind::Foundation, .index = *dest},
                           1, drawCount, geometry, startNanos);
}

bool sourceMatches(Source a, Source b, bool includeStack) {
  if (a.kind != b.kind || a.index != b.index) {
    return false;
  }
  if (a.kind == PileKind::Tableau && includeStack) {
    return a.row >= b.row;
  }
  return a.row == b.row;
}

bool cardIsAnimating(SolitaireState const& state, int cardId, std::int64_t now);

void handleBoardClick(Signal<SolitaireState> const& state, Signal<int> const& drawMode,
                      Point localPoint, Size viewport) {
  int const drawCount = drawMode.peek() == 0 ? 1 : 3;
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  if (p.x < -20.f || p.y < -20.f || p.x > geometry.layoutWidth + 20.f || p.y > geometry.layoutHeight + 80.f) {
    mutate(state, [](SolitaireState& s) {
      s.selection = {};
      s.peek = {};
    });
    return;
  }

  if (slotRect(PileKind::Stock, 0, geometry).contains(p)) {
    mutate(state, [drawCount](SolitaireState& s) { drawStock(s, drawCount); });
    return;
  }

  SolitaireState current = state();
  if (current.selection.active()) {
    if (std::optional<Source> dest = hitDestination(geometry, p)) {
      if (canMove(current.board, current.selection.source, current.selection.count, *dest)) {
        std::int64_t const startNanos = nowNanos();
        mutate(state, [dest, drawCount, geometry, startNanos](SolitaireState& s) {
          moveWithAnimation(s, s.selection.source, *dest, s.selection.count, drawCount, geometry, startNanos);
        });
        return;
      }
    }
  }

  if (std::optional<CardPosition> hit = hitCard(current.board, drawCount, geometry, p)) {
    if (current.selection.active() && sourceMatches(hit->source, current.selection.source, false)) {
      std::int64_t const startNanos = nowNanos();
      mutate(state, [drawCount, geometry, startNanos](SolitaireState& s) {
        autoMoveWithAnimation(s, s.selection.source, s.selection.count, drawCount, geometry, startNanos);
        s.selection = {};
      });
      return;
    }
    mutate(state, [hit](SolitaireState& s) {
      s.selection = Selection{.source = hit->source, .count = hit->count};
      s.hint = {};
    });
    return;
  }

  mutate(state, [](SolitaireState& s) {
    s.selection = {};
    s.peek = {};
  });
}

void startBoardDrag(Signal<SolitaireState> const& state, Signal<int> const& drawMode,
                    Point localPoint, Size viewport) {
  int const drawCount = drawMode.peek() == 0 ? 1 : 3;
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  SolitaireState const current = state();
  std::int64_t const animationNow = current.frameNanos > 0 ? current.frameNanos : nowNanos();

  std::optional<CardPosition> hit = hitCard(current.board, drawCount, geometry, p);
  if (!hit || cardIsAnimating(current, hit->card.id, animationNow)) {
    return;
  }

  std::vector<Card> cards = cardsFromSource(current.board, hit->source, hit->count);
  if (cards.empty()) {
    return;
  }

  mutate(state, [hit, p, cards = std::move(cards)](SolitaireState& s) mutable {
    s.drag = DragState{
        .source = hit->source,
        .count = hit->count,
        .cards = std::move(cards),
        .startPoint = p,
        .currentPoint = p,
        .grabOffset = Point{p.x - hit->rect.x, p.y - hit->rect.y},
    };
    s.peek = {};
  });
}

void updateBoardDrag(Signal<SolitaireState> const& state, Point localPoint, Size viewport) {
  if (!state.peek().drag.active()) {
    return;
  }
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  float const threshold = 4.f / std::max(0.01f, geometry.scale);

  mutate(state, [p, threshold](SolitaireState& s) {
    if (!s.drag.active()) {
      return;
    }
    bool const movedNow = s.drag.moved || dragMoved(s.drag, p, threshold);
    s.drag.currentPoint = p;
    if (movedNow && !s.drag.moved) {
      s.selection = {};
      s.hint = {};
      s.peek = {};
    }
    s.drag.moved = movedNow;
  });
}

void finishBoardDrag(Signal<SolitaireState> const& state, Signal<int> const& drawMode,
                     Point localPoint, Size viewport) {
  int const drawCount = drawMode.peek() == 0 ? 1 : 3;
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  float const threshold = 4.f / std::max(0.01f, geometry.scale);

  SolitaireState const current = state();
  if (!current.drag.active()) {
    handleBoardClick(state, drawMode, localPoint, viewport);
    return;
  }

  DragState drag = current.drag;
  drag.currentPoint = p;
  drag.moved = drag.moved || dragMoved(drag, p, threshold);
  if (!drag.moved) {
    mutate(state, [](SolitaireState& s) { s.drag = {}; });
    handleBoardClick(state, drawMode, localPoint, viewport);
    return;
  }

  std::vector<Rect> fromRects;
  fromRects.reserve(drag.cards.size());
  for (std::size_t i = 0; i < drag.cards.size(); ++i) {
    fromRects.push_back(dragCardRect(drag, i));
  }

  std::optional<Source> dest = dropDestinationForDrag(geometry, drag);
  if (dest && canMove(current.board, drag.source, drag.count, *dest)) {
    std::int64_t const startNanos = nowNanos();
    mutate(state, [drag, dest, drawCount, geometry, startNanos, fromRects = std::move(fromRects)](SolitaireState& s) mutable {
      if (performMove(s, drag.source, *dest, drag.count)) {
        std::vector<CardPosition> const after = buildCardPositions(s.board, drawCount, geometry);
        std::vector<Rect> toRects;
        toRects.reserve(drag.cards.size());
        for (Card const& card : drag.cards) {
          toRects.push_back(rectForCard(after, card.id).value_or(slotRect(dest->kind, dest->index, geometry)));
        }
        s.animations.push_back(FlyAnimation{
            .cards = drag.cards,
            .fromRects = std::move(fromRects),
            .toRects = std::move(toRects),
            .startNanos = startNanos,
            .durationNanos = kDropFlyDurationNanos,
        });
        s.frameNanos = startNanos;
      }
      s.drag = {};
    });
    return;
  }

  std::int64_t const startNanos = nowNanos();
  std::vector<CardPosition> const currentPositions = buildCardPositions(current.board, drawCount, geometry);
  std::vector<Rect> toRects;
  toRects.reserve(drag.cards.size());
  for (Card const& card : drag.cards) {
    toRects.push_back(rectForCard(currentPositions, card.id).value_or(slotRect(drag.source.kind, drag.source.index, geometry)));
  }
  mutate(state, [drag, startNanos, fromRects = std::move(fromRects), toRects = std::move(toRects)](SolitaireState& s) mutable {
    s.animations.push_back(FlyAnimation{
        .cards = drag.cards,
        .fromRects = std::move(fromRects),
        .toRects = std::move(toRects),
        .startNanos = startNanos,
        .durationNanos = kDropFlyDurationNanos,
    });
    s.frameNanos = startNanos;
    s.drag = {};
  });
}

void startBoardPeek(Signal<SolitaireState> const& state, Signal<int> const& drawMode,
                    Point localPoint, Size viewport) {
  int const drawCount = drawMode.peek() == 0 ? 1 : 3;
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  SolitaireState const current = state();
  std::vector<CardPosition> positions = buildCardPositions(current.board, drawCount, geometry);

  for (auto it = positions.rbegin(); it != positions.rend(); ++it) {
    if (it->source.kind != PileKind::Tableau || !it->card.faceUp || !it->rect.contains(p)) {
      continue;
    }
    auto const& column = current.board.tableau[static_cast<std::size_t>(it->source.index)];
    if (it->source.row >= static_cast<int>(column.size()) - 1) {
      continue;
    }
    Card card = it->card;
    card.faceUp = true;
    mutate(state, [card, source = it->source, rect = it->rect](SolitaireState& s) {
      s.peek = PeekState{
          .source = source,
          .card = card,
          .rect = rect,
          .expiresNanos = nowNanos() + 60'000'000'000,
      };
    });
    return;
  }

  mutate(state, [](SolitaireState& s) { s.peek = {}; });
}

void stopBoardPeek(Signal<SolitaireState> const& state) {
  mutate(state, [](SolitaireState& s) { s.peek = {}; });
}

void showHint(Signal<SolitaireState> const& state) {
  mutate(state, [](SolitaireState& s) {
    Board const& board = s.board;
    if (!board.waste.empty()) {
      Source source{.kind = PileKind::Waste};
      Card const& card = board.waste.back();
      if (findFoundationFor(board, card)) {
        s.hint = Hint{.source = source};
        return;
      }
      for (int col = 0; col < 7; ++col) {
        auto const& dest = board.tableau[static_cast<std::size_t>(col)];
        Card const* top = dest.empty() ? nullptr : &dest.back();
        if (canPlaceOnTableau(card, top)) {
          s.hint = Hint{.source = source};
          return;
        }
      }
    }

    for (int col = 0; col < 7; ++col) {
      auto const& column = board.tableau[static_cast<std::size_t>(col)];
      if (column.empty()) {
        continue;
      }
      int const topRow = static_cast<int>(column.size()) - 1;
      Card const& top = column.back();
      Source topSource{.kind = PileKind::Tableau, .index = col, .row = topRow};
      if (top.faceUp && findFoundationFor(board, top)) {
        s.hint = Hint{.source = topSource};
        return;
      }
      for (int row = 0; row < static_cast<int>(column.size()); ++row) {
        Card const& moving = column[static_cast<std::size_t>(row)];
        if (!moving.faceUp) {
          continue;
        }
        for (int destCol = 0; destCol < 7; ++destCol) {
          if (destCol == col) {
            continue;
          }
          auto const& dest = board.tableau[static_cast<std::size_t>(destCol)];
          Card const* destTop = dest.empty() ? nullptr : &dest.back();
          if (canPlaceOnTableau(moving, destTop)) {
            s.hint = Hint{.source = Source{.kind = PileKind::Tableau, .index = col, .row = row}};
            return;
          }
        }
      }
    }

    s.hint = Hint{.source = Source{.kind = PileKind::Stock}};
  });
}

void undo(Signal<SolitaireState> const& state) {
  mutate(state, [](SolitaireState& s) {
    if (s.history.empty()) {
      return;
    }
    HistoryEntry prev = s.history.back();
    s.history.pop_back();
    s.board = std::move(prev.board);
    s.moves = prev.moves;
    s.score = prev.score;
    s.selection = {};
    s.hint = {};
    s.peek = {};
    s.drag = {};
  });
}

void newGame(Signal<SolitaireState> const& state) {
  std::uint32_t nextSeed = randomSeed();
  if (nextSeed == state.peek().seed) {
    ++nextSeed;
  }
  state = makeState(nextSeed);
}

void drawText(Canvas& canvas, std::string_view text, Font font, Color color, Point origin,
              float maxWidth = 0.f) {
  auto layout = Application::instance().textSystem().layout(text, font, color, maxWidth);
  canvas.drawTextLayout(*layout, origin);
}

void drawCenteredText(Canvas& canvas, std::string_view text, Font font, Color color, Point center) {
  auto layout = Application::instance().textSystem().layout(text, font, color, 0.f);
  Size const size = layout->measuredSize;
  canvas.drawTextLayout(*layout, Point{center.x - size.width * 0.5f, center.y - size.height * 0.5f});
}

void drawRotatedCenteredText(Canvas& canvas, std::string_view text, Font font, Color color,
                             Point center, float radians) {
  canvas.save();
  canvas.translate(center);
  canvas.rotate(radians);
  auto layout = Application::instance().textSystem().layout(text, font, color, 0.f);
  Size const size = layout->measuredSize;
  canvas.drawTextLayout(*layout, Point{-size.width * 0.5f, -size.height * 0.5f});
  canvas.restore();
}

void drawSuit(Canvas& canvas, Suit suit, Point center, float size, Color color, float radians = 0.f) {
  Font const font{.size = size * 1.12f, .weight = 400.f};
  if (radians != 0.f) {
    drawRotatedCenteredText(canvas, suitGlyph(suit), font, color, center, radians);
  } else {
    drawCenteredText(canvas, suitGlyph(suit), font, color, center);
  }
}

void drawSlot(Canvas& canvas, Rect rect, std::string_view marker, bool highlighted) {
  Color const fill = highlighted ? colorHex(0x0A84FF, 0.18f) : Color{0.f, 0.f, 0.f, 0.18f};
  Color const stroke = highlighted ? colorHex(0x0A84FF, 0.95f) : Colors::white;
  float const strokeAlpha = highlighted ? 1.f : 0.25f;
  canvas.drawRect(rect, CornerRadius{8.f}, FillStyle::solid(fill),
                  StrokeStyle::solid(withAlpha(stroke, strokeAlpha), highlighted ? 2.f : 1.2f));
  if (!marker.empty()) {
    Color const markerColor = highlighted ? Colors::white : withAlpha(Colors::white, 0.36f);
    drawCenteredText(canvas, marker, Font{.size = 36.f, .weight = 300.f}, markerColor, rect.center());
  }
}

void drawCardBack(Canvas& canvas, Rect rect) {
  canvas.drawRect(rect, CornerRadius{8.f},
                  FillStyle::linearGradient({{0.f, Color::hex(0x1E3FA8)},
                                             {0.48f, Color::hex(0x2563EB)},
                                             {1.f, Color::hex(0x0A84FF)}},
                                            Point{0.f, 0.f}, Point{1.f, 1.f}),
                  StrokeStyle::solid(colorHex(0x0A2A5C), 1.4f),
                  ShadowStyle{.radius = 4.f, .offset = {0.f, 1.f}, .color = Color{0.f, 0.f, 0.f, 0.24f}});

  canvas.save();
  canvas.clipRect(rect, CornerRadius{8.f}, true);
  for (float x = rect.x - rect.height; x < rect.x + rect.width + rect.height; x += 8.f) {
    canvas.drawLine(Point{x, rect.y}, Point{x + rect.height, rect.y + rect.height},
                    StrokeStyle::solid(Color{1.f, 1.f, 1.f, 0.13f}, 0.7f));
  }
  canvas.restore();

  canvas.drawRect(Rect::sharp(rect.x + 5.f, rect.y + 5.f, rect.width - 10.f, rect.height - 10.f),
                  CornerRadius{5.f}, FillStyle::none(), StrokeStyle::solid(Color{1.f, 1.f, 1.f, 0.46f}, 0.8f));
  canvas.drawRect(Rect::sharp(rect.x + 8.f, rect.y + 8.f, rect.width - 16.f, rect.height - 16.f),
                  CornerRadius{4.f}, FillStyle::none(), StrokeStyle::solid(Color{1.f, 1.f, 1.f, 0.24f}, 0.6f));
  canvas.drawCircle(rect.center(), 18.f, FillStyle::solid(Color{1.f, 1.f, 1.f, 0.08f}),
                    StrokeStyle::solid(Color{1.f, 1.f, 1.f, 0.45f}, 0.8f));
  drawCenteredText(canvas, "λ", Font{.size = 29.f, .weight = 650.f}, Color{1.f, 1.f, 1.f, 0.88f},
                   Point{rect.center().x, rect.center().y - 1.f});
}

std::vector<Point> pipLayout(int rank) {
  switch (rank) {
  case 2:
    return {{0.5f, 0.18f}, {0.5f, 0.82f}};
  case 3:
    return {{0.5f, 0.18f}, {0.5f, 0.5f}, {0.5f, 0.82f}};
  case 4:
    return {{0.25f, 0.18f}, {0.75f, 0.18f}, {0.25f, 0.82f}, {0.75f, 0.82f}};
  case 5:
    return {{0.25f, 0.18f}, {0.75f, 0.18f}, {0.5f, 0.5f}, {0.25f, 0.82f}, {0.75f, 0.82f}};
  case 6:
    return {{0.25f, 0.18f}, {0.75f, 0.18f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.82f}, {0.75f, 0.82f}};
  case 7:
    return {{0.25f, 0.18f}, {0.75f, 0.18f}, {0.5f, 0.32f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.25f, 0.82f}, {0.75f, 0.82f}};
  case 8:
    return {{0.25f, 0.18f}, {0.75f, 0.18f}, {0.5f, 0.32f}, {0.25f, 0.5f}, {0.75f, 0.5f}, {0.5f, 0.68f}, {0.25f, 0.82f}, {0.75f, 0.82f}};
  case 9:
    return {{0.25f, 0.18f}, {0.75f, 0.18f}, {0.25f, 0.36f}, {0.75f, 0.36f}, {0.5f, 0.5f}, {0.25f, 0.64f}, {0.75f, 0.64f}, {0.25f, 0.82f}, {0.75f, 0.82f}};
  case 10:
    return {{0.25f, 0.18f}, {0.75f, 0.18f}, {0.5f, 0.28f}, {0.25f, 0.4f}, {0.75f, 0.4f}, {0.25f, 0.6f}, {0.75f, 0.6f}, {0.5f, 0.72f}, {0.25f, 0.82f}, {0.75f, 0.82f}};
  default:
    return {};
  }
}

Point pipPoint(Rect rect, Point unit) {
  return Point{
      rect.x + rect.width * (0.28f + 0.44f * unit.x),
      rect.y + rect.height * (0.18f + 0.64f * unit.y),
  };
}

void drawCardFace(Canvas& canvas, Card const& card, Rect rect) {
  Color const ink = suitColor(card.suit);
  canvas.drawRect(rect, CornerRadius{8.f},
                  FillStyle::linearGradient(Color::hex(0xFFFFFF), Color::hex(0xFAFAFC), Point{0.f, 0.f}, Point{0.f, 1.f}),
                  StrokeStyle::solid(Color{0.f, 0.f, 0.f, 0.18f}, 1.f),
                  ShadowStyle{.radius = 3.f, .offset = {0.f, 1.f}, .color = Color{0.f, 0.f, 0.f, 0.18f}});

  std::string const rank = rankLabel(card.rank);
  Font const cornerFont{.size = card.rank == 10 ? 15.f : 17.f, .weight = 650.f};
  drawCenteredText(canvas, rank, cornerFont, ink, Point{rect.x + 15.f, rect.y + 16.f});
  drawSuit(canvas, card.suit, Point{rect.x + 15.f, rect.y + 35.f}, 11.5f, ink);

  drawRotatedCenteredText(canvas, rank, cornerFont, ink, Point{rect.x + rect.width - 15.f, rect.y + rect.height - 16.f}, kPi);
  drawSuit(canvas, card.suit, Point{rect.x + rect.width - 15.f, rect.y + rect.height - 35.f}, 11.5f, ink, kPi);

  if (card.rank == 1) {
    drawSuit(canvas, card.suit, rect.center(), 58.f, ink);
  } else if (card.rank >= 11) {
    drawCenteredText(canvas, rank, Font{.size = 64.f, .weight = 750.f}, ink,
                     Point{rect.center().x, rect.center().y + 1.f});
  } else {
    float const pipSize = card.rank >= 7 ? 15.f : 17.f;
    for (Point const unit : pipLayout(card.rank)) {
      drawSuit(canvas, card.suit, pipPoint(rect, unit), pipSize, ink, unit.y > 0.5f ? kPi : 0.f);
    }
  }
}

void drawCard(Canvas& canvas, Card const& card, Rect rect, bool selected, bool hinted) {
  if (card.faceUp) {
    drawCardFace(canvas, card, rect);
  } else {
    drawCardBack(canvas, rect);
  }
  if (selected || hinted) {
    Color const accent = selected ? colorHex(0x0A84FF, 0.95f) : colorHex(0xFBBF24, 0.95f);
    canvas.drawRect(Rect::sharp(rect.x - 2.f, rect.y - 2.f, rect.width + 4.f, rect.height + 4.f),
                    CornerRadius{10.f}, FillStyle::none(), StrokeStyle::solid(accent, 2.5f));
    canvas.drawRect(Rect::sharp(rect.x - 5.f, rect.y - 5.f, rect.width + 10.f, rect.height + 10.f),
                    CornerRadius{12.f}, FillStyle::none(), StrokeStyle::solid(withAlpha(accent, 0.35f), 4.f));
  }
}

bool animationActive(FlyAnimation const& animation, std::int64_t now) {
  return now < animation.startNanos + animation.durationNanos;
}

bool cardIsAnimating(SolitaireState const& state, int cardId, std::int64_t now) {
  for (FlyAnimation const& animation : state.animations) {
    if (!animationActive(animation, now)) {
      continue;
    }
    for (Card const& card : animation.cards) {
      if (card.id == cardId) {
        return true;
      }
    }
  }
  return false;
}

std::vector<int> hiddenCardIds(SolitaireState const& state, std::int64_t now) {
  std::vector<int> ids;
  for (FlyAnimation const& animation : state.animations) {
    if (!animationActive(animation, now)) {
      continue;
    }
    for (Card const& card : animation.cards) {
      ids.push_back(card.id);
    }
  }
  if (state.drag.active() && state.drag.moved) {
    for (Card const& card : state.drag.cards) {
      ids.push_back(card.id);
    }
  }
  return ids;
}

float easedProgress(FlyAnimation const& animation, std::int64_t now) {
  double const raw = static_cast<double>(now - animation.startNanos) /
                     static_cast<double>(std::max<std::int64_t>(1, animation.durationNanos));
  float const t = std::clamp(static_cast<float>(raw), 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

Rect lerpRect(Rect from, Rect to, float t) {
  float const lift = std::sin(t * kPi) * 22.f;
  return Rect::sharp(
      from.x + (to.x - from.x) * t,
      from.y + (to.y - from.y) * t - lift,
      from.width + (to.width - from.width) * t,
      from.height + (to.height - from.height) * t);
}

void drawFlyAnimations(Canvas& canvas, SolitaireState const& state, std::int64_t now) {
  for (FlyAnimation const& animation : state.animations) {
    if (!animationActive(animation, now)) {
      continue;
    }
    float const t = easedProgress(animation, now);
    for (std::size_t i = 0; i < animation.cards.size(); ++i) {
      Rect const from = i < animation.fromRects.size() ? animation.fromRects[i] : Rect::sharp(0.f, 0.f, kCardW, kCardH);
      Rect const to = i < animation.toRects.size() ? animation.toRects[i] : from;
      drawCard(canvas, animation.cards[i], lerpRect(from, to, t), false, false);
    }
  }
}

void drawDraggedCards(Canvas& canvas, DragState const& drag) {
  if (!drag.active() || !drag.moved) {
    return;
  }
  for (std::size_t i = 0; i < drag.cards.size(); ++i) {
    drawCard(canvas, drag.cards[i], dragCardRect(drag, i), true, false);
  }
}

std::pair<Color, Color> feltColors(int feltIndex) {
  switch (feltIndex) {
  case 1:
    return {Color::hex(0x1E3FA8), Color::hex(0x0F1E5A)};
  case 2:
    return {Color::hex(0x2A2A2E), Color::hex(0x0A0A0C)};
  case 3:
    return {Color::hex(0x7F1D1D), Color::hex(0x2D0808)};
  default:
    return {Color::hex(0x1F6F4A), Color::hex(0x0A3220)};
  }
}

void drawFelt(Canvas& canvas, Rect frame, int feltIndex) {
  auto [top, bottom] = feltColors(feltIndex);
  canvas.drawRect(frame, {}, FillStyle::linearGradient(top, bottom, Point{0.f, 0.f}, Point{0.f, 1.f}),
                  StrokeStyle::none());

  for (int i = 0; i < 44; ++i) {
    float const x = frame.x + std::fmod(static_cast<float>(i * 79), std::max(1.f, frame.width));
    float const y = frame.y + std::fmod(static_cast<float>(i * 47), std::max(1.f, frame.height));
    float const r = 18.f + static_cast<float>(i % 7) * 7.f;
    canvas.drawCircle(Point{x, y}, r, FillStyle::solid(Color{1.f, 1.f, 1.f, 0.012f}), StrokeStyle::none());
  }

  canvas.drawRect(frame, {}, FillStyle::radialGradient(Color{0.f, 0.f, 0.f, 0.f}, Color{0.f, 0.f, 0.f, 0.42f},
                                                       Point{0.5f, 0.46f}, 0.72f),
                  StrokeStyle::none());
}

void drawBoard(Canvas& canvas, Rect frame, SolitaireState const& state, int drawCount, int feltIndex) {
  drawFelt(canvas, frame, feltIndex);

  BoardGeometry const geometry = boardGeometry(Size{frame.width, frame.height});
  std::int64_t const animationNow = state.frameNanos > 0 ? state.frameNanos : nowNanos();
  std::vector<int> const hiddenIds = hiddenCardIds(state, animationNow);
  canvas.save();
  canvas.translate(frame.x + geometry.origin.x, frame.y + geometry.origin.y);
  canvas.scale(geometry.scale);

  Selection const selection = state.selection;
  std::optional<Source> const dragDest = dropDestinationForDrag(geometry, state.drag);
  auto validDrop = [&](PileKind kind, int index) {
    Source const dest{.kind = kind, .index = index};
    bool const selectionDrop = selection.active() && canMove(state.board, selection.source, selection.count, dest);
    bool const dragDrop = dragDest && *dragDest == dest &&
                          canMove(state.board, state.drag.source, state.drag.count, dest);
    return selectionDrop || dragDrop;
  };

  drawSlot(canvas, slotRect(PileKind::Stock, 0, geometry), state.board.stock.empty() ? "↻" : "",
           state.hint.source.kind == PileKind::Stock);
  drawSlot(canvas, slotRect(PileKind::Waste, 0, geometry), "", false);
  for (int i = 0; i < 4; ++i) {
    Suit const suit = std::array<Suit, 4>{Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs}
        [static_cast<std::size_t>(i)];
    std::string_view marker = suitGlyph(suit);
    drawSlot(canvas, slotRect(PileKind::Foundation, i, geometry), marker, validDrop(PileKind::Foundation, i));
  }
  for (int i = 0; i < 7; ++i) {
    drawSlot(canvas, slotRect(PileKind::Tableau, i, geometry), "", validDrop(PileKind::Tableau, i));
  }

  for (CardPosition const& pos : buildCardPositions(state.board, drawCount, geometry, hiddenIds)) {
    bool const selected = selection.active() && sourceMatches(pos.source, selection.source, true);
    bool const hinted = state.hint.active() && sourceMatches(pos.source, state.hint.source, false);
    drawCard(canvas, pos.card, pos.rect, selected, hinted);
  }

  drawFlyAnimations(canvas, state, animationNow);
  drawDraggedCards(canvas, state.drag);
  if (state.peek.active(animationNow)) {
    drawCard(canvas, state.peek.card, state.peek.rect, false, true);
  }

  canvas.restore();
}

struct StatView : ViewModifiers<StatView> {
  IconName icon{};
  std::string label;
  Bindable<std::string> value;
  bool accent = false;

  auto body() const {
    auto theme = useEnvironment<ThemeKey>();
    return VStack{
        .spacing = 1.f,
        .alignment = Alignment::Start,
        .children = children(
            HStack{
                .spacing = 3.f,
                .alignment = Alignment::Center,
                .children = children(
                    Icon{.name = icon, .size = 12.f, .weight = 400.f, .color = Color::tertiary()},
                    Text{.text = label, .font = Font{.size = 10.f, .weight = 600.f}, .color = Color::tertiary()}
                ),
            },
            Text{
                .text = value,
                .font = Font{.size = 13.f, .weight = 650.f},
                .color = accent ? Color::accent() : Color::primary(),
            }
        ),
    }.width(68.f);
  }
};

struct BrandMark : ViewModifiers<BrandMark> {
  auto body() const {
    return ZStack{
        .horizontalAlignment = Alignment::Center,
        .verticalAlignment = Alignment::Center,
        .children = children(
            Rectangle{}
                .size(28.f, 28.f)
                .fill(FillStyle::linearGradient({{0.f, Color::hex(0x1E3FA8)},
                                                 {0.5f, Color::hex(0x2563EB)},
                                                 {1.f, Color::hex(0x0A84FF)}},
                                                Point{0.f, 0.f}, Point{1.f, 1.f}))
                .cornerRadius(7.f)
                .shadow(ShadowStyle{.radius = 3.f, .offset = {0.f, 1.f}, .color = Color{0.f, 0.f, 0.f, 0.2f}}),
            Text{
                .text = "λ",
                .font = Font{.size = 18.f, .weight = 650.f},
                .color = Colors::white,
                .horizontalAlignment = HorizontalAlignment::Center,
                .verticalAlignment = VerticalAlignment::Center,
            }
        ),
    }.size(28.f, 28.f);
  }
};

struct BoardSurface : ViewModifiers<BoardSurface> {
  Signal<SolitaireState> state;
  Signal<int> drawMode;
  Signal<int> feltIndex;

  auto body() const {
    Rect const bounds = useBounds();
    auto viewport = std::make_shared<Size>(Size{
        bounds.width > 0.f ? bounds.width : 1200.f,
        bounds.height > 0.f ? bounds.height : 760.f,
    });

    auto stateSignal = state;
    auto drawModeSignal = drawMode;
    auto feltSignal = feltIndex;

    return Render{
        .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
          float const width = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 1200.f;
          float const height = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 760.f;
          return Size{std::max(1.f, width), std::max(1.f, height)};
        },
        .draw = [stateSignal, drawModeSignal, feltSignal, viewport](Canvas& canvas, Rect frame) {
          *viewport = Size{frame.width, frame.height};
          int const drawCount = drawModeSignal.evaluate() == 0 ? 1 : 3;
          drawBoard(canvas, frame, stateSignal.evaluate(), drawCount, feltSignal.evaluate());
        },
    }
        .flex(1.f, 1.f)
        .cursor(Cursor::Hand)
        .onPointerDown(std::function<void(Point, MouseButton)>{[stateSignal, drawModeSignal, viewport](Point p, MouseButton button) {
          if (button == MouseButton::Right) {
            startBoardPeek(stateSignal, drawModeSignal, p, *viewport);
            return;
          }
          if (button == MouseButton::Left) {
            startBoardDrag(stateSignal, drawModeSignal, p, *viewport);
          }
        }})
        .onPointerMove(std::function<void(Point)>{[stateSignal, viewport](Point p) {
          updateBoardDrag(stateSignal, p, *viewport);
        }})
        .onPointerUp(std::function<void(Point, MouseButton)>{[stateSignal, drawModeSignal, viewport](Point p, MouseButton button) {
          if (button == MouseButton::Right) {
            stopBoardPeek(stateSignal);
            return;
          }
          if (button == MouseButton::Left) {
            finishBoardDrag(stateSignal, drawModeSignal, p, *viewport);
          }
        }});
  }

  bool operator==(BoardSurface const& other) const {
    return state == other.state && drawMode == other.drawMode && feltIndex == other.feltIndex;
  }
};

struct RootView : ViewModifiers<RootView> {
  auto body() const {
    auto state = useState<SolitaireState>(makeState(randomSeed()));
    auto drawMode = useState<int>(0);
    auto feltIndex = useState<int>(0);

    useFrame([state](AnimationTick const& tick) {
      SolitaireState current = state.peek();
      bool changed = false;
      int const elapsed = static_cast<int>(std::max<std::int64_t>(0, tick.deadlineNanos - current.startedNanos) /
                                           1'000'000'000);
      if (elapsed != current.elapsedSeconds) {
        current.elapsedSeconds = elapsed;
        changed = true;
      }
      if (!current.animations.empty()) {
        current.frameNanos = tick.deadlineNanos;
        auto const oldSize = current.animations.size();
        current.animations.erase(
            std::remove_if(current.animations.begin(), current.animations.end(), [now = tick.deadlineNanos](FlyAnimation const& animation) {
              return !animationActive(animation, now);
            }),
            current.animations.end());
        changed = true;
        (void)oldSize;
      }
      if (current.peek.source.kind != PileKind::None && !current.peek.active(tick.deadlineNanos)) {
        current.peek = {};
        changed = true;
      }
      if (changed) {
        state = std::move(current);
      }
    });

    auto theme = useEnvironment<ThemeKey>();

    return VStack{
        .spacing = 0.f,
        .alignment = Alignment::Stretch,
        .children = children(
            HStack{
                .spacing = theme().space3,
                .alignment = Alignment::Center,
                .children = children(
                    HStack{
                        .spacing = theme().space2,
                        .alignment = Alignment::Center,
                        .children = children(
                            BrandMark{},
                            Text{.text = "Lambda Solitaire", .font = Font{.size = 13.f, .weight = 700.f}, .color = Color::primary()},
                            Divider{.orientation = Divider::Orientation::Vertical}.height(20.f),
                            HStack{
                                .spacing = 4.f,
                                .alignment = Alignment::Center,
                                .children = children(
                                    Icon{.name = IconName::Casino, .size = 14.f, .weight = 400.f, .color = Color::tertiary()},
                                    Text{
                                        .text = [drawMode] {
                                          return drawMode.evaluate() == 0 ? std::string{"Klondike - Draw 1"}
                                                                         : std::string{"Klondike - Draw 3"};
                                        },
                                        .font = Font{.size = 11.f, .weight = 500.f},
                                        .color = Color::tertiary(),
                                    }
                                ),
                            }
                        ),
                    },
                    Spacer{},
                    HStack{
                        .spacing = theme().space4,
                        .alignment = Alignment::Center,
                        .children = children(
                            StatView{
                                .icon = IconName::Schedule,
                                .label = "TIME",
                                .value = [state] { return formatTime(state.evaluate().elapsedSeconds); },
                            },
                            StatView{
                                .icon = IconName::SwapVert,
                                .label = "MOVES",
                                .value = [state] { return std::to_string(state.evaluate().moves); },
                            },
                            StatView{
                                .icon = IconName::Trophy,
                                .label = "SCORE",
                                .value = [state] { return std::to_string(state.evaluate().score); },
                                .accent = true,
                            }
                        ),
                    },
                    Divider{.orientation = Divider::Orientation::Vertical}.height(20.f),
                    Button{
                        .label = "Undo",
                        .variant = ButtonVariant::Secondary,
                        .disabled = [state] { return state.evaluate().history.empty(); },
                        .onTap = [state] { undo(state); },
                    },
                    Button{
                        .label = "Hint",
                        .variant = ButtonVariant::Secondary,
                        .onTap = [state] { showHint(state); },
                    },
                    Button{
                        .label = "New Game",
                        .variant = ButtonVariant::Primary,
                        .onTap = [state] { newGame(state); },
                    },
                    SegmentedControl{
                        .selectedIndex = drawMode,
                        .options = {SegmentedControlOption{.label = "Draw 1"},
                                    SegmentedControlOption{.label = "Draw 3"}},
                        .onChange = [drawMode](int index) { drawMode = index; },
                    }.width(158.f),
                    Select{
                        .selectedIndex = feltIndex,
                        .options = {SelectOption{.label = "Emerald"},
                                    SelectOption{.label = "Sapphire"},
                                    SelectOption{.label = "Obsidian"},
                                    SelectOption{.label = "Crimson"}},
                        .placeholder = "Felt",
                        .showDetailInTrigger = false,
                        .onChange = [feltIndex](int index) { feltIndex = index; },
                    }.width(132.f)
                ),
            }
                .padding(12.f, 20.f, 12.f, 20.f)
                .fill(Color::elevatedBackground())
                .stroke(Color::separator(), 1.f)
                .flex(0.f, 0.f),
            BoardSurface{.state = state, .drawMode = drawMode, .feltIndex = feltIndex}.flex(1.f, 1.f)
        ),
    }.fill(Color::windowBackground());
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& window = app.createWindow<Window>({
      .size = {1200, 820},
      .title = "Lambda Solitaire",
      .resizable = true,
  });
  window.setTheme(Theme::light());
  window.setView<RootView>();

  return app.exec();
}
