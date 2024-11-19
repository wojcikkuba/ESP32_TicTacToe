#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>

// Definicje pinów dla wyświetlacza
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17

// Definicje pinów dla przycisków
const int buttonPins[3][3] = {
  {33, 32, 21},
  {25, 26, 27},
  {14, 12, 13}
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
String occupiedMessage = "";

const char* ssid = "ESP32 WLAN";
const char* password = "12345678";

// Utworzenie obiektu serwera na porcie 80
WebServer server(80);

String winnerMessage = "";

enum GameState { PLAYING, WIN, DRAW };
GameState gameState = PLAYING;

unsigned long resetTime = 0; // Czas zaplanowanego resetu gry (0 = brak resetu)

// Funkcja generująca HTML reprezentujący aktualną planszę
String generateBoardHTML() {
  String html = "<!DOCTYPE html><html><head><title>Tic Tac Toe Server</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"; // Responsywność
  html += "<meta http-equiv='refresh' content='1'>"; // Automatyczne odświeżanie
  html += "<style>";
  html += "body { font-family: 'Roboto', sans-serif; text-align: center; background-color: #f0f8ff; margin: 0; padding: 0; }";
  html += "h1 { color: #333; }";
  html += "table { border-collapse: collapse; margin: 20px auto; box-shadow: 0 4px 8px rgba(0,0,0,0.2); }";
  html += "td { width: 60px; height: 60px; text-align: center; font-size: 40px; border: 1px solid #000; background-color: #ffffff; color: #ff1744; }";
  html += "p { font-size: 20px; color: #555; }";
  html += "input[type='submit'] { padding: 10px 20px; font-size: 16px; background-color: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 20px; }";
  html += "</style></head><body>";

  html += "<h1>Stan gry Kółko i Krzyżyk</h1>";
  html += "<table>";
  for (int row = 0; row < GRID_SIZE; row++) {
    html += "<tr>";
    for (int col = 0; col < GRID_SIZE; col++) {
      if (board[row][col] == 1) {
        html += "<td>&#x274C;</td>"; // Czerwony X
      } else if (board[row][col] == 2) {
        html += "<td>&#x2B55;</td>"; // Czerwone O
      } else {
        html += "<td></td>"; // Puste pole
      }
    }
    html += "</tr>";
  }
  html += "</table>";

  if (!occupiedMessage.isEmpty()) {
    html += "<p style='color: red; font-size: 18px;'>" + occupiedMessage + "</p>";
  }

  if (gameState == WIN || gameState == DRAW) {
    html += "<p style='color: red; font-size: 24px;'>" + winnerMessage + "</p>";
  } else if (gameState == PLAYING) {
    html += "<p>Aktualny gracz: " + String((currentPlayer == 1) ? "Krzyżyk (X)" : "Kółko (O)") + "</p>";
  }

  html += "<form action='/reset' method='get'>";
  html += "<input type='submit' value='Resetuj grę'>";
  html += "</form>";

  html += "</body></html>";
  return html;
}

// Funkcja obsługująca stronę główną
void handleRoot() {
  server.send(200, "text/html", generateBoardHTML());
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("Access Point utworzony.");
  Serial.print("Nazwa (SSID): ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.begin();
  Serial.println("Serwer HTTP uruchomiony.");

  tft.begin();
  tft.fillScreen(ILI9341_BLACK);

  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      pinMode(buttonPins[row][col], INPUT_PULLUP);
      board[row][col] = 0;
    }
  }

  occupiedMessageDisplayed = false;
  currentPlayer = 1;

  drawGrid();
  drawPlayerTurn();

  server.on("/reset", handleReset);
}

void loop() {
  server.handleClient(); // Obsługa klientów serwera HTTP

  // Sprawdź, czy gra wymaga resetu
  if (resetTime > 0 && millis() > resetTime) {
    resetGame();       // Zresetuj grę
    resetTime = 0;     // Wyzeruj czas resetu
    return;            // Wyjdź z pętli
  }

  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      if (isButtonPressed(buttonPins[row][col], row, col)) {
        if (board[row][col] == 0) {
          board[row][col] = currentPlayer;
          clearOccupiedMessage();
          occupiedMessage = "";

          if (currentPlayer == 1) {
            drawX(row, col);
            currentPlayer = 2;
          } else {
            drawO(row, col);
            currentPlayer = 1;
          }
          drawPlayerTurn();

          // Sprawdź, czy ktoś wygrał lub remis
          if (checkWin()) {
            gameState = WIN;
            showWinMessage(); // Wyświetl komunikat o wygranej
            resetTime = millis() + 3000; // Zaplanuj reset za 3 sekundy
          } else if (checkDraw()) {
            gameState = DRAW;
            showDrawMessage(); // Wyświetl komunikat o remisie
            resetTime = millis() + 3000; // Zaplanuj reset za 3 sekundy
          }
        } else {
          showOccupiedMessage(); // TFT
          occupiedMessage = "Zajęte pole!"; // HTML
        }
      }
    }
  }
}


