#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <ESP8266HTTPClient.h>
#include "JsonListener.h"
#include <time.h>
#include <sys/time.h>
#include <U8g2lib.h>

#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"

/******************** Paramètres ********************/

// WIFI
const char* WiFi_Name = "WIFI";
const char* WiFi_Password = "MDP-WIFI";

// OpenWeatherMap
const boolean IS_METRIC = true;
String OPEN_WEATHER_MAP_APP_ID = "CLE-API";
String OPEN_WEATHER_MAP_LOCATION_ID = "ID-VILLE"; 
String OPEN_WEATHER_MAP_LANGUAGE = "fr";
const uint8_t MAX_FORECASTS = 8; 

OpenWeatherMapCurrentData  currentWeather;
OpenWeatherMapCurrent      currentWeatherClient;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast     forecastClient;

// OLED - DECOMENTER CE QUE VOUS UTILISE (1 à la fois !!!!!!!!!!!)
// ************* //
// ECRAN EXTERNE //
// ************* //
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); //si vous avez un ecran oled SSD1306 classique mis sur D1/D2
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ D1, /* data=*/ D2, /* reset=*/ U8X8_PIN_NONE); //si vous avez un oled SSD1306 classique mais avec SCL/SDA non mis sur D1/D2 :  remplace D1/D2 par les broches ous vous avez connecte votre ecran
// U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ D1, /* data=*/ D2, /* reset=*/ U8X8_PIN_NONE); //si vous avez un ecran classique SSD1106, modifiez D1/D2 si votre SDA/SCL ne sont pas sur D1/D2
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 12, /* data=*/ 14, /* reset=*/ U8X8_PIN_NONE); //si vous avez un esp8266 

// ************* //
// ECRAN INTERNE //
// ************* //
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 12, /* data=*/ 14, /* reset=*/ U8X8_PIN_NONE); //note : cela fonctionne pour le miens, mais vue le nombre de derivee possible, cela ne peut peut etre pas marcher

// BOUTON 
#define BUTTON_PIN D3 

// Interface & Timers (en millisecondes)
const int TIME_HOME_PAGE_MS = 10000;      // 10 sec par page d'accueil
const int TIME_FORECAST_PAGE_MS = 5000;   // 5 sec par prévision
const int MAX_FORECAST_PAGES = 5; 
const unsigned long TIME_SLEEP_DURATION = 5 * 60 * 1000UL; // 5 minutes de veille
const unsigned long TIME_DISPLAY_DURATION = 15 * 1000UL;   // 15 secondes pour l'heure seule

// Variables d'état
enum DisplayState {
  STATE_CYCLE,      // Fait défiler la météo (boot ou manuel)
  STATE_SLEEP,      // Écran éteint
  STATE_TIME_ONLY   // Affiche juste l'heure pendant 15s
};

DisplayState currentState = STATE_CYCLE;
int currentPage = 0; 
unsigned long lastPageChangeTime = 0;
unsigned long stateTimer = 0;
bool buttonLastState = HIGH; // HIGH car on utilise INPUT_PULLUP

// Temps & Maj
time_t now;
const int UPDATE_INTERVAL_SECS = 20 * 60; // 20 minutes
unsigned long timeSinceLastWUpdate = 0;

// Déclarations
void updateData();
void drawProgress(int percentage, String label);
void drawHomePage();
void drawForecastPage(int pageIndex);
void drawTimeOnlyPage();
const char* getWeatherIcon(String iconNameFromApi);

/******************** Fin des Paramètres ********************/

