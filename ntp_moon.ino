https://github.com/jpwsutton/moonlight/blob/0963bb414962bacf8ecfe16a9f63fbea8eaedd3f/ntp_moon.ino//-------- Libraries --------
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/Wi-fi manager

#include <time.h>
#include <sys/time.h>
#include <coredecls.h>
#include <Ephemeris.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <Ticker.h>
//-------------------------------


//---------- User Defines ------------
#define TZ              0       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
FLOAT latitude = 50.941122;     // Latitude
FLOAT longitude = -1.400783;    // Longitude
int UTCOffset = +1;             // + or - hours UTC offset. (U
//------------------------------------


//---------- System  Defines ------------
// DotStar 
#define NUMPIXELS 1 // Number of LEDs in strip
#define DATAPIN    D7
#define CLOCKPIN   D5
Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);
uint32_t onColour = 0xFFFFFF;      // 'On' color
uint32_t offColour = 0x000000;      // 'Off' color

// NTP
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
timeval cbtime;      // time set in callback
boolean ntpSuccess = false;

// WiFi Setup
Ticker ticker;              // Led Flash ticker
boolean flashState = false; // WiFi Connection Flash State
uint8_t macAddr[6]; // Mac Address
char apName[11];    // Device Name
//-------------------------------

/*
   Connection Tick, will flash when in connecting state.
*/
void tick()
{
  if (flashState == false)
  {
    flashState = true;
    strip.setPixelColor(0, offColour); // 'On' pixel at head
    strip.setBrightness(100);

  }
  else
  {
    flashState = false;
    strip.setPixelColor(0, onColour); // 'On' pixel at head

  }
  strip.show();                     // Refresh strip
}


//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void time_is_set (void)
{
  gettimeofday(&cbtime, NULL);
  ntpSuccess = true;
  Serial.println("------------------ settimeofday() was called ------------------");
}



void setup()
{
  Serial.begin(115200);
  strip.begin(); // Initialize pins for output
  strip.setPixelColor(0, offColour); // 'On' pixel at head
  strip.setBrightness(100);
  strip.show();                     // Refresh strip

  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  settimeofday_cb(time_is_set);

  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);



  // Get MAC Address
  WiFi.macAddress(macAddr);

  // Prepare Device connection details.
  sprintf(apName, "moon-%02x%02x", macAddr[4], macAddr[5]);

  // Print Debug Info
  Serial.printf("[moon] - Mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  Serial.printf("[moon] - Device Name: %s\n", apName);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "moon-beef" (Last two segments of MAC address)
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(apName))
  {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("[moon] - Successfully connected to WiFi. Now attempting entering normal mode.");
  ticker.detach();

}

time_t now;


struct tm * timeinfo;

boolean moonVisible = false;
boolean lightState = false;


boolean isTheMoonVisible() {

  // - Update and Print Time.
  time(&now);
  // human readable
  Serial.print("Time:(UTC+");
  Serial.print((uint32_t)(TZ * 60 + DST_MN));
  Serial.print("mn) ");
  Serial.println(ctime(&now));

  timeinfo = localtime(&now);
  int currentHour = timeinfo->tm_hour;
  int currentMinute = timeinfo->tm_min;
  int currentDay = timeinfo->tm_mday;
  int currentMonth = timeinfo->tm_mon + 1;
  int currentYear = timeinfo->tm_year + 1900;

  // - Get MoonRise and MoonSet times for current day
  int moonRiseHour, moonRiseMinute, moonSetHour, moonSetMinute;
  FLOAT moonRiseSeconds, moonSetSeconds;

  Ephemeris::setLocationOnEarth(latitude, longitude);

  SolarSystemObject moon = Ephemeris::solarSystemObjectAtDateAndTime(EarthsMoon,
                           currentDay, currentMonth, currentYear,
                           0, 0, 0);

  // Print sunrise and sunset if available according to location on Earth
  if ( moon.riseAndSetState == RiseAndSetOk )
  {

    Ephemeris::floatingHoursToHoursMinutesSeconds(Ephemeris::floatingHoursWithUTCOffset(moon.rise, UTCOffset), &moonRiseHour, &moonRiseMinute, &moonRiseSeconds);
    Ephemeris::floatingHoursToHoursMinutesSeconds(Ephemeris::floatingHoursWithUTCOffset(moon.set, UTCOffset), &moonSetHour, &moonSetMinute, &moonSetSeconds);

    Serial.print("MoonRise: ");
    Serial.print(moonRiseHour);
    Serial.print("h");
    Serial.print(moonRiseMinute);
    Serial.print("m");
    Serial.print(moonRiseSeconds, 0);
    Serial.print("s");
    Serial.print("  MoonSet:  ");
    Serial.print(moonSetHour);
    Serial.print("h");
    Serial.print(moonSetMinute);
    Serial.print("m");
    Serial.print(moonSetSeconds, 0);
    Serial.println("s");

    if (moonRiseHour < currentHour || (moonRiseHour == currentHour && moonRiseMinute <= currentMinute)) {
      // The moon has risen
      if (currentHour < moonSetHour || (currentHour == moonSetHour && currentMinute <= moonSetMinute)) {
        // The Moon has not yet set
        Serial.println("The Moon is Visible, sets today.");
        return true;
      } else if (moonSetHour < moonRiseHour && currentHour > moonSetHour){
        // The Moon has not yet set, but will set tomorrow.
        Serial.println("The Moon is Visible, sets tomorrow.");
        return true;
      }
    }
    else if ( moon.riseAndSetState == ObjectAlwaysInSky )
    {
      Serial.println("Moon always in sky for your location.");
      return true;
    }
    return false;


  }
}

void fadeUp() {
  // Fade Up
  strip.setPixelColor(0, onColour); // 'On' pixel at head
  for (int x = 0; x < 255; x = x + 5) {
    strip.setBrightness(x);
    strip.show();
    delay(50);
  }
}

void fadeDown() {
  // Fade Down
  for (int x = 255; x > 0; x = x - 5) {
    strip.setBrightness(x);
    strip.show();
    delay(50);
  }
  strip.setPixelColor(0, offColour); // 'Off' pixel at head
}


void loop() {
  if(ntpSuccess){
    // Check to see if the moon is visible
    moonVisible = isTheMoonVisible();
    //moonVisible = true; // For testing
  
    // Update light
    if (lightState == false && moonVisible == true) {
      fadeUp();
      lightState = true;
    } else if (lightState == true && moonVisible == false) {
      fadeDown();
      lightState = false;
    }
  
    // Sleep for 1 minute
    delay(60000);
  }

}
