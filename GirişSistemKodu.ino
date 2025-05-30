#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Servo.h>

// --- Donanım Tanımlamaları ---
#define RST_PIN 5
#define SS_PIN 53
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_Fingerprint finger(&Serial1);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo kapi;

#define BUZZER 7
#define LED_R 8
#define LED_G 9
#define LED_B 10
#define SERVO_PIN 6

// --- Setup ---
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  finger.begin(57600);
  lcd.init(); lcd.backlight();
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  kapi.attach(SERVO_PIN);

  if (!finger.verifyPassword()) {
    lcd.setCursor(0, 0);
    lcd.print("Parmak hatasi!");
    while (1);
  }

  lcd.setCursor(0, 0);
  lcd.print("Giris Bekleniyor");
  kapi.write(0); // Kapalı
}

// --- Ana Döngü ---
void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  byte uid[4];
  for (int i = 0; i < 4; i++) uid[i] = rfid.uid.uidByte[i];

  int addr = findUserByUID(uid);
  if (addr == -1) {
    lcd.clear(); lcd.print("Yetkisiz kart!");
    errorSignal();
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
    return;
  }

  String name = readName(addr);
  int fingerID = EEPROM.read(addr + 1 + name.length() + 4);
  int yetki = EEPROM.read(addr + 1 + name.length() + 4 + 1);

  lcd.clear(); lcd.print("Merhaba, "); lcd.print(name);
  delay(1000);

  if (yetki == 1) {
    lcd.clear(); lcd.print("Kapi 1 Aciliyor");
    openDoor(LED_B);
  } else if (yetki == 2) {
    lcd.clear(); lcd.print("Parmak gerekli");
    delay(1000);
    if (fingerSearch(fingerID)) {
      lcd.clear(); lcd.print("Dogrulandi!");
      openDoor(LED_G);
    } else {
      lcd.clear(); lcd.print("Parmak HATA!");
      errorSignal();
    }
  }

  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
  lcd.clear(); lcd.print("Giris Bekleniyor");
}

// --- Yardımcı Fonksiyonlar ---

int findUserByUID(byte *uid) {
  uint8_t count = EEPROM.read(0);
  int addr = 1;
  for (int i = 0; i < count; i++) {
    int nameLen = EEPROM.read(addr++);
    addr += nameLen;

    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (EEPROM.read(addr + j) != uid[j]) match = false;
    }
    if (match) return addr - nameLen - 1;
    addr += 4 + 1 + 1;
  }
  return -1;
}

String readName(int addr) {
  int nameLen = EEPROM.read(addr++);
  String name = "";
  for (int i = 0; i < nameLen; i++) name += (char)EEPROM.read(addr++);
  return name;
}

bool fingerSearch(int expectedID) {
  finger.getImage();
  if (finger.image2Tz() != FINGERPRINT_OK) return false;
  if (finger.fingerSearch() != FINGERPRINT_OK) return false;
  return finger.fingerID == expectedID;
}

void openDoor(int ledColor) {
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
  digitalWrite(ledColor, HIGH);
  kapi.write(90);
  delay(2000);
  kapi.write(0);
  delay(1000);
  digitalWrite(ledColor, LOW);
}

void errorSignal() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(BUZZER, HIGH);
  delay(1000);
  digitalWrite(LED_R, LOW);
  digitalWrite(BUZZER, LOW);
  delay(500);
}