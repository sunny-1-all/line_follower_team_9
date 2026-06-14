const int IN1 = 26;
const int IN2 = 27;
const int IN3 = 14;
const int IN4 = 12;

const int ENA = 23;
const int ENB = 22;

const int S1 = 34;
const int S2 = 35;
const int S3 = 32;
const int S4 = 33;
const int S5 = 25;

const int VLS_BUTTON = 4;

const int PWM_FREQ = 20000;
const int PWM_RESOLUTION = 8;

const int BLACK = 0;

// ==================================================
// VLS 起跑設定
// ==================================================

const unsigned long startDelayAfterVLS = 1500;

// ==================================================
// 起步設定
// ==================================================

int startBoostSpeed = 240;
const unsigned long startBoostTime = 280;

// ==================================================
// 一般速度設定
// ==================================================

int forwardSpeed = 190;

int fixSlowSpeed = 115;
int fixFastSpeed = 200;

int hardFixSlowSpeed = 95;
int hardFixFastSpeed = 205;

int stabilizeSpeed = 180;

int searchSlowSpeed = 100;
int searchFastSpeed = 195;

// ==================================================
// 直角彎設定
// ==================================================

int cornerSlowSpeed = 45;
int cornerFastSpeed = 195;

// 進入直角後，至少轉這麼久
const unsigned long minCornerTime = 260;

// 直角最多轉多久
const unsigned long maxCornerTime = 1150;

// 直角候選方向要穩定多久才算可信
const unsigned long cornerCandidateConfirmTime = 100;

// 已確認的直角方向保留多久
const unsigned long confirmedCornerKeepTime = 650;

// 直角彎結束判斷：S3 要連續穩定看到線，才退出直角
const unsigned long centerConfirmTime = 80;

// 紀錄 S3 連續看到線的起始時間
unsigned long centerDetectStartTime = 0;
bool centerDetecting = false;

// ==================================================
// 一般時間設定
// ==================================================

const unsigned long minFixTime = 120;
const unsigned long maxFixTime = 420;
const unsigned long stabilizeTime = 160;
const unsigned long switchSearchTime = 900;

// ==================================================
// 狀態設定
// ==================================================

const int MODE_FOLLOW = 0;
const int MODE_FIX_LEFT = 1;
const int MODE_FIX_RIGHT = 2;
const int MODE_STABILIZE = 3;
const int MODE_LOST = 4;
const int MODE_CORNER_LEFT = 5;
const int MODE_CORNER_RIGHT = 6;

int mode = MODE_FOLLOW;
unsigned long modeStartTime = 0;

int lastDirection = 0;
// -1 = 最後需要往左修
//  1 = 最後需要往右修
//  0 = 還沒有明確方向

// ==================================================
// 直角候選與確認
// ==================================================

int cornerCandidateSide = 0;
// -1 = 左直角候選
//  1 = 右直角候選
//  0 = 沒有候選

unsigned long cornerCandidateStartTime = 0;

int confirmedCornerSide = 0;
// -1 = 已確認左直角
//  1 = 已確認右直角
//  0 = 沒有已確認方向

unsigned long confirmedCornerTime = 0;

bool isBlack(int value) {
  return value == BLACK;
}

// ==================================================
// 函式宣告
// ==================================================

void moveForward(int leftSpeed, int rightSpeed);
void stopMotors();

void waitForVLS();
void delayBeforeStart(unsigned long waitTime);

void fixLeft();
void fixRight();
void fixLeftHard();
void fixRightHard();

void cornerLeft();
void cornerRight();

void searchLine(unsigned long lostTime);
void searchLeftArc();
void searchRightArc();

void updateCornerCandidate(bool b1, bool b2, bool b3, bool b4, bool b5);
bool hasConfirmedCorner();
void clearCornerCandidate();
void clearConfirmedCorner();
void enterCorner(int side);

bool centerConfirmedAfterCorner(bool b3);

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(S1, INPUT);
  pinMode(S2, INPUT);
  pinMode(S3, INPUT);
  pinMode(S4, INPUT);
  pinMode(S5, INPUT);

  pinMode(VLS_BUTTON, INPUT_PULLUP);

  ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);

  stopMotors();

  Serial.println("Line follower ready.");
  Serial.println("Waiting for VLS switch ON...");

  waitForVLS();

  Serial.println("VLS ON. Waiting before start...");
  delayBeforeStart(startDelayAfterVLS);

  Serial.println("Start!");

  moveForward(startBoostSpeed, startBoostSpeed);
  delay(startBoostTime);

  mode = MODE_FOLLOW;
  modeStartTime = millis();
}

