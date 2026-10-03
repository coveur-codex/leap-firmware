#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace leap {
struct MathQuestion {
  int left, right, result;
  std::array<int, 4> answers;
  std::string question, explanation;
};
inline std::string placeValues(int value) {
  return std::to_string(value / 1000) + " | " + std::to_string(value / 100 % 10) +
         " | " + std::to_string(value / 10 % 10) + " | " + std::to_string(value % 10);
}
template <class Random>
MathQuestion generateMathQuestion(const std::string &operation, int limit, Random random) {
  bool multiply = operation == "multiply", subtract = operation == "subtract";
  limit = std::max(3, std::min(limit, multiply ? 20 : 1000));
  MathQuestion q{};
  if (multiply) {
    q.left = 1 + random() % limit;
    q.right = 1 + random() % limit;
    q.result = q.left * q.right;
  } else {
    int high = random() % (limit + 1);
    int low = random() % (high + 1);
    q.left = subtract ? high : low;
    q.right = subtract ? low : high - low;
    q.result = subtract ? high - low : high;
  }
  auto number = [](int n) { return std::to_string(n); };
  std::string symbol = multiply ? " x " : subtract ? " - " : " + ";
  q.question = number(q.left) + symbol + number(q.right) + " = ?";
  q.explanation = number(q.left) + symbol + number(q.right) + " = " + number(q.result) + "\n";
  if (multiply) {
    q.explanation += number(q.right) + " Gruppen mit je " + number(q.left) + ".\n";
    for (int i = 0; i < q.right; ++i)
      q.explanation += (i ? " + " : "") + number(q.left);
    q.explanation += " = " + number(q.result);
  } else {
    int tens = q.right / 10 * 10, ones = q.right % 10;
    int intermediate = q.left + (subtract ? -tens : tens);
    q.explanation += "Zuerst " + number(tens) + (subtract ? " abziehen" : " dazu") + ": " +
                     number(q.left) + symbol + number(tens) + " = " + number(intermediate) +
                     "\nDann " + number(ones) + (subtract ? " abziehen" : " dazu") + ": " +
                     number(intermediate) + symbol + number(ones) + " = " + number(q.result);
  }
  q.explanation += "\nStellenwerttafel:\nT=Tausender, H=Hunderter\nZ=Zehner, E=Einer\n    T | H | Z | E\nA:  " + placeValues(q.left) +
                   "\nB:  " + placeValues(q.right) + "\n=:  " + placeValues(q.result);
  // Four unique non-negative choices, all in the configured result range.
  int maximum = multiply ? limit * limit : limit;
  std::vector<int> distractors;
  for (int candidate = std::max(0, q.result - 10);
       candidate <= std::min(maximum, q.result + 10); ++candidate)
    if (candidate != q.result) distractors.push_back(candidate);
  q.answers[0] = q.result;
  for (int i = 1; i < 4; ++i) {
    size_t index = random() % distractors.size();
    q.answers[i] = distractors[index];
    distractors.erase(distractors.begin() + index);
  }
  return q;
}
} // namespace leap
