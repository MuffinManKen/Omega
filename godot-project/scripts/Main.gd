class_name Main
extends Node2D

@onready var omega_game: OmegaGame = $OmegaGame
@onready var player_info_container: PlayerInfoContainer = $ScreenBounds/PlayerInfoContainer
@onready var title: ColorRect = $ScreenBounds/Title

const FONT_SIZE := 26

# ---------------------------------------------------------------------------
# Color system
# ---------------------------------------------------------------------------
# The game uses curses COLOR_PAIR(n) where n is 0-32.  Each pair has a
# foreground and background curses color (0-7 dark, 8-15 bright).
# CURSES_COLORS maps curses color index → RGB.  Note index 0 is true black,
# unlike the old COLOR_MAP where 0 was the "default terminal" white.

const CURSES_COLORS : Array[Color] = [
	Color("#000000"),  #  0 BLACK
	Color("#B21818"),  #  1 RED
	Color("#18B218"),  #  2 GREEN
	Color("#B26818"),  #  3 YELLOW (dark — looks brown)
	Color("#1818B2"),  #  4 BLUE
	Color("#B218B2"),  #  5 MAGENTA
	Color("#18B2B2"),  #  6 CYAN
	Color("#B2B2B2"),  #  7 WHITE (light grey)
	Color("#686868"),  #  8 BRIGHT_BLACK (dark grey)
	Color("#FF5454"),  #  9 BRIGHT_RED
	Color("#54FF54"),  # 10 BRIGHT_GREEN
	Color("#FFFF54"),  # 11 BRIGHT_YELLOW
	Color("#5454FF"),  # 12 BRIGHT_BLUE
	Color("#FF54FF"),  # 13 BRIGHT_MAGENTA
	Color("#54FFFF"),  # 14 BRIGHT_CYAN
	Color("#FFFFFF"),  # 15 BRIGHT_WHITE
]

# Foreground curses color index per COLOR_PAIR index 0-32 (from clrgen.cpp).
# A_BOLD brightens fg: if fg_idx < 8, use fg_idx+8 instead.
const PAIR_FG : Array[int] = [
	7,  #  0 default (white fg)
	1,  #  1 red
	2,  #  2 green
	3,  #  3 yellow/brown
	4,  #  4 blue
	5,  #  5 magenta
	6,  #  6 cyan
	7,  #  7 white
	8,  #  8 grey  (bright black — already bright, A_BOLD is no-op)
	9,  #  9 bright red
	10, # 10 bright green
	11, # 11 bright yellow
	12, # 12 bright blue
	13, # 13 bright magenta
	14, # 14 bright cyan
	15, # 15 bright white
	0,  # 16 black on green
	0,  # 17 black on red
	0,  # 18 black on white  ← PLAYER '@'
	0,  # 19 black on yellow
	4,  # 20 blue on white
	2,  # 21 green on blue
	2,  # 22 green on red
	2,  # 23 green on yellow
	5,  # 24 magenta on white
	1,  # 25 red on white
	7,  # 26 white on blue
	7,  # 27 white on red
	7,  # 28 white on yellow
	3,  # 29 yellow on blue
	3,  # 30 yellow on red
	3,  # 31 yellow on white
	3,  # 32 yellow on yellow
]

# Background curses color index per COLOR_PAIR index 0-32.
const PAIR_BG : Array[int] = [
	0,  #  0 black
	0,  #  1 black
	0,  #  2 black
	0,  #  3 black
	0,  #  4 black
	0,  #  5 black
	0,  #  6 black
	0,  #  7 black
	0,  #  8 black
	0,  #  9 black
	0,  # 10 black
	0,  # 11 black
	0,  # 12 black
	0,  # 13 black
	0,  # 14 black
	0,  # 15 black
	2,  # 16 green
	1,  # 17 red
	7,  # 18 white  ← PLAYER '@'
	3,  # 19 yellow
	7,  # 20 white
	4,  # 21 blue
	1,  # 22 red
	3,  # 23 yellow
	7,  # 24 white
	7,  # 25 white
	4,  # 26 blue
	1,  # 27 red
	3,  # 28 yellow
	4,  # 29 blue
	1,  # 30 red
	7,  # 31 white
	3,  # 32 yellow
]

const A_BOLD      := 0x00800000
const A_REVERSE   := 0x00200000
const A_UNDERLINE := 0x00100000

