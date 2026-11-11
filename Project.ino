
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// ================= SETTINGS ================

const char* ssid = "here";
const char* password = "here";
const char* weatherApiKey = "here";
const char* city = "here";
const char* country = "here";

const char* openskyUser = "here";
const char* openskyPass = "here";

// ================= OLED =================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ================= PINS =================

const int BTN_UP = 13;
const int BTN_SELECT = 12;
const int BTN_BACK = 11;

const int PIN_BUZZER = 10;
bool buzzerEnabled = true;

#define RGB_PIN 21
#define NUM_PIXELS 1

Adafruit_NeoPixel rgbLed(
  NUM_PIXELS,
  RGB_PIN,
  NEO_GRB + NEO_KHZ800
);

// ================= STATES =================

enum State {
  MENU_MAIN,
  MENU_GAMES,
  MENU_WEATHER,
  MENU_PLANES,
  MENU_SETTINGS,
  GAME_PLAYING
};

State currentState = MENU_MAIN;

int mainMenuIndex = 0;
int gameMenuIndex = 0;
int settingsIndex = 0;

int selectedGameType = 0;
int gameDifficulty = 1;

// ================= DATA =================

float temperature = 0.0;
int humidity = 0;
float windSpeed = 0.0;

bool weatherLoaded = false;

int activeAircraftCount = 0;
bool planesLoaded = false;

// ================= TIMING =================

unsigned long lastRgbChange = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastGameTick = 0;

unsigned long lastWiFiAttempt = 0;
unsigned long lastWeatherAttempt = 0;
unsigned long lastPlanesAttempt = 0;

const unsigned long buttonDelay = 150;
const unsigned long WIFI_RETRY_INTERVAL = 10000;
const unsigned long DATA_RETRY_INTERVAL = 30000;

// ================= RGB =================

int rgbState = 0;

// ================= GAME VARIABLES =================

int gameScore = 0;

int playerX = 64;
int playerY = 56;

int entityY = 32;
int entityVy = 0;

int obstacleX = 128;
int obstacleParam = 25;

int carX1 = 0;
int carX2 = 128;

// ================= SNAKE =================

struct Point {
  int8_t x;
  int8_t y;
};

Point snake[30];

int snakeLength = 3;

int8_t dirX = 1;
int8_t dirY = 0;

Point food;

// ================= FUNCTION DECLARATIONS =================

void updateDisplay();
void handleInput();
void initGame(int gameType);
void runGameLoop();

void getWeatherData();
void getOpenSkyData();

void maintainWiFi();
void updateRgbAnimation();

void playTone(int frequency, int duration);

bool rectsOverlap(
  int ax, int ay, int aw, int ah,
  int bx, int by, int bw, int bh
);

void endGame();
void placeFood();
bool foodOnSnake(int x, int y);

// ================= SETUP =================

