#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

// Flat tile buffer — one uint32_t (chtype) per grid cell.
// Written by the game thread via tile_buffer().set().
// Read by Godot's main thread via OmegaGame::get_tile_data().
// Layout: tiles[y * W + x]
struct TileBuffer
{
  // W is wider than MAXWIDTH (64) so text menus and the message window, which
  // the standalone game renders across up to 106 columns, fit without clipping.
  static constexpr int W = 120;
  static constexpr int H = 64; // MAXLENGTH

  std::array<uint32_t, W * H> tiles{};
  mutable std::mutex           mutex;
  std::atomic<bool>            dirty{false};

  // Single-cell write — bounds-checked.
  void set(int x, int y, uint32_t ch)
  {
    if(x < 0 || x >= W || y < 0 || y >= H)
      return;
    std::lock_guard<std::mutex> lk(mutex);
    tiles[y * W + x] = ch;
    dirty.store(true, std::memory_order_release);
  }

  // Fill every cell. One lock acquisition.
  void fill(uint32_t ch)
  {
    std::lock_guard<std::mutex> lk(mutex);
    tiles.fill(ch);
    dirty.store(true, std::memory_order_release);
  }

  // Fill rows [start_y, H) with ch. One lock acquisition.
  void fill_from(int start_y, uint32_t ch)
  {
    if(start_y < 0)
      start_y = 0;
    if(start_y >= H)
      return;
    std::lock_guard<std::mutex> lk(mutex);
    for(int y = start_y; y < H; ++y)
      for(int x = 0; x < W; ++x)
        tiles[y * W + x] = ch;
    dirty.store(true, std::memory_order_release);
  }

  // Write a run of cells on a single row. One lock acquisition per row.
  // Clamps to buffer bounds.
  void write_row(int y, int x_start, const uint32_t *src, int count)
  {
    if(y < 0 || y >= H || x_start >= W || count <= 0)
      return;
    if(x_start < 0) { src -= x_start; count += x_start; x_start = 0; }
    if(x_start + count > W) count = W - x_start;
    if(count <= 0) return;
    std::lock_guard<std::mutex> lk(mutex);
    for(int i = 0; i < count; ++i)
      tiles[y * W + x_start + i] = src[i];
    dirty.store(true, std::memory_order_release);
  }
};

// Defined in scr_godot.cpp.
TileBuffer &tile_buffer();
