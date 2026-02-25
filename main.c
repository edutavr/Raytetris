/* Programmed by edutavr */

#include "raylib.h"
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "icon_data.h"

/* ===================== CONFIG ===================== */

#define COLS 12
#define ROWS 21
#define BOARD_X_AXIS 50
#define BOARD_Y_AXIS 70
#define SQUARE_SIZE 24
#define LEFT  -1
#define RIGHT  1
#define SPAWN_DELAY_FRAMES 15
#define PAGE_SIZE 10
#define MAX_SCORES 200
#define MAX_NAME_LEN 16
#define LEADERBOARD_FILE "leaderboard.dat"
#define KEYBINDS_FILE    "keybinds.dat"
#define KEYBIND_COUNT    7
//#define GAMEPAD_ID       0
#define MIN_START_LEVEL 1
#define MAX_START_LEVEL 19

/* ===================== TYPES ===================== */

typedef enum CellState {
  EMPTY, MOVING_PIECE, PLACED_PIECE, CLEAN_LINE, BOARD_LIMIT
} CellState;

typedef enum PiecesFormat {
  I, O, T, S, Z, J, L, TETROMINO_COUNT
} PiecesFormat;

typedef enum MainMenu {
  MAINSCREEN = 0, GAMEPLAY, SCORES, SETTINGS
} MainMenu;

typedef enum ThemeOptions {
  PURPLE_THEME, RED_THEME, GREEN_THEME, BLUE_THEME, YELLOW_THEME, ORANGE_THEME, PINK_THEME, THEME_COUNT
} ThemeOptions;

typedef struct ThemeColors {
  Color background;
  Color text;
  Color highlight;
  const char *name;
} ThemeColors;

typedef struct ActivePiece {
  PiecesFormat type;
  int rot;
  int x;
  int y;
} ActivePiece;

typedef struct ScoreEntry {
  char name[MAX_NAME_LEN];
  int value;
} ScoreEntry;

typedef enum GameOverFlow {
  GO_ASK_SAVE = 0,
  GO_ENTER_NAME,
  GO_SHOW_GAMEOVER
} GameOverFlow;

/* --- Keybinds: keyboard + gamepad button per action --- */
typedef struct Binding {
  KeyboardKey key;
  int         gpButton; /* -1 = not set */
} Binding;

typedef struct Keybinds {
  Binding moveLeft;
  Binding moveRight;
  Binding softDrop;
  Binding hardDrop;
  Binding rotateCW;
  Binding rotateCCW;
  Binding pause;
} Keybinds;

typedef enum SettingsFlow {
  SF_IDLE = 0,
  SF_WAITING_KEY,
  SF_WAITING_GP
} SettingsFlow;

/* ===================== GLOBAL STATE ===================== */

static CellState grid[COLS][ROWS];

static float holdLeftTime  = 0.0f;
static float holdRightTime = 0.0f;
static int   spawnDelayFrames = 0;

static const float DAS = 0.15f;
static const float ARR = 0.05f;

static bool itsOver = false;

static int frameCounter = 0;
static int scrollSpeed  = 1;
static int startLevel = 1;
static bool prevHoverLevel = false;
static bool gamePaused = false;
static float pauseCooldown = 0.0f;
static bool prevHoverMute = false;
static bool hMute = false;

static int score        = 0;
static int linesCleared = 0;
static int level        = 1;

static int  combo     = -1;
static bool backToBack = false;

static PiecesFormat nextType;
static ActivePiece  cur;
static bool         pieceActive = false;

static GameOverFlow goFlow    = GO_SHOW_GAMEOVER;
static char         nameInput[MAX_NAME_LEN] = {0};
static int          nameLen = 0;

static ScoreEntry leaderboard[MAX_SCORES];
static int        leaderboardCount = 0;
static int        scoresPage       = 0;

/* --- Audio --- */
static Music musicNormal;
static Music musicFast;
static Music musicMenu;
static float menuMusicDelay  = 0.0f;
static bool  audioReady      = false;
static bool  playingFast     = false;
static bool musicMuted = false;
static Sound sfxLineClear;
static Sound sfxTetris;
static Sound sfxTick;
static bool  sfxLineClearReady = false;
static bool  sfxTickReady      = false;
static Sound sfxGameOver;
static bool  sfxGameOverReady = false;

/* --- Soft drop --- */
static float holdDownTime = 0.0f;
static bool  downBlocked  = false;
static const float SD_DAS = 0.00f;
static const float SD_ARR = 0.03f;

/* --- Line clear animation --- */
static bool clearingLines    = false;
static int  clearTimerFrames = 0;
#define LINE_CLEAR_DELAY_FRAMES 20
#define LINE_CLEAR_BLINK_EVERY   6
static int  blinkFrameCounter  = 0;
static bool blinkOn            = false;
static int  linesToClear[4];
static int  linesToClearCount  = 0;

/* --- Keybinds --- */
static Keybinds keys = {
  /* moveLeft   */ { KEY_LEFT,  -1 },
  /* moveRight  */ { KEY_RIGHT, -1 },
  /* softDrop   */ { KEY_DOWN,  -1 },
  /* hardDrop   */ { KEY_SPACE, -1 },
  /* rotateCW   */ { KEY_UP,    -1 },
  /* rotateCCW  */ { KEY_X,     -1 },
  /* pause */      { KEY_P,     -1 },
};

static SettingsFlow settingsFlow   = SF_IDLE;
static int          rebindingIndex = -1;
static bool         rebindingGP    = false; /* true = waiting gamepad, false = waiting keyboard */

static const char *keybindLabels[KEYBIND_COUNT] = {
  "Move Left", "Move Right", "Soft Drop", "Hard Drop", "Rotate CW", "Rotate CCW", "Pause"
};

/* --- Hover tracking for tick sfx --- */
static bool prevHoverPlay      = false;
static bool prevHoverScoresBtn = false;
static bool prevHoverSettings  = false;
static bool prevHoverBack      = false;
static bool prevHoverYes       = false;
static bool prevHoverNo        = false;
static bool prevHoverInput     = false;
static bool prevHoverPrev      = false;
static bool prevHoverNext      = false;
static bool prevHoverClear[PAGE_SIZE];
static bool prevHoverKbBtns[KEYBIND_COUNT];
static bool prevHoverGpBtns[KEYBIND_COUNT];
static bool prevHoverReset     = false;

/* ===================== SHAPES ===================== */

static const int SHAPES[TETROMINO_COUNT][4][4][2] = {
  /* I */
  {
    {{-1,0},{0,0},{1,0},{2,0}},
    {{1,-1},{1,0},{1,1},{1,2}},
    {{-1,1},{0,1},{1,1},{2,1}},
    {{0,-1},{0,0},{0,1},{0,2}},
  },
  /* O */
  {
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
  },
  /* T */
  {
    {{-1,0},{0,0},{1,0},{0,1}},
    {{0,-1},{0,0},{0,1},{1,0}},
    {{-1,0},{0,0},{1,0},{0,-1}},
    {{0,-1},{0,0},{0,1},{-1,0}},
  },
  /* S */
  {
    {{0,0},{1,0},{-1,1},{0,1}},
    {{0,-1},{0,0},{1,0},{1,1}},
    {{0,0},{1,0},{-1,1},{0,1}},
    {{0,-1},{0,0},{1,0},{1,1}},
  },
  /* Z */
  {
    {{-1,0},{0,0},{0,1},{1,1}},
    {{1,-1},{0,0},{1,0},{0,1}},
    {{-1,0},{0,0},{0,1},{1,1}},
    {{1,-1},{0,0},{1,0},{0,1}},
  },
  /* J */
  {
    {{-1,0},{0,0},{1,0},{-1,1}},
    {{0,-1},{0,0},{0,1},{1,1}},
    {{-1,0},{0,0},{1,0},{1,-1}},
    {{0,-1},{0,0},{0,1},{-1,-1}},
  },
  /* L */
  {
    {{-1,0},{0,0},{1,0},{1,1}},
    {{0,-1},{0,0},{0,1},{1,-1}},
    {{-1,0},{0,0},{1,0},{-1,-1}},
    {{0,-1},{0,0},{0,1},{-1,1}},
  },
};