void setup() {

  Serial.begin(115200);

  randomSeed(esp_random());

  Wire.begin(9, 8);
  Wire.setClock(100000);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  pinMode(PIN_BUZZER, OUTPUT);

  rgbLed.begin();
  rgbLed.setBrightness(30);
  rgbLed.show();

  if (!display.begin(
    SSD1306_SWITCHCAPVCC,
    SCREEN_ADDRESS
  )) {

    Serial.println("SSD1306 allocation failed!");

    for (;;) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("System Starting...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  WiFi.begin(ssid, password);

  lastWiFiAttempt = millis();

  updateDisplay();
}

// ================= MAIN LOOP =================

void loop() {

  maintainWiFi();

  updateRgbAnimation();

  handleInput();

  // GAME LOOP

  if (currentState == GAME_PLAYING) {

   int tickInterval;

if (gameDifficulty == 0) {
  tickInterval = (selectedGameType == 2) ? 250 : 90;
}
else if (gameDifficulty == 1) {
  tickInterval = (selectedGameType == 2) ? 170 : 60;
}
else {
  tickInterval = (selectedGameType == 2) ? 110 : 40;
}

    if (millis() - lastGameTick >= tickInterval) {

      lastGameTick = millis();

      runGameLoop();
    }
  }

  // WEATHER UPDATE

  if (
    currentState == MENU_WEATHER &&
    !weatherLoaded &&
    WiFi.status() == WL_CONNECTED &&
    millis() - lastWeatherAttempt >= DATA_RETRY_INTERVAL
  ) {

    lastWeatherAttempt = millis();

    getWeatherData();

    updateDisplay();
  }

  // OPENSKY UPDATE

  if (
    currentState == MENU_PLANES &&
    !planesLoaded &&
    WiFi.status() == WL_CONNECTED &&
    millis() - lastPlanesAttempt >= DATA_RETRY_INTERVAL
  ) {

    lastPlanesAttempt = millis();

    getOpenSkyData();

    updateDisplay();
  }
}

// ================= WIFI RECONNECT =================

void maintainWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (
    millis() - lastWiFiAttempt >= WIFI_RETRY_INTERVAL
  ) {

    lastWiFiAttempt = millis();

    Serial.println("WiFi reconnecting...");

    WiFi.disconnect(false, false);
    WiFi.begin(ssid, password);
  }
}

// ================= INPUT =================

void handleInput() {

  unsigned long currentDelay =
    (currentState == GAME_PLAYING)
    ? 80
    : buttonDelay;

  if (millis() - lastButtonCheck < currentDelay) {
    return;
  }

  // BACK

  if (digitalRead(BTN_BACK) == LOW) {

    lastButtonCheck = millis();

    playTone(1000, 40);

    if (currentState == GAME_PLAYING) {

      currentState = MENU_GAMES;

    } else {

      currentState = MENU_MAIN;

      planesLoaded = false;
      weatherLoaded = false;
    }

    updateDisplay();

    return;
  }

  // UP

  if (digitalRead(BTN_UP) == LOW) {

    lastButtonCheck = millis();

    playTone(1200, 40);

    if (currentState == MENU_MAIN) {

      mainMenuIndex = (mainMenuIndex + 1) % 4;

      updateDisplay();

    } else if (currentState == MENU_GAMES) {

      gameMenuIndex = (gameMenuIndex + 1) % 4;

      updateDisplay();

    } else if (currentState == MENU_SETTINGS) {

      settingsIndex = (settingsIndex + 1) % 2;

      updateDisplay();

    } else if (currentState == GAME_PLAYING) {

      if (selectedGameType == 0) {

        entityVy = -4;

      } else if (selectedGameType == 1) {

        playerY -= 8;

        if (playerY < 12) {
          playerY = 12;
        }

      } else if (selectedGameType == 2) {

        int8_t temp = dirX;

        dirX = -dirY;
        dirY = temp;
      }
    }

    return;
  }

  // SELECT

  if (digitalRead(BTN_SELECT) == LOW) {

    lastButtonCheck = millis();

    playTone(1500, 50);

    if (currentState == MENU_MAIN) {

      if (mainMenuIndex == 0)
        currentState = MENU_GAMES;

      else if (mainMenuIndex == 1)
        currentState = MENU_WEATHER;

      else if (mainMenuIndex == 2)
        currentState = MENU_PLANES;

      else if (mainMenuIndex == 3)
        currentState = MENU_SETTINGS;

      updateDisplay();

    } else if (currentState == MENU_GAMES) {

      if (gameMenuIndex == 3) {

        gameDifficulty = (gameDifficulty + 1) % 3;

      } else {

        selectedGameType = gameMenuIndex;

        currentState = GAME_PLAYING;

        initGame(selectedGameType);

        lastGameTick = millis();
      }

      updateDisplay();

    } else if (currentState == MENU_SETTINGS) {

      if (settingsIndex == 0) {
        buzzerEnabled = !buzzerEnabled;
      }

      updateDisplay();

    } else if (currentState == GAME_PLAYING) {

      if (selectedGameType == 1) {

        playerX += 10;

        if (playerX > 120) {
          playerX = 120;
        }

      } else if (selectedGameType == 2) {

        int8_t temp = dirX;

        dirX = dirY;
        dirY = -temp;
      }
    }

    return;
  }
}

// ================= DISPLAY =================

void updateDisplay() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (currentState == MENU_MAIN) {

    display.setCursor(0, 0);

    display.println("=== MAIN MENU ===");

    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    const char* items[] = {
      "Games",
      "Weather",
      "Planes",
      "Settings"
    };

    for (int i = 0; i < 4; i++) {

      display.setCursor(5, 15 + i * 12);

      display.print(i == mainMenuIndex ? "> " : "  ");

      display.println(items[i]);
    }

  } else if (currentState == MENU_GAMES) {

    display.setCursor(0, 0);

    display.println("=== GAMES ===");

    display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

    const char* diffS =
      (gameDifficulty == 0) ? "Easy" :
      (gameDifficulty == 1) ? "Medium" : "Hard";

    const char* games[] = {
      "1. Flappy Bird",
      "2. Froggit",
      "3. Snake",
      "Difficulty:"
    };

    for (int i = 0; i < 4; i++) {

      display.setCursor(2, 13 + i * 12);

      display.print(i == gameMenuIndex ? "> " : "  ");

      display.print(games[i]);

      if (i == 3) {

        display.print(" [");
        display.print(diffS);
        display.print("]");
      }
    }

  } else if (currentState == MENU_WEATHER) {

    display.setCursor(0, 0);

    display.println("=== WEATHER ===");

    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    if (WiFi.status() == WL_CONNECTED) {

      display.setCursor(0, 16);

      display.print("City: ");
      display.println(city);

      display.setCursor(0, 28);

      display.print("TEMP: ");
      display.print(temperature, 1);
      display.println(" C");

      display.setCursor(0, 40);

      display.print("HUMIDITY: %");
      display.println(humidity);

      display.setCursor(0, 52);

      display.print("WIND: ");
      display.print(windSpeed, 1);
      display.println(" m/s");

      if (!weatherLoaded) {
        display.setCursor(80, 0);
        display.print("Loading");
      }

    } else {

      display.setCursor(0, 25);

      display.println("WiFi not connected!");
    }

  } else if (currentState == MENU_PLANES) {

    display.setCursor(0, 0);

    display.println("=== FLIGHT TRACKER ===");

    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    if (WiFi.status() == WL_CONNECTED) {

      display.setCursor(0, 20);

      display.println("OpenSky Planes:");

      display.setCursor(0, 35);

      display.print("Active Planes: ");

      display.println(activeAircraftCount);

      if (!planesLoaded) {
        display.setCursor(80, 0);
        display.print("Loading");
      }

    } else {

      display.setCursor(0, 25);

      display.println("WiFi not connected!");
    }

  } else if (currentState == MENU_SETTINGS) {

    display.setCursor(0, 0);

    display.println("=== SETTINGS ===");

    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    display.setCursor(5, 20);

    display.print(settingsIndex == 0 ? "> " : "  ");

    display.print("Sound: ");

    display.println(buzzerEnabled ? "ON" : "OFF");

    display.setCursor(5, 35);

    display.print(settingsIndex == 1 ? "> " : "  ");

    display.print("WiFi: ");

    display.println(
      WiFi.status() == WL_CONNECTED
      ? "Connected"
      : "Disconnected"
    );
  }

  display.display();
}

