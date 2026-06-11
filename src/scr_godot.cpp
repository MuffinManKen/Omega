// Godot rendering implementation of the scr.h interface.
// Replaces scr.cpp in the GDExtension build.

#include "glob.h"
#include "input_queue.hpp"
#include "level_snapshot.hpp"
#include "panel_buffer.hpp"
#include "render_buffer.hpp"
#include "scr.h"
#include "scrolling_buffer.hpp"

#include <cctype>
#include <format>
#include <limits>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Buffer singletons
// ---------------------------------------------------------------------------

TileBuffer &tile_buffer()
{
  static TileBuffer instance;
  return instance;
}

PanelBuffer &panel_buffer()
{
  static PanelBuffer instance;
  return instance;
}

LevelSnapshot &level_snapshot()
{
  static LevelSnapshot instance;
  return instance;
}

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static scrolling_buffer message_buffer(80, 64);
static int              lastx = -1, lasty = -1;

// Defined in glob.h as extern, provided here for the GDExtension build.
std::unique_ptr<interactive_menu> menu;

// menu_window is a curses WINDOW used by char.cpp; null in GDExtension.
WINDOW *menu_window = nullptr;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void initgraf()
{
  ScreenWidth  = MAXWIDTH;
  ScreenLength = MAXLENGTH;
  message_buffer.resize(ScreenWidth, 64);
  menu = std::make_unique<interactive_menu>(nullptr, 0, 0);
  // Pre-fill with SPACE so the first _draw() call has a valid (blank) state
  // rather than null chtype values.  dirty=true triggers an immediate redraw.
  tile_buffer().fill(static_cast<uint32_t>(SPACE));
}

void endgraf()
{
  InputQueue::instance().shutdown();
}

// ---------------------------------------------------------------------------
// Input — all routes through InputQueue::pop()
// ---------------------------------------------------------------------------

int mgetc()
{
  return InputQueue::instance().pop();
}

int mcigetc()
{
  int c = InputQueue::instance().pop();
  if(c >= static_cast<int>('A') && c <= static_cast<int>('Z'))
    return c + static_cast<int>('a' - 'A');
  return c;
}

char menugetc()
{
  return static_cast<char>(InputQueue::instance().pop());
}

int get_level_input()
{
  return InputQueue::instance().pop();
}

int get_message_input()
{
  return InputQueue::instance().pop();
}

int get_mouse_event(mouse_event &)
{
  return -1;
}

int ynq()
{
  int p;
  do
  {
    p = InputQueue::instance().pop();
  } while(p != 'n' && p != 'y' && p != 'q' && p != ESCAPE && p != EOF && p != ' ');

  switch(p)
  {
    case 'y':
      message_buffer.append("yes.", false);
      break;
    case ' ':
      p = 'n';
      [[fallthrough]];
    case 'n':
      message_buffer.append("no.", false);
      break;
    case ESCAPE:
      p = 'q';
      [[fallthrough]];
    case 'q':
      message_buffer.append("quit.", false);
      break;
    default:
      break;
  }
  return p;
}

void more_wait()
{
  if(gamestatusp(SUPPRESS_PRINTING, GameStatus))
    return;
  append_message("--MORE--", true);
  int c;
  do
  {
    c = InputQueue::instance().pop();
  } while(c != KEY_ENTER && c != '\n' && c != ' ');
  message_buffer.pop_back();
}

bool stillonblock()
{
  int c;
  do
  {
    c = InputQueue::instance().pop();
  } while(c != ' ' && c != ESCAPE && c != EOF);
  return (c == ' ');
}

std::string msgscanstring()
{
  std::string input_str;
  int         player_input = mgetc();
  while(player_input != '\n' && player_input != KEY_ENTER)
  {
    if(player_input == KEY_BACKSPACE || player_input == '\b' || player_input == KEY_DC ||
       player_input == DELETE)
    {
      if(!input_str.empty())
      {
        input_str.pop_back();
        dobackspace();
      }
    }
    else if(player_input >= 32 && player_input <= 126)
    {
      input_str.push_back(static_cast<char>(player_input));
      message_buffer.append(std::string(1, static_cast<char>(player_input)), false);
    }
    player_input = mgetc();
  }
  return input_str;
}

