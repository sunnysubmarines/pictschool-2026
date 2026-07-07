#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

extern "C" {
#include "movement_layer.h"
}

const char* WIFI_SSID = "ITMOrobots";
const char* WIFI_PASSWORD = "ITMOrobots";

const uint16_t COMMAND_PORT = 5055;
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 5000;
const unsigned long CLIENT_READ_TIMEOUT_MS = 1500;
const unsigned long CLIENT_READ_IDLE_MS = 100;
const unsigned long ENCODER_STARTUP_IGNORE_MS = 80;
const int MAX_REQUEST_LENGTH = 8192;
const int MAX_SCHOOL_COMMANDS = 5;
const int MAX_PLATFORM_COMMANDS = 64;
const bool EXECUTE_AGENT_COMMANDS = false;
const char* DEVICE_ACTOR = "agent";

WiFiServer commandServer(COMMAND_PORT);
WebServer httpServer(80);
Preferences preferences;

#define IN1 18
#define IN2 19
#define IN3 21
#define IN4 22

#define ENA 25
#define ENB 26

#define LEFT_ENCODER 32
#define RIGHT_ENCODER 33

volatile long leftTicks = 0;
volatile long rightTicks = 0;

int motorSpeed = 430;
int minMotorSpeed = 300;
int maxMotorSpeed = 650;

int leftTrim = 0;
int rightTrim = 0;

int currentLeftPwm = 0;
int currentRightPwm = 0;

int targetLeftPwm = 0;
int targetRightPwm = 0;

int encoderLeftCorrection = 0;
int encoderRightCorrection = 0;

float pidKp = 0.80;
float pidKi = 0.00;
float pidKd = 0.20;

float pidIntegral = 0;
float lastPidError = 0;

int maxCorrection = 180;

int rampStep = 15;
int rampIntervalMs = 15;

float forwardDistanceCm = 20.0;
float backwardDistanceCm = 20.0;
float TICKS_PER_CM = 2.0;

unsigned long leftTurnTimeMs = 600;
unsigned long rightTurnTimeMs = 600;
unsigned long moveStartMs = 0;
unsigned long activeTurnTimeMs = 600;

long targetTicks = 0;
long lastLeftTicks = 0;
long lastRightTicks = 0;

unsigned long lastControlTime = 0;
unsigned long lastRampTime = 0;
unsigned long lastWifiReconnectAttempt = 0;

bool smoothStopping = false;

enum MoveMode {
  STOPPED,
  FORWARD,
  BACKWARD,
  LEFT,
  RIGHT
};

MoveMode currentMode = STOPPED;

int commandQueue[MAX_PLATFORM_COMMANDS];
int commandCount = 0;
int commandIndex = 0;
bool batchActive = false;
bool commandRunning = false;

struct Position {
  int x;
  int y;
};

struct Obstacle {
  Position position;
};

struct Duck {
  String id;
  Position position;
  bool collected;
};

struct SimulationState {
  String actor;
  Position position;
  char direction;
  int fieldWidth;
  int fieldHeight;
  Obstacle obstacles[40];
  int obstacleCount;
  Duck ducks[40];
  int duckCount;
};

void loadCalibration() {
  preferences.begin("calib", true);
  motorSpeed = preferences.getInt("speed", motorSpeed);
  leftTrim = preferences.getInt("ltrim", leftTrim);
  rightTrim = preferences.getInt("rtrim", rightTrim);
  rampStep = preferences.getInt("rstep", rampStep);
  rampIntervalMs = preferences.getInt("rint", rampIntervalMs);
  forwardDistanceCm = preferences.getFloat("fdist", forwardDistanceCm);
  backwardDistanceCm = preferences.getFloat("bdist", backwardDistanceCm);
  TICKS_PER_CM = preferences.getFloat("tpcm", TICKS_PER_CM);
  leftTurnTimeMs = preferences.getULong("lturn", leftTurnTimeMs);
  rightTurnTimeMs = preferences.getULong("rturn", rightTurnTimeMs);
  pidKp = preferences.getFloat("kp", pidKp);
  pidKi = preferences.getFloat("ki", pidKi);
  pidKd = preferences.getFloat("kd", pidKd);
  preferences.end();

  motorSpeed = constrain(motorSpeed, minMotorSpeed, maxMotorSpeed);
  leftTrim = constrain(leftTrim, -250, 250);
  rightTrim = constrain(rightTrim, -250, 250);
  rampStep = constrain(rampStep, 1, 80);
  rampIntervalMs = constrain(rampIntervalMs, 1, 150);
  forwardDistanceCm = constrain(forwardDistanceCm, 0.5, 300.0);
  backwardDistanceCm = constrain(backwardDistanceCm, 0.5, 300.0);
  TICKS_PER_CM = constrain(TICKS_PER_CM, 0.1, 200.0);
  leftTurnTimeMs = constrain((int)leftTurnTimeMs, 50, 5000);
  rightTurnTimeMs = constrain((int)rightTurnTimeMs, 50, 5000);
  pidKp = constrain(pidKp, 0.0, 10.0);
  pidKi = constrain(pidKi, 0.0, 10.0);
  pidKd = constrain(pidKd, 0.0, 10.0);
}

void saveCalibration() {
  preferences.begin("calib", false);
  preferences.putInt("speed", motorSpeed);
  preferences.putInt("ltrim", leftTrim);
  preferences.putInt("rtrim", rightTrim);
  preferences.putInt("rstep", rampStep);
  preferences.putInt("rint", rampIntervalMs);
  preferences.putFloat("fdist", forwardDistanceCm);
  preferences.putFloat("bdist", backwardDistanceCm);
  preferences.putFloat("tpcm", TICKS_PER_CM);
  preferences.putULong("lturn", leftTurnTimeMs);
  preferences.putULong("rturn", rightTurnTimeMs);
  preferences.putFloat("kp", pidKp);
  preferences.putFloat("ki", pidKi);
  preferences.putFloat("kd", pidKd);
  preferences.end();
}