// ================= GAME INITIALIZATION =================

void initGame(int gameType) {

  gameScore = 0;

  if (gameType == 0) {

    entityY = 32;
    entityVy = 0;

    obstacleX = 128;
    obstacleParam = 25;

  } else if (gameType == 1) {

    playerX = 64;
    playerY = 56;

    carX1 = 0;
    carX2 = 128;

  } else if (gameType == 2) {

    snakeLength = 3;

    dirX = 1;
    dirY = 0;

    for (int i = 0; i < snakeLength; i++) {

      snake[i].x = 10 - i;
      snake[i].y = 8;
    }

    placeFood();
  }
}

// ================= COLLISION DETECTION =================

bool rectsOverlap(
  int ax, int ay, int aw, int ah,
  int bx, int by, int bw, int bh
) {

  return (
    ax < bx + bw &&
    ax + aw > bx &&
    ay < by + bh &&
    ay + ah > by
  );
}

// ================= GAME OVER =================

void endGame() {

  playTone(300, 150);

  currentState = MENU_GAMES;

  updateDisplay();
}

// ================= FOOD SYSTEM =================

bool foodOnSnake(int x, int y) {

  for (int i = 0; i < snakeLength; i++) {

    if (snake[i].x == x && snake[i].y == y) {
      return true;
    }
  }

  return false;
}

