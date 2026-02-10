#include "raylib.h"
#include <stdbool.h>

#define COLS 12 //2 extra columns to the walls
#define ROWS 21 //1 extra wall to the floor
#define BOARD_X_AXIS 50
#define BOARD_Y_AXIS 70
#define SQUARE_SIZE 24
#define LEFT  -1
#define RIGHT  1

typedef enum CellState{
  EMPTY, MOVING_PIECE, PLACED_PIECE, CLEAN_LINE, BOARD_LIMIT
}CellState;

static CellState grid[COLS][ROWS];

typedef enum MainMenu{
  MAINSCREEN = 0, GAMEPLAY, SETTINGS
}MainMenu;

typedef enum ThemeOptions{
  PURPLE_THEME,RED_THEME, GREEN_THEME,BLUE_THEME, YELLOW_THEME, ORANGE_THEME, THEME_COUNT
}ThemeOptions;

typedef struct ThemeColors{
  Color background;
  Color text;
  Color highlight;
  const char *name;
}ThemeColors;


static float holdLeftTime = 0.0f;
static float holdRightTime = 0.0f;

static const float DAS = 0.15f; // lag to start operating while you press a key (150 ms)
static const float ARR = 0.05f; // lag to make the piece moving without rushing too much(50 ms)

static bool itsOver = false;
static bool pieceActive = false;
static int pieceX = 0;
static int pieceY = 0;

static void GenerateTestPiece(){
  pieceX = (COLS - 2) / 2;
  pieceY = 0;

  //Game over decided at the spawnpoint
  if (grid[pieceX][pieceY] == PLACED_PIECE ||
      grid[pieceX+1][pieceY] == PLACED_PIECE ||
      grid[pieceX][pieceY+1] == PLACED_PIECE ||
      grid[pieceX+1][pieceY+1] == PLACED_PIECE) {
    itsOver = true;
    pieceActive = false;
    return;
  }
  
  //Square piece for tests
  grid[pieceX][pieceY] = MOVING_PIECE;
  grid[pieceX+1][pieceY] = MOVING_PIECE;
  grid[pieceX][pieceY+1] = MOVING_PIECE;
  grid[pieceX+1][pieceY+1] = MOVING_PIECE;
  
  pieceActive = true;
}

static void TickFall(){
  for (int y = ROWS - 2; y >= 0; y--) {
    for (int x = 1; x < COLS - 1; x++) {
      if (grid[x][y] == MOVING_PIECE) {
	//The condition below will verify if there's something that blocks the piece's fall. If so, it turns into a placed piece.
	if (grid[x][y+1] == PLACED_PIECE || grid[x][y+1] == BOARD_LIMIT){
	  for (int yy = 0; yy < ROWS; yy++){
	    for (int xx = 0; xx < COLS; xx++){
	      if (grid[xx][yy] == MOVING_PIECE){
		grid[xx][yy] = PLACED_PIECE;
		pieceActive = false;
	      }
	    }
	  }
	}
      }
    }
  }
  
  //operates from the below to the above. If theres no collision, it keep going down.
  for (int y = ROWS - 2; y >= 0; y--) {
    for (int x = 1; x < COLS - 1; x++) {
      if (grid[x][y] == MOVING_PIECE) {
	grid[x][y+1] = MOVING_PIECE;
	grid[x][y] = EMPTY;
      }
    }
  }
}

static bool CanMove(int direction){
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (grid[x][y] == MOVING_PIECE) {
	int newX = x + direction;
	if (grid[newX][y] == BOARD_LIMIT || grid[newX][y] == PLACED_PIECE) {
	  return false;
	}
      }
    }
  }
  return true;
}

static void MovePiece(int direction) {
  if (!CanMove(direction)) return;
  
  if (direction > 0) {
    //moves from right to left
    for (int y = 0; y < ROWS; y++) {
      for (int x = COLS - 2; x >= 1; x--) {
	if (grid[x][y] == MOVING_PIECE) {
	  grid[x + direction][y] = MOVING_PIECE;
	  grid[x][y] = EMPTY;
	}
      }
    }
  } else {
    // now from left to right
    for (int y = 0; y < ROWS; y++) {
      for (int x = 1; x < COLS - 1; x++) {
	if (grid[x][y] == MOVING_PIECE) {
	  grid[x + direction][y] = MOVING_PIECE;
	  grid[x][y] = EMPTY;
	}
      }
    }
  }
}

