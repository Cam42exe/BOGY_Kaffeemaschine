#include <SPI.h>  //Config für RFID Reader vom Typ RC522
#include <MFRC522.h>
#define SS_PIN 5    // ESP32 pin GPIO5
#define RST_PIN 27  // ESP32 pin GPIO27
MFRC522 rfid(SS_PIN, RST_PIN);

String UID = "";
const int but_c = 26;      //Cancel Knopf
const int but_scoff = 25;  //Knopf für einen einfachen Kaffee
const int but_dcoff = 33;  //Knopf für einen doppelten Kaffee
const int but_sespr = 32;  //Knopf für einen einfachen Espresso
const int but_despr = 14;  //Knopf für einen doppelten Espresso

int coffeenumber = 0;   //Welcher Kaffee gemacht werden soll. 1 = Einfach Kaffee; 2 = Doppelter Kaffee;
int requestcoffee = 0;  //Um Mehrfacheingaben zu verhindern. 3 = Einfacher Espresso; 4 = Doppelter Espresso

void setup() {
  Serial.begin(115200);

  Serial.print("Initialisiere RFID Reader");
  SPI.begin();      // init SPI bus
  rfid.PCD_Init();  // init MFRC522
  Serial.println(" Fertig.");

  Serial.print("Knöpfe werden an Interrups gebunden");
  pinMode(but_c, INPUT_PULLUP);  //Pindefinierungen von oben als Input festlegen
  pinMode(but_scoff, INPUT_PULLUP);
  pinMode(but_dcoff, INPUT_PULLUP);
  pinMode(but_sespr, INPUT_PULLUP);
  pinMode(but_despr, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(but_c), cancel, FALLING);
  attachInterrupt(digitalPinToInterrupt(but_scoff), scoff, FALLING);
  attachInterrupt(digitalPinToInterrupt(but_dcoff), dcoff, FALLING);
  attachInterrupt(digitalPinToInterrupt(but_sespr), sespr, FALLING);
  attachInterrupt(digitalPinToInterrupt(but_despr), despr, FALLING);
  Serial.println(" Fertig.");
}

void loop() {
  getUID();
  if (requestcoffee > 0) {
    Serial.print("Kaffeenummer: ");
    Serial.println(requestcoffee);
    requestcoffee = 0;
  }
  if (UID.length() > 0) {
    Serial.print("KartenID: ");
    Serial.println(UID);
    UID.clear();
  }
}

void getUID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }  // UID in einer Variablen speichern
for (int i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) UID += "0";
    UID += String(rfid.uid.uidByte[i], HEX);
}
  UID.trim();  // Trim leading/trailing whitespaces
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

//ISRs für Userinputs
void cancel() {
  cancelrequest = true;
}

void scoff() {
  doublecoff = false;
  requestcoffee = 1;
}

void dcoff() {
  doublecoff = true;
  requestcoffee = 2;
}
void sespr() {
  doublecoff = false;
  requestcoffee = 3;
}
void despr() {
  doublecoff = true;
  requestcoffee = 4;
}