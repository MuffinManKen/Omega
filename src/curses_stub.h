// Minimal curses type and constant definitions for the GDExtension build.
// Provides the same chtype bit layout as PDCurses wincon for save compatibility.
// Function declarations are implemented as no-ops in curses_stub.cpp,
// except wgetch/getch which route through InputQueue.
#pragma once

#include <cstdarg>
#include <cstdint>

// ---- Types ----------------------------------------------------------------

typedef unsigned long chtype;
typedef unsigned long attr_t;

// WINDOW is opaque in core code — only ever held as a pointer and passed to
// scr.h functions which stub them out.
typedef struct _omega_stub_window WINDOW;

// ---- Attribute bits (PDCurses wincon layout) ------------------------------

#define A_NORMAL      0x00000000UL
#define A_ALTCHARSET  0x00010000UL
#define A_RIGHTLINE   0x00020000UL
#define A_LEFTLINE    0x00040000UL
#define A_ITALIC      0x00080000UL
#define A_UNDERLINE   0x00100000UL
#define A_REVERSE     0x00200000UL
#define A_BLINK       0x00400000UL
#define A_BOLD        0x00800000UL
#define A_STANDOUT    (A_REVERSE | A_BOLD)
#define A_DIM         0x00000000UL
#define A_INVIS       0x08000000UL
#define A_PROTECT     0x00000000UL
#define A_CHARTEXT    0x0000FFFFUL
#define A_ATTRIBUTES  (~A_CHARTEXT)
#define A_COLOR       0xFF000000UL

#define COLOR_PAIR(n)   ((chtype)(n) << 24)
#define PAIR_NUMBER(n)  (((n) & A_COLOR) >> 24)

// ---- Color indices --------------------------------------------------------

#define COLOR_BLACK    0
#define COLOR_RED      1
#define COLOR_GREEN    2
#define COLOR_YELLOW   3
#define COLOR_BLUE     4
#define COLOR_MAGENTA  5
#define COLOR_CYAN     6
#define COLOR_WHITE    7

// ---- Key codes (PDCurses values) -----------------------------------------
// ESCAPE and DELETE are already constexpr ints in defs.h — not redefined here.

#define KEY_BREAK      0x101
#define KEY_DOWN       0x102
#define KEY_UP         0x103
#define KEY_LEFT       0x104
#define KEY_RIGHT      0x105
#define KEY_HOME       0x106
#define KEY_BACKSPACE  0x107
#define KEY_DC         0x14A
#define KEY_LL         0x14F
#define KEY_NPAGE      0x152
#define KEY_PPAGE      0x153
#define KEY_ENTER      0x157
#define KEY_END        0x166
#define KEY_RESIZE     0x222
#define KEY_MOUSE      0x21b

// ---- Mouse button event masks (PDCurses values, needed by scr.h) ---------

#define BUTTON1_RELEASED        0x00000001L
#define BUTTON1_PRESSED         0x00000002L
#define BUTTON1_CLICKED         0x00000004L
#define BUTTON1_DOUBLE_CLICKED  0x00000008L
#define BUTTON1_TRIPLE_CLICKED  0x00000010L
#define BUTTON2_RELEASED        0x00000040L
#define BUTTON2_PRESSED         0x00000080L
#define BUTTON2_CLICKED         0x00000100L
#define BUTTON2_DOUBLE_CLICKED  0x00000200L
#define BUTTON2_TRIPLE_CLICKED  0x00000400L
#define BUTTON3_RELEASED        0x00001000L
#define BUTTON3_PRESSED         0x00002000L
#define BUTTON3_CLICKED         0x00004000L
#define BUTTON3_DOUBLE_CLICKED  0x00008000L
#define BUTTON3_TRIPLE_CLICKED  0x00010000L
#define BUTTON4_RELEASED        0x00040000L
#define BUTTON4_PRESSED         0x00080000L
#define BUTTON4_CLICKED         0x00100000L
#define BUTTON4_DOUBLE_CLICKED  0x00200000L
#define BUTTON4_TRIPLE_CLICKED  0x00400000L
#define BUTTON5_RELEASED        0x04000000L
#define BUTTON5_PRESSED         0x08000000L
#define BUTTON5_CLICKED         0x02000000L
#define BUTTON5_DOUBLE_CLICKED  0x01000000L
#define BUTTON5_TRIPLE_CLICKED  0x00800000L
#define ALL_MOUSE_EVENTS        0xFFFFFFFFL

