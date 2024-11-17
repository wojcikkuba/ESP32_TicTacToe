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

const char* ssid = "ESP32 WLAN";
const char* password = "12345678";

// Utworzenie obiektu serwera na porcie 80
WebServer server(80);

String winnerMessage = "";

// Funkcja generująca HTML reprezentujący aktualną planszę
String generateBoardHTML() {
  String html = "<!DOCTYPE html><html><head><title>Tic Tac Toe Server</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='1'>"; // Automatyczne odświeżanie strony co 1 sekunda
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; }";
  html += "table { border-collapse: collapse; margin: 0 auto; }";
  html += "td { width: 60px; height: 60px; text-align: center; font-size: 24px; border: 1px solid #000; }";
  html += "p { font-size: 20px; }";
  html += "</style></head><body>";

  // Nagłówek z informacją o grze
  html += "<h1>Stan gry Tic Tac Toe</h1>";

  // Tworzenie planszy gry jako tabeli HTML
  html += "<table>";
  for (int row = 0; row < GRID_SIZE; row++) {
    html += "<tr>";
    for (int col = 0; col < GRID_SIZE; col++) {
      char mark = (board[row][col] == 1) ? 'X' : (board[row][col] == 2) ? 'O' : ' ';
      html += "<td>" + String(mark) + "</td>";
    }
    html += "</tr>";
  }
  html += "</table>";

  // Komunikat o stanie gry
  if (!winnerMessage.isEmpty()) {
    Serial.println("Generowanie komunikatu o wygranej w HTML.");
    html += "<p style='color: red; font-size: 24px;'>Wygrana: " + winnerMessage + "</p>";
  } else {
    Serial.println("Brak komunikatu o wygranej do wyświetlenia.");
    html += "<p>Aktualny gracz: " + String((currentPlayer == 1) ? "Krzyzyk (X)" : "Kolko (O)") + "</p>";
  }

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
}

void loop() {
  server.handleClient();

  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      if (isButtonPressed(buttonPins[row][col], row, col)) {
        if (board[row][col] == 0) {
          board[row][col] = currentPlayer;
          clearOccupiedMessage();
          if (currentPlayer == 1) {
            drawX(row, col);
            currentPlayer = 2;
          } else {
            drawO(row, col);
            currentPlayer = 1;
          }
          drawPlayerTurn();

          server.send(200, "text/html", generateBoardHTML()); // Zaktualizuj stronę po wykonaniu ruchu

          if (checkWin()) {
            showWinMessage();  // Wyświetlenie komunikatu na ekranie TFT
            Serial.println("Ustawienie komunikatu o wygranej: " + winnerMessage);
            server.send(200, "text/html", generateBoardHTML());  // Wyślij zaktualizowaną stronę z komunikatem o zwycięstwie
            Serial.println("Wysłanie komunikatu o zwycięzcy na stronę.");
            delay(3000);  // Opóźnienie przed zresetowaniem gry, aby użytkownik mógł zobaczyć komunikat
            resetGame();  // Dopiero teraz zresetuj grę
            return;
          } else if (checkDraw()) {
            showWinMessage();  // Wyświetlenie komunikatu na ekranie TFT
            server.send(200, "text/html", generateBoardHTML());  // Wyślij zaktualizowaną stronę z komunikatem o zwycięstwie
            Serial.println("Wysłanie komunikatu o zwycięzcy na stronę.");
            delay(3000);  // Opóźnienie przed zresetowaniem gry, aby użytkownik mógł zobaczyć komunikat
            resetGame();  // Dopiero teraz zresetuj grę
            return;
          }
        } else {
          showOccupiedMessage();
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
  tft.drawLine(CELL_SIZE, 30, CELL_SIZE, SCREEN_HEIGHT, ILI9341_GREEN);
  tft.drawLine(CELL_SIZE * 2, 30, CELL_SIZE * 2, SCREEN_HEIGHT, ILI9341_GREEN);
  tft.drawLine(0, CELL_SIZE + 30, SCREEN_WIDTH, CELL_SIZE + 30, ILI9341_GREEN);
  tft.drawLine(0, CELL_SIZE * 2 + 30, SCREEN_WIDTH, CELL_SIZE * 2 + 30, ILI9341_GREEN);
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

  // Wyczyszczenie komunikatu o zwycięzcy
  winnerMessage = "";

  // Wyczyszczenie i ponowne narysowanie planszy
  tft.fillScreen(ILI9341_BLACK); // Wyczyszczenie ekranu
  drawGrid();                    // Ponowne narysowanie kratki
  drawPlayerTurn();              // Aktualizacja komunikatu o turze
  occupiedMessageDisplayed = false;        // Wyczyść komunikat, jeśli jest wyświetlany
}