/* ===================== COLOR HELPERS ===================== */

static Color Mix(Color a, Color b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return (Color){
    (unsigned char)(a.r + (b.r - a.r) * t),
    (unsigned char)(a.g + (b.g - a.g) * t),
    (unsigned char)(a.b + (b.b - a.b) * t),
    255
  };
}

/* ===================== LEADERBOARD ===================== */

static void SortLeaderboard(void) {
  for (int i = 0; i < leaderboardCount - 1; i++)
    for (int j = 0; j < leaderboardCount - 1 - i; j++)
      if (leaderboard[j].value < leaderboard[j+1].value) {
        ScoreEntry tmp = leaderboard[j];
        leaderboard[j] = leaderboard[j+1];
        leaderboard[j+1] = tmp;
      }
}

static void SaveLeaderboardToFile(void) {
  FILE *f = fopen(LEADERBOARD_FILE, "wb");
  if (!f) return;
  fwrite(&leaderboardCount, sizeof(int), 1, f);
  fwrite(leaderboard, sizeof(ScoreEntry), (size_t)leaderboardCount, f);
  fclose(f);
}

static void LoadLeaderboardFromFile(void) {
  FILE *f = fopen(LEADERBOARD_FILE, "rb");
  if (!f) { leaderboardCount = 0; return; }
  int count = 0;
  if (fread(&count, sizeof(int), 1, f) != 1) { fclose(f); leaderboardCount = 0; return; }
  if (count < 0) count = 0;
  if (count > MAX_SCORES) count = MAX_SCORES;
  int readN = (int)fread(leaderboard, sizeof(ScoreEntry), (size_t)count, f);
  fclose(f);
  leaderboardCount = readN;
  for (int i = 0; i < leaderboardCount; i++)
    leaderboard[i].name[MAX_NAME_LEN-1] = '\0';
  SortLeaderboard();
}

static void AddScoreToLeaderboard(const char *name, int value) {
  if (leaderboardCount < MAX_SCORES) {
    ScoreEntry e = {0};
    strncpy(e.name, name, MAX_NAME_LEN-1);
    e.name[MAX_NAME_LEN-1] = '\0';
    e.value = value;
    leaderboard[leaderboardCount++] = e;
  } else {
    SortLeaderboard();
    if (leaderboard[leaderboardCount-1].value < value) {
      strncpy(leaderboard[leaderboardCount-1].name, name, MAX_NAME_LEN-1);
      leaderboard[leaderboardCount-1].name[MAX_NAME_LEN-1] = '\0';
      leaderboard[leaderboardCount-1].value = value;
    }
  }
  SortLeaderboard();
  SaveLeaderboardToFile();
}

static void RemoveScoreAtIndex(int idx) {
  if (idx < 0 || idx >= leaderboardCount) return;
  for (int i = idx; i < leaderboardCount-1; i++)
    leaderboard[i] = leaderboard[i+1];
  leaderboardCount--;
  int maxPage = (leaderboardCount <= 0) ? 0 : (leaderboardCount-1) / PAGE_SIZE;
  if (scoresPage > maxPage) scoresPage = maxPage;
  SaveLeaderboardToFile();
}

/* ===================== KEYBINDS SAVE/LOAD ===================== */

static void SaveKeybinds(void) {
  FILE *f = fopen(KEYBINDS_FILE, "wb");
  if (!f) return;
  fwrite(&keys, sizeof(Keybinds), 1, f);
  fclose(f);
}

static void LoadKeybinds(void) {
  FILE *f = fopen(KEYBINDS_FILE, "rb");
  if (!f) return;
  fread(&keys, sizeof(Keybinds), 1, f);
  fclose(f);
}

/* ===================== KEYBIND HELPERS ===================== */

static int GetActiveGamepadId(void) {
  for (int i = 0; i < 4; i++) {              // Raylib costuma suportar 4
    if (IsGamepadAvailable(i)) return i;
  }
  return -1;
}

static Binding *BindingByIndex(int i) {
  switch (i) {
  case 0: return &keys.moveLeft;
  case 1: return &keys.moveRight;
  case 2: return &keys.softDrop;
  case 3: return &keys.hardDrop;
  case 4: return &keys.rotateCW;
  case 5: return &keys.rotateCCW;
  case 6: return &keys.pause;
  default: return NULL;
  }
}

static const char *KeyName(KeyboardKey k) {
  switch (k) {
    case KEY_LEFT:         return "LEFT";
    case KEY_RIGHT:        return "RIGHT";
    case KEY_UP:           return "UP";
    case KEY_DOWN:         return "DOWN";
    case KEY_SPACE:        return "SPACE";
    case KEY_ENTER:        return "ENTER";
    case KEY_LEFT_SHIFT:   return "L.SHIFT";
    case KEY_RIGHT_SHIFT:  return "R.SHIFT";
    case KEY_LEFT_CONTROL: return "L.CTRL";
    case KEY_ESCAPE:       return "ESC";
    case KEY_BACKSPACE:    return "BKSP";
    case KEY_TAB:          return "TAB";
    case KEY_NULL:         return "---";
    default: {
      if (k >= KEY_A && k <= KEY_Z)
        return TextFormat("%c", 'A' + (k - KEY_A));
      if (k >= KEY_ZERO && k <= KEY_NINE)
        return TextFormat("%c", '0' + (k - KEY_ZERO));
      return "???";
    }
  }
}

static const char *GPButtonName(int btn) {
  if (btn < 0) return "---";
  switch (btn) {
    case GAMEPAD_BUTTON_LEFT_FACE_UP:     return "GP UP";
    case GAMEPAD_BUTTON_LEFT_FACE_DOWN:   return "GP DOWN";
    case GAMEPAD_BUTTON_LEFT_FACE_LEFT:   return "GP LEFT";
    case GAMEPAD_BUTTON_LEFT_FACE_RIGHT:  return "GP RIGHT";
    case GAMEPAD_BUTTON_RIGHT_FACE_DOWN:  return "GP A";
    case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return "GP B";
    case GAMEPAD_BUTTON_RIGHT_FACE_LEFT:  return "GP X";
    case GAMEPAD_BUTTON_RIGHT_FACE_UP:    return "GP Y";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_1:   return "GP LB";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_1:  return "GP RB";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_2:   return "GP LT";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_2:  return "GP RT";
    case GAMEPAD_BUTTON_MIDDLE_LEFT:      return "GP SEL";
    case GAMEPAD_BUTTON_MIDDLE_RIGHT:     return "GP STR";
    default: return TextFormat("GP %d", btn);
  }
}

/* Check press/hold for a binding (keyboard OR gamepad) */
static bool BindingDown(Binding b) {
  if (IsKeyDown(b.key)) return true;

  int gid = GetActiveGamepadId();
  if (b.gpButton >= 0 && gid >= 0 && IsGamepadButtonDown(gid, b.gpButton)) return true;

  return false;
}


static bool BindingPressed(Binding b) {
  if (IsKeyPressed(b.key)) return true;

  int gid = GetActiveGamepadId();
  if (b.gpButton >= 0 && gid >= 0 && IsGamepadButtonPressed(gid, b.gpButton)) return true;

  return false;
}




/* ===================== AUDIO ===================== */

static void PlayTick(void) {
  if (sfxTickReady) PlaySound(sfxTick);
}

