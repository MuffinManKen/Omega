#pragma once

#include "game_session.h"
#include "input_queue.hpp"

#include <atomic>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <thread>

namespace godot
{

class OmegaGame : public Node
{
  GDCLASS(OmegaGame, Node)

public:
  enum PhaseEnum
  {
    PHASE_STARTING  = 0, // game thread not yet running
    PHASE_MENU      = 1, // title / character-creation menus
    PHASE_RUNNING   = 2, // main game loop active
    PHASE_GAME_OVER = 3, // run_game_loop returned normally
  };

  OmegaGame()  = default;
  ~OmegaGame() = default;

  void _ready() override;
  void _notification(int p_what);

  // Game phase — read from any thread.
  int get_game_phase();

  // Input: called from GDScript on each keypress.
  void provide_input(int key);

  // Render data: called from GDScript each frame.
  PackedInt32Array get_tile_data(); // W×H flat array of chtype values
  bool is_dirty();                  // true if tiles changed since last get_tile_data()
  Vector2i get_player_pos();        // player grid position for camera centering
  int get_buffer_width();           // TileBuffer::W (currently 120)
  int get_buffer_height();          // TileBuffer::H (currently 64)

  // Right-side HUD panel — separate buffer, never cleared by show_screen().
  // Only populated during PHASE_RUNNING; during PHASE_MENU use the tile buffer.
  PackedInt32Array get_panel_data(); // W×H flat array of chtype values
  bool is_panel_dirty();             // true if panel changed since last get_panel_data()
  int get_panel_width();             // PanelBuffer::W (55)
  int get_panel_height();            // PanelBuffer::H (64)

  // Full player state snapshot — returns all scalar fields plus status/rank/immunity
  // arrays as a Dictionary.  Possession and pack contents are counts only;
  // full item data needs a separate API.
  Dictionary get_player();

  // Game-world clock and calendar as a Dictionary:
  //   "clock"  String — "10:30 AM", same format as the curses HUD timeprint()
  //   "hour"   int    — 0-23
  //   "minute" int    — 0-59 (the HUD clock rounds down to 10-minute steps)
  //   "day"    int    — 1-30 (day of month)
  //   "month"  String — month name ("Freeze", "Ice", ...)
  //   "time"   int64  — raw Time counter (minutes)
  //   "date"   int    — raw Date counter (days elapsed)
  Dictionary get_time();

  // Structured per-cell snapshot — one array per layer, indexed [y * W + x].
  // is_snapshot_dirty() is test-and-clear: returns true once per update.
  bool is_snapshot_dirty();
  int get_snapshot_width();                // 64
  int get_snapshot_height();               // 64
  PackedInt32Array get_terrain_data();     // terrain_char (chtype) per cell
  PackedInt32Array get_terrain_aux_data(); // terrain_aux (deity id, etc.) per cell
  PackedInt32Array get_item_data();        // top item objchar, PILE if >1, 0 if none
  PackedInt32Array get_creature_data();    // creature monchar if visible, 0 otherwise
  PackedByteArray get_cell_flags();        // FLAG_SEEN | FLAG_IN_LOS per cell

protected:
  static void _bind_methods();

private:
  std::thread game_thread_;
};

} // namespace godot