int getnumber(int range)
{
  int value = 1;
  if(range == 1)
    return 1;

  std::string message = "How many? Change with < or >, ESCAPE to select: " + std::to_string(value);
  append_message(message, true);
  bool done = false;
  while(!done)
  {
    message = "How many? Change with < or >, ESCAPE to select: " + std::to_string(value);
    message_buffer.replace_last(message);
    int atom;
    do
    {
      atom = mcigetc();
    } while(atom != '<' && atom != '>' && atom != ESCAPE);
    if(atom == '>' && value < range)
      value++;
    else if(atom == '<' && value > 1)
      value--;
    else if(atom == ESCAPE)
      done = true;
  }
  return value;
}

long parsenum()
{
  int  digits[8];
  int  place = -1;
  long num   = 0;

  int player_input = ' ';
  while(player_input != ESCAPE && player_input != '\n' && player_input != KEY_ENTER)
  {
    player_input = mgetc();
    if(player_input == KEY_BACKSPACE || player_input == '\b' || player_input == KEY_DC ||
       player_input == DELETE)
    {
      if(place > -1)
        --place;
    }
    else if(player_input >= '0' && player_input <= '9' && place < 7)
    {
      digits[++place] = player_input - '0';
    }
  }
  long mult = 1;
  for(int i = place; i >= 0; --i)
  {
    num += digits[i] * mult;
    mult *= 10;
  }
  return num;
}

// ---------------------------------------------------------------------------
// Message buffer
// ---------------------------------------------------------------------------

void queue_message(const std::string &message, bool force_break)
{
  if(!gamestatusp(SUPPRESS_PRINTING, GameStatus))
    message_buffer.receive(message, force_break);
}

void append_message(const std::string &message, bool force_break)
{
  if(!gamestatusp(SUPPRESS_PRINTING, GameStatus))
    message_buffer.append(message, true, force_break);
}

void replace_last_message(const std::string &message)
{
  if(!gamestatusp(SUPPRESS_PRINTING, GameStatus))
    message_buffer.replace_last(message);
}

void commanderror()
{
  message_buffer.receive(std::to_string(Cmd) + " : unknown command");
}

// ---------------------------------------------------------------------------
// Tile writes — the single crossing point for level rendering
// ---------------------------------------------------------------------------

void putspot(int x, int y, chtype c)
{
  if(inbounds(x, y))
    tile_buffer().set(x, y, static_cast<uint32_t>(c));
}

void plotchar(chtype c, int x, int y)
{
  if(inbounds(x, y))
    tile_buffer().set(x, y, static_cast<uint32_t>(c));
}

void plotmon(monster *m)
{
  if(inbounds(m->x, m->y))
    tile_buffer().set(m->x, m->y, static_cast<uint32_t>(m->monchar));
}

// ---------------------------------------------------------------------------
// Game-logic queries (identical logic to scr.cpp)
// ---------------------------------------------------------------------------

bool litroom(int x, int y)
{
  if(Level->site[x][y].roomnumber < ROOMBASE)
    return false;
  return loc_statusp(x, y, LIT, *Level) || Player.status[ILLUMINATION];
}

chtype getspot(int x, int y, int showmonster)
{
  if(loc_statusp(x, y, SECRET, *Level))
    return WALL;

  switch(Level->site[x][y].locchar)
  {
    case WATER:
      if(!Level->site[x][y].creature)
        return WATER;
      if(m_statusp(*Level->site[x][y].creature, SWIMMING))
        return WATER;
      if(showmonster)
        return Level->site[x][y].creature->monchar;
      return WATER;

    case CLOSED_DOOR:
    case LAVA:
    case FIRE:
    case ABYSS:
      return Level->site[x][y].locchar;

    case RUBBLE:
    case HEDGE:
      if(showmonster && Level->site[x][y].creature)
      {
        if(m_statusp(*Level->site[x][y].creature, M_INVISIBLE) && !Player.status[TRUESIGHT])
          return getspot(x, y, false);
        return Level->site[x][y].creature->monchar;
      }
      return Level->site[x][y].locchar;

    default:
      if(showmonster && Level->site[x][y].creature)
      {
        if(m_statusp(*Level->site[x][y].creature, M_INVISIBLE) && !Player.status[TRUESIGHT])
          return getspot(x, y, false);
        return Level->site[x][y].creature->monchar;
      }
      if(!Level->site[x][y].things.empty())
      {
        if(Level->site[x][y].things.size() > 1)
          return PILE;
        return Level->site[x][y].things.back()->objchar;
      }
      return Level->site[x][y].locchar;
  }
}

