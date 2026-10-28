#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace leap {
// Bounded streaming identity check. This detects mislabeled builds, not malicious
// firmware; SHA-256, ESP image validation and the trusted server are still required.
class FirmwareIdentity {
  inline static constexpr char Prefix[] = "LEAP_APP_VERSION_V1=";
  char candidate[41]{}, version[41]{};
  size_t prefix = 0, used = 0;
  bool collecting = false, terminator = false, found = false, conflict = false;
  void search(uint8_t c) {
    prefix = c == uint8_t(Prefix[prefix]) ? prefix + 1 : c == uint8_t(Prefix[0]) ? 1 : 0;
    if (prefix == sizeof(Prefix)-1) { prefix=0; used=0; collecting=true; }
  }
public:
  void append(const uint8_t *data, size_t size) {
    for (size_t i=0;i<size;++i) {
      uint8_t c=data[i];
      if (terminator) {
        terminator=false;
        if (c==0) {
          candidate[used]=0;
          if (found && std::strcmp(version,candidate)) conflict=true;
          if (!found) std::strcpy(version,candidate);
          found=true;
        } else search(c);
      } else if (collecting) {
        if (c==';' && used) { collecting=false; terminator=true; }
        else if (((c>='0' && c<='9') || (c>='a' && c<='z') || c=='.' || c=='-') && used<40)
          candidate[used++]=char(c);
        else { collecting=false; search(c); }
      } else search(c);
    }
  }
  const char *imageVersion() const { return found && !conflict ? version : ""; }
  bool matches(const char *expected) const {
    return expected && expected[0] && found && !conflict && !std::strcmp(expected,version);
  }
};
} // namespace leap
