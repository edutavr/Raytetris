#include "raylib.h"
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

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

/* ===================== TYPES ===================== */

typedef enum CellState{
  EMPTY, MOVING_PIECE, PLACED_PIECE, CLEAN_LINE, BOARD_LIMIT
} CellState;

typedef enum PiecesFormat {
  I, O, T, S, Z, J, L, TETROMINO_COUNT
} PiecesFormat;

typedef enum MainMenu{
  MAINSCREEN = 0, GAMEPLAY, SCORES
} MainMenu;

typedef enum ThemeOptions{
  PURPLE_THEME, RED_THEME, GREEN_THEME, BLUE_THEME, YELLOW_THEME, ORANGE_THEME, THEME_COUNT
} ThemeOptions;

typedef struct ThemeColors{
  Color background;
  Color text;
  Color highlight;
  const char *name;
} ThemeColors;

typedef struct ActivePiece {
  PiecesFormat type;
  int rot;   // 0..3
  int x;
  int y;
} ActivePiece;

typedef struct ScoreEntry {
  char name[MAX_NAME_LEN]; // includes '\0'
  int value;
} ScoreEntry;

typedef enum GameOverFlow {
  GO_ASK_SAVE = 0,
  GO_ENTER_NAME,
  GO_SHOW_GAMEOVER
} GameOverFlow;

/* ===================== GLOBAL STATE ===================== */

static CellState grid[COLS][ROWS];

static float holdLeftTime = 0.0f;
static float holdRightTime = 0.0f;

static int spawnDelayFrames = 0;


static const float DAS = 0.15f;
static const float ARR = 0.05f;

static bool itsOver = false;

/* timing + scoring */
static int frameCounter = 0;
static int scrollSpeed  = 1;

static int score = 0;
static int linesCleared = 0;
static int level = 1;

static int combo = -1;
static bool backToBack = false;

/* next preview */
static PiecesFormat nextType;

/* active */
static ActivePiece cur;
static bool pieceActive = false;

/* game over flow */
static GameOverFlow goFlow = GO_SHOW_GAMEOVER;
static char nameInput[MAX_NAME_LEN] = {0};
static int nameLen = 0;

/* leaderboard */
static ScoreEntry leaderboard[MAX_SCORES];
static int leaderboardCount = 0;
static int scoresPage = 0;

// ===== AUDIO =====
static Music musicNormal;
static Music musicFast;
static bool audioReady = false;
static bool playingFast = false;
static Sound sfxLineClear;
static Sound sfxTetris; 
static bool sfxLineClearReady = false;

/* --- ADDED: soft drop ARR + "must release" between pieces --- */
static float holdDownTime = 0.0f;
static bool downBlocked = false;
static const float SD_DAS = 0.00f;
static const float SD_ARR = 0.03f;

/* --- ADDED: line clear freeze + blink animation --- */
static bool clearingLines = false;
static int clearTimerFrames = 0;

#define LINE_CLEAR_DELAY_FRAMES 20
#define LINE_CLEAR_BLINK_EVERY  6

static int blinkFrameCounter = 0;
static bool blinkOn = false;

static int linesToClear[4];
static int linesToClearCount = 0;

/* ===================== SHAPES ===================== */