const MAP_WIDTH_IN_TILES := 64
const MAP_HEIGHT_IN_TILES := 34
const TILE_SIZE := Vector2(24, 24)

var tile_data: PackedInt32Array

var current_phase := OmegaGame.PHASE_STARTING

const TILE_GFX := {
	OmegaGame.TERRAIN_FLOOR : preload("res://gfx/tiles/floor.png"),
	OmegaGame.TERRAIN_WALL : preload("res://gfx/tiles/wall.png"),
	OmegaGame.TERRAIN_WATER : preload("res://gfx/tiles/water.png"),
	OmegaGame.TERRAIN_OPEN_DOOR : preload("res://gfx/tiles/open_door.png"),
	OmegaGame.TERRAIN_CLOSED_DOOR : preload("res://gfx/tiles/closed_door.png"),
}

# Structured per-cell snapshot for PHASE_RUNNING — separate terrain/item/creature
# layers so the renderer can eventually draw them independently with proper
# depth sorting, visibility tinting, and tile-sprite substitution.
# Populated by C++ snapshot_rebuild() on every drawvision() / show_screen() call.
var snap_terrain  : PackedInt32Array   # terrain_char (chtype) per cell
var snap_terrain_aux  : PackedInt32Array   # terrain auxilliary data per cell
var snap_items    : PackedInt32Array   # top item objchar / PILE / 0
var snap_creatures: PackedInt32Array   # creature monchar if visible / 0
var snap_flags    : PackedByteArray    # FLAG_SEEN | FLAG_IN_LOS per cell

var font : SystemFont
var font_size : Vector2
var font_ascent : float

var player : Player


func _ready() -> void:
	font = SystemFont.new()
	font.font_names = ["Consolas", "monospace"]
	font.font_weight = 400
	font_size = font.get_string_size("W", HorizontalAlignment.HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE)
	font_ascent = font.get_ascent(FONT_SIZE)
	printt("W", font_size)

	player = Player.new()
	player_info_container.set_player(player)

	# _draw can run before the first dirty fetch (initial draw on tree entry,
	# phase-change redraws); a zero-filled buffer renders as a blank screen.
	tile_data.resize(omega_game.get_buffer_width() * omega_game.get_buffer_height())
	var snap_count := omega_game.get_snapshot_width() * omega_game.get_snapshot_height()
	snap_terrain.resize(snap_count)
	snap_terrain_aux.resize(snap_count)
	snap_creatures.resize(snap_count)
	snap_items.resize(snap_count)
	snap_flags.resize(snap_count)

	title.visible = true
	title.gui_input.connect(handle_title_input)


func _process(_delta):
	var phase := omega_game.get_game_phase()
	if phase != current_phase:
		current_phase = phase
		queue_redraw()

	match phase:
		OmegaGame.PHASE_MENU:
			# Title screen and character creation — everything in the flat TileBuffer
			# (panel functions also write into TileBuffer cols 65-119 during menus).
			if omega_game.is_dirty():
				var pl_dat := omega_game.get_player()
				#print(pl_dat.name, pl_dat.str)
				if pl_dat.str > 0:
					player.update_data(pl_dat)
					player_info_container.update_ui()

				tile_data = omega_game.get_tile_data()
				queue_redraw()

		OmegaGame.PHASE_RUNNING, OmegaGame.PHASE_GAME_OVER:
			# TODO: switch to snapshot-based rendering when the tile-sprite layer
			# is ready.  The snapshot gives separate terrain / item / creature
			# arrays plus per-cell FLAG_SEEN / FLAG_IN_LOS so the renderer can
			# handle visibility tinting, depth sorting, and tile substitution.
			#
			# if omega_game.is_snapshot_dirty():
			#     snap_terrain   = omega_game.get_terrain_data()
			#		snap_terrain_aux = omega_game.get_terrain_aux_data()
			#     snap_items     = omega_game.get_item_data()
			#     snap_creatures = omega_game.get_creature_data()
			#     snap_flags     = omega_game.get_cell_flags()
			#     queue_redraw()
			#
			# Until then, fall through to the same TileBuffer path.
			if omega_game.is_dirty():
				var pl_dat := omega_game.get_player()
				player.update_data(pl_dat)
				var t := omega_game.get_time()
				player_info_container.update_time(t)
				player_info_container.update_ui()
				tile_data = omega_game.get_tile_data()
				snap_terrain = omega_game.get_terrain_data()
				snap_terrain_aux = omega_game.get_terrain_aux_data()
				queue_redraw()


