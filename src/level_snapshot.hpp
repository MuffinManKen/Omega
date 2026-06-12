#pragma once

#include "defs.h"

#include <array>
#include <atomic>
#include <mutex>

// Per-cell structured snapshot of the game world, written by the game thread
// and read by Godot's main thread. Preserves all three render layers (terrain,
// item, creature) so Godot can draw them independently rather than receiving
// the single collapsed chtype that putspot() produces.
struct CellSnapshot
{
  static constexpr uint8_t FLAG_SEEN   = 0x01; // player has visited this cell
  static constexpr uint8_t FLAG_IN_LOS = 0x02; // cell is currently visible

  chtype  terrain_char  = 0; // site[x][y].locchar (WALL for SECRET cells)
  int     terrain_aux   = 0; // site[x][y].aux — meaning depends on the site type: deity id for
                             // altars, lock state for closed doors (UNLOCKED=0/BURGLED=2/LOCKED=3),
                             // destination depth for stairs. For most cells it is leftover noise:
                             // clear_level() defaults every cell to difficulty()*20 and city sites
                             // set it to 1. Renderers should ignore it except for known site types.
  chtype  item_char     = 0; // top item objchar, PILE if >1, 0 if none/hidden
  chtype  creature_char = 0; // creature->monchar if visible, 0 otherwise
  uint8_t flags         = 0;
};

// 64×64 grid of CellSnapshots. Written in bulk by snapshot_rebuild() on the
// game thread; read under mutex by OmegaGame::get_level_snapshot().
struct LevelSnapshot
{
  static constexpr int W = MAXWIDTH;   // 64
  static constexpr int H = MAXLENGTH;  // 64

  std::array<CellSnapshot, W * H> cells{};
  mutable std::mutex               mutex;
  std::atomic<bool>                dirty{false};

  // Replace the entire buffer atomically. Caller builds src without the lock.
  void rebuild(const std::array<CellSnapshot, W * H> &src)
  {
    std::lock_guard<std::mutex> lk(mutex);
    cells = src;
    dirty.store(true, std::memory_order_release);
  }
};

// Singleton defined in scr_godot.cpp.
LevelSnapshot &level_snapshot();
