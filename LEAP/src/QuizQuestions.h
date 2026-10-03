#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>
namespace leap {
// Reservoir sampling keeps the 200-question memory limit without favouring the
// first assigned catalog. Call only for age-eligible questions.
inline void collectQuizQuestion(JsonArray pool, JsonVariantConst question, uint32_t &seen,
                                uint32_t random, size_t limit = 200) {
  ++seen;
  if (pool.size() < limit)
    pool.add(question);
  else {
    size_t slot = random % seen;
    if (slot < limit)
      pool[slot].set(question);
  }
}
// A null catalog ID means rows already belong to one versioned package (or an
// older server's single legacy pool). Never mix IDs when using the new API.
template <class Random>
void collectQuizCatalog(JsonArray pool, JsonArrayConst rows, int age,
                        JsonVariantConst catalogId, Random random) {
  uint32_t seen = 0;
  for (JsonObjectConst q : rows)
    if ((q["minAge"] | 0) <= age &&
        (catalogId.isNull() || q["catalogId"] == catalogId))
      collectQuizQuestion(pool, q, seen, random());
}
template <class Random>
void shuffleQuizQuestions(std::vector<uint16_t> &order, size_t count, Random random) {
  order.resize(count);
  std::iota(order.begin(), order.end(), uint16_t(0));
  for (size_t i = count; i > 1; --i)
    std::swap(order[i - 1], order[random() % i]);
}
} // namespace leap