void loop() {
  int s1 = digitalRead(S1);
  int s2 = digitalRead(S2);
  int s3 = digitalRead(S3);
  int s4 = digitalRead(S4);
  int s5 = digitalRead(S5);

  bool b1 = isBlack(s1);
  bool b2 = isBlack(s2);
  bool b3 = isBlack(s3);
  bool b4 = isBlack(s4);
  bool b5 = isBlack(s5);

  bool anyBlack = b1 || b2 || b3 || b4 || b5;

  Serial.print("Mode=");
  Serial.print(mode);
  Serial.print(" | Sensors: ");
  Serial.print(s1); Serial.print(" ");
  Serial.print(s2); Serial.print(" ");
  Serial.print(s3); Serial.print(" ");
  Serial.print(s4); Serial.print(" ");
  Serial.print(s5); Serial.print(" | ");

  // ==================================================
  // FOLLOW：正常循跡
  // ==================================================
  if (mode == MODE_FOLLOW) {

    if (anyBlack) {
      updateCornerCandidate(b1, b2, b3, b4, b5);
    }

    // 進直角時常會突然全白。
    // 這時只相信「已確認過的直角方向」
    if (!anyBlack) {
      if (hasConfirmedCorner()) {
        if (confirmedCornerSide == -1) {
          Serial.println("FOLLOW white -> CORNER_LEFT by confirmed candidate");
          enterCorner(-1);
          cornerLeft();
          return;
        }

        if (confirmedCornerSide == 1) {
          Serial.println("FOLLOW white -> CORNER_RIGHT by confirmed candidate");
          enterCorner(1);
          cornerRight();
          return;
        }
      }

      mode = MODE_LOST;
      modeStartTime = millis();
      Serial.println("FOLLOW -> LOST");
      return;
    }

    // 中間有黑線：直接前進
    if (b3) {
      Serial.println("S3 -> forward");
      moveForward(forwardSpeed, forwardSpeed);
    }

    // 線在左邊：進入左修正
    else if (b1 || b2) {
      lastDirection = -1;
      mode = MODE_FIX_LEFT;
      modeStartTime = millis();

      if (b1) {
        Serial.println("S1 -> hard fix left");
        fixLeftHard();
      } else {
        Serial.println("S2 -> fix left");
        fixLeft();
      }
    }

    // 線在右邊：進入右修正
    else if (b4 || b5) {
      lastDirection = 1;
      mode = MODE_FIX_RIGHT;
      modeStartTime = millis();

      if (b5) {
        Serial.println("S5 -> hard fix right");
        fixRightHard();
      } else {
        Serial.println("S4 -> fix right");
        fixRight();
      }
    }
  }

  // ==================================================
  // FIX_LEFT：一般左修正
  // ==================================================
  else if (mode == MODE_FIX_LEFT) {
    unsigned long t = millis() - modeStartTime;

    if (anyBlack) {
      updateCornerCandidate(b1, b2, b3, b4, b5);
    }

    // 左修正中突然全白，通常代表左直角。
    // 直接往左，不重新判斷右邊。
    if (!anyBlack) {
      Serial.println("FIX_LEFT white -> CORNER_LEFT locked");
      enterCorner(-1);
      cornerLeft();
      return;
    }

    if (b3 && t > minFixTime) {
      mode = MODE_STABILIZE;
      modeStartTime = millis();
      Serial.println("FIX_LEFT -> STABILIZE");
      moveForward(stabilizeSpeed, stabilizeSpeed);
      return;
    }

    if (t > maxFixTime) {
      mode = MODE_STABILIZE;
      modeStartTime = millis();
      Serial.println("FIX_LEFT timeout -> STABILIZE");
      moveForward(stabilizeSpeed, stabilizeSpeed);
      return;
    }

    if (b1) {
      fixLeftHard();
    } else {
      fixLeft();
    }
  }

  // ==================================================
  // FIX_RIGHT：一般右修正
  // ==================================================
  else if (mode == MODE_FIX_RIGHT) {
    unsigned long t = millis() - modeStartTime;

    if (anyBlack) {
      updateCornerCandidate(b1, b2, b3, b4, b5);
    }

    // 右修正中突然全白，通常代表右直角。
    // 直接往右，不重新判斷左邊。
    if (!anyBlack) {
      Serial.println("FIX_RIGHT white -> CORNER_RIGHT locked");
      enterCorner(1);
      cornerRight();
      return;
    }

    if (b3 && t > minFixTime) {
      mode = MODE_STABILIZE;
      modeStartTime = millis();
      Serial.println("FIX_RIGHT -> STABILIZE");
      moveForward(stabilizeSpeed, stabilizeSpeed);
      return;
    }

    if (t > maxFixTime) {
      mode = MODE_STABILIZE;
      modeStartTime = millis();
      Serial.println("FIX_RIGHT timeout -> STABILIZE");
      moveForward(stabilizeSpeed, stabilizeSpeed);
      return;
    }

    if (b5) {
      fixRightHard();
    } else {
      fixRight();
    }
  }

  // ==================================================
  // CORNER_LEFT：直角左彎
  // ==================================================
  else if (mode == MODE_CORNER_LEFT) {
    unsigned long t = millis() - modeStartTime;

    // 進入直角後先鎖方向，持續左轉
    cornerLeft();

    // 至少轉滿 minCornerTime 之後，S3 還要連續穩定看到線，才退出直角，避免太早出彎。
    if (t > minCornerTime && centerConfirmedAfterCorner(b3)) {
      mode = MODE_STABILIZE;
      modeStartTime = millis();
      centerDetecting = false;
      clearCornerCandidate();
      clearConfirmedCorner();
      Serial.println("CORNER_LEFT -> STABILIZE");
      moveForward(stabilizeSpeed, stabilizeSpeed);
      return;
    }

    if (t > maxCornerTime) {
      mode = MODE_LOST;
      modeStartTime = millis();
      centerDetecting = false;
      clearCornerCandidate();
      clearConfirmedCorner();
      Serial.println("CORNER_LEFT timeout -> LOST");
      return;
    }
  }

  // ==================================================
  // CORNER_RIGHT：直角右彎
  // ==================================================
  else if (mode == MODE_CORNER_RIGHT) {
    unsigned long t = millis() - modeStartTime;

    // 進入直角後先鎖方向，持續右轉
    cornerRight();

    if (t > minCornerTime && centerConfirmedAfterCorner(b3)) {
      mode = MODE_STABILIZE;
      modeStartTime = millis();
      centerDetecting = false;
      clearCornerCandidate();
      clearConfirmedCorner();
      Serial.println("CORNER_RIGHT -> STABILIZE");
      moveForward(stabilizeSpeed, stabilizeSpeed);
      return;
    }

    if (t > maxCornerTime) {
      mode = MODE_LOST;
      modeStartTime = millis();
      centerDetecting = false;
      clearCornerCandidate();
      clearConfirmedCorner();
      Serial.println("CORNER_RIGHT timeout -> LOST");
      return;
    }
  }

  // ==================================================
  // STABILIZE：修回來後先直走一小段
  // ==================================================
  else if (mode == MODE_STABILIZE) {
    unsigned long t = millis() - modeStartTime;

    if (!anyBlack) {
      mode = MODE_LOST;
      modeStartTime = millis();
      Serial.println("STABILIZE -> LOST");
      return;
    }

    moveForward(stabilizeSpeed, stabilizeSpeed);

    if (t > stabilizeTime) {
      mode = MODE_FOLLOW;
      modeStartTime = millis();
      Serial.println("STABILIZE -> FOLLOW");
    }
  }

  // ==================================================
  // LOST：普通失線，不是已確認直角
  // ==================================================
  else if (mode == MODE_LOST) {

    if (anyBlack) {
      updateCornerCandidate(b1, b2, b3, b4, b5);

      if (b1 || b2) {
        lastDirection = -1;
        mode = MODE_FIX_LEFT;
        modeStartTime = millis();
        Serial.println("LOST -> FIX_LEFT");
        fixLeft();
      }
      else if (b4 || b5) {
        lastDirection = 1;
        mode = MODE_FIX_RIGHT;
        modeStartTime = millis();
        Serial.println("LOST -> FIX_RIGHT");
        fixRight();
      }
      else if (b3) {
        mode = MODE_STABILIZE;
        modeStartTime = millis();
        Serial.println("LOST -> STABILIZE");
        moveForward(stabilizeSpeed, stabilizeSpeed);
      }

      return;
    }

    unsigned long lostTime = millis() - modeStartTime;
    searchLine(lostTime);
  }

  delay(10);
}

