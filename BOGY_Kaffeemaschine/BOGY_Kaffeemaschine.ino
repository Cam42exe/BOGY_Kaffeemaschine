/* Credits: Simon // Cam42exe
Graphviz veranschaulichung des Codes als Finite State Machine. Der folgende Code zeigt meine eigene Implementation.
digraph G {
waiting -> authorised [color = "blue" fontcolor = "blue" label = "show token"]
authorised -> brewing [color = "blue" fontcolor = "blue" label = "press button\n for coffee"]
brewing -> waiting [color = "blue" fontcolor = "blue" label = "waiting 30sec,\n then pay"]
start -> waiting [color = "green" fontcolor = "green" label = "machine on?"]
authorised -> waiting [color = "orange" fontcolor = "orange" label = "timeout \n10sec"]
authorised -> waiting [color = "red" fontcolor = "red" label = "cancel"]
brewing -> waiting [color = "red" fontcolor = "red" label = "cancel"]
}
*/

#include <Preferences.h>  //Bibliothek zum finalen Abspeichern
Preferences prefs;

#include <SPI.h>  //Config für RFID Reader vom Typ RC522
#include <MFRC522.h>
const int ss_pin 5;    // ESP32 pin GPIO5
const int rst_pin 27;  // ESP32 pin GPIO27
MFRC522 rfid(ss_pin, rst_pin);


bool doublecoff = false;  //Variable für die Abrechnung, um zu bestimmen, ob der Nutzer einen doppelten Kaffe wollte.
String UID;
const int but_c = 26;      //Cancel Knopf
const int but_scoff = 25;  //Knopf für einen einfachen Kaffee
const int but_dcoff = 33;  //Knopf für einen doppelten Kaffee
const int but_sespr = 32;  //Knopf für einen einfachen Espresso
const int but_despr = 14;  //Knopf für einen doppelten Espresso

const int authorised_timeout = 10000;  //Zeit in ms bis man nicht mehr Autorisiert ist.
const int finish_transaction = 30000;  //Zeit in ms bis der Kaufvertrag mit der Kaffeemaschine geschlossen wird.

bool cancelrequest = false;
bool authorised = false;
int coffeenumber = 0;   //Welcher Kaffee gemacht werden soll. 1 = Einfach Kaffee; 2 = Doppelter Kaffee;
int requestcoffee = 0;  //Um Mehrfacheingaben zu verhindern. 3 = Einfacher Espresso; 4 = Doppelter Espresso
/*
AN:01 Anschalten
AN:02 Ausschalten
FA:04 1Kaffee
FA:05 2Kaffee
FA:06 1Espresso
FA:07 2Espresso
*/

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

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
  /*  getUID();                       //Auskommentieren für Debug von Knöpfen
  if (requestcoffee > 0) {
    Serial.print("Kaffeenummer: ");
    Serial.println(requestcoffee);
    requestcoffee = 0;
  }*/
  /*if (UID.length() > 0) {
  Serial.print("KartenID: ");     //Auskommentieren für Debug von Karten
  Serial.println(UID);
  }*/
  if (authorised) {
    requestcoffee = 0;  //Auskommentieren, wenn keine Explizierte Reihenfolge gefragt ist.
    pressCoffee();
  }
}

void pressCoffee() {  //Name ist ein Insider-Joke, mir ist kein besserer Name für die Funktion eingefallen.
  int wait_till_timeout = 0;
  while (requestcoffee < 1) {
    if (wait_till_timeout > authorised_timeout) {
      reset();
      return;
    }
    if (cancelrequest) {
      reset();
      return;
    }
    wait_till_timeout++;
    delay(1);
  }
  coffeenumber = requestcoffee;
  makeCoffee();
}

void makeCoffee() {         //Um den Insider Joke fortzusetzen, ein einigermaßen sinnvoller Name um der Maschine zu sagen, mach mal! Ähh Bitte. SOFORT!
  if (coffeenumber == 1) {  //Über den Levelshifter namens Arduino Uno als Umweg den richtigen Befehl an die Maschine senden.
    Serial.println("FA:04");
  } else if (coffeenumber == 2) {
    Serial.println("FA:05");
  } else if (coffeenumber == 3) {
    Serial.println("FA:06");
  } else if (coffeenumber == 4) {
    Serial.println("FA:07");
  }
  checkout();
}

void checkout() {
  int pay_up_wait = 0;
  while (finish_transaction > pay_up_wait) {  // 30 Sek. warten bis Kaufvertrag schließen.
    if (cancelrequest) {
      reset();
      return;
    }
    delay(1);
    pay_up_wait++;
  }
  prefs.begin("Strichliste", false);         // Nicht schreibgeschützten Namensraum öffnen
  int count = prefs.getInt(UID.c_str(), 0);  // `String` in `const char*` umwandeln
  count++;                                   // Abrechnen
  if (doublecoff) {                      // Sonderfall doppelter Kaffee/Espresso abarbeiten.
    count++;
  }
  prefs.putInt(UID.c_str(), count);  // Wert schreiben
  prefs.end();                       // Und finales Abspeichern, um eine korrekte Zählung zu ermöglichen.
}

void reset() {
  UID.clear();         //Um falsche Abrechnungen zu vermeiden einfach einmal den String leeren.
  authorised = false;  //Und einmal noch den Zustand zurücksetzen.
  requestcoffee = 0;
  coffeenumber = 0;
  cancelrequest = false;
}

void getUID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }  // UID in einer Variablen speichern
  UID.clear();
  for (int i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) UID += "0";
    UID += String(rfid.uid.uidByte[i], HEX);
  }
  UID.trim();  //String verkürzen
  authorised = true;
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.println(UID);
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