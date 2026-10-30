#include <Wire.h>
#include <U8g2lib.h>
#include <avr/pgmspace.h>
#include <TimerOne.h>

#define BTN_ROTATE 4
#define BUZZER     8

#define JOY_X  A0
#define JOY_Y  A1
#define JOY_SW 6

U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

const byte segPins[7] = {
  2,   //a
  3,   // b
  5,   // c
  7,   // d
  9,   // e
  10,  // f
  11   // g
};

// digit1,digit2,digit3,digit4
const byte digitPins[4] = {
  12,  // digit1
  13,  // digit2
  A2,  // digit3
  A3   // digit4
};

const bool COMMON_ANODE = false;

//数字のパターン a,b,c,d,e,f,g
const byte numbers[10][7] PROGMEM = {
  {1,1,1,1,1,1,0}, // 0
  {0,1,1,0,0,0,0}, // 1
  {1,1,0,1,1,0,1}, // 2
  {1,1,1,1,0,0,1}, // 3
  {0,1,1,0,0,1,1}, // 4
  {1,0,1,1,0,1,1}, // 5
  {1,0,1,1,1,1,1}, // 6
  {1,1,1,0,0,0,0}, // 7
  {1,1,1,1,1,1,1}, // 8
  {1,1,1,1,0,1,1}  // 9
};

volatile byte displayDigits[4] = {0, 0, 0, 0};
volatile byte currentDigit = 0;

#define BOARD_W 16
#define BOARD_H 20
#define CELL 3

#define ORIGIN_X 3
#define ORIGIN_Y 2

#define NEXT_X 58
#define NEXT_Y 18
#define PREVIEW_CELL 4

#define TITLE 0
#define GAME 1
#define GAMEOVER 2

#define PIECE_O 0
#define PIECE_I 1
#define PIECE_T 2
#define PIECE_L 3
#define PIECE_J 4
#define PIECE_S 5
#define PIECE_Z 6

const int8_t pieceCells[7][4][4][2] PROGMEM = {
  // O
  {
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}}
  },

  // I
  {
    {{0,0},{1,0},{2,0},{3,0}},
    {{1,0},{1,1},{1,2},{1,3}},
    {{0,0},{1,0},{2,0},{3,0}},
    {{1,0},{1,1},{1,2},{1,3}}
  },

  // T
  {
    {{1,0},{0,1},{1,1},{2,1}},
    {{1,0},{1,1},{2,1},{1,2}},
    {{0,1},{1,1},{2,1},{1,2}},
    {{1,0},{0,1},{1,1},{1,2}}
  },

  // L
  {
    {{0,0},{0,1},{1,1},{2,1}},
    {{1,0},{2,0},{1,1},{1,2}},
    {{0,1},{1,1},{2,1},{2,2}},
    {{1,0},{1,1},{0,2},{1,2}}
  },

  // J
  {
    {{2,0},{0,1},{1,1},{2,1}},
    {{1,0},{1,1},{1,2},{2,2}},
    {{0,1},{1,1},{2,1},{0,2}},
    {{0,0},{1,0},{1,1},{1,2}}
  },

  // S
  {
    {{1,0},{2,0},{0,1},{1,1}},
    {{1,0},{1,1},{2,1},{2,2}},
    {{1,0},{2,0},{0,1},{1,1}},
    {{1,0},{1,1},{2,1},{2,2}}
  },

  // Z
  {
    {{0,0},{1,0},{1,1},{2,1}},
    {{2,0},{1,1},{2,1},{1,2}},
    {{0,0},{1,0},{1,1},{2,1}},
    {{2,0},{1,1},{2,1},{1,2}}
  }
};

int scene = TITLE;

bool board[BOARD_H][BOARD_W];

int blockX = 4;
int blockY = 0;
int blockType = PIECE_O;
int blockRot = 0;

int nextBlockType = PIECE_O;

int score = 0;

unsigned long lastOledDrawTime = 0;
int oledDrawInterval = 80;

unsigned long lastFallTime = 0;
int fallInterval = 500;

unsigned long lastJoyDownTime = 0;
int downInterval = 80;

unsigned long lastJoyMoveTime = 0;
int joyMoveInterval = 150;

unsigned long lockStartTime = 0;
int lockDelay = 600;
bool isLocking = false;

const int bgmMelody[] PROGMEM = {
  262, 330, 392, 330,
  294, 349, 440, 349
};

const int BGM_LEN = 8;

int bgmIndex = 0;
unsigned long lastBgmTime = 0;
int bgmInterval = 240;