static void HandleHorizontalInput(void) {
  float frameTime = GetFrameTime();

  bool left = IsKeyDown(KEY_LEFT);
  bool right = IsKeyDown(KEY_RIGHT);

  //blocks movement if both keys are pressed
  if (left && right) {
    holdLeftTime = 0.0f;
    holdRightTime = 0.0f;
    return;
  }

  if (left) {
    //it will only move once in the first frame of the holding
    if (holdLeftTime == 0.0f) {
      MovePiece(LEFT);
    }

    holdLeftTime += frameTime;

    // after DAS, it will turn the movement auto
    if (holdLeftTime >= DAS) {
      while (holdLeftTime >= DAS + ARR) {
	MovePiece(LEFT);
	holdLeftTime -= ARR;
      }
    }
  } else {
    holdLeftTime = 0.0f;
  }

  if (right) {
    if (holdRightTime == 0.0f) {
      MovePiece(RIGHT);
    }

    holdRightTime += frameTime;

    if (holdRightTime >= DAS) {
      while (holdRightTime >= DAS + ARR) {
	MovePiece(RIGHT);
	holdRightTime -= ARR;
      }
    }
  } else {
    holdRightTime = 0.0f;
  }
}


static void GenerateGrid(){
  for(int x = 0; x < COLS; x++){
    for(int y = 0; y < ROWS; y++){
      if (x == 0 || x == COLS - 1 || y == ROWS - 1){
        grid[x][y] = BOARD_LIMIT;
      } else {
        grid[x][y] = EMPTY;
      }
    }
  }
}

static void GridGraphic(){
  for (int y = 0; y < ROWS; y++)
      {
      for (int x = 0; x < COLS; x++)
	  {
	  int xPos = BOARD_X_AXIS + x * SQUARE_SIZE;
            int yPos = BOARD_Y_AXIS + y * SQUARE_SIZE;
	    
            switch (grid[x][y])
	      {
	      case EMPTY:
		DrawRectangleLines(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, LIGHTGRAY);
		break;
		
	      case MOVING_PIECE:
		DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, DARKGRAY);
		break;
		
	      case PLACED_PIECE:
		DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, GRAY);
		break;
		
	      case CLEAN_LINE:
		DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, ORANGE);
		break;
		
	      case BOARD_LIMIT:
		DrawRectangle(xPos, yPos, SQUARE_SIZE, SQUARE_SIZE, LIGHTGRAY);
		break;	  
	      }
        }
    }
}

static int frameCounter=0;

static int scrollSpeed = 5;

static void UpdateGameplay(){
  
  if(itsOver){
    return;   
  }

  if (pieceActive) {
    HandleHorizontalInput();
  }

  
  if (!pieceActive) {
    GenerateTestPiece();
  }
  
  if (itsOver){
    return;
  }
  
  frameCounter+=scrollSpeed;
  if (frameCounter >= 60) {
    frameCounter = 0;
    TickFall();
  }
  
}

static void RestartGame(){
  itsOver = false;
  pieceActive = false;
  frameCounter = 0;

  GenerateGrid();
}


