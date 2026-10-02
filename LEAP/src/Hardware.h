#pragma once
#include <stdint.h>
namespace leap::hw {
constexpr int Sck = 12, Mosi = 11, Reset = 13, Dc = 10, Cs = 9, Backlight = 14;
constexpr int LeftKeys[] = {4, 5, 6, 7, 8};
constexpr int RightKeys[] = {2, 15, 16, 21, 47};
constexpr int Bclk = 39, Ws = 40, Din = 41;
constexpr int NativeWidth = 142, NativeHeight = 428, Width = 428, Height = 142;
constexpr int Rotation = 1;
// 142x428 NV3007 panel uses columns 12..153 of its 168-column RAM.
// Verify this panel-specific assumption during the hardware acceptance test.
constexpr int ColumnOffset = 12;
constexpr int SpiHz = 20000000, BacklightHz = 20000;
constexpr int ImuSda = 17, ImuScl = 18;
constexpr bool ImuSwapAxes = false;
constexpr int ImuXSign = 1, ImuYSign = 1; // Match assembly orientation during acceptance.
// GPIO1 ADC remains unused: no confirmed voltage-divider wiring.
} // namespace leap::hw
