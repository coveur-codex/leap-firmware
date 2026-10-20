#include "Ui.h"
#include "Network.h"
#include "FinanceDraw.h"
#include <time.h>
namespace leap {
void Ui::drawFinance() {
  leap::drawFinance(*canvas, state["content"]["finance"], financeNavigation,
                    network.connected.load(), network.timeSynced ? int64_t(time(nullptr)) : 0);
}
} // namespace leap