unsigned long sfxUntil = 0;

bool oldRotate = HIGH;
bool oldJoySw = HIGH;

#define MASCOT_IDLE     0
#define MASCOT_PLACE    1
#define MASCOT_CLEAR    2
#define MASCOT_DANGER   3
#define MASCOT_GAMEOVER 4
#define MASCOT_HAPPY 5
byte mascotState = MASCOT_IDLE;
unsigned long mascotStateStart = 0;

void updateGame();
void drawGame();
void drawScreen();

void startGame();
void clearBoard();
void spawnBlock();

void rotateBlock();
void getBlockCell(int type, int rot, int index, int &x, int &y);
bool canPlace(int newBlockX, int newBlockY, int newRot);

void fixBlock();
int clearLines();

void handleLockDelay();
void resetLockDelay();

void drawCurrentBlock();
void drawCell(int x, int y);
void drawNextBlock(int type, int px, int py);
void drawPreviewCell(int x, int y, int px, int py);

bool isPressed(int pin, bool &oldState);

void playBGM();
void playSfx(int freq, int duration);

void init7Seg();
void refresh7Seg();
void update7SegScore(int value);
void setSegments(int num);
void digitOn(int digit);
void allDigitsOff();
void allSegmentsOff();

void setMascotState(byte state);
void updateMascot();
bool isDangerous();
void drawCharacter();
int nextHappyScore = 100;

void setup() {
  pinMode(BTN_ROTATE, INPUT_PULLUP);
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  init7Seg();

  Timer1.initialize(5000);
  Timer1.attachInterrupt(refresh7Seg);

  display.begin();

  randomSeed(micros());

  clearBoard();
  update7SegScore(score);

  drawGame();
}

void loop() {
  updateGame();

  updateMascot();

  playBGM();

  update7SegScore(score);

  if (millis() - lastOledDrawTime > oledDrawInterval) {
    drawGame();
    lastOledDrawTime = millis();
  }
}

void updateGame() {
  if (scene == TITLE) {
    bool rotateStart = isPressed(BTN_ROTATE, oldRotate);
    bool joyStart = isPressed(JOY_SW, oldJoySw);

    if (rotateStart || joyStart) {
      startGame();
    }
    return;
  }

  if (scene == GAMEOVER) {
    bool rotateStart = isPressed(BTN_ROTATE, oldRotate);
    bool joyStart = isPressed(JOY_SW, oldJoySw);

    if (rotateStart || joyStart) {
      startGame();
    }
    return;
  }

  if (scene == GAME) {
    if (isPressed(BTN_ROTATE, oldRotate)) {
      rotateBlock();
    }

    int joyX = analogRead(JOY_X);
    int joyY = analogRead(JOY_Y);

    if (millis() - lastJoyMoveTime > joyMoveInterval) {
      if (joyX < 300) {
        if (canPlace(blockX - 1, blockY, blockRot)) {
          blockX--;
          resetLockDelay();
          playSfx(700, 50);
        }
        lastJoyMoveTime = millis();
      } 
      else if (joyX > 700) {
        if (canPlace(blockX + 1, blockY, blockRot)) {
          blockX++;
          resetLockDelay();
          playSfx(700, 50);
        }
        lastJoyMoveTime = millis();
      }
    }

    if (isPressed(JOY_SW, oldJoySw)) {
      rotateBlock();
    }

    if (joyY > 700) {
      if (millis() - lastJoyDownTime > downInterval) {
        if (canPlace(blockX, blockY + 1, blockRot)) {
          blockY++;
          resetLockDelay();
          playSfx(400, 30);
        }

        lastJoyDownTime = millis();
        lastFallTime = millis();
      }
    }

    if (millis() - lastFallTime > fallInterval) {
      if (canPlace(blockX, blockY + 1, blockRot)) {
        blockY++;
        resetLockDelay();
      }

      lastFallTime = millis();
    }

    handleLockDelay();
  }
}

void rotateBlock() {
  int newRot = (blockRot + 1) % 4;

  if (canPlace(blockX, blockY, newRot)) {
    blockRot = newRot;
    resetLockDelay();
    playSfx(900, 50);
    return;
  }

  if (canPlace(blockX + 1, blockY, newRot)) {
    blockX++;
    blockRot = newRot;
    resetLockDelay();
    playSfx(900, 50);
    return;
  }

  if (canPlace(blockX - 1, blockY, newRot)) {
    blockX--;
    blockRot = newRot;
    resetLockDelay();
    playSfx(900, 50);
    return;
  }
}

