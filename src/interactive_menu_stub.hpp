#pragma once

#include "curses_stub.h"

#include <cstdint>
#include <string>
#include <vector>

// GDExtension stub for interactive_menu: stores lines and renders them to the
// tile buffer via the curses stub (mvwaddstr etc.) so menus remain visible
// until replaced by proper Godot UI scenes.
class interactive_menu
{
public:
  interactive_menu(void *, uint16_t w, uint16_t h) : width_(w), height_(h) {}

  void print()
  {
    werase(nullptr); // full clear, resets protected_rows to 0
    int row = 0;
    for(const auto &h : header_)
      mvwaddstr(nullptr, row++, 0, h.c_str());
    for(size_t i = position_; i < lines_.size() && row < 63; ++i)
      mvwaddstr(nullptr, row++, 0, lines_[i].c_str());
    // Protect the rows we just drew so clear() below us doesn't wipe them.
    set_protected_rows(row);
  }

  int get_player_input()
  {
    print();
    return wgetch(nullptr);
  }

  void load(const std::vector<std::string> &lines, const std::vector<std::string> &header)
  {
    lines_    = lines;
    header_   = header;
    position_ = 0;
  }
  void load(const std::vector<std::string> &lines)
  {
    lines_    = lines;
    header_.clear();
    position_ = 0;
  }

  void resize(uint16_t w, uint16_t h) { width_ = w; height_ = h; }
  void move(uint16_t, uint16_t) {}
  void reset_position() { position_ = 0; }

private:
  std::vector<std::string> lines_;
  std::vector<std::string> header_;
  uint16_t                 position_ = 0;
  uint16_t                 width_    = 0;
  uint16_t                 height_   = 0;
};
