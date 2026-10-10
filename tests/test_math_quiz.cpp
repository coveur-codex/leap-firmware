#include "MathQuiz.h"
#include <cassert>
#include <iostream>
#include <random>
#include <set>
using namespace leap;
int main() {
  std::mt19937 rng(41);
  for (const std::string op : {"add", "subtract", "multiply"})
    for (int limit : {3, 10, 20, 100, 1000}) {
      int bound = op == "multiply" ? std::min(20, limit) : limit;
      std::set<std::string> generated;
      for (int i = 0; i < 3000; ++i) {
        auto q = generateMathQuestion(op, limit, [&] { return rng(); });
        assert(q.result == (op == "add" ? q.left + q.right :
                           op == "subtract" ? q.left - q.right : q.left * q.right));
        assert(q.left >= 0 && q.right >= 0 && q.left <= bound && q.right <= bound);
        assert(q.result >= 0 && q.result <= (op == "multiply" ? bound * bound : bound));
        if (op == "multiply") assert(q.left > 0 && q.right > 0);
        assert(q.explanation.find("Stellenwerttafel") != std::string::npos);
        generated.insert(q.question);
      }
      assert(generated.size() > 5);
    }
  // A degenerate RNG still terminates at configured boundaries.
  for (const std::string op : {"add", "subtract", "multiply"})
    for (int badLimit : {-1, 0, 1, 100000}) {
      auto q = generateMathQuestion(op, badLimit, [] { return uint32_t(0); });
      assert(q.result >= 0);
    }
  MathAnswerInput input;
  assert(input.value.empty() && !input.correct(0));
  input.left();
  assert(input.digit == 9);
  input.right();
  assert(input.digit == 0);
  input.erase();
  input.append();
  assert(input.value == "0" && input.correct(0));
  input.right();
  input.append();
  input.right();
  input.append();
  assert(input.value == "012" && input.correct(12) && !input.correct(11));
  input.append();
  input.append();
  assert(input.value == "0122"); // Four digits fit the largest result, 1000.
  input.erase();
  assert(input.value == "012");
  input.reset();
  assert(input.value.empty() && input.digit == 0);
  for (int i = 0; i < 10; ++i) {
    assert(input.digit == i);
    input.right();
  }
  assert(input.digit == 0);
  assert(placeValues(1000) == "1 | 0 | 0 | 0");
  assert(placeValues(207) == "0 | 2 | 0 | 7");
  std::cout << "PASS: random math, bounds, numeric input and place values\n";
}
