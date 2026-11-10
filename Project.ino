#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// --- SETTINGS ---
const char* ssid     = "YOURWIFI";
const char* password = "YOURPASSWORD";
const char* weatherApiKey = "YOURAPI";
const char* city     = "YOURCITY";
const char* country  = "YOURCOUNTRY";

// OpenSky Info
const char* openskyUser = "YOUROPENSKYNAME";
const char* openskyPass = "YOUROPENSKYAPI";

// --- OLED DISPLAY SETTINGS (SDA: 9, SCL: 8) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- BUTTON PINS ---
const int BTN_DOWN     = 13; 
const int BTN_SELECT = 12; 
const int BTN_BACK   = 11; 

// --- BUZZER ---
const int PIN_BUZZER = 10;
bool buzzerEnabled = true;

// --- RGB LED ---
#define RGB_PIN 21
#define NUM_PIXELS 1
Adafruit_NeoPixel rgbLed(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// --- SYSTEM STATES ---
enum State { MENU_MAIN, MENU_GAMES, MENU_WEATHER, MENU_PLANES, MENU_SETTINGS, GAME_PLAYING };
State currentState = MENU_MAIN;

int mainMenuIndex = 0; 
int gameMenuIndex = 0; 
int settingsIndex = 0; 
int selectedGameType = 0; 
int gameDifficulty = 1; 

// Data variables
float temperature = 0.0;
int humidity = 0;
float windSpeed = 0.0;
bool weatherLoaded = false;
int activeAircraftCount = 0;
bool planesLoaded = false;

// RGB animation
unsigned long lastRgbChange = 0;
int rgbState = 0;

// Button debouncing
unsigned long lastButtonCheck = 0;
const unsigned long buttonDelay = 150; 

// Game Variables
int gameScore = 0;
int playerX = 64;
int playerY = 56;
int entityY = 32;     
int entityVy = 0;
int obstacleX = 128;
int obstacleParam = 25;
int carX1 = 0, carX2 = 128; 

// Snake Game Variables
struct Point { int8_t x; int8_t y; };
Point snake[30];
int snakeLength = 3;
int8_t dirX = 1;
int8_t dirY = 0;
Point food;

void setup() {
  Serial.begin(115200);
  Wire.begin(9, 8);
  Wire.setClock(100000);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  rgbLed.begin();
  rgbLed.setBrightness(30);
  rgbLed.show();

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed!"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("System Booting...");
  display.display();
  delay(1000);

  WiFi.begin(ssid, password);
  updateDisplay();
}

void loop() {
  updateRgbAnimation();
  handleInput();

  if (currentState == GAME_PLAYING) {
    runGameLoop();
  }

  if (currentState == MENU_WEATHER && !weatherLoaded && WiFi.status() == WL_CONNECTED) {
    getWeatherData();
    updateDisplay();
  }

  if (currentState == MENU_PLANES && !planesLoaded && WiFi.status() == WL_CONNECTED) {
    getOpenSkyData();
    updateDisplay();
  }
}

void handleInput() {
  unsigned long currentDelay = (currentState == GAME_PLAYING) ? 80 : buttonDelay;
  if (millis() - lastButtonCheck < currentDelay) return;

  // Back Button
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

  // Up Button
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
          gameScore += 10;
          playerY = 56;
          playTone(1800, 50);
        }
      } else if (selectedGameType == 2) {
        int8_t temp = dirX;
        dirX = -dirY;
        dirY = temp;
      }
    }
    return;
  }

  // Select Button
  if (digitalRead(BTN_SELECT) == LOW) {
    lastButtonCheck = millis();
    playTone(1500, 50);
    if (currentState == MENU_MAIN) {
      if (mainMenuIndex == 0) currentState = MENU_GAMES;
      else if (mainMenuIndex == 1) currentState = MENU_WEATHER;
      else if (mainMenuIndex == 2) currentState = MENU_PLANES;
      else if (mainMenuIndex == 3) currentState = MENU_SETTINGS;
      updateDisplay();
    } else if (currentState == MENU_GAMES) {
      if (gameMenuIndex == 3) {
        gameDifficulty = (gameDifficulty + 1) % 3;
      } else {
        selectedGameType = gameMenuIndex; 
        currentState = GAME_PLAYING;
        initGame(selectedGameType);
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
        if (playerX > 120) playerX = 120;
      } else if (selectedGameType == 2) {
        int8_t temp = dirX;
        dirX = dirY;
        dirY = -temp;
      }
    }
    return;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (currentState == MENU_MAIN) {
    display.setCursor(0, 0);
    display.println("=== MAIN MENU ===");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    const char* items[] = {"Games", "Weather", "Planes (OpenSky)", "Settings"};
    for (int i = 0; i < 4; i++) {
      display.setCursor(5, 15 + (i * 12));
      if (i == mainMenuIndex) display.print("> ");
      else display.print("  ");
      display.println(items[i]);
    }
  } 
  else if (currentState == MENU_GAMES) {
    display.setCursor(0, 0);
    display.println("=== GAMES ===");
    display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

    const char* diffStr = (gameDifficulty == 0) ? "Easy" : (gameDifficulty == 1) ? "Medium" : "Hard";
    const char* games[] = {"1. Flappy Bird", "2. Froggit", "3. Snake", "Difficulty:"};
    for (int i = 0; i < 4; i++) {
      display.setCursor(2, 13 + (i * 12));
      if (i == gameMenuIndex) display.print("> ");
      else display.print("  ");
      display.print(games[i]);
      if (i == 3) {
        display.print(" [");
        display.print(diffStr);
        display.print("]");
      }
    }
  } 
  else if (currentState == MENU_WEATHER) {
    display.setCursor(0, 0);
    display.println("=== WEATHER ===");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    if (WiFi.status() == WL_CONNECTED) {
      display.setCursor(0, 16);
      display.print("City: "); display.println(city);
      display.setCursor(0, 28);
      display.print("Temp: "); display.print(temperature, 1); display.println(" C");
      display.setCursor(0, 40);
      display.print("Humidity: %"); display.println(humidity);
      display.setCursor(0, 52);
      display.print("Wind: "); display.print(windSpeed, 1); display.println(" m/s");
    } else {
      display.setCursor(0, 25);
      display.println("WiFi Disconnected!");
    }
  }
  else if (currentState == MENU_PLANES) {
    display.setCursor(0, 0);
    display.println("=== FLIGHT TRACKER ===");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    if (WiFi.status() == WL_CONNECTED) {
      display.setCursor(0, 20);
      display.println("OpenSky Data:");
      display.setCursor(0, 35);
      display.print("Active Planes: "); 
      display.println(activeAircraftCount);
    } else {
      display.setCursor(0, 25);
      display.println("WiFi Disconnected!");
    }
  }
  else if (currentState == MENU_SETTINGS) {
    display.setCursor(0, 0);
    display.println("=== SETTINGS ===");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    display.setCursor(5, 20);
    if (settingsIndex == 0) display.print("> "); else display.print("  ");
    display.print("Buzzer: "); 
    display.println(buzzerEnabled ? "On" : "Off");

    display.setCursor(5, 35);
    if (settingsIndex == 1) display.print("> "); else display.print("  ");
    display.print("WiFi: ");
    display.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Offline");
  }

  display.display();
}

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
    food.x = random(2, 30);
    food.y = random(4, 14);
  }
}

