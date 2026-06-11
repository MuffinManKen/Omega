#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

// Right-side HUD tile buffer — 55 wide × 64 tall.
// Covers screen columns 65–119 (MAXWIDTH+1 .. TileBuffer::W-1).
// Written by the game thread via panel functions (dataprint, timeprint, etc.).
// Read by Godot's main thread via OmegaGame::get_panel_data().
// Layout: tiles[y * W + x], with x=0 == screen column 65.
//
// Intentionally separate from TileBuffer so show_screen() never wipes it.
// When the panel is eventually replaced by Godot UI nodes, this struct is
// simply removed and the panel functions in scr_godot.cpp are stubbed again.
struct PanelBuffer
{
  static constexpr int W = 55; // TileBuffer::W - (MAXWIDTH + 1), columns 65-119
  static constexpr int H = 64; // MAXLENGTH

  std::array<uint32_t, W * H> tiles{};
  mutable std::mutex           mutex;
  std::atomic<bool>            dirty{false};

  void set(int x, int y, uint32_t ch)
  {
    if(x < 0 || x >= W || y < 0 || y >= H)
      return;
    std::lock_guard<std::mutex> lk(mutex);
    tiles[y * W + x] = ch;
    dirty.store(true, std::memory_order_release);
  }

  void fill(uint32_t ch)
  {
    std::lock_guard<std::mutex> lk(mutex);
    tiles.fill(ch);
    dirty.store(true, std::memory_order_release);
  }

  void write_row(int y, int x_start, const uint32_t *src, int count)
  {
    if(y < 0 || y >= H || x_start >= W || count <= 0)
      return;
    if(x_start < 0) { src -= x_start; count += x_start; x_start = 0; }
    if(x_start + count > W) count = W - x_start;
    if(count <= 0)
      return;
    std::lock_guard<std::mutex> lk(mutex);
    for(int i = 0; i < count; ++i)
      tiles[y * W + x_start + i] = src[i];
    dirty.store(true, std::memory_order_release);
  }
};

// Defined in scr_godot.cpp.
PanelBuffer &panel_buffer();