int main() {

  ThemeColors Themes[THEME_COUNT] = {
    [PURPLE_THEME]={PURPLE, DARKPURPLE, BLACK, "Purple"},
    [RED_THEME]={RED, MAROON, RAYWHITE, "Red"},
    [GREEN_THEME]={GREEN, DARKGREEN, LIME, "Green"},
    [BLUE_THEME]={BLUE, DARKBLUE, SKYBLUE, "Blue"},
    [YELLOW_THEME]={YELLOW, GOLD, (Color){111,118,17,255}, "Yellow"},
    [ORANGE_THEME]={ORANGE, (Color){230,76,20,255},(Color){168,63,24,255} , "Orange"},
  };
  
  const int screenWidth = 800;
  const int screenHeight = 600;
  const char *title = "TETRIS BETA V0.1 by edutavr";
  const char *gameOver = "GAME OVER";
  const char *restartText="Click here to restart";
  int fontSize = 45; 

  InitWindow(screenWidth, screenHeight, "TETRIS BETA V0.1");

  MainMenu currentScreen = MAINSCREEN;
  
  ThemeOptions currentTheme = PURPLE_THEME;
  
  SetTargetFPS(60);
  
  int textWidth = MeasureText(title, fontSize);
  int textWidth2 = MeasureText("Play Game", 40);
  int textWidth3 = MeasureText("Settings", 40);

  
  int centerTitle = (screenWidth - textWidth)/2;
  int centerPlay = (screenWidth - textWidth2)/2;
  int centerSettings = (screenWidth - textWidth3)/2;

  int goSize;
  int restartSize;

  Rectangle playButton = { centerPlay, 240, textWidth2, 40 };
  Rectangle settingsButton = { centerSettings, 300, textWidth3, 40 };

  
  while (!WindowShouldClose()) {

    Color bgColor = Themes[currentTheme].background;
    Color textBase = Themes[currentTheme].text;
    Color highlight = Themes[currentTheme].highlight;

    Vector2 mousePoint = GetMousePosition();

    int textWidth5 = MeasureText("TELA DO JOGO",20);
    Rectangle backToTheBegginning={20,20,textWidth5,20};

    const char *themeLabel = TextFormat("Theme: %s", Themes[currentTheme].name);
    int textWidth4 = MeasureText(themeLabel, 20);
    Rectangle themeButton = {centerPlay+25, 550, textWidth4, 20};
      
    Color titleColor = textBase;
    Color playColor = textBase;
    Color settingsColor = textBase;
    Color themeColor = textBase;

    switch(currentScreen){
     case MAINSCREEN:{
       if (CheckCollisionPointRec(mousePoint, playButton)) {
  	 if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	   GenerateGrid();
	   currentScreen = GAMEPLAY;
	 }
       }
       if (CheckCollisionPointRec(mousePoint, settingsButton)) {
	 if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	   currentScreen = SETTINGS;
	 }
       }
       if (CheckCollisionPointRec(mousePoint, themeButton)) {
	 if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	   currentTheme = (ThemeOptions)((currentTheme + 1) % THEME_COUNT);
	 }
       }
     }break;
     case GAMEPLAY:{
       UpdateGameplay();
       
       if (CheckCollisionPointRec(mousePoint, backToTheBegginning)) {
	 if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	   currentScreen = MAINSCREEN;
	 }
       }
       
       if (itsOver && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	 RestartGame();
       }
       
       if (CheckCollisionPointRec(mousePoint, backToTheBegginning)) {
	 if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	   currentScreen = MAINSCREEN;
	 }
       }
     }break;
    }
    BeginDrawing();
    switch(currentScreen){
    //draw-menu-----------------------------------------------------------
    case(MAINSCREEN):      
      ClearBackground(bgColor);
      DrawText(title, centerTitle, 20, fontSize, titleColor);
      if (CheckCollisionPointRec(mousePoint, playButton)) {
	playColor = highlight;
      }
      DrawText("Play Game", centerPlay , 240, 40, playColor);
      if (CheckCollisionPointRec(mousePoint, settingsButton)) {
	settingsColor = highlight;
      }
      DrawText("Settings", centerSettings , 300, 40, settingsColor);
      DrawText(themeLabel, centerPlay + 25 , 550, 20, themeColor);
      break;
      
case (GAMEPLAY): {
    ClearBackground(BLACK);
    DrawText("TELA DO JOGO", 20, 20, 20, WHITE);
    GridGraphic();

    if (itsOver) {
        const int goSize = 50;
        const int restartSize = 20;

        int goWidth = MeasureText(gameOver, goSize);
        int restartWidth = MeasureText(restartText, restartSize);

        int centerX = screenWidth / 2;

        DrawRectangle(180, 200, 440, 180, (Color){0, 0, 0, 200});
        DrawRectangleLines(180, 200, 440, 180, RAYWHITE);

        DrawText(gameOver, centerX - goWidth / 2 ,230, goSize, RED);

        DrawText(restartText, centerX - restartWidth / 2, 300, restartSize, RAYWHITE);
    }

    break;
}


    case(SETTINGS):
      ClearBackground(bgColor);
      DrawText("SETTINGS", 20, 20, 40, DARKPURPLE);
      break;      
    }
    EndDrawing();
    //draw-menu----------------------------------------------------------- 
   
  }
  
  CloseWindow();
  return 0;
}
