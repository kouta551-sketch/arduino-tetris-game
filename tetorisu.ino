#include <Wire.h>
#include <U8g2lib.h>
#include <avr/pgmspace.h>

// ピン設定 
#define BTN_LEFT   2
#define BTN_RIGHT  3
#define BTN_ROTATE 4
#define BTN_DOWN   5
#define BUZZER     8

#define JOY_X  A0
#define JOY_Y  A1
#define JOY_SW 6

//OLED設定：SH1106 省メモリ版 
U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

//盤面設定
#define BOARD_W 10
#define BOARD_H 20
#define CELL 3

#define ORIGIN_X 8
#define ORIGIN_Y 2

//シーン
#define TITLE 0
#define GAME 1
#define GAMEOVER 2

//ブロック種類
#define PIECE_O 0
#define PIECE_I 1
#define PIECE_T 2
#define PIECE_L 3
#define PIECE_J 4
#define PIECE_S 5
#define PIECE_Z 6

int scene = TITLE;

//盤面
bool board[BOARD_H][BOARD_W];

//現在のブロック
int blockX = 4;
int blockY = 0;
int blockType = PIECE_O;
int blockRot = 0;

int score = 0;

//自動落下
unsigned long lastFallTime = 0;
int fallInterval = 500;

//下ボタン長押し
unsigned long lastDownTime = 0;
int downInterval = 80;

//ジョイスティック入力
unsigned long lastJoyMoveTime = 0;
unsigned long lastJoyDownTime = 0;
int joyMoveInterval = 150;

//ロックディレイ
unsigned long lockStartTime = 0;
int lockDelay = 600;   // ブロック固定までの待ち時間。長くしたいなら700〜1000
bool isLocking = false;

//BGM
const int bgmMelody[] PROGMEM = {
  262, 330, 392, 330,
  294, 349, 440, 349
};

const int BGM_LEN = 8;

int bgmIndex = 0;
unsigned long lastBgmTime = 0;
int bgmInterval = 180;

//効果音中はBGMを一瞬止める
unsigned long sfxUntil = 0;

//ボタンの前回状態
bool oldLeft = HIGH;
bool oldRight = HIGH;
bool oldRotate = HIGH;
bool oldDown = HIGH;
bool oldJoySw = HIGH;

//関数宣言
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

bool isPressed(int pin, bool &oldState);

void playBGM();
void playSfx(int freq, int duration);

void setup() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_ROTATE, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  pinMode(JOY_SW, INPUT_PULLUP);

  pinMode(BUZZER, OUTPUT);

  display.begin();

  // A0はジョイスティックに使うのでanalogRead(A0)は使わない
  randomSeed(micros());

  clearBoard();
}

void loop() {
  updateGame();
  drawGame();
  playBGM();

  delay(10);
}

//ゲーム更新
void updateGame() {
  if (scene == TITLE) {
    if (isPressed(BTN_DOWN, oldDown) || isPressed(JOY_SW, oldJoySw)) {
      startGame();
    }
    return;
  }

  if (scene == GAMEOVER) {
    if (isPressed(BTN_DOWN, oldDown) || isPressed(JOY_SW, oldJoySw)) {
      startGame();
    }
    return;
  }

  if (scene == GAME) {
    //ボタン操作：左移動
    if (isPressed(BTN_LEFT, oldLeft)) {
      if (canPlace(blockX - 1, blockY, blockRot)) {
        blockX--;
        resetLockDelay();
        playSfx(700, 50);
      }
    }

    //ボタン操作：右移動
    if (isPressed(BTN_RIGHT, oldRight)) {
      if (canPlace(blockX + 1, blockY, blockRot)) {
        blockX++;
        resetLockDelay();
        playSfx(700, 50);
      }
    }

    //ボタン操作：回転
    if (isPressed(BTN_ROTATE, oldRotate)) {
      rotateBlock();
    }

    //ボタン操作：下長押し高速落下
    if (digitalRead(BTN_DOWN) == LOW) {
      if (millis() - lastDownTime > downInterval) {
        if (canPlace(blockX, blockY + 1, blockRot)) {
          blockY++;
          resetLockDelay();
          playSfx(400, 30);
        }

        lastDownTime = millis();
        lastFallTime = millis();
      }
    }

    //ジョイスティック操作
    int joyX = analogRead(JOY_X);
    int joyY = analogRead(JOY_Y);

    //ジョイスティック左右移動
    if (millis() - lastJoyMoveTime > joyMoveInterval) {
      if (joyX < 300) {
        if (canPlace(blockX - 1, blockY, blockRot)) {
          blockX--;
          resetLockDelay();
          playSfx(700, 50);
        }
        lastJoyMoveTime = millis();
      } else if (joyX > 700) {
        if (canPlace(blockX + 1, blockY, blockRot)) {
          blockX++;
          resetLockDelay();
          playSfx(700, 50);
        }
        lastJoyMoveTime = millis();
      }
    }

    //ジョイスティック押し込みで回転
    if (isPressed(JOY_SW, oldJoySw)) {
      rotateBlock();
    }

    //ジョイスティック下方向で高速落下
    //もし下に倒して反応しない場合は joyY > 700 を joyY < 300 に変える
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

    //自動落下
    if (millis() - lastFallTime > fallInterval) {
      if (canPlace(blockX, blockY + 1, blockRot)) {
        blockY++;
        resetLockDelay();
      }

      lastFallTime = millis();
    }

    //固定遅延処理
   handleLockDelay();
  }
}

