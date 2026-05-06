#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
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
constexpr float kBoardH = 640.f;
constexpr float kBoardTop = 108.f;
constexpr float kBoardHorizontalInset = 40.f;
constexpr std::int64_t kDropFlyDurationNanos = 200'000'000;
constexpr std::int64_t kDealFlyDurationNanos = 120'000'000;
constexpr std::int64_t kDealFlyIntervalNanos = 120'000'000;
constexpr std::int64_t kAutoFlyDurationNanos = 85'000'000;
constexpr std::int64_t kWinCelebrationDurationNanos = 8'000'000'000;
constexpr std::int64_t kWinCardIntervalNanos = 160'000'000;
constexpr std::int64_t kHintBounceCycleNanos = 380'000'000;
constexpr int kHintBounceCount = 2;
constexpr std::int64_t kHintAnimationDurationNanos = kHintBounceCycleNanos * kHintBounceCount;
constexpr int kElapsedSaveIntervalSeconds = 15;
constexpr float kHintBounceAmplitude = 16.f;
constexpr float kSelectedCardScale = 1.035f;
constexpr float kMovingCardScale = 1.07f;
constexpr std::int64_t kSelectionScaleDurationNanos = 140'000'000;
constexpr float kWinCelebrationFloorSpeedScale = 0.25f;
constexpr int kDrawCount = 1;

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
  bool completed = false;

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

struct SelectionScaleAnimation {
  Source source;
  int count = 0;
  float fromScale = 1.f;
  float toScale = 1.f;
  std::int64_t startNanos = 0;

  bool active(std::int64_t now) const {
    return source.kind != PileKind::None && count > 0 &&
           now < startNanos + kSelectionScaleDurationNanos;
  }

  bool operator==(SelectionScaleAnimation const&) const = default;
};

struct Hint {
  Source source;
  std::int64_t startedNanos = 0;

  bool active() const { return source.kind != PileKind::None; }
  bool animating(std::int64_t now) const {
    return active() && startedNanos > 0 &&
           now < startedNanos + kHintAnimationDurationNanos;
  }
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
  std::vector<SelectionScaleAnimation> selectionScaleAnimations;
  Selection selection;
  Hint hint;
  PeekState peek;
  DragState drag;
  int moves = 0;
  int score = 0;
  int elapsedSeconds = 0;
  std::uint32_t seed = 1;
  std::uint64_t saveRevision = 1;
  std::int64_t startedNanos = 0;
  std::int64_t frameNanos = 0;
  bool completed = false;
  bool dealing = false;
  bool autoFinishing = false;
  std::int64_t celebrationStartNanos = 0;

  bool operator==(SolitaireState const&) const = default;
};

bool dealAnimationRunning(SolitaireState const& state) {
  return state.dealing && !state.animations.empty();
}

bool playControlsDisabled(SolitaireState const& state) {
  return state.completed || state.autoFinishing || dealAnimationRunning(state);
}

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

FillStyle glassFill() {
  return FillStyle::solid(Color{20.f / 255.f, 22.f / 255.f, 28.f / 255.f, 0.42f});
}

StrokeStyle glassStroke() {
  return StrokeStyle::solid(Color{1.f, 1.f, 1.f, 0.12f}, 1.f);
}

ShadowStyle glassShadow(float alpha = 0.22f) {
  return ShadowStyle{.radius = 12.f, .offset = {0.f, 4.f}, .color = Color{0.f, 0.f, 0.f, alpha}};
}

std::pair<Color, Color> feltColors(int feltIndex);

FillStyle feltPreviewFill(int feltIndex) {
  auto [top, bottom] = feltColors(feltIndex);
  Color mid = top;
  switch (feltIndex) {
  case 1:
    mid = Color::hex(0x1E3A8A);
    break;
  case 2:
    mid = Color::hex(0x18181B);
    break;
  case 3:
    mid = Color::hex(0x5B1414);
    break;
  default:
    mid = Color::hex(0x0F4A30);
    break;
  }
  return FillStyle::radialGradient({GradientStop{0.f, top}, GradientStop{0.55f, mid},
                                    GradientStop{1.f, bottom}},
                                   Point{0.5f, 0.f}, 1.1f);
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

struct SavedGame {
  SolitaireState state;
};

struct SaveMarker {
  std::uint64_t revision = 0;
  std::uint32_t seed = 0;
  int elapsedBucket = 0;
  bool completed = false;

  bool operator==(SaveMarker const&) const = default;
};

std::filesystem::path saveFilePath() {
  std::string const dir = Application::instance().userDataDir();
  if (dir.empty()) {
    return {};
  }
  return std::filesystem::path(dir) / "save.txt";
}

void writeCard(std::ostream& out, Card const& card) {
  out << card.id << ' ' << card.rank << ' ' << static_cast<int>(card.suit) << ' '
      << (card.faceUp ? 1 : 0) << '\n';
}

bool readCard(std::istream& in, Card& card) {
  int suit = 0;
  int faceUp = 0;
  if (!(in >> card.id >> card.rank >> suit >> faceUp)) {
    return false;
  }
  if (card.rank < 1 || card.rank > 13 || suit < 0 || suit > 3) {
    return false;
  }
  card.suit = static_cast<Suit>(suit);
  card.faceUp = faceUp != 0;
  return true;
}

void writePile(std::ostream& out, char prefix, int index, std::vector<Card> const& pile) {
  out << prefix << ' ' << index << ' ' << pile.size() << '\n';
  for (Card const& card : pile) {
    writeCard(out, card);
  }
}

bool readPile(std::istream& in, char expectedPrefix, int expectedIndex, std::vector<Card>& pile) {
  char prefix = 0;
  int index = 0;
  std::size_t count = 0;
  if (!(in >> prefix >> index >> count) || prefix != expectedPrefix || index != expectedIndex) {
    return false;
  }
  pile.clear();
  pile.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    Card card;
    if (!readCard(in, card)) {
      return false;
    }
    pile.push_back(card);
  }
  return true;
}

void writeBoard(std::ostream& out, Board const& board) {
  out << "BOARD\n";
  for (int i = 0; i < 7; ++i) {
    writePile(out, 'T', i, board.tableau[static_cast<std::size_t>(i)]);
  }
  writePile(out, 'S', 0, board.stock);
  writePile(out, 'W', 0, board.waste);
  for (int i = 0; i < 4; ++i) {
    writePile(out, 'F', i, board.foundations[static_cast<std::size_t>(i)]);
  }
  out << "END_BOARD\n";
}

bool readBoard(std::istream& in, Board& board) {
  std::string token;
  if (!(in >> token) || token != "BOARD") {
    return false;
  }
  for (int i = 0; i < 7; ++i) {
    if (!readPile(in, 'T', i, board.tableau[static_cast<std::size_t>(i)])) {
      return false;
    }
  }
  if (!readPile(in, 'S', 0, board.stock) || !readPile(in, 'W', 0, board.waste)) {
    return false;
  }
  for (int i = 0; i < 4; ++i) {
    if (!readPile(in, 'F', i, board.foundations[static_cast<std::size_t>(i)])) {
      return false;
    }
  }
  return (in >> token) && token == "END_BOARD";
}

SolitaireState stableForSave(SolitaireState state) {
  state.animations.clear();
  state.selectionScaleAnimations.clear();
  state.selection = {};
  state.hint = {};
  state.peek = {};
  state.drag = {};
  state.dealing = false;
  state.autoFinishing = false;
  state.frameNanos = 0;
  state.celebrationStartNanos = 0;
  return state;
}

bool saveGame(SolitaireState const& state) {
  std::filesystem::path const path = saveFilePath();
  if (path.empty()) {
    return false;
  }
  SolitaireState stable = stableForSave(state);
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    return false;
  }
  std::filesystem::path const tmp = path.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) {
      return false;
    }
    out << "FLUX_SOLITAIRE_SAVE_V1\n";
    out << "STATE " << stable.seed << ' ' << stable.moves << ' ' << stable.score << ' '
        << stable.elapsedSeconds << ' ' << (stable.completed ? 1 : 0) << '\n';
    writeBoard(out, stable.board);
    out << "HISTORY " << stable.history.size() << '\n';
    for (HistoryEntry const& entry : stable.history) {
      out << "ENTRY " << entry.moves << ' ' << entry.score << ' '
          << (entry.completed ? 1 : 0) << '\n';
      writeBoard(out, entry.board);
    }
    if (!out) {
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(tmp, path, ec);
  }
  return !ec;
}

std::optional<SavedGame> loadGame() {
  std::filesystem::path const path = saveFilePath();
  if (path.empty()) {
    return std::nullopt;
  }
  std::ifstream in(path);
  if (!in) {
    return std::nullopt;
  }
  std::string token;
  SavedGame saved;
  int completed = 0;
  if (!(in >> token) || token != "FLUX_SOLITAIRE_SAVE_V1") {
    return std::nullopt;
  }
  if (!(in >> token)) {
    return std::nullopt;
  }
  if (token == "DRAW") {
    int ignoredDrawMode = 0;
    if (!(in >> ignoredDrawMode >> token)) {
      return std::nullopt;
    }
  }
  if (!(in >> token >> saved.state.seed >> saved.state.moves >> saved.state.score >>
        saved.state.elapsedSeconds >> completed) ||
      token != "STATE") {
    return std::nullopt;
  }
  saved.state.completed = completed != 0;
  if (!readBoard(in, saved.state.board)) {
    return std::nullopt;
  }
  std::size_t historyCount = 0;
  if (!(in >> token >> historyCount) || token != "HISTORY") {
    return std::nullopt;
  }
  saved.state.history.clear();
  saved.state.history.reserve(historyCount);
  for (std::size_t i = 0; i < historyCount; ++i) {
    HistoryEntry entry;
    int entryCompleted = 0;
    if (!(in >> token >> entry.moves >> entry.score >> entryCompleted) || token != "ENTRY") {
      return std::nullopt;
    }
    entry.completed = entryCompleted != 0;
    if (!readBoard(in, entry.board)) {
      return std::nullopt;
    }
    saved.state.history.push_back(std::move(entry));
  }
  saved.state = stableForSave(saved.state);
  saved.state.startedNanos =
      nowNanos() -
      static_cast<std::int64_t>(std::max(0, saved.state.elapsedSeconds)) * 1'000'000'000;
  return saved;
}

void markSaveDirty(SolitaireState& state) {
  ++state.saveRevision;
  if (state.saveRevision == 0) {
    state.saveRevision = 1;
  }
}

bool saveBlockedByTransientState(SolitaireState const& state) {
  return state.dealing || state.autoFinishing || !state.animations.empty() ||
         (state.drag.active() && state.drag.moved);
}

int elapsedSaveBucket(SolitaireState const& state) {
  if (state.completed) {
    return std::max(0, state.elapsedSeconds);
  }
  return std::max(0, state.elapsedSeconds) / kElapsedSaveIntervalSeconds;
}

SaveMarker saveMarker(SolitaireState const& state) {
  return SaveMarker{
      .revision = state.saveRevision,
      .seed = state.seed,
      .elapsedBucket = elapsedSaveBucket(state),
      .completed = state.completed,
  };
}

