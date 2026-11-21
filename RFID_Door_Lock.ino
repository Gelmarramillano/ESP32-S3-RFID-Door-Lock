#include <SPI.h>
#include <MFRC522.h>
#include <U8g2lib.h>
#include <DFRobotDFPlayerMini.h>
#include <string.h>

constexpr int RFID_SCK = 12;
constexpr int RFID_MISO = 13;
constexpr int RFID_MOSI = 11;
constexpr int RFID_SS = 4;
constexpr int RFID_RST = 8;

// Replace these example bytes with your own card UID.
constexpr byte ALLOWED_UID[] = {0x12, 0x34, 0x56, 0x78};

constexpr int OLED_SCK = 14;
constexpr int OLED_MOSI = 15;
constexpr int OLED_CS = 6;
constexpr int OLED_DC = 7;
constexpr int OLED_RESET = 5;

constexpr int DFPLAYER_RX = 17;
constexpr int DFPLAYER_TX = 18;
constexpr int DENIED_TRACK = 1;
constexpr int GRANTED_TRACK = 2;
constexpr int VOLUME = 25;

constexpr int RELAY_PIN = 10;  // Active-HIGH 2N2222 driver
constexpr unsigned long UNLOCK_MS = 3000;
constexpr unsigned long DENIED_MS = 1500;

MFRC522 rfid(RFID_SS, RFID_RST);
U8G2_SSD1309_128X64_NONAME0_F_4W_SW_SPI oled(
    U8G2_R0, OLED_SCK, OLED_MOSI, OLED_CS, OLED_DC, OLED_RESET);
HardwareSerial dfSerial(1);
DFRobotDFPlayerMini player;
bool audioReady = false;

enum class Screen { Waiting, Granted, Denied };
Screen screen = Screen::Waiting;
unsigned long resultStarted = 0;

void showMessage(const char* message) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_helvB10_tr);
  const int x = (oled.getDisplayWidth() - oled.getStrWidth(message)) / 2;
  oled.drawStr(x, 37, message);
  oled.sendBuffer();
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.begin(115200);

  oled.begin();
  showMessage("STARTING...");
  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  rfid.PCD_Init();

  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(2000);
  audioReady = player.begin(dfSerial);
  if (audioReady) {
    player.volume(VOLUME);
  } else {
    Serial.println("DFPlayer initialization failed. Audio disabled.");
  }

  Serial.println("Relay OFF. Tap your card.");
  showMessage("TAP YOUR CARD");
}

void loop() {
  if (screen != Screen::Waiting) {
    const unsigned long duration =
        screen == Screen::Granted ? UNLOCK_MS : DENIED_MS;

    if (millis() - resultStarted >= duration) {
      if (screen == Screen::Granted) {
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("Relay OFF");
      }
      screen = Screen::Waiting;
      showMessage("TAP YOUR CARD");
    }
    return;
  }

  if (audioReady && player.available()) {
    const uint8_t event = player.readType();
    const int value = player.read();
    if (event == DFPlayerError) {
      Serial.printf("DFPlayer error: %d\n", value);
    }
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("Card UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.printf("%02X", static_cast<unsigned int>(rfid.uid.uidByte[i]));
  }
  Serial.println();

  const bool allowed = rfid.uid.size == sizeof(ALLOWED_UID) &&
      memcmp(rfid.uid.uidByte, ALLOWED_UID, sizeof(ALLOWED_UID)) == 0;
  rfid.PICC_HaltA();

  screen = allowed ? Screen::Granted : Screen::Denied;
  const char* message = allowed ? "ACCESS GRANTED" : "ACCESS DENIED";
  Serial.println(message);
  showMessage(message);

  if (audioReady) {
    player.play(allowed ? GRANTED_TRACK : DENIED_TRACK);
  }
  if (allowed) {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Relay ON for 3 seconds");
  }
  resultStarted = millis();
}