typedef struct { int x, y; unsigned long bstate; } MEVENT;

// ---- Protected row zone --------------------------------------------------
// set_protected_rows(n) marks rows 0..n-1 as belonging to an overlay window
// (e.g. interactive_menu).  clear()/erase() only wipe rows n..H-1 and set
// the cursor to row n, so text printed afterwards appears below the overlay.
// werase() always does a full clear and resets this to 0.
void set_protected_rows(int n);
int  get_protected_rows();

// ---- Curses globals ------------------------------------------------------

extern WINDOW *stdscr;
extern int     COLOR_PAIRS;
extern int     COLORS;
extern int     COLS;
extern int     LINES;

// ---- Function declarations -----------------------------------------------

// Screen lifecycle
int  initscr();
int  endwin();
int  doupdate();
int  refresh();
int  clear();
int  erase();
int  noecho();
int  raw();
int  cbreak();

// Window operations
int  werase(WINDOW *win);
int  wrefresh(WINDOW *win);
int  wnoutrefresh(WINDOW *win);
int  wclear(WINDOW *win);
int  touchwin(WINDOW *win);
int  wclrtoeol(WINDOW *win);
int  wclrtobot(WINDOW *win);
int  scrollok(WINDOW *win, bool bf);
int  idlok(WINDOW *win, bool bf);
int  leaveok(WINDOW *win, bool bf);
int  keypad(WINDOW *win, bool bf);
int  nodelay(WINDOW *win, bool bf);
int  wmove(WINDOW *win, int y, int x);
int  getcury(WINDOW *win);
int  getmaxy(WINDOW *win);
int  getmaxx(WINDOW *win);
int  curs_set(int visibility);

WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x);
WINDOW *subwin(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x);
WINDOW *derwin(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x);
int     delwin(WINDOW *win);

// Attribute control
int  wattr_on(WINDOW *win, attr_t attrs, void *opts);
int  wattr_off(WINDOW *win, attr_t attrs, void *opts);
int  attron(int attrs);
int  attroff(int attrs);
int  wattrset(WINDOW *win, int attrs);
int  wcolor_set(WINDOW *win, short color_pair_number, void *opts);
int  standout();
int  standend();

// Output
int  waddch(WINDOW *win, chtype ch);
int  mvwaddch(WINDOW *win, int y, int x, chtype ch);
int  waddstr(WINDOW *win, const char *str);
int  mvwaddstr(WINDOW *win, int y, int x, const char *str);
int  waddnstr(WINDOW *win, const char *str, int n);
int  mvwaddnstr(WINDOW *win, int y, int x, const char *str, int n);
int  addch(chtype ch);
int  mvaddstr(int y, int x, const char *str);
int  color_mvaddstr(int y, int x, const char *str);
int  wprintw(WINDOW *win, const char *fmt, ...);
int  mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...);
int  printw(const char *fmt, ...);

// Input — implemented in curses_stub.cpp to route through InputQueue
int  wgetch(WINDOW *win);
int  getch();
int  ungetch(int ch);      // push back to InputQueue
int  mvwgetch(WINDOW *win, int y, int x);
int  mousemask(unsigned long newmask, unsigned long *oldmask);
int  mouseinterval(int erval);
int  getmouse(MEVENT *event);
int  ungetmouse(MEVENT *event);

// Additional output
int  addstr(const char *str);
int  wresize(WINDOW *win, int nlines, int ncols);
int  wtimeout(WINDOW *win, int delay);
int  set_escdelay(int ms);

// Color initialization
int  start_color();
int  init_pair(short pair, short fg, short bg);
bool has_colors();