void runGameLoop() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Score:"); display.print(gameScore);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE); 

  int speedMultiplier = (gameDifficulty == 0) ? 2 : (gameDifficulty == 1) ? 3 : 5;

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

    if (entityY > 60 || entityY < 12) {
      playTone(300, 150);
      currentState = MENU_GAMES;
      updateDisplay();
      return;
    }

    display.fillRect(20, entityY, 6, 6, SSD1306_WHITE);
    display.fillRect(obstacleX, 12, 10, obstacleParam, SSD1306_WHITE);
    display.fillRect(obstacleX, obstacleParam + 28, 10, 64 - (obstacleParam + 28), SSD1306_WHITE);
  } 
  else if (selectedGameType == 1) {
    display.setCursor(80, 0);
    display.print("[Froggit]");

    carX1 += speedMultiplier;
    if (carX1 > 128) carX1 = -20;

    carX2 -= speedMultiplier;
    if (carX2 < -20) carX2 = 128;

    if (playerY > 28 && playerY < 45 && abs(playerX - carX1) < 12) {
      playTone(200, 200);
      currentState = MENU_GAMES;
      updateDisplay();
      return;
    }

    display.drawFastHLine(0, 26, 128, SSD1306_WHITE);
    display.drawFastHLine(0, 46, 128, SSD1306_WHITE);

    display.fillRect(carX1, 30, 14, 12, SSD1306_WHITE); 
    display.fillRect(carX2, 48, 14, 12, SSD1306_WHITE); 

    display.fillRect(playerX, playerY, 6, 6, SSD1306_WHITE);
  }
  else if (selectedGameType == 2) {
    display.setCursor(85, 0);
    display.print("[Snake]");

    for (int i = snakeLength - 1; i > 0; i--) {
      snake[i] = snake[i - 1];
    }
    snake[0].x += dirX;
    snake[0].y += dirY;

    if (snake[0].x < 0 || snake[0].x >= 32 || snake[0].y < 3 || snake[0].y >= 16) {
      playTone(300, 150);
      currentState = MENU_GAMES;
      updateDisplay();
      return;
    }

    for (int i = 1; i < snakeLength; i++) {
      if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
        playTone(300, 150);
        currentState = MENU_GAMES;
        updateDisplay();
        return;
      }
    }

    if (snake[0].x == food.x && snake[0].y == food.y) {
      playTone(1800, 40);
      gameScore += 5;
      if (snakeLength < 30) snakeLength++;
      food.x = random(0, 32);
      food.y = random(4, 16);
    }

    display.drawRect(food.x * 4, food.y * 4, 3, 3, SSD1306_WHITE);
    for (int i = 0; i < snakeLength; i++) {
      display.fillRect(snake[i].x * 4, snake[i].y * 4, 4, 4, SSD1306_WHITE);
    }
  }

  display.display();
  int delayMs = (selectedGameType == 2) ? (150 - (gameDifficulty * 35)) : 30;
  delay(delayMs); 
}

