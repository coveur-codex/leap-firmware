#include "FinanceDraw.h"
#include <cassert>
using namespace leap;
int main() {
  Arduino_GFX gfx;
  gfx.recordAll = gfx.recordTextBounds = true;
  JsonDocument data;
  FinanceNavigation nav;
  auto verify = [&]() {
    for (const auto &r : gfx.rects) {
      assert(r.x >= 86 && r.y >= 0);
      assert(r.x + r.w <= 428 && r.y + r.h <= 132);
    }
    gfx.rects.clear(); gfx.text.clear();
  };
  data["generatedAt"] = 1000;
  drawFinance(gfx, data, nav, true, 1000); verify();
  for (size_t count : {size_t(1),size_t(2),size_t(3),size_t(4),size_t(6),size_t(7),size_t(48)}) {
    auto rows = data["securities"].to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
      auto q = rows.add<JsonObject>();
      q["symbol"] = "VERY-LONG-INDEX-SYMBOL"; q["currency"] = "USD";
      q["available"] = i % 7 != 6; q["stale"] = false;
      q["price"] = i%2 ? 999999999999.99 : 12345.67;
      q["high"] = 999999999999.99; q["low"] = .00001;
      q["changePercent"] = int(i%3)-1;
      q["market"] = i%2 ? "closed" : "open";
      q["quoteTime"] = "20.10. 15:30"; q["timezone"] = "America/New_York";
      q["news"]["title"] = "Eine lange Überschrift über ein Unternehmen mit außerordentlich umfangreichem Namen und einemsehrlangenwortdasumbrechenmuss";
      q["news"]["source"] = "International Financial News Source";
      q["news"]["time"] = "20.10. 15:25";
      for (int j=0; j<60; ++j) {
        auto p=q["points"].add<JsonArray>();p.add(100+j*300);p.add(20.0+(j%3));p.add(j>30?1:0);
      }
    }
    for (size_t i=0;i<count;++i) {
      nav.selected=i; nav.detailSelected=i;
      for (bool detail : {false,true}) for (bool online : {false,true}) {
        nav.detail=detail;drawFinance(gfx,data,nav,online,1000);verify();
      }
    }
  }
  auto q=data["securities"][0];nav.detail=true;nav.detailSelected=0;
  for (int count : {0,1,2}) {
    auto points=q["points"].to<JsonArray>();
    for(int i=0;i<count;++i) {auto p=points.add<JsonArray>();p.add(i+1);p.add(5);}
    drawFinance(gfx,data,nav,true,1000);verify();
  }
}