// ==================================================
// VLS 船型開關
// ==================================================

void waitForVLS() {
  // INPUT_PULLUP：
  // 船型開關 OFF = HIGH
  // 船型開關 ON  = LOW

  while (digitalRead(VLS_BUTTON) == HIGH) {
    stopMotors();
    delay(10);
  }

  delay(80);

  while (digitalRead(VLS_BUTTON) == HIGH) {
    stopMotors();
    delay(10);
  }
}

void delayBeforeStart(unsigned long waitTime) {
  unsigned long startTime = millis();

  while (millis() - startTime < waitTime) {
    stopMotors();
    delay(10);
  }
}

// ==================================================
// 直角候選方向確認
// ==================================================

void updateCornerCandidate(bool b1, bool b2, bool b3, bool b4, bool b5) {
  // S3 在中間看到線，代表目前還在線上。
  // 這時不確認直角方向，避免直線時記錯。
  if (b3) {
    clearCornerCandidate();
    clearConfirmedCorner();
    return;
  }

  // 左側明顯，右側沒有線：左直角候選
  bool leftCandidate = (b1 || b2) && !b4 && !b5;

  // 右側明顯，左側沒有線：右直角候選
  bool rightCandidate = (b4 || b5) && !b1 && !b2;

  int candidateSide = 0;

  if (leftCandidate && !rightCandidate) {
    candidateSide = -1;
  }
  else if (rightCandidate && !leftCandidate) {
    candidateSide = 1;
  }
  else {
    clearCornerCandidate();
    return;
  }

  // 候選方向改變時，重新計時
  if (cornerCandidateSide != candidateSide) {
    cornerCandidateSide = candidateSide;
    cornerCandidateStartTime = millis();
    return;
  }

  // 候選方向連續穩定一段時間，才確認直角方向
  if (millis() - cornerCandidateStartTime > cornerCandidateConfirmTime) {
    confirmedCornerSide = candidateSide;
    confirmedCornerTime = millis();

    if (candidateSide == -1) {
      Serial.println("confirmed candidate: LEFT");
    } else {
      Serial.println("confirmed candidate: RIGHT");
    }
  }
}