void IRAM_ATTR leftEncoderISR() {
  leftTicks++;
}

void IRAM_ATTR rightEncoderISR() {
  rightTicks++;
}

void writeMotors() {
  analogWrite(ENA, currentLeftPwm);
  analogWrite(ENB, currentRightPwm);
}

void setMotorTargets() {
  int l = motorSpeed + leftTrim + encoderLeftCorrection;
  int r = motorSpeed + rightTrim + encoderRightCorrection;

  targetLeftPwm = constrain(l, minMotorSpeed, maxMotorSpeed);
  targetRightPwm = constrain(r, minMotorSpeed, maxMotorSpeed);
}

void updateRamp() {
  if (millis() - lastRampTime < (unsigned long)rampIntervalMs) return;
  lastRampTime = millis();

  if (currentLeftPwm < targetLeftPwm) {
    currentLeftPwm += rampStep;
    if (currentLeftPwm > targetLeftPwm) currentLeftPwm = targetLeftPwm;
  } else if (currentLeftPwm > targetLeftPwm) {
    currentLeftPwm -= rampStep;
    if (currentLeftPwm < targetLeftPwm) currentLeftPwm = targetLeftPwm;
  }

  if (currentRightPwm < targetRightPwm) {
    currentRightPwm += rampStep;
    if (currentRightPwm > targetRightPwm) currentRightPwm = targetRightPwm;
  } else if (currentRightPwm > targetRightPwm) {
    currentRightPwm -= rampStep;
    if (currentRightPwm < targetRightPwm) currentRightPwm = targetRightPwm;
  }

  writeMotors();

  if (smoothStopping && currentLeftPwm == 0 && currentRightPwm == 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);

    currentMode = STOPPED;
    smoothStopping = false;
  }
}

void smoothStop() {
  targetLeftPwm = 0;
  targetRightPwm = 0;
  smoothStopping = true;
}

void clearCommandQueue() {
  commandCount = 0;
  commandIndex = 0;
  batchActive = false;
  commandRunning = false;
}

void emergencyStop() {
  currentMode = STOPPED;
  smoothStopping = false;

  targetLeftPwm = 0;
  targetRightPwm = 0;
  currentLeftPwm = 0;
  currentRightPwm = 0;

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  writeMotors();
  clearCommandQueue();
}

void stopCurrentMotionNow() {
  currentMode = STOPPED;
  smoothStopping = false;

  targetLeftPwm = 0;
  targetRightPwm = 0;
  currentLeftPwm = 0;
  currentRightPwm = 0;

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  writeMotors();
}

void resetEncoders() {
  noInterrupts();
  leftTicks = 0;
  rightTicks = 0;
  interrupts();

  lastLeftTicks = 0;
  lastRightTicks = 0;

  encoderLeftCorrection = 0;
  encoderRightCorrection = 0;

  pidIntegral = 0;
  lastPidError = 0;

  lastControlTime = millis();
}

void startMove(MoveMode mode) {
  currentMode = mode;
  smoothStopping = false;

  resetEncoders();

  float activeDistanceCm = mode == BACKWARD ? backwardDistanceCm : forwardDistanceCm;
  targetTicks = activeDistanceCm * TICKS_PER_CM;
  moveStartMs = millis();

  currentLeftPwm = 0;
  currentRightPwm = 0;

  if (mode == FORWARD) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  }

  if (mode == BACKWARD) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  }

  setMotorTargets();
}

void startTurn(MoveMode mode) {
  currentMode = mode;
  smoothStopping = false;

  resetEncoders();

  moveStartMs = millis();
  activeTurnTimeMs = mode == LEFT ? leftTurnTimeMs : rightTurnTimeMs;

  currentLeftPwm = 0;
  currentRightPwm = 0;

  if (mode == LEFT) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  }

  if (mode == RIGHT) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  }

  setMotorTargets();
}

void updateEncoderControl() {
  if (currentMode != FORWARD && currentMode != BACKWARD) return;
  if (smoothStopping) return;
  if (millis() - lastControlTime < 100) return;

  lastControlTime = millis();

  long l, r;

  noInterrupts();
  l = leftTicks;
  r = rightTicks;
  interrupts();

  long avgTicks = (l + r) / 2;

  if (millis() - moveStartMs >= ENCODER_STARTUP_IGNORE_MS && avgTicks >= targetTicks) {
    stopCurrentMotionNow();
    Serial.print("Target reached: L=");
    Serial.print(l);
    Serial.print(" R=");
    Serial.print(r);
    Serial.print(" avg=");
    Serial.print(avgTicks);
    Serial.print(" target=");
    Serial.println(targetTicks);
    return;
  }

  long leftDelta = l - lastLeftTicks;
  long rightDelta = r - lastRightTicks;

  lastLeftTicks = l;
  lastRightTicks = r;

  float error = (float)(leftDelta - rightDelta);

  pidIntegral += error;
  pidIntegral = constrain(pidIntegral, -300.0, 300.0);

  float derivative = error - lastPidError;
  lastPidError = error;

  float pidOutput = pidKp * error + pidKi * pidIntegral + pidKd * derivative;
  pidOutput = constrain(pidOutput, -maxCorrection, maxCorrection);

  encoderLeftCorrection = constrain((int)(-pidOutput), -maxCorrection, maxCorrection);
  encoderRightCorrection = constrain((int)(pidOutput), -maxCorrection, maxCorrection);

  setMotorTargets();
}

