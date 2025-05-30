// 1. Gerekli Kütüphaneler
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// 2. Donanım Tanımlamaları
#define RST_PIN 5
#define SS_PIN 53
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_Fingerprint finger(&Serial1);
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define MAX_NAME_LEN 10

// 3. Setup Fonksiyonu
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  finger.begin(57600);
  lcd.init();
  lcd.backlight();

  if (!finger.verifyPassword()) {
    lcd.setCursor(0, 0);
    lcd.print("Finger error!");
    while (1);
  }

  lcd.setCursor(0, 0);
  lcd.print("KAYIT MODU");
  Serial.println("Komutlar:");
  Serial.println("A <isim> <id> → Yeni kişi ekle");
  Serial.println("L → Listele");
  Serial.println("D <isim> → Kisi sil");
  Serial.println("S → Hepsini sil");
}

// 4. Ana Loop
void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("A ")) {
      String data = input.substring(2);
      int spaceIndex = data.lastIndexOf(' ');
      if (spaceIndex == -1) {
        Serial.println("Hatalı format: A <isim> <id>");
        return;
      }

      String name = data.substring(0, spaceIndex);
      int id = data.substring(spaceIndex + 1).toInt();
      if (id <= 0 || id >= 127 || name.length() > MAX_NAME_LEN) {
        Serial.println("ID 1-126 arası, isim en fazla 10 harf");
        return;
      }
      enrollAndStore(name, id);
    }

    else if (input.equalsIgnoreCase("L")) {
      listRecords();
    }

    else if (input.equalsIgnoreCase("S")) {
      clearAllRecords();
    }

    else if (input.startsWith("D ")) {
      deleteRecord(input.substring(2));
    }

    while (Serial.available()) Serial.read();
  }
}

// === FONKSİYONLAR ===

bool isCardRegistered(byte *uid) {
  uint8_t count = EEPROM.read(0);
  int addr = 1;

  for (int i = 0; i < count; i++) {
    int nameLen = EEPROM.read(addr++); addr += nameLen;
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (EEPROM.read(addr + j) != uid[j]) {
        match = false; break;
      }
    }
    if (match) return true;
    addr += 4 + 1 + 1;
  }
  return false;
}

void enrollAndStore(String name, int fingerID) {
  lcd.clear();
  lcd.print("Parmak okut...");
  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz(1) != FINGERPRINT_OK) return;

  lcd.clear();
  lcd.print("Tekrar okut...");
  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz(2) != FINGERPRINT_OK) return;

  if (finger.createModel() != FINGERPRINT_OK) return;
  if (finger.storeModel(fingerID) != FINGERPRINT_OK) return;

  lcd.clear();
  lcd.print("Kart okut...");
  while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial());
  byte uid[4];
  for (int i = 0; i < 4; i++) uid[i] = rfid.uid.uidByte[i];

  if (isCardRegistered(uid)) {
    Serial.println("❌ Bu kart zaten kayıtlı.");
    lcd.clear(); lcd.print("Kart zaten var!");
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
    return;
  }

  Serial.println("Yetki gir (1: Kart, 2: Kart+Parmak):");
  while (!Serial.available());
  int yetki = Serial.parseInt();
  Serial.read();
  if (yetki != 1 && yetki != 2) {
    Serial.println("❌ Geçersiz yetki. Kayıt iptal.");
    return;
  }

  writeToEEPROM(name, uid, fingerID, yetki);
  Serial.print("✅ Kayıt: "); Serial.print(name);
  Serial.print(", ID: "); Serial.print(fingerID);
  Serial.print(", UID: ");
  for (int i = 0; i < 4; i++) Serial.print(uid[i], HEX), Serial.print(" ");
  Serial.print(", Yetki: "); Serial.println(yetki);

  lcd.clear(); lcd.print("Kayit OK");
  lcd.setCursor(0,1); lcd.print(name);
  delay(2000);
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
}

void writeToEEPROM(String name, byte *uid, int fingerID, int yetki) {
  uint8_t count = EEPROM.read(0);
  int addr = 1;
  for (int i = 0; i < count; i++) {
    int nameLen = EEPROM.read(addr);
    addr += 1 + nameLen + 4 + 1 + 1;
  }

  EEPROM.write(addr++, name.length());
  for (int i = 0; i < name.length(); i++) EEPROM.write(addr++, name[i]);
  for (int i = 0; i < 4; i++) EEPROM.write(addr++, uid[i]);
  EEPROM.write(addr++, fingerID);
  EEPROM.write(addr++, yetki);
  EEPROM.write(0, count + 1);
}

void listRecords() {
  uint8_t count = EEPROM.read(0);
  Serial.print("Toplam kişi: "); Serial.println(count);
  lcd.clear(); lcd.print("Toplam: "); lcd.print(count);
  delay(1500);

  int addr = 1;
  for (int i = 0; i < count; i++) {
    int nameLen = EEPROM.read(addr++);
    String name = "";
    for (int j = 0; j < nameLen; j++) name += (char)EEPROM.read(addr++);
    byte uid[4]; for (int j = 0; j < 4; j++) uid[j] = EEPROM.read(addr++);
    int fid = EEPROM.read(addr++);
    int yetki = EEPROM.read(addr++);

    Serial.print("İsim: "); Serial.print(name);
    Serial.print(", ID: "); Serial.print(fid);
    Serial.print(", UID: ");
    for (int j = 0; j < 4; j++) {
      Serial.print(uid[j], HEX);
      if (j < 3) Serial.print(" ");
    }
    Serial.print(", Yetki: ");
    Serial.println(yetki == 1 ? "Kart" : "Kart+Parmak");
  }
}

void deleteRecord(String name) {
  uint8_t count = EEPROM.read(0);
  int addr = 1;
  int foundAddr = -1;
  int nameLen = 0;

  for (int i = 0; i < count; i++) {
    nameLen = EEPROM.read(addr);
    String current = "";
    for (int j = 0; j < nameLen; j++)
      current += (char)EEPROM.read(addr + 1 + j);

    if (current == name) {
      foundAddr = addr;
      break;
    }
    addr += 1 + nameLen + 4 + 1 + 1;
  }

  if (foundAddr == -1) {
    Serial.println("İsim bulunamadı.");
    lcd.clear(); lcd.print("Bulunamadi!");
    return;
  }

  int fid = EEPROM.read(foundAddr + 1 + nameLen + 4);
  finger.deleteModel(fid);
  int total = EEPROM.read(0);
  int nextAddr = foundAddr + 1 + nameLen + 4 + 1 + 1;

  for (int i = nextAddr; i < EEPROM.length(); i++)
    EEPROM.write(i - (1 + nameLen + 4 + 1 + 1), EEPROM.read(i));

  for (int i = EEPROM.length() - (1 + nameLen + 4 + 1 + 1); i < EEPROM.length(); i++)
    EEPROM.write(i, 0);

  EEPROM.write(0, total - 1);
  Serial.print(name); Serial.println(" silindi!");
  lcd.clear(); lcd.print("Silindi: "); lcd.print(name);
  delay(1500);
}

void clearAllRecords() {
  uint8_t count = EEPROM.read(0);
  for (int i = 1; i < EEPROM.length(); i++) EEPROM.write(i, 0);
  EEPROM.write(0, 0);
  for (int i = 1; i < 127; i++) finger.deleteModel(i);
  Serial.println("Tüm kayıtlar silindi.");
  lcd.clear(); lcd.print("Hepsi silindi!");
  delay(1500);
}