static const int SHAPES[TETROMINO_COUNT][4][4][2] = {
  // I
  {
    {{-1,0},{0,0},{1,0},{2,0}},
    {{1,-1},{1,0},{1,1},{1,2}},
    {{-1,1},{0,1},{1,1},{2,1}},
    {{0,-1},{0,0},{0,1},{0,2}},
  },
  // O
  {
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
  },
  // T
  {
    {{-1,0},{0,0},{1,0},{0,1}},
    {{0,-1},{0,0},{0,1},{1,0}},
    {{-1,0},{0,0},{1,0},{0,-1}},
    {{0,-1},{0,0},{0,1},{-1,0}},
  },
  // S
  {
    {{0,0},{1,0},{-1,1},{0,1}},
    {{0,-1},{0,0},{1,0},{1,1}},
    {{0,0},{1,0},{-1,1},{0,1}},
    {{0,-1},{0,0},{1,0},{1,1}},
  },
  // Z
  {
    {{-1,0},{0,0},{0,1},{1,1}},
    {{1,-1},{0,0},{1,0},{0,1}},
    {{-1,0},{0,0},{0,1},{1,1}},
    {{1,-1},{0,0},{1,0},{0,1}},
  },
  // J
  {
    {{-1,0},{0,0},{1,0},{-1,1}},
    {{0,-1},{0,0},{0,1},{1,1}},
    {{-1,0},{0,0},{1,0},{1,-1}},
    {{0,-1},{0,0},{0,1},{-1,-1}},
  },
  // L
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

/* ===================== LEADERBOARD (LOAD/SAVE/SORT) ===================== */

static void SortLeaderboard(void) {
  for (int i = 0; i < leaderboardCount - 1; i++) {
    for (int j = 0; j < leaderboardCount - 1 - i; j++) {
      if (leaderboard[j].value < leaderboard[j + 1].value) {
        ScoreEntry tmp = leaderboard[j];
        leaderboard[j] = leaderboard[j + 1];
        leaderboard[j + 1] = tmp;
      }
    }
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
  if (!f) {
    leaderboardCount = 0;
    return;
  }

  int count = 0;
  if (fread(&count, sizeof(int), 1, f) != 1) {
    fclose(f);
    leaderboardCount = 0;
    return;
  }

  if (count < 0) count = 0;
  if (count > MAX_SCORES) count = MAX_SCORES;

  int readN = (int)fread(leaderboard, sizeof(ScoreEntry), (size_t)count, f);
  fclose(f);

  leaderboardCount = readN;

  for (int i = 0; i < leaderboardCount; i++) {
    leaderboard[i].name[MAX_NAME_LEN - 1] = '\0';
  }

  SortLeaderboard();
}

static void AddScoreToLeaderboard(const char *name, int value) {
  if (leaderboardCount < MAX_SCORES) {
    ScoreEntry e = {0};
    strncpy(e.name, name, MAX_NAME_LEN - 1);
    e.name[MAX_NAME_LEN - 1] = '\0';
    e.value = value;

    leaderboard[leaderboardCount++] = e;
  } else {
    SortLeaderboard();
    if (leaderboard[leaderboardCount - 1].value < value) {
      strncpy(leaderboard[leaderboardCount - 1].name, name, MAX_NAME_LEN - 1);
      leaderboard[leaderboardCount - 1].name[MAX_NAME_LEN - 1] = '\0';
      leaderboard[leaderboardCount - 1].value = value;
    }
  }

  SortLeaderboard();
  SaveLeaderboardToFile();
}

static void RemoveScoreAtIndex(int idx) {
  if (idx < 0 || idx >= leaderboardCount) return;

  for (int i = idx; i < leaderboardCount - 1; i++) {
    leaderboard[i] = leaderboard[i + 1];
  }
  leaderboardCount--;

  int maxPage = (leaderboardCount <= 0) ? 0 : (leaderboardCount - 1) / PAGE_SIZE;
  if (scoresPage > maxPage) scoresPage = maxPage;

  SaveLeaderboardToFile();
}

/* ===================== ENGINE HELPERS ===================== */

static bool CanPlace(PiecesFormat t, int rot, int px, int py) {
  for (int i = 0; i < 4; i++) {
    int gx = px + SHAPES[t][rot][i][0];
    int gy = py + SHAPES[t][rot][i][1];

    if (gx < 1 || gx >= COLS - 1) return false;
    if (gy >= ROWS) return false;

    //allow blocks above the visible top
    if (gy < 0) continue;

    if (grid[gx][gy] == BOARD_LIMIT || grid[gx][gy] == PLACED_PIECE) return false;
  }
  return true;
}

static PiecesFormat RandomType(void) {
  return (PiecesFormat)GetRandomValue(0, TETROMINO_COUNT - 1);
}

static int ClearFullLines(void) {
  int cleared = 0;
  int writeRow = ROWS - 2;

  for (int readRow = ROWS - 2; readRow >= 0; readRow--) {
    bool full = true;
    for (int x = 1; x < COLS - 1; x++) {
      if (grid[x][readRow] != PLACED_PIECE) { full = false; break; }
    }

    if (full) {
      cleared++;
    } else {
      if (writeRow != readRow) {
        for (int x = 1; x < COLS - 1; x++) {
          grid[x][writeRow] = grid[x][readRow];
        }
      }
      writeRow--;
    }
  }

  for (int y = writeRow; y >= 0; y--) {
    for (int x = 1; x < COLS - 1; x++) {
      grid[x][y] = EMPTY;
    }
  }

  return cleared;
}

/* --- find full lines without clearing yet (for animation) --- */
static int FindFullLines(int outLines[4]) {
  int count = 0;

  for (int y = 0; y < ROWS - 1; y++) {
    bool full = true;
    for (int x = 1; x < COLS - 1; x++) {
      if (grid[x][y] != PLACED_PIECE) { full = false; break; }
    }
    if (full) {
      if (count < 4) outLines[count++] = y;
    }
  }
  return count;
}

/* --- apply clear using known lines (after delay) --- */
static void ApplyLineClearNow(const int clearLines[4], int clearCount) {
  if (clearCount <= 0) return;

  bool toClear[ROWS] = {0};
  for (int i = 0; i < clearCount; i++) {
    int y = clearLines[i];
    if (y >= 0 && y < ROWS - 1) toClear[y] = true;
  }

  int writeRow = ROWS - 2;

  for (int readRow = ROWS - 2; readRow >= 0; readRow--) {
    if (toClear[readRow]) continue;

    if (writeRow != readRow) {
      for (int x = 1; x < COLS - 1; x++) {
        grid[x][writeRow] = grid[x][readRow];
      }
    }
    writeRow--;
  }

  for (int y = writeRow; y >= 0; y--) {
    for (int x = 1; x < COLS - 1; x++) {
      grid[x][y] = EMPTY;
    }
  }
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

  scrollSpeed = 1 + (level - 1);
  if (scrollSpeed > 30) scrollSpeed = 30;
  frameCounter = 0;
}

/* ===================== PIECE ACTIONS ===================== */

static void LockCurrentPiece(void) {
  for (int i = 0; i < 4; i++) {
    int gx = cur.x + SHAPES[cur.type][cur.rot][i][0];
    int gy = cur.y + SHAPES[cur.type][cur.rot][i][1];

    // ADDED: only write inside visible grid (gy can be < 0 now)
    if (gy >= 0) grid[gx][gy] = PLACED_PIECE;
  }
  pieceActive = false;

  // ADDED: if holding DOWN when locking, require release for next piece
  if (IsKeyDown(KEY_DOWN)) {
    downBlocked = true;
    holdDownTime = 0.0f;
  }

  // ADDED: line clear "freeze + blink" instead of instant clear
  linesToClearCount = FindFullLines(linesToClear);

  if (linesToClearCount > 0) {
    clearingLines = true;
    clearTimerFrames = LINE_CLEAR_DELAY_FRAMES;
    blinkFrameCounter = 0;
    blinkOn = false;

    if (sfxLineClearReady) {
        if (linesToClearCount == 4)
            PlaySound(sfxTetris);
        else
            PlaySound(sfxLineClear);
    }
    // scoring will happen after delay (when we actually clear)
  } else {
    // preserve combo reset behavior when no line is cleared
    ApplyScoring(0);
    spawnDelayFrames = SPAWN_DELAY_FRAMES;
  }
}

static void GenerateRandomPiece(void) {
  cur.type = nextType;
  cur.rot  = 0;
  cur.x    = (COLS - 2) / 2;
  cur.y    = 0;

  nextType = RandomType();

  if (!CanPlace(cur.type, cur.rot, cur.x, cur.y)) {
    itsOver = true;
    pieceActive = false;

    goFlow = GO_ASK_SAVE;
    nameInput[0] = '\0';
    nameLen = 0;

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
  int nr = (cur.rot + 1) & 3;

  if (CanPlace(cur.type, nr, cur.x, cur.y)) {
    cur.rot = nr;
    return;
  }

  const int kicks[] = { -1, 1, -2, 2 };
  for (int i = 0; i < 4; i++) {
    int kx = kicks[i];
    if (CanPlace(cur.type, nr, cur.x + kx, cur.y)) {
      cur.x += kx;
      cur.rot = nr;
      return;
    }
  }
}

static void TryRotateCCW(void) {
  int nr = (cur.rot + 3) & 3;//same as (rot - 1) mod 4

  if (CanPlace(cur.type, nr, cur.x, cur.y)) {
    cur.rot = nr;
    return;
  }

  const int kicks[] = { -1, 1, -2, 2 };
  for (int i = 0; i < 4; i++) {
    int kx = kicks[i];
    if (CanPlace(cur.type, nr, cur.x + kx, cur.y)) {
      cur.x += kx;
      cur.rot = nr;
      return;
    }
  }
}

static void HardDrop(void) {
  int dropped = 0;
  while (CanPlace(cur.type, cur.rot, cur.x, cur.y + 1)) {
    cur.y += 1;
    dropped++;
  }
  score += dropped * 2 * level;
  LockCurrentPiece();
}

/* ===================== INPUT (HORIZONTAL) ===================== */

static void HandleHorizontalInput(void) {
  float frameTime = GetFrameTime();

  bool left  = IsKeyDown(KEY_LEFT);
  bool right = IsKeyDown(KEY_RIGHT);

  if (left && right) {
    holdLeftTime = 0.0f;
    holdRightTime = 0.0f;
    return;
  }

  if (left) {
    if (holdLeftTime == 0.0f) TryMove(LEFT, 0);
    holdLeftTime += frameTime;

    if (holdLeftTime >= DAS) {
      while (holdLeftTime >= DAS + ARR) {
        TryMove(LEFT, 0);
        holdLeftTime -= ARR;
      }
    }
  } else {
    holdLeftTime = 0.0f;
  }

  if (right) {
    if (holdRightTime == 0.0f) TryMove(RIGHT, 0);
    holdRightTime += frameTime;

    if (holdRightTime >= DAS) {
      while (holdRightTime >= DAS + ARR) {
        TryMove(RIGHT, 0);
        holdRightTime -= ARR;
      }
    }
  } else {
    holdRightTime = 0.0f;
  }
}

/* ===================== GRID ===================== */

static void GenerateGrid(void) {
  for (int x = 0; x < COLS; x++) {
    for (int y = 0; y < ROWS; y++) {
      if (x == 0 || x == COLS - 1 || y == ROWS - 1) grid[x][y] = BOARD_LIMIT;
      else grid[x][y] = EMPTY;
    }
  }
}

/* ===================== DRAW HELPERS ===================== */

static void GridGraphic(Color gridLine, Color placedColor, Color wallColor) {
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      int xPos = BOARD_X_AXIS + x * SQUARE_SIZE;
      int yPos = BOARD_Y_AXIS + y * SQUARE_SIZE;

      switch (grid[x][y]) {
        case EMPTY:
          DrawRectangleLines(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, gridLine);
          break;

        case PLACED_PIECE: {
          Color fill = placedColor;

          // blink lines that will be cleared
          if (clearingLines) {
            bool isClearLine = false;
            for (int i = 0; i < linesToClearCount; i++) {
              if (linesToClear[i] == y) { isClearLine = true; break; }
            }
            if (isClearLine) {
              if (blinkOn) fill = (Color){255, 255, 255, 200};
              else         fill = placedColor;
            }
          }

          DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, fill);
          DrawRectangleLines(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, gridLine);
        } break;

        case BOARD_LIMIT:
          DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, wallColor);
          break;

        default:
          break;
      }
    }
  }
}

