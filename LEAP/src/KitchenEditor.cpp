#include "KitchenEditor.h"
namespace leap {
namespace {
constexpr uint16_t Ink = 0x3249, Wood = 0xcbef, White = 0xffdf, Steel = 0xad96;
constexpr uint16_t Fronts[] = {0xaeb9, 0xff18, 0xad5f, 0xfeb2};
struct Bounds { int x, y, w, h; };
Bounds bounds(KitchenObject o, int left, int top) {
  int y = 60, h = 36;
  if (o.layer == KitchenLayer::Upper) { y = 18; h = 27; }
  if (o.layer == KitchenLayer::Counter) { y = 43; h = 17; }
  if (KitchenState::tall(o.type)) { y = 18; h = 78; }
  return {left + 6 + o.position * 26, top + y, o.width * 26 - 2, h};
}
void object(Arduino_GFX &g, KitchenObject o, int left, int top) {
  auto b = bounds(o, left, top);
  int x = b.x, y = b.y, w = b.w, h = b.h;
  uint16_t front = Fronts[o.variant];
  auto box = [&](int bx, int by, int bw, int bh, uint16_t color) {
    g.fillRoundRect(bx, by, bw, bh, 2, color);
    g.drawRect(bx, by, bw, bh, Ink);
  };
  auto handle = [&](int hx, int hy, int hw) { g.fillRect(hx, hy, hw, 2, Ink); };
  bool cabinet = o.layer == KitchenLayer::Floor || o.type == KitchenType::WallCabinet ||
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
  case KitchenType::Count: break;
  }
}
} // namespace
void KitchenEditor::start() {
  position = choice = variant = 0;
  moving = -1;
  layer = KitchenLayer::Floor;
  blocked = leaveRequested = resetConfirm = confirmClear = false;
}
void KitchenEditor::browse(int direction) {
  moving = -1;
  do {
    choice = (choice + ChoiceCount + direction) % ChoiceCount;
  } while (choice < int(KitchenType::Count) && KitchenCatalog[choice].layer != layer);
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
        start();
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
    position = (position + KitchenState::Columns + (e.key == Key::Right) - (e.key == Key::Left)) %
               KitchenState::Columns;
    if (e.key == Key::Up || e.key == Key::Down) {
      layer = KitchenLayer((int(layer) + 3 + (e.key == Key::Down ? -1 : 1)) % 3);
      if (moving < 0 && choice < int(KitchenType::Count) && KitchenCatalog[choice].layer != layer)
        browse(1);
    }
    if (e.key == Key::Center) {
      if (moving >= 0) { moving = -1; return false; }
      moving = state.at(layer, position);
      if (moving >= 0) {
        auto o = state.objects[moving];
        choice = int(o.type); variant = o.variant; position = o.position;
      }
    }
    return false;
  }
  if (e.key == Key::Left || e.key == Key::Right) browse(e.key == Key::Right ? 1 : -1);
  if (e.key == Key::Up || e.key == Key::Down)
    variant = (variant + KitchenState::Variants + (e.key == Key::Up ? 1 : -1)) % KitchenState::Variants;
  if (e.key != Key::Center) return false;
  if (choice == BackChoice) {
    leaveRequested = true;
    return false;
  }
  if (choice == ResetChoice) {
    resetConfirm = true;
    confirmClear = false;
    return false;
  }
  KitchenState before = state;
  bool changed;
  if (choice == RemoveChoice) changed = state.remove(layer, position);
  else {
    auto o = kitchenObject(KitchenType(choice), position, variant);
    changed = o.layer == layer && state.place(o, moving);
  }
  blocked = !changed;
  if (changed) moving = -1;
  if (!changed) return false;
  if (before.count != state.count) return true;
  for (int i = 0; i < state.count; ++i)
    if (!(before.objects[i] == state.objects[i])) return true;
  return false;
}
void KitchenEditor::draw(Arduino_GFX &g, int left, int top, bool savePending) {
  g.fillRect(left, top, 324, 132, 0xff9a);
  g.setTextSize(1); g.setTextColor(Ink); g.setCursor(left + 4, top + 2);
  const char *label = choice == RemoveChoice ? "Entfernen" : choice == BackChoice ? "Zurueck" :
                      choice == ResetChoice ? "Kueche leeren" : KitchenCatalog[choice].name;
  g.print(label);
  for (int i = 0; i < 4; ++i) {
    g.fillRect(left + 236 + i * 13, top + 1, 10, 9, Fronts[i]);
    if (variant == i) g.drawRect(left + 235 + i * 13, top, 12, 11, Ink);
  }
  // Wallpaper, tiled backsplash, wooden floor and a permanent rail at worktop height.
  g.fillRect(left + 4, top + 14, 316, 84, 0xf739);
  for (int y = 46; y < 59; y += 6) {
    g.drawFastHLine(left + 4, top + y, 316, 0xded5);
    for (int x = 4 + ((y / 6) % 2) * 13; x < 320; x += 26)
      g.drawLine(left + x, top + y, left + x, top + y + 5, 0xded5);
  }
  g.fillRect(left + 4, top + 98, 316, 8, Wood);
  for (int x = 4; x < 320; x += 39) g.drawLine(left + x, top + 98, left + x + 5, top + 105, 0xb34d);
  g.drawFastHLine(left + 4, top + 59, 316, 0xb596);
  for (int i = 0; i < state.count; ++i) object(g, state.objects[i], left, top);
  if (resetConfirm) {
    g.fillRoundRect(left + 22, top + 30, 280, 65, 5, White);
    g.drawRect(left + 22, top + 30, 280, 65, Ink);
    g.setCursor(left + 38, top + 38); g.print("Kueche leeren?");
    g.setCursor(left + 38, top + 50); g.print("Alle Gegenstaende entfernen.");
    g.fillRoundRect(left + 36, top + 67, 118, 18, 3, confirmClear ? Steel : 0x05a8);
    g.fillRoundRect(left + 169, top + 67, 118, 18, 3, confirmClear ? 0xfeb2 : Steel);
    g.setCursor(left + 46, top + 72); g.print("Abbrechen");
    g.setCursor(left + 185, top + 72); g.print("Leeren");
    g.setCursor(left + 4, top + 108); g.print("R: waehlen + OK  L OK: abbrechen");
    g.setCursor(left + 4, top + 128); g.print("L OK 2s halten: zur Spieleauswahl");
    return;
  }
  if (choice == BackChoice || choice == ResetChoice) {
    g.setCursor(left + 4, top + 108);
    g.print(choice == BackChoice ? "R OK: zur Spieleauswahl" : "R OK: Kueche leeren?");
    g.setCursor(left + 4, top + 118); g.print("R links/rechts: Objekt oder Aktion");
    g.setCursor(left + 4, top + 128); g.print("L OK 2s halten: zur Spieleauswahl");
    return;
  }
  bool deleting = choice == RemoveChoice;
  auto preview = kitchenObject(deleting ? KitchenType::Cabinet : KitchenType(choice), position, variant);
  if (deleting) {
    int target = state.at(layer, position);
    if (target >= 0) preview = state.objects[target];
    else preview.layer = layer;
  }
  auto b = bounds(preview, left, top);
  bool fits = deleting ? state.at(layer, position) >= 0 : preview.layer == layer && state.place(preview, moving, false);
  // Render only valid previews, so wide objects never draw beyond the scene.
  if (!deleting && fits) object(g, preview, left, top);
  int visibleWidth = b.w;
  if (b.x + visibleWidth > left + 318) visibleWidth = left + 318 - b.x;
  uint16_t highlight = fits ? 0x05a8 : 0xf900;
  g.drawRect(b.x - 1, b.y - 1, visibleWidth + 2, b.h + 2, highlight);
  g.drawRect(b.x - 2, b.y - 2, visibleWidth + 4, b.h + 4, highlight);
  g.setTextColor(Ink); g.setCursor(left + 4, top + 108);
  const char *layers[] = {"Unten", "Platte", "Oben"};
  g.printf("%s %d/12 %s", layers[int(layer)], position + 1,
           savePending ? "Speichern..." : blocked ? "Kein Platz!" : deleting ? "R OK: weg" : moving >= 0 ? "L OK: zurueck" : "L OK: nehmen");
  g.setCursor(left + 4, top + 118); g.print("L: Ort/Ebene  R: Objekt/Farbe/OK");
  g.setCursor(left + 4, top + 128); g.print("L OK 2s halten: zur Spieleauswahl");
}
} // namespace leap
