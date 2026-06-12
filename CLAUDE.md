# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working Agreements

- **`godot-project/` (GDScript, scenes, assets) is Ken's domain.** Never edit files there unless
  explicitly asked. Diagnose problems and provide snippets for Ken to apply himself.
- **`src/` C++ and the CMake files are fair game** once a fix or feature has been agreed in
  conversation.
- **No AI attribution, anywhere.** No `Co-Authored-By` trailers on commits, no "Generated with"
  footers on PRs or issues.
- **Upstream-facing actions need explicit approval each time**: creating or commenting on PRs and
  issues, and anything else visible to the upstream maintainer. Local reversible work, and commits
  and pushes of already-agreed work to Ken's own fork, do not.
- **Keep upstream-shared files minimally diffed.** This repo is a fork of
  https://github.com/Lyle-Tafoya/Omega (PRs target its `development` branch). In files that exist
  upstream, match the surrounding style by hand and do not run clang-format over the whole file —
  reformat churn makes upstream PRs unreviewable. Fork-only files follow `.clang-format` fully.

## Project Overview

Omega is a modern C++ fork of the 1987 roguelike "Omega", featuring bug fixes, balance improvements, and UI enhancements. The active goal is a **Godot 4 port** replacing the curses terminal renderer with graphical 64×64 tiles.

- **Language**: C++23
- **Build System**: CMake
- **Rendering (standalone)**: PDCurses (terminal) or SDL2/OpenGL (Windows graphical mode)
- **Rendering (Godot port)**: GDExtension — C++ game logic runs on a background thread, Godot reads tile state and renders
- **Repository**: https://github.com/MuffinManKen/Omega.git
- **License**: GNU General Public License v3

## Build Instructions

`CMakePresets.json` captures all build configurations. Use the preset commands below rather than raw `cmake` flags.

### Standalone (curses) build

**Windows — terminal mode:**
```
cmake --preset standalone-release
cmake --build --preset standalone-release
```
Must be run from a **Visual Studio x64 Developer Command Prompt** (any recent VS version).

**Linux/macOS:**
```
mkdir build && cd build && cmake .. && cmake --build .
```
Linux requires GCC 13+; see README for Ubuntu PPA instructions.

### GDExtension (Godot) build

> **VS2022 required.** godot-cpp was compiled with MSVC 14.44. Using a later toolchain
> (e.g. VS2026) will produce `STL1001: Unexpected compiler version` errors. Always
> configure and build from a **VS2022 x64 Developer Command Prompt**.
> If you see that error, delete `build_gdext/` and reconfigure from VS2022.

Requires the godot-cpp submodule:
```
git submodule update --init --recursive
```

From a **VS2022 x64 Developer Command Prompt**:
```
cmake --preset gdext-release
cmake --build --preset gdext-release
```

A post-build step automatically copies `omega.dll` and the `lib/` data directory into `godot-project/bin/` and `godot-project/lib/`.

To run: open `godot-project/` in Godot 4 and press Play.

## Code Style & Formatting

Strict `.clang-format` config: 105-char lines, 2-space indentation, Allman braces, right-aligned pointers. Run clang-format on modified files before committing.

## CMake Target Structure

Two targets share most source code:

```
omega-core (STATIC)        — game logic, no rendering, no main()
    ├── omega (EXECUTABLE) — links omega-core + scr.cpp + interactive_menu.cpp + omega.cpp
    └── omega-gdextension  — links all omega-core sources + scr_godot.cpp + curses_stub.cpp
        (SHARED, compiled   + OmegaGame.cpp + gdextension_entry.cpp
         with -DOMEGA_CURSES_STUB)
```

`omega-core` is compiled **without** `OMEGA_CURSES_STUB`. The GDExtension target re-compiles the same source files **with** `OMEGA_CURSES_STUB`, which swaps out the curses headers for `curses_stub.h` and enables the `interactive_menu` stub.

## Architecture Overview

### Core Game State
- **Global variables** defined in `globals.cpp`, extern'd via `glob.h`
- `player Player`, `level *Level`, `std::vector<unique_ptr<level>> Dungeon`, `std::unique_ptr<level> City`, `terrain Country[64][64]`
- `long GameStatus` — bit-vector tracking quests, items, game progress
- `int Current_Environment` — which of the E_* environments the player is in