void setup() {
  Serial.begin(115200);

  // Configuration du bouton (broche connectée entre D3 et GND)
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  u8g2.begin();
  u8g2.enableUTF8Print(); 

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(10, 35, "Connexion WiFi...");
  u8g2.sendBuffer();

  WiFi.begin(WiFi_Name, WiFi_Password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Fuseau horaire Paris
  configTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");

  updateData();
  timeSinceLastWUpdate = millis();
  
  // Initialisation des timers pour le premier cycle
  lastPageChangeTime = millis();
  currentPage = 0;
  currentState = STATE_CYCLE;
}

void loop() {
  unsigned long currentTime = millis();

  // 1. Mise à jour de la météo en arrière-plan
  if (currentTime - timeSinceLastWUpdate > (1000L * UPDATE_INTERVAL_SECS)) {
    updateData();
    timeSinceLastWUpdate = currentTime;
  }

  // 2. Gestion du bouton
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW && buttonLastState == HIGH) {
    delay(50); // Anti-rebond simple
    if (digitalRead(BUTTON_PIN) == LOW) {
      // Réveil forcé et lancement d'un cycle complet
      u8g2.setPowerSave(0); // Allume l'écran
      currentState = STATE_CYCLE;
      currentPage = 0;
      lastPageChangeTime = currentTime;
    }
  }
  buttonLastState = buttonState;

  // 3. Machine à états pour l'affichage
  u8g2.clearBuffer();

  switch (currentState) {

    case STATE_CYCLE:
      // Défilement des pages
      unsigned long pageDuration;
      if (currentPage == 0) {
        pageDuration = TIME_HOME_PAGE_MS;
        drawHomePage();
      } else {
        pageDuration = TIME_FORECAST_PAGE_MS;
        drawForecastPage(currentPage - 1);
      }

      // Changement de page
      if (currentTime - lastPageChangeTime > pageDuration) {
        currentPage++;
        lastPageChangeTime = currentTime;
        
        // Fin du cycle -> on s'endort
        if (currentPage > MAX_FORECAST_PAGES) {
          currentState = STATE_SLEEP;
          stateTimer = currentTime;
          u8g2.setPowerSave(1); // Éteint l'écran
          u8g2.clearBuffer();   // Efface la mémoire
        }
      }
      break;

    case STATE_SLEEP:
      // Écran éteint, on attend 5 minutes
      if (currentTime - stateTimer >= TIME_SLEEP_DURATION) {
        currentState = STATE_TIME_ONLY;
        stateTimer = currentTime;
        u8g2.setPowerSave(0); // Rallume l'écran
      }
      break;

    case STATE_TIME_ONLY:
      // Affiche seulement l'heure
      drawTimeOnlyPage();
      
      // On retourne dormir au bout de 15s
      if (currentTime - stateTimer >= TIME_DISPLAY_DURATION) {
        currentState = STATE_SLEEP;
        stateTimer = currentTime;
        u8g2.setPowerSave(1);
        u8g2.clearBuffer();
      }
      break;
  }

  // On envoie le buffer seulement si l'écran n'est pas censé être en veille
  if (currentState != STATE_SLEEP) {
    u8g2.sendBuffer();
  }

  delay(50); // Petite pause pour la stabilité
}

// =================== FONCTIONS D'AFFICHAGE =================== //

void drawProgress(int percentage, String label) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(5, 15); // Dans la zone jaune
  u8g2.print(label);
  u8g2.drawRBox(10, 30, 108, 12, 3); // Dans la zone bleue
  u8g2.drawBox(12, 32, 104 * percentage / 100, 8);
  u8g2.sendBuffer();
}

void updateData() {
  // Optionnel : ne pas allumer l'écran pour la maj en arrière-plan si on est en veille
  if (currentState != STATE_SLEEP) {
    drawProgress(25, "Maj. Actuelle...");
  }
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);

  if (currentState != STATE_SLEEP) {
    drawProgress(75, "Maj. Previsions...");
  }
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);
}

