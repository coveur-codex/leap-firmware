#include "FinanceDraw.h"
#include "DisplayText.h"
#include <limits>
namespace leap {
void drawFinance(Arduino_GFX &display, JsonVariantConst data, FinanceNavigation &nav,
                 bool online, int64_t now) {
  auto *canvas = &display;
  auto glyphText = [&](const String &s, int x, int y, int size, uint16_t color) {
    canvas->setCursor(x, y); canvas->setTextSize(size); canvas->setTextColor(color);
    canvas->print(s.c_str());
  };
  auto text = [&](const String &s, int x, int y, int size = 1, uint16_t color = 0xffff) {
    glyphText(displayGlyphs(s.c_str()).c_str(), x, y, size, color);
  };
  auto body = [&](const String &raw, int x, int y, int width, int height) {
    std::string s = displayGlyphs(raw.c_str());
    const size_t cells = size_t(width / 6);
    size_t start = 0;
    for (int yy = y; start < s.size() && yy + 8 <= y + height; yy += 10) {
      size_t end = std::min(start + cells, s.size());
      if (end < s.size()) {
        auto space = s.rfind(' ', end);
        if (space != std::string::npos && space > start) end = space;
      }
      std::string line = s.substr(start, end - start);
      if (yy + 18 > y + height && end < s.size() && !line.empty()) line.back() = '.';
      glyphText(line.c_str(), x, yy, 1, 0xffff);
      start = end;
      while (start < s.size() && s[start] == ' ') ++start;
    }
  };
  constexpr uint16_t muted = 0x9d35, panelColor = 0x18e7;
  constexpr uint16_t green = 0x47f0, red = 0xf34d, background = 0x10e5;
  auto rows = data["securities"].as<JsonArrayConst>();
  size_t count = rows.size();
  nav.normalize(count);
  if (!count) {
    text("Finanzen", 96, 28, 1, 0xffff);
    body("Wertpapiere am Homeserver hinzufügen und Finanzen aktivieren.", 96, 48, 316, 72);
    return;
  }
  auto fit = [&](const String &value, int x, int y, int width, int size, uint16_t color) {
    std::string glyphs = displayGlyphs(value.c_str());
    int cells = width / (6 * size);
    if (int(glyphs.size()) > cells) glyphs = glyphs.substr(0, std::max(0, cells - 1)) + ".";
    glyphText(String(glyphs.c_str()), x, y, size, color);
  };
  auto status = [&](JsonVariantConst q) -> String {
    if (financeStale(q["stale"] | true, online,
                     now,
                     data["generatedAt"] | int64_t(0), q["market"] == "closed")) return "! Veraltet";
    if (q["delayed"] | false) return "Verzögert";
    if (q["market"] == "closed") return "Geschlossen";
    if (q["market"] == "unknown") return "Handel unklar";
    return "";
  };
  auto color = [&](JsonVariantConst q) -> uint16_t {
    if (q["changePercent"].isNull()) return muted;
    double change = q["changePercent"].as<double>();
    return change > 0.00005 ? green : change < -0.00005 ? red : muted;
  };
  auto moon = [&](int x, int y) {
    canvas->fillCircle(x, y, 4, muted);
    canvas->fillCircle(x + 3, y - 2, 4, background);
  };
  auto change = [&](JsonVariantConst q, int x, int y, int width) {
    uint16_t c = color(q);
    double value = q["changePercent"] | 0.0;
    if (!q["changePercent"].isNull() && std::abs(value) > .00005) {
      bool up = value > 0;
      canvas->drawLine(x + 3, y + 1, x + 3, y + 9, c);
      canvas->drawLine(x, y + (up ? 4 : 6), x + 3, y + (up ? 1 : 9), c);
      canvas->drawLine(x + 6, y + (up ? 4 : 6), x + 3, y + (up ? 1 : 9), c);
    } else canvas->drawFastHLine(x, y + 5, 7, c);
    char valueText[32];
    if (q["changePercent"].isNull()) snprintf(valueText, sizeof(valueText), "-- %%");
    else snprintf(valueText, sizeof(valueText), "%+.2f%%", value);
    fit(valueText, x + 11, y, width - 11, 1, c);
  };
  if (!nav.detail) {
    size_t first = nav.page() * 3;
    for (size_t i = first; i < std::min(first + 3, count); ++i) {
      int x = 86 + int(i - first) * 114;
      auto q = rows[i];
      canvas->drawRect(x + 2, 3, 110, 114, i == nav.selected ? 0xffff : panelColor);
      fit(q["displaySymbol"] | (q["symbol"] | ""), x + 7, 10, 99, 1, 0xffff);
      if (q["market"] == "closed") moon(x + 101, 15);
      if (!(q["available"] | false)) {
        text("Kein Kurs", x + 7, 42, 1, muted);
        text("Server prüfen", x + 7, 64, 1, muted);
        continue;
      }
      fit(String(financeNumber(q["price"] | NAN).c_str()), x + 7, 32, 100, 2, 0xffff);
      fit(String(q["currency"] | "") + (q["exchange"].is<const char *>() ? String(" ") + q["exchange"].as<const char *>() : String()), x + 7, 52, 100, 1, muted);
      change(q, x + 7, 70, 98);
      fit(q["quoteTime"] | "", x + 7, 88, 100, 1, muted);
      fit(status(q), x + 7, 102, 100, 1, muted);
    }
    text(String(int(nav.page() + 1)) + "/" + String(int(FinanceNavigation::pages(count))), 391, 121, 1, muted);
    text("Mitte: Details", 94, 121, 1, muted);
    return;
  }
  auto q = rows[nav.detailSelected];
  fit(q["displaySymbol"] | (q["symbol"] | ""), 94, 6, 145, 1, 0xffff);
  if (q["market"] == "closed") moon(246, 10);
  if (!(q["available"] | false)) {
    body(q["error"] | "Kein Kurs verfügbar", 94, 30, 320, 90);
    return;
  }
  fit(String(financeNumber(q["price"] | NAN).c_str()), 94, 23, 158, 2, 0xffff);
  fit(String(q["currency"] | "") + (q["exchange"].is<const char *>() ? String(" ") + q["exchange"].as<const char *>() : String()), 94, 42, 158, 1, muted);
  change(q, 94, 55, 155);
  fit(String(q["quoteTime"] | "") + " " + String(q["timezone"] | ""), 265, 6, 155, 1, muted);
  fit(status(q), 265, 19, 155, 1, muted);
  // Four 26-character lines; plain firmware text uses the normal LEAP font.
  body(q["news"]["title"] | "Keine Nachricht verfügbar", 94, 72, 157, 40);
  fit(q["news"]["source"] | "", 94, 112, 157, 1, muted);
  fit(String(q["news"]["time"] | "") + ((q["newsStale"] | false) ? " !" : ""), 94, 122, 157, 1, muted);
  canvas->drawLine(257, 30, 257, 126, panelColor);
  auto points = q["points"].as<JsonArrayConst>();
  double lo = std::numeric_limits<double>::infinity(), hi = -lo;
  int64_t first = INT64_MAX, last = 0;
  for (JsonArrayConst p : points) {
    double v = p[1] | NAN;
    int64_t t = p[0] | int64_t(0);
    if (!std::isfinite(v) || v <= 0 || t <= 0) continue;
    lo = std::min(lo, v); hi = std::max(hi, v);
    first = std::min(first, t); last = std::max(last, t);
  }
  fit(String("H ") + financeNumber(q["high"] | NAN).c_str(), 265, 34, 155, 1, muted);
  fit(String("T ") + financeNumber(q["low"] | NAN).c_str(), 265, 112, 155, 1, muted);
  if (!last) { text("Keine Intradaydaten", 265, 74, 1, muted); return; }
  double padding = hi > lo ? (hi - lo) * .08 : std::max(.01, hi * .001);
  lo -= padding; hi += padding;
  for (int y : {49, 77, 105}) canvas->drawFastHLine(265, y, 154, panelColor);
  int px = 0, py = 0;
  int64_t previousTime = 0;
  int previousSegment = -1;
  for (JsonArrayConst p : points) {
    int64_t t = p[0] | int64_t(0); double v = p[1] | NAN;
    if (!std::isfinite(v) || v <= 0 || t <= 0) { previousTime = 0; continue; }
    int x = financeChartX(t, first, last, 265, 154);
    int y = financeChartY(v, lo, hi, 49, 57);
    int segment = p.size() >= 3 ? (p[2] | 0) : 0;
    bool connectedSegment = p.size() >= 3 ? segment == previousSegment : t - previousTime <= (q["gapSeconds"] | 900);
    if (previousTime && t > previousTime && connectedSegment) canvas->drawLine(px, py, x, y, color(q));
    canvas->fillCircle(x, y, 1, color(q));
    px = x; py = y; previousTime = t; previousSegment = segment;
  }
  fit(q["chartMode"] == "collected" ? "Gesammelte Kurse" : "Intraday", 265, 123, 155, 1, muted);
}
} // namespace leap