void drawGame() {
  display.firstPage();
  do {
    drawScreen();
  } while (display.nextPage());
}

void drawScreen() {
  display.setFont(u8g2_font_6x10_tf);

  if (scene == TITLE) {
    // 外枠
  display.drawFrame(2, 2, 124, 60);

  display.drawBox(10, 10, 5, 5);
  display.drawBox(16, 10, 5, 5);
  display.drawBox(16, 16, 5, 5);

  display.drawBox(107, 10, 5, 5);
  display.drawBox(113, 10, 5, 5);
  display.drawBox(107, 16, 5, 5);

  display.setFont(u8g2_font_7x13B_tf);
  display.drawStr(25, 25, "BLOCK");
  display.drawStr(32, 40, "DROP");

  //サブタイトル
  display.setFont(u8g2_font_5x8_tf);
  display.drawStr(28, 50, "ARDUINO GAME");

  //スタート表示
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(20, 61, "PUSH TO START");

  return;
  }

  if (scene == GAMEOVER) {
    display.drawStr(25, 20, "GAME OVER");
    display.drawStr(18, 42, "PUSH to RETRY");

    drawCharacter();

    return;
  }

  if (scene == GAME) {
    // 盤面枠
    display.drawFrame(
      ORIGIN_X - 1,
      ORIGIN_Y - 1,
      BOARD_W * CELL + 2,
      BOARD_H * CELL + 2
    );

    for (int y = 0; y < BOARD_H; y++) {
      for (int x = 0; x < BOARD_W; x++) {
        if (board[y][x]) {
          drawCell(x, y);
        }
      }
    }

    drawCurrentBlock();

    // NEXT表示
    display.drawStr(NEXT_X, 10, "NEXT");
    drawNextBlock(nextBlockType, NEXT_X, NEXT_Y);

    // キャラクター表示
    drawCharacter();
  }
}

void startGame() {
  scene = GAME;
  score = 0;
  nextHappyScore = 100;
  fallInterval = 500;

  setMascotState(MASCOT_IDLE);
  clearBoard();
  nextBlockType = random(7);
  spawnBlock();

  lastFallTime = millis();
  lastJoyMoveTime = millis();
  lastJoyDownTime = millis();
  lastBgmTime = millis();
  lastOledDrawTime = 0;

  bgmIndex = 0;
  resetLockDelay();

  update7SegScore(score);
  setMascotState(MASCOT_IDLE);
}

void clearBoard() {
  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      board[y][x] = false;
    }
  }
}

void spawnBlock() {
  blockType = nextBlockType;
  nextBlockType = random(7);

  blockRot = 0;
  blockX = BOARD_W / 2 - 2;
  blockY = 0;

  lastFallTime = millis();
  lastJoyDownTime = millis();

  resetLockDelay();

  if (!canPlace(blockX, blockY, blockRot)) {
    scene = GAMEOVER;
    setMascotState(MASCOT_GAMEOVER);
    playSfx(200, 500);
  }
}

void getBlockCell(int type, int rot, int index, int &x, int &y) {
  rot = rot % 4;

  x = pgm_read_byte(&pieceCells[type][rot][index][0]);
  y = pgm_read_byte(&pieceCells[type][rot][index][1]);
}

bool canPlace(int newBlockX, int newBlockY, int newRot) {
  for (int i = 0; i < 4; i++) {
    int cx, cy;
    getBlockCell(blockType, newRot, i, cx, cy);

    int x = newBlockX + cx;
    int y = newBlockY + cy;

    if (x < 0 || x >= BOARD_W) return false;
    if (y < 0 || y >= BOARD_H) return false;
    if (board[y][x]) return false;
  }

  return true;
}

void fixBlock() {
  for (int i = 0; i < 4; i++) {
    int cx, cy;
    getBlockCell(blockType, blockRot, i, cx, cy);

    int x = blockX + cx;
    int y = blockY + cy;

    if (x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H) {
      board[y][x] = true;
    }
  }

  int cleared = clearLines();

  if (cleared > 0) {
    score += cleared * 10;
    playSfx(1200, 150);
  } else {
    score++;
    playSfx(1000, 80);
  }

bool reachedHappyScore = false;

while (score >= nextHappyScore) {
  reachedHappyScore = true;
  nextHappyScore += 100;
}

if (reachedHappyScore) {
  setMascotState(MASCOT_HAPPY);
}
else if (isDangerous()) {
  setMascotState(MASCOT_DANGER);
}
else if (cleared > 0) {
  setMascotState(MASCOT_CLEAR);
}
else {
  setMascotState(MASCOT_PLACE);
}

  while (score >= nextHappyScore) {
    reachedHappyScore = true;
    nextHappyScore += 100;
  }

  //スコアが上がるほど落下速度アップ
  fallInterval = 500 - score * 5;

  if (fallInterval < 150) {
    fallInterval = 150;
  }

  update7SegScore(score);

if (isDangerous()) {
  setMascotState(MASCOT_DANGER);
}
else if (cleared > 0) {
  setMascotState(MASCOT_CLEAR);
}
else {
  setMascotState(MASCOT_PLACE);
}
}

