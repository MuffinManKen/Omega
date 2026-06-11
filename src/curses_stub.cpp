// No-op and tile-buffer implementations of curses functions for the
// GDExtension build.
//
// Text output functions (mvwaddstr, wprintw, etc.) write characters into the
// shared TileBuffer so Godot's _draw() can display them.  This makes the
// title menu and other curses UI phases visible without a separate text layer.
//
// wgetch/getch route through InputQueue so the game thread's blocking input
// calls work correctly.

#include "curses_stub.h"
#include "input_queue.hpp"
#include "render_buffer.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

WINDOW *stdscr     = nullptr;
int     COLOR_PAIRS = 256;
int     COLORS      = 256;
int     COLS        = 80;
int     LINES       = 24;

// Current text cursor and attribute, shared across all stub windows.
static int    cursor_y     = 0;
static int    cursor_x     = 0;
static attr_t current_attr = COLOR_PAIR(7); // default: white on black

// Rows 0..protected_rows-1 belong to an overlay (e.g. interactive_menu) and
// are not wiped by clear()/erase().  werase() always resets this to 0.
static int protected_rows = 0;

void set_protected_rows(int n) { protected_rows = n; }
int  get_protected_rows()      { return protected_rows; }

// ---------------------------------------------------------------------------
// Internal helper: write a C string into the tile buffer at (x, y).
// Advances the shared cursor.  Characters outside the buffer are clipped.
// ---------------------------------------------------------------------------

// Accumulates characters into a row buffer and flushes with write_row() —
// one mutex acquisition per row rather than one per character.
static void put_chars(int y, int x, const char *str, int len = -1)
{
  if(!str)
    return;
  if(len < 0)
    len = static_cast<int>(strlen(str));

  // Staging buffer for the current row; flushed on newline or row change.
  uint32_t row_buf[TileBuffer::W];
  int      row_start = x;
  int      row_count = 0;

  auto flush_row = [&]() {
    if(row_count > 0)
    {
      tile_buffer().write_row(y, row_start, row_buf, row_count);
      row_count = 0;
    }
  };

  while(len-- > 0)
  {
    unsigned char ch = static_cast<unsigned char>(*str++);
    if(ch == '\n')
    {
      flush_row();
      ++y;
      x         = 0;
      row_start = 0;
    }
    else if(x >= 0 && y >= 0 && x < TileBuffer::W && y < TileBuffer::H)
    {
      if(row_count == 0)
        row_start = x;
      row_buf[row_count++] = static_cast<uint32_t>(ch) | static_cast<uint32_t>(current_attr);
      ++x;
    }
  }
  flush_row();

  cursor_y = y;
  cursor_x = x;
}

// ---------------------------------------------------------------------------
// Screen lifecycle
// ---------------------------------------------------------------------------

int initscr()  { return 0; }
int endwin()   { InputQueue::instance().shutdown(); return 0; }
int doupdate() { return 0; }
int refresh()  { return 0; }
int noecho()   { return 0; }
int raw()      { return 0; }
int cbreak()   { return 0; }

// Fill rows start_y..H-1 with spaces. One mutex acquisition.
static void clear_from(int start_y)
{
  tile_buffer().fill_from(start_y, static_cast<uint32_t>(' ') | (7u << 24));
  cursor_y = start_y;
  cursor_x = 0;
}

int clear()
{
  clear_from(protected_rows);
  return 0;
}
int erase()
{
  clear_from(protected_rows);
  return 0;
}

// ---------------------------------------------------------------------------
// Window operations
// ---------------------------------------------------------------------------

int werase(WINDOW *)
{
  // Full clear — resets protected zone so no overlay survives.
  protected_rows = 0;
  clear_from(0);
  return 0;
}
int wrefresh(WINDOW *)     { return 0; }
int wnoutrefresh(WINDOW *) { return 0; }
int wclear(WINDOW *)
{
  // Same semantics as werase for our flat-buffer implementation.
  protected_rows = 0;
  clear_from(0);
  return 0;
}
int touchwin(WINDOW *)       { return 0; }
int wclrtoeol(WINDOW *)      { return 0; }
int wclrtobot(WINDOW *)      { return 0; }
int scrollok(WINDOW *, bool) { return 0; }
int idlok(WINDOW *, bool)    { return 0; }
int leaveok(WINDOW *, bool)  { return 0; }
int keypad(WINDOW *, bool)   { return 0; }
int nodelay(WINDOW *, bool)  { return 0; }
int wmove(WINDOW *, int y, int x) { cursor_y = y; cursor_x = x; return 0; }
int getcury(WINDOW *)  { return cursor_y; }
int getmaxy(WINDOW *)  { return TileBuffer::H; }
int getmaxx(WINDOW *)  { return TileBuffer::W; }
int curs_set(int)      { return 0; }

