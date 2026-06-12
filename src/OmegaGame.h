#pragma once

// clang-format off
// Include order is load-bearing here: godot-cpp's generated headers declare
// enum members (KEY_BACKSPACE, KEY_UP, ...) whose names are also curses
// macros defined via defs.h -> curses_stub.h. All godot headers must be fully
// parsed before defs.h, or the macros mangle godot's enum declarations.
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "defs.h"
#include "game_session.h"
#include "input_queue.hpp"

#include <atomic>
#include <thread>
// clang-format on

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

  // Values returned by get_terrain_data() for dungeon/city/interior levels.
  // Each is the full chtype from defs.h (ASCII glyph in bits 0-7, color pair
  // in bits 24-31). Members carry a TERRAIN_ prefix because the bare names
  // (WALL, FLOOR, ...) are preprocessor macros in defs.h.
  enum LevelTerrain
  {
    TERRAIN_SPACE       = SPACE,
    TERRAIN_WALL        = WALL,
    TERRAIN_PORTCULLIS  = PORTCULLIS,
    TERRAIN_OPEN_DOOR   = OPEN_DOOR,
    TERRAIN_CLOSED_DOOR = CLOSED_DOOR,
    TERRAIN_WHIRLWIND   = WHIRLWIND,
    TERRAIN_ABYSS       = ABYSS,
    TERRAIN_VOID        = VOID_CHAR, // alias of TERRAIN_SPACE — same chtype
    TERRAIN_LAVA        = LAVA,
    TERRAIN_HEDGE       = HEDGE,
    TERRAIN_WATER       = WATER,
    TERRAIN_FIRE        = FIRE,
    TERRAIN_TRAP        = TRAP, // revealed traps only; hidden traps look like FLOOR
    TERRAIN_LIFT        = LIFT,
    TERRAIN_STAIRS_UP   = STAIRS_UP,
    TERRAIN_STAIRS_DOWN = STAIRS_DOWN,
    TERRAIN_FLOOR       = FLOOR,
    TERRAIN_STATUE      = STATUE,
    TERRAIN_RUBBLE      = RUBBLE,
    TERRAIN_ALTAR       = ALTAR, // deity id in get_terrain_aux_data()
    TERRAIN_CHAIR       = CHAIR,
    TERRAIN_SAFE        = SAFE,
    TERRAIN_FURNITURE   = FURNITURE,
    TERRAIN_BED         = BED,
  };

  // Values returned by get_terrain_data() in the countryside (overworld).
  enum CountryTerrain
  {
    COUNTRY_PLAINS     = PLAINS,
    COUNTRY_TUNDRA     = TUNDRA,
    COUNTRY_ROAD       = ROAD,
    COUNTRY_MOUNTAINS  = MOUNTAINS,
    COUNTRY_PASS       = PASS,
    COUNTRY_RIVER      = RIVER,
    COUNTRY_CITY       = CITY,
    COUNTRY_VILLAGE    = VILLAGE,
    COUNTRY_FOREST     = FOREST,
    COUNTRY_JUNGLE     = JUNGLE,
    COUNTRY_SWAMP      = SWAMP,
    COUNTRY_VOLCANO    = VOLCANO,
    COUNTRY_CASTLE     = CASTLE,
    COUNTRY_TEMPLE     = TEMPLE,
    COUNTRY_CAVES      = CAVES,
    COUNTRY_DESERT     = DESERT,
    COUNTRY_CHAOS_SEA  = CHAOS_SEA,
    COUNTRY_STARPEAK   = STARPEAK,
    COUNTRY_DRAGONLAIR = DRAGONLAIR,
    COUNTRY_MAGIC_ISLE = MAGIC_ISLE,
  };

  // Item-class glyphs returned by get_item_data(). ITEM_PILE means more than
  // one item on the cell. Individual items of a class share one glyph.
  enum ItemGlyph
  {
    ITEM_CORPSE         = CORPSE,
    ITEM_CASH           = CASH,
    ITEM_PILE           = PILE,
    ITEM_FOOD           = FOOD,
    ITEM_WEAPON         = WEAPON,
    ITEM_MISSILE_WEAPON = MISSILEWEAPON,
    ITEM_SCROLL         = SCROLL,
    ITEM_POTION         = POTION,
    ITEM_ARMOR          = ARMOR,
    ITEM_SHIELD         = SHIELD,
    ITEM_CLOAK          = CLOAK,
    ITEM_BOOTS          = BOOTS,
    ITEM_STICK          = STICK,
    ITEM_RING           = RING,
    ITEM_THING          = THING,
    ITEM_ARTIFACT       = ARTIFACT,
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

VARIANT_ENUM_CAST(godot::OmegaGame::LevelTerrain);
VARIANT_ENUM_CAST(godot::OmegaGame::CountryTerrain);
VARIANT_ENUM_CAST(godot::OmegaGame::ItemGlyph);
