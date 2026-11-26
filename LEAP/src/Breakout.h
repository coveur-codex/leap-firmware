#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace leap {
class Breakout {
public:
  enum class Phase { Ready, Playing, Paused, Won, Lost };
  enum class Event { None, Hit, Miss, Cleared };
  static constexpr int Width = 322, Height = 94;
  static constexpr int Rows = 4, Columns = 12;
  static constexpr int BrickX = 17, BrickY = 5, BrickW = 22, BrickH = 7;
  static constexpr int BrickPitchX = 24, BrickPitchY = 9;
  static constexpr int PaddleY = Height - 9, PaddleH = 4, PaddleHalf = 21, BallRadius = 2;
  static constexpr int LevelCount = 3;

  void open(uint32_t now) {
    score_ = level_ = 0;
    lives_ = 3;
    steppedAt_ = now;
    loadLevel();
  }
  Phase phase() const { return phase_; }
  int score() const { return score_; }
  int level() const { return level_ + 1; }
  int lives() const { return lives_; }
  int paddleX() const { return int(paddleX_); }
  int ballX() const { return int(ballX_); }
  int ballY() const { return int(ballY_); }
  bool brick(int row, int col) const { return bricks_[row] & (1u << col); }

  void confirm(uint32_t now) {
    steppedAt_ = now;
    switch (phase_) {
    case Phase::Ready:
      vx_ = speed() * (level_ % 2 ? -0.55f : 0.55f);
      vy_ = -std::sqrt(speed() * speed() - vx_ * vx_);
      phase_ = Phase::Playing;
      break;
    case Phase::Playing:
      phase_ = Phase::Paused;
      break;
    case Phase::Paused:
      phase_ = Phase::Playing;
      break;
    case Phase::Lost:
    case Phase::Won:
      open(now);
      break;
    }
  }

  void suspend() {
    if (phase_ == Phase::Playing)
      phase_ = Phase::Paused;
  }

  Event tick(uint32_t now, int direction) {
    if (phase_ != Phase::Playing && phase_ != Phase::Ready) {
      steppedAt_ = now;
      return Event::None;
    }
    if (uint32_t(now - steppedAt_) > 64)
      steppedAt_ = now - 64;
    Event event = Event::None;
    while (uint32_t(now - steppedAt_) >= StepMs) {
      steppedAt_ += StepMs;
      event = std::max(event, step(direction));
      if (phase_ == Phase::Won || phase_ == Phase::Lost)
        break;
    }
    return event;
  }

private:
  inline static constexpr uint16_t Levels[LevelCount][Rows] = {
      {0b111111111111, 0b111111111111, 0b011111111110, 0b001111111100},
      {0b110000000011, 0b111000000111, 0b011110011110, 0b001111111100},
      {0b111100001111, 0b101111111101, 0b011111111110, 0b110011110011},
  };
  static constexpr uint32_t StepMs = 8;
  static constexpr float Dt = StepMs / 1000.0f;
  Phase phase_ = Phase::Ready;
  uint16_t bricks_[Rows]{};
  int score_ = 0, level_ = 0, lives_ = 3;
  float paddleX_ = 0, ballX_ = 0, ballY_ = 0, vx_ = 0, vy_ = 0;
  uint32_t steppedAt_ = 0;

  float speed() const { return 132.0f + level_ * 24.0f; }
  void resetBall() {
    ballX_ = paddleX_ = Width / 2.0f;
    ballY_ = PaddleY - BallRadius - 2;
    phase_ = Phase::Ready;
  }
  void loadLevel() {
    std::copy_n(Levels[level_], Rows, bricks_);
    resetBall();
  }
  Event hitBrick(float x, float y) {
    for (int row = 0; row < Rows; ++row) {
      int top = BrickY + row * BrickPitchY;
      if (!bricks_[row] || y + BallRadius <= top || y - BallRadius >= top + BrickH)
        continue;
      for (int col = 0; col < Columns; ++col) {
        int left = BrickX + col * BrickPitchX;
        if (!brick(row, col) || x + BallRadius <= left || x - BallRadius >= left + BrickW)
          continue;
        bricks_[row] &= ~(1u << col);
        score_ += 10;
        if (std::any_of(bricks_, bricks_ + Rows, [](uint16_t row) { return row != 0; }))
          return Event::Hit;
        if (level_ + 1 == LevelCount)
          phase_ = Phase::Won;
        else {
          ++level_;
          loadLevel();
        }
        return Event::Cleared;
      }
    }
    return Event::None;
  }

  Event step(int direction) {
    paddleX_ = std::clamp(paddleX_ + direction * 220.0f * Dt, float(1 + PaddleHalf),
                          float(Width - 1 - PaddleHalf));
    if (phase_ == Phase::Ready) {
      ballX_ = paddleX_;
      return Event::None;
    }
    // Resolve each axis separately at a fixed step smaller than a brick.
    Event hit = Event::None;
    float x = ballX_ + vx_ * Dt;
    constexpr float left = 1 + BallRadius, right = Width - 2 - BallRadius;
    if (x < left || x > right) {
      ballX_ = std::clamp(x, left, right);
      vx_ = -vx_;
    } else {
      hit = hitBrick(x, ballY_);
      if (hit == Event::Cleared)
        return hit;
      if (hit == Event::Hit)
        vx_ = -vx_;
      else
        ballX_ = x;
    }
    float y = ballY_ + vy_ * Dt;
    constexpr float contact = PaddleY - BallRadius;
    if (vy_ > 0 && ballY_ <= contact && y >= contact &&
        std::abs(ballX_ - paddleX_) <= PaddleHalf + BallRadius) {
      float offset = (ballX_ - paddleX_) / (PaddleHalf + BallRadius);
      vx_ = speed() * 0.8f * offset;
      // Centre hits still travel sideways instead of repeating the same path.
      if (std::abs(vx_) < speed() * 0.15f)
        vx_ = std::copysign(speed() * 0.15f, vx_);
      vy_ = -std::sqrt(speed() * speed() - vx_ * vx_);
      ballY_ = contact;
      return Event::Hit;
    }
    if (y < 1 + BallRadius) {
      ballY_ = 1 + BallRadius;
      vy_ = -vy_;
      return hit;
    }
    if (y - BallRadius >= Height) {
      if (--lives_)
        resetBall();
      else
        phase_ = Phase::Lost;
      return Event::Miss;
    }
    Event verticalHit = hitBrick(ballX_, y);
    if (verticalHit == Event::Hit)
      vy_ = -vy_;
    else if (verticalHit == Event::None)
      ballY_ = y;
    return std::max(hit, verticalHit);
  }
};
} // namespace leap