//回転
void rotateBlock() {
  int newRot = (blockRot + 1) % 4;

  //その場で回転できる場合
  if (canPlace(blockX, blockY, newRot)) {
    blockRot = newRot;
    resetLockDelay();
    playSfx(900, 50);
    return;
  }

  //壁際補正：右に1マスずらす
  if (canPlace(blockX + 1, blockY, newRot)) {
    blockX++;
    blockRot = newRot;
    resetLockDelay();
    playSfx(900, 50);
    return;
  }

  //壁際補正：左に1マスずらす
  if (canPlace(blockX - 1, blockY, newRot)) {
    blockX--;
    blockRot = newRot;
    resetLockDelay();
    playSfx(900, 50);
    return;
  }
}

//描画
void drawGame() {
  display.firstPage();
  do {
    drawScreen();
  } while (display.nextPage());
}

void drawScreen() {
  display.setFont(u8g2_font_6x10_tf);

  if (scene == TITLE) {
    display.drawStr(18, 16, "Arduino Tetris");
    display.drawStr(15, 34, "7 blocks + BGM");
    display.drawStr(18, 54, "DOWN or PUSH");
    return;
  }

  if (scene == GAMEOVER) {
    display.drawStr(25, 20, "GAME OVER");
    display.setCursor(25, 40);
    display.print(F("SCORE:"));
    display.print(score);
    return;
  }

  if (scene == GAME) {
    //盤面枠
    display.drawFrame(
      ORIGIN_X - 1,
      ORIGIN_Y - 1,
      BOARD_W * CELL + 2,
      BOARD_H * CELL + 2
    );

    //固定済みブロック
    for (int y = 0; y < BOARD_H; y++) {
      for (int x = 0; x < BOARD_W; x++) {
        if (board[y][x]) {
          drawCell(x, y);
        }
      }
    }

    //落下中ブロック
    drawCurrentBlock();

    //スコア表示
    display.setCursor(50, 15);
    display.print(F("SCORE"));
    display.setCursor(50, 30);
    display.print(score);

    //現在のブロック表示
    display.setCursor(50, 50);

    if (blockType == PIECE_O) {
      display.print(F("O"));
    } else if (blockType == PIECE_I) {
      display.print(F("I"));
    } else if (blockType == PIECE_T) {
      display.print(F("T"));
    } else if (blockType == PIECE_L) {
      display.print(F("L"));
    } else if (blockType == PIECE_J) {
      display.print(F("J"));
    } else if (blockType == PIECE_S) {
      display.print(F("S"));
    } else if (blockType == PIECE_Z) {
      display.print(F("Z"));
    }
  }
}

//ゲーム開始
void startGame() {
  scene = GAME;
  score = 0;
  fallInterval = 500;

  clearBoard();
  spawnBlock();

  lastFallTime = millis();
  lastDownTime = millis();
  lastJoyMoveTime = millis();
  lastJoyDownTime = millis();
  lastBgmTime = millis();

  bgmIndex = 0;
  resetLockDelay();
}

//盤面クリア 
void clearBoard() {
  for (int y= 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      board[y][x] = false;
    }
  }
}

//新しいブロック
void spawnBlock() {
  blockType = random(7);
  blockRot = 0;
  blockX = 3;
  blockY = 0;

  lastFallTime = millis();
  lastDownTime = millis();
  lastJoyDownTime = millis();

  resetLockDelay();

  if (!canPlace(blockX, blockY, blockRot)) {
    scene = GAMEOVER;
    playSfx(200, 500);
  }
}