/* falling piece WITHOUT outline */
static void DrawActivePiece(Color activeColor) {
  if (!pieceActive) return;

  for (int i = 0; i < 4; i++) {
    int gx = cur.x + SHAPES[cur.type][cur.rot][i][0];
    int gy = cur.y + SHAPES[cur.type][cur.rot][i][1];

    // don't draw blocks above the visible top
    if (gy < 0) continue;

    int xPos = BOARD_X_AXIS + gx * SQUARE_SIZE;
    int yPos = BOARD_Y_AXIS + gy * SQUARE_SIZE;

    DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, activeColor);
  }
}

static void DrawPiecePreview(PiecesFormat t, int px, int py, int cell, Color fill) {
  int minX = 999, minY = 999;

  for (int i = 0; i < 4; i++) {
    int dx = SHAPES[t][0][i][0];
    int dy = SHAPES[t][0][i][1];
    if (dx < minX) minX = dx;
    if (dy < minY) minY = dy;
  }

  for (int i = 0; i < 4; i++) {
    int dx = SHAPES[t][0][i][0] - minX;
    int dy = SHAPES[t][0][i][1] - minY;
    DrawRectangle(px + dx * cell, py + dy * cell, cell, cell, fill);
  }
}

/* ===================== GAMEPLAY UPDATE ===================== */

