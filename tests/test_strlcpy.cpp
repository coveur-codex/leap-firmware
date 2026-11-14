#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
  char out[8];
  memset(out, '#', sizeof(out));
  assert(strlcpy(out, "Hallo", sizeof(out)) == 5);
  assert(!strcmp(out, "Hallo") && out[6] == '#');
  memset(out, '#', sizeof(out));
  assert(strlcpy(out, "Hallo", 3) == 5);
  assert(!strcmp(out, "Ha") && out[3] == '#');
  memset(out, '#', sizeof(out));
  assert(strlcpy(out, "Hallo", 0) == 5);
  for (char value : out) assert(value == '#');
  assert(strlcpy(out, "Hallo", 1) == 5 && out[0] == '\0' && out[1] == '#');
  assert(strlcpy(out, "", sizeof(out)) == 0 && out[0] == '\0');
  puts("PASS: fortified Arduino strlcpy compatibility, truncation, zero capacity and termination");
}
