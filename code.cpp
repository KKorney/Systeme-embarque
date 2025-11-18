
//Import des bibliothèque pour les différents composants connecté à l'Arduino
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

#define SEALEVELPRESSURE_HPA (1013.25)
enum Erreur { ERR_NONE, ERR_RTC, ERR_GPS, ERR_CAPTEUR, ERR_CAPTEUR_INCO, ERR_SD_PLEINE, ERR_SD_WRITE };//6

// Déclaration des constantes
// --- Capteurs ---
Adafruit_BME280 bme;

// --- LED RGB ---
const int redpin = 10;
const int greenpin = 11;
const int bluepin = 9;

// --- Boutons ---
const int RED_BTN_PIN = 2;
const int GREEN_BTN_PIN = 3;

// --- Lumière ---
const int LIGHT_ANALOG_PIN = A0;
float Rsensor;
int LUMIN_LOW = 255;
int LUMIN_HIGH = 768;

// --- SD card ---
#define FILE_MAX_SIZE 2048
int revision = 0;

// --- RTC ---
RTC_DS1307 rtc;

// --- Paramètres ---
unsigned long LOG_INTERVAL = 2000; // 2 secondes pour 10 min 600000
unsigned long TIMEOUT = 30000;       // 30 s
int LUMIN = 1;
int TEMP_AIR = 1;
int HYGR = 1;
int PRESSURE = 1;

// --- Modes ---
// Chaque mod possède un indice
enum Mode { MODE_STANDARD, MODE_CONFIGURATION, MODE_MAINTENANCE, MODE_ECONOMIQUE };
volatile Mode currentMode = MODE_STANDARD;
volatile Mode previousMode = MODE_STANDARD;

// --- Boutons ISR ---
// Volatile est utile pour les attachInterrupt
volatile unsigned long redPressStartMicros = 0, redLastEventMicros = 0;
volatile bool redActionPending = false;
volatile unsigned long redActionDurationMs = 0;

volatile unsigned long greenPressStartMicros = 0, greenLastEventMicros = 0;
volatile bool greenActionPending = false;
volatile unsigned long greenActionDurationMs = 0;

const unsigned long DEBOUNCE_MS = 50;
const unsigned long LONG_PRESS_MS = 5000;
const unsigned long CONFIG_PRESS_MS = 10000;

// --- Fonctions ---

// Fonction qui change la couleur RGB des LEDs
void couleur_change(int r, int g, int b){
  analogWrite(redpin, r);
  analogWrite(greenpin, g);
  analogWrite(bluepin, b);
}

// fonction des différents mods qui change de couleur en fonction des mods. 
void mode_standard()      { Serial.println(">> Mode: STANDARD"); couleur_change(0,255,0);}
void mode_configuration() { Serial.println(">> Mode: CONFIGURATION"); couleur_change(255,0,255);}
void mode_maintenance()   { Serial.println(">> Mode: MAINTENANCE"); couleur_change(255,165,0);}
void mode_economique()    { Serial.println(">> Mode: ÉCONOMIQUE"); couleur_change(0,0,255);}


/*Fonction ISR pour AttachInterrupt 
Les deux fonctions vont compter le temps au moment où le bouton est pressé
*/ 
void redISR(){
  unsigned long now = micros();
  if(now - redLastEventMicros < DEBOUNCE_MS*1000UL) return;
  redLastEventMicros = now;
  int state = digitalRead(RED_BTN_PIN);
  if(state == LOW) redPressStartMicros = now;
  else if(redPressStartMicros != 0){
    unsigned long durMs = (now - redPressStartMicros)/1000UL;
    redPressStartMicros = 0;
    redActionDurationMs = durMs;
    redActionPending = true;
  }
}

void greenISR(){
  unsigned long now = micros();
  if(now - greenLastEventMicros < DEBOUNCE_MS*1000UL) return;
  greenLastEventMicros = now;
  int state = digitalRead(GREEN_BTN_PIN);
  if(state == LOW) greenPressStartMicros = now;
  else if(greenPressStartMicros !=0){
    unsigned long durMs = (now - greenPressStartMicros)/1000UL;
    greenPressStartMicros = 0;
    greenActionDurationMs = durMs;
    greenActionPending = true;
  }
}