void placeFood() {

  // Board: X = 0..31, Y = 3..15

  int attempts = 0;

  do {

    food.x = random(0, 32);
    food.y = random(3, 16);

    attempts++;

  } while (
    foodOnSnake(food.x, food.y) &&
    attempts < 500
  );
}

// ================= GAME LOOP =================

void runGameLoop() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);

  display.print("Score:");
  display.print(gameScore);

  display.drawLine(
    0, 10, 128, 10, SSD1306_WHITE
  );

  int speedMultiplier =
  (gameDifficulty == 0) ? 1 :
  (gameDifficulty == 1) ? 2 : 3;

  // ================= FLAPPY BIRD =================

  if (selectedGameType == 0) {

    display.setCursor(85, 0);

    display.print("[Flappy]");

    entityVy += 1;

    entityY += entityVy;

    obstacleX -= speedMultiplier;

    if (obstacleX < -10) {

      obstacleX = 128;

      obstacleParam = random(20, 40);

      gameScore++;
    }

    // Screen collision

    if (entityY < 12 || entityY + 6 > 64) {

      endGame();

      return;
    }

    // Pipe collision

    if (
      rectsOverlap(
        20, entityY, 6, 6,
        obstacleX, 12, 10, obstacleParam
      ) ||

      rectsOverlap(
        20, entityY, 6, 6,
        obstacleX,
        obstacleParam + 28,
        10,
        64 - (obstacleParam + 28)
      )
    ) {

      endGame();

      return;
    }

    display.fillRect(
      20, entityY, 6, 6, SSD1306_WHITE
    );

    display.fillRect(
      obstacleX,
      12,
      10,
      obstacleParam,
      SSD1306_WHITE
    );

    display.fillRect(
      obstacleX,
      obstacleParam + 28,
      10,
      64 - (obstacleParam + 28),
      SSD1306_WHITE
    );
  }

  // ================= FROGGIT =================

  else if (selectedGameType == 1) {

    display.setCursor(80, 0);

    display.print("[Froggit]");

    carX1 += speedMultiplier;

    if (carX1 > 128) {
      carX1 = -20;
    }

    carX2 -= speedMultiplier;

    if (carX2 < -20) {
      carX2 = 128;
    }

    // First car collision

    bool hitCar1 = rectsOverlap(
      playerX, playerY, 6, 6,
      carX1, 30, 14, 12
    );

    // Second car collision

    bool hitCar2 = rectsOverlap(
      playerX, playerY, 6, 6,
      carX2, 48, 14, 12
    );

    if (hitCar1 || hitCar2) {

      endGame();

      return;
    }

    display.drawFastHLine(
      0, 26, 128, SSD1306_WHITE
    );

    display.drawFastHLine(
      0, 46, 128, SSD1306_WHITE
    );

    display.fillRect(
      carX1, 30, 14, 12, SSD1306_WHITE
    );

    display.fillRect(
      carX2, 48, 14, 12, SSD1306_WHITE
    );

    display.fillRect(
      playerX, playerY, 6, 6, SSD1306_WHITE
    );
  }

  // ================= SNAKE =================

  else if (selectedGameType == 2) {

    display.setCursor(85, 0);

    display.print("[Snake]");

    Point newHead;

    newHead.x = snake[0].x + dirX;
    newHead.y = snake[0].y + dirY;

    bool ateFood =
      (newHead.x == food.x && newHead.y == food.y);

    bool willGrow =
      ateFood && snakeLength < 30;

    // Wall collision

    if (
      newHead.x < 0 ||
      newHead.x >= 32 ||
      newHead.y < 3 ||
      newHead.y >= 16
    ) {

      endGame();

      return;
    }

    // Self collision
    // Tail is excluded when it will move away.

    for (int i = 1; i < snakeLength; i++) {

      if (
        newHead.x == snake[i].x &&
        newHead.y == snake[i].y
      ) {

        if (
          i == snakeLength - 1 &&
          !willGrow
        ) {
          continue;
        }

        endGame();

        return;
      }
    }

    // Increase length before shifting body

    if (willGrow) {
      snakeLength++;
    }

    for (int i = snakeLength - 1; i > 0; i--) {

      snake[i] = snake[i - 1];
    }

    snake[0] = newHead;

    // Food collected

    if (ateFood) {

      playTone(1800, 40);

      gameScore += 5;

      placeFood();
    }

    // Draw food

    display.drawRect(
      food.x * 4,
      food.y * 4,
      3,
      3,
      SSD1306_WHITE
    );

    // Draw snake

    for (int i = 0; i < snakeLength; i++) {

      display.fillRect(
        snake[i].x * 4,
        snake[i].y * 4,
        4,
        4,
        SSD1306_WHITE
      );
    }
  }

  display.display();
}

