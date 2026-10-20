#include "Finance.h"
#include <cassert>
#include <limits>
using namespace leap;
int main() {
  for (size_t count : {size_t(0),size_t(1),size_t(2),size_t(3),size_t(4),size_t(6),size_t(7)}) {
    FinanceNavigation nav; nav.enter(0);
    assert(!nav.tick(count, 9999));
    assert(nav.tick(count, 10000) == (count > 3));
    assert(nav.selected == (count > 3 ? 3u : 0u));
    if (!count) { nav.center(count, 10000); assert(!nav.detail); continue; }
    nav.selected = count - 1;
    nav.move(1, count, 12000); assert(nav.selected == 0);
    nav.move(-1, count, 13000); assert(nav.selected == count - 1);
    assert(!nav.tick(count, 43000)); // pause elapsed, next normal period begins
    assert(!nav.tick(count, 52999));
    assert(nav.tick(count, 53000) == (count > 3));
    nav.selected = count - 1;
    nav.center(count, 54000); assert(nav.detail && nav.detailSelected == count - 1);
    assert(!nav.tick(count, 1000000));
    nav.move(1, count, 55000); assert(nav.detailSelected == 0);
    nav.move(-1, count, 56000); assert(nav.detailSelected == count - 1);
    nav.back(57000); assert(!nav.detail && nav.selected == count - 1);
    nav.normalize(1); assert(nav.selected == 0 && nav.detailSelected == 0);
  }
  FinanceNavigation nav; nav.enter(UINT32_MAX - 5000);
  assert(!nav.tick(7, 4998)); assert(nav.tick(7, 4999));
  nav.selected = 6; nav.enter(0); nav.tick(7, 10000); assert(nav.selected == 0);
  assert(financeStale(false, false, 100, 100, false));
  assert(!financeStale(false, true, 1000, 500, false));
  assert(financeStale(false, true, 1200, 500, false));
  assert(!financeStale(false, true, 1200, 500, true));
  assert(financeStale(true, true, 1000, 999, true));
  assert(financeNumber(std::numeric_limits<double>::quiet_NaN()) == "--");
  for (double v : {0.00001, 1.0, 99999.99, 999999.9, 1e6, 1e9}) assert(financeNumber(v).size() <= 8);
  assert(financeChartX(150,100,300,0,101) == 25);
  assert(financeChartX(100,100,100,0,100) == 50);
  assert(financeChartY(5,5,5,0,100) == 50);
  assert(financeChartY(10,0,10,0,100) == 0);
}
