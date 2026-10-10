#pragma once
#include "DragonRun.h"
#include <Arduino_GFX_Library.h>
namespace leap {
void drawDragonRun(Arduino_GFX &gfx, const DragonRun &run, int best, bool saving = false);
}
