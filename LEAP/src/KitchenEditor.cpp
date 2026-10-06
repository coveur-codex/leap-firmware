#include "KitchenEditor.h"
#include <algorithm>
#include <cstdlib>
namespace leap {
namespace {
constexpr uint16_t Ink = 0x3249, Wood = 0xcbef, White = 0xffdf, Steel = 0xad96;
constexpr uint16_t Fronts[] = {0xaeb9, 0xff18, 0xad5f, 0xfeb2};
// Clip every scene primitive, including objects straddling the viewport edges.
// No second framebuffer and no dependency on a hardware-specific clip API.
class SceneCanvas {
  Arduino_GFX &g;
  int left, top, right, bottom;
public:
  SceneCanvas(Arduino_GFX &gfx, int x, int y) : g(gfx), left(x + 4), top(y + 14),
      right(x + 320), bottom(y + 98) {}
  void drawPixel(int x, int y, uint16_t color) {
    if (x >= left && x < right && y >= top && y < bottom) g.drawPixel(x, y, color);
  }
  void fillRect(int x, int y, int w, int h, uint16_t color) {
    int x2 = std::min(x + w, right), y2 = std::min(y + h, bottom);
    x = std::max(x, left); y = std::max(y, top);
    if (x2 > x && y2 > y) g.fillRect(x, y, x2 - x, y2 - y, color);
  }
  void drawFastHLine(int x, int y, int w, uint16_t color) { fillRect(x, y, w, 1, color); }
  void drawRect(int x, int y, int w, int h, uint16_t color) {
    fillRect(x, y, w, 1, color); fillRect(x, y + h - 1, w, 1, color);
    fillRect(x, y, 1, h, color); fillRect(x + w - 1, y, 1, h, color);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    if (x >= left && y >= top && x + w <= right && y + h <= bottom) {
      g.fillRoundRect(x, y, w, h, r, color); return;
    }
    // Only partly visible objects need the scanline fallback.
    for (int row = 0; row < h; ++row) {
      int inset = (row == 0 || row == h - 1) ? r : 0;
      fillRect(x + inset, y + row, w - inset * 2, 1, color);
    }
  }
  void fillCircle(int x, int y, int r, uint16_t color) {
    if (x - r >= left && y - r >= top && x + r < right && y + r < bottom) {
      g.fillCircle(x, y, r, color); return;
    }
    for (int dy = -r; dy <= r; ++dy)
      for (int dx = -r; dx <= r; ++dx)
        if (dx * dx + dy * dy <= r * r) drawPixel(x + dx, y + dy, color);
  }
  void drawLine(int x, int y, int x2, int y2, uint16_t color) {
    int dx = std::abs(x2 - x), sx = x < x2 ? 1 : -1;
    int dy = -std::abs(y2 - y), sy = y < y2 ? 1 : -1, error = dx + dy;
    for (;;) {
      drawPixel(x, y, color);
      if (x == x2 && y == y2) break;
      int twice = 2 * error;
      if (twice >= dy) { error += dy; x += sx; }
      if (twice <= dx) { error += dx; y += sy; }
    }
  }
};
struct Bounds { int x, y, w, h; };
Bounds bounds(KitchenObject o, int left, int top) {
  int y = 60, h = 36;
  if (o.layer == KitchenLayer::Upper) { y = 18; h = 27; }
  if (o.layer == KitchenLayer::Counter) { y = 43; h = 17; }
  if (KitchenState::tall(o.type)) { y = 18; h = 78; }
  return {left + 6 + o.position * 26, top + y, o.width * 26 - 2, h};
}
void object(SceneCanvas &g, KitchenObject o, int left, int top) {
  auto b = bounds(o, left, top);
  int x = b.x, y = b.y, w = b.w, h = b.h;
  uint16_t front = Fronts[o.variant];
  auto box = [&](int bx, int by, int bw, int bh, uint16_t color) {
    g.fillRoundRect(bx, by, bw, bh, 2, color);
    g.drawRect(bx, by, bw, bh, Ink);
  };
  auto handle = [&](int hx, int hy, int hw) { g.fillRect(hx, hy, hw, 2, Ink); };
  bool cabinet = (o.layer == KitchenLayer::Floor && o.type != KitchenType::Table &&
                  o.type != KitchenType::NarrowTable && o.type != KitchenType::Chair &&
                  o.type != KitchenType::ChairLeft && o.type != KitchenType::ChairRight) || o.type == KitchenType::WallCabinet ||
                 o.type == KitchenType::WideWallCabinet;
  if (cabinet) {
    g.fillRect(x + 2, y + 2, w, h, 0x8c51);
    box(x, y, w, h, front);
    g.drawFastHLine(x + 2, y + 2, w - 4, White);
    if (o.layer == KitchenLayer::Floor) {
      g.fillRect(x + 2, y + h - 4, w - 4, 4, Ink);
      if (!KitchenState::tall(o.type)) {
        g.fillRect(x - 1, y - 3, w + 2, 4, Wood);
        g.drawFastHLine(x, y - 3, w, White);
      }
    }
  }
  switch (o.type) {
  case KitchenType::Cabinet:
  case KitchenType::WallCabinet:
    g.drawRect(x + 3, y + 4, w - 6, h - 10, 0x8c71);
    handle(x + w - 7, y + 9, 4); break;
  case KitchenType::WideWallCabinet:
  case KitchenType::Pantry:
    g.drawLine(x + w / 2, y + 2, x + w / 2, y + h - 5, Ink);
    handle(x + w / 2 - 7, y + 12, 4); handle(x + w / 2 + 3, y + 12, 4); break;
  case KitchenType::Drawers:
    for (int row = 0; row < 3; ++row) {
      g.drawFastHLine(x + 2, y + 10 + row * 10, w - 4, Ink);
      handle(x + 8, y + 4 + row * 10, 8);
    } break;
  case KitchenType::Sink:
    g.drawLine(x + w / 2, y + 3, x + w / 2, y + h - 5, Ink);
    handle(x + 9, y + 8, 7); handle(x + w - 16, y + 8, 7);
    box(x + 6, y - 5, w - 12, 7, Steel);
    g.fillRect(x + 10, y - 3, w - 20, 3, 0x7c73);
    g.drawLine(x + w / 2, y - 6, x + w / 2, y - 14, Ink);
    g.drawLine(x + w / 2, y - 14, x + w / 2 + 7, y - 14, Ink);
    g.drawLine(x + w / 2 + 7, y - 14, x + w / 2 + 7, y - 10, Ink); break;
  case KitchenType::Stove:
    g.fillRect(x + 2, y - 4, w - 4, 5, Ink);
    for (int i = 0; i < 4; ++i) {
      g.fillCircle(x + 8 + i * 11, y - 2, 2, Steel);
      g.fillCircle(x + 8 + i * 11, y + 5, 2, Ink);
    }
    box(x + 4, y + 11, w - 8, 19, Ink);
    g.fillRect(x + 7, y + 15, w - 14, 12, 0x5390);
    g.drawLine(x + 9, y + 24, x + 19, y + 17, 0x9e3c);
    handle(x + 8, y + 11, w - 16); break;
  case KitchenType::Dishwasher:
    g.fillRect(x + 2, y + 2, w - 4, 7, Steel);
    g.fillCircle(x + 5, y + 5, 1, Ink);
    g.fillRect(x + w - 8, y + 4, 4, 2, 0x06b8);
    handle(x + 5, y + 11, w - 10);
    g.drawRect(x + 3, y + 17, w - 6, 12, 0x8c71); break;
  case KitchenType::Fridge:
    g.fillRect(x + 2, y + 3, w - 4, h - 8, White);
    g.drawFastHLine(x + 2, y + 24, w - 4, Ink);
    g.fillRect(x + w - 8, y + 10, 2, 10, Ink);
    g.fillRect(x + w - 8, y + 31, 2, 17, Ink);
    g.fillRect(x + 8, y + 38, 7, 9, Fronts[o.variant]);
    g.fillCircle(x + 11, y + 36, 1, 0xf800); break;
  case KitchenType::Shelf:
    g.fillRect(x, y, 3, h, Wood); g.fillRect(x + w - 3, y, 3, h, Wood);
    for (int r = 0; r < 2; ++r) {
      g.fillRect(x, y + 12 + r * 13, w, 3, Wood);
      for (int c = 0; c < 3; ++c) {
        box(x + 6 + c * 13, y + 4 + r * 13, 8, 8, Fronts[(o.variant + c) % 4]);
        g.drawFastHLine(x + 7 + c * 13, y + 5 + r * 13, 6, White);
      }
    } break;
  case KitchenType::Hood:
    box(x + w / 2 - 6, y, 12, 17, Steel);
    box(x + 3, y + 16, w - 6, 9, Steel);
    g.fillRect(x + 6, y + 22, w - 12, 3, Ink);
    g.fillCircle(x + 9, y + 20, 1, White); g.fillCircle(x + w - 9, y + 20, 1, White); break;
  case KitchenType::Microwave:
    box(x, y + 2, w, h - 2, Steel);
    box(x + 3, y + 5, w - 15, 9, Ink);
    g.drawLine(x + 5, y + 11, x + 15, y + 6, 0x7c73);
    g.fillRect(x + w - 9, y + 5, 5, 3, 0x06b8);
    g.fillCircle(x + w - 6, y + 12, 2, Ink); break;
  case KitchenType::Coffee:
    box(x + 3, y, w - 6, h, front);
    g.fillRect(x + 6, y + 4, w - 12, 9, Ink);
    g.fillRect(x + 10, y + 8, 7, 5, White);
    g.drawRect(x + 17, y + 9, 3, 3, White);
    g.fillCircle(x + 8, y + 2, 1, 0x06b8);
    g.fillRect(x + 5, y + h - 3, w - 10, 2, Steel); break;
  case KitchenType::Toaster:
    box(x + 2, y + 6, w - 4, h - 6, front);
    for (int i = 0; i < 2; ++i) {
      g.fillRoundRect(x + 5 + i * 9, y + 2, 7, 7, 2, Wood);
      g.drawFastHLine(x + 6 + i * 9, y + 3, 5, White);
    }
    g.fillRect(x + w - 5, y + 9, 2, 4, Ink); break;
  case KitchenType::Plant:
    g.fillRoundRect(x + 7, y + 10, 11, 7, 2, 0xd38c);
    g.drawLine(x + 12, y + 11, x + 12, y + 2, 0x3387);
    g.fillCircle(x + 8, y + 5, 4, 0x650b);
    g.fillCircle(x + 16, y + 3, 4, 0x3387);
    g.fillCircle(x + 13, y + 7, 3, 0x650b); break;
  case KitchenType::Table:
  case KitchenType::NarrowTable:
    g.fillRect(x + 4, y + 3, 5, h - 3, Wood);
    g.fillRect(x + w - 9, y + 3, 5, h - 3, Wood);
    g.drawLine(x + 7, y + 7, x + 7, y + h - 1, Ink);
    g.drawLine(x + w - 7, y + 7, x + w - 7, y + h - 1, Ink);
    box(x, y - 3, w, 6, front);
    g.drawFastHLine(x + 3, y - 2, w - 6, White);
    g.fillRect(x + 9, y + 4, w - 18, 3, Wood);
    for (int i = 15; i < w - 8; i += 20) g.drawFastHLine(x + i, y, 12, Wood);
    break;
  case KitchenType::Chair:
    box(x + 3, y - 10, w - 6, 23, Wood);
    g.fillRect(x + 6, y - 7, w - 12, 15, front);
    g.drawFastHLine(x + 7, y - 4, w - 14, White);
    box(x + 1, y + 13, w - 2, 5, front);
    g.fillRect(x + 3, y + 18, 3, h - 18, Wood);
    g.fillRect(x + w - 6, y + 18, 3, h - 18, Wood);
    g.drawFastHLine(x + 4, y + 27, w - 8, Ink);
    break;
  case KitchenType::Kettle:
    box(x + 5, y + 4, 13, 11, front);
    g.drawRect(x + 17, y + 5, 5, 7, Ink);
    g.drawLine(x + 5, y + 7, x + 2, y + 4, Steel);
    g.fillRect(x + 7, y + 2, 9, 2, Steel);
    g.fillCircle(x + 11, y + 1, 1, Ink);
    g.fillRect(x + 4, y + 15, 15, 2, Ink);
    g.drawLine(x + 8, y + 6, x + 8, y + 12, White);
    break;
  case KitchenType::KnifeBlock:
    box(x + 5, y + 8, 15, 9, Wood);
    for (int i = 0; i < 3; ++i) {
      g.fillRect(x + 7 + i * 4, y + 4 - i, 2, 7, Steel);
      g.fillRect(x + 7 + i * 4, y - i, 2, 5, Ink);
    }
    g.drawLine(x + 7, y + 12, x + 17, y + 15, 0xb34d);
    break;
  case KitchenType::FruitBowl:
    for (int i = 0; i < 5; ++i) {
      g.fillCircle(x + 9 + i * 8, y + 8 - (i % 2) * 3, 4, i % 2 ? 0xfec0 : 0xe986);
      g.drawLine(x + 9 + i * 8, y + 5 - (i % 2) * 3, x + 10 + i * 8, y + 2 - (i % 2) * 3, 0x3387);
    }
    box(x + 4, y + 10, w - 8, 7, front);
    g.drawFastHLine(x + 7, y + 12, w - 14, White);
    break;
  case KitchenType::SpiceRack:
    g.fillRect(x + 1, y + 3, 3, h - 3, Wood);
    g.fillRect(x + w - 4, y + 3, 3, h - 3, Wood);
    for (int row = 0; row < 2; ++row) {
      g.fillRect(x, y + 13 + row * 11, w, 3, Wood);
      for (int i = 0; i < 4; ++i) {
        box(x + 6 + i * 10, y + 5 + row * 11, 7, 8, Fronts[(i + o.variant) % 4]);
        g.fillRect(x + 6 + i * 10, y + 4 + row * 11, 7, 2, Ink);
        g.fillRect(x + 8 + i * 10, y + 8 + row * 11, 3, 3, White);
      }
    }
    break;
  case KitchenType::Clock:
    g.fillCircle(x + 12, y + 13, 11, front);
    g.fillCircle(x + 12, y + 13, 9, White);
    for (int i : {0, 1}) {
      g.fillRect(x + 11, y + 5 + i * 15, 2, 2, Ink);
      g.fillRect(x + 4 + i * 15, y + 12, 2, 2, Ink);
    }
    g.drawLine(x + 12, y + 13, x + 12, y + 7, Ink);
    g.drawLine(x + 12, y + 13, x + 17, y + 16, Ink);
    g.fillCircle(x + 12, y + 13, 1, Ink);
    break;
  case KitchenType::Mixer:
    // Stand mixer: motor arm, vertical stand, whisk, steel bowl and speed dial.
    box(x + 3, y + 14, w - 6, 3, front);
    box(x + 4, y + 3, 9, 12, front);
    box(x + 5, y, w - 14, 6, front);
    g.drawFastHLine(x + 8, y + 1, w - 22, White);
    g.fillCircle(x + 10, y + 6, 2, Ink);
    g.drawPixel(x + 10, y + 5, White);
    g.fillRect(x + 29, y + 5, 2, 5, Steel);
    g.drawLine(x + 26, y + 7, x + 29, y + 11, Ink);
    g.drawLine(x + 33, y + 7, x + 30, y + 11, Ink);
    box(x + 18, y + 9, 24, 6, Steel);
    g.drawFastHLine(x + 19, y + 9, 22, White);
    g.drawFastHLine(x + 22, y + 12, 16, 0x7c73);
    break;
  case KitchenType::PlateShelf:
    // Plate rack with two ledges, upright plates and visible hanging brackets.
    g.fillRect(x + 1, y + 1, 3, h - 2, Wood);
    g.fillRect(x + w - 4, y + 1, 3, h - 2, Wood);
    for (int row = 0; row < 2; ++row) {
      for (int plate = 0; plate < 3; ++plate) {
        int px = x + 10 + plate * 15, py = y + 6 + row * 12;
        g.fillCircle(px, py, 5, front);
        g.fillCircle(px, py, 4, White);
        g.fillCircle(px, py, 2, front);
        g.drawPixel(px - 2, py - 2, White);
      }
      g.fillRect(x, y + 11 + row * 12, w, 3, Wood);
      g.drawFastHLine(x + 2, y + 11 + row * 12, w - 4, White);
    }
    g.drawLine(x + 5, y + 25, x + 10, y + 25, Ink);
    g.drawLine(x + w - 10, y + 25, x + w - 5, y + 25, Ink);
    break;
  case KitchenType::Candle:
    // Candle, wick, flame and brass-style stem/base; fits on either table.
    g.fillRect(x + 5, y + 15, w - 10, 2, front);
    g.fillRect(x + 10, y + 11, 4, 4, front);
    g.drawFastHLine(x + 7, y + 10, 10, Ink);
    g.fillRect(x + 9, y + 4, 6, 6, White);
    g.drawLine(x + 10, y + 5, x + 10, y + 8, 0xfeb2);
    g.drawPixel(x + 12, y + 3, Ink);
    g.fillCircle(x + 12, y + 1, 2, 0xfd20);
    g.drawPixel(x + 12, y - 2, 0xfd20);
    g.drawPixel(x + 12, y + 1, 0xffe0);
    break;
  case KitchenType::ChairLeft:
  case KitchenType::ChairRight: {
    // Mirror the side profile independently of the four upholstery colors.
    auto sideBox = [&](int dx, int dy, int bw, int bh, uint16_t color) {
      int sx = o.type == KitchenType::ChairRight ? dx : w - dx - bw;
      g.fillRect(x + sx, y + dy, bw, bh, color);
      if (bw > 3 && bh > 2) g.drawRect(x + sx, y + dy, bw, bh, Ink);
    };
    sideBox(2, -10, 5, 25, Wood);
    sideBox(3, -8, 3, 19, front);
    sideBox(5, 13, w - 7, 5, front);
    sideBox(3, 18, 3, h - 18, Wood);
    sideBox(w - 6, 18, 3, h - 18, Wood);
    sideBox(6, 26, w - 12, 2, Wood);
    sideBox(7, 14, w - 12, 1, White);
    break;
  }
  case KitchenType::Count: break;
  }
}
} // namespace
void KitchenEditor::start() {
  position = choice = variant = viewport = menuChoice = 0;
  moving = -1;
  layer = KitchenLayer::Floor;
  action = KitchenAction::Build;
  blocked = leaveRequested = resetConfirm = confirmClear = false;
}
void KitchenEditor::browse(int direction) {
  do {
    choice = (choice + int(KitchenType::Count) + direction) % int(KitchenType::Count);
  } while (KitchenCatalog[choice].layer != layer);
}
void KitchenEditor::followCursor() {
  if (position < viewport) viewport = position;
  if (position >= viewport + VisibleColumns) viewport = position - VisibleColumns + 1;
  viewport = std::max(0, std::min(viewport, KitchenState::Columns - VisibleColumns));
}
bool KitchenEditor::input(const InputEvent &e) {
  if (e.longPress) {
    if (!e.right && e.key == Key::Center && e.heldMs >= Input::ExtendedCenterHoldMs)
      leaveRequested = true;
    return false;
  }
  if (resetConfirm) {
    if (e.key == Key::Center) {
      bool clear = e.right && confirmClear;
      resetConfirm = confirmClear = false;
      if (clear) {
        bool changed = state.count != 0;
        state = KitchenState{};
        moving = -1;
        return changed;
      }
    } else if (e.right) {
      if (e.key == Key::Right || e.key == Key::Down) confirmClear = true;
      if (e.key == Key::Left || e.key == Key::Up) confirmClear = false;
    }
    return false;
  }
  blocked = false;
  if (!e.right) {
    if (e.key == Key::Up || e.key == Key::Down || e.key == Key::Center) {
      int direction = e.key == Key::Up ? -1 : 1;
      action = KitchenAction((int(action) + int(KitchenAction::Count) + direction) % int(KitchenAction::Count));
      moving = -1; // Mode changes cancel an unconfirmed move.
    } else {
      int direction = e.key == Key::Right ? 1 : -1;
      if (action == KitchenAction::Build) browse(direction);
      if (action == KitchenAction::Color)
        variant = (variant + KitchenState::Variants + direction) % KitchenState::Variants;
      if (action == KitchenAction::Menu) menuChoice = 1 - menuChoice;
    }
    return false;
  }
  if (action == KitchenAction::Menu) {
    if (e.key != Key::Center) menuChoice = 1 - menuChoice;
    else if (menuChoice == 0) leaveRequested = true;
    else { resetConfirm = true; confirmClear = false; }
    return false;
  }
  if (action == KitchenAction::Scroll) {
    int delta = (e.key == Key::Right) - (e.key == Key::Left);
    int old = viewport;
    viewport = std::max(0, std::min(viewport + delta, KitchenState::Columns - VisibleColumns));
    position = std::max(0, std::min(position + viewport - old, KitchenState::Columns - 1));
    return false;
  }
  if (e.key == Key::Left || e.key == Key::Right) {
    position = std::max(0, std::min(position + (e.key == Key::Right ? 1 : -1), KitchenState::Columns - 1));
    followCursor();
  }
  if (e.key == Key::Up || e.key == Key::Down) {
    // An object being moved keeps its semantic layer.
    if (moving < 0) {
      layer = KitchenLayer((int(layer) + 3 + (e.key == Key::Up ? 1 : -1)) % 3);
      if (KitchenCatalog[choice].layer != layer) {
        choice = 0;
        while (KitchenCatalog[choice].layer != layer) ++choice;
      }
    }
  }
  if (e.key != Key::Center) return false;
  int target = state.at(layer, position);
  if (action == KitchenAction::Move && moving < 0) {
    moving = target;
    if (moving >= 0) {
      auto o = state.objects[moving];
      choice = int(o.type); variant = o.variant; position = o.position;
      followCursor();
    } else blocked = true;
    return false;
  }
  KitchenState before = state;
  bool success = false;
  if (action == KitchenAction::Remove) success = state.remove(layer, position);
  if (action == KitchenAction::Color && target >= 0) {
    state.objects[target].variant = uint8_t(variant);
    success = true;
  }
  if (action == KitchenAction::Build || (action == KitchenAction::Move && moving >= 0))
    success = state.place(kitchenObject(KitchenType(choice), position, variant), moving);
  blocked = !success;
  if (!success) return false;
  moving = -1;
  if (before.count != state.count) return true;
  for (int i = 0; i < state.count; ++i)
    if (!(before.objects[i] == state.objects[i])) return true;
  return false;
}
void KitchenEditor::draw(Arduino_GFX &g, int left, int top, bool savePending) {
  static const char *actions[] = {"Scrollen", "Bauen", "Verschieben", "Abreissen", "Farbe", "Menue"};
  g.fillRect(left, top, 324, 142, 0xff9a);
  g.setTextSize(1); g.setTextColor(Ink); g.setCursor(left + 4, top + 2);
  g.printf("%s: %s", actions[int(action)], action == KitchenAction::Menu ?
           (menuChoice ? "Kueche leeren" : "Zurueck") :
           action == KitchenAction::Build || action == KitchenAction::Move ? KitchenCatalog[choice].name : "");
  for (int i = 0; i < 4; ++i) {
    g.fillRect(left + 268 + i * 13, top + 1, 10, 9, Fronts[i]);
    if (variant == i) g.drawRect(left + 267 + i * 13, top, 12, 11, Ink);
  }
  g.fillRect(left + 4, top + 14, 316, 84, 0xf739);
  for (int y = 46; y < 59; y += 6) {
    g.drawFastHLine(left + 4, top + y, 316, 0xded5);
    for (int x = 4 + ((y / 6) % 2) * 13; x < 320; x += 26)
      g.drawLine(left + x, top + y, left + x, top + y + 5, 0xded5);
  }
  g.fillRect(left + 4, top + 98, 316, 8, Wood);
  for (int x = 4; x < 315; x += 39) g.drawLine(left + x, top + 98, left + x + 5, top + 105, 0xb34d);
  g.drawFastHLine(left + 4, top + 59, 316, 0xb596);
  SceneCanvas canvas(g, left, top);
  int sceneLeft = left - viewport * 26;
  auto visible = [&](KitchenObject o) {
    return o.position < viewport + VisibleColumns && o.position + o.width > viewport;
  };
  for (int i = 0; i < state.count; ++i)
    if (visible(state.objects[i])) object(canvas, state.objects[i], sceneLeft, top);
  if (resetConfirm) {
    g.fillRoundRect(left + 22, top + 30, 280, 65, 5, White);
    g.drawRect(left + 22, top + 30, 280, 65, Ink);
    g.setCursor(left + 38, top + 38); g.print("Kueche leeren?");
    g.setCursor(left + 38, top + 50); g.print("Alle Gegenstaende entfernen.");
    g.fillRoundRect(left + 36, top + 67, 118, 18, 3, confirmClear ? Steel : 0x05a8);
    g.fillRoundRect(left + 169, top + 67, 118, 18, 3, confirmClear ? 0xfeb2 : Steel);
    g.setCursor(left + 46, top + 72); g.print("Abbrechen");
    g.setCursor(left + 185, top + 72); g.print("Leeren");
  } else if (action != KitchenAction::Menu && action != KitchenAction::Scroll) {
    int target = state.at(layer, position);
    bool building = action == KitchenAction::Build || (action == KitchenAction::Move && moving >= 0);
    auto preview = kitchenObject(building ? KitchenType(choice) : KitchenType::Cabinet, position, variant);
    if (!building) {
      if (target >= 0) preview = state.objects[target];
      else preview.layer = layer;
    }
    bool fits = building ? state.place(preview, moving, false) : target >= 0;
    if (building && fits) object(canvas, preview, sceneLeft, top);
    auto b = bounds(preview, sceneLeft, top);
    uint16_t highlight = fits ? 0x05a8 : 0xf900;
    canvas.drawRect(b.x - 1, b.y - 1, b.w + 2, b.h + 2, highlight);
    canvas.drawRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, highlight);
  }
  // Track covers the full 24-column kitchen; the thumb covers the visible half.
  g.fillRect(left + 4, top + 107, 316, 2, Steel);
  g.fillRect(left + 4 + viewport * 316 / KitchenState::Columns, top + 107,
             316 * VisibleColumns / KitchenState::Columns, 2, Ink);
  const char *layers[] = {"Unten", "Platte", "Oben"};
  g.setCursor(left + 4, top + 111);
  g.printf("%s %d/24  Sicht %d-%d %s", layers[int(layer)], position + 1, viewport + 1,
           viewport + VisibleColumns, savePending ? "Speichern..." : blocked ? "Geht nicht!" : "");
  g.setCursor(left + 4, top + 121);
  if (resetConfirm) g.print("R: waehlen/OK  L OK: abbrechen");
  else if (action == KitchenAction::Build) g.print("L: Modus/Objekt  R: Ort/Ebene/OK");
  else if (action == KitchenAction::Color) g.print("L: Modus/Farbe  R: Ort/Ebene/OK");
  else if (action == KitchenAction::Scroll) g.print("L hoch/runter: Modus  R: scrollen");
  else if (action == KitchenAction::Menu) g.print("L: Modus  R: waehlen/OK");
  else g.print(moving >= 0 ? "L: abbrechen  R: Ort/OK absetzen" : "L: Modus  R: Ort/Ebene/OK");
  g.setCursor(left + 4, top + 131); g.print("L OK 2s halten: zur Spieleauswahl");
}
} // namespace leap
