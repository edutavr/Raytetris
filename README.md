# <img src="https://i.ibb.co/JW0gxs61/Sem-T-tulo-1.png" width="50"> RAYBLOCKS<img src="https://i.ibb.co/JW0gxs61/Sem-T-tulo-1.png" width="50">


RayBlocks is a Tetris-inspired game built in C using raylib.
It combines classic arcade gameplay with a minimalist interface.

Programmed by Eduardo Rocha (edutavr)</br>
OST made by Victor Severo de Oliveira

## Requirements

- C Compiler (GCC / Clang recommended)
- Raylib installed
- POSIX-compatible shell (for `build.sh`)
  
To run the code, you have to download [Raylib](https://www.raylib.com/). Once you've downloaded it, just follow the instructions during the installation.

After installing(/If you already have) raylib in your computer, open w64devkit, then locate the folder where you cloned this repository:

(example)
```bash
cd Rayblocks
```
Now, to compile, write
```bash
./build.sh
```

## Game Settings

### Controls

Arrow Keys → Move

Up → Rotate CW

X → Rotate Counter CW

P → Pause Game

Down → Soft drop

Space → Hard drop

F11 → Toggle FullScreen Mode

### Mechanics

- **Gravity System** → Pieces fall automatically. Speed increases as the level goes up.
- **Line Clear** → Completing a horizontal line clears it and shifts everything above downward.
- **Level Progression** → Every 10 cleared lines increases the level.
- **Score System** → Points are awarded based on:
  - Single line clear
  - Double line clear
  - Triple line clear
  - Tetris (4 lines)
- **Back-to-Back Bonus** → Consecutive Tetrises grant extra score.
- **Combo System** → Clearing lines consecutively without failing increases combo bonus.
- **Soft Drop Scoring** → Grants small score per cell dropped.
- **Hard Drop Scoring** → Grants higher score per cell instantly dropped.
- **Danger Zone Music** → Music switches to a faster version when the board is near the top.
- **Start Level Selection** → Choose the initial difficulty before starting.
- **Custom Keybinds** → Rebind keyboard and gamepad controls in the Settings menu.
- **Leaderboard System** → Saves top scores locally.

---

## Audio

RayBlocks features:

- Dynamic background music (normal & fast version)
- Menu theme
- Line clear sound effects
- Tetris sound effect
- UI tick sound
- Game Over sound

---

## Visual Features

- Multiple color themes
- Line clear blinking animation
- Pause overlay
- Game Over overlay with name input
- Responsive UI highlighting

## Requirements

- C Compiler (GCC / Clang recommended)
- Raylib installed
- POSIX-compatible shell (for `build.sh`)