void indiqueErreur(Erreur e){
  switch(e){
    case ERR_RTC:
      // Rouge et jaune 1Hz
      for(int i=0;i<5;i++){
        couleur_change(0,0,255); // jaune
        delay(500);
        couleur_change(0,0,255); // rouge
        delay(500);
      }
      break;
    case ERR_GPS:
      // Rouge et verte 1Hz
      for(int i=0;i<5;i++){
        couleur_change(255,0,0); // rouge
        delay(500);
        couleur_change(255,255,0); // vert
        delay(500);
      }
      break;
    case ERR_CAPTEUR:
      // Rouge et verte 1Hz, vert deux fois plus long
      for(int i=0;i<5;i++){
        couleur_change(255,0,0); // rouge 0.5s
        delay(500);
        couleur_change(0,255,0); // vert 1s
        delay(500);
      }
      break;
    case ERR_CAPTEUR_INCO:
      // Rouge et blanche 1Hz
      for(int i=0;i<5;i++){
        couleur_change(255,0,0); // rouge
        delay(500);
        couleur_change(0,255,0); // blanc
        delay(1000);
      }
      break;
    case ERR_SD_PLEINE:
      // Rouge et blanche 1Hz, blanc 2x plus long
      for(int i=0;i<5;i++){
        couleur_change(255,0,0); // rouge 0.5s
        delay(500);
        couleur_change(255,255,255); // blanc 1s
        delay(1000);
      }
      break;
    case ERR_SD_WRITE:
      // Rouge et blanche 1Hz
      for(int i=0;i<5;i++){
        couleur_change(255,0,0); // rouge
        delay(500);
        couleur_change(255,255,255); // blanc
        delay(1000);
      }
      break;
    default:
      couleur_change(0,0,0); // aucune erreur
      break;
  }
}

// --- Entrer dans un mode ---
// Fonction permettant de définir un mod
void enterMode(Mode m){
  if(m == MODE_MAINTENANCE) previousMode = currentMode;
  currentMode = m;
  switch(currentMode){
    case MODE_STANDARD: mode_standard(); break;
    case MODE_CONFIGURATION: mode_configuration(); break;
    case MODE_MAINTENANCE: mode_maintenance(); break;
    case MODE_ECONOMIQUE: mode_economique(); break;
  }
}

// --- Gestion boutons ---
// fonction permettant de défnir comment et par quel moyen on passe d un mode a l autre
void change_mod(){
  noInterrupts();
  bool redPending = redActionPending; unsigned long redDur = redActionDurationMs; redActionPending=false;
  bool greenPending = greenActionPending; unsigned long greenDur = greenActionDurationMs; greenActionPending=false;
  interrupts();

  if(redPending){
    switch(currentMode){
      case MODE_STANDARD: if(redDur>=CONFIG_PRESS_MS) enterMode(MODE_CONFIGURATION); else if(redDur>=LONG_PRESS_MS) enterMode(MODE_MAINTENANCE); break;
      case MODE_CONFIGURATION: if(redDur>=LONG_PRESS_MS) enterMode(MODE_STANDARD); break;
      case MODE_ECONOMIQUE: if(redDur>=LONG_PRESS_MS) enterMode(MODE_MAINTENANCE); break;
      case MODE_MAINTENANCE: if(redDur>=LONG_PRESS_MS) enterMode(previousMode); break;
    }
  }

  if(greenPending){
    switch(currentMode){
      case MODE_STANDARD: if(greenDur>=LONG_PRESS_MS) enterMode(MODE_ECONOMIQUE); break;
      case MODE_CONFIGURATION: if(greenDur>=LONG_PRESS_MS) enterMode(MODE_STANDARD); break;
      case MODE_MAINTENANCE: if(greenDur>=LONG_PRESS_MS && previousMode==MODE_ECONOMIQUE) enterMode(MODE_ECONOMIQUE); break;
      case MODE_ECONOMIQUE: if(greenDur>=LONG_PRESS_MS) enterMode(MODE_STANDARD); break;
    }
  }
}

// --- Lecture capteurs ---
// permet de lire les données
String lecture_capteurs(){
  float temp=TEMP_AIR?bme.readTemperature():NAN;
  float hum=HYGR?bme.readHumidity():NAN;
  float pres=PRESSURE?bme.readPressure()/100.0F:NAN;

  int light = analogRead(LIGHT_ANALOG_PIN);
  Rsensor = (float)(1023-light)*10.0/light;
  // lecture des capteurs : Température , humidité, pression, , luminosité , résistance
  String s="";
  s+=isnan(temp)?"NA":String(temp,2); s+=",";
  s+=isnan(hum)?"NA":String(hum,2); s+=",";
  s+=isnan(pres)?"NA":String(pres,2); s+=",";
  s+=String(light); s+=",";
  s+=String(Rsensor,2);
  return s;
}