int clearLines() {
  int cleared = 0;

  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;

    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x] == false) {
        full = false;
        break;
      }
    }

    if (full) {
      cleared++;

      for (int yy = y; yy > 0; yy--) {
        for (int x = 0; x < BOARD_W; x++) {
          board[yy][x] = board[yy - 1][x];
        }
      }

      for (int x = 0; x < BOARD_W; x++) {
        board[0][x] = false;
      }

      y++;
    }
  }

  return cleared;
}

void handleLockDelay() {
  if (canPlace(blockX, blockY + 1, blockRot)) {
    isLocking = false;
    return;
  }

  if (!isLocking) {
    isLocking = true;
    lockStartTime = millis();
    return;
  }

  if (millis() - lockStartTime >= lockDelay) {
    fixBlock();
    spawnBlock();
    isLocking = false;
  }
}

void resetLockDelay() {
  isLocking = false;
  lockStartTime = millis();
}

void drawCurrentBlock() {
  for (int i = 0; i < 4; i++) {
    int cx, cy;
    getBlockCell(blockType, blockRot, i, cx, cy);

    drawCell(blockX + cx, blockY + cy);
  }
}

void drawCell(int x, int y) {
  int px = ORIGIN_X + x * CELL;
  int py = ORIGIN_Y + y * CELL;

  display.drawBox(px, py, CELL, CELL);
}

void drawNextBlock(int type, int px, int py) {
  for (int i = 0; i < 4; i++) {
    int cx, cy;

    getBlockCell(type, 0, i, cx, cy);

    drawPreviewCell(cx, cy, px, py);
  }
}

void drawPreviewCell(int x, int y, int px, int py) {
  int drawX = px + x * PREVIEW_CELL;
  int drawY = py + y * PREVIEW_CELL;

  display.drawBox(drawX, drawY, PREVIEW_CELL, PREVIEW_CELL);
}

bool isPressed(int pin, bool &oldState) {
  bool nowState = digitalRead(pin);

  if (nowState == LOW && oldState == HIGH) {
    oldState = LOW;
    return true;
  }

  if (nowState == HIGH) {
    oldState = HIGH;
  }

  return false;
}

void playBGM() {
  if (scene != GAME) {
    return;
  }

  if (millis() < sfxUntil) {
    return;
  }

  if (millis() - lastBgmTime > bgmInterval) {
    int freq = pgm_read_word(&bgmMelody[bgmIndex]);

    tone(BUZZER, freq, 60);

    bgmIndex++;
    if (bgmIndex >= BGM_LEN) {
      bgmIndex = 0;
    }

    lastBgmTime = millis();
  }
}

void playSfx(int freq, int duration) {
  tone(BUZZER, freq, duration);
  sfxUntil = millis() + duration + 40;
}

void init7Seg() {
  for (int i = 0; i < 7; i++) {
    pinMode(segPins[i], OUTPUT);
  }

  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
  }

  allDigitsOff();
  allSegmentsOff();
}

void update7SegScore(int value) {
  if (value < 0) value = 0;
  if (value > 9999) value = 9999;

  byte d0 = value / 1000;
  byte d1 = (value / 100) % 10;
  byte d2 = (value / 10) % 10;
  byte d3 = value % 10;

  noInterrupts();
  displayDigits[0] = d0;
  displayDigits[1] = d1;
  displayDigits[2] = d2;
  displayDigits[3] = d3;
  interrupts();
}

void refresh7Seg() {
  allDigitsOff();

  byte num = displayDigits[currentDigit];

  setSegments(num);
  digitOn(currentDigit);

  currentDigit++;
  if (currentDigit >= 4) {
    currentDigit = 0;
  }
}

