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
const int ss_pin = 5;    // ESP32 pin GPIO5
const int rst_pin = 27;  // ESP32 pin GPIO27
MFRC522 rfid(ss_pin, rst_pin);

#include <WiFi.h>//Tools für den Webserver.
#include <WebServer.h>
WebServer server;

const char* ssid = "SSH-Kaffeemaschine";  //Wie soll die Kaffeemaschine heißen?
const char* password = "Abrechnung";      //Mir ist bewusst, dass das ein Passwort im Klartext ist, dies soll vom Admin geändert werden und wurde es auch im HLRS.

bool doublecoff = false;  //Variable für die Abrechnung, um zu bestimmen, ob der Nutzer einen doppelten Kaffe wollte.
String UID = "";
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

void wallet();

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
  WiFi.softAP(ssid, password);
  Serial.println("WiFi-AP starten Fertig.");
  Serial.print("http in die Kaffeemaschine unter: ");
  Serial.println(WiFi.softAPIP());
  server.on("/", wallet); //Stand in der Dokumentation
  server.begin();
  Serial.println("Webserver starten Fertig.");
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
  server.handleClient();
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
  Serial.println("Rechne ab.");
  prefs.begin("Strichliste", false);         // Nicht schreibgeschützten Namensraum öffnen
  int count = prefs.getInt(UID.c_str(), 0);  // `String` in `const char*` umwandeln
  Serial.print("Alter Stand: ");
  Serial.println(count);
  count++;           // Abrechnen
  if (doublecoff) {  // Sonderfall doppelter Kaffee/Espresso abarbeiten.
    count++;
  }
  Serial.print("Neuer Stand:");
  Serial.println(count);
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
  Google(); //Um später alle auszugeben einfach einmal alle Schlüssel abspeichern.
  authorised = true;
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.println(UID);
}

void Google() {  // Name ist ebenfalls ein Witz, weil dies die Funktion ist, welche die UIDs abspeichern soll, um später alle auflisten zu können.
  bool found = false; //Sollte die UID gefunden werden, kann ich mir die Hälfte sparen.
  prefs.begin("Strichliste", false); //Müssen irgendwo her lesen.
  int totalCount = prefs.getInt("UIDs", 0); //Wie viele UIDs haben wir bereits eingesammelt?

  for (int i = 1; i <= totalCount; i++) {//UID bereits bekannt?
    String key = String(i);
    String storedUID = prefs.getString(key.c_str(), "");
    if (storedUID == UID) {
      found = true;
      break;
    }
  }

  if (!found) {//Und wie angekündigt, einmal bitte abspeichern, wenn noch nicht vorhanden.
    totalCount++;
    prefs.putString(String(totalCount).c_str(), UID);
    prefs.putInt("UIDs", totalCount); //Wir haben jetzt eine UID mehr im System. Sollten wir uns merken.
    Serial.println("Neue UID gespeichert.");
  } else {
    Serial.println("UID existiert bereits.");
  }

  Serial.print("UIDs im System: ");
  Serial.println(totalCount);
  prefs.end(); // Und einmal Datenbank schließen.
}

void wallet() {
  prefs.begin("Strichliste", true);//Muss ich das oder den Grund wirklich nochmal erklären?
  String html = "<html><body><h1>Willkommen im Speicher der Kaffeemaschine oder so</h1><p>Dies ist das Abrechnungs-Tool.</p><h2>Liste der UIDs:</h2><ul>";
  html += listUIDs(); //Nutzer will Liste sehen
  html += "</ul></body></html>";
  server.send(200, "text/html", html);//Und so schnell es geht an den Nutzer liefern.
  prefs.end();
}

String listUIDs() { //Um nicht die HTML senden Funktion zu überlasten...
  String html = ""; //Wir lesen hier drin alle ein, nach der Gesammtzahl. Diese werden dann schön zurückgegeben.
  int totalCount = prefs.getInt("UIDs", 0);
  for (int i = 1; i <= totalCount; i++) {
    String key = String(i);
    String storedUID = prefs.getString(key.c_str(), "");
    int coffeeCount = prefs.getInt(storedUID.c_str(), 0); // Kaffeestriche auslesen
    html += "<li>UID: " + storedUID + " - Kaffeestriche: " + String(coffeeCount) + "</li>";
  }
  return html;
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