func _draw() -> void:
	match current_phase:
		OmegaGame.PHASE_MENU, OmegaGame.PHASE_GAME_OVER:
			draw_tile_data()
		OmegaGame.PHASE_RUNNING:
			draw_game_world()


func draw_game_world() -> void:
	var tile_data_width := omega_game.get_buffer_width()
	var snap_stride := omega_game.get_snapshot_width()
	var w := mini(MAP_WIDTH_IN_TILES, snap_stride)
	var h := mini(MAP_HEIGHT_IN_TILES, omega_game.get_snapshot_height())

	#var pos := omega_game.get_player_pos()
	#printt("Player at ", pos, snap_terrain[pos.y * snap_stride + pos.x])

	for y in h:
		for x in w:
			var data_idx := y * tile_data_width + x
			var snap_idx := y * snap_stride + x
			if snap_terrain[snap_idx] == omega_game.TERRAIN_OPEN_DOOR:
				print("OpenDoor:", snap_terrain_aux[snap_idx])
			if TILE_GFX.has(snap_terrain[snap_idx]):
				draw_gfx_tile(TILE_GFX[snap_terrain[snap_idx]], x, y)
			#else:
			if snap_terrain[snap_idx] not in [omega_game.TERRAIN_WALL, omega_game.TERRAIN_WATER]:
				draw_tile(tile_data[data_idx], x, y, TILE_SIZE)


# Render the full 120×64 TileBuffer.  Used for PHASE_MENU and as the current
# PHASE_RUNNING fallback until the snapshot-based renderer is implemented.
# Panel functions write directly into TileBuffer at cols 65-119, so the HUD
# is included automatically.
func draw_tile_data() -> void:
	var w := omega_game.get_buffer_width()   # 120
	var h := omega_game.get_buffer_height()  # 64

	for y in h:
		for x in w:
			draw_tile(tile_data[y * w + x], x, y, font_size)


func draw_tile(raw_tile: int, x: int, y: int, p_tile_size : Vector2) -> void:
	var is_bold      := raw_tile & A_BOLD
	var is_reverse   := raw_tile & A_REVERSE
	var is_underline := raw_tile & A_UNDERLINE
	assert(not is_underline)

	var char_code := raw_tile & 0xFF
	if char_code == 0:
		char_code = 32
	var glyph := String.chr(char_code)

	var pair_idx := (raw_tile >> 24) & 0xFF
	if pair_idx >= PAIR_FG.size():
		push_error("Unknown color pair index: %d" % pair_idx)
		pair_idx = 0

	var fg_idx := PAIR_FG[pair_idx]
	var bg_idx := PAIR_BG[pair_idx]

	# A_BOLD brightens the foreground (only dark colors 0-7 shift to 8-15)
	if is_bold and fg_idx < 8:
		fg_idx += 8

	var fg_color := CURSES_COLORS[fg_idx]
	var bg_color := CURSES_COLORS[bg_idx]

	if is_reverse:
		var tmp := fg_color
		fg_color = bg_color
		bg_color = tmp


	var location := Vector2(x * p_tile_size.x, y * p_tile_size.y)
	var char_location := location + Vector2(0, font_ascent)

	# Draw background (skip true-black to avoid unnecessary calls; the viewport
	# clear color is already black).
	if bg_color != CURSES_COLORS[0]:
		draw_rect(Rect2(location, p_tile_size), bg_color)

	# Draw glyph for printable non-space characters.
	# Space and control chars are invisible — only the background rect matters.
	if char_code > 32:
		draw_char(font, char_location, glyph, FONT_SIZE, fg_color)


func draw_gfx_tile(p_tile_texture : Texture2D, p_x : int, p_y : int) -> void:
	var location := Vector2(p_x * TILE_SIZE.x, p_y * TILE_SIZE.y)
	draw_texture_rect(p_tile_texture, Rect2(location, TILE_SIZE), false)


