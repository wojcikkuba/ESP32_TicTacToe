#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <WiFi.h>

// Definicje pinów dla wyświetlacza
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17

// Definicje pinów dla przycisków
const int buttonPins[3][3] = {
  {13, 12, 14},
  {27, 26, 25},
  {33, 32, 35}
};

// Utworzenie obiektu wyświetlacza
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Stałe do określenia wielkości planszy
const int GRID_SIZE = 3;
const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 320;
const int CELL_SIZE = SCREEN_WIDTH / GRID_SIZE;

// Tablica reprezentująca stan planszy (0: puste, 1: krzyżyk, 2: kółko)
int board[3][3] = {0};
int currentPlayer = 1;  // 1: Krzyżyk, 2: Kółko

const int debounceDelay = 200; // czas debounce w milisekundach
unsigned long lastDebounceTime[3][3] = {0}; // czas ostatniego naciśnięcia dla każdego przycisku

bool occupiedMessageDisplayed = false; // flaga dla komunikatu "zajęte pole"

const char* ssid = "ESP32 WLAN";
const char* password = "12345678";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Łączenie...");
  }
  
  Serial.println("Połączono z siecią Wi-Fi.");
  Serial.println(WiFi.localIP());

  tft.begin();
  tft.fillScreen(ILI9341_BLACK);

  // Inicjalizacja pinów dla przycisków
  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      pinMode(buttonPins[row][col], INPUT_PULLUP);
    }
  }

  drawGrid();
  drawPlayerTurn();
}

void loop() {
  // Sprawdzenie stanu każdego przycisku
  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      if (isButtonPressed(buttonPins[row][col], row, col)) {
        if (board[row][col] == 0) {  // Sprawdź, czy pole jest wolne
          board[row][col] = currentPlayer;
          clearOccupiedMessage(); // Usuń komunikat, jeśli wybrano wolne pole
          if (currentPlayer == 1) {
            drawX(row, col);
            currentPlayer = 2;
          } else {
            drawO(row, col);
            currentPlayer = 1;
          }
          drawPlayerTurn();

          if (checkWin()) {
            showWinMessage();
            delay(5000); // Wyświetla komunikat przez 5 sekund
            resetGame();
            return;
          } else if (checkDraw()) {
            showDrawMessage();
            delay(5000); // Wyświetla komunikat przez 5 sekund
            resetGame();
            return;
          }
        } else {
          showOccupiedMessage(); // Wyświetl komunikat "zajęte pole"
        }
      }
    }
  }
}

// Funkcja do debounce przycisków
bool isButtonPressed(int pin, int row, int col) {
  if (digitalRead(pin) == LOW) {
    if (millis() - lastDebounceTime[row][col] > debounceDelay) {
      lastDebounceTime[row][col] = millis();
      return true;
    }
  }
  return false;
}

// Funkcja rysująca kratkę
void drawGrid() {
  tft.fillScreen(ILI9341_BLACK);  // Wyczyść ekran przed narysowaniem planszy
  tft.drawLine(CELL_SIZE, 30, CELL_SIZE, SCREEN_HEIGHT, ILI9341_GREEN);
  tft.drawLine(CELL_SIZE * 2, 30, CELL_SIZE * 2, SCREEN_HEIGHT, ILI9341_GREEN);
  tft.drawLine(0, CELL_SIZE + 30, SCREEN_WIDTH, CELL_SIZE + 30, ILI9341_GREEN);
  tft.drawLine(0, CELL_SIZE * 2 + 30, SCREEN_WIDTH, CELL_SIZE * 2 + 30, ILI9341_GREEN);
}

// Funkcja wyświetlająca aktualny gracz
void drawPlayerTurn() {
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, ILI9341_BLACK);  // Wyczyść pole tekstowe nad planszą
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  if (currentPlayer == 1) {
    tft.print("Tura: Krzyzyk");
  } else {
    tft.print("Tura: Kolko");
  }
}

// Funkcja rysująca X
void drawX(int row, int col) {
  int x0 = col * CELL_SIZE;
  int y0 = row * CELL_SIZE + 30;
  tft.drawLine(x0 + 10, y0 + 10, x0 + CELL_SIZE - 10, y0 + CELL_SIZE - 10, ILI9341_RED);
  tft.drawLine(x0 + 10, y0 + CELL_SIZE - 10, x0 + CELL_SIZE - 10, y0 + 10, ILI9341_RED);
}

// Funkcja rysująca O
void drawO(int row, int col) {
  int x0 = col * CELL_SIZE + CELL_SIZE / 2;
  int y0 = row * CELL_SIZE + CELL_SIZE / 2 + 30;
  tft.drawCircle(x0, y0, CELL_SIZE / 2 - 10, ILI9341_BLUE);
}

// Funkcja sprawdzająca warunek zwycięstwa
bool checkWin() {
  for (int i = 0; i < GRID_SIZE; i++) {
    if (board[i][0] != 0 && board[i][0] == board[i][1] && board[i][1] == board[i][2]) return true;
    if (board[0][i] != 0 && board[0][i] == board[1][i] && board[1][i] == board[2][i]) return true;
  }
  if (board[0][0] != 0 && board[0][0] == board[1][1] && board[1][1] == board[2][2]) return true;
  if (board[0][2] != 0 && board[0][2] == board[1][1] && board[1][1] == board[2][0]) return true;

  return false;
}

// Funkcja sprawdzająca remis
bool checkDraw() {
  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      if (board[row][col] == 0) return false;  // Znaleziono puste pole, brak remisu
    }
  }
  return true;  // Wszystkie pola zapełnione, remis
}

// Funkcja wyświetlająca komunikat o wygranej
void showWinMessage() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(20, 140);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  if (currentPlayer == 2) {
    tft.print("Wygrywa Krzyzyk!");
  } else {
    tft.print("Wygrywa Kolko!");
  }
}

// Funkcja wyświetlająca komunikat o remisie
void showDrawMessage() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(50, 140);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.print("Remis!");
}

// Funkcja wyświetlająca komunikat "zajęte pole"
void showOccupiedMessage() {
  if (!occupiedMessageDisplayed) { // Jeśli komunikat nie jest wyświetlany
    tft.setCursor(10, SCREEN_HEIGHT - 20);
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(2);
    tft.print("Zajete pole!");
    occupiedMessageDisplayed = true;
  }
}

// Funkcja czyszcząca komunikat "zajęte pole"
void clearOccupiedMessage() {
  if (occupiedMessageDisplayed) { // Jeśli komunikat jest wyświetlany
    tft.fillRect(0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20, ILI9341_BLACK); // Wyczyść tekst
    occupiedMessageDisplayed = false;
  }
}

// Funkcja resetująca grę
void resetGame() {
  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      board[row][col] = 0;
    }
  }
  currentPlayer = 1;
  drawGrid();
  drawPlayerTurn();
  clearOccupiedMessage(); // Wyczyść komunikat przy resecie
}