int move_slot(int oldslot, int newslot, int maxslot)
{
  return (newslot >= 0 && newslot < maxslot) ? newslot : oldslot;
}

// ---------------------------------------------------------------------------
// Level snapshot — structured per-cell data for the Godot tile renderer
// ---------------------------------------------------------------------------

static CellSnapshot build_cell_snapshot(int x, int y)
{
  CellSnapshot      cs{};
  const location   &loc    = Level->site[x][y];
  const bool        seen   = loc_statusp(x, y, SEEN, *Level);
  const bool        in_los = view_los_p(Player.x, Player.y, x, y);

  if(seen)   cs.flags |= CellSnapshot::FLAG_SEEN;
  if(in_los) cs.flags |= CellSnapshot::FLAG_IN_LOS;

  if(!seen && !in_los)
    return cs;

  // Secret passages always appear as wall
  if(loc_statusp(x, y, SECRET, *Level))
  {
    cs.terrain_char = WALL;
    return cs;
  }

  cs.terrain_char = loc.locchar;
  cs.terrain_aux  = loc.aux;

  switch(loc.locchar)
  {
    // These terrain types never reveal what is on them
    case CLOSED_DOOR:
    case LAVA:
    case FIRE:
    case ABYSS:
      break;

    // Rubble and hedge hide items but can show creatures
    case RUBBLE:
    case HEDGE:
      if(in_los && loc.creature && loc.creature->hp > 0)
        if(Player.status[TRUESIGHT] || !m_statusp(*loc.creature, M_INVISIBLE))
          cs.creature_char = loc.creature->monchar;
      break;

    // Water shows non-swimming creatures but hides items
    case WATER:
      if(in_los && loc.creature && loc.creature->hp > 0)
        if(!m_statusp(*loc.creature, SWIMMING))
          if(Player.status[TRUESIGHT] || !m_statusp(*loc.creature, M_INVISIBLE))
            cs.creature_char = loc.creature->monchar;
      break;

    default:
      if(!loc.things.empty())
        cs.item_char = loc.things.size() > 1 ? PILE : loc.things.back()->objchar;
      if(in_los && loc.creature && loc.creature->hp > 0)
        if(Player.status[TRUESIGHT] || !m_statusp(*loc.creature, M_INVISIBLE))
          cs.creature_char = loc.creature->monchar;
      break;
  }

  return cs;
}

// Build the full 64×64 snapshot from current game state and push it to the
// shared LevelSnapshot buffer. Called after every rendering event.
static void snapshot_rebuild()
{
  std::array<CellSnapshot, LevelSnapshot::W * LevelSnapshot::H> buf{};

  if(Current_Environment != E_COUNTRYSIDE)
  {
    for(int y = 0; y < MAXLENGTH; ++y)
      for(int x = 0; x < MAXWIDTH; ++x)
        buf[y * LevelSnapshot::W + x] = build_cell_snapshot(x, y);
  }
  else
  {
    // Countryside: no items or creatures on the overworld map.
    // All seen tiles are fully visible (it behaves as a revealed map).
    for(int y = 0; y < MAXLENGTH; ++y)
      for(int x = 0; x < MAXWIDTH; ++x)
      {
        CellSnapshot cs{};
        if(c_statusp(x, y, SEEN, Country))
        {
          cs.flags        = CellSnapshot::FLAG_SEEN | CellSnapshot::FLAG_IN_LOS;
          cs.terrain_char = static_cast<chtype>(Country[x][y].current_terrain_type);
        }
        buf[y * LevelSnapshot::W + x] = cs;
      }
  }

  level_snapshot().rebuild(buf);
}

// ---------------------------------------------------------------------------
// Level rendering
// ---------------------------------------------------------------------------

void erase_level()
{
  tile_buffer().fill(static_cast<uint32_t>(SPACE));
}

void erase_monster(monster *m)
{
  if(loc_statusp(m->x, m->y, SEEN, *Level))
    putspot(m->x, m->y, getspot(m->x, m->y, false));
  else
    blotspot(m->x, m->y);
}

void blotspot(int x, int y)
{
  if(inbounds(x, y))
  {
    lreset(x, y, SEEN, *Level);
    Level->site[x][y].showchar = SPACE;
    putspot(x, y, SPACE);
  }
}

void blankoutspot(int x, int y)
{
  if(inbounds(x, y))
  {
    lreset(x, y, LIT, *Level);
    lset(x, y, CHANGED, *Level);
    if(Level->site[x][y].locchar == FLOOR)
    {
      Level->site[x][y].showchar = SPACE;
      putspot(x, y, SPACE);
    }
  }
}

void drawspot(int x, int y)
{
  if(inbounds(x, y))
  {
    chtype c = getspot(x, y, false);
    if(c != Level->site[x][y].showchar)
    {
      if(view_los_p(Player.x, Player.y, x, y))
      {
        lset(x, y, SEEN, *Level);
        Level->site[x][y].showchar = c;
        putspot(x, y, c);
      }
    }
  }
}

void dodrawspot(int x, int y)
{
  if(inbounds(x, y))
  {
    chtype c = getspot(x, y, false);
    if(c != Level->site[x][y].showchar)
    {
      lset(x, y, SEEN, *Level);
      Level->site[x][y].showchar = c;
      putspot(x, y, c);
    }
  }
}

void plotspot(int x, int y, int showmonster)
{
  if(loc_statusp(x, y, SEEN, *Level))
    putspot(x, y, getspot(x, y, showmonster));
  else
    putspot(x, y, SPACE);
}

void drawplayer()
{
  if(Current_Environment == E_COUNTRYSIDE)
  {
    if(inbounds(lastx, lasty))
      putspot(lastx, lasty, static_cast<chtype>(Country[lastx][lasty].current_terrain_type));
    putspot(Player.x, Player.y, PLAYER);
  }
  else
  {
    if(inbounds(lastx, lasty))
      plotspot(lastx, lasty, !Player.status[BLINDED]);
    if(!Player.status[INVISIBLE] || Player.status[TRUESIGHT])
      putspot(Player.x, Player.y, PLAYER);
    else
      putspot(Player.x, Player.y, getspot(Player.x, Player.y, false) | A_REVERSE);
  }
  lastx = Player.x;
  lasty = Player.y;
}

void drawmonsters(int display)
{
  for(std::unique_ptr<monster> &m : Level->mlist)
  {
    if(m->hp > 0)
    {
      if(display)
      {
        if(view_los_p(Player.x, Player.y, m->x, m->y))
        {
          if(Player.status[TRUESIGHT] || !m_statusp(*m, M_INVISIBLE))
            putspot(m->x, m->y, m->monchar);
        }
      }
      else
      {
        erase_monster(m.get());
      }
    }
  }
}

void drawvision(int x, int y)
{
  static int oldx = -1, oldy = -1;

  if(Current_Environment != E_COUNTRYSIDE)
  {
    if(Player.status[BLINDED])
    {
      drawspot(oldx, oldy);
      drawspot(x, y);
      drawplayer();
    }
    else
    {
      if(Player.status[ILLUMINATION] > 0)
      {
        for(int i = -2; i < 3; ++i)
          for(int j = -2; j < 3; ++j)
            if(inbounds(x + i, y + j) && view_los_p(x + i, y + j, Player.x, Player.y))
              dodrawspot(x + i, y + j);
      }
      else
      {
        for(int i = -1; i < 2; ++i)
          for(int j = -1; j < 2; ++j)
            if(inbounds(x + i, y + j))
              dodrawspot(x + i, y + j);
      }
      drawplayer();
      drawmonsters(false);
      drawmonsters(true);
    }
    oldx = x;
    oldy = y;
  }
  else
  {
    for(int i = -1; i < 2; ++i)
    {
      for(int j = -1; j < 2; ++j)
      {
        if(!inbounds(x + i, y + j) || c_statusp(x + i, y + j, SEEN, Country))
          continue;
        c_set(x + i, y + j, SEEN, Country);
        putspot(x + i, y + j, static_cast<chtype>(Country[x + i][y + j].current_terrain_type));
      }
    }
    drawplayer();
  }
  snapshot_rebuild();
}