static void UpdateGameplay(void) {
  if (itsOver) return;

  // freeze gameplay during line clear animation
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

      clearingLines = false;
      linesToClearCount = 0;
      spawnDelayFrames = SPAWN_DELAY_FRAMES;
    }
    return;
  }

  if (!pieceActive) {
    if (spawnDelayFrames > 0) {
      spawnDelayFrames--;
      return; //freezes gameplay at this interval
    }
    GenerateRandomPiece();
  }
  if (itsOver) return;

  if (pieceActive) {
    HandleHorizontalInput();

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_Z)) TryRotateCW();

    if (IsKeyPressed(KEY_X)) TryRotateCCW();

    if (IsKeyPressed(KEY_SPACE)) {
      HardDrop();
      return;
    }

    // soft drop with ARR + "must release" between pieces
    float dt = GetFrameTime();

    if (!IsKeyDown(KEY_DOWN)) {
      downBlocked = false;
      holdDownTime = 0.0f;
    }

    if (IsKeyDown(KEY_DOWN) && !downBlocked) {
      if (holdDownTime == 0.0f) {
        int oldY = cur.y;
        TryMove(0, 1);
        if (cur.y > oldY) score += 1 * level;
      }

      holdDownTime += dt;

      if (holdDownTime >= SD_DAS) {
        while (holdDownTime >= SD_DAS + SD_ARR) {
          int oldY = cur.y;
          TryMove(0, 1);
          if (cur.y > oldY) score += 1 * level;
          holdDownTime -= SD_ARR;
        }
      }
    }
  }

  frameCounter += scrollSpeed;
  if (frameCounter >= 60) {
    frameCounter = 0;
    TryMove(0, 1);
  }
}