bool hasConfirmedCorner() {
  if (confirmedCornerSide == 0) {
    return false;
  }

  return millis() - confirmedCornerTime < confirmedCornerKeepTime;
}

void clearCornerCandidate() {
  cornerCandidateSide = 0;
}

void clearConfirmedCorner() {
  confirmedCornerSide = 0;
}

void enterCorner(int side) {
  clearCornerCandidate();
  clearConfirmedCorner();
  centerDetecting = false;

  lastDirection = side;
  modeStartTime = millis();

  if (side == -1) {
    mode = MODE_CORNER_LEFT;
  } else if (side == 1) {
    mode = MODE_CORNER_RIGHT;
  }
}

// ==================================================
// 直角出彎中心確認
// ==================================================

bool centerConfirmedAfterCorner(bool b3) {
  // 直角轉彎後，不能只因為 S3 瞬間看到黑線就退出。
  // 必須讓 S3 連續看到黑線一小段時間，才代表車身真的回到新線上。

  if (b3) {
    if (!centerDetecting) {
      centerDetecting = true;
      centerDetectStartTime = millis();
    }

    if (millis() - centerDetectStartTime > centerConfirmTime) {
      return true;
    }
  } else {
    centerDetecting = false;
  }

  return false;
}

// ==================================================
// 修正動作
// ==================================================

void fixLeft() {
  // 往左修：左輪快，右輪慢
  moveForward(fixFastSpeed, fixSlowSpeed);
}

void fixRight() {
  // 往右修：左輪慢，右輪快
  moveForward(fixSlowSpeed, fixFastSpeed);
}

void fixLeftHard() {
  // 大幅往左修
  moveForward(hardFixFastSpeed, hardFixSlowSpeed);
}

void fixRightHard() {
  // 大幅往右修
  moveForward(hardFixSlowSpeed, hardFixFastSpeed);
}

// ==================================================
// 直角彎動作：進入後會鎖定方向
// ==================================================

void cornerLeft() {
  moveForward(cornerFastSpeed, cornerSlowSpeed);
}

void cornerRight() {
  moveForward(cornerSlowSpeed, cornerFastSpeed);
}

// ==================================================
// 失線找線
// ==================================================

void searchLine(unsigned long lostTime) {
  bool reverseSearch = (lostTime / switchSearchTime) % 2 == 1;

  int directionMemory = lastDirection;

  if (directionMemory == 0) {
    directionMemory = 1;
  }

  int searchDirection = -directionMemory;

  if (reverseSearch) {
    searchDirection = directionMemory;
  }

  if (searchDirection < 0) {
    searchLeftArc();
  } else {
    searchRightArc();
  }
}

void searchLeftArc() {
  moveForward(searchFastSpeed, searchSlowSpeed);
}

void searchRightArc() {
  moveForward(searchSlowSpeed, searchFastSpeed);
}

// ==================================================
// 馬達控制
// ==================================================

void moveForward(int leftSpeed, int rightSpeed) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  ledcWrite(ENA, constrain(leftSpeed, 0, 255));
  ledcWrite(ENB, constrain(rightSpeed, 0, 255));
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
}