void updateTurnControl() {
  if (currentMode != LEFT && currentMode != RIGHT) return;
  if (smoothStopping) return;

  if (millis() - moveStartMs >= activeTurnTimeMs) {
    stopCurrentMotionNow();
  }
}

const char* platformCommandName(int command) {
  switch (command) {
    case PLATFORM_CMD_FORWARD:
      return "forward";
    case PLATFORM_CMD_BACKWARD:
      return "backward";
    case PLATFORM_CMD_TURN_LEFT:
      return "left";
    case PLATFORM_CMD_TURN_RIGHT:
      return "right";
    default:
      return "unknown";
  }
}

bool robotIsBusy() {
  return batchActive || commandRunning || currentMode != STOPPED || smoothStopping;
}

void enqueuePlatformBatch(const int* commands, int count) {
  int clippedCount = min(count, MAX_PLATFORM_COMMANDS);
  for (int i = 0; i < clippedCount; i++) {
    commandQueue[i] = commands[i];
  }

  commandCount = clippedCount;
  commandIndex = 0;
  batchActive = clippedCount > 0;
  commandRunning = false;
}

void startQueuedCommand(int command) {
  Serial.print("Starting platform command ");
  Serial.print(command);
  Serial.print(" (");
  Serial.print(platformCommandName(command));
  Serial.println(")");

  if (command == PLATFORM_CMD_FORWARD) {
    startMove(FORWARD);
  } else if (command == PLATFORM_CMD_BACKWARD) {
    startMove(BACKWARD);
  } else if (command == PLATFORM_CMD_TURN_LEFT) {
    startTurn(LEFT);
  } else if (command == PLATFORM_CMD_TURN_RIGHT) {
    startTurn(RIGHT);
  } else {
    Serial.println("ERR unknown queued platform command");
    emergencyStop();
    return;
  }

  commandRunning = true;
}

void processCommandQueue() {
  if (!batchActive) return;

  if (commandRunning) {
    if (currentMode == STOPPED && !smoothStopping) {
      Serial.print("Command completed: ");
      Serial.println(commandQueue[commandIndex]);

      commandRunning = false;
      commandIndex++;
    } else {
      return;
    }
  }

  if (commandIndex >= commandCount) {
    Serial.println("Batch completed");
    clearCommandQueue();
    return;
  }

  startQueuedCommand(commandQueue[commandIndex]);
}

bool readClientPayload(WiFiClient& client, String& payload, bool& payloadTooLong) {
  payload = "";
  payloadTooLong = false;
  unsigned long startedAt = millis();
  unsigned long lastByteAt = startedAt;
  bool receivedAny = false;

  while (millis() - startedAt < CLIENT_READ_TIMEOUT_MS) {
    bool readSomething = false;
    while (client.available()) {
      char c = client.read();
      receivedAny = true;
      readSomething = true;
      lastByteAt = millis();
      if (payload.length() < MAX_REQUEST_LENGTH) {
        payload += c;
      } else {
        payloadTooLong = true;
      }
    }

    if (receivedAny && !readSomething && millis() - lastByteAt >= CLIENT_READ_IDLE_MS) {
      return true;
    }

    if (receivedAny && !client.connected() && millis() - lastByteAt >= CLIENT_READ_IDLE_MS) {
      return true;
    }

    delay(1);
  }

  return receivedAny;
}

int findMatching(const String& text, int openIndex, char openChar, char closeChar) {
  int depth = 0;
  bool inString = false;
  bool escape = false;

  for (int i = openIndex; i < text.length(); i++) {
    char c = text.charAt(i);
    if (escape) {
      escape = false;
      continue;
    }
    if (c == '\\') {
      escape = inString;
      continue;
    }
    if (c == '"') {
      inString = !inString;
      continue;
    }
    if (inString) continue;
    if (c == openChar) depth++;
    if (c == closeChar) {
      depth--;
      if (depth == 0) return i;
    }
  }

  return -1;
}

String extractStringValue(const String& text, const String& key, int from = 0) {
  int keyIndex = text.indexOf("\"" + key + "\"", from);
  if (keyIndex < 0) return "";
  int colon = text.indexOf(':', keyIndex);
  if (colon < 0) return "";
  int firstQuote = text.indexOf('"', colon + 1);
  if (firstQuote < 0) return "";
  int secondQuote = text.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return "";
  return text.substring(firstQuote + 1, secondQuote);
}

int extractIntValue(const String& text, const String& key, int from = 0, int fallback = 0) {
  int keyIndex = text.indexOf("\"" + key + "\"", from);
  if (keyIndex < 0) return fallback;
  int colon = text.indexOf(':', keyIndex);
  if (colon < 0) return fallback;
  int i = colon + 1;
  while (i < text.length() && isspace(text.charAt(i))) i++;
  int sign = 1;
  if (text.charAt(i) == '-') {
    sign = -1;
    i++;
  }
  int value = 0;
  bool found = false;
  while (i < text.length() && isdigit(text.charAt(i))) {
    value = value * 10 + (text.charAt(i) - '0');
    found = true;
    i++;
  }
  return found ? sign * value : fallback;
}

bool extractPositionObject(const String& text, int from, Position& out) {
  int positionKey = text.indexOf("\"position\"", from);
  if (positionKey < 0) return false;
  int openBrace = text.indexOf('{', positionKey);
  if (openBrace < 0) return false;
  int closeBrace = findMatching(text, openBrace, '{', '}');
  if (closeBrace < 0) return false;
  String object = text.substring(openBrace, closeBrace + 1);
  out.x = extractIntValue(object, "x");
  out.y = extractIntValue(object, "y");
  return true;
}