void setSegments(int num) {
  for (int i = 0; i < 7; i++) {
    byte on = pgm_read_byte(&numbers[num][i]);

    if (on == 1) {
      digitalWrite(segPins[i], COMMON_ANODE ? LOW : HIGH);
    } else {
      digitalWrite(segPins[i], COMMON_ANODE ? HIGH : LOW);
    }
  }
}

void digitOn(int digit) {
  digitalWrite(digitPins[digit], COMMON_ANODE ? HIGH : LOW);
}

void allDigitsOff() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(digitPins[i], COMMON_ANODE ? LOW : HIGH);
  }
}

void allSegmentsOff() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(segPins[i], COMMON_ANODE ? HIGH : LOW);
  }
}

void setMascotState(byte state) {
  mascotState = state;
  mascotStateStart = millis();
}

void updateMascot() {
  unsigned long elapsed = millis() - mascotStateStart;

  if (mascotState == MASCOT_PLACE && elapsed > 350) {
    mascotState = MASCOT_IDLE;
  }

  if (mascotState == MASCOT_CLEAR && elapsed > 650) {
    mascotState = MASCOT_IDLE;
  }

  else if (mascotState == MASCOT_HAPPY &&
         millis() - mascotStateStart > 1200) {
  setMascotState(MASCOT_IDLE);
}

}

bool isDangerous() {
  
  for (int y = 0; y < 10; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x]) {
        return true;
      }
    }
  }

  return false;
}

void drawCharacter() {
  int x = 78;
  int y = 29;

if (scene == GAMEOVER) {
  x = 88;
  y = 35;
}

  unsigned long elapsed = millis() - mascotStateStart;

  int jump = 0;
  int shake = 0;

  bool armsUp = false;
  bool worried = false;
  bool sad = false;

  if (mascotState == MASCOT_PLACE) {
    jump = (elapsed / 100) % 2 == 0 ? 3 : 0;
    armsUp = true;
  }

   else if (mascotState == MASCOT_HAPPY) {
   jump = (elapsed / 80) % 2 == 0 ? 9 : 0;
   armsUp = true;
}

  else if (mascotState == MASCOT_CLEAR) {
    jump = (elapsed / 100) % 2 == 0 ? 6 : 1;
    armsUp = true;
  }

  else if (mascotState == MASCOT_DANGER) {
    shake = (elapsed / 50) % 2 == 0 ? -2 : 2;
    worried = true;
  }

  else if (mascotState == MASCOT_GAMEOVER) {
    sad = true;
  }

  int drawX = x + shake;
  int drawY = y - jump;

  display.drawVLine(drawX + 18, drawY, 4);
  display.drawPixel(drawX + 18, drawY - 1);

  display.drawFrame(drawX + 6, drawY + 4, 24, 16);

  display.drawBox(drawX + 1, drawY + 9, 5, 6);
  display.drawBox(drawX + 30, drawY + 9, 5, 6);

  if (sad) {
    display.drawLine(drawX + 12, drawY + 10,
                     drawX + 16, drawY + 14);

    display.drawLine(drawX + 16, drawY + 14,
                     drawX + 20, drawY + 10);

    display.drawLine(drawX + 22, drawY + 10,
                     drawX + 26, drawY + 14);
  }
  else {
    display.drawBox(drawX + 12, drawY + 10, 4, 4);
    display.drawBox(drawX + 22, drawY + 10, 4, 4);
  }

  if (sad || worried) {
    display.drawLine(drawX + 14, drawY + 18,
                     drawX + 22, drawY + 17);
  }
  else {
    display.drawLine(drawX + 14, drawY + 16,
                     drawX + 18, drawY + 18);
    display.drawLine(drawX + 18, drawY + 18,
                     drawX + 22, drawY + 16);
  }

  display.drawFrame(drawX + 9, drawY + 21, 18, 10);

  if (armsUp) {
    display.drawLine(drawX + 9, drawY + 24,
                     drawX + 2, drawY + 17);

    display.drawLine(drawX + 27, drawY + 24,
                     drawX + 34, drawY + 17);
  }
  else {
    display.drawLine(drawX + 9, drawY + 24,
                     drawX + 2, drawY + 29);

    display.drawLine(drawX + 27, drawY + 24,
                     drawX + 34, drawY + 29);
  }

  display.drawVLine(drawX + 14, drawY + 31, 3);
  display.drawVLine(drawX + 22, drawY + 31, 3);

  if (worried) {
    display.drawVLine(drawX + 38, drawY + 7, 7);
    display.drawPixel(drawX + 38, drawY + 16);
  }
}
