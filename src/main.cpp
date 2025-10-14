/**************************************************************************/
/*! 
  NFC + MQTT Example:
  - Leest PN532 NFC tags via I2C
  - Publiceert de UID in hex naar MQTT topic "tag"
*/
/**************************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <PubSubClient.h>

// -------------------- WiFi & MQTT --------------------
#define SSID          "IB3"
#define PWD           "ingenieursbeleving3"

#define MQTT_SERVER   "dramco.local"
#define MQTT_PORT     1883

WiFiClient espClient;
PubSubClient client(espClient);

// -------------------- PN532 NFC --------------------
#define PN532_IRQ   4
#define PN532_RESET 5 

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

const int DELAY_BETWEEN_CARDS = 500;
long timeLastCardRead = 0;
boolean readerDisabled = false;
int irqCurr;
int irqPrev;

// -------------------- Function Declarations --------------------
static void startListeningToNFC();
static void handleCardDetected();
void setup_wifi();
void reconnect();

// -------------------- Setup --------------------
void setup(void) {
  Serial.begin(115200);
  Serial.println("Begin NFC532 Scanning Software.");

  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 board");
    while (1);
  }

  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  nfc.SAMConfig();
  startListeningToNFC();
}

// -------------------- WiFi Setup --------------------
void setup_wifi() {
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.begin(SSID, PWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

// -------------------- MQTT Reconnect --------------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32NFCClient")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" — retrying in 5 seconds");
      delay(5000);
    }
  }
}

// -------------------- Main Loop --------------------
void loop(void) {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (readerDisabled) {
    if (millis() - timeLastCardRead > DELAY_BETWEEN_CARDS) {
      readerDisabled = false;
      startListeningToNFC();
    }
  } else {
    irqCurr = digitalRead(PN532_IRQ);
    if (irqCurr == LOW && irqPrev == HIGH) {
      handleCardDetected();
    }
    irqPrev = irqCurr;
  }
}

// -------------------- NFC Handling --------------------
void startListeningToNFC() {
  irqPrev = irqCurr = HIGH;
  Serial.println("Present an ISO14443A Card ...");
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

void handleCardDetected() {
  uint8_t success = false;
  uint8_t uid[7] = {0};
  uint8_t uidLength;

  success = nfc.readDetectedPassiveTargetID(uid, &uidLength);
  Serial.println(success ? "Read successful" : "Read failed (not a card?)");

  if (success) {
    Serial.print("Card ID HEX Value: ");
    nfc.PrintHex(uid, uidLength);

    // --- Bouw hex string ---
    String hexString = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) hexString += "0";
      hexString += String(uid[i], HEX);
      if (i < uidLength - 1) hexString += " ";
    }
    hexString.toUpperCase();

    Serial.print("Publishing tag: ");
    Serial.println(hexString);

    // --- Publish naar MQTT ---
    client.publish("tag", hexString.c_str());

    timeLastCardRead = millis();
  }

  readerDisabled = true;
}
