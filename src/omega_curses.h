#pragma once

// Single point of indirection for the curses API. Alternative front-ends
// (e.g. a graphical port) define OMEGA_CURSES_STUB and supply curses_stub.h,
// which provides the curses types, constants, and function signatures while
// routing output to the replacement renderer. The standard build includes
// the real curses header.
#ifdef OMEGA_CURSES_STUB
#  include "curses_stub.h"
#else
#  include <curses.h>
#endif
