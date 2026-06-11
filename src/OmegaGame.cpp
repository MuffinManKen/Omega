#include "OmegaGame.h"

#include "curses_stub.h"
#include "glob.h"
#include "level_snapshot.hpp"
#include "panel_buffer.hpp"
#include "render_buffer.hpp"
#include "scr.h"

#include <atomic>
#include <format>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <mutex>
#include <string>

namespace godot
{

static std::atomic<int> s_phase{OmegaGame::PHASE_STARTING};

// Godot 4 Key enum values (from godot_cpp/classes/global_constants.hpp).
namespace GodotKey
{
  constexpr int ESCAPE    = 4194305;
  constexpr int TAB       = 4194306;
  constexpr int BACKSPACE = 4194308;
  constexpr int ENTER     = 4194309;
  constexpr int KP_ENTER  = 4194310;
  constexpr int DELETE_K  = 4194312;
  constexpr int HOME      = 4194317;
  constexpr int END       = 4194318;
  constexpr int LEFT      = 4194319;
  constexpr int UP        = 4194320;
  constexpr int RIGHT     = 4194321;
  constexpr int DOWN      = 4194322;
  constexpr int PAGEUP    = 4194323;
  constexpr int PAGEDOWN  = 4194324;
} // namespace GodotKey

static int godot_to_pdcurses(int key)
{
  if(key >= 1 && key < 127)
    return key;

  switch(key)
  {
    case GodotKey::ESCAPE:
      return 27;
    case GodotKey::TAB:
      return '\t';
    case GodotKey::BACKSPACE:
      return KEY_BACKSPACE;
    case GodotKey::DELETE_K:
      return KEY_DC;
    case GodotKey::ENTER:
      return '\n';
    case GodotKey::KP_ENTER:
      return '\n';
    case GodotKey::UP:
      return KEY_UP;
    case GodotKey::DOWN:
      return KEY_DOWN;
    case GodotKey::LEFT:
      return KEY_LEFT;
    case GodotKey::RIGHT:
      return KEY_RIGHT;
    case GodotKey::HOME:
      return KEY_HOME;
    case GodotKey::END:
      return KEY_END;
    case GodotKey::PAGEUP:
      return KEY_PPAGE;
    case GodotKey::PAGEDOWN:
      return KEY_NPAGE;
    default:
      return key;
  }
}

void OmegaGame::_ready()
{
  static std::string lib_path =
    std::string(ProjectSettings::get_singleton()->globalize_path("res://lib/").utf8().get_data());
  Omegalib = lib_path.c_str();

  game_thread_ = std::thread(
    []()
    {
      try
      {
        s_phase.store(PHASE_MENU, std::memory_order_release);
        const InitResult result = init_game_session();
        if(result != InitResult::Failed)
        {
          s_phase.store(PHASE_RUNNING, std::memory_order_release);
          run_game_loop(result == InitResult::NewGame);
          s_phase.store(PHASE_GAME_OVER, std::memory_order_release);
        }
      }
      catch(const ShutdownException &)
      {
      }
      catch(...)
      {
      }
    }
  );
}

void OmegaGame::_notification(int p_what)
{
  if(p_what == NOTIFICATION_EXIT_TREE || p_what == NOTIFICATION_PREDELETE)
  {
    InputQueue::instance().shutdown();
    if(game_thread_.joinable())
      game_thread_.join();
  }
}

void OmegaGame::provide_input(int key)
{
  InputQueue::instance().push(godot_to_pdcurses(key));
}

PackedInt32Array OmegaGame::get_tile_data()
{
  TileBuffer &buf = tile_buffer();
  PackedInt32Array arr;
  arr.resize(TileBuffer::W * TileBuffer::H);
  {
    std::lock_guard<std::mutex> lk(buf.mutex);
    for(int i = 0; i < TileBuffer::W * TileBuffer::H; ++i)
      arr.set(i, static_cast<int32_t>(buf.tiles[i]));
    buf.dirty.store(false, std::memory_order_release);
  }
  return arr;
}

bool OmegaGame::is_dirty()
{
  return tile_buffer().dirty.load(std::memory_order_acquire);
}

Vector2i OmegaGame::get_player_pos()
{
  // Player is owned by the game thread; int reads are atomic on x86/x64.
  return Vector2i(Player.x, Player.y);
}

int OmegaGame::get_buffer_width()
{
  return TileBuffer::W;
}

int OmegaGame::get_buffer_height()
{
  return TileBuffer::H;
}

PackedInt32Array OmegaGame::get_panel_data()
{
  PanelBuffer &buf = panel_buffer();
  PackedInt32Array arr;
  arr.resize(PanelBuffer::W * PanelBuffer::H);
  {
    std::lock_guard<std::mutex> lk(buf.mutex);
    for(int i = 0; i < PanelBuffer::W * PanelBuffer::H; ++i)
      arr.set(i, static_cast<int32_t>(buf.tiles[i]));
    buf.dirty.store(false, std::memory_order_release);
  }
  return arr;
}

bool OmegaGame::is_panel_dirty()
{
  return panel_buffer().dirty.load(std::memory_order_acquire);
}

int OmegaGame::get_panel_width()
{
  return PanelBuffer::W;
}

int OmegaGame::get_panel_height()
{
  return PanelBuffer::H;
}

bool OmegaGame::is_snapshot_dirty()
{
  return level_snapshot().dirty.exchange(false, std::memory_order_acq_rel);
}

int OmegaGame::get_snapshot_width()
{
  return LevelSnapshot::W;
}

int OmegaGame::get_snapshot_height()
{
  return LevelSnapshot::H;
}

PackedInt32Array OmegaGame::get_terrain_data()
{
  LevelSnapshot &snap = level_snapshot();
  PackedInt32Array arr;
  arr.resize(LevelSnapshot::W * LevelSnapshot::H);
  std::lock_guard<std::mutex> lk(snap.mutex);
  for(int i = 0; i < LevelSnapshot::W * LevelSnapshot::H; ++i)
    arr.set(i, static_cast<int32_t>(snap.cells[i].terrain_char));
  return arr;
}

PackedInt32Array OmegaGame::get_terrain_aux_data()
{
  LevelSnapshot &snap = level_snapshot();
  PackedInt32Array arr;
  arr.resize(LevelSnapshot::W * LevelSnapshot::H);
  std::lock_guard<std::mutex> lk(snap.mutex);
  for(int i = 0; i < LevelSnapshot::W * LevelSnapshot::H; ++i)
    arr.set(i, snap.cells[i].terrain_aux);
  return arr;
}

PackedInt32Array OmegaGame::get_item_data()
{
  LevelSnapshot &snap = level_snapshot();
  PackedInt32Array arr;
  arr.resize(LevelSnapshot::W * LevelSnapshot::H);
  std::lock_guard<std::mutex> lk(snap.mutex);
  for(int i = 0; i < LevelSnapshot::W * LevelSnapshot::H; ++i)
    arr.set(i, static_cast<int32_t>(snap.cells[i].item_char));
  return arr;
}

PackedInt32Array OmegaGame::get_creature_data()
{
  LevelSnapshot &snap = level_snapshot();
  PackedInt32Array arr;
  arr.resize(LevelSnapshot::W * LevelSnapshot::H);
  std::lock_guard<std::mutex> lk(snap.mutex);
  for(int i = 0; i < LevelSnapshot::W * LevelSnapshot::H; ++i)
    arr.set(i, static_cast<int32_t>(snap.cells[i].creature_char));
  return arr;
}

PackedByteArray OmegaGame::get_cell_flags()
{
  LevelSnapshot &snap = level_snapshot();
  PackedByteArray arr;
  arr.resize(LevelSnapshot::W * LevelSnapshot::H);
  std::lock_guard<std::mutex> lk(snap.mutex);
  for(int i = 0; i < LevelSnapshot::W * LevelSnapshot::H; ++i)
    arr.set(i, snap.cells[i].flags);
  return arr;
}

Dictionary OmegaGame::get_player()
{
  // Game thread owns Player; it is typically blocked at wgetch() while Godot
  // reads this, so no mutex is needed.  Individual int/long reads are atomic
  // on x86-64.  The name and meleestr strings are set before PHASE_RUNNING
  // and do not change during normal gameplay.
  Dictionary d;

  // Identity / progression
  d["name"]  = String(Player.name.c_str());
  d["title"] = String(levelname(Player.level).c_str());
  d["level"] = Player.level;
  d["xp"]    = static_cast<int64_t>(Player.xp);

  // Health / mana
  d["hp"]      = Player.hp;
  d["maxhp"]   = Player.maxhp;
  d["mana"]    = static_cast<int64_t>(Player.mana);
  d["maxmana"] = static_cast<int64_t>(Player.maxmana);

  // Base stats (current / max)
  d["str"]    = Player.str;
  d["maxstr"] = Player.maxstr;
  d["con"]    = Player.con;
  d["maxcon"] = Player.maxcon;
  d["dex"]    = Player.dex;
  d["maxdex"] = Player.maxdex;
  d["agi"]    = Player.agi;
  d["maxagi"] = Player.maxagi;
  d["iq"]     = Player.iq;
  d["maxiq"]  = Player.maxiq;
  d["pow"]    = Player.pow;
  d["maxpow"] = Player.maxpow;

  // Derived combat values (recomputed by calc_melee() each turn)
  d["hit"]        = Player.hit;
  d["dmg"]        = Player.dmg;
  d["absorption"] = Player.absorption;
  d["speed"]      = Player.speed;
  d["defense"]    = Player.defense;
  d["click"]      = Player.click;

  // Misc
  d["food"]       = Player.food;
  d["alignment"]  = Player.alignment;
  d["cash"]       = static_cast<int64_t>(Player.cash);
  d["patron"]     = Player.patron;
  d["birthday"]   = Player.birthday;
  d["sx"]         = Player.sx;
  d["sy"]         = Player.sy;
  d["x"]          = Player.x;
  d["y"]          = Player.y;
  d["itemweight"] = Player.itemweight;
  d["maxweight"]  = Player.maxweight;
  d["options"]    = static_cast<int64_t>(Player.options);
  d["meleestr"]   = String(Player.meleestr.c_str());

  // immunity[NUMIMMUNITIES] — 14 entries
  {
    PackedInt32Array arr;
    arr.resize(NUMIMMUNITIES);
    for(int i = 0; i < NUMIMMUNITIES; ++i)
      arr.set(i, Player.immunity[i]);
    d["immunity"] = arr;
  }

  // status[NUMSTATI] — 25 entries (duration counters; non-zero = active)
  {
    PackedInt32Array arr;
    arr.resize(NUMSTATI);
    for(int i = 0; i < NUMSTATI; ++i)
      arr.set(i, Player.status[i]);
    d["status"] = arr;
  }

  // rank[NUMRANKS] — 10 entries (guild ranks; 0 = not joined)
  {
    PackedInt32Array arr;
    arr.resize(NUMRANKS);
    for(int i = 0; i < NUMRANKS; ++i)
      arr.set(i, Player.rank[i]);
    d["rank"] = arr;
  }

  // guildxp[NUMRANKS] — 10 entries (XP within each guild)
  {
    PackedInt64Array arr;
    arr.resize(NUMRANKS);
    for(int i = 0; i < NUMRANKS; ++i)
      arr.set(i, static_cast<int64_t>(Player.guildxp[i]));
    d["guildxp"] = arr;
  }

  // Hunger state (from Player.food)
  d["hunger"] = String(hunger_string().c_str());

  // Movement / position state
  {
    const char *movement;
    if(gamestatusp(MOUNTED, GameStatus))
      movement = "Mounted";
    else if(Player.status[LEVITATING])
      movement = "Levitating";
    else
      movement = "Afoot";
    d["movement"] = String(movement);
  }

  // Inventory counts — full item data needs a separate API
  int possessions_count = 0;
  for(int i = 0; i < MAXITEMS; ++i)
    if(Player.possessions[i])
      ++possessions_count;
  d["possessions_count"] = possessions_count;
  d["pack_size"]         = static_cast<int>(Player.pack.size());

  return d;
}

Dictionary OmegaGame::get_time()
{
  // Time and Date are owned by the game thread; same access pattern as
  // get_player() — individual int/long reads are atomic on x86-64.
  Dictionary d;
  d["clock"] =
    String(std::format("{:2d}:{:02d} {}", showhour(), showminute(), hour() > 11 ? "PM" : "AM").c_str());
  d["hour"]   = hour();
  d["minute"] = static_cast<int>(Time % 60);
  d["day"]    = day();
  d["month"]  = String(month().c_str());
  d["time"]   = static_cast<int64_t>(Time);
  d["date"]   = Date;
  return d;
}

int OmegaGame::get_game_phase()
{
  return s_phase.load(std::memory_order_acquire);
}

void OmegaGame::_bind_methods()
{
  ClassDB::bind_method(D_METHOD("get_game_phase"), &OmegaGame::get_game_phase);
  BIND_CONSTANT(PHASE_STARTING);
  BIND_CONSTANT(PHASE_MENU);
  BIND_CONSTANT(PHASE_RUNNING);
  BIND_CONSTANT(PHASE_GAME_OVER);

  ClassDB::bind_method(D_METHOD("provide_input", "key"), &OmegaGame::provide_input);
  ClassDB::bind_method(D_METHOD("get_tile_data"), &OmegaGame::get_tile_data);
  ClassDB::bind_method(D_METHOD("is_dirty"), &OmegaGame::is_dirty);
  ClassDB::bind_method(D_METHOD("get_player_pos"), &OmegaGame::get_player_pos);
  ClassDB::bind_method(D_METHOD("get_buffer_width"), &OmegaGame::get_buffer_width);
  ClassDB::bind_method(D_METHOD("get_buffer_height"), &OmegaGame::get_buffer_height);
  ClassDB::bind_method(D_METHOD("get_player"), &OmegaGame::get_player);
  ClassDB::bind_method(D_METHOD("get_time"), &OmegaGame::get_time);

  // status_id enum — indices into the "status" PackedInt32Array from get_player()
#define BIND_STATUS(s) ClassDB::bind_integer_constant(get_class_static(), StringName(), "STATUS_" #s, s)
  BIND_STATUS(ACCURACY);
  BIND_STATUS(BLINDED);
  BIND_STATUS(SLOWED);
  BIND_STATUS(DISPLACED);
  BIND_STATUS(SLEPT);
  BIND_STATUS(DISEASED);
  BIND_STATUS(POISONED);
  BIND_STATUS(HASTED);
  BIND_STATUS(BREATHING);
  BIND_STATUS(INVISIBLE);
  BIND_STATUS(REGENERATING);
  BIND_STATUS(VULNERABLE);
  BIND_STATUS(BERSERK);
  BIND_STATUS(IMMOBILE);
  BIND_STATUS(ALERT);
  BIND_STATUS(AFRAID);
  BIND_STATUS(HERO);
  BIND_STATUS(LEVITATING);
  BIND_STATUS(ACCURATE);
  BIND_STATUS(TRUESIGHT);
  BIND_STATUS(SHADOWFORM);
  BIND_STATUS(ILLUMINATION);
  BIND_STATUS(DEFLECTION);
  BIND_STATUS(PROTECTION);
  BIND_STATUS(RETURNING);
  BIND_CONSTANT(NUMSTATI);
#undef BIND_STATUS
  ClassDB::bind_method(D_METHOD("get_panel_data"), &OmegaGame::get_panel_data);
  ClassDB::bind_method(D_METHOD("is_panel_dirty"), &OmegaGame::is_panel_dirty);
  ClassDB::bind_method(D_METHOD("get_panel_width"), &OmegaGame::get_panel_width);
  ClassDB::bind_method(D_METHOD("get_panel_height"), &OmegaGame::get_panel_height);
  ClassDB::bind_method(D_METHOD("is_snapshot_dirty"), &OmegaGame::is_snapshot_dirty);
  ClassDB::bind_method(D_METHOD("get_snapshot_width"), &OmegaGame::get_snapshot_width);
  ClassDB::bind_method(D_METHOD("get_snapshot_height"), &OmegaGame::get_snapshot_height);
  ClassDB::bind_method(D_METHOD("get_terrain_data"), &OmegaGame::get_terrain_data);
  ClassDB::bind_method(D_METHOD("get_terrain_aux_data"), &OmegaGame::get_terrain_aux_data);
  ClassDB::bind_method(D_METHOD("get_item_data"), &OmegaGame::get_item_data);
  ClassDB::bind_method(D_METHOD("get_creature_data"), &OmegaGame::get_creature_data);
  ClassDB::bind_method(D_METHOD("get_cell_flags"), &OmegaGame::get_cell_flags);
}

} // namespace godot
