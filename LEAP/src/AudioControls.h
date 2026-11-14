#pragma once
#include <algorithm>
namespace leap {
struct AudioControls {
  static constexpr int MaxVolume=60, Step=5;
  // Key ordinals: Up, Down, Left, Right, Center. Navigation remains in Ui.
  static int volumeDelta(bool right,int key) { return right ? 0 : key==0 ? Step : key==1 ? -Step : 0; }
  static int stationDelta(bool right,int key) { return !right ? 0 : key==0 ? 1 : key==1 ? -1 : 0; }
  static int adjustVolume(int value,int delta) { return std::max(0,std::min(MaxVolume,value+delta)); }
};
} // namespace leap
