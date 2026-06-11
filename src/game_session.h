#pragma once

// Outcome of init_game_session(): Failed if data files are missing or
// initialization otherwise fails; NewGame for a fresh character;
// Continued for a restored save.
enum class InitResult
{
  Failed,
  NewGame,
  Continued
};

// Initializes the game world and rendering. Returns once the world is set up
// and the first screen has been drawn; the first turn has NOT yet run.
InitResult init_game_session();

// Runs the main game loop. Blocks until the player quits.
// Pass reset_clock = true for a new game (resets Tick/Player.click on the
// first turn). In the GDExtension build this is called on a background thread.
void run_game_loop(bool reset_clock);