int actorObjectStart(const String& payload, const String& actor) {
  int actorsKey = payload.indexOf("\"actors\"");
  if (actorsKey < 0) return -1;
  int actorKey = payload.indexOf("\"" + actor + "\"", actorsKey);
  if (actorKey < 0) return -1;
  return payload.indexOf('{', actorKey);
}

void parseCommands(const String& payload, int* schoolCommands, int& schoolCount) {
  schoolCount = 0;
  int keyIndex = payload.indexOf("\"commands\"");
  if (keyIndex < 0) return;
  int openBracket = payload.indexOf('[', keyIndex);
  if (openBracket < 0) return;
  int closeBracket = findMatching(payload, openBracket, '[', ']');
  if (closeBracket < 0) return;

  int i = openBracket + 1;
  while (i < closeBracket && schoolCount < MAX_SCHOOL_COMMANDS) {
    while (i < closeBracket && !isdigit(payload.charAt(i)) && payload.charAt(i) != '-') i++;
    if (i >= closeBracket) break;
    int sign = 1;
    if (payload.charAt(i) == '-') {
      sign = -1;
      i++;
    }
    int value = 0;
    bool found = false;
    while (i < closeBracket && isdigit(payload.charAt(i))) {
      value = value * 10 + (payload.charAt(i) - '0');
      found = true;
      i++;
    }
    if (found) schoolCommands[schoolCount++] = sign * value;
  }
}

bool parseLegacyCommands(const String& payload, int* schoolCommands, int& schoolCount) {
  schoolCount = 0;
  int i = 0;

  while (i < payload.length() && schoolCount < MAX_SCHOOL_COMMANDS) {
    while (i < payload.length()) {
      char c = payload.charAt(i);
      if (isdigit(c) || c == '-') break;
      if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != ',') return false;
      i++;
    }
    if (i >= payload.length()) break;

    int sign = 1;
    if (payload.charAt(i) == '-') {
      sign = -1;
      i++;
    }

    int value = 0;
    bool found = false;
    while (i < payload.length() && isdigit(payload.charAt(i))) {
      value = value * 10 + (payload.charAt(i) - '0');
      found = true;
      i++;
    }
    if (!found) return false;

    schoolCommands[schoolCount++] = sign * value;
  }

  while (i < payload.length()) {
    char c = payload.charAt(i);
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != ',') return false;
    i++;
  }

  return schoolCount > 0;
}

void parseObstacles(const String& payload, SimulationState& state) {
  state.obstacleCount = 0;
  int keyIndex = payload.indexOf("\"obstacles\"");
  if (keyIndex < 0) return;
  int openBracket = payload.indexOf('[', keyIndex);
  if (openBracket < 0) return;
  int closeBracket = findMatching(payload, openBracket, '[', ']');
  if (closeBracket < 0) return;

  int cursor = openBracket + 1;
  while (cursor < closeBracket && state.obstacleCount < 40) {
    int objectStart = payload.indexOf('{', cursor);
    if (objectStart < 0 || objectStart > closeBracket) break;
    int objectEnd = findMatching(payload, objectStart, '{', '}');
    if (objectEnd < 0 || objectEnd > closeBracket) break;
    String object = payload.substring(objectStart, objectEnd + 1);
    Position position;
    if (!extractPositionObject(object, 0, position)) break;
    state.obstacles[state.obstacleCount++].position = position;
    cursor = objectEnd + 1;
  }
}

void parseDucks(const String& payload, SimulationState& state) {
  state.duckCount = 0;
  int keyIndex = payload.indexOf("\"ducks\"");
  if (keyIndex < 0) return;
  int openBracket = payload.indexOf('[', keyIndex);
  if (openBracket < 0) return;
  int closeBracket = findMatching(payload, openBracket, '[', ']');
  if (closeBracket < 0) return;

  int cursor = openBracket + 1;
  while (cursor < closeBracket && state.duckCount < 40) {
    int objectStart = payload.indexOf('{', cursor);
    if (objectStart < 0 || objectStart > closeBracket) break;
    int objectEnd = findMatching(payload, objectStart, '{', '}');
    if (objectEnd < 0 || objectEnd > closeBracket) break;
    String object = payload.substring(objectStart, objectEnd + 1);
    String collectedBy = extractStringValue(object, "collectedBy");
    if (collectedBy.length() == 0 || object.indexOf("\"collectedBy\":null") >= 0 || object.indexOf("\"collectedBy\": null") >= 0) {
      Position position;
      if (extractPositionObject(object, 0, position)) {
        state.ducks[state.duckCount].id = extractStringValue(object, "id");
        state.ducks[state.duckCount].position = position;
        state.ducks[state.duckCount].collected = false;
        state.duckCount++;
      }
    }
    cursor = objectEnd + 1;
  }
}

bool parseSimulationRequest(const String& payload, SimulationState& state, int* schoolCommands, int& schoolCount) {
  state.actor = extractStringValue(payload, "actor");
  if (state.actor.length() == 0) return false;

  int actorStart = actorObjectStart(payload, state.actor);
  if (actorStart < 0) return false;

  if (!extractPositionObject(payload, actorStart, state.position)) return false;
  String direction = extractStringValue(payload, "direction", actorStart);
  state.direction = direction.length() > 0 ? direction.charAt(0) : 'N';

  state.fieldWidth = extractIntValue(payload, "width", 0, 10);
  state.fieldHeight = extractIntValue(payload, "height", 0, 10);

  parseCommands(payload, schoolCommands, schoolCount);
  parseObstacles(payload, state);
  parseDucks(payload, state);

  return schoolCount > 0;
}