bool isButtonPressed(int pin, int row, int col) {
  if (digitalRead(pin) == LOW) {
    if (millis() - lastDebounceTime[row][col] > debounceDelay) {
      lastDebounceTime[row][col] = millis();
      return true;
    }
  }
  return false;
}

void drawGrid() {
  tft.fillScreen(ILI9341_BLACK);
  int startY = 30;
  tft.drawLine(CELL_SIZE, startY, CELL_SIZE, SCREEN_HEIGHT - startY, ILI9341_GREEN);
  tft.drawLine(CELL_SIZE * 2, startY, CELL_SIZE * 2, SCREEN_HEIGHT - startY, ILI9341_GREEN);
  tft.drawLine(0, CELL_SIZE + startY, SCREEN_WIDTH, CELL_SIZE + startY, ILI9341_GREEN);
  tft.drawLine(0, CELL_SIZE * 2 + startY, SCREEN_WIDTH, CELL_SIZE * 2 + startY, ILI9341_GREEN);
}

void drawPlayerTurn() {
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, ILI9341_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  if (currentPlayer == 1) {
    tft.print("Tura: Krzyzyk");
  } else {
    tft.print("Tura: Kolko");
  }
}

void drawX(int row, int col) {
  int x0 = col * CELL_SIZE;
  int y0 = row * CELL_SIZE + 30;
  tft.drawLine(x0 + 10, y0 + 10, x0 + CELL_SIZE - 10, y0 + CELL_SIZE - 10, ILI9341_RED);
  tft.drawLine(x0 + 10, y0 + CELL_SIZE - 10, x0 + CELL_SIZE - 10, y0 + 10, ILI9341_RED);
}

void drawO(int row, int col) {
  int x0 = col * CELL_SIZE + CELL_SIZE / 2;
  int y0 = row * CELL_SIZE + CELL_SIZE / 2 + 30;
  tft.drawCircle(x0, y0, CELL_SIZE / 2 - 10, ILI9341_BLUE);
}

bool checkWin() {
  for (int i = 0; i < GRID_SIZE; i++) {
    if (board[i][0] != 0 && board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
      winnerMessage = "Wygrywa " + String((board[i][0] == 1) ? "Krzyżyk (X)" : "Kółko (O)") + "!";
      return true;
    }
    if (board[0][i] != 0 && board[0][i] == board[1][i] && board[1][i] == board[2][i]) {
      winnerMessage = "Wygrywa " + String((board[0][i] == 1) ? "Krzyżyk (X)" : "Kółko (O)") + "!";
      return true;
    }
  }
  if (board[0][0] != 0 && board[0][0] == board[1][1] && board[1][1] == board[2][2]) {
    winnerMessage = "Wygrywa " + String((board[0][0] == 1) ? "Krzyżyk (X)" : "Kółko (O)") + "!";
    return true;
  }
  if (board[0][2] != 0 && board[0][2] == board[1][1] && board[1][1] == board[2][0]) {
    winnerMessage = "Wygrywa " + String((board[0][2] == 1) ? "Krzyżyk (X)" : "Kółko (O)") + "!";
    return true;
  }
  return false;
}

bool checkDraw() {
  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      if (board[row][col] == 0) {
        return false;
      }
    }
  }
  winnerMessage = "Remis!";
  return true;
}

// Funkcja wyświetlająca komunikat o wygranej
void showWinMessage() {
  delay(3000); // Dodanie 3-sekundowego opóźnienia
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
  delay(3000); // Dodanie 3-sekundowego opóźnienia
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
  // Resetowanie tablicy reprezentującej planszę
  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      board[row][col] = 0;  // Ustawienie wszystkich pozycji na puste (0)
    }
  }

  // Ustawienie aktualnego gracza na początkowego (Krzyżyk)
  currentPlayer = 1;

  gameState = PLAYING;

  // Wyczyszczenie komunikatu o zwycięzcy
  winnerMessage = "";

  // Wyczyszczenie i ponowne narysowanie planszy
  tft.fillScreen(ILI9341_BLACK); // Wyczyszczenie ekranu
  drawGrid();                    // Ponowne narysowanie kratki
  drawPlayerTurn();              // Aktualizacja komunikatu o turze
  occupiedMessageDisplayed = false;        // Wyczyść komunikat, jeśli jest wyświetlany
}

// Funkcja obsługująca reset gry z poziomu serwera
void handleReset() {
  resetGame();
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0; url=/'></head><body></body></html>");
}