void getOpenSkyData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    char urlBuffer[128];
    snprintf(urlBuffer, sizeof(urlBuffer), "https://opensky-network.org/api/states/all?lamin=36.0&lamax=42.0&lomin=26.0&lomax=45.0");
    
    http.begin(urlBuffer);
    http.setAuthorization(openskyUser, openskyPass);
    http.setTimeout(3000); 

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String payload = http.getString();
      
      StaticJsonDocument<64> filter;
      filter["states"] = true;

      DynamicJsonDocument doc(3072);
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
      if (!error) {
        JsonArray states = doc["states"];
        activeAircraftCount = states.size();
        planesLoaded = true;
      }
    }
    http.end();
  }
}

void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    char urlBuffer[160];
    snprintf(urlBuffer, sizeof(urlBuffer), "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&units=metric&appid=%s", city, country, weatherApiKey);
    
    http.begin(urlBuffer);
    http.setTimeout(3000);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      temperature = doc["main"]["temp"];
      humidity = doc["main"]["humidity"];
      windSpeed = doc["wind"]["speed"];
      weatherLoaded = true;
    }
    http.end();
  }
}

void playTone(int frequency, int duration) {
  if (buzzerEnabled) {
    tone(PIN_BUZZER, frequency, duration);
  }
}

void updateRgbAnimation() {
  if (millis() - lastRgbChange > 1000) {
    lastRgbChange = millis();
    rgbState = (rgbState + 1) % 3;
    if (rgbState == 0) rgbLed.setPixelColor(0, rgbLed.Color(0, 0, 255));
    else if (rgbState == 1) rgbLed.setPixelColor(0, rgbLed.Color(255, 0, 0));
    else if (rgbState == 2) rgbLed.setPixelColor(0, rgbLed.Color(0, 255, 0));
    rgbLed.show();
  }
}