bool expandSchoolCommands(const int* schoolCommands, int schoolCount, int* platformCommands, int& platformCount, String& error) {
  platformCount = 0;

  for (int i = 0; i < schoolCount; i++) {
    int remaining = MAX_PLATFORM_COMMANDS - platformCount;
    int expandedCount = movement_expand_command(schoolCommands[i], platformCommands + platformCount, remaining);
    if (expandedCount < 0) {
      error = movement_is_supported_command(schoolCommands[i]) ? "turn_limit_exceeded" : "unknown_command";
      return false;
    }
    platformCount += expandedCount;
  }

  return platformCount > 0;
}

bool positionBlocked(const SimulationState& state, const Position& position) {
  if (position.x < 0 || position.x >= state.fieldWidth || position.y < 0 || position.y >= state.fieldHeight) {
    return true;
  }
  for (int i = 0; i < state.obstacleCount; i++) {
    if (state.obstacles[i].position.x == position.x && state.obstacles[i].position.y == position.y) {
      return true;
    }
  }
  return false;
}

void collectAt(SimulationState& state, const Position& position) {
  for (int i = 0; i < state.duckCount; i++) {
    if (!state.ducks[i].collected && state.ducks[i].position.x == position.x && state.ducks[i].position.y == position.y) {
      state.ducks[i].collected = true;
    }
  }
}

void applyPlatformCommandToState(SimulationState& state, int command) {
  if (command == PLATFORM_CMD_TURN_LEFT) {
    if (state.direction == 'N') state.direction = 'W';
    else if (state.direction == 'W') state.direction = 'S';
    else if (state.direction == 'S') state.direction = 'E';
    else state.direction = 'N';
    return;
  }

  if (command == PLATFORM_CMD_TURN_RIGHT) {
    if (state.direction == 'N') state.direction = 'E';
    else if (state.direction == 'E') state.direction = 'S';
    else if (state.direction == 'S') state.direction = 'W';
    else state.direction = 'N';
    return;
  }

  if (command != PLATFORM_CMD_FORWARD && command != PLATFORM_CMD_BACKWARD) return;

  int step = command == PLATFORM_CMD_FORWARD ? 1 : -1;
  Position candidate = state.position;
  if (state.direction == 'N') candidate.y -= step;
  if (state.direction == 'E') candidate.x += step;
  if (state.direction == 'S') candidate.y += step;
  if (state.direction == 'W') candidate.x -= step;

  if (!positionBlocked(state, candidate)) {
    state.position = candidate;
    collectAt(state, state.position);
  }
}

void applyPlatformCommandsToState(SimulationState& state, const int* platformCommands, int platformCount) {
  for (int i = 0; i < platformCount; i++) {
    applyPlatformCommandToState(state, platformCommands[i]);
  }
}

String collectedDucksJson(const SimulationState& state) {
  String result = "[";
  bool first = true;
  for (int i = 0; i < state.duckCount; i++) {
    if (!state.ducks[i].collected) continue;
    if (!first) result += ",";
    result += "\"";
    result += state.ducks[i].id;
    result += "\"";
    first = false;
  }
  result += "]";
  return result;
}

String successResponse(const SimulationState& state) {
  String response = "{\"ok\":true,\"actor\":\"";
  response += state.actor;
  response += "\",\"finalPosition\":{\"x\":";
  response += String(state.position.x);
  response += ",\"y\":";
  response += String(state.position.y);
  response += "},\"finalDirection\":\"";
  response += state.direction;
  response += "\",\"ducksCollected\":";
  response += collectedDucksJson(state);
  response += ",\"error\":null}\n";
  return response;
}

String errorResponse(const String& actor, const String& error) {
  String response = "{\"ok\":false,\"actor\":\"";
  response += actor.length() > 0 ? actor : "unknown";
  response += "\",\"ducksCollected\":[],\"error\":\"";
  response += error;
  response += "\"}\n";
  return response;
}

void sendTcpResponse(WiFiClient& client, const String& response) {
  client.print(response);
  client.flush();
  delay(20);
  client.stop();
}

void handleCommandClient() {
  WiFiClient client = commandServer.available();
  if (!client) return;

  bool payloadTooLong = false;
  String payload;
  readClientPayload(client, payload, payloadTooLong);
  payload.trim();

  Serial.print("TCP request bytes: ");
  Serial.println(payload.length());
  Serial.print("TCP payload preview: ");
  if (payload.length() <= 160) {
    Serial.println(payload);
  } else {
    Serial.print(payload.substring(0, 160));
    Serial.println("...");
  }

  if (payloadTooLong) {
    sendTcpResponse(client, errorResponse("unknown", "payload_too_long"));
    return;
  }

  SimulationState state;
  int schoolCommands[MAX_SCHOOL_COMMANDS];
  int schoolCount = 0;

  bool jsonRequest = payload.startsWith("{");
  bool parsed = jsonRequest
    ? parseSimulationRequest(payload, state, schoolCommands, schoolCount)
    : parseLegacyCommands(payload, schoolCommands, schoolCount);

  if (!parsed) {
    sendTcpResponse(client, jsonRequest ? errorResponse("unknown", "bad_request") : "ERR bad_request\n");
    return;
  }

  int platformCommands[MAX_PLATFORM_COMMANDS];
  int platformCount = 0;
  String error = "";

  if (!expandSchoolCommands(schoolCommands, schoolCount, platformCommands, platformCount, error)) {
    sendTcpResponse(client, jsonRequest ? errorResponse(state.actor, error) : String("ERR ") + error + "\n");
    return;
  }

  if (!jsonRequest) {
    if (robotIsBusy()) {
      sendTcpResponse(client, "BUSY\n");
      return;
    }
    enqueuePlatformBatch(platformCommands, platformCount);
    sendTcpResponse(client, "OK\n");

    Serial.print("Accepted legacy ");
    Serial.print(schoolCount);
    Serial.print(" school commands, expanded to ");
    Serial.print(platformCount);
    Serial.println(" platform commands");
    return;
  }

  applyPlatformCommandsToState(state, platformCommands, platformCount);

  if (state.actor == DEVICE_ACTOR || EXECUTE_AGENT_COMMANDS) {
    if (robotIsBusy()) {
      sendTcpResponse(client, errorResponse(state.actor, "robot_busy"));
      return;
    }
    enqueuePlatformBatch(platformCommands, platformCount);
  }

  sendTcpResponse(client, successResponse(state));

  Serial.print("Accepted ");
  Serial.print(schoolCount);
  Serial.print(" school commands, expanded to ");
  Serial.print(platformCount);
  Serial.println(" platform commands");
}