void show_screen()
{
  // Returning to game rendering — no overlay rows should survive.
  set_protected_rows(0);
  // Clear the full buffer so columns beyond the 64-wide game world and any
  // stale menu text are wiped before we redraw game tiles.
  tile_buffer().fill(static_cast<uint32_t>(SPACE));

  if(Current_Environment != E_COUNTRYSIDE)
  {
    for(int y = 0; y < LENGTH; ++y)
      for(int x = 0; x < WIDTH; ++x)
      {
        chtype c = loc_statusp(x, y, SEEN, *Level) ? getspot(x, y, false) : SPACE;
        if(c == PILE)
          c = Level->site[x][y].things.back()->objchar | A_STANDOUT;
        tile_buffer().set(x, y, static_cast<uint32_t>(c));
      }
  }
  else
  {
    for(int y = 0; y < LENGTH; ++y)
      for(int x = 0; x < WIDTH; ++x)
      {
        chtype c = c_statusp(x, y, SEEN, Country) ? Country[x][y].current_terrain_type : SPACE;
        tile_buffer().set(x, y, static_cast<uint32_t>(c));
      }
  }
  snapshot_rebuild();
}

void drawscreen()
{
  if(Current_Environment == E_COUNTRYSIDE)
  {
    for(int i = 0; i < WIDTH; ++i)
      for(int j = 0; j < LENGTH; ++j)
        c_set(i, j, SEEN, Country);
  }
  else
  {
    for(int i = 0; i < WIDTH; ++i)
      for(int j = 0; j < LENGTH; ++j)
        lset(i, j, SEEN, *Level);
  }
  if(Current_Environment == E_CITY)
    for(int i = 0; i < NUMCITYSITES; ++i)
      CitySiteList[i][0] = 1;
  show_screen();
}

void setlastxy(int new_x, int new_y)
{
  lastx = new_x;
  lasty = new_y;
}

void plotchar_explosion(chtype ch, int x, int y)
{
  if(inbounds(x, y))
    tile_buffer().set(x, y, static_cast<uint32_t>(ch));
}

void draw_explosion(chtype pyx, int x, int y)
{
  // Simplified: write the final tile state without animation.
  for(int i = 0; i < 9; ++i)
    plotspot(x + Dirs[0][i], y + Dirs[1][i], true);
}

void lightspot(int x, int y)
{
  if(inbounds(x, y))
    lset(x, y, LIT, *Level);
}

void calculate_offsets(int, int) {}
void screencheck(int, int)       {}

// ---------------------------------------------------------------------------
// Rendering stubs — HUD/UI output (step 4 will wire these to Godot nodes)
// ---------------------------------------------------------------------------

void xredraw()
{
  drawscreen();
  dataprint();
  print_combat_stats();
  showflags();
  timeprint();
  print_messages();
}
void redraw()        { show_screen(); }
void levelrefresh()  {}
void clear_screen()  {}

// Parse Omega's |r |G etc. color markup and write to the tile buffer.
void color_waddstr(WINDOW *w, const std::string &s)
{
  const size_t len = s.length();
  for(size_t i = 0; i < len; ++i)
  {
    if(s[i] == '|' && i + 1 < len)
    {
      ++i;
      switch(s[i])
      {
        case 'l': wcolor_set(w, COLOR_BLACK, nullptr); break;
        case 'L': wcolor_set(w, COLOR_BLACK + 8, nullptr); break;
        case 'r': wcolor_set(w, COLOR_RED, nullptr); break;
        case 'R': wcolor_set(w, COLOR_RED + 8, nullptr); break;
        case 'g': wcolor_set(w, COLOR_GREEN, nullptr); break;
        case 'G': wcolor_set(w, COLOR_GREEN + 8, nullptr); break;
        case 'y': wcolor_set(w, COLOR_YELLOW, nullptr); break;
        case 'Y': wcolor_set(w, COLOR_YELLOW + 8, nullptr); break;
        case 'b': wcolor_set(w, COLOR_BLUE, nullptr); break;
        case 'B': wcolor_set(w, COLOR_BLUE + 8, nullptr); break;
        case 'm': wcolor_set(w, COLOR_MAGENTA, nullptr); break;
        case 'M': wcolor_set(w, COLOR_MAGENTA + 8, nullptr); break;
        case 'c': wcolor_set(w, COLOR_CYAN, nullptr); break;
        case 'C': wcolor_set(w, COLOR_CYAN + 8, nullptr); break;
        case 'w': wcolor_set(w, COLOR_WHITE, nullptr); break;
        case 'W': wcolor_set(w, COLOR_WHITE + 8, nullptr); break;
        case '!': wattr_on(w, A_REVERSE, nullptr); break;
        case '_': wattr_on(w, A_UNDERLINE, nullptr); break;
        case '0': wattr_on(w, A_NORMAL, nullptr); break;
        case '|': waddch(w, '|'); break;
        default:  break;
      }
    }
    else
    {
      waddch(w, static_cast<chtype>(static_cast<unsigned char>(s[i])));
    }
  }
}

void color_mvwaddstr(WINDOW *w, int y, int x, const std::string &s)
{
  wmove(w, y, x);
  color_waddstr(w, s);
}

void color_mvaddstr(int y, int x, const std::string &s)
{
  color_mvwaddstr(stdscr, y, x, s);
}
void enable_attr(WINDOW *w, attr_t attr) { wattrset(w, static_cast<int>(attr)); }

void colour_on()  {}
void colour_off() {}

// ---------------------------------------------------------------------------
// Right-panel layout.
// Panel columns are 0-based; screen column = panel_col + PANEL_START_COL.
// Panel data is written directly into TileBuffer at cols PANEL_START_COL+.
// This keeps a single buffer for all rendering so draw_tile_data() works
// uniformly in both PHASE_MENU (character creation) and PHASE_RUNNING.
// ---------------------------------------------------------------------------

static constexpr int PANEL_START_COL = 65;                          // first TileBuffer col of right panel
static constexpr int PC      = 0;                                   // panel col: name/HP/MP/stats
static constexpr int PCV     = 5;                                   // panel col: left value
static constexpr int PCR     = 17;                                  // panel col: right label
static constexpr int PCRV    = 24;                                  // panel col: right value
static constexpr int PCRS    = 31;                                  // panel col: status text
static constexpr int PCT     = 33;                                  // panel col: time
static constexpr int MSG_ROW  = 57;
static constexpr int MSG_ROWS = TileBuffer::H - MSG_ROW;            // 7
static constexpr int MSG_W    = TileBuffer::W - PANEL_START_COL;    // 55

// Write exactly `width` chars at (row, panel_col) into TileBuffer at screen
// column PANEL_START_COL + panel_col, padding/truncating to erase stale content.
static void panel_field(int row, int col, int width, chtype attr, const std::string &text)
{
  if(width <= 0)
    return;
  std::string s = text;
  if(static_cast<int>(s.size()) > width)
    s.resize(width);
  else
    s.append(width - static_cast<int>(s.size()), ' ');
  uint32_t       row_buf[MSG_W];
  const uint32_t a     = static_cast<uint32_t>(attr);
  const int      count = std::min(static_cast<int>(s.size()), MSG_W - col);
  if(count <= 0)
    return;
  for(int i = 0; i < count; ++i)
    row_buf[i] = static_cast<uint32_t>(static_cast<unsigned char>(s[i])) | a;
  tile_buffer().write_row(row, col + PANEL_START_COL, row_buf, count);
}

// Parse Omega's |r |G etc. colour markup and write into TileBuffer at screen
// column PANEL_START_COL + col.
static void color_panel_addstr(int row, int col, const std::string &s)
{
  uint32_t       attr = static_cast<uint32_t>(COLOR_PAIR(7));
  const size_t   len  = s.size();
  for(size_t i = 0; i < len && col < MSG_W; ++i)
  {
    if(s[i] == '|' && i + 1 < len)
    {
      ++i;
      switch(s[i])
      {
        case 'l': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_BLACK));       break;
        case 'L': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_BLACK + 8));   break;
        case 'r': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_RED));         break;
        case 'R': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_RED + 8));     break;
        case 'g': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_GREEN));       break;
        case 'G': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_GREEN + 8));   break;
        case 'y': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_YELLOW));      break;
        case 'Y': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_YELLOW + 8));  break;
        case 'b': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_BLUE));        break;
        case 'B': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_BLUE + 8));    break;
        case 'm': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_MAGENTA));     break;
        case 'M': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_MAGENTA + 8)); break;
        case 'c': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_CYAN));        break;
        case 'C': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_CYAN + 8));    break;
        case 'w': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_WHITE));       break;
        case 'W': attr = static_cast<uint32_t>(COLOR_PAIR(COLOR_WHITE + 8));   break;
        case '!': attr = static_cast<uint32_t>(A_REVERSE);                     break;
        case '_': attr = static_cast<uint32_t>(A_UNDERLINE);                   break;
        case '0': attr = static_cast<uint32_t>(COLOR_PAIR(7));                 break;
        case '|': tile_buffer().set(col++ + PANEL_START_COL, row, static_cast<uint32_t>('|') | attr); break;
        default:  break;
      }
    }
    else
    {
      tile_buffer().set(col++ + PANEL_START_COL, row,
        static_cast<uint32_t>(static_cast<unsigned char>(s[i])) | attr);
    }
  }
}