static void InitGameAudio(void) {
  InitAudioDevice();

  sfxLineClear = LoadSound("songs/lineclear.ogg");
  SetSoundVolume(sfxLineClear, 1.0f);
  sfxTetris = LoadSound("songs/tetris.ogg");
  SetSoundVolume(sfxTetris, 0.9f);
  sfxLineClearReady = true;

  sfxTick = LoadSound("songs/tick.wav");
  SetSoundVolume(sfxTick, 0.6f);
  sfxTickReady = true;

  sfxGameOver = LoadSound("songs/Sound-Effect-Game-Over.ogg");
  SetSoundVolume(sfxGameOver, 0.9f);
 sfxGameOverReady = true;

  musicNormal = LoadMusicStream("songs/Victor-Severo-Raytetris.ogg");
  musicFast   = LoadMusicStream("songs/Victor-Severo-Raytetris-Brutal.ogg");
  musicMenu   = LoadMusicStream("songs/Victor-Severo-Raytetris-MENU.ogg");

  SetMusicVolume(musicNormal, 0.50f);
  SetMusicVolume(musicFast,   0.40f);
  SetMusicVolume(musicMenu,   0.20f);

  audioReady  = true;
  playingFast = false;
}

static void UnloadGameAudio(void) {
  if (!audioReady) return;
  StopMusicStream(musicNormal);
  StopMusicStream(musicFast);
  StopMusicStream(musicMenu);
  UnloadMusicStream(musicNormal);
  UnloadMusicStream(musicFast);
  UnloadMusicStream(musicMenu);
  if (sfxLineClearReady) {
    UnloadSound(sfxLineClear);
    UnloadSound(sfxTetris);
    sfxLineClearReady = false;
  }
  if (sfxTickReady) {
    UnloadSound(sfxTick);
    sfxTickReady = false;
  }
  if (sfxGameOverReady) {
    UnloadSound(sfxGameOver);
    sfxGameOverReady = false;
  }
  CloseAudioDevice();
  audioReady = false;
}

static bool IsDangerZone(void) {
  for (int y = 0; y <= 6; y++)
    for (int x = 1; x < COLS-1; x++)
      if (grid[x][y] == PLACED_PIECE) return true;
  return false;
}

static void StartGameplayMusic(void) {
  if (!audioReady) return;
  StopMusicStream(musicFast);
  playingFast = false;
  PlayMusicStream(musicNormal);
}

static void StopGameplayMusic(void) {
  if (!audioReady) return;
  StopMusicStream(musicNormal);
  StopMusicStream(musicFast);
  playingFast = false;
}

static void SwitchToFastMusic(void) {
  if (!audioReady || playingFast) return;
  StopMusicStream(musicNormal);
  PlayMusicStream(musicFast);
  playingFast = true;
}

static void SwitchToNormalMusic(void) {
  if (!audioReady || !playingFast) return;
  StopMusicStream(musicFast);
  PlayMusicStream(musicNormal);
  playingFast = false;
}

static void SetMusicMuted(bool mute) {
    musicMuted = mute;
    if (mute) {
        SetMusicVolume(musicNormal, 0.0f);
        SetMusicVolume(musicFast,   0.0f);
        SetMusicVolume(musicMenu,   0.0f);
    } else {
        SetMusicVolume(musicNormal, 0.50f);
        SetMusicVolume(musicFast,   0.40f);
        SetMusicVolume(musicMenu,   0.20f);
    }
}

static void UpdateGameplayMusic(void) {
  if (!audioReady) return;
  if (playingFast) UpdateMusicStream(musicFast);
  else             UpdateMusicStream(musicNormal);
  if (!itsOver) {
    if (IsDangerZone()) SwitchToFastMusic();
    else                SwitchToNormalMusic();
  } else {
    StopGameplayMusic();
  }
}

/* ===================== ENGINE HELPERS ===================== */

static bool CanPlace(PiecesFormat t, int rot, int px, int py) {
  for (int i = 0; i < 4; i++) {
    int gx = px + SHAPES[t][rot][i][0];
    int gy = py + SHAPES[t][rot][i][1];
    if (gx < 1 || gx >= COLS-1) return false;
    if (gy >= ROWS) return false;
    if (gy < 0) continue;
    if (grid[gx][gy] == BOARD_LIMIT || grid[gx][gy] == PLACED_PIECE) return false;
  }
  return true;
}

static PiecesFormat RandomType(void) {
  static PiecesFormat last = TETROMINO_COUNT;
  PiecesFormat t = (PiecesFormat)GetRandomValue(0, TETROMINO_COUNT-1);
  if (t == last) t = (PiecesFormat)GetRandomValue(0, TETROMINO_COUNT-1);
  last = t;
  return t;
}

static int FindFullLines(int outLines[4]) {
  int count = 0;
  for (int y = 0; y < ROWS-1; y++) {
    bool full = true;
    for (int x = 1; x < COLS-1; x++)
      if (grid[x][y] != PLACED_PIECE) { full = false; break; }
    if (full && count < 4) outLines[count++] = y;
  }
  return count;
}

static void ApplyLineClearNow(const int clearLines[4], int clearCount) {
  if (clearCount <= 0) return;
  bool toClear[ROWS] = {0};
  for (int i = 0; i < clearCount; i++) {
    int y = clearLines[i];
    if (y >= 0 && y < ROWS-1) toClear[y] = true;
  }
  int writeRow = ROWS-2;
  for (int readRow = ROWS-2; readRow >= 0; readRow--) {
    if (toClear[readRow]) continue;
    if (writeRow != readRow)
      for (int x = 1; x < COLS-1; x++)
        grid[x][writeRow] = grid[x][readRow];
    writeRow--;
  }
  for (int y = writeRow; y >= 0; y--)
    for (int x = 1; x < COLS-1; x++)
      grid[x][y] = EMPTY;
}

/* ===================== SCORING ===================== */

static void ApplyScoring(int clearedThisMove) {
  int add = 0;
  if (clearedThisMove > 0) {
    linesCleared += clearedThisMove;
    level = 1 + (linesCleared / 10);
  }
  switch (clearedThisMove) {
    case 1: add = 100 * level; break;
    case 2: add = 300 * level; break;
    case 3: add = 500 * level; break;
    case 4: add = 800 * level; break;
    default: add = 0; break;
  }
  if (clearedThisMove == 4) {
    if (backToBack) add += add / 2;
    backToBack = true;
  } else if (clearedThisMove > 0) {
    backToBack = false;
  }
  if (clearedThisMove > 0) {
    combo++;
    if (combo > 0) add += (50 * combo * level);
  } else {
    combo = -1;
  }
  score += add;
  if (level < 10) scrollSpeed = 1 + (level - 1); 
  else if (level <= 12) scrollSpeed = 12;
  else if (level <= 15) scrollSpeed = 15;
  else if (level <= 18) scrollSpeed = 20;
  else if (level <= 28) scrollSpeed = 30;
  else                  scrollSpeed = 60;
  frameCounter = 0;
}

/* ===================== PIECE ACTIONS ===================== */

static void LockCurrentPiece(void) {
  for (int i = 0; i < 4; i++) {
    int gx = cur.x + SHAPES[cur.type][cur.rot][i][0];
    int gy = cur.y + SHAPES[cur.type][cur.rot][i][1];
    if (gy >= 0) grid[gx][gy] = PLACED_PIECE;
  }
  pieceActive = false;

  if (BindingDown(keys.softDrop)) {
    downBlocked = true;
    holdDownTime = 0.0f;
  }

  linesToClearCount = FindFullLines(linesToClear);
  if (linesToClearCount > 0) {
    clearingLines    = true;
    clearTimerFrames = LINE_CLEAR_DELAY_FRAMES;
    blinkFrameCounter = 0;
    blinkOn = false;
    if (sfxLineClearReady) {
      if (linesToClearCount == 4) PlaySound(sfxTetris);
      else                        PlaySound(sfxLineClear);
    }
  } else {
    ApplyScoring(0);
    spawnDelayFrames = SPAWN_DELAY_FRAMES;
  }
}