void drawHomePage() {
  char buff[32];
  now = time(nullptr);
  struct tm* timeInfo = localtime(&now);

  // --- ZONE JAUNE (0 à 15px) ---
  u8g2.setFont(u8g2_font_ncenB08_tr);
  const String WDAY_NAMES[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
  const String MONTH_NAMES[] = {"Jan", "Fev", "Mar", "Avr", "Mai", "Juin", "Juil", "Aou", "Sep", "Oct", "Nov", "Dec"};
  sprintf(buff, "%s %d %s", WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, MONTH_NAMES[timeInfo->tm_mon].c_str());
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(buff)/2, 13, buff);

  // --- ZONE BLEUE (16 à 63px) ---
  // Icône météo
  u8g2.setFont(u8g2_font_open_iconic_weather_4x_t);
  const char* icon = getWeatherIcon(currentWeather.icon);
  u8g2.drawStr(5, 52, icon);

  // Heure 
  u8g2.setFont(u8g2_font_logisoso20_tr);
  sprintf(buff, "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  u8g2.drawStr(50, 40, buff);

  // Température 
  String temp = String(currentWeather.temp, 1) + "°C";
  u8g2.setFont(u8g2_font_ncenB12_tr);
  u8g2.drawStr(55, 60, temp.c_str());
}

void drawForecastPage(int pageIndex) {
  if (pageIndex >= MAX_FORECASTS) return; 

  time_t forecastTime = forecasts[pageIndex].observationTime;
  struct tm* timeInfo = localtime(&forecastTime);
  char buff[32];

  // --- ZONE JAUNE ---
  u8g2.setFont(u8g2_font_ncenB08_tr);
  sprintf(buff, "Previsions pour %02dh00", timeInfo->tm_hour);
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(buff)/2, 13, buff);

  // --- ZONE BLEUE ---
  // Icône
  u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
  const char* icon = getWeatherIcon(forecasts[pageIndex].icon);
  u8g2.drawStr(30, 42, icon);

  // Température prévue
  String temp = String(forecasts[pageIndex].temp, 0) + "°C";
  u8g2.setFont(u8g2_font_logisoso16_tr);
  u8g2.drawStr(60, 42, temp.c_str());

  // Description en bas
  String description = forecasts[pageIndex].description;
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(description.c_str())/2, 60, description.c_str());

  // Indicateur de page à droite
  for (int i=0; i < MAX_FORECAST_PAGES; i++) {
    if (i == pageIndex) {
      u8g2.drawDisc(u8g2.getDisplayWidth() - 5, 25 + i * 8, 2);
    } else {
      u8g2.drawCircle(u8g2.getDisplayWidth() - 5, 25 + i * 8, 2);
    }
  }
}

void drawTimeOnlyPage() {
  char buff[32];
  now = time(nullptr);
  struct tm* timeInfo = localtime(&now);

  // --- ZONE JAUNE ---
  u8g2.setFont(u8g2_font_ncenB08_tr);
  const String WDAY_NAMES[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
  sprintf(buff, "%s", WDAY_NAMES[timeInfo->tm_wday].c_str());
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(buff)/2, 13, buff);

  // --- ZONE BLEUE ---
  // Heure en plein centre (très gros)
  u8g2.setFont(u8g2_font_logisoso28_tr);
  sprintf(buff, "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(buff)/2, 54, buff);
}

const char* getWeatherIcon(String iconNameFromApi) {
  if (iconNameFromApi == "01d") return "A"; // soleil
  if (iconNameFromApi == "01n") return "B"; // lune
  if (iconNameFromApi == "02d") return "C"; // nuage-soleil
  if (iconNameFromApi == "02n") return "D"; // nuage-lune
  if (iconNameFromApi == "03d" || iconNameFromApi == "03n") return "E"; // nuages
  if (iconNameFromApi == "04d" || iconNameFromApi == "04n") return "F"; // nuages gris
  if (iconNameFromApi == "09d" || iconNameFromApi == "09n") return "G"; // pluie
  if (iconNameFromApi == "10d" || iconNameFromApi == "10n") return "H"; // forte pluie
  if (iconNameFromApi == "11d" || iconNameFromApi == "11n") return "I"; // orage
  if (iconNameFromApi == "13d" || iconNameFromApi == "13n") return "J"; // neige
  if (iconNameFromApi == "50d" || iconNameFromApi == "50n") return "K"; // brouillard
  return "E"; 
}
