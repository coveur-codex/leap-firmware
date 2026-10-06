#pragma once
#include "CrabJourney.h"
#include <Arduino_GFX_Library.h>
namespace leap {
void drawCrabJourney(Arduino_GFX &gfx, const CrabJourney &world, int left);
}