static void GenerateRandomPiece(void) {
  cur.type = nextType;
  cur.rot  = 0;
  cur.x    = (COLS-2) / 2;
  cur.y    = 0;
  nextType = RandomType();
  if (!CanPlace(cur.type, cur.rot, cur.x, cur.y)) {
    itsOver     = true;
    pieceActive = false;
    goFlow      = GO_ASK_SAVE;
    StopGameplayMusic();
    if (sfxGameOverReady) PlaySound(sfxGameOver);
    nameInput[0] = '\0';
    nameLen      = 0;
    return;
  }
  pieceActive = true;
}

static void TryMove(int dx, int dy) {
  int nx = cur.x + dx;
  int ny = cur.y + dy;
  if (CanPlace(cur.type, cur.rot, nx, ny)) {
    cur.x = nx;
    cur.y = ny;
  } else if (dy == 1) {
    LockCurrentPiece();
  }
}

static void TryRotateCW(void) {
  int nr = (cur.rot+1) & 3;
  if (CanPlace(cur.type, nr, cur.x, cur.y)) { cur.rot = nr; return; }
  const int kicks[] = { -1, 1, -2, 2 };
  for (int i = 0; i < 4; i++) {
    if (CanPlace(cur.type, nr, cur.x + kicks[i], cur.y)) {
      cur.x += kicks[i]; cur.rot = nr; return;
    }
  }
}

static void TryRotateCCW(void) {
  int nr = (cur.rot+3) & 3;
  if (CanPlace(cur.type, nr, cur.x, cur.y)) { cur.rot = nr; return; }
  const int kicks[] = { -1, 1, -2, 2 };
  for (int i = 0; i < 4; i++) {
    if (CanPlace(cur.type, nr, cur.x + kicks[i], cur.y)) {
      cur.x += kicks[i]; cur.rot = nr; return;
    }
  }
}

static void HardDrop(void) {
  int dropped = 0;
  while (CanPlace(cur.type, cur.rot, cur.x, cur.y+1)) { cur.y++; dropped++; }
  score += dropped * 2 * level;
  LockCurrentPiece();
}

/* ===================== INPUT (HORIZONTAL) ===================== */

static void HandleHorizontalInput(void) {
  float frameTime = GetFrameTime();
  bool left  = BindingDown(keys.moveLeft);
  bool right = BindingDown(keys.moveRight);
  if (left && right) { holdLeftTime = holdRightTime = 0.0f; return; }
  if (left) {
    if (holdLeftTime == 0.0f) TryMove(LEFT, 0);
    holdLeftTime += frameTime;
    if (holdLeftTime >= DAS)
      while (holdLeftTime >= DAS + ARR) { TryMove(LEFT, 0); holdLeftTime -= ARR; }
  } else { holdLeftTime = 0.0f; }
  if (right) {
    if (holdRightTime == 0.0f) TryMove(RIGHT, 0);
    holdRightTime += frameTime;
    if (holdRightTime >= DAS)
      while (holdRightTime >= DAS + ARR) { TryMove(RIGHT, 0); holdRightTime -= ARR; }
  } else { holdRightTime = 0.0f; }
}

/* ===================== GRID ===================== */

static void GenerateGrid(void) {
  for (int x = 0; x < COLS; x++)
    for (int y = 0; y < ROWS; y++) {
      if (x == 0 || x == COLS-1 || y == ROWS-1) grid[x][y] = BOARD_LIMIT;
      else grid[x][y] = EMPTY;
    }
}

/* ===================== DRAW HELPERS ===================== */

static void GridGraphic(Color gridLine, Color placedColor, Color wallColor) {
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++) {
      int xPos = BOARD_X_AXIS + x * SQUARE_SIZE;
      int yPos = BOARD_Y_AXIS + y * SQUARE_SIZE;
      switch (grid[x][y]) {
        case EMPTY:
          DrawRectangleLines(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, gridLine);
          break;
        case PLACED_PIECE: {
          Color fill = placedColor;
          if (clearingLines) {
            for (int i = 0; i < linesToClearCount; i++)
              if (linesToClear[i] == y) {
                fill = blinkOn ? (Color){255,255,255,200} : placedColor;
                break;
              }
          }
          DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, fill);
          DrawRectangleLines(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, gridLine);
        } break;
        case BOARD_LIMIT:
          DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, wallColor);
          break;
        default: break;
      }
    }
}

static void DrawActivePiece(Color activeColor) {
  if (!pieceActive) return;
  for (int i = 0; i < 4; i++) {
    int gx = cur.x + SHAPES[cur.type][cur.rot][i][0];
    int gy = cur.y + SHAPES[cur.type][cur.rot][i][1];
    if (gy < 0) continue;
    DrawRectangle(BOARD_X_AXIS + gx * SQUARE_SIZE,
                  BOARD_Y_AXIS + gy * SQUARE_SIZE,
                  SQUARE_SIZE, SQUARE_SIZE, activeColor);
  }
}

static void DrawPiecePreview(PiecesFormat t, int px, int py, int cell, Color fill) {
  int minX = 999, minY = 999;
  for (int i = 0; i < 4; i++) {
    if (SHAPES[t][0][i][0] < minX) minX = SHAPES[t][0][i][0];
    if (SHAPES[t][0][i][1] < minY) minY = SHAPES[t][0][i][1];
  }
  for (int i = 0; i < 4; i++) {
    int dx = SHAPES[t][0][i][0] - minX;
    int dy = SHAPES[t][0][i][1] - minY;
    DrawRectangle(px + dx*cell, py + dy*cell, cell, cell, fill);
  }
}

/* ===================== GAMEPLAY UPDATE ===================== */

static void UpdateGameplay(void) {
  if (itsOver) return;
  if (gamePaused){
    return;
  }
  
  if (clearingLines) {
    blinkFrameCounter++;
    if (blinkFrameCounter >= LINE_CLEAR_BLINK_EVERY) {
      blinkFrameCounter = 0;
      blinkOn = !blinkOn;
    }
    clearTimerFrames--;
    if (clearTimerFrames <= 0) {
      ApplyLineClearNow(linesToClear, linesToClearCount);
      ApplyScoring(linesToClearCount);
      clearingLines    = false;
      linesToClearCount = 0;
      spawnDelayFrames  = SPAWN_DELAY_FRAMES;
    }
    return;
  }

  if (!pieceActive) {
    if (spawnDelayFrames > 0) { spawnDelayFrames--; return; }
    GenerateRandomPiece();
  }
  if (itsOver) return;

  if (pieceActive) {
    HandleHorizontalInput();

    if (BindingPressed(keys.rotateCW))  TryRotateCW();
    if (BindingPressed(keys.rotateCCW)) TryRotateCCW();
    if (BindingPressed(keys.hardDrop))  { HardDrop(); return; }

    float dt = GetFrameTime();
    if (!BindingDown(keys.softDrop)) { downBlocked = false; holdDownTime = 0.0f; }

    if (BindingDown(keys.softDrop) && !downBlocked) {
      if (holdDownTime == 0.0f) {
        int oldY = cur.y;
        TryMove(0, 1);
        if (cur.y > oldY) score += 1 * level;
      }
      holdDownTime += dt;
      if (holdDownTime >= SD_DAS)
        while (holdDownTime >= SD_DAS + SD_ARR) {
          int oldY = cur.y;
          TryMove(0, 1);
          if (cur.y > oldY) score += 1 * level;
          holdDownTime -= SD_ARR;
        }
    }
  }

  frameCounter += scrollSpeed;
  if (frameCounter >= 60) { frameCounter = 0; TryMove(0, 1); }
}