//ブロック形状
void getBlockCell(int type, int rot, int index, int &x, int &y) {
  rot = rot % 4;
  x = 0;
  y = 0;

  if (type == PIECE_O) {
    if (index == 0) { x = 0; y = 0; }
    if (index == 1) { x = 1; y = 0; }
    if (index == 2) { x = 0; y = 1; }
    if (index == 3) { x = 1; y = 1; }
  }

  else if (type == PIECE_I) {
    if (rot % 2 == 0) {
      if (index == 0) { x = 0; y = 0; }
      if (index == 1) { x = 1; y = 0; }
      if (index == 2) { x = 2; y = 0; }
      if (index == 3) { x = 3; y = 0; }
    } else {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 1; y = 2; }
      if (index == 3) { x = 1; y = 3; }
    }
  }

  else if (type == PIECE_T) {
    if (rot == 0) {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 0; y = 1; }
      if (index == 2) { x = 1; y = 1; }
      if (index == 3) { x = 2; y = 1; }
    } else if (rot == 1) {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 2; y = 1; }
      if (index == 3) { x = 1; y = 2; }
    } else if (rot == 2) {
      if (index == 0) { x = 0; y = 1; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 2; y = 1; }
      if (index == 3) { x = 1; y = 2; }
    } else {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 0; y = 1; }
      if (index == 2) { x = 1; y = 1; }
      if (index == 3) { x = 1; y = 2; }
    }
  }

  else if (type == PIECE_L) {
    if (rot == 0) {
      if (index == 0) { x = 0; y = 0; }
      if (index == 1) { x = 0; y = 1; }
      if (index == 2) { x = 1; y = 1; }
      if (index == 3) { x = 2; y = 1; }
    } else if (rot == 1) {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 2; y = 0; }
      if (index == 2) { x = 1; y = 1; }
      if (index == 3) { x = 1; y = 2; }
    } else if (rot == 2) {
      if (index == 0) { x = 0; y = 1; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 2; y = 1; }
      if (index == 3) { x = 2; y = 2; }
    } else {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 0; y = 2; }
      if (index == 3) { x = 1; y = 2; }
    }
  }

  else if (type == PIECE_J) {
    if (rot == 0) {
      if (index == 0) { x = 2; y = 0; }
      if (index == 1) { x = 0; y = 1; }
      if (index == 2) { x = 1; y = 1; }
      if (index == 3) { x = 2; y = 1; }
    } else if (rot == 1) {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 1; y = 2; }
      if (index == 3) { x = 2; y = 2; }
    } else if (rot == 2) {
      if (index == 0) { x = 0; y = 1; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 2; y = 1; }
      if (index == 3) { x = 0; y = 2; }
    } else {
      if (index == 0) { x = 0; y = 0; }
      if (index == 1) { x = 1; y = 0; }
      if (index == 2) { x = 1; y = 1; }
      if (index == 3) { x = 1; y = 2; }
    }
  }

  else if (type == PIECE_S) {
    if (rot % 2 == 0) {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 2; y = 0; }
      if (index == 2) { x = 0; y = 1; }
      if (index == 3) { x = 1; y = 1; }
    } else {
      if (index == 0) { x = 1; y = 0; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 2; y = 1; }
      if (index == 3) { x = 2; y = 2; }
    }
  }

  else if (type == PIECE_Z) {
    if (rot % 2 == 0) {
      if (index == 0) { x = 0; y = 0; }
      if (index == 1) { x = 1; y = 0; }
      if (index == 2) { x = 1; y = 1; }
      if (index == 3) { x = 2; y = 1; }
    } else {
      if (index == 0) { x = 2; y = 0; }
      if (index == 1) { x = 1; y = 1; }
      if (index == 2) { x = 2; y = 1; }
      if (index == 3) { x = 1; y = 2; }
    }
  }
}

//置けるか判定
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

//固定処理
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

  //スコアが上がるほど落下速度アップ
  fallInterval = 500 - score * 5;

  if (fallInterval < 150) {
    fallInterval = 150;
  }
}

//ライン消去
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

      //消えた行より上を1段下げる
      for (int yy = y; yy > 0; yy--) {
        for (int x = 0; x < BOARD_W; x++) {
          board[yy][x] = board[yy - 1][x];
        }
      }

      //一番上の行を空にする
      for (int x = 0; x < BOARD_W; x++) {
        board[0][x] = false;
      }

      //同じ行をもう一度確認
      y++;
    }
  }

  return cleared;
}

//ロックディレイ
void handleLockDelay() {
  //まだ下に動けるなら固定待ちは不要
  if (canPlace(blockX, blockY + 1, blockRot)) {
    isLocking = false;
    return;
  }

  //下に動けない状態になった瞬間、タイマー開始
  if (!isLocking) {
    isLocking = true;
    lockStartTime = millis();
    return;
  }

  //一定時間経ったら固定
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

//現在ブロック描画
void drawCurrentBlock() {
  for (int i = 0; i < 4; i++) {
    int cx, cy;
    getBlockCell(blockType, blockRot, i, cx, cy);

    drawCell(blockX + cx, blockY + cy);
  }
}

//1マス描画
void drawCell(int x, int y) {
  int px = ORIGIN_X + x * CELL;
  int py = ORIGIN_Y + y * CELL;

  display.drawBox(px, py, CELL, CELL);
}

//1回押した瞬間だけ検出
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

//BGM 
void playBGM() {
  if (scene != GAME) {
    return;
  }

  //効果音中はBGMを少し止める
  if (millis() < sfxUntil) {
    return;
  }

  if (millis() - lastBgmTime > bgmInterval) {
    int freq = pgm_read_word(&bgmMelody[bgmIndex]);

    tone(BUZZER, freq, 90);

    bgmIndex++;
    if (bgmIndex >= BGM_LEN) {
      bgmIndex = 0;
    }

    lastBgmTime = millis();
  }
}

//効果音
void playSfx(int freq, int duration) {
  tone(BUZZER, freq, duration);
  sfxUntil = millis() + duration + 40;
}