// ---------------------------------------------------------------------------
// Panel helper — stat sub-functions
// ---------------------------------------------------------------------------

static void print_name_panel()
{
  panel_field(0, PC, 33, A_BOLD,
    std::format("{}, {}", Player.name, levelname(Player.level)));
}

static void print_health_panel()
{
  int len = Player.maxhp == 0 ? 0
    : std::min(24, static_cast<int>(24.0f * Player.hp / Player.maxhp));
  panel_field(1, PC,        4,      CLR(GREY),           "HP:");
  panel_field(1, PCV,       12,     A_BOLD,              std::format("{}/{}", Player.hp, Player.maxhp));
  panel_field(1, PCR,       len,    CLR(GREEN) | A_BOLD, std::string(len, '='));
  panel_field(1, PCR + len, 24-len, CLR(GREY),           std::string(24-len, '-'));
}

static void print_mana_panel()
{
  int len = Player.maxmana == 0 ? 0
    : std::min(24, static_cast<int>(24.0f * Player.mana / Player.maxmana));
  panel_field(2, PC,        4,      CLR(GREY),          "MP:");
  panel_field(2, PCV,       12,     A_BOLD,             std::format("{}/{}", Player.mana, Player.maxmana));
  panel_field(2, PCR,       len,    CLR(BLUE) | A_BOLD, std::string(len, '='));
  panel_field(2, PCR + len, 24-len, CLR(GREY),          std::string(24-len, '-'));
}

static void print_stat_row(int row, const char *label, int val, int maxval)
{
  panel_field(row, PC,  5,  CLR(GREY), label);
  panel_field(row, PCV, 11, A_BOLD,    std::format("{}/{}", val, maxval));
}

std::string hunger_string()
{
  if(Player.food < 0)   return "Starving";
  if(Player.food <= 3)  return "Weak";
  if(Player.food <= 10) return "Ravenous";
  if(Player.food <= 20) return "Hungry";
  if(Player.food <= 30) return "Peckish";
  if(Player.food <= 36) return "Content";
  if(Player.food <= 44) return "Satiated";
  return "Bloated";
}

// ---------------------------------------------------------------------------
// Public HUD functions
// ---------------------------------------------------------------------------

void timeprint()
{
  panel_field(0, PCT, 9, A_BOLD,
    std::format("{:2d}:{:02d} {}", showhour(), showminute(), hour() > 11 ? "PM" : "AM"));
}

void dataprint()
{
  print_name_panel();
  print_health_panel();
  print_mana_panel();
  print_stat_row(3, "Str:", Player.str, Player.maxstr);
  print_stat_row(4, "Dex:", Player.dex, Player.maxdex);
  print_stat_row(5, "Con:", Player.con, Player.maxcon);
  print_stat_row(6, "Agi:", Player.agi, Player.maxagi);
  print_stat_row(7, "Int:", Player.iq,  Player.maxiq);
  print_stat_row(8, "Pow:", Player.pow, Player.maxpow);
  panel_field(9, PC,  4,  CLR(GREY), "Au:");
  panel_field(9, PCV, 11, A_BOLD,    std::format("{}", Player.cash));
}