String statusJson() {
  long l, r;
  noInterrupts();
  l = leftTicks;
  r = rightTicks;
  interrupts();

  String result = "{";
  result += "\"deviceActor\":\"";
  result += DEVICE_ACTOR;
  result += "\",\"wifi\":\"";
  result += WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
  result += "\",\"ip\":\"";
  result += WiFi.localIP().toString();
  result += "\",\"tcpPort\":";
  result += String(COMMAND_PORT);
  result += ",\"calibrationStored\":true";
  result += ",\"mode\":";
  result += String((int)currentMode);
  result += ",\"busy\":";
  result += robotIsBusy() ? "true" : "false";
  result += ",\"leftTicks\":";
  result += String(l);
  result += ",\"rightTicks\":";
  result += String(r);
  result += ",\"motorSpeed\":";
  result += String(motorSpeed);
  result += ",\"forwardDistanceCm\":";
  result += String(forwardDistanceCm);
  result += ",\"backwardDistanceCm\":";
  result += String(backwardDistanceCm);
  result += ",\"ticksPerCm\":";
  result += String(TICKS_PER_CM);
  result += ",\"targetTicks\":";
  result += String(targetTicks);
  result += ",\"leftTurnTimeMs\":";
  result += String(leftTurnTimeMs);
  result += ",\"rightTurnTimeMs\":";
  result += String(rightTurnTimeMs);
  result += ",\"leftTrim\":";
  result += String(leftTrim);
  result += ",\"rightTrim\":";
  result += String(rightTrim);
  result += ",\"rampStep\":";
  result += String(rampStep);
  result += ",\"rampIntervalMs\":";
  result += String(rampIntervalMs);
  result += ",\"pidKp\":";
  result += String(pidKp, 3);
  result += ",\"pidKi\":";
  result += String(pidKi, 3);
  result += ",\"pidKd\":";
  result += String(pidKd, 3);
  result += "}";
  return result;
}

