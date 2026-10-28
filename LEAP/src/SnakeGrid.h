#pragma once
namespace leap {
// Keep the existing game content area below the score and beside the sidebar.
struct SnakeGrid {
  static constexpr int AvailableWidth = 320, AvailableHeight = 104;
  static constexpr int Rows = 16, CellSize = AvailableHeight / Rows;
  static constexpr int Columns = AvailableWidth / CellSize, Capacity = Columns * Rows;
  static constexpr int PixelWidth = Columns * CellSize, PixelHeight = Rows * CellSize;
  static constexpr int LegacyColumns = 40, LegacyRows = 13;
  static constexpr int cellSize(int columns, int rows) {
    return AvailableWidth / columns < AvailableHeight / rows ? AvailableWidth / columns : AvailableHeight / rows;
  }
  static constexpr bool supported(int columns, int rows) {
    return (columns == Columns && rows == Rows) || (columns == LegacyColumns && rows == LegacyRows);
  }
};
static_assert(SnakeGrid::Rows == 16 && SnakeGrid::Columns == 53, "Snake geometry must match the homeserver");
} // namespace leap