WINDOW *newwin(int, int, int, int)           { return nullptr; }
WINDOW *subwin(WINDOW *, int, int, int, int) { return nullptr; }
WINDOW *derwin(WINDOW *, int, int, int, int) { return nullptr; }
int     delwin(WINDOW *)                     { return 0; }

// ---------------------------------------------------------------------------
// Attribute control — track current_attr so color changes affect output
// ---------------------------------------------------------------------------

int wattr_on(WINDOW *, attr_t attr, void *)  { current_attr = attr; return 0; }
int wattr_off(WINDOW *, attr_t, void *)      { current_attr = COLOR_PAIR(7); return 0; }
int wattrset(WINDOW *, int attr)             { current_attr = attr ? static_cast<attr_t>(attr) : static_cast<attr_t>(COLOR_PAIR(7)); return 0; }
int wcolor_set(WINDOW *, short pair, void *) { current_attr = COLOR_PAIR(pair); return 0; }
int attron(int attr)                         { current_attr = static_cast<attr_t>(attr); return 0; }
int attroff(int)                             { current_attr = COLOR_PAIR(7); return 0; }
int standout()                               { current_attr = A_STANDOUT; return 0; }
int standend()                               { current_attr = COLOR_PAIR(7); return 0; }

// ---------------------------------------------------------------------------
// Output — write characters into the tile buffer
// ---------------------------------------------------------------------------

int waddch(WINDOW *, chtype ch)
{
  char c = static_cast<char>(ch & 0xFF);
  put_chars(cursor_y, cursor_x, &c, 1);
  return 0;
}
int mvwaddch(WINDOW *, int y, int x, chtype ch)
{
  char c = static_cast<char>(ch & 0xFF);
  put_chars(y, x, &c, 1);
  return 0;
}
int waddstr(WINDOW *, const char *str)
{
  put_chars(cursor_y, cursor_x, str);
  return 0;
}
int mvwaddstr(WINDOW *, int y, int x, const char *str)
{
  put_chars(y, x, str);
  return 0;
}
int waddnstr(WINDOW *, const char *str, int n)
{
  put_chars(cursor_y, cursor_x, str, n);
  return 0;
}
int mvwaddnstr(WINDOW *, int y, int x, const char *str, int n)
{
  put_chars(y, x, str, n);
  return 0;
}
int addch(chtype ch)
{
  char c = static_cast<char>(ch & 0xFF);
  put_chars(cursor_y, cursor_x, &c, 1);
  return 0;
}
int mvaddstr(int y, int x, const char *str)
{
  put_chars(y, x, str);
  return 0;
}
int color_mvaddstr(int y, int x, const char *str)
{
  put_chars(y, x, str);
  return 0;
}
int addstr(const char *str)
{
  put_chars(cursor_y, cursor_x, str);
  return 0;
}

int wprintw(WINDOW *, const char *fmt, ...)
{
  char    buf[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  put_chars(cursor_y, cursor_x, buf);
  return 0;
}
int mvwprintw(WINDOW *, int y, int x, const char *fmt, ...)
{
  char    buf[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  put_chars(y, x, buf);
  return 0;
}
int printw(const char *fmt, ...)
{
  char    buf[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  put_chars(cursor_y, cursor_x, buf);
  return 0;
}

// ---------------------------------------------------------------------------
// Mouse — no-ops
// ---------------------------------------------------------------------------

int mousemask(unsigned long, unsigned long *) { return 0; }
int mouseinterval(int)                        { return 0; }
int getmouse(MEVENT *)                        { return -1; }
int ungetmouse(MEVENT *)                      { return 0; }

// ---------------------------------------------------------------------------
// Misc output — no-ops
// ---------------------------------------------------------------------------

int wresize(WINDOW *, int, int)  { return 0; }
int wtimeout(WINDOW *, int)      { return 0; }
int set_escdelay(int)            { return 0; }

// ---------------------------------------------------------------------------
// Color initialization
// ---------------------------------------------------------------------------

int  start_color()                  { return 0; }
int  init_pair(short, short, short) { return 0; }
bool has_colors()                   { return false; }

// ---------------------------------------------------------------------------
// Input — routes through InputQueue so the blocking game thread
// receives keys pushed by Godot's input callbacks
// ---------------------------------------------------------------------------

int wgetch(WINDOW *)             { return InputQueue::instance().pop(); }
int getch()                      { return InputQueue::instance().pop(); }
int mvwgetch(WINDOW *, int, int) { return InputQueue::instance().pop(); }
int ungetch(int ch)              { InputQueue::instance().push(ch); return 0; }