static void RestartGame(void) {
  itsOver      = false;
  gamePaused = false;
  pauseCooldown = 0.0f;
  pieceActive  = false;
  frameCounter = 0;
  spawnDelayFrames = 0;

  holdLeftTime  = 0.0f;
  holdRightTime = 0.0f;
  holdDownTime  = 0.0f;
  downBlocked   = false;

  clearingLines    = false;
  clearTimerFrames = 0;
  linesToClearCount = 0;
  blinkFrameCounter = 0;
  blinkOn = false;

  score        = 0;
  linesCleared = (startLevel - 1) * 10;
  level        = startLevel;

  if (level < 10) scrollSpeed = 1 + (level - 1);
  else if (level <= 12) scrollSpeed = 12;
  else if (level <= 15) scrollSpeed = 15;
  else if (level <= 18) scrollSpeed = 20;
  else if (level <= 28) scrollSpeed = 30;
  else                  scrollSpeed = 60;

  combo      = -1;
  backToBack = false;

  goFlow       = GO_SHOW_GAMEOVER;
  nameInput[0] = '\0';
  nameLen      = 0;

  GenerateGrid();
  nextType = RandomType();
}

/* ===================== GAME OVER OVERLAY ===================== */

static void UpdateGameOverOverlay(Rectangle panel, Rectangle yesBtn, Rectangle noBtn, Rectangle inputBox) {
  Vector2 m = GetMousePosition();
  if (goFlow == GO_ASK_SAVE) {
    if (CheckCollisionPointRec(m, yesBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      goFlow = GO_ENTER_NAME; nameInput[0] = '\0'; nameLen = 0;
    }
    if (CheckCollisionPointRec(m, noBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      goFlow = GO_SHOW_GAMEOVER;
  } else if (goFlow == GO_ENTER_NAME) {
    int c = GetCharPressed();
    while (c > 0) {
      if (c >= 32 && c <= 126 && nameLen < MAX_NAME_LEN-1) {
        nameInput[nameLen++] = (char)c;
        nameInput[nameLen]   = '\0';
      }
      c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && nameLen > 0)
      nameInput[--nameLen] = '\0';
    if (IsKeyPressed(KEY_ENTER)) {
      AddScoreToLeaderboard(nameLen == 0 ? "PLAYER" : nameInput, score);
      goFlow = GO_SHOW_GAMEOVER;
    }
    (void)panel; (void)inputBox;
  }
}

static void DrawGameOverOverlay(int screenWidth, int screenHeight, Color hudText, Color highlight) {
  Rectangle panel = (Rectangle){ 160, 170, 480, 230 };
  DrawRectangleRec(panel, (Color){0,0,0,200});
  DrawRectangleLinesEx(panel, 2, RAYWHITE);
  int cx = screenWidth / 2;

  if (goFlow == GO_ASK_SAVE) {
    const char *q = "Save score?";
    DrawText(q, cx - MeasureText(q, 34)/2, (int)panel.y + 25, 34, hudText);
    DrawText(TextFormat("Score: %d", score), (int)panel.x + 30, (int)panel.y + 80, 22, hudText);

    Rectangle yesBtn = { panel.x + 110,                    panel.y + 150, 110, 40 };
    Rectangle noBtn  = { panel.x + panel.width - 220,      panel.y + 150, 110, 40 };
    Vector2 m = GetMousePosition();

    Color yesC = CheckCollisionPointRec(m, yesBtn) ? highlight : hudText;
    Color noC  = CheckCollisionPointRec(m, noBtn)  ? highlight : hudText;
    DrawRectangleLinesEx(yesBtn, 2, yesC);
    DrawRectangleLinesEx(noBtn,  2, noC);
    DrawText("YES", (int)yesBtn.x + 30, (int)yesBtn.y + 8, 24, yesC);
    DrawText("NO",  (int)noBtn.x  + 40, (int)noBtn.y  + 8, 24, noC);

    UpdateGameOverOverlay(panel, yesBtn, noBtn, (Rectangle){0});

  } else if (goFlow == GO_ENTER_NAME) {
    const char *t = "Type your name:";
    DrawText(t, cx - MeasureText(t, 28)/2, (int)panel.y + 25, 28, hudText);
    DrawText(TextFormat("Score: %d", score), (int)panel.x + 30, (int)panel.y + 70, 22, hudText);

    Rectangle inputBox = { panel.x + 80, panel.y + 120, panel.width - 160, 45 };
    Vector2 m = GetMousePosition();
    Color boxC = CheckCollisionPointRec(m, inputBox) ? highlight : hudText;
    DrawRectangleLinesEx(inputBox, 2, boxC);
    DrawText(nameInput, (int)inputBox.x + 10, (int)inputBox.y + 10, 24, hudText);

    const char *hint = "Press ENTER to save";
    DrawText(hint, cx - MeasureText(hint, 18)/2, (int)panel.y + 180, 18, hudText);

    UpdateGameOverOverlay(panel, (Rectangle){0}, (Rectangle){0}, inputBox);
  }
}

/* ===================== SCORES SCREEN ===================== */

static void UpdateScoresScreen(Rectangle backBtn, Rectangle prevBtn, Rectangle nextBtn,
                               Rectangle clearBtns[PAGE_SIZE], int indices[PAGE_SIZE]) {
  Vector2 m = GetMousePosition();
  if (CheckCollisionPointRec(m, prevBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    { if (scoresPage > 0) scoresPage--; }
  if (CheckCollisionPointRec(m, nextBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    int maxPage = (leaderboardCount <= 0) ? 0 : (leaderboardCount-1) / PAGE_SIZE;
    if (scoresPage < maxPage) scoresPage++;
  }
  for (int i = 0; i < PAGE_SIZE; i++) {
    if (indices[i] < 0) continue;
    if (CheckCollisionPointRec(m, clearBtns[i]) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      RemoveScoreAtIndex(indices[i]); break;
    }
  }
  (void)backBtn;
}

/* ===================== MAIN ===================== */

int main(void) {

  ThemeColors Themes[THEME_COUNT] = {
    [PURPLE_THEME] = { PURPLE,  DARKPURPLE, (Color){150,28,176,255},  "Purple" },
    [RED_THEME]    = { (Color){235,63,83,255}, (Color){128,18,31,255}, (Color){194,39,59,255}, "Red" },
    [GREEN_THEME]  = { GREEN,   DARKGREEN,  LIME,                      "Green"  },
    [BLUE_THEME]   = { BLUE,    (Color){17, 17, 133 ,255},   SKYBLUE,                   "Blue"   },
    [YELLOW_THEME] = { YELLOW,  (Color){214, 157, 0 ,255},       (Color){223,230,41,255},   "Yellow" },
    [ORANGE_THEME] = { ORANGE,  (Color){230,76,20,255}, (Color){245,148,12,255}, "Orange" },
    [PINK_THEME] = { (Color){255,105,180,255}, (Color){180,30,100,255}, (Color){255,20,147,255}, "Pink" },
  };

  const int screenWidth  = 800;
  const int screenHeight = 600;

  const char *title       = "RAYBLOCKS";
  const char *gameOver    = "GAME OVER";
  const char *restartText = "Click here to restart";
  const int   fontSize    = 100;

  InitWindow(screenWidth, screenHeight, "Rayblocks");
  Image icon = LoadImageFromMemory(".png", rayblocks_png, (int)rayblocks_png_len);
  SetWindowIcon(icon);
  UnloadImage(icon);
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  RenderTexture2D target = LoadRenderTexture(screenWidth, screenHeight);
  SetExitKey(0);
  SetTargetFPS(60);
  InitGameAudio();
  SetRandomSeed((unsigned int)time(NULL));
  nextType = RandomType();

  LoadLeaderboardFromFile();
  LoadKeybinds();

  /* init hover arrays */
  for (int i = 0; i < PAGE_SIZE;    i++) prevHoverClear[i]  = false;
  for (int i = 0; i < KEYBIND_COUNT; i++) { prevHoverKbBtns[i] = false; prevHoverGpBtns[i] = false; }

  MainMenu    currentScreen = MAINSCREEN;
  ThemeOptions currentTheme = PURPLE_THEME;

  int textWidth    = MeasureText(title, fontSize);
  int playW        = MeasureText("Play Game", 40);
  int scoresW      = MeasureText("Scores",    40);
  int settingsW    = MeasureText("Settings",  40);

  int centerTitle    = (screenWidth - textWidth)   / 2;
  int centerPlay     = (screenWidth - playW)       / 2;
  int centerScores   = (screenWidth - scoresW)     / 2;
  int centerSettings = (screenWidth - settingsW)   / 2;

  Rectangle playButton     = { (float)centerPlay,     240, (float)playW,     40 };
  Rectangle scoresButton   = { (float)centerScores,   300, (float)scoresW,   40 };
  Rectangle settingsButton = { (float)centerSettings, 360, (float)settingsW, 40 };

  PlayMusicStream(musicMenu);

  while (!WindowShouldClose()) {

    if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
    
    Color bgColor   = Themes[currentTheme].background;
    Color textBase  = Themes[currentTheme].text;
    Color highlight = Themes[currentTheme].highlight;

    Color gameBg      = Mix(bgColor, BLACK, 0.75f);
    Color gridLine    = Mix(textBase, gameBg, 0.60f);
    Color wallColor   = Mix(textBase, gameBg, 0.30f);
    Color activeColor = highlight;
    Color placedColor = Mix(highlight, gameBg, 0.55f);
    Color hudText     = Mix(textBase, RAYWHITE, 0.65f);

    float scaleX = (float)GetRenderWidth()  / (float)screenWidth;
    float scaleY = (float)GetRenderHeight() / (float)screenHeight;
    float scale  = (scaleX < scaleY) ? scaleX : scaleY;
    float offsetX = ((float)GetRenderWidth()  - (float)screenWidth  * scale) / 2.0f;
    float offsetY = ((float)GetRenderHeight() - (float)screenHeight * scale) / 2.0f;
    Vector2 mousePoint = {
      (GetMousePosition().x - offsetX) / scale,
      (GetMousePosition().y - offsetY) / scale
    };

    Rectangle muteBtn = { (float)(GetRenderWidth() - 36), 4, 28, 28 };
    Rectangle muteBtnVirt = {
      (muteBtn.x - offsetX) / scale,
      (muteBtn.y - offsetY) / scale,
      muteBtn.width  / scale,
      muteBtn.height / scale
    };

    hMute = CheckCollisionPointRec(mousePoint, muteBtnVirt);
    if (hMute && !prevHoverMute) PlayTick();
    prevHoverMute = hMute;

    if (hMute && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      SetMusicMuted(!musicMuted);

    int backW = MeasureText("BACK", 20);
    Rectangle backBtn = { 20, 20, (float)backW, 20 };

    const char *themeLabel = TextFormat("Theme: %s", Themes[currentTheme].name);
    int themeW = MeasureText(themeLabel, 20);
    Rectangle themeButton = { (float)(centerPlay + 25), 550, (float)themeW, 20 };

    const char *lvlLabel = TextFormat("Start Level: [ %d ]", startLevel);
    int lvlW = MeasureText(lvlLabel, 28);
    Rectangle levelButton = { (float)(screenWidth/2 - lvlW/2), 420, (float)lvlW, 28 };

    /* ---- UPDATE SWITCH ---- */
    switch (currentScreen) {

      case MAINSCREEN: {
        bool hPlay     = CheckCollisionPointRec(mousePoint, playButton);
        bool hScores   = CheckCollisionPointRec(mousePoint, scoresButton);
        bool hSettings = CheckCollisionPointRec(mousePoint, settingsButton);
        bool hLevel = CheckCollisionPointRec(mousePoint, levelButton);
        if (hPlay     && !prevHoverPlay)      PlayTick();
        if (hScores   && !prevHoverScoresBtn) PlayTick();
        if (hSettings && !prevHoverSettings)  PlayTick();
        if (hLevel    && !prevHoverLevel)     PlayTick();
        prevHoverPlay      = hPlay;
        prevHoverScoresBtn = hScores;
        prevHoverSettings  = hSettings;
        prevHoverLevel     = hLevel;

        if (hPlay && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          GenerateGrid();
          itsOver = false;
          pieceActive = false;
          frameCounter = 0;
          holdLeftTime = holdRightTime = holdDownTime = 0.0f;
          downBlocked = false;
          spawnDelayFrames = 0;
          clearingLines = false;
          clearTimerFrames = 0;
          linesToClearCount = 0;
          blinkFrameCounter = 0;
          blinkOn = false;
          score = 0;
          level = startLevel;
          linesCleared = (startLevel - 1) * 10;
          
          if (level < 10) scrollSpeed = 1 + (level - 1);
          else if (level <= 12) scrollSpeed = 12;
          else if (level <= 15) scrollSpeed = 15;
          else if (level <= 18) scrollSpeed = 20;
          else if (level <= 28) scrollSpeed = 30;
          else                  scrollSpeed = 60;
          
          combo = -1;
          backToBack = false;
          goFlow = GO_SHOW_GAMEOVER;
          nameInput[0] = '\0';
          nameLen = 0;
          nextType = RandomType();
          menuMusicDelay = 0.0f;
          StopMusicStream(musicMenu);
          currentScreen = GAMEPLAY;
          StartGameplayMusic();
        }
        if (hScores && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          scoresPage = 0; currentScreen = SCORES;
        }
        if (hSettings && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          settingsFlow = SF_IDLE; rebindingIndex = -1; rebindingGP = false;
          currentScreen = SETTINGS;
        }
        if (CheckCollisionPointRec(mousePoint, themeButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
          currentTheme = (ThemeOptions)((currentTheme+1) % THEME_COUNT);
        
        if (hLevel) {
          if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            startLevel++;
            if (startLevel > MAX_START_LEVEL) startLevel = MIN_START_LEVEL;  /* wrap */
          }
          if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            startLevel--;
            if (startLevel < MIN_START_LEVEL) startLevel = MAX_START_LEVEL;  /* wrap */
          }
        }
        
      } break;

      case GAMEPLAY: {
        bool hBack = CheckCollisionPointRec(mousePoint, backBtn);
        if (hBack && !prevHoverBack) PlayTick();
        prevHoverBack = hBack;

        if (hBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          StopGameplayMusic();
          StopMusicStream(musicMenu);
          SeekMusicStream(musicMenu, 0.0f);
          menuMusicDelay = 1.0f;
          currentScreen  = MAINSCREEN;
          break;
        }
        
        if(BindingPressed(keys.pause)&& !itsOver){
          
          if(pauseCooldown <= 0.0f) {
            gamePaused = !gamePaused;
            pauseCooldown = 10.0f;       
          } else {
            if(gamePaused) {
              gamePaused = false;
            }
          }

          if (gamePaused) {
            if (playingFast) PauseMusicStream(musicFast);
            else             PauseMusicStream(musicNormal);
          } else {
            if (playingFast) ResumeMusicStream(musicFast);
            else             ResumeMusicStream(musicNormal);
          }     
        }

        UpdateGameplay();
        UpdateGameplayMusic();

        if (itsOver && goFlow == GO_ASK_SAVE) {
          Rectangle panel  = { 160, 170, 480, 230 };
          Rectangle yesBtn = { panel.x + 110,               panel.y + 150, 110, 40 };
          Rectangle noBtn  = { panel.x + panel.width - 220, panel.y + 150, 110, 40 };
          bool hYes = CheckCollisionPointRec(mousePoint, yesBtn);
          bool hNo  = CheckCollisionPointRec(mousePoint, noBtn);
          if (hYes && !prevHoverYes) PlayTick();
          if (hNo  && !prevHoverNo)  PlayTick();
          prevHoverYes = hYes;
          prevHoverNo  = hNo;
        } else if (itsOver && goFlow == GO_ENTER_NAME) {
          Rectangle panel    = { 160, 170, 480, 230 };
          Rectangle inputBox = { panel.x + 80, panel.y + 120, panel.width - 160, 45 };
          bool hInp = CheckCollisionPointRec(mousePoint, inputBox);
          if (hInp && !prevHoverInput) PlayTick();
          prevHoverInput = hInp;
        }

        if (itsOver && goFlow == GO_SHOW_GAMEOVER && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          RestartGame();
          StartGameplayMusic();
        }
      } break;

      case SCORES: {
        bool hBack = CheckCollisionPointRec(mousePoint, backBtn);
        if (hBack && !prevHoverBack) PlayTick();
        prevHoverBack = hBack;

        if (hBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          currentScreen = MAINSCREEN;
          break;
        }

        Rectangle clearBtns[PAGE_SIZE] = {0};
        int idxs[PAGE_SIZE];
        for (int i = 0; i < PAGE_SIZE; i++) idxs[i] = -1;
        int start = scoresPage * PAGE_SIZE;
        for (int row = 0; row < PAGE_SIZE; row++) {
          int idx = start + row;
          if (idx >= leaderboardCount) break;
          idxs[row] = idx;
          clearBtns[row] = (Rectangle){ 640, (float)(140 + row*36 - 2), 80, 28 };
        }
        Rectangle prevBtn = { 270, 520, 90, 32 };
        Rectangle nextBtn = { 440, 520, 90, 32 };

        bool hPrev = CheckCollisionPointRec(mousePoint, prevBtn);
        bool hNext = CheckCollisionPointRec(mousePoint, nextBtn);
        if (hPrev && !prevHoverPrev) PlayTick();
        if (hNext && !prevHoverNext) PlayTick();
        prevHoverPrev = hPrev;
        prevHoverNext = hNext;
        for (int i = 0; i < PAGE_SIZE; i++) {
          if (idxs[i] < 0) { prevHoverClear[i] = false; continue; }
          bool hC = CheckCollisionPointRec(mousePoint, clearBtns[i]);
          if (hC && !prevHoverClear[i]) PlayTick();
          prevHoverClear[i] = hC;
        }

        UpdateScoresScreen(backBtn, prevBtn, nextBtn, clearBtns, idxs);
      } break;

      case SETTINGS: {
        bool hBack = CheckCollisionPointRec(mousePoint, backBtn);
        if (hBack && !prevHoverBack) PlayTick();
        prevHoverBack = hBack;

        if (hBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          settingsFlow = SF_IDLE; rebindingIndex = -1; rebindingGP = false;
          SaveKeybinds();
          currentScreen = MAINSCREEN;
          break;
        }

        if (settingsFlow == SF_WAITING_KEY) {
          if (IsKeyPressed(KEY_ESCAPE)) { settingsFlow = SF_IDLE; rebindingIndex = -1; }
          else {
            for (int k = 1; k < 350; k++) {
              if (IsKeyPressed((KeyboardKey)k)) {
                Binding *b = BindingByIndex(rebindingIndex);
                if (b) b->key = (KeyboardKey)k;
                settingsFlow = SF_IDLE; rebindingIndex = -1;
                break;
              }
            }
          }
        }
        else if (settingsFlow == SF_WAITING_GP) {

          int gid = GetActiveGamepadId();   // ← aqui
          bool gpConnected = (gid >= 0);

          if (IsKeyPressed(KEY_ESCAPE)) {
            settingsFlow = SF_IDLE;
            rebindingIndex = -1;
          }
          else if (gpConnected) {

            for (int b = 0; b < 32; b++) {
              if (IsGamepadButtonPressed(gid, b)) {

                Binding *bd = BindingByIndex(rebindingIndex);
                if (bd) bd->gpButton = b;

                settingsFlow = SF_IDLE;
                rebindingIndex = -1;
                break;
              }
            }
          }
        } else {
          for (int i = 0; i < KEYBIND_COUNT; i++) {
            Rectangle kbBtn = { 390, (float)(150 + i*55), 130, 34 };
            Rectangle gpBtn = { 530, (float)(150 + i*55), 130, 34 };
            bool hKb = CheckCollisionPointRec(mousePoint, kbBtn);
            bool hGp = CheckCollisionPointRec(mousePoint, gpBtn);
            if (hKb && !prevHoverKbBtns[i]) PlayTick();
            if (hGp && !prevHoverGpBtns[i]) PlayTick();
            prevHoverKbBtns[i] = hKb;
            prevHoverGpBtns[i] = hGp;
            if (hKb && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
              { settingsFlow = SF_WAITING_KEY; rebindingIndex = i; rebindingGP = false; }
            if (hGp && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
              { settingsFlow = SF_WAITING_GP; rebindingIndex = i; rebindingGP = true; }
          }
          /* reset button */
          Rectangle resetBtn = { 280, 550, 240, 36 };
          bool hReset = CheckCollisionPointRec(mousePoint, resetBtn);
          if (hReset && !prevHoverReset) PlayTick();
          prevHoverReset = hReset;
          if (hReset && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            keys = (Keybinds){
              { KEY_LEFT,  -1 }, { KEY_RIGHT, -1 },
              { KEY_DOWN,  -1 }, { KEY_SPACE, -1 },
              { KEY_UP,    -1 }, { KEY_X,     -1 },
	      { KEY_P,    -1 },
            };
            settingsFlow = SF_IDLE;
          }
        }
      } break;
    }

    /* ---- menu music delay + stream update ---- */
    if (menuMusicDelay > 0.0f) {
      menuMusicDelay -= GetFrameTime();
      if (menuMusicDelay <= 0.0f) { menuMusicDelay = 0.0f; PlayMusicStream(musicMenu); }
    }
    
    if (pauseCooldown > 0.0f) {  /*  */
      pauseCooldown -= GetFrameTime(); //pause cooldown
    }
    
    if (currentScreen == MAINSCREEN || currentScreen == SCORES || currentScreen == SETTINGS)
      UpdateMusicStream(musicMenu);



    /* ---- DRAW SWITCH ---- */
    BeginTextureMode(target);

    switch (currentScreen) {

      case MAINSCREEN: {
        ClearBackground(bgColor);
        DrawText(title, centerTitle, 20, fontSize, textBase);

        Color cPlay     = CheckCollisionPointRec(mousePoint, playButton)     ? highlight : textBase;
        Color cScores   = CheckCollisionPointRec(mousePoint, scoresButton)   ? highlight : textBase;
        Color cSettings = CheckCollisionPointRec(mousePoint, settingsButton) ? highlight : textBase;
        Color cLevel = CheckCollisionPointRec(mousePoint, levelButton) ? highlight : textBase;

        DrawText("Play Game", centerPlay,     240, 40, cPlay);
        DrawText("Scores",    centerScores,   300, 40, cScores);
        DrawText("Settings",  centerSettings, 360, 40, cSettings);
        DrawText(themeLabel,  centerPlay + 25, 550, 20, textBase);
        DrawText(lvlLabel, screenWidth/2 - lvlW/2, 420, 28, cLevel);
      } break;

      case GAMEPLAY: {
        ClearBackground(gameBg);
        DrawText("BACK", 20, 20, 20, hudText);

        GridGraphic(gridLine, placedColor, wallColor);
        DrawActivePiece(activeColor);
	
        DrawText(TextFormat("Score: %d", score),        380, 100, 20, hudText);
        DrawText(TextFormat("Lines: %d", linesCleared), 380, 130, 20, hudText);
        DrawText(TextFormat("Level: %d", level),        380, 160, 20, hudText);
        DrawText("Next:", 380, 210, 20, hudText);
        DrawPiecePreview(nextType, 380, 240, 18, activeColor);

	if (gamePaused) {
	  DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 255});
	  const char *pt = "PAUSED";
	  int pw = MeasureText(pt, 60);
	  DrawText(pt, screenWidth/2 - pw/2, screenHeight/2 - 60, 60, highlight);
	  const char *ph = TextFormat("Press %s to resume", KeyName(keys.pause.key));
	  int phw = MeasureText(ph, 20);
	  DrawText(ph, screenWidth/2 - phw/2, screenHeight/2 + 10, 20, hudText);
	}

        if (itsOver) {
          if (goFlow != GO_SHOW_GAMEOVER) {
            DrawGameOverOverlay(screenWidth, screenHeight, hudText, highlight);
          } else {
            int goWidth      = MeasureText(gameOver,    50);
            int restartWidth = MeasureText(restartText, 20);
            int centerX      = screenWidth / 2;
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0,0,0,130});
            DrawText(gameOver,    centerX - goWidth/2,      230, 50, highlight);
            DrawText(restartText, centerX - restartWidth/2, 300, 20, hudText);
            DrawText("BACK", 20, 20, 20, RAYWHITE);
          }
        }
      } break;

      case SCORES: {
        ClearBackground(bgColor);
        DrawText("BACK",        20,  20, 20, textBase);
        DrawText("LEADERBOARD", 260, 30, 36, textBase);
        DrawText("RANK",  140, 115, 18, textBase);
        DrawText("NAME",  240, 115, 18, textBase);
        DrawText("SCORE", 500, 115, 18, textBase);

        int start = scoresPage * PAGE_SIZE;
        Rectangle clearBtns[PAGE_SIZE] = {0};
        int idxs[PAGE_SIZE];
        for (int i = 0; i < PAGE_SIZE; i++) idxs[i] = -1;

        for (int row = 0; row < PAGE_SIZE; row++) {
          int idx = start + row;
          if (idx >= leaderboardCount) break;
          idxs[row] = idx;
          int y = 140 + row * 36;
          DrawText(TextFormat("%d",  idx+1),                    145, y, 20, textBase);
          DrawText(leaderboard[idx].name,                       240, y, 20, textBase);
          DrawText(TextFormat("%d",  leaderboard[idx].value),   500, y, 20, textBase);
          clearBtns[row] = (Rectangle){ 640, (float)(y-2), 80, 28 };
          bool hov = CheckCollisionPointRec(mousePoint, clearBtns[row]);
          DrawRectangleLinesEx(clearBtns[row], 2, hov ? highlight : textBase);
          DrawText("CLEAR", (int)clearBtns[row].x + 10, (int)clearBtns[row].y + 5, 18, hov ? highlight : textBase);
        }

        Rectangle prevBtn = { 270, 520, 90, 32 };
        Rectangle nextBtn = { 440, 520, 90, 32 };
        int maxPage = (leaderboardCount <= 0) ? 0 : (leaderboardCount-1) / PAGE_SIZE;

        bool hPrev = CheckCollisionPointRec(mousePoint, prevBtn);
        bool hNext = CheckCollisionPointRec(mousePoint, nextBtn);
        DrawRectangleLinesEx(prevBtn, 2, hPrev ? highlight : textBase);
        DrawRectangleLinesEx(nextBtn, 2, hNext ? highlight : textBase);
        DrawText("PREV", (int)prevBtn.x + 18, (int)prevBtn.y + 6, 20, hPrev ? highlight : textBase);
        DrawText("NEXT", (int)nextBtn.x + 18, (int)nextBtn.y + 6, 20, hNext ? highlight : textBase);
        DrawText(TextFormat("Page %d/%d", scoresPage+1, maxPage+1), 345, 560, 18, textBase);

        if (leaderboardCount == 0)
          DrawText("No scores yet.", 320, 320, 22, textBase);
      } break;

      case SETTINGS: {
        ClearBackground(bgColor);
        DrawText("BACK",     20,  20, 20, textBase);
        DrawText("SETTINGS", 290, 30, 36, textBase);

        /* column headers */
        DrawText("ACTION",   160, 118, 18, textBase);
        DrawText("KEYBOARD", 390, 118, 18, textBase);
        DrawText("GAMEPAD",  530, 118, 18, textBase);

        int gid = GetActiveGamepadId();
        bool gpConnected = (gid >= 0);

        for (int i = 0; i < KEYBIND_COUNT; i++) {
          int y = 150 + i * 55;
          Binding *b = BindingByIndex(i);

          DrawText(keybindLabels[i], 160, y + 8, 22, textBase);

          /* keyboard button */
          Rectangle kbBtn = { 390, (float)y, 130, 34 };
          bool waitingThisKb = (settingsFlow == SF_WAITING_KEY && rebindingIndex == i);
          bool hKb = CheckCollisionPointRec(mousePoint, kbBtn);
          Color kbC = (waitingThisKb || hKb) ? highlight : textBase;
          DrawRectangleLinesEx(kbBtn, 2, kbC);
          const char *kbLabel = waitingThisKb ? "Press key..." : KeyName(b->key);
          int kbLW = MeasureText(kbLabel, 18);
          DrawText(kbLabel, (int)kbBtn.x + (int)(kbBtn.width/2) - kbLW/2, (int)kbBtn.y + 8, 18, kbC);

          /* gamepad button */
          Rectangle gpBtn = { 530, (float)y, 130, 34 };
          bool waitingThisGp = (settingsFlow == SF_WAITING_GP && rebindingIndex == i);
          bool hGp = CheckCollisionPointRec(mousePoint, gpBtn);
          Color gpC = (!gpConnected) ? Mix(textBase, bgColor, 0.5f)
                      : (waitingThisGp || hGp) ? highlight : textBase;
          DrawRectangleLinesEx(gpBtn, 2, gpC);
          const char *gpLabel = waitingThisGp ? "Press btn..." : GPButtonName(b->gpButton);
          int gpLW = MeasureText(gpLabel, 18);
          DrawText(gpLabel, (int)gpBtn.x + (int)(gpBtn.width/2) - gpLW/2, (int)gpBtn.y + 8, 18, gpC);
        }

        if (!gpConnected)
          DrawText("No gamepad detected", 530, 140 + KEYBIND_COUNT*55, 16, Mix(textBase, bgColor, 0.4f));

        Rectangle resetBtn = { 280, 550, 240, 36 };
        bool hReset = CheckCollisionPointRec(mousePoint, resetBtn);
        Color resetC = hReset ? highlight : textBase;
        DrawRectangleLinesEx(resetBtn, 2, resetC);
        DrawText("Reset Defaults", (int)resetBtn.x + 30, (int)resetBtn.y + 8, 20, resetC);

        if (settingsFlow != SF_IDLE) {
          const char *hint = "Press ESC to cancel";
          DrawText(hint, 530, 560, 18, textBase);
        }
      } break;
    }

    const char *muteLabel = musicMuted ? "[M]" : "[S]";
    bool hMute = CheckCollisionPointRec(mousePoint, muteBtnVirt);
    Color muteC = hMute ? highlight : (Color){textBase.r, textBase.g, textBase.b, 128};
    DrawTextEx(GetFontDefault(), muteLabel, (Vector2){(int)muteBtnVirt.x + 4, (int)muteBtnVirt.y + 6}, 16, 1, muteC);
    
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(
                   target.texture,
                   (Rectangle){ 0, (float)screenHeight, (float)screenWidth, -(float)screenHeight },
                   (Rectangle){ offsetX, offsetY, (float)screenWidth * scale, (float)screenHeight * scale },
                   (Vector2){ 0, 0 },
                   0.0f,
                   WHITE
                   );
    EndDrawing();
  }

  SaveKeybinds();
  UnloadGameAudio();
  UnloadRenderTexture(target);
  CloseWindow();
  return 0;
}