func _unhandled_input(p_event: InputEvent) -> void:
	var key_event := p_event as InputEventKey
	if not key_event:
		return
	if not key_event.pressed or key_event.echo:
		return
	if title.visible:
		return

	var unicode := key_event.unicode

	# Printable ASCII (space through ~) and control characters (Ctrl+A..Z = 1..26)
	# both come through as unicode values and pass straight to the game.
	if (unicode >= 1 and unicode < 127):
		omega_game.provide_input(unicode)
	else:
		# Special key (arrows, home/end, pgup/pgdn, etc.) — pass the Godot
		# keycode and let OmegaGame::provide_input map it to a PDCurses value.
		omega_game.provide_input(key_event.keycode)


#TODO: This should be moved into a proper title component
func handle_title_input(p_input : InputEvent) -> void:
	if p_input is InputEventMouseButton or p_input is InputEventKey:
		title.visible = false


class Player extends RefCounted:
	var source_dict : Dictionary
	var name : String
	var title : String
	var level : int
	var xp : int
	var hit_points : int
	var max_hit_points : int
	var mana : int
	var max_mana : int
	var strength : int
	var max_strength : int
	var con : int
	var max_con : int
	var dex : int
	var max_dex : int
	var agi : int
	var max_agi : int
	var iq : int
	var max_iq : int
	var power : int
	var max_power : int
	var cash : int
	var hit : int
	var dmg : int
	var defense : int
	var armor : int
	var speed : int
	var item_weight : int
	var max_weight : int
	var hunger : String
	var is_poisoned : bool
	var is_diseased : bool
	var movement : String


	func update_data(p_dict : Dictionary) -> void:
		source_dict = p_dict
		name = source_dict.name
		title = source_dict.title
		level = source_dict.level
		xp = source_dict.xp
		hit_points = source_dict.hp
		max_hit_points = source_dict.maxhp
		mana = source_dict.mana
		max_mana = source_dict.maxmana

		strength = source_dict.str
		max_strength = source_dict.maxstr
		con = source_dict.con
		max_con = source_dict.maxcon
		dex = source_dict.dex
		max_dex = source_dict.maxdex
		agi = source_dict.agi
		max_agi = source_dict.maxagi
		iq = source_dict.iq
		max_iq = source_dict.maxiq
		power = source_dict.pow
		max_power = source_dict.maxpow

		cash = source_dict.cash
		hit = source_dict.hit
		dmg = source_dict.dmg
		defense = source_dict.defense
		armor = source_dict.absorption
		speed = source_dict.speed	#displayed as std::format("{}.{:02d}", 5 / Player.speed, 500 / Player.speed % 100)
		item_weight = source_dict.itemweight
		max_weight = source_dict.maxweight

		hunger = source_dict.hunger
		is_poisoned = source_dict.status[OmegaGame.STATUS_POISONED] > 0
		is_diseased = source_dict.status[OmegaGame.STATUS_DISEASED] > 0
		movement = source_dict.movement


  #┌──────────────────────────────────────┬──────────────────┬───────────────────────────────────────────┐
  #│                 Key                  │       Type       │                   Notes                   │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ name                                 │ String           │                                           │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ level, xp                            │ int / int64      │                                           │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ hp, maxhp                            │ int              │                                           │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ mana, maxmana                        │ int64            │                                           │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ str/con/dex/agi/iq/pow               │ int              │ current                                   │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ maxstr/maxcon/…                      │ int              │ base                                      │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ hit, dmg, absorption, speed, defense │ int              │ derived combat                            │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ click                                │ int              │ internal turn counter                     │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ food, alignment, cash                │ int / int64      │                                           │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ patron, birthday                     │ int              │                                           │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ sx, sy                               │ int              │ sanctuary coords                          │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ x, y                                 │ int              │ current position                          │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ itemweight, maxweight                │ int              │                                           │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ options                              │ int64            │ bitmask                                   │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ meleestr                             │ String           │ melee attack string                       │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ immunity                             │ PackedInt32Array │ 14 entries                                │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ status                               │ PackedInt32Array │ 25 entries (durations; non-zero = active) │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ rank                                 │ PackedInt32Array │ 10 guild ranks                            │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ guildxp                              │ PackedInt64Array │ 10 guild XP values                        │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ possessions_count                    │ int              │ equipped items (0–16)                     │
  #├──────────────────────────────────────┼──────────────────┼───────────────────────────────────────────┤
  #│ pack_size                            │ int              │ carried items                             │
  #└──────────────────────────────────────┴──────────────────┴───────────────────────────────────────────┘