// --- Nom fichier horodaté ---
// pour créer le fichier
String fichier_courant(){
  DateTime now = rtc.now();
  char buf[13]; // 8.3 max : 8 caractères pour nom + 3 pour extension + '\0'
  sprintf(buf,"%02d%02d%02d_%d.TXT", now.year()%100, now.month(), now.day(), revision);
  return String(buf);
}
// pour écrire les données dans le fichier
void ecrire_SD(String data){
  String fname = fichier_courant();
  File f = SD.open(fname.c_str(), FILE_WRITE); // important le .c_str()
  if(f){
    f.println(data);
    f.close();
    Serial.println("Écriture OK");
  } else {
    Serial.print("Erreur ouverture fichier: "); Serial.println(fname);
  }
  //partie Archivage
  /*f.println(data);
  f.close();

  // Vérifier la taille du fichier
  File fcheck = SD.open(fname.c_str());
  if (fcheck && fcheck.size() > FILE_MAX_SIZE) {
    fcheck.close();
    revision++;
    String newFile = fichier_courant();
    if (SD.exists(newFile)) SD.remove(newFile);
    Serial.print("Fichier plein -> création fichier révision : ");
    Serial.println(newFile);
  } else if (fcheck) fcheck.close();
}*/
}


// --- Console série configuration ---
// permission du mod config
void checkSerial(){
  if(Serial.available()){
    String cmd = Serial.readStringUntil('\n'); cmd.trim();
    if(cmd.startsWith("LOG_INTERVAL=")) LOG_INTERVAL = cmd.substring(13).toInt()*60000UL;
    else if(cmd.startsWith("TIMEOUT=")) TIMEOUT = cmd.substring(8).toInt()*1000UL;
    else if(cmd.startsWith("LUMIN=")) LUMIN = cmd.substring(6).toInt();
    else if(cmd.startsWith("TEMP_AIR=")) TEMP_AIR = cmd.substring(9).toInt();
    else if(cmd.startsWith("HYGR=")) HYGR = cmd.substring(5).toInt();
    else if(cmd.startsWith("PRESSURE=")) PRESSURE = cmd.substring(9).toInt();
    else if(cmd=="RESET"){ LOG_INTERVAL=600000; TIMEOUT=30000; LUMIN=1; TEMP_AIR=1; HYGR=1; PRESSURE=1;}
    else if(cmd=="VERSION"){ Serial.println("Version 1.0 Lot 001"); }
  }
}

// --- SETUP ---
void setup(){
  // initialise les boutons
  pinMode(redpin, OUTPUT); pinMode(greenpin, OUTPUT); pinMode(bluepin, OUTPUT);
  pinMode(RED_BTN_PIN, INPUT_PULLUP); pinMode(GREEN_BTN_PIN, INPUT_PULLUP);
  Serial.begin(9600); while(!Serial);

  Serial.println("=== Système prêt ===");

  // Init BME
  if(!bme.begin(0x76)){
    Serial.println("Erreur BME280");
    indiqueErreur(ERR_CAPTEUR);
    while(1) delay(10);
  }
  // Init SD
  if(!SD.begin(4)){
    Serial.println("Erreur SD");
    indiqueErreur(ERR_SD_WRITE);
  }
  // Init RTC
  if(!rtc.begin()){
    Serial.println("Erreur RTC");
    indiqueErreur(ERR_RTC);
    while(1) delay(10);
  }

  attachInterrupt(digitalPinToInterrupt(RED_BTN_PIN), redISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(GREEN_BTN_PIN), greenISR, CHANGE);


  


  // --- Attente du choix au démarrage ---
  Serial.println("Appuyez sur le bouton VERT pour MODE STANDARD ou ROUGE pour MODE CONFIGURATION...");
  while(true){
    if(digitalRead(GREEN_BTN_PIN) == LOW){ enterMode(MODE_STANDARD); break; }
    if(digitalRead(RED_BTN_PIN) == LOW){ enterMode(MODE_CONFIGURATION); break; }
    delay(10);
  }

  Serial.println("=== Système prêt ===");
  if (!SD.begin(4)) { // initialisation de la SD avec la broche CS
        Serial.println("Erreur SD"); // si ça échoue, SD non détectée
    } else {
        Serial.println("SD initialisée !");
    }

}

// --- LOOP ---
// lance le code
unsigned long lastLog=0;
void loop(){
  //checkSerial();
  change_mod();

  unsigned long interval = LOG_INTERVAL;
  if(currentMode==MODE_ECONOMIQUE) interval*=2;

  if(currentMode==MODE_STANDARD || currentMode==MODE_ECONOMIQUE){
    if(millis()-lastLog >= interval){
      lastLog = millis();
      String data = lecture_capteurs();
      ecrire_SD(data);
      Serial.println(data);
    }
  }
  if(currentMode==MODE_MAINTENANCE){
    if(millis()-lastLog >= interval){
      lastLog = millis();
      String data = lecture_capteurs();
      Serial.println(data);
  }
  }
  if (currentMode==MODE_CONFIGURATION){
    checkSerial();
  }
  
}