static void RestartGame(void) {
  itsOver = false;
  pieceActive = false;
  frameCounter = 0;
  spawnDelayFrames = 0;

  holdLeftTime = 0.0f;
  holdRightTime = 0.0f;

  // reset soft drop state
  holdDownTime = 0.0f;
  downBlocked = false;

  // reset line clear anim state
  clearingLines = false;
  clearTimerFrames = 0;
  linesToClearCount = 0;
  blinkFrameCounter = 0;
  blinkOn = false;

  score = 0;
  linesCleared = 0;
  level = 1;
  scrollSpeed = 1;

  combo = -1;
  backToBack = false;

  goFlow = GO_SHOW_GAMEOVER;
  nameInput[0] = '\0';
  nameLen = 0;

  GenerateGrid();
  nextType = RandomType();
}

/* ===================== GAME OVER UI (SAVE SCORE?) ===================== */

static void UpdateGameOverOverlay(Rectangle panel, Rectangle yesBtn, Rectangle noBtn, Rectangle inputBox) {
  Vector2 m = GetMousePosition();

  if (goFlow == GO_ASK_SAVE) {
    if (CheckCollisionPointRec(m, yesBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      goFlow = GO_ENTER_NAME;
      nameInput[0] = '\0';
      nameLen = 0;
    }
    if (CheckCollisionPointRec(m, noBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      goFlow = GO_SHOW_GAMEOVER;
    }
  } else if (goFlow == GO_ENTER_NAME) {
    int c = GetCharPressed();
    while (c > 0) {
      if (c >= 32 && c <= 126) {
        if (nameLen < MAX_NAME_LEN - 1) {
          nameInput[nameLen++] = (char)c;
          nameInput[nameLen] = '\0';
        }
      }
      c = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
      if (nameLen > 0) {
        nameLen--;
        nameInput[nameLen] = '\0';
      }
    }

    if (IsKeyPressed(KEY_ENTER)) {
      const char *finalName = nameInput;
      if (nameLen == 0) finalName = "PLAYER";
      AddScoreToLeaderboard(finalName, score);
      goFlow = GO_SHOW_GAMEOVER;
    }

    (void)panel; (void)inputBox;
  }
}

static void DrawGameOverOverlay(
  int screenWidth, int screenHeight,
  Color hudText, Color highlight
) {
  Rectangle panel = (Rectangle){ 160, 170, 480, 230 };
  DrawRectangleRec(panel, (Color){0, 0, 0, 200});
  DrawRectangleLinesEx(panel, 2, RAYWHITE);

  int cx = screenWidth / 2;

  if (goFlow == GO_ASK_SAVE) {
    const char *q = "Save score?";
    int qW = MeasureText(q, 34);
    DrawText(q, cx - qW/2, (int)panel.y + 25, 34, hudText);

    DrawText(TextFormat("Score: %d", score), (int)panel.x + 30, (int)panel.y + 80, 22, hudText);

    Rectangle yesBtn = (Rectangle){ panel.x + 110, panel.y + 150, 110, 40 };
    Rectangle noBtn  = (Rectangle){ panel.x + panel.width - 220, panel.y + 150, 110, 40 };

    Vector2 m = GetMousePosition();
    Color yesC = CheckCollisionPointRec(m, yesBtn) ? highlight : hudText;
    Color noC  = CheckCollisionPointRec(m, noBtn)  ? highlight : hudText;

    DrawRectangleLinesEx(yesBtn, 2, yesC);
    DrawRectangleLinesEx(noBtn, 2, noC);

    DrawText("YES", (int)yesBtn.x + 30, (int)yesBtn.y + 8, 24, yesC);
    DrawText("NO",  (int)noBtn.x  + 40, (int)noBtn.y  + 8, 24, noC);

    UpdateGameOverOverlay(panel, yesBtn, noBtn, (Rectangle){0});

  } else if (goFlow == GO_ENTER_NAME) {
    const char *t = "Type your name:";
    int tW = MeasureText(t, 28);
    DrawText(t, cx - tW/2, (int)panel.y + 25, 28, hudText);

    DrawText(TextFormat("Score: %d", score), (int)panel.x + 30, (int)panel.y + 70, 22, hudText);

    Rectangle inputBox = (Rectangle){ panel.x + 80, panel.y + 120, panel.width - 160, 45 };

    Vector2 m = GetMousePosition();
    Color boxC = CheckCollisionPointRec(m, inputBox) ? highlight : hudText;

    DrawRectangleLinesEx(inputBox, 2, boxC);
    DrawText(nameInput, (int)inputBox.x + 10, (int)inputBox.y + 10, 24, hudText);

    const char *hint = "Press ENTER to save";
    int hW = MeasureText(hint, 18);
    DrawText(hint, cx - hW/2, (int)panel.y + 180, 18, hudText);

    UpdateGameOverOverlay(panel, (Rectangle){0}, (Rectangle){0}, inputBox);

  } else {
    // GO_SHOW_GAMEOVER: old UI stays in main drawing (kept below)
  }
}

/* ===================== SCORES SCREEN ===================== */

static void UpdateScoresScreen(Rectangle backBtn, Rectangle prevBtn, Rectangle nextBtn,
                               Rectangle clearBtns[PAGE_SIZE], int indices[PAGE_SIZE]) {
  Vector2 m = GetMousePosition();

  if (CheckCollisionPointRec(m, prevBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (scoresPage > 0) scoresPage--;
  }
  if (CheckCollisionPointRec(m, nextBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    int maxPage = (leaderboardCount <= 0) ? 0 : (leaderboardCount - 1) / PAGE_SIZE;
    if (scoresPage < maxPage) scoresPage++;
  }

  for (int i = 0; i < PAGE_SIZE; i++) {
    if (indices[i] < 0) continue;
    if (CheckCollisionPointRec(m, clearBtns[i]) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      RemoveScoreAtIndex(indices[i]);
      break;
    }
  }

  (void)backBtn;
}

/* ===================== AUDIO ===================== */

static void InitGameAudio(void) {
  InitAudioDevice();

  sfxLineClear = LoadSound("songs/lineclear.wav");
  SetSoundVolume(sfxLineClear, 1.0f);
  sfxTetris    = LoadSound("songs/tetris.wav");
  SetSoundVolume(sfxLineClear, 0.9f);
  sfxLineClearReady = true;
  musicNormal = LoadMusicStream("songs/Victor-Severo-Raytetris.ogg");
  musicFast   = LoadMusicStream("songs/Victor-Severo-Raytetris-Brutal.ogg");

  SetMusicVolume(musicNormal, 0.50f);
  SetMusicVolume(musicFast,   0.40f);

  audioReady = true;
  playingFast = false;
}

static void UnloadGameAudio(void) {
  if (!audioReady) return;

  StopMusicStream(musicNormal);
  StopMusicStream(musicFast);

  UnloadMusicStream(musicNormal);
  UnloadMusicStream(musicFast);
  
  if (sfxLineClearReady) {
    UnloadSound(sfxLineClear);
    UnloadSound(sfxTetris);
    sfxLineClearReady = false;
  }
  
  CloseAudioDevice();
  audioReady = false;
}

static bool IsDangerZone(void) {
  for (int y = 0; y <= 6; y++) {
    for (int x = 1; x < COLS - 1; x++) {
      if (grid[x][y] == PLACED_PIECE) return true;
    }
  }
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

static void UpdateGameplayMusic(void) {
  if (!audioReady) return;

  // keep streams updated so the music does not stop suddenly
  if (playingFast) UpdateMusicStream(musicFast);
  else             UpdateMusicStream(musicNormal);

  // changes music state
  if (!itsOver) {
    if (IsDangerZone()) SwitchToFastMusic();
    else                SwitchToNormalMusic();
  } else {
    // stops music at game over
    StopGameplayMusic();
  }
}

/* ===================== MAIN ===================== */

int main(void) {

  ThemeColors Themes[THEME_COUNT] = {
    [PURPLE_THEME]={PURPLE,DARKPURPLE, (Color){150, 28, 176,255}, "Purple"},
    [RED_THEME]={(Color){235, 63, 83,255},(Color){128, 18, 31, 255}, (Color){235, 80, 99, 255}, "Red"},
    [GREEN_THEME]={GREEN, DARKGREEN, LIME, "Green"},
    [BLUE_THEME]={BLUE, DARKBLUE, SKYBLUE, "Blue"},
    [YELLOW_THEME]={YELLOW, GOLD, (Color){223, 230, 41,255}, "Yellow"},
    [ORANGE_THEME]={ORANGE, (Color){230,76,20,255}, (Color){245, 148, 12,255}, "Orange"},
  };

  const int screenWidth = 800;
  const int screenHeight = 600;

  const char *title = "RAYTETRIS";
  const char *gameOver = "GAME OVER";
  const char *restartText = "Click here to restart";
  const int fontSize = 100;

  InitWindow(screenWidth, screenHeight, "Raytetris");
  SetTargetFPS(60);
  InitGameAudio();
  SetRandomSeed((unsigned int)time(NULL));
  nextType = RandomType();

  LoadLeaderboardFromFile();

  MainMenu currentScreen = MAINSCREEN;
  ThemeOptions currentTheme = PURPLE_THEME;

  int textWidth  = MeasureText(title, fontSize);
  int playW      = MeasureText("Play Game", 40);
  int scoresW    = MeasureText("Scores", 40);

  int centerTitle  = (screenWidth - textWidth) / 2;
  int centerPlay   = (screenWidth - playW) / 2;
  int centerScores = (screenWidth - scoresW) / 2;

  Rectangle playButton   = { (float)centerPlay, 240, (float)playW, 40 };
  Rectangle scoresButton = { (float)centerScores, 300, (float)scoresW, 40 };

  while (!WindowShouldClose()) {

    Color bgColor   = Themes[currentTheme].background;
    Color textBase  = Themes[currentTheme].text;
    Color highlight = Themes[currentTheme].highlight;

    Color gameBg      = Mix(bgColor, BLACK, 0.65f);
    Color gridLine    = Mix(textBase, gameBg, 0.70f);
    Color wallColor   = Mix(textBase, gameBg, 0.30f);
    Color activeColor = highlight;
    Color placedColor = Mix(highlight, gameBg, 0.55f);
    Color hudText     = Mix(textBase, RAYWHITE, 0.65f);

    Vector2 mousePoint = GetMousePosition();

    int backW = MeasureText("BACK", 20);
    Rectangle backBtn = { 20, 20, (float)backW, 20 };

    const char *themeLabel = TextFormat("Theme: %s", Themes[currentTheme].name);
    int themeW = MeasureText(themeLabel, 20);
    Rectangle themeButton = { (float)(centerPlay + 25), 550, (float)themeW, 20 };

    switch (currentScreen) {

      case MAINSCREEN: {
        if (CheckCollisionPointRec(mousePoint, playButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          GenerateGrid();

          itsOver = false;
          pieceActive = false;
          frameCounter = 0;
          holdLeftTime = 0.0f;
          holdRightTime = 0.0f;

          holdDownTime = 0.0f;
          downBlocked = false;

	  spawnDelayFrames = 0;

          // reset line clear anim state
          clearingLines = false;
          clearTimerFrames = 0;
          linesToClearCount = 0;
          blinkFrameCounter = 0;
          blinkOn = false;

          score = 0;
          linesCleared = 0;
          level = 1;
          scrollSpeed = 1;

          combo = -1;
          backToBack = false;

          goFlow = GO_SHOW_GAMEOVER;
          nameInput[0] = '\0';
          nameLen = 0;

          nextType = RandomType();

          currentScreen = GAMEPLAY;
          StartGameplayMusic();
        }

        if (CheckCollisionPointRec(mousePoint, scoresButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          scoresPage = 0;
          currentScreen = SCORES;
        }

        if (CheckCollisionPointRec(mousePoint, themeButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          currentTheme = (ThemeOptions)((currentTheme + 1) % THEME_COUNT);
        }
      } break;

      case GAMEPLAY: {
        UpdateGameplay();
        UpdateGameplayMusic();

        if (CheckCollisionPointRec(mousePoint, backBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          StopGameplayMusic();
          currentScreen = MAINSCREEN;
        }

        if (itsOver && goFlow == GO_SHOW_GAMEOVER && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          RestartGame();
          StartGameplayMusic();
        }
      } break;

      case SCORES: {
        if (CheckCollisionPointRec(mousePoint, backBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          currentScreen = MAINSCREEN;
          StopGameplayMusic();
        }

        Rectangle clearBtns[PAGE_SIZE] = {0};
        int idxs[PAGE_SIZE];
        for (int i = 0; i < PAGE_SIZE; i++) idxs[i] = -1;

        int start = scoresPage * PAGE_SIZE;

        for (int row = 0; row < PAGE_SIZE; row++) {
          int idx = start + row;
          if (idx >= leaderboardCount) break;
          idxs[row] = idx;

          int y = 140 + row * 36;
          clearBtns[row] = (Rectangle){ 640, (float)(y - 2), 80, 28 };
        }

        Rectangle prevBtn = { 270, 520, 90, 32 };
        Rectangle nextBtn = { 440, 520, 90, 32 };

        UpdateScoresScreen(backBtn, prevBtn, nextBtn, clearBtns, idxs);
      } break;
    }

    BeginDrawing();

    switch (currentScreen) {

      case MAINSCREEN: {
        ClearBackground(bgColor);

        DrawText(title, centerTitle, 20, fontSize, textBase);

        Color playColor = CheckCollisionPointRec(mousePoint, playButton) ? highlight : textBase;
        DrawText("Play Game", centerPlay, 240, 40, playColor);

        Color scoresColor = CheckCollisionPointRec(mousePoint, scoresButton) ? highlight : textBase;
        DrawText("Scores", centerScores, 300, 40, scoresColor);

        DrawText(themeLabel, centerPlay + 25, 550, 20, textBase);
      } break;

      case GAMEPLAY: {
        ClearBackground(gameBg);

        DrawText("BACK", 20, 20, 20, hudText);

        GridGraphic(gridLine, placedColor, wallColor);
        DrawActivePiece(activeColor);

        DrawText(TextFormat("Score: %d", score), 380, 100, 20, hudText);
        DrawText(TextFormat("Lines: %d", linesCleared), 380, 130, 20, hudText);
        DrawText(TextFormat("Level: %d", level), 380, 160, 20, hudText);

        DrawText("Next:", 380, 210, 20, hudText);
        DrawPiecePreview(nextType, 380, 240, 18, activeColor);

        if (itsOver) {
          if (goFlow != GO_SHOW_GAMEOVER) {
            DrawGameOverOverlay(screenWidth, screenHeight, hudText, highlight);
          } else {
            const int goSize = 50;
            const int restartSize = 20;

            int goWidth = MeasureText(gameOver, goSize);
            int restartWidth = MeasureText(restartText, restartSize);

            int centerX = screenWidth / 2;

            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 130});

            DrawText(gameOver, centerX - goWidth / 2, 230, goSize, highlight);
            DrawText(restartText, centerX - restartWidth / 2, 300, restartSize, hudText);
          }
        }
      } break;

      case SCORES: {
        ClearBackground(bgColor);

        DrawText("BACK", 20, 20, 20, textBase);

        DrawText("LEADERBOARD", 260, 30, 36, textBase);

        int start = scoresPage * PAGE_SIZE;
        int end = start + PAGE_SIZE;
        if (end > leaderboardCount) end = leaderboardCount;

        DrawText("RANK", 140, 115, 18, textBase);
        DrawText("NAME", 240, 115, 18, textBase);
        DrawText("SCORE", 500, 115, 18, textBase);

        Rectangle clearBtns[PAGE_SIZE] = {0};
        int idxs[PAGE_SIZE];
        for (int i = 0; i < PAGE_SIZE; i++) idxs[i] = -1;

        for (int row = 0; row < PAGE_SIZE; row++) {
          int idx = start + row;
          if (idx >= leaderboardCount) break;

          idxs[row] = idx;

          int y = 140 + row * 36;

          DrawText(TextFormat("%d", idx + 1), 145, y, 20, textBase);
          DrawText(leaderboard[idx].name, 240, y, 20, textBase);
          DrawText(TextFormat("%d", leaderboard[idx].value), 500, y, 20, textBase);

          clearBtns[row] = (Rectangle){ 640, (float)(y - 2), 80, 28 };
          bool hover = CheckCollisionPointRec(mousePoint, clearBtns[row]);

          DrawRectangleLinesEx(clearBtns[row], 2, hover ? highlight : textBase);
          DrawText("CLEAR", (int)clearBtns[row].x + 10, (int)clearBtns[row].y + 5, 18, hover ? highlight : textBase);
        }

        Rectangle prevBtn = { 270, 520, 90, 32 };
        Rectangle nextBtn = { 440, 520, 90, 32 };

        int maxPage = (leaderboardCount <= 0) ? 0 : (leaderboardCount - 1) / PAGE_SIZE;

        bool prevHover = CheckCollisionPointRec(mousePoint, prevBtn);
        bool nextHover = CheckCollisionPointRec(mousePoint, nextBtn);

        DrawRectangleLinesEx(prevBtn, 2, prevHover ? highlight : textBase);
        DrawRectangleLinesEx(nextBtn, 2, nextHover ? highlight : textBase);

        DrawText("PREV", (int)prevBtn.x + 18, (int)prevBtn.y + 6, 20, prevHover ? highlight : textBase);
        DrawText("NEXT", (int)nextBtn.x + 18, (int)nextBtn.y + 6, 20, nextHover ? highlight : textBase);

        DrawText(TextFormat("Page %d/%d", scoresPage + 1, maxPage + 1), 345, 560, 18, textBase);

        if (leaderboardCount == 0) {
          DrawText("No scores yet.", 320, 320, 22, textBase);
        }
      } break;
    }

    EndDrawing();
  }

  UnloadGameAudio();
  CloseWindow();
  return 0;
}