### Level and Terrain Structure
- **`level`** (`level.h`): `site[64][64]` grid of `location`, plus monster list and metadata
- **`location`** (`level.h`): `locchar`/`showchar` (chtype tile), `creature*`, `things` vector, `p_locf` function pointer, `lstatus` bitfield
- **`terrain`** (`defs.h`): countryside tile with `base_terrain_type`/`current_terrain_type` (both chtype)

### The `chtype` Type
`chtype` (typedef'd as `unsigned long`) is Omega's tile representation. Its bit layout matches PDCurses wincon:
- Bits 0–7: ASCII character code (bits 8–15 are defined as part of `A_CHARTEXT` by PDCurses but are unused in practice — all characters are ASCII and fit in 8 bits)
- Bits 16–23: attribute flags — `A_BOLD`=0x00800000, `A_BLINK`=0x00400000, `A_REVERSE`=0x00200000, `A_UNDERLINE`=0x00100000, `A_ITALIC`=0x00080000, etc.
- Bits 24–31: color pair index (via `COLOR_PAIR(n)`, extracted with `(ch >> 24) & 0xFF`)

The C++ code actively sets attribute flags: `A_REVERSE` highlights the player cursor, `A_STANDOUT` (`A_REVERSE | A_BOLD`) highlights items, and `A_BOLD` is used throughout the UI. These flags are stored in `TileBuffer` intact. However, the current GDScript renderer (`Main.gd`) only reads bits 0–7 (character) and bits 24–31 (color pair) — **attribute flags are not yet rendered by Godot** and silently discarded. Implementing attribute rendering (e.g. translating `A_REVERSE` to a highlight tile, `A_BOLD` to a brighter color variant) is a known gap.

Color pairs (from `clrgen.h`): 0=default/white, 1=red, 2=green, 3=brown, 4=blue, 5=magenta, 6=cyan, 7=white, 8=grey, 9–15=light variants.

### Game Flow
- **`game_session.cpp`**: `init_game_session()` (setup, title menu, world load) and `run_game_loop()` (the `while(true)` dispatch loop). Both are called by `main()` in the standalone build and by `OmegaGame::_ready()` on a background thread in the GDExtension.
- **`time_clock()`** (`time.cpp`): coordinates player turns and monster actions using `Tick`/`Player.click` counters
- **`p_process()`** (`command1.cpp`): reads player input and dispatches to game commands — the inner turn loop
- **`p_country_process()`** (`command1.cpp`): same for the countryside environment

### Rendering Interface (`scr.h`)
All rendering goes through functions declared in `scr.h`. Two implementations exist:
- **`scr.cpp`**: curses-based, used by the standalone build
- **`scr_godot.cpp`**: writes to `TileBuffer`, used by the GDExtension

Key functions: `putspot(x, y, chtype)` — the single tile write point; `drawvision(x, y)` — updates visible tiles each turn; `show_screen()` / `drawscreen()` — full redraws; `queue_message()` — queues text to `message_buffer`.

### Godot Port Architecture

#### Input flow
```
Godot _input() → Main.gd → OmegaGame::provide_input(key)
              → godot_to_pdcurses(key) → InputQueue::push(key)
              → game thread unblocks from InputQueue::pop()
              → game processes command → drawvision() → TileBuffer
```

#### Output flow
```
game thread: putspot(x, y, ch) → TileBuffer::set(x, y, ch) [mutex]
Godot thread: OmegaGame::get_tile_data() → PackedInt32Array [mutex]
             → _draw() renders 120×64 tile buffer
```

#### Thread boundary
The game loop runs on a **background thread** started in `OmegaGame::_ready()`. The only shared state crossing the thread boundary:
- **In**: `InputQueue` (blocking queue; `pop()` throws `ShutdownException` on shutdown)
- **Out**: `TileBuffer` (120×64 array of chtype; mutex-protected)

Shutdown: `NOTIFICATION_EXIT_TREE` calls `InputQueue::shutdown()` → throws `ShutdownException` through all blocking input calls → game thread exits → `join()` returns.

#### Key files
| File | Purpose |
|------|---------|
| `src/OmegaGame.h/cpp` | Godot Node class; exposes input (`provide_input`), tile buffer (`get_tile_data`, `is_dirty`), HUD panel (`get_panel_*`), per-layer snapshot (`get_terrain_data`, `get_terrain_aux_data`, `get_item_data`, `get_creature_data`, `get_cell_flags`, `is_snapshot_dirty`), `get_player`, `get_time`, `get_game_phase`, plus `LevelTerrain`/`CountryTerrain`/`ItemGlyph` enums (chtype values from defs.h) and `STATUS_*`/`PHASE_*` constants. **Include order in OmegaGame.h is load-bearing** — godot headers before defs.h (curses `KEY_*` macros vs godot enum members); see the clang-format off block. |
| `src/render_buffer.hpp` | `TileBuffer` struct (120×64, mutex + atomic dirty flag); `tile_buffer()` singleton |
| `src/scr_godot.cpp` | Full implementation of `scr.h` for the GDExtension: tile writes, input routing, color markup parsing |
| `src/curses_stub.h/cpp` | Defines curses types/constants and stubs all curses functions; text output routes to `TileBuffer`; `wgetch`/`getch` route to `InputQueue` |
| `src/input_queue.hpp` | Thread-safe blocking input queue; throws `ShutdownException` on shutdown |
| `src/game_session.h/cpp` | `init_game_session()` and `run_game_loop()` extracted from `omega.cpp` |
| `src/globals.cpp` | Global variable definitions (moved from `omega.cpp`) |
| `godot-project/` | Godot 4 project: `omega.gdextension` manifest, `scenes/Main.tscn`, `scripts/Main.gd` |

#### Tile buffer layout
Buffer is 120 wide × 64 tall. Game world tiles occupy columns 0–63. Columns 64–119 are used by menus and text UI (the standalone game used a 106-column terminal). `show_screen()` clears the full 120-wide buffer before redrawing the 64-wide game world to prevent stale menu text.

In GDScript:
```gdscript
var w := omega_game.get_buffer_width()   # 120
var h := omega_game.get_buffer_height()  # 64
var tiles := omega_game.get_tile_data()  # PackedInt32Array, length w*h
var ch := tiles[y * w + x]
var char_code  := ch & 0xFF          # ASCII character
var color_pair := (ch >> 24) & 0xFF  # color pair index (0–15+)
```

#### Curses compatibility
All curses access goes through `src/omega_curses.h`, the single point of indirection: it includes `curses_stub.h` when `OMEGA_CURSES_STUB` is defined, `<curses.h>` otherwise. `defs.h`, `object.h`, `monster.h`, `level.h`, `clrgen.cpp`, and `scr.h` include it instead of curses directly. The stub's `chtype` bit layout matches PDCurses for save file compatibility. Similarly, `interactive_menu.hpp` redirects to `interactive_menu_stub.hpp` under `OMEGA_CURSES_STUB` — a stub that stores lines and renders them to `TileBuffer` via `mvwaddstr`. Keep upstream-file changes to these one-line redirects so the enabling layer stays easy to submit as an upstream PR.

## Testing

No automated tests. For the standalone build: compile and play through affected gameplay paths. For the GDExtension: open `godot-project/` in Godot 4, press Play, navigate through the title menu (no visible title screen yet — it's curses-based; press keys to advance), then verify in-game rendering and input.

## Important Implementation Notes

- **Global state**: most game state is global. The game thread owns all of it exclusively — Godot never touches game state directly.
- **Function pointers**: monsters, items, and locations store behavior as int IDs in `.dat` files, resolved to C++ function pointers at runtime.
- **Save file compatibility**: `SAVE_FILE_VERSION` in `defs.h` gates compatibility. The GDExtension uses the same `chtype` bit layout as PDCurses to maintain save compatibility with the standalone build.
- **Random seeding**: `std::mt19937 generator` in `util.cpp`; seeded per-environment via `initrand()`.
- **TOML config**: play-as-yourself mode with stat customization (clamped 4–18).
- **Title menu**: still curses-based (rendered via `curses_stub` → `TileBuffer`); will be replaced with a Godot UI scene.
