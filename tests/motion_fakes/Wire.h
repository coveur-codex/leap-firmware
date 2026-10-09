#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
struct FakeWire {
  uint8_t address = 0, reg = 0;
  size_t offset = 0;
  bool registerWritten = false, fail = false;
  std::array<uint8_t, 14> sample{};
  void begin(int, int, int) {}
  void setTimeOut(int) {}
  void beginTransmission(uint8_t value) { address = value; registerWritten = false; }
  void write(uint8_t value) {
    if (!registerWritten) { reg = value; registerWritten = true; }
  }
  int endTransmission(bool = true) { return fail ? 1 : 0; }
  uint8_t requestFrom(uint8_t, uint8_t count) { offset = 0; return fail ? 0 : count; }
  uint8_t read() { return reg == 0x75 ? (address == 0x68 ? 0x68 : 0) : sample[offset++]; }
};
inline FakeWire Wire;