void print_combat_stats()
{
  panel_field(3, PCR,  5,  CLR(GREY) | A_BOLD, "Hit:");
  panel_field(3, PCRV, 7,  A_BOLD,              std::format("{}", Player.hit));
  panel_field(4, PCR,  5,  CLR(GREY),           "Dmg:");
  panel_field(4, PCRV, 7,  A_BOLD,              std::format("{}", Player.dmg));
  panel_field(5, PCR,  5,  CLR(GREY),           "Def:");
  panel_field(5, PCRV, 7,  A_BOLD,              std::format("{}", Player.defense));
  panel_field(6, PCR,  5,  CLR(GREY),           "Arm:");
  panel_field(6, PCRV, 7,  A_BOLD,              std::format("{}", Player.absorption));
  panel_field(7, PCR,  5,  CLR(GREY),           "Spd:");
  panel_field(7, PCRV, 7,  A_BOLD,
    Player.speed > 0
      ? std::format("{}.{:02d}", 5 / Player.speed, 500 / Player.speed % 100)
      : "---");
  panel_field(8, PCR,  7,  CLR(GREY), "Level:");
  panel_field(8, PCRV, 9,  A_BOLD,    std::format("{}/{}", Player.level, Player.xp));
  panel_field(9, PCR,  7,  CLR(GREY), "Carry:");
  panel_field(9, PCRV, 11, A_BOLD,    std::format("{}/{}", Player.itemweight, Player.maxweight));
}

void showflags()
{
  panel_field(3, PCRS, 10, A_BOLD, hunger_string());
  panel_field(4, PCRS, 10, A_BOLD,
    Player.status[POISONED] > 0 ? "Poisoned" : "Vigorous");
  panel_field(5, PCRS, 10, A_BOLD,
    Player.status[DISEASED] > 0 ? "Diseased" : "Healthy");
  panel_field(6, PCRS, 11, A_BOLD,
    gamestatusp(MOUNTED, GameStatus) ? "Mounted"
    : Player.status[LEVITATING]      ? "Levitating"
                                      : "Afoot");
}

void locprint(const std::string &s)
{
  panel_field(11, PC, 41, A_BOLD, s);
}

void room_name_print(const std::string &s)
{
  panel_field(12, PC, 41, A_BOLD, s);
}

void print_messages()
{
  const auto &history = message_buffer.get_message_history();
  int         n       = static_cast<int>(history.size());
  int         start   = std::max(0, n - MSG_ROWS);
  for(int i = 0; i < MSG_ROWS; ++i)
  {
    panel_field(MSG_ROW + i, 0, MSG_W, COLOR_PAIR(7), ""); // clear line
    if(start + i < n)
      color_panel_addstr(MSG_ROW + i, 0, history[start + i]);
  }
}

void deathprint()         {}
void phaseprint()         {}
void print_inventory_menu(chtype) {}
void showscores()         {}

void display_bigwin()  {}
void display_death(const std::string &) {}
void display_option_slot(int) {}
void display_options() {}
void display_pack()    {}
void display_quit()    {}
void display_win()     {}

void maddch(char)           {}
void menuaddch(char)        {}
void menuclear()            {}
void menuprint(const std::string &) {}
void showmenu()             {}
void hide_line(int)         {}
void bufferprint()
{
  const std::deque<std::string> &history = message_buffer.get_message_history();
  bool                           finished = false;
  auto                           first_it = history.rbegin();
  do
  {
    werase(nullptr);
    int row = static_cast<int>(std::min(history.size(), static_cast<size_t>(LINES)));
    for(auto it = first_it; it != history.rend() && row-- > 0; ++it)
      color_mvwaddstr(nullptr, row, 0, *it);
    int input = wgetch(nullptr);
    switch(input)
    {
      case KEY_UP:
      case 16: // ^P
        if(first_it + 1 != history.rend())
          ++first_it;
        break;
      case KEY_DOWN:
      case 14: // ^N
        if(first_it != history.rbegin())
          --first_it;
        break;
      case ESCAPE:
        finished = true;
        break;
    }
  } while(!finished);
  xredraw();
}
void dobackspace()          {}
void expand_message_window()  {}
void shrink_message_window()  {}
void spreadroomdark(int, int, int)  {}
void spreadroomlight(int, int, int) {}
void omshowcursor(int, int) {}

void mouse_enable()  {}
void mouse_disable() {}

void drawomega()     {}
void omega_title()   {}
void title()         {}