void saveGameIfNeeded(SolitaireState const& state) {
  static std::optional<SaveMarker> lastSaved;
  if (saveBlockedByTransientState(state)) {
    return;
  }
  SaveMarker const marker = saveMarker(state);
  if (lastSaved && *lastSaved == marker) {
    return;
  }
  if (saveGame(state)) {
    lastSaved = marker;
  }
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

void startDealAnimation(SolitaireState& state);
Board makeWinnableBoard(std::uint32_t seed, int drawCount, std::uint32_t& resolvedSeed);

SolitaireState makeState(std::uint32_t seed, int drawCount) {
  std::uint32_t resolvedSeed = seed;
  SolitaireState state{
      .board = makeWinnableBoard(seed, drawCount, resolvedSeed),
      .seed = resolvedSeed,
      .startedNanos = nowNanos(),
  };
  startDealAnimation(state);
  return state;
}

std::string formatTime(int seconds) {
  int const minutes = seconds / 60;
  int const secs = seconds % 60;
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
  return buffer;
}

bool gameComplete(Board const& board) {
  return std::all_of(board.foundations.begin(), board.foundations.end(), [](std::vector<Card> const& pile) {
    return pile.size() == 13;
  });
}

void refreshCompletion(SolitaireState& state) {
  bool const complete = gameComplete(state.board);
  if (complete && !state.completed) {
    std::int64_t const now = nowNanos();
    state.elapsedSeconds = static_cast<int>(
        std::max<std::int64_t>(0, now - state.startedNanos) / 1'000'000'000);
    state.selection = {};
    state.hint = {};
    state.peek = {};
    state.drag = {};
    state.celebrationStartNanos = 0;
  }
  state.completed = complete;
  if (!complete) {
    state.celebrationStartNanos = 0;
  }
}

void pushHistory(SolitaireState& state) {
  if (state.history.size() >= 50) {
    state.history.erase(state.history.begin());
  }
  state.history.push_back(HistoryEntry{
      .board = state.board,
      .moves = state.moves,
      .score = state.score,
      .completed = state.completed,
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

bool performMove(SolitaireState& state, Source source, Source dest, int count, bool recordHistory = true) {
  if (!canMove(state.board, source, count, dest)) {
    return false;
  }

  if (recordHistory) {
    pushHistory(state);
  }
  std::vector<Card> moving;
  if (!takeCards(state.board, source, count, moving)) {
    if (recordHistory) {
      state.history.pop_back();
    }
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
  state.selectionScaleAnimations.clear();
  state.hint = {};
  state.peek = {};
  state.drag = {};
  ++state.moves;
  refreshCompletion(state);
  markSaveDirty(state);
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
  if (playControlsDisabled(state)) {
    return;
  }
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
  state.selectionScaleAnimations.clear();
  state.hint = {};
  state.peek = {};
  state.drag = {};
  ++state.moves;
  markSaveDirty(state);
}

Card cardFor(Suit suit, int rank, bool faceUp = false) {
  return Card{
      .id = foundationIndex(suit) * 13 + rank - 1,
      .rank = rank,
      .suit = suit,
      .faceUp = faceUp,
  };
}

enum class WitnessMoveKind : std::uint8_t {
  TableauToFoundation,
  FlipTableau,
  DrawStock,
  WasteToFoundation,
  TableauToTableau,
  WasteToTableau,
};

struct WitnessMove {
  WitnessMoveKind kind = WitnessMoveKind::DrawStock;
  int column = -1;
  int destColumn = -1;
};

struct GeneratedDeal {
  Board board;
  std::vector<WitnessMove> solution;
};

bool verifyGeneratedDeal(GeneratedDeal const& generated, int drawCount);

void fillStockForDrawMode(Board& board, std::vector<Card> const& foundationOrder, int drawCount) {
  std::vector<Card> stockPopOrder;
  stockPopOrder.reserve(foundationOrder.size());
  int const groupSize = std::max(1, drawCount);
  // Draw-3 exposes a group as LIFO waste, so each draw group is stored reversed.
  for (int start = 0; start < static_cast<int>(foundationOrder.size()); start += groupSize) {
    int const end = std::min(start + groupSize, static_cast<int>(foundationOrder.size()));
    for (int i = end - 1; i >= start; --i) {
      stockPopOrder.push_back(foundationOrder[static_cast<std::size_t>(i)]);
    }
  }

  board.stock.clear();
  board.stock.reserve(stockPopOrder.size());
  for (auto it = stockPopOrder.rbegin(); it != stockPopOrder.rend(); ++it) {
    Card card = *it;
    card.faceUp = false;
    board.stock.push_back(card);
  }
}

std::vector<Card> directFoundationOrder(std::mt19937& rng) {
  std::vector<Card> order;
  order.reserve(52);
  std::array<int, 4> nextRank{1, 1, 1, 1};
  int lastSuit = -1;
  int runLength = 0;

  while (order.size() < 52) {
    std::vector<int> suits;
    for (int suit = 0; suit < 4; ++suit) {
      if (nextRank[static_cast<std::size_t>(suit)] <= 13) {
        suits.push_back(suit);
      }
    }
    std::shuffle(suits.begin(), suits.end(), rng);
    if (suits.size() > 1 && runLength >= 2) {
      suits.erase(std::remove(suits.begin(), suits.end(), lastSuit), suits.end());
    }

    std::uniform_int_distribution<int> pick(0, static_cast<int>(suits.size()) - 1);
    int const suitIndex = suits[static_cast<std::size_t>(pick(rng))];
    int const rank = nextRank[static_cast<std::size_t>(suitIndex)]++;
    order.push_back(cardFor(static_cast<Suit>(suitIndex), rank, true));

    if (suitIndex == lastSuit) {
      ++runLength;
    } else {
      lastSuit = suitIndex;
      runLength = 1;
    }
  }
  return order;
}

struct DirectSlot {
  bool stock = false;
  int column = -1;
};

std::vector<DirectSlot> directSourceSlots(std::mt19937& rng, int drawCount) {
  std::vector<DirectSlot> tableauSlots;
  tableauSlots.reserve(28);
  for (int column = 0; column < 7; ++column) {
    for (int i = 0; i <= column; ++i) {
      tableauSlots.push_back(DirectSlot{.column = column});
    }
  }
  std::shuffle(tableauSlots.begin(), tableauSlots.end(), rng);

  std::vector<std::vector<DirectSlot>> units;
  units.reserve(52);
  int const groupSize = std::max(1, drawCount);
  for (int i = 0; i < 24; i += groupSize) {
    std::vector<DirectSlot> unit;
    int const count = std::min(groupSize, 24 - i);
    unit.reserve(static_cast<std::size_t>(count));
    for (int j = 0; j < count; ++j) {
      unit.push_back(DirectSlot{.stock = true});
    }
    units.push_back(std::move(unit));
  }
  for (DirectSlot slot : tableauSlots) {
    units.push_back({slot});
  }
  std::shuffle(units.begin(), units.end(), rng);

  std::vector<DirectSlot> slots;
  slots.reserve(52);
  for (auto const& unit : units) {
    for (DirectSlot slot : unit) {
      slots.push_back(slot);
    }
  }
  return slots;
}

GeneratedDeal makeDirectFoundationDeal(std::mt19937& rng, int drawCount) {
  std::vector<Card> const foundationOrder = directFoundationOrder(rng);
  std::vector<DirectSlot> const slots = directSourceSlots(rng, drawCount);
  GeneratedDeal generated;
  std::array<std::vector<Card>, 7> columnForwardOrder;
  std::array<int, 7> columnSeen{};
  std::vector<Card> stockFoundationOrder;
  stockFoundationOrder.reserve(24);
  generated.solution.reserve(104);

  for (std::size_t i = 0; i < foundationOrder.size(); ++i) {
    Card card = foundationOrder[i];
    DirectSlot const slot = slots[i];
    if (slot.stock) {
      std::size_t const groupSize = static_cast<std::size_t>(std::max(1, drawCount));
      if (stockFoundationOrder.size() % groupSize == 0) {
        generated.solution.push_back(WitnessMove{.kind = WitnessMoveKind::DrawStock});
      }
      stockFoundationOrder.push_back(card);
      generated.solution.push_back(WitnessMove{.kind = WitnessMoveKind::WasteToFoundation});
      continue;
    }

    int& seen = columnSeen[static_cast<std::size_t>(slot.column)];
    if (seen > 0) {
      generated.solution.push_back(WitnessMove{
          .kind = WitnessMoveKind::FlipTableau,
          .column = slot.column,
      });
    }
    generated.solution.push_back(WitnessMove{
        .kind = WitnessMoveKind::TableauToFoundation,
        .column = slot.column,
    });
    ++seen;
    columnForwardOrder[static_cast<std::size_t>(slot.column)].push_back(card);
  }

  for (int column = 0; column < 7; ++column) {
    auto const& forward = columnForwardOrder[static_cast<std::size_t>(column)];
    auto& pile = generated.board.tableau[static_cast<std::size_t>(column)];
    for (int i = static_cast<int>(forward.size()) - 1; i >= 0; --i) {
      Card card = forward[static_cast<std::size_t>(i)];
      card.faceUp = i == 0;
      pile.push_back(card);
    }
  }

  fillStockForDrawMode(generated.board, stockFoundationOrder, drawCount);
  return generated;
}

struct BuildSource {
  bool stock = false;
  int column = -1;
};

struct Placement {
  BuildSource source;
  int cardIndex = 0;
  bool toFoundation = false;
  bool stayInColumn = false;
  int destColumn = -1;
};

struct LiveMove {
  int sourceColumn = -1;
  int destColumn = -1;
};

struct ChainBuildState {
  std::array<std::vector<Card>, 7> revealOrder;
  std::array<std::vector<Card>, 7> liveColumns;
  std::array<int, 7> revealed{};
  std::array<int, 4> foundationRanks{};
  std::array<bool, 52> relocated{};
  std::vector<Card> stockOrder;
  std::vector<Card> remaining;
  std::vector<WitnessMove> solution;
};

std::vector<Card> shuffledDeck(std::mt19937& rng) {
  std::vector<Card> deck;
  deck.reserve(52);
  for (Suit suit : {Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs}) {
    for (int rank = 1; rank <= 13; ++rank) {
      deck.push_back(cardFor(suit, rank));
    }
  }
  std::shuffle(deck.begin(), deck.end(), rng);
  return deck;
}

bool foundationReady(std::array<int, 4> const& foundationRanks, Card const& card) {
  int const foundation = foundationIndex(card.suit);
  return card.rank == foundationRanks[static_cast<std::size_t>(foundation)] + 1;
}

bool canMoveToGeneratedColumn(ChainBuildState const& state, Card const& card, int destColumn) {
  auto const& dest = state.liveColumns[static_cast<std::size_t>(destColumn)];
  if (!dest.empty()) {
    Card const& top = dest.back();
    return redSuit(card.suit) != redSuit(top.suit) && card.rank == top.rank - 1;
  }
  return state.revealed[static_cast<std::size_t>(destColumn)] == destColumn + 1 && card.rank == 13;
}

void moveGeneratedTopToFoundation(ChainBuildState& state, int column) {
  auto& live = state.liveColumns[static_cast<std::size_t>(column)];
  Card card = live.back();
  live.pop_back();
  state.foundationRanks[static_cast<std::size_t>(foundationIndex(card.suit))] = card.rank;
  state.solution.push_back(WitnessMove{
      .kind = WitnessMoveKind::TableauToFoundation,
      .column = column,
  });
}

void autoClearGeneratedFoundations(ChainBuildState& state) {
  bool changed = true;
  while (changed) {
    changed = false;
    std::vector<int> candidates;
    for (int column = 0; column < 7; ++column) {
      auto const& live = state.liveColumns[static_cast<std::size_t>(column)];
      if (!live.empty() && foundationReady(state.foundationRanks, live.back())) {
        candidates.push_back(column);
      }
    }
    if (!candidates.empty()) {
      int const column = *std::min_element(candidates.begin(), candidates.end(), [&](int a, int b) {
        Card const& ca = state.liveColumns[static_cast<std::size_t>(a)].back();
        Card const& cb = state.liveColumns[static_cast<std::size_t>(b)].back();
        if (ca.rank != cb.rank) {
          return ca.rank < cb.rank;
        }
        return a < b;
      });
      moveGeneratedTopToFoundation(state, column);
      changed = true;
    }
  }
}

std::vector<LiveMove> possibleLiveMoves(ChainBuildState const& state) {
  std::vector<LiveMove> moves;
  for (int sourceColumn = 0; sourceColumn < 7; ++sourceColumn) {
    auto const& source = state.liveColumns[static_cast<std::size_t>(sourceColumn)];
    if (source.empty() || state.revealed[static_cast<std::size_t>(sourceColumn)] >= sourceColumn + 1) {
      continue;
    }
    Card const& card = source.back();
    if (state.relocated[static_cast<std::size_t>(card.id)]) {
      continue;
    }
    for (int destColumn = 0; destColumn < 7; ++destColumn) {
      if (destColumn == sourceColumn) {
        continue;
      }
      if (canMoveToGeneratedColumn(state, card, destColumn)) {
        moves.push_back(LiveMove{
            .sourceColumn = sourceColumn,
            .destColumn = destColumn,
        });
      }
    }
  }
  return moves;
}

void applyLiveMove(ChainBuildState& state, LiveMove move) {
  auto& source = state.liveColumns[static_cast<std::size_t>(move.sourceColumn)];
  Card card = source.back();
  source.pop_back();
  state.relocated[static_cast<std::size_t>(card.id)] = true;
  state.liveColumns[static_cast<std::size_t>(move.destColumn)].push_back(card);
  state.solution.push_back(WitnessMove{
      .kind = WitnessMoveKind::TableauToTableau,
      .column = move.sourceColumn,
      .destColumn = move.destColumn,
  });
  autoClearGeneratedFoundations(state);
}

std::vector<BuildSource> availableSources(ChainBuildState const& state) {
  std::vector<BuildSource> sources;
  sources.reserve(8);
  for (int column = 0; column < 7; ++column) {
    if (state.liveColumns[static_cast<std::size_t>(column)].empty() &&
        state.revealed[static_cast<std::size_t>(column)] < column + 1) {
      sources.push_back(BuildSource{.column = column});
    }
  }
  if (state.stockOrder.size() < 24) {
    sources.push_back(BuildSource{.stock = true});
  }
  return sources;
}

std::vector<Placement> possiblePlacements(ChainBuildState const& state) {
  std::vector<Placement> placements;
  std::vector<BuildSource> const sources = availableSources(state);
  for (BuildSource source : sources) {
    for (int cardIndex = 0; cardIndex < static_cast<int>(state.remaining.size()); ++cardIndex) {
      Card const& card = state.remaining[static_cast<std::size_t>(cardIndex)];
      if (!source.stock) {
        placements.push_back(Placement{
            .source = source,
            .cardIndex = cardIndex,
            .stayInColumn = true,
            .destColumn = source.column,
        });
      }
      if (foundationReady(state.foundationRanks, card)) {
        placements.push_back(Placement{
            .source = source,
            .cardIndex = cardIndex,
            .toFoundation = true,
        });
      }
      for (int destColumn = 0; destColumn < 7; ++destColumn) {
        if (!source.stock && source.column == destColumn) {
          continue;
        }
        if (canMoveToGeneratedColumn(state, card, destColumn)) {
          placements.push_back(Placement{
              .source = source,
              .cardIndex = cardIndex,
              .destColumn = destColumn,
          });
        }
      }
    }
  }
  return placements;
}

void applyPlacement(ChainBuildState& state, Placement placement) {
  Card card = state.remaining[static_cast<std::size_t>(placement.cardIndex)];
  state.remaining.erase(state.remaining.begin() + placement.cardIndex);
  card.faceUp = true;

  if (placement.source.stock) {
    Card stockCard = card;
    stockCard.faceUp = false;
    state.stockOrder.push_back(stockCard);
    state.solution.push_back(WitnessMove{.kind = WitnessMoveKind::DrawStock});
    if (placement.toFoundation) {
      state.solution.push_back(WitnessMove{.kind = WitnessMoveKind::WasteToFoundation});
    } else {
      state.solution.push_back(WitnessMove{
          .kind = WitnessMoveKind::WasteToTableau,
          .destColumn = placement.destColumn,
      });
    }
  } else {
    int const column = placement.source.column;
    auto& revealed = state.revealed[static_cast<std::size_t>(column)];
    state.revealOrder[static_cast<std::size_t>(column)].push_back(card);
    if (revealed > 0) {
      state.solution.push_back(WitnessMove{
          .kind = WitnessMoveKind::FlipTableau,
          .column = column,
      });
    }
    ++revealed;
    if (placement.stayInColumn) {
      // The generated card is now the visible top of its original column.
    } else if (placement.toFoundation) {
      state.solution.push_back(WitnessMove{
          .kind = WitnessMoveKind::TableauToFoundation,
          .column = column,
      });
    } else {
      state.solution.push_back(WitnessMove{
          .kind = WitnessMoveKind::TableauToTableau,
          .column = column,
          .destColumn = placement.destColumn,
      });
    }
  }

  if (placement.toFoundation) {
    state.foundationRanks[static_cast<std::size_t>(foundationIndex(card.suit))] = card.rank;
  } else {
    state.liveColumns[static_cast<std::size_t>(placement.destColumn)].push_back(card);
  }
  autoClearGeneratedFoundations(state);
}

bool generatedBuildComplete(ChainBuildState const& state) {
  if (!state.remaining.empty() || state.stockOrder.size() != 24) {
    return false;
  }
  for (int column = 0; column < 7; ++column) {
    if (state.revealed[static_cast<std::size_t>(column)] != column + 1 ||
        !state.liveColumns[static_cast<std::size_t>(column)].empty()) {
      return false;
    }
  }
  return std::all_of(state.foundationRanks.begin(), state.foundationRanks.end(), [](int rank) {
    return rank == 13;
  });
}

std::optional<ChainBuildState> tryBuildChainedDeal(std::mt19937& rng) {
  ChainBuildState state;
  state.remaining = shuffledDeck(rng);
  state.stockOrder.reserve(24);
  state.solution.reserve(160);

  int steps = 0;
  while (!state.remaining.empty() && steps++ < 500) {
    autoClearGeneratedFoundations(state);
    std::vector<LiveMove> liveMoves = possibleLiveMoves(state);
    if (!liveMoves.empty()) {
      std::uniform_int_distribution<int> pick(0, static_cast<int>(liveMoves.size()) - 1);
      applyLiveMove(state, liveMoves[static_cast<std::size_t>(pick(rng))]);
      continue;
    }

    std::vector<Placement> placements = possiblePlacements(state);
    if (placements.empty()) {
      return std::nullopt;
    }

    std::vector<Placement> tableauPlacements;
    std::vector<Placement> stayPlacements;
    tableauPlacements.reserve(placements.size());
    stayPlacements.reserve(placements.size());
    for (Placement const& placement : placements) {
      if (placement.stayInColumn) {
        stayPlacements.push_back(placement);
      } else if (!placement.toFoundation) {
        tableauPlacements.push_back(placement);
      }
    }

    std::vector<Placement> const* choices = &placements;
    if (!tableauPlacements.empty() && !stayPlacements.empty()) {
      std::uniform_int_distribution<int> preferEscape(0, 99);
      choices = preferEscape(rng) < 70 ? &tableauPlacements : &stayPlacements;
    } else if (!tableauPlacements.empty()) {
      choices = &tableauPlacements;
    } else if (!stayPlacements.empty()) {
      choices = &stayPlacements;
    }
    std::uniform_int_distribution<int> pick(0, static_cast<int>(choices->size()) - 1);
    applyPlacement(state, (*choices)[static_cast<std::size_t>(pick(rng))]);
  }

  autoClearGeneratedFoundations(state);
  if (steps >= 500) {
    return std::nullopt;
  }
  if (!generatedBuildComplete(state)) {
    return std::nullopt;
  }
  return state;
}

GeneratedDeal makeGeneratedDeal(std::mt19937& rng, int drawCount) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    auto state = tryBuildChainedDeal(rng);
    if (!state) {
      continue;
    }

    GeneratedDeal generated;
    generated.solution = std::move(state->solution);
    for (int column = 0; column < 7; ++column) {
      auto const& revealOrder = state->revealOrder[static_cast<std::size_t>(column)];
      auto& pile = generated.board.tableau[static_cast<std::size_t>(column)];
      for (int i = static_cast<int>(revealOrder.size()) - 1; i >= 0; --i) {
        Card card = revealOrder[static_cast<std::size_t>(i)];
        card.faceUp = i == 0;
        pile.push_back(card);
      }
    }
    fillStockForDrawMode(generated.board, state->stockOrder, drawCount);
    if (verifyGeneratedDeal(generated, drawCount)) {
      return generated;
    }
  }

  std::fprintf(stderr, "solitaire chained generation failed; using direct fallback\n");
  GeneratedDeal fallback = makeDirectFoundationDeal(rng, drawCount);
  if (!verifyGeneratedDeal(fallback, drawCount)) {
    std::fprintf(stderr, "solitaire direct fallback produced an invalid witness\n");
  }
  return fallback;
}

bool moveTableauToFoundation(Board& board, int column) {
  if (column < 0 || column >= 7) {
    return false;
  }
  auto& pile = board.tableau[static_cast<std::size_t>(column)];
  if (pile.empty() || !pile.back().faceUp) {
    return false;
  }
  Card card = pile.back();
  auto& foundation = board.foundations[static_cast<std::size_t>(foundationIndex(card.suit))];
  Card const* top = foundation.empty() ? nullptr : &foundation.back();
  if (!canPlaceOnFoundation(card, top)) {
    return false;
  }
  pile.pop_back();
  card.faceUp = true;
  foundation.push_back(card);
  return true;
}

bool moveWasteToFoundation(Board& board) {
  if (board.waste.empty()) {
    return false;
  }
  Card card = board.waste.back();
  auto& foundation = board.foundations[static_cast<std::size_t>(foundationIndex(card.suit))];
  Card const* top = foundation.empty() ? nullptr : &foundation.back();
  if (!canPlaceOnFoundation(card, top)) {
    return false;
  }
  board.waste.pop_back();
  card.faceUp = true;
  foundation.push_back(card);
  return true;
}

bool moveTableauToTableau(Board& board, int sourceColumn, int destColumn) {
  if (sourceColumn < 0 || sourceColumn >= 7 || destColumn < 0 || destColumn >= 7 ||
      sourceColumn == destColumn) {
    return false;
  }
  auto& source = board.tableau[static_cast<std::size_t>(sourceColumn)];
  if (source.empty() || !source.back().faceUp) {
    return false;
  }
  auto& dest = board.tableau[static_cast<std::size_t>(destColumn)];
  Card const* destTop = dest.empty() ? nullptr : &dest.back();
  if (!canPlaceOnTableau(source.back(), destTop)) {
    return false;
  }
  Card card = source.back();
  source.pop_back();
  card.faceUp = true;
  dest.push_back(card);
  return true;
}

bool moveWasteToTableau(Board& board, int destColumn) {
  if (destColumn < 0 || destColumn >= 7 || board.waste.empty()) {
    return false;
  }
  auto& dest = board.tableau[static_cast<std::size_t>(destColumn)];
  Card const* destTop = dest.empty() ? nullptr : &dest.back();
  if (!canPlaceOnTableau(board.waste.back(), destTop)) {
    return false;
  }
  Card card = board.waste.back();
  board.waste.pop_back();
  card.faceUp = true;
  dest.push_back(card);
  return true;
}

bool drawForWitness(Board& board, int drawCount) {
  if (board.stock.empty()) {
    return false;
  }
  int const count = std::min(std::max(1, drawCount), static_cast<int>(board.stock.size()));
  for (int i = 0; i < count; ++i) {
    Card card = board.stock.back();
    board.stock.pop_back();
    card.faceUp = true;
    board.waste.push_back(card);
  }
  return true;
}

bool applyWitnessMove(Board& board, WitnessMove move, int drawCount) {
  if (move.kind == WitnessMoveKind::TableauToFoundation) {
    return moveTableauToFoundation(board, move.column);
  }
  if (move.kind == WitnessMoveKind::WasteToFoundation) {
    return moveWasteToFoundation(board);
  }
  if (move.kind == WitnessMoveKind::DrawStock) {
    return drawForWitness(board, drawCount);
  }
  if (move.kind == WitnessMoveKind::TableauToTableau) {
    return moveTableauToTableau(board, move.column, move.destColumn);
  }
  if (move.kind == WitnessMoveKind::WasteToTableau) {
    return moveWasteToTableau(board, move.destColumn);
  }

  if (move.column < 0 || move.column >= 7) {
    return false;
  }
  auto& pile = board.tableau[static_cast<std::size_t>(move.column)];
  if (pile.empty() || pile.back().faceUp) {
    return false;
  }
  pile.back().faceUp = true;
  return true;
}

bool verifyGeneratedDeal(GeneratedDeal const& generated, int drawCount) {
  Board replay = generated.board;
  for (WitnessMove move : generated.solution) {
    if (!applyWitnessMove(replay, move, drawCount)) {
      return false;
    }
  }
  if (!gameComplete(replay) || !replay.stock.empty() || !replay.waste.empty()) {
    return false;
  }
  return std::all_of(replay.tableau.begin(), replay.tableau.end(), [](std::vector<Card> const& pile) {
    return pile.empty();
  });
}

Board constructedWinnableBoard(std::uint32_t seed, int drawCount) {
  std::mt19937 rng(seed);
  GeneratedDeal generated = makeGeneratedDeal(rng, drawCount);
  if (!verifyGeneratedDeal(generated, drawCount)) {
    std::fprintf(stderr, "solitaire reverse generation produced an invalid witness for seed %u\n", seed);
  }
  return generated.board;
}

Board makeWinnableBoard(std::uint32_t seed, int drawCount, std::uint32_t& resolvedSeed) {
  (void)drawCount;
  int const normalizedDrawCount = 1;
  resolvedSeed = seed == 0 ? 1u : seed;
  return constructedWinnableBoard(resolvedSeed, normalizedDrawCount);
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
  float const availableH = std::max(1.f, viewport.height - kBoardTop - 32.f);
  float const scale = std::max(0.01f, std::min(availableW / kMinBoardW, availableH / kBoardH));
  return BoardGeometry{
      .origin = Point{(viewport.width - kMinBoardW * scale) * 0.5f, kBoardTop},
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

void addCardAnimation(SolitaireState& state, Card card, Rect from, Rect to,
                      std::int64_t startNanos, std::int64_t durationNanos) {
  state.animations.push_back(FlyAnimation{
      .cards = {card},
      .fromRects = {from},
      .toRects = {to},
      .startNanos = startNanos,
      .durationNanos = durationNanos,
  });
}

void startDealAnimation(SolitaireState& state) {
  BoardGeometry const geometry{};
  std::int64_t const now = nowNanos();
  Rect const stockTop = slotRect(PileKind::Stock, 0, geometry);
  Rect const from = Rect::sharp(stockTop.x, stockTop.y - 10.f, stockTop.width, stockTop.height);
  std::int64_t delay = 0;

  state.animations.clear();
  state.dealing = true;
  state.autoFinishing = false;
  state.completed = false;
  state.celebrationStartNanos = 0;
  state.frameNanos = now;

  float const tableauTop = kCardH + kTopToTableauGap;
  for (int row = 0; row < 7; ++row) {
    for (int col = row; col < 7; ++col) {
      auto const& column = state.board.tableau[static_cast<std::size_t>(col)];
      if (row >= static_cast<int>(column.size())) {
        continue;
      }
      float y = tableauTop;
      for (int previousRow = 0; previousRow < row; ++previousRow) {
        Card const& previous = column[static_cast<std::size_t>(previousRow)];
        y += previous.faceUp ? kFanOpen : kFanClosed;
      }
      Card const card = column[static_cast<std::size_t>(row)];
      Card animatedCard = card;
      animatedCard.faceUp = false;
      Rect const to = Rect::sharp(columnX(geometry, col), y, kCardW, kCardH);
      addCardAnimation(state, animatedCard, from, to, now + delay, kDealFlyDurationNanos);
      delay += kDealFlyIntervalNanos;
    }
  }

  state.startedNanos = now + delay + kDealFlyDurationNanos;
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

std::optional<Source> nextFoundationAutoSource(Board const& board) {
  if (!board.waste.empty() && findFoundationFor(board, board.waste.back())) {
    return Source{.kind = PileKind::Waste};
  }

  std::optional<Source> best;
  int bestRank = 99;
  for (int col = 0; col < 7; ++col) {
    auto const& column = board.tableau[static_cast<std::size_t>(col)];
    if (column.empty()) {
      continue;
    }
    Card const& top = column.back();
    if (!top.faceUp || !findFoundationFor(board, top)) {
      continue;
    }
    if (top.rank < bestRank) {
      bestRank = top.rank;
      best = Source{.kind = PileKind::Tableau, .index = col, .row = static_cast<int>(column.size()) - 1};
    }
  }
  return best;
}

bool scheduleNextAutoFinishMove(SolitaireState& state, int drawCount, BoardGeometry const& geometry,
                                std::int64_t startNanos, bool recordHistory) {
  if (!state.animations.empty()) {
    return false;
  }

  std::optional<Source> source = nextFoundationAutoSource(state.board);
  if (!source) {
    return false;
  }

  std::vector<Card> moving = cardsFromSource(state.board, *source, 1);
  if (moving.size() != 1) {
    return false;
  }
  std::optional<int> dest = findFoundationFor(state.board, moving.front());
  if (!dest) {
    return false;
  }

  std::vector<CardPosition> const before = buildCardPositions(state.board, drawCount, geometry);
  Rect const from = rectForCard(before, moving.front().id)
                        .value_or(slotRect(source->kind, source->index, geometry));
  if (recordHistory) {
    pushHistory(state);
  }
  if (!performMove(state, *source, Source{.kind = PileKind::Foundation, .index = *dest}, 1, false)) {
    if (recordHistory && !state.history.empty()) {
      state.history.pop_back();
    }
    return false;
  }

  std::vector<CardPosition> const after = buildCardPositions(state.board, drawCount, geometry);
  Rect const to = rectForCard(after, moving.front().id)
                      .value_or(slotRect(PileKind::Foundation, *dest, geometry));
  addCardAnimation(state, moving.front(), from, to, startNanos, kAutoFlyDurationNanos);
  state.selection = {};
  state.hint = {};
  state.peek = {};
  state.drag = {};
  state.autoFinishing = true;
  state.frameNanos = startNanos;
  return true;
}

bool autoFinishToFoundations(SolitaireState& state, int drawCount, BoardGeometry const& geometry,
                             std::int64_t startNanos) {
  if (playControlsDisabled(state) || !state.animations.empty()) {
    return false;
  }
  return scheduleNextAutoFinishMove(state, drawCount, geometry, startNanos, true);
}

void autoFinish(Signal<SolitaireState> const& state) {
  BoardGeometry const geometry{};
  std::int64_t const startNanos = nowNanos();
  mutate(state, [geometry, startNanos](SolitaireState& s) {
    autoFinishToFoundations(s, kDrawCount, geometry, startNanos);
  });
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

float easeSelectionScale(float t) {
  t = std::clamp(t, 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

float selectionAnimationScale(SelectionScaleAnimation const& animation, std::int64_t now) {
  double const raw = static_cast<double>(now - animation.startNanos) /
                     static_cast<double>(std::max<std::int64_t>(1, kSelectionScaleDurationNanos));
  float const t = easeSelectionScale(static_cast<float>(raw));
  return animation.fromScale + (animation.toScale - animation.fromScale) * t;
}

float selectionScaleForSource(SolitaireState const& state, Source source, std::int64_t now) {
  float scale = state.selection.active() && sourceMatches(source, state.selection.source, true)
                    ? kSelectedCardScale
                    : 1.f;
  for (SelectionScaleAnimation const& animation : state.selectionScaleAnimations) {
    if (sourceMatches(source, animation.source, true)) {
      scale = animation.active(now) ? selectionAnimationScale(animation, now) : animation.toScale;
    }
  }
  return scale;
}

void addSelectionScaleAnimation(SolitaireState& state, Selection selection, float toScale,
                                std::int64_t now) {
  if (!selection.active()) {
    return;
  }
  float const fromScale = selectionScaleForSource(state, selection.source, now);
  state.selectionScaleAnimations.erase(
      std::remove_if(state.selectionScaleAnimations.begin(), state.selectionScaleAnimations.end(),
                     [source = selection.source](SelectionScaleAnimation const& animation) {
                       return sourceMatches(animation.source, source, false);
                     }),
      state.selectionScaleAnimations.end());
  if (std::abs(fromScale - toScale) < 0.001f) {
    return;
  }
  state.selectionScaleAnimations.push_back(SelectionScaleAnimation{
      .source = selection.source,
      .count = selection.count,
      .fromScale = fromScale,
      .toScale = toScale,
      .startNanos = now,
  });
}

void setSelectionWithAnimation(SolitaireState& state, Selection next, std::int64_t now) {
  Selection const previous = state.selection;
  if (previous == next) {
    return;
  }
  if (previous.active()) {
    addSelectionScaleAnimation(state, previous, 1.f, now);
  }
  if (next.active()) {
    addSelectionScaleAnimation(state, next, kSelectedCardScale, now);
  }
  state.selection = next;
  state.frameNanos = now;
}

bool cardIsAnimating(SolitaireState const& state, int cardId, std::int64_t now);

void handleBoardClick(Signal<SolitaireState> const& state, Point localPoint, Size viewport) {
  SolitaireState const stateSnapshot = state.peek();
  if (playControlsDisabled(stateSnapshot)) {
    return;
  }
  int const drawCount = kDrawCount;
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  if (p.x < -20.f || p.y < -20.f || p.x > geometry.layoutWidth + 20.f || p.y > geometry.layoutHeight + 80.f) {
    std::int64_t const now = nowNanos();
    mutate(state, [now](SolitaireState& s) {
      setSelectionWithAnimation(s, {}, now);
      s.peek = {};
    });
    return;
  }

  if (slotRect(PileKind::Stock, 0, geometry).contains(p)) {
    mutate(state, [drawCount](SolitaireState& s) { drawStock(s, drawCount); });
    return;
  }

  SolitaireState const& current = state.peek();
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
      });
      return;
    }
    std::int64_t const now = nowNanos();
    mutate(state, [hit, now](SolitaireState& s) {
      setSelectionWithAnimation(s, Selection{.source = hit->source, .count = hit->count}, now);
      s.hint = {};
    });
    return;
  }

  std::int64_t const now = nowNanos();
  mutate(state, [now](SolitaireState& s) {
    setSelectionWithAnimation(s, {}, now);
    s.peek = {};
  });
}

void startBoardDrag(Signal<SolitaireState> const& state, Point localPoint, Size viewport) {
  int const drawCount = kDrawCount;
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  SolitaireState const& current = state.peek();
  if (playControlsDisabled(current)) {
    return;
  }
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
      s.selectionScaleAnimations.clear();
      s.hint = {};
      s.peek = {};
    }
    s.drag.moved = movedNow;
  });
}

void finishBoardDrag(Signal<SolitaireState> const& state, Point localPoint, Size viewport) {
  int const drawCount = kDrawCount;
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  float const threshold = 4.f / std::max(0.01f, geometry.scale);

  SolitaireState const& current = state.peek();
  if (!current.drag.active()) {
    handleBoardClick(state, localPoint, viewport);
    return;
  }

  DragState drag = current.drag;
  drag.currentPoint = p;
  drag.moved = drag.moved || dragMoved(drag, p, threshold);
  if (!drag.moved) {
    mutate(state, [](SolitaireState& s) { s.drag = {}; });
    handleBoardClick(state, localPoint, viewport);
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

void startBoardPeek(Signal<SolitaireState> const& state, Point localPoint, Size viewport) {
  int const drawCount = kDrawCount;
  BoardGeometry const geometry = boardGeometry(viewport);
  Point const p = toBoardPoint(localPoint, geometry);
  SolitaireState const& current = state.peek();
  if (playControlsDisabled(current)) {
    return;
  }
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
  std::int64_t const hintNanos = nowNanos();
  mutate(state, [hintNanos](SolitaireState& s) {
    if (playControlsDisabled(s)) {
      return;
    }
    auto hint = [hintNanos](Source source) {
      return Hint{.source = source, .startedNanos = hintNanos};
    };
    Board const& board = s.board;
    if (!board.waste.empty()) {
      Source source{.kind = PileKind::Waste};
      Card const& card = board.waste.back();
      if (findFoundationFor(board, card)) {
        s.hint = hint(source);
        s.frameNanos = hintNanos;
        return;
      }
      for (int col = 0; col < 7; ++col) {
        auto const& dest = board.tableau[static_cast<std::size_t>(col)];
        Card const* top = dest.empty() ? nullptr : &dest.back();
        if (canPlaceOnTableau(card, top)) {
          s.hint = hint(source);
          s.frameNanos = hintNanos;
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
        s.hint = hint(topSource);
        s.frameNanos = hintNanos;
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
            s.hint = hint(Source{.kind = PileKind::Tableau, .index = col, .row = row});
            s.frameNanos = hintNanos;
            return;
          }
        }
      }
    }

    s.hint = hint(Source{.kind = PileKind::Stock});
    s.frameNanos = hintNanos;
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
    s.completed = prev.completed;
    s.selection = {};
    s.selectionScaleAnimations.clear();
    s.hint = {};
    s.peek = {};
    s.drag = {};
    s.animations.clear();
    s.frameNanos = 0;
    s.dealing = false;
    s.autoFinishing = false;
    s.celebrationStartNanos = 0;
    markSaveDirty(s);
  });
}

void newGame(Signal<SolitaireState> const& state) {
  std::uint32_t nextSeed = randomSeed();
  if (nextSeed == state.peek().seed) {
    ++nextSeed;
  }
  state = makeState(nextSeed, kDrawCount);
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
  Font const font{.size = size * 1.44f, .weight = 400.f};
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
                  StrokeStyle::solid(colorHex(0x0A2A5C, 0.42f), 1.f),
                  ShadowStyle{.radius = 8.f, .offset = {0.f, 2.f}, .color = Color{0.f, 0.f, 0.f, 0.18f}});

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
    return {{0.5f, 0.14f}, {0.5f, 0.86f}};
  case 3:
    return {{0.5f, 0.14f}, {0.5f, 0.5f}, {0.5f, 0.86f}};
  case 4:
    return {{0.20f, 0.14f}, {0.80f, 0.14f}, {0.20f, 0.86f}, {0.80f, 0.86f}};
  case 5:
    return {{0.20f, 0.14f}, {0.80f, 0.14f}, {0.5f, 0.5f}, {0.20f, 0.86f}, {0.80f, 0.86f}};
  case 6:
    return {{0.20f, 0.14f}, {0.80f, 0.14f}, {0.20f, 0.5f}, {0.80f, 0.5f}, {0.20f, 0.86f}, {0.80f, 0.86f}};
  case 7:
    return {{0.20f, 0.14f}, {0.80f, 0.14f}, {0.5f, 0.32f}, {0.20f, 0.5f}, {0.80f, 0.5f}, {0.20f, 0.86f}, {0.80f, 0.86f}};
  case 8:
    return {{0.20f, 0.14f}, {0.80f, 0.14f}, {0.5f, 0.32f}, {0.20f, 0.5f}, {0.80f, 0.5f}, {0.5f, 0.68f}, {0.20f, 0.86f}, {0.80f, 0.86f}};
  case 9:
    return {{0.20f, 0.14f}, {0.80f, 0.14f}, {0.20f, 0.36f}, {0.80f, 0.36f}, {0.5f, 0.5f}, {0.20f, 0.64f}, {0.80f, 0.64f}, {0.20f, 0.86f}, {0.80f, 0.86f}};
  case 10:
    return {{0.20f, 0.14f}, {0.80f, 0.14f}, {0.5f, 0.28f}, {0.20f, 0.4f}, {0.80f, 0.4f}, {0.20f, 0.6f}, {0.80f, 0.6f}, {0.5f, 0.72f}, {0.20f, 0.86f}, {0.80f, 0.86f}};
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
                  StrokeStyle::solid(Color{0.f, 0.f, 0.f, 0.10f}, 0.9f),
                  ShadowStyle{.radius = 8.f, .offset = {0.f, 2.f}, .color = Color{0.f, 0.f, 0.f, 0.14f}});

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

void drawCard(Canvas& canvas, Card const& card, Rect rect) {
  if (card.faceUp) {
    drawCardFace(canvas, card, rect);
  } else {
    drawCardBack(canvas, rect);
  }
}

Rect scaledRect(Rect rect, float scale) {
  if (std::abs(scale - 1.f) < 0.001f) {
    return rect;
  }
  Point const center = rect.center();
  float const width = rect.width * scale;
  float const height = rect.height * scale;
  return Rect::sharp(center.x - width * 0.5f, center.y - height * 0.5f, width, height);
}

void drawRotatedCard(Canvas& canvas, Card const& card, Rect rect, float radians) {
  if (std::abs(radians) < 0.001f) {
    drawCard(canvas, card, rect);
    return;
  }
  Point const center = rect.center();
  canvas.save();
  canvas.translate(center);
  canvas.rotate(radians);
  drawCard(canvas, card, Rect::sharp(-rect.width * 0.5f, -rect.height * 0.5f, rect.width, rect.height));
  canvas.restore();
}

float hintBounceOffset(Hint const& hint, std::int64_t now) {
  if (!hint.animating(now)) {
    return 0.f;
  }
  std::int64_t const elapsed = std::max<std::int64_t>(0, now - hint.startedNanos);
  double const phase = static_cast<double>(elapsed % kHintBounceCycleNanos) /
                       static_cast<double>(kHintBounceCycleNanos);
  return -kHintBounceAmplitude * std::sin(static_cast<float>(phase) * kPi);
}

Rect visualCardRect(CardPosition const& pos, SolitaireState const& state, std::int64_t now) {
  bool const hinted = state.hint.animating(now) && sourceMatches(pos.source, state.hint.source, true);
  float const offsetY = hinted ? hintBounceOffset(state.hint, now) : 0.f;
  return Rect::sharp(pos.rect.x, pos.rect.y + offsetY, pos.rect.width, pos.rect.height);
}

float visualCardScale(CardPosition const& pos, SolitaireState const& state, std::int64_t now) {
  return selectionScaleForSource(state, pos.source, now);
}

bool celebrationActive(SolitaireState const& state, std::int64_t now) {
  if (state.celebrationStartNanos <= 0) {
    return false;
  }
  return now < state.celebrationStartNanos + kWinCelebrationDurationNanos;
}

bool animationActive(FlyAnimation const& animation, std::int64_t now) {
  return now < animation.startNanos + animation.durationNanos;
}

bool animationVisible(FlyAnimation const& animation, std::int64_t now) {
  return now >= animation.startNanos && animationActive(animation, now);
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
  if (celebrationActive(state, now)) {
    for (auto const& pile : state.board.foundations) {
      for (Card const& card : pile) {
        ids.push_back(card.id);
      }
    }
  }
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

float positiveModulo(float value, float divisor) {
  if (divisor <= 0.f) {
    return 0.f;
  }
  float result = std::fmod(value, divisor);
  if (result < 0.f) {
    result += divisor;
  }
  return result;
}

float reflectedPosition(float value, float limit) {
  if (limit <= 0.f) {
    return 0.f;
  }
  float const period = limit * 2.f;
  float const wrapped = positiveModulo(value, period);
  return wrapped <= limit ? wrapped : period - wrapped;
}

int winCelebrationLaunchIndex(Card const& card) {
  return std::clamp(13 - card.rank, 0, 12);
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
  auto drawAnimation = [&](FlyAnimation const& animation) {
    if (!animationVisible(animation, now)) {
      return;
    }
    float const t = easedProgress(animation, now);
    for (std::size_t i = 0; i < animation.cards.size(); ++i) {
      Rect const from = i < animation.fromRects.size() ? animation.fromRects[i] : Rect::sharp(0.f, 0.f, kCardW, kCardH);
      Rect const to = i < animation.toRects.size() ? animation.toRects[i] : from;
      float const scale = 1.f + (kMovingCardScale - 1.f) * std::sin(t * kPi);
      drawCard(canvas, animation.cards[i], scaledRect(lerpRect(from, to, t), scale));
    }
  };

  for (FlyAnimation const& animation : state.animations) {
    drawAnimation(animation);
  }
}

void drawWinCelebration(Canvas& canvas, SolitaireState const& state, BoardGeometry const& geometry,
                        std::int64_t now) {
  if (!celebrationActive(state, now)) {
    return;
  }

  float const windowLeft = -geometry.origin.x / geometry.scale;
  float const windowRight = (geometry.viewport.width - geometry.origin.x) / geometry.scale;
  float const laneWidth = std::max(0.f, windowRight - windowLeft - kCardW);
  float const floorY = (geometry.viewport.height - geometry.origin.y) / geometry.scale - kCardH + 8.f;
  for (int foundation = 0; foundation < 4; ++foundation) {
    Rect const base = slotRect(PileKind::Foundation, foundation, geometry);
    auto const& pile = state.board.foundations[static_cast<std::size_t>(foundation)];

    for (int cardIndex = 0; cardIndex < static_cast<int>(pile.size()); ++cardIndex) {
      Card const& card = pile[static_cast<std::size_t>(cardIndex)];
      int const launchIndex = winCelebrationLaunchIndex(card);
      std::int64_t const localNanos = now - state.celebrationStartNanos -
                                      static_cast<std::int64_t>(launchIndex) * kWinCardIntervalNanos;
      if (localNanos <= 0) {
        drawCard(canvas, card, base);
        continue;
      }
      Rect rect = base;
      float angle = 0.f;
      float const t = static_cast<float>(localNanos) / 1'000'000'000.f;
      float const floorT = t * kWinCelebrationFloorSpeedScale;
      int const motionIndex = launchIndex * 4 + foundation;
      float const direction = (motionIndex % 2 == 0) ? 1.f : -1.f;
      float const speed = direction * (128.f + static_cast<float>((motionIndex * 19) % 96));
      float const x = windowLeft + reflectedPosition(base.x - windowLeft + speed * floorT +
                                                         static_cast<float>(motionIndex % 5) * 37.f,
                                                     laneWidth);
      float const jump = std::abs(std::sin(floorT * (4.1f + static_cast<float>(motionIndex % 4) * 0.35f) +
                                           static_cast<float>(motionIndex) * 0.41f));
      float const y = floorY - jump * (120.f + static_cast<float>(motionIndex % 8) * 15.f);
      float const intro = std::clamp(floorT / 0.34f, 0.f, 1.f);
      float const eased = intro * intro * (3.f - 2.f * intro);
      rect = Rect::sharp(base.x + (x - base.x) * eased,
                         base.y + (y - base.y) * eased,
                         kCardW, kCardH);
      angle = std::sin(floorT * 4.8f + static_cast<float>(motionIndex) * 0.27f) * 0.16f;
      drawRotatedCard(canvas, card, rect, angle);
    }
  }
}

void drawDraggedCards(Canvas& canvas, DragState const& drag) {
  if (!drag.active() || !drag.moved) {
    return;
  }
  for (std::size_t i = 0; i < drag.cards.size(); ++i) {
    drawCard(canvas, drag.cards[i], scaledRect(dragCardRect(drag, i), kMovingCardScale));
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

void drawBoardTable(Canvas& canvas, Rect frame, int feltIndex) {
  drawFelt(canvas, frame, feltIndex);

  BoardGeometry const geometry = boardGeometry(Size{frame.width, frame.height});
  canvas.save();
  canvas.translate(frame.x + geometry.origin.x, frame.y + geometry.origin.y);
  canvas.scale(geometry.scale);

  drawSlot(canvas, slotRect(PileKind::Stock, 0, geometry), "", false);
  drawSlot(canvas, slotRect(PileKind::Waste, 0, geometry), "", false);
  for (int i = 0; i < 4; ++i) {
    Suit const suit = std::array<Suit, 4>{Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs}
        [static_cast<std::size_t>(i)];
    drawSlot(canvas, slotRect(PileKind::Foundation, i, geometry), suitGlyph(suit), false);
  }
  for (int i = 0; i < 7; ++i) {
    drawSlot(canvas, slotRect(PileKind::Tableau, i, geometry), "", false);
  }

  canvas.restore();
}

void drawFloatingCompletionText(Canvas& canvas, Rect frame) {
  Point const center{frame.x + frame.width * 0.5f,
                     frame.y + frame.height * 0.46f};
  float panelWidth = std::clamp(frame.width * 0.42f, 360.f, 560.f);
  panelWidth = std::min(panelWidth, std::max(0.f, frame.width - 48.f));
  Rect const panel = Rect::sharp(center.x - panelWidth * 0.5f, center.y - 55.f, panelWidth, 110.f);

  canvas.drawRect(panel, CornerRadius {24.f},
                  // FillStyle::solid(Color {20.f / 255.f, 22.f / 255.f, 28.f / 255.f, 0.58f}),
                  glassFill(),
                  glassStroke(),
                  ShadowStyle {.radius = 18.f, .offset = {0.f, 5.f}, .color = Color {0.f, 0.f, 0.f, 0.22f}});

  auto shadowed = [&](std::string_view text, Font font, Color color, Point point) {
    drawCenteredText(canvas, text, font, Color{0.f, 0.f, 0.f, 0.32f},
                     Point{point.x, point.y + 2.f});
    drawCenteredText(canvas, text, font, color, point);
  };
  shadowed("Congratulations!", Font{.size = 30.f, .weight = 750.f},
           Color{1.f, 1.f, 1.f, 0.96f}, Point{center.x, center.y - 16.f});
  shadowed("Click anywhere to start a new game.", Font{.size = 14.f, .weight = 600.f},
           Color{1.f, 1.f, 1.f, 0.82f}, Point{center.x, center.y + 24.f});
}

void drawBoard(Canvas& canvas, Rect frame, SolitaireState const& state, int drawCount,
               std::int64_t frameNanos) {
  BoardGeometry const geometry = boardGeometry(Size{frame.width, frame.height});
  std::int64_t const animationNow = frameNanos > 0 ? frameNanos : nowNanos();
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

  Rect const stockSlot = slotRect(PileKind::Stock, 0, geometry);
  if (state.hint.source.kind == PileKind::Stock) {
    drawSlot(canvas, stockSlot, state.board.stock.empty() ? "↻" : "", true);
  } else if (state.board.stock.empty()) {
    drawCenteredText(canvas, "↻", Font{.size = 36.f, .weight = 300.f},
                     withAlpha(Colors::white, 0.36f), stockSlot.center());
  }
  for (int i = 0; i < 4; ++i) {
    Suit const suit = std::array<Suit, 4>{Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs}
        [static_cast<std::size_t>(i)];
    if (validDrop(PileKind::Foundation, i)) {
      drawSlot(canvas, slotRect(PileKind::Foundation, i, geometry), suitGlyph(suit), true);
    }
  }
  for (int i = 0; i < 7; ++i) {
    if (validDrop(PileKind::Tableau, i)) {
      drawSlot(canvas, slotRect(PileKind::Tableau, i, geometry), "", true);
    }
  }

  for (CardPosition const& pos : buildCardPositions(state.board, drawCount, geometry, hiddenIds)) {
    drawCard(canvas, pos.card, scaledRect(visualCardRect(pos, state, animationNow),
                                          visualCardScale(pos, state, animationNow)));
  }

  drawFlyAnimations(canvas, state, animationNow);
  drawWinCelebration(canvas, state, geometry, animationNow);
  drawDraggedCards(canvas, state.drag);
  if (state.peek.active(animationNow)) {
    drawCard(canvas, state.peek.card, state.peek.rect);
  }

  canvas.restore();
  if (state.completed && state.animations.empty() && !state.autoFinishing &&
      state.celebrationStartNanos > 0) {
    drawFloatingCompletionText(canvas, frame);
  }
}

struct StatView : ViewModifiers<StatView> {
    IconName icon {};
    Bindable<std::string> value;
    bool accent = false;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        return HStack {
            .spacing = theme().space2,
            .alignment = Alignment::Center,
            .children = children(
                Icon {.name = icon, .size = 16.f, .weight = 400.f, .color = Color::tertiary()},
                Text {
                    .text = value,
                    .font = Font {.size = 16.f, .weight = 650.f},
                    .color = accent ? Color::accent() : Color::primary(),
                }
            ),
        };
    }
};

struct HudIconButton : ViewModifiers<HudIconButton> {
  IconName icon = IconName::Info;
  bool primary = false;
  Reactive::Bindable<bool> disabled{false};
  std::function<void()> onTap;

  auto body() const {
    Reactive::Bindable<bool> disabledBinding = disabled;
    Reactive::Signal<bool> hovered = useHover();
    Reactive::Signal<bool> pressed = usePress();

    auto iconColor = [primary = primary, disabledBinding] {
      if (disabledBinding.evaluate()) {
        return Color{1.f, 1.f, 1.f, 0.42f};
      }
      return primary ? Colors::white : Color{1.f, 1.f, 1.f, 0.92f};
    };
    auto handleTap = [disabledBinding, onTap = onTap] {
      if (!disabledBinding.evaluate() && onTap) {
        onTap();
      }
    };

    auto chrome = Render{
        .measureFn = [](LayoutConstraints const&, LayoutHints const&) {
          return Size{40.f, 40.f};
        },
        .draw = [primary = primary, hovered, pressed, disabledBinding](Canvas& canvas, Rect frame) {
          auto fill = [&] {
            if (primary) {
              Color const top = hovered() ? Color::hex(0x3B8FFF) : Color::hex(0x0A84FF);
              Color const bottom = pressed() ? Color::hex(0x005BBF) : Color::hex(0x0066D6);
              return FillStyle::linearGradient(top, bottom, Point{0.f, 0.f}, Point{1.f, 1.f});
            }
            if (pressed() && !disabledBinding.evaluate()) {
              return FillStyle::solid(Color{1.f, 1.f, 1.f, 0.11f});
            }
            if (hovered() && !disabledBinding.evaluate()) {
              return FillStyle::solid(Color{1.f, 1.f, 1.f, 0.16f});
            }
            return glassFill();
          };
          auto shadow = [&] {
            if (disabledBinding.evaluate()) {
              return ShadowStyle::none();
            }
            if (primary) {
              return ShadowStyle{.radius = 16.f, .offset = {0.f, 5.f}, .color = Color{10.f / 255.f, 132.f / 255.f, 1.f, 0.26f}};
            }
            return glassShadow(hovered() ? 0.28f : 0.22f);
          };
          Rect const base = Rect::sharp(frame.x, frame.y, 40.f, 40.f);
          canvas.drawRect(base, CornerRadius{12.f}, fill(),
                          primary ? StrokeStyle::solid(Color{1.f, 1.f, 1.f, 0.18f}, 1.f) : glassStroke(),
                          shadow());
          canvas.drawRect(Rect::sharp(frame.x + 1.f, frame.y + 1.f, 38.f, 38.f),
                          CornerRadius{11.f},
                          FillStyle::linearGradient(Color{1.f, 1.f, 1.f, primary ? 0.12f : 0.07f},
                                                    Color{1.f, 1.f, 1.f, 0.f},
                                                    Point{0.f, 0.f}, Point{0.f, 1.f}),
                          StrokeStyle::none(),
                          ShadowStyle::none());
        },
    };

    Element button = ZStack{
        .horizontalAlignment = Alignment::Center,
        .verticalAlignment = Alignment::Center,
        .children = children(
            chrome,
            Icon{
                .name = icon,
                .size = 20.f,
                .weight = 500.f,
                .color = iconColor,
            }
        ),
    }
                         .size(40.f, 40.f)
                         .opacity([disabledBinding] { return disabledBinding.evaluate() ? 0.32f : 1.f; })
                         .cursor([disabledBinding] { return disabledBinding.evaluate() ? Cursor::Arrow : Cursor::Hand; })
                         .focusable([disabledBinding] { return !disabledBinding.evaluate(); })
                         .onTap(std::function<void()>{handleTap});

    return ScaleAroundCenter{
        .scale = [pressed, disabledBinding] {
          return pressed() && !disabledBinding.evaluate() ? 0.94f : 1.f;
        },
        .child = std::move(button),
    };
  }

  bool operator==(HudIconButton const& other) const {
    bool const sameDisabled = disabled.isValue() && other.disabled.isValue() &&
                              disabled.value() == other.disabled.value();
    return icon == other.icon && primary == other.primary && sameDisabled &&
           static_cast<bool>(onTap) == static_cast<bool>(other.onTap);
  }
};

struct HudStat : ViewModifiers<HudStat> {
  std::string label;
  Bindable<std::string> value;
  bool accent = false;

  auto body() const {
    return VStack{
        .spacing = 4.f,
        .alignment = Alignment::Stretch,
        .children = children(
            Text{
                .text = label,
                .font = Font{.size = 9.5f, .weight = 600.f},
                .color = Color{1.f, 1.f, 1.f, 0.45f},
                .horizontalAlignment = HorizontalAlignment::Leading,
                .verticalAlignment = VerticalAlignment::Center,
            },
            Text{
                .text = value,
                .font = Font{.size = 16.f, .weight = 600.f},
                .color = accent ? Color::hex(0xFBBF24) : Color{1.f, 1.f, 1.f, 0.96f},
                .horizontalAlignment = HorizontalAlignment::Leading,
                .verticalAlignment = VerticalAlignment::Center,
            }
        ),
    }
        .width(88.f)
        .padding(4.f, 18.f, 4.f, 18.f);
  }

  bool operator==(HudStat const& other) const {
    bool const sameValue = value.isValue() && other.value.isValue() && value.value() == other.value.value();
    return label == other.label && sameValue && accent == other.accent;
  }
};

struct PillDivider : ViewModifiers<PillDivider> {
  auto body() const {
    return Rectangle{}
        .size(1.f, 22.f)
        .fill(Color{1.f, 1.f, 1.f, 0.12f});
  }

  bool operator==(PillDivider const&) const = default;
};

struct HudStatsPill : ViewModifiers<HudStatsPill> {
  Signal<SolitaireState> state;

  auto body() const {
    return HStack{
        .spacing = 2.f,
        .alignment = Alignment::Center,
        .children = children(
            HudStat{
                .label = "Time",
                .value = [state = state] { return formatTime(state.evaluate().elapsedSeconds); },
            },
            PillDivider{},
            HudStat{
                .label = "Moves",
                .value = [state = state] { return std::to_string(state.evaluate().moves); },
            },
            PillDivider{},
            HudStat{
                .label = "Score",
                .value = [state = state] { return std::to_string(state.evaluate().score); },
                .accent = true,
            }
        ),
    }
        .height(56.f)
        .padding(8.f, 6.f, 8.f, 6.f)
        .fill(glassFill())
        .stroke(glassStroke())
        .cornerRadius(999.f)
        .shadow(ShadowStyle{.radius = 18.f, .offset = {0.f, 5.f}, .color = Color{0.f, 0.f, 0.f, 0.22f}});
  }

  bool operator==(HudStatsPill const& other) const { return state == other.state; }
};

struct SolitaireHud : ViewModifiers<SolitaireHud> {
  Signal<SolitaireState> state;
  std::function<void()> onSettings;

  auto body() const {
      auto theme = useEnvironment<ThemeKey>();

      return HStack {
          .children = children(
              HStack {
                  .spacing = theme().space4,
                  .alignment = Alignment::Center,
                  .justifyContent = JustifyContent::Start,
                  .children = children(
                    HudIconButton {
                        .icon = IconName::RestartAlt,
                        .primary = true,
                        .onTap = [state = state] { newGame(state); },
                    },
                    HudIconButton {
                        .icon = IconName::Lightbulb,
                        .disabled = [state = state] {
                                auto const& s = state.evaluate();
                                return playControlsDisabled(s); },
                        .onTap = [state = state] { showHint(state); },
                    },
                    HudIconButton {
                        .icon = IconName::Undo,
                        .disabled = [state = state] {
                                auto const& s = state.evaluate();
                                return s.completed || s.autoFinishing || dealAnimationRunning(s) ||
                                      s.history.empty(); },
                        .onTap = [state = state] { undo(state); },
                    }
                  ),
              }
                  .flex(1.f, 1.f, 0.f),
              HStack {
                  .spacing = theme().space4,
                  .alignment = Alignment::Center,
                  .justifyContent = JustifyContent::Center,
                  .children = children(
                    HudStatsPill {
                      .state = state
                    }
                  ),
              }
                  .flex(1.f, 1.f, 0.f),
              HStack {
                  .spacing = theme().space4,
                  .alignment = Alignment::Center,
                  .justifyContent = JustifyContent::End,
                  .children = children(
                    HudIconButton {
                        .icon = IconName::AutoAwesome,
                        .disabled = [state = state] {
                                auto const& s = state.evaluate();
                                return playControlsDisabled(s) || !s.animations.empty(); },
                        .onTap = [state = state] { autoFinish(state); },
                    },
                    HudIconButton {
                        .icon = IconName::Tune,
                        .onTap = onSettings,
                    }
                  ),
              }
                  .flex(1.f, 1.f, 0.f)
          ),
      }
          .padding(theme().space4, theme().space5, theme().space4, theme().space5);
  }

  bool operator==(SolitaireHud const& other) const {
    return state == other.state && static_cast<bool>(onSettings) == static_cast<bool>(other.onSettings);
  }
};

struct SettingsSection : ViewModifiers<SettingsSection> {
  std::string title;
  std::vector<Element> content;

  auto body() const {
    return VStack{
        .spacing = 8.f,
        .alignment = Alignment::Stretch,
        .children = children(
            Text{
                .text = title,
                .font = Font{.size = 12.f, .weight = 500.f},
                .color = Color{0.f, 0.f, 0.f, 0.55f},
            },
            VStack{
                .spacing = 6.f,
                .alignment = Alignment::Stretch,
                .children = content,
            }
        ),
    };
  }

  bool operator==(SettingsSection const& other) const { return title == other.title; }
};

struct ThemeSwatch : ViewModifiers<ThemeSwatch> {
  Signal<int> feltIndex;
  int index = 0;
  std::string label;

  auto body() const {
    auto selected = [feltIndex = feltIndex, index = index] {
      return feltIndex.evaluate() == index;
    };
    return VStack{
        .spacing = 6.f,
        .alignment = Alignment::Stretch,
        .children = children(
            Rectangle{}
                .height(64.f)
                .fill(feltPreviewFill(index))
                .stroke([selected] {
                  return selected() ? StrokeStyle::solid(Color::hex(0x0A84FF), 2.f)
                                    : StrokeStyle::solid(Color{0.f, 0.f, 0.f, 0.10f}, 1.f);
                })
                .cornerRadius(6.f),
            Text{
                .text = label,
                .font = Font{.size = 12.f, .weight = 500.f},
                .color = [selected] {
                  return selected() ? Color::hex(0x1A1A1A) : Color{0.f, 0.f, 0.f, 0.60f};
                },
                .horizontalAlignment = HorizontalAlignment::Center,
                .verticalAlignment = VerticalAlignment::Center,
            }
        ),
    }
        .cursor(Cursor::Hand)
        .onTap(std::function<void()>{[feltIndex = feltIndex, index = index] { feltIndex = index; }});
  }

  bool operator==(ThemeSwatch const& other) const {
    return feltIndex == other.feltIndex && index == other.index && label == other.label;
  }
};

struct SettingsDialog : ViewModifiers<SettingsDialog> {
  Signal<int> feltIndex;
  std::function<void()> onClose;

  auto body() const {
    auto feltSignal = feltIndex;
    auto closeAction = onClose;

    return Dialog{
        .title = "Settings",
        .content = children(
            SettingsSection{
                .title = "Felt theme",
                .content = children(
                    HStack{
                        .spacing = 8.f,
                        .alignment = Alignment::Stretch,
                        .children = children(
                            ThemeSwatch{.feltIndex = feltSignal, .index = 0, .label = "Emerald"}.flex(1.f, 1.f),
                            ThemeSwatch{.feltIndex = feltSignal, .index = 1, .label = "Sapphire"}.flex(1.f, 1.f),
                            ThemeSwatch{.feltIndex = feltSignal, .index = 2, .label = "Obsidian"}.flex(1.f, 1.f),
                            ThemeSwatch{.feltIndex = feltSignal, .index = 3, .label = "Crimson"}.flex(1.f, 1.f)
                        ),
                    }
                ),
            }
        ),
        .footer = children(
            Spacer{},
            Button{
                .label = "Done",
                .variant = ButtonVariant::Primary,
                .style = Button::Style{
                    .font = Font{.size = 13.f, .weight = 500.f},
                    .paddingH = 16.f,
                    .paddingV = 7.f,
                    .cornerRadius = 6.f,
                    .accentColor = Color::hex(0x0A84FF),
                },
                .onTap = closeAction,
            }.height(32.f)
        ),
        .onClose = closeAction,
    };
  }

  bool operator==(SettingsDialog const& other) const {
    return feltIndex == other.feltIndex && static_cast<bool>(onClose) == static_cast<bool>(other.onClose);
  }
};

struct BoardTableSurface : ViewModifiers<BoardTableSurface> {
  Signal<int> feltIndex;

  auto body() const {
    auto feltSignal = feltIndex;
    return Render{
        .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
          float const width = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 1200.f;
          float const height = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 760.f;
          return Size{std::max(1.f, width), std::max(1.f, height)};
        },
        .draw = [feltSignal](Canvas& canvas, Rect frame) {
          drawBoardTable(canvas, frame, feltSignal.evaluate());
        },
    }.rasterize();
  }

  bool operator==(BoardTableSurface const& other) const {
    return feltIndex == other.feltIndex;
  }
};

struct BoardCardSurface : ViewModifiers<BoardCardSurface> {
  Signal<SolitaireState> state;
  Signal<std::int64_t> frameNanos;

  auto body() const {
    Rect const bounds = useBounds();
    auto viewport = std::make_shared<Size>(Size{
        bounds.width > 0.f ? bounds.width : 1200.f,
        bounds.height > 0.f ? bounds.height : 760.f,
    });

    auto stateSignal = state;
    auto frameSignal = frameNanos;

    return Render{
        .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
          float const width = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 1200.f;
          float const height = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 760.f;
          return Size{std::max(1.f, width), std::max(1.f, height)};
        },
        .draw = [stateSignal, frameSignal, viewport](Canvas& canvas, Rect frame) {
          *viewport = Size{frame.width, frame.height};
          drawBoard(canvas, frame, stateSignal.evaluate(), kDrawCount, frameSignal.evaluate());
        },
    }
        .cursor(Cursor::Hand)
        .onPointerDown(std::function<void(Point, MouseButton)>{[stateSignal, viewport](Point p, MouseButton button) {
          SolitaireState const& snapshot = stateSignal.peek();
          if (snapshot.completed && snapshot.celebrationStartNanos > 0) {
            newGame(stateSignal);
            return;
          }
          if (button == MouseButton::Right) {
            startBoardPeek(stateSignal, p, *viewport);
            return;
          }
          if (button == MouseButton::Left) {
            startBoardDrag(stateSignal, p, *viewport);
          }
        }})
        .onPointerMove(std::function<void(Point)>{[stateSignal, viewport](Point p) {
          updateBoardDrag(stateSignal, p, *viewport);
        }})
        .onPointerUp(std::function<void(Point, MouseButton)>{[stateSignal, viewport](Point p, MouseButton button) {
          if (button == MouseButton::Right) {
            stopBoardPeek(stateSignal);
            return;
          }
          if (button == MouseButton::Left) {
            finishBoardDrag(stateSignal, p, *viewport);
          }
        }});
  }

  bool operator==(BoardCardSurface const& other) const {
    return state == other.state && frameNanos == other.frameNanos;
  }
};

struct RootView : ViewModifiers<RootView> {
  auto body() const {
    auto savedGame = loadGame();
    bool const useSavedBoard =
        savedGame && !savedGame->state.completed && !gameComplete(savedGame->state.board);
    auto state = useState<SolitaireState>(
        useSavedBoard ? savedGame->state : makeState(randomSeed(), kDrawCount));
    auto frameNanos = useState<std::int64_t>(0);
    auto feltIndex = useState<int>(0);
    auto [showSettings, hideSettings, settingsPresented] = useOverlay();
    (void)settingsPresented;

    useEffect([state] {
      saveGameIfNeeded(state.evaluate());
    });

    useFrame([state, frameNanos](AnimationTick const& tick) {
      SolitaireState const& snapshot = state.peek();
      if (snapshot.completed && snapshot.animations.empty() && !snapshot.autoFinishing &&
          !snapshot.dealing && !snapshot.hint.active() &&
          snapshot.peek.source.kind == PileKind::None && !snapshot.drag.active() &&
          celebrationActive(snapshot, tick.deadlineNanos)) {
        frameNanos = tick.deadlineNanos;
        return;
      }

      SolitaireState current = snapshot;
      bool changed = false;
      if (!current.completed) {
        int const elapsed = static_cast<int>(std::max<std::int64_t>(0, tick.deadlineNanos - current.startedNanos) /
                                             1'000'000'000);
        if (elapsed != current.elapsedSeconds) {
          current.elapsedSeconds = elapsed;
          changed = true;
        }
      }
      if (!current.animations.empty()) {
        current.frameNanos = tick.deadlineNanos;
        current.animations.erase(
            std::remove_if(current.animations.begin(), current.animations.end(), [now = tick.deadlineNanos](FlyAnimation const& animation) {
              return !animationActive(animation, now);
            }),
            current.animations.end());
        changed = true;
      }
      if (current.autoFinishing && current.animations.empty()) {
        if (scheduleNextAutoFinishMove(current, kDrawCount, BoardGeometry{}, tick.deadlineNanos, false)) {
          changed = true;
        } else {
          current.autoFinishing = false;
          if (!current.completed) {
            current.frameNanos = 0;
          }
          changed = true;
        }
      }
      if (current.dealing && current.animations.empty()) {
        current.dealing = false;
        if (!current.completed && !current.autoFinishing) {
          current.frameNanos = 0;
        }
        changed = true;
      }
      if (current.completed && current.celebrationStartNanos == 0 && current.animations.empty()) {
        current.celebrationStartNanos = tick.deadlineNanos;
        current.frameNanos = tick.deadlineNanos;
        changed = true;
      }
      if (current.hint.animating(tick.deadlineNanos)) {
        current.frameNanos = tick.deadlineNanos;
        changed = true;
      } else if (current.hint.active()) {
        current.hint = {};
        if (current.animations.empty() && !celebrationActive(current, tick.deadlineNanos)) {
          current.frameNanos = 0;
        }
        changed = true;
      }
      if (!current.selectionScaleAnimations.empty()) {
        current.frameNanos = tick.deadlineNanos;
        current.selectionScaleAnimations.erase(
            std::remove_if(current.selectionScaleAnimations.begin(), current.selectionScaleAnimations.end(),
                           [now = tick.deadlineNanos](SelectionScaleAnimation const& animation) {
                             return !animation.active(now);
                           }),
            current.selectionScaleAnimations.end());
        if (current.selectionScaleAnimations.empty() && current.animations.empty() &&
            !current.hint.animating(tick.deadlineNanos) &&
            !celebrationActive(current, tick.deadlineNanos)) {
          current.frameNanos = 0;
        }
        changed = true;
      }
      if (celebrationActive(current, tick.deadlineNanos)) {
        current.frameNanos = tick.deadlineNanos;
        changed = true;
      } else if (current.celebrationStartNanos > 0 && current.frameNanos != 0) {
        current.frameNanos = 0;
        changed = true;
      }
      if (current.peek.source.kind != PileKind::None && !current.peek.active(tick.deadlineNanos)) {
        current.peek = {};
        changed = true;
      }
      if (changed) {
        frameNanos = current.frameNanos;
        state = std::move(current);
      }
    });

    auto openSettings = [showSettings, hideSettings, feltIndex] {
      showSettings(
          SettingsDialog{
              .feltIndex = feltIndex,
              .onClose = hideSettings,
          },
          OverlayConfig{
              .modal = true,
              .backdropColor = Color{0.f, 0.f, 0.f, 0.28f},
              .dismissOnOutsideTap = true,
              .dismissOnEscape = true,
              .onDismiss = hideSettings,
              .debugName = "solitaire-settings",
          });
    };

    useWindowAction(
        "new-game",
        [state = state] { newGame(state); },
        ActionDescriptor{
            .label = "New Game",
            .shortcut = Shortcut{keys::N, Modifiers::Meta},
        });
    useWindowAction(
        "hint",
        [state = state] { showHint(state); },
        ActionDescriptor{
            .label = "Hint",
            .shortcut = Shortcut{keys::H, Modifiers::Meta | Modifiers::Shift},
            .isEnabled = [state = state] {
              auto const& s = state.evaluate();
              return !playControlsDisabled(s);
            },
        });
    useWindowAction(
        "undo",
        [state = state] { undo(state); },
        ActionDescriptor{
            .label = "Undo",
            .shortcut = shortcuts::Undo,
            .isEnabled = [state = state] {
              auto const& s = state.evaluate();
              return !s.completed && !s.autoFinishing && !dealAnimationRunning(s) &&
                     !s.history.empty();
            },
        });
    useWindowAction(
        "auto-finish",
        [state = state] { autoFinish(state); },
        ActionDescriptor{
            .label = "Auto-Finish",
            .shortcut = Shortcut{keys::Return, Modifiers::Meta},
            .isEnabled = [state = state] {
              auto const& s = state.evaluate();
              return !playControlsDisabled(s) && s.animations.empty();
            },
        });
    useWindowAction(
        "settings",
        openSettings,
        ActionDescriptor{
            .label = "Settings",
            .shortcut = Shortcut{keys::Comma, Modifiers::Meta},
        });
    useWindowAction(
        "new-game-after-win",
        [state = state] { newGame(state); },
        ActionDescriptor{
            .label = "New Game",
            .shortcut = Shortcut{keys::Escape, Modifiers::None},
            .isEnabled = [state = state] {
              auto const& s = state.evaluate();
              return s.completed && s.celebrationStartNanos > 0;
            },
        });

    return ZStack{
        .horizontalAlignment = Alignment::Stretch,
        .verticalAlignment = Alignment::Start,
        .children = children(
            BoardTableSurface{.feltIndex = feltIndex}
                .flex(1.f, 1.f),
            BoardCardSurface{.state = state, .frameNanos = frameNanos}
                .flex(1.f, 1.f),
            SolitaireHud{
                .state = state,
                .onSettings = openSettings,
            }
        ),
    };
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  app.setMenuBar(MenuBar{
      .menus = {
          MenuItem::submenu("Solitaire", {
              MenuItem::standard(MenuRole::AppAbout),
              MenuItem::separator(),
              MenuItem::action("Settings...", "settings", Shortcut{keys::Comma, Modifiers::Meta}),
              MenuItem::separator(),
              MenuItem::standard(MenuRole::AppHide),
              MenuItem::standard(MenuRole::AppHideOthers),
              MenuItem::standard(MenuRole::AppShowAll),
              MenuItem::separator(),
              MenuItem::standard(MenuRole::AppQuit),
          }),
          MenuItem::submenu("Game", {
              MenuItem::action("New Game", "new-game", Shortcut{keys::N, Modifiers::Meta}),
              MenuItem::separator(),
              MenuItem::action("Hint", "hint", Shortcut{keys::H, Modifiers::Meta | Modifiers::Shift}),
              MenuItem::action("Auto-Finish", "auto-finish", Shortcut{keys::Return, Modifiers::Meta}),
          }),
          MenuItem::submenu("Edit", {
              MenuItem::standard(MenuRole::EditUndo),
          }),
          MenuItem::submenu("View", {
              MenuItem::standard(MenuRole::WindowFullscreen),
          }),
          MenuItem::submenu("Window", {
              MenuItem::standard(MenuRole::WindowMinimize),
              MenuItem::standard(MenuRole::WindowZoom),
              MenuItem::separator(),
              MenuItem::standard(MenuRole::WindowBringAllToFront),
          }),
          MenuItem::submenu("Help", {
              MenuItem::standard(MenuRole::HelpSearch),
          }),
      },
      .appName = "Solitaire",
  });

  auto& window = app.createWindow<Window>({
      .size = {1200, 960},
      .title = "Solitaire",
      .resizable = true,
      .minSize = {800, 600},
      .restoreId = "main",
  });
  window.setTheme(Theme::light());
  window.setView<RootView>();

  return app.exec();
}