// ================= OPENSKY =================

void getOpenSkyData() {

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  WiFiClientSecure secureClient;

  // TLS encryption; certificate verification is disabled.
  secureClient.setInsecure();

  HTTPClient http;

  char urlBuffer[160];

  snprintf(
    urlBuffer,
    sizeof(urlBuffer),
    "https://opensky-network.org/api/states/all?lamin=38.0&lamax=40.0&lomin=28.0&lomax=30.0"
  );

  if (!http.begin(secureClient, urlBuffer)) {
    Serial.println("OpenSky begin failed");
    return;
  }

  if (
    strlen(openskyUser) > 0 &&
    strcmp(openskyUser, "YOUR_OPENSKY_NAME") != 0
  ) {

    http.setAuthorization(
      openskyUser,
      openskyPass
    );
  }

  http.setTimeout(5000);

  int httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {

    WiFiClient* stream = http.getStreamPtr();

    // Only parse the first field of every aircraft state.
    // This is enough to count the aircraft.

    StaticJsonDocument<64> filter;

    filter["states"][0][0] = true;

    DynamicJsonDocument doc(8192);

    DeserializationError error = deserializeJson(
      doc,
      *stream,
      DeserializationOption::Filter(filter)
    );

    if (!error) {

      JsonArray states = doc["states"];

      activeAircraftCount = states.isNull()
        ? 0
        : states.size();

      planesLoaded = true;

      Serial.print("Active aircraft: ");
      Serial.println(activeAircraftCount);

    } else {

      Serial.print("OpenSky parsing error: ");
      Serial.println(error.c_str());
    }

  } else {

    Serial.print("OpenSky HTTP error: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

// ================= WEATHER =================

void getWeatherData() {

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  WiFiClientSecure secureClient;

  // TLS encryption; certificate verification is disabled.
  secureClient.setInsecure();

  HTTPClient http;

  char urlBuffer[256];

  snprintf(
    urlBuffer,
    sizeof(urlBuffer),
    "https://api.openweathermap.org/data/2.5/weather?q=%s,%s&units=metric&appid=%s",
    city,
    country,
    weatherApiKey
  );

  if (!http.begin(secureClient, urlBuffer)) {

    Serial.println("Weather begin failed");

    return;
  }

  http.setTimeout(5000);

  int httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {

    String payload = http.getString();

    DynamicJsonDocument doc(2048);

    DeserializationError error = deserializeJson(
      doc,
      payload
    );

    if (!error) {

      if (
        !doc["main"]["temp"].isNull() &&
        !doc["main"]["humidity"].isNull() &&
        !doc["wind"]["speed"].isNull()
      ) {

        temperature = doc["main"]["temp"];
        humidity = doc["main"]["humidity"];
        windSpeed = doc["wind"]["speed"];

        weatherLoaded = true;

        Serial.println("Weather updated!");

      } else {

        Serial.println("Weather data incomplete");
      }

    } else {

      Serial.print("Weather JSON error: ");
      Serial.println(error.c_str());
    }

  } else {

    Serial.print("Weather HTTP error: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

// ================= BUZZER =================

void playTone(int frequency, int duration) {

  if (buzzerEnabled) {

    tone(
      PIN_BUZZER,
      frequency,
      duration
    );
  }
}

// ================= RGB 
void updateRgbAnimation() {

  if (millis() - lastRgbChange > 1000) {

    lastRgbChange = millis();

    rgbState = (rgbState + 1) % 3;

    if (rgbState == 0) {

      rgbLed.setPixelColor(
        0,
        rgbLed.Color(0, 0, 255)
      );

    } else if (rgbState == 1) {

      rgbLed.setPixelColor(
        0,
        rgbLed.Color(255, 0, 0)
      );

    } else {
    rgbLed.setPixelColor(
        0,
        rgbLed.Color(0, 255, 0)
      );
    }

    rgbLed.show();
  }
}
    