String page() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>PIKT ESP32 Robot</title>
  <style>
    body { font-family: Arial; text-align: center; background: #111; color: white; margin: 24px; }
    button { width: 130px; height: 58px; margin: 7px; font-size: 18px; border-radius: 8px; border: none; background: #1976d2; color: white; }
    .stop { background: #d32f2f; }
    input { width: min(220px, 90vw); }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap: 12px; max-width: 920px; margin: 18px auto; }
    .field { background: #1d1d1d; border: 1px solid #333; border-radius: 8px; padding: 10px; }
    .field p { margin: 0 0 8px; font-size: 14px; color: #ddd; }
    pre { margin: 16px auto; padding: 12px; max-width: 720px; text-align: left; background: #222; border-radius: 8px; overflow: auto; }
  </style>
</head>
<body>
  <h2>PIKT ESP32 Robot</h2>
  <p>TCP backend port: 5055</p>
  <div><button onclick="cmd(1)">Forward</button></div>
  <div>
    <button onclick="cmd(3)">Left</button>
    <button class="stop" onclick="stopRobot()">Stop</button>
    <button onclick="cmd(4)">Right</button>
  </div>
  <div><button onclick="cmd(2)">Back</button></div>
  <div class="grid">
    <div class="field">
      <p>Speed <span id="speedLabel">430</span></p>
      <input id="speedInput" type="number" min="0" max="1023" step="5" value="430" onchange="param('speed', this.value, 'speedLabel')">
    </div>
    <div class="field">
      <p>Forward distance <span id="forwardDistanceLabel">20</span> cm</p>
      <input id="forwardDistanceInput" type="number" min="0.5" max="300" step="0.5" value="20" onchange="param('forwardDistance', this.value, 'forwardDistanceLabel')">
    </div>
    <div class="field">
      <p>Back distance <span id="backwardDistanceLabel">20</span> cm</p>
      <input id="backwardDistanceInput" type="number" min="0.5" max="300" step="0.5" value="20" onchange="param('backwardDistance', this.value, 'backwardDistanceLabel')">
    </div>
    <div class="field">
      <p>Ticks/cm <span id="ticksPerCmLabel">2</span></p>
      <input id="ticksPerCmInput" type="number" min="0.1" max="200" step="0.1" value="2" onchange="param('ticksPerCm', this.value, 'ticksPerCmLabel')">
    </div>
    <div class="field">
      <p>Left turn time <span id="leftTurnLabel">600</span> ms</p>
      <input id="leftTurnInput" type="number" min="50" max="5000" step="10" value="600" onchange="param('leftTurnTime', this.value, 'leftTurnLabel')">
    </div>
    <div class="field">
      <p>Right turn time <span id="rightTurnLabel">600</span> ms</p>
      <input id="rightTurnInput" type="number" min="50" max="5000" step="10" value="600" onchange="param('rightTurnTime', this.value, 'rightTurnLabel')">
    </div>
    <div class="field">
      <p>Left trim <span id="leftTrimLabel">0</span></p>
      <input id="leftTrimInput" type="number" min="-250" max="250" step="5" value="0" onchange="param('leftTrim', this.value, 'leftTrimLabel')">
    </div>
    <div class="field">
      <p>Right trim <span id="rightTrimLabel">0</span></p>
      <input id="rightTrimInput" type="number" min="-250" max="250" step="5" value="0" onchange="param('rightTrim', this.value, 'rightTrimLabel')">
    </div>
    <div class="field">
      <p>Ramp step <span id="rampStepLabel">15</span></p>
      <input id="rampStepInput" type="number" min="1" max="80" step="1" value="15" onchange="param('rampStep', this.value, 'rampStepLabel')">
    </div>
    <div class="field">
      <p>Ramp interval <span id="rampIntervalLabel">15</span> ms</p>
      <input id="rampIntervalInput" type="number" min="1" max="150" step="1" value="15" onchange="param('rampInterval', this.value, 'rampIntervalLabel')">
    </div>
    <div class="field">
      <p>PID Kp <span id="pidKpLabel">0.800</span></p>
      <input id="pidKpInput" type="number" min="0" max="10" step="0.05" value="0.8" onchange="param('kp', this.value, 'pidKpLabel')">
    </div>
    <div class="field">
      <p>PID Ki <span id="pidKiLabel">0.000</span></p>
      <input id="pidKiInput" type="number" min="0" max="10" step="0.01" value="0" onchange="param('ki', this.value, 'pidKiLabel')">
    </div>
    <div class="field">
      <p>PID Kd <span id="pidKdLabel">0.200</span></p>
      <input id="pidKdInput" type="number" min="0" max="10" step="0.05" value="0.2" onchange="param('kd', this.value, 'pidKdLabel')">
    </div>
  </div>
  <button class="stop" onclick="resetCalibration()">Reset calibration</button>
  <pre id="status"></pre>
  <script>
    async function cmd(value) { await fetch('/run?commands=' + value); refresh(); }
    async function stopRobot() { await fetch('/stop'); refresh(); }
    async function param(name, value, label) {
      document.getElementById(label).innerText = value;
      await fetch('/param?' + name + '=' + value);
    }
    async function resetCalibration() {
      await fetch('/resetCalibration');
      refresh();
    }
    function syncControl(inputId, labelId, value) {
      const input = document.getElementById(inputId);
      const label = document.getElementById(labelId);
      if (document.activeElement !== input) input.value = value;
      label.innerText = value;
    }
    function syncControls(status) {
      syncControl('speedInput', 'speedLabel', status.motorSpeed);
      syncControl('forwardDistanceInput', 'forwardDistanceLabel', Number(status.forwardDistanceCm).toFixed(1));
      syncControl('backwardDistanceInput', 'backwardDistanceLabel', Number(status.backwardDistanceCm).toFixed(1));
      syncControl('ticksPerCmInput', 'ticksPerCmLabel', Number(status.ticksPerCm).toFixed(2));
      syncControl('leftTurnInput', 'leftTurnLabel', status.leftTurnTimeMs);
      syncControl('rightTurnInput', 'rightTurnLabel', status.rightTurnTimeMs);
      syncControl('leftTrimInput', 'leftTrimLabel', status.leftTrim);
      syncControl('rightTrimInput', 'rightTrimLabel', status.rightTrim);
      syncControl('rampStepInput', 'rampStepLabel', status.rampStep);
      syncControl('rampIntervalInput', 'rampIntervalLabel', status.rampIntervalMs);
      syncControl('pidKpInput', 'pidKpLabel', Number(status.pidKp).toFixed(3));
      syncControl('pidKiInput', 'pidKiLabel', Number(status.pidKi).toFixed(3));
      syncControl('pidKdInput', 'pidKdLabel', Number(status.pidKd).toFixed(3));
    }
    async function refresh() {
      const status = await (await fetch('/status')).json();
      syncControls(status);
      document.getElementById('status').innerText = JSON.stringify(status, null, 2);
    }
    setInterval(refresh, 1000);
    refresh();
  </script>
</body>
</html>
)rawliteral";
}

void handleManualRun() {
  if (robotIsBusy()) {
    httpServer.send(409, "text/plain", "robot_busy");
    return;
  }

  String commandsArg = httpServer.arg("commands");
  int schoolCommands[MAX_SCHOOL_COMMANDS];
  int schoolCount = 0;
  int start = 0;

  while (start < commandsArg.length() && schoolCount < MAX_SCHOOL_COMMANDS) {
    int comma = commandsArg.indexOf(',', start);
    if (comma < 0) comma = commandsArg.length();
    String token = commandsArg.substring(start, comma);
    token.trim();
    if (token.length() > 0) {
      schoolCommands[schoolCount++] = token.toInt();
    }
    start = comma + 1;
  }

  int platformCommands[MAX_PLATFORM_COMMANDS];
  int platformCount = 0;
  String error = "";

  if (!expandSchoolCommands(schoolCommands, schoolCount, platformCommands, platformCount, error)) {
    httpServer.send(400, "text/plain", error);
    return;
  }

  enqueuePlatformBatch(platformCommands, platformCount);
  httpServer.send(200, "text/plain", "OK");
}

void setupHttpServer() {
  httpServer.on("/", []() {
    httpServer.send(200, "text/html", page());
  });
  httpServer.on("/status", []() {
    httpServer.send(200, "application/json", statusJson());
  });
  httpServer.on("/stop", []() {
    emergencyStop();
    httpServer.send(200, "text/plain", "OK");
  });
  httpServer.on("/resetCalibration", []() {
    preferences.begin("calib", false);
    preferences.clear();
    preferences.end();
    motorSpeed = 430;
    leftTrim = 0;
    rightTrim = 0;
    rampStep = 15;
    rampIntervalMs = 15;
    forwardDistanceCm = 20.0;
    backwardDistanceCm = 20.0;
    TICKS_PER_CM = 2.0;
    leftTurnTimeMs = 600;
    rightTurnTimeMs = 600;
    pidKp = 0.80;
    pidKi = 0.00;
    pidKd = 0.20;
    if (currentMode != STOPPED && !smoothStopping) setMotorTargets();
    httpServer.send(200, "application/json", statusJson());
  });
  httpServer.on("/run", handleManualRun);
  httpServer.on("/param", []() {
    if (httpServer.hasArg("speed")) {
      motorSpeed = constrain(httpServer.arg("speed").toInt(), minMotorSpeed, maxMotorSpeed);
      if (currentMode != STOPPED && !smoothStopping) setMotorTargets();
    }
    if (httpServer.hasArg("forwardDistance")) {
      forwardDistanceCm = constrain(httpServer.arg("forwardDistance").toFloat(), 0.5, 300.0);
    }
    if (httpServer.hasArg("backwardDistance")) {
      backwardDistanceCm = constrain(httpServer.arg("backwardDistance").toFloat(), 0.5, 300.0);
    }
    if (httpServer.hasArg("distance")) {
      float value = constrain(httpServer.arg("distance").toFloat(), 0.5, 300.0);
      forwardDistanceCm = value;
      backwardDistanceCm = value;
    }
    if (httpServer.hasArg("leftTurnTime")) {
      leftTurnTimeMs = constrain(httpServer.arg("leftTurnTime").toInt(), 50, 5000);
    }
    if (httpServer.hasArg("rightTurnTime")) {
      rightTurnTimeMs = constrain(httpServer.arg("rightTurnTime").toInt(), 50, 5000);
    }
    if (httpServer.hasArg("turnTime")) {
      unsigned long value = constrain(httpServer.arg("turnTime").toInt(), 50, 5000);
      leftTurnTimeMs = value;
      rightTurnTimeMs = value;
    }
    if (httpServer.hasArg("ticksPerCm")) {
      TICKS_PER_CM = constrain(httpServer.arg("ticksPerCm").toFloat(), 0.1, 200.0);
    }
    if (httpServer.hasArg("leftTrim")) {
      leftTrim = constrain(httpServer.arg("leftTrim").toInt(), -250, 250);
      if (currentMode != STOPPED && !smoothStopping) setMotorTargets();
    }
    if (httpServer.hasArg("rightTrim")) {
      rightTrim = constrain(httpServer.arg("rightTrim").toInt(), -250, 250);
      if (currentMode != STOPPED && !smoothStopping) setMotorTargets();
    }
    if (httpServer.hasArg("rampStep")) {
      rampStep = constrain(httpServer.arg("rampStep").toInt(), 1, 80);
    }
    if (httpServer.hasArg("rampInterval")) {
      rampIntervalMs = constrain(httpServer.arg("rampInterval").toInt(), 1, 150);
    }
    if (httpServer.hasArg("kp")) {
      pidKp = constrain(httpServer.arg("kp").toFloat(), 0.0, 10.0);
    }
    if (httpServer.hasArg("ki")) {
      pidKi = constrain(httpServer.arg("ki").toFloat(), 0.0, 10.0);
      pidIntegral = 0;
    }
    if (httpServer.hasArg("kd")) {
      pidKd = constrain(httpServer.arg("kd").toFloat(), 0.0, 10.0);
    }
    saveCalibration();
    httpServer.send(200, "application/json", statusJson());
  });
  httpServer.begin();
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiReconnectAttempt < WIFI_RECONNECT_INTERVAL_MS) return;
  lastWifiReconnectAttempt = millis();

  Serial.print("Connecting to Wi-Fi ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void setup() {
  Serial.begin(115200);
  loadCalibration();

  analogWriteResolution(ENA, 10);
  analogWriteResolution(ENB, 10);
  analogWriteFrequency(ENA, 1000);
  analogWriteFrequency(ENB, 1000);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(LEFT_ENCODER, INPUT_PULLUP);
  pinMode(RIGHT_ENCODER, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(LEFT_ENCODER), leftEncoderISR, RISING);
  attachInterrupt(digitalPinToInterrupt(RIGHT_ENCODER), rightEncoderISR, RISING);

  emergencyStop();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  commandServer.begin();
  setupHttpServer();

  Serial.println("ESP32 robot backend replacement started");
  Serial.print("Wi-Fi SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("TCP command port: ");
  Serial.println(COMMAND_PORT);
}

void loop() {
  connectWifi();

  if (WiFi.status() == WL_CONNECTED) {
    static bool printedIp = false;
    if (!printedIp) {
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      printedIp = true;
    }
    handleCommandClient();
    httpServer.handleClient();
  }

  processCommandQueue();
  updateEncoderControl();
  updateTurnControl();
  updateRamp();
}
