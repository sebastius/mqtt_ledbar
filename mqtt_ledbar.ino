

/*
  
*/

#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>

#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient/releases/tag/2.4

#include "FastLED.h"
FASTLED_USING_NAMESPACE
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

void onMqttMessage(char* topic, byte * payload, unsigned int length);
boolean reconnect();
void mqtt_publish (String topic, String message);
void rainbow();
void juggle();
void confetti();
void nextPattern();

#define DATA_PIN    3
#define CLK_PIN   4
#define LED_TYPE    APA102
#define COLOR_ORDER BGR
#define NUM_LEDS    214
CRGB leds[NUM_LEDS];

#define BRIGHTNESS         255
#define FRAMES_PER_SECOND  10
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

bool spacestate = LOW;
bool klok_ok = false;
bool switchstate;
bool zapgevaar = LOW;
bool iemand_kijkt = LOW;


// WiFi settings
char ssid[] = "revspace-pub-2.4ghz";  //  your network SSID (name)
char pass[] = "";       // your network password

// MQTT Server settings and preparations
const char* mqtt_server = "mosquitto.space.revspace.nl";
WiFiClient espClient;

PubSubClient client(mqtt_server, 1883, onMqttMessage, espClient);
long lastReconnectAttempt = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { rainbow, confetti, juggle };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current


boolean reconnect() {
  if (client.connect("Ledbar069")) {
    // Once connected, publish an announcement...
    client.publish("sebastius/ledbar/01", "hello world");
    // ... and resubscribe
    client.subscribe("revspace/state");
    client.loop();
    client.subscribe("revspace/button/nomz");
    client.loop();
    client.subscribe("revspace/button/doorbell");
    client.loop();
    client.subscribe("revspace/sensors/co2");
    client.loop();
    client.subscribe("revspace/sensors/humidity");
    client.loop();
    client.subscribe("revspace/lights/ledbar");
    client.loop();
    client.subscribe("revspace/cams");
    client.loop();


  }
  return client.connected();
}


void onMqttMessage(char* topic, byte * payload, unsigned int length) {
  uint16_t spaceCnt;
  uint8_t numCnt = 0;
  char bericht[50] = "";

  Serial.print("received topic: ");
  Serial.println(topic);
  Serial.print("length: ");
  Serial.println(length);
  Serial.print("payload: ");
  for (uint8_t pos = 0; pos < length; pos++) {
    bericht[pos] = payload[pos];
  }
  Serial.println(bericht);
  Serial.println();

  // Lets select a topic/payload handler
  // Some topics (buttons for example) don't need a specific payload handled, just a reaction to the topic. Saves a lot of time!

  if (strcmp(topic, "revspace/lights/ledbar") == 0) {
    gCurrentPatternNumber = atoi(bericht) % ARRAY_SIZE( gPatterns);
  }

  // Space State
  if (strcmp(topic, "revspace/state") == 0) {
    for (uint8_t pos = 0; pos < length; pos++) {
      bericht[pos] = payload[pos];
    }

    if (strcmp(bericht, "open") == 0) {
      Serial.println("Revspace is open");
      if (spacestate == LOW) {
        spacestate = HIGH;
      }
    } else {
      // If it ain't open, it's closed! (saves a lot of time doing strcmp).
      Serial.println("Revspace is dicht");
      if (spacestate == HIGH) {
        spacestate = LOW;

      }
    }
  }

  if (strcmp(topic, "revspace/cams") == 0) {
    Serial.print("cams:");
    Serial.println(bericht[8]);

    uint8_t spatieteller = 4;
    uint8_t positie = 0;
    
    while (spatieteller) if (bericht[positie++] == ' ') spatieteller--;
    
    Serial.print("positie");
    Serial.println(bericht[positie], DEC);
    iemand_kijkt = (bericht[positie] != '0');
  }


  // NOMZ because we are hungry! Lets join the blinking lights parade!
  if (strcmp(topic, "revspace/button/nomz") == 0) {
    for (uint8_t tel = 0; tel < 150; tel++) {
      Serial.println("NOMZ!");

      for (uint8_t vuller = 0; vuller < NUM_LEDS; vuller++) {
        leds[vuller] = CRGB::Orange;
      }
      FastLED.show();
      delay(150);

      for (uint8_t vuller = 0; vuller < NUM_LEDS; vuller++) {
        leds[vuller] = CRGB::Blue;
      }
      FastLED.show();
      delay(100);
    }

  }

  // DOORBELL
  if (strcmp(topic, "revspace/button/doorbell") == 0) {
    for (uint8_t tel = 0; tel < 10; tel++) {
      Serial.println("Doorbell!");

      for (uint8_t vuller = 0; vuller < NUM_LEDS; vuller++) {
        leds[vuller] = CRGB::Yellow;
      }
      FastLED.show();
      delay(1000);

      for (uint8_t vuller = 0; vuller < NUM_LEDS; vuller++) {
        leds[vuller] = CRGB::Black;
      }
      FastLED.show();
      delay(1000);
    }
  }

  // CO2 measurements and alerts. Set to Revspace default like Ledbanner
  if (strcmp(topic, "revspace/sensors/co2") == 0) {
    char num[4] = "";
    spaceCnt = 0;
    numCnt = 0;
    uint16_t waarde = 0;

    while (((uint8_t)payload[spaceCnt] != 32) && (spaceCnt <= length) && (numCnt < 4)) {
      num[numCnt] = payload[spaceCnt];
      numCnt++;
      spaceCnt++;
    }
    if (numCnt > 0) {
      waarde = atoi(&num[0]);
    }
    Serial.print("co2: ");
    Serial.print(waarde);
    Serial.println(" PPM");

    if (waarde > 1600) {
      for (uint8_t tel = 0; tel < 10; tel++) {
        Serial.println("CO2 Danger");
        for (uint8_t vuller = 0; vuller < NUM_LEDS; vuller++) {
          leds[vuller] = CRGB::Red;
        }
        FastLED.show();
        delay(500);
        for (uint8_t vuller = 0; vuller < NUM_LEDS; vuller++) {
          leds[vuller] = CRGB::Black;
        }
        FastLED.show();
        delay(50);
      }
    }
  }

  // Humidity measurements and alerts. Set to Revspace default like Ledbanner
  if (strcmp(topic, "revspace/sensors/humidity") == 0) {
    char num[2] = "";
    spaceCnt = 0;
    numCnt = 0;
    uint16_t waarde = 0;

    while (((uint8_t)payload[spaceCnt] != 46) && (spaceCnt <= length) && (numCnt < 2)) {
      num[numCnt] = payload[spaceCnt];
      numCnt++;
      spaceCnt++;
    }
    if (numCnt > 0) {
      waarde = atoi(&num[0]);
    }
    Serial.print("Humidity: ");
    Serial.print(waarde);
    Serial.println("");
    if (waarde < 32) {
      Serial.println("Zapgevaar");
      zapgevaar = HIGH;
    } else {
      zapgevaar = LOW;
    }
  }

  Serial.println();
}

void mqtt_publish (String topic, String message) {
  Serial.println();
  Serial.print("Publishing ");
  Serial.print(message);
  Serial.print(" to ");
  Serial.println(topic);

  char t[100], m[100];
  topic.toCharArray(t, sizeof t);
  message.toCharArray(m, sizeof m);

  client.publish(t, m, /*retain*/ 0);
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void fadeall() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(250);
  }
}

void cylon() {
  static uint8_t hue = 255;
  Serial.print("x");
  // First slide the led in one direction
  for (int i = 0; i < NUM_LEDS; i++) {
    // Set the i'th led to red
    leds[i] = CRGB::Green;
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }
  Serial.print("x");

  // Now go in the other direction.
  for (int i = (NUM_LEDS) - 1; i >= 0; i--) {
    // Set the i'th led to red
    leds[i] = CRGB::Green;
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }
}

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}



void setup()
{
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  Serial.println();
  Serial.println(ESP.getChipId());
  pinMode(4, INPUT_PULLUP);
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<APA102, DATA_PIN, CLK_PIN, BGR>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");

  }
  Serial.println("");

  Serial.print("WiFi connected");
  Serial.println(ssid);

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void loop()
{
  if (!client.connected()) {
    Serial.println(".");
    long verstreken = millis();
    if (verstreken - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = verstreken;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }

  if (spacestate == HIGH) {
    gPatterns[gCurrentPatternNumber]();
    if (zapgevaar == HIGH) {
      addGlitter(80);
    }
  } else {
    {
      // FastLED's built-in rainbow generator
      fadeToBlackBy( leds, NUM_LEDS, 20);
      fill_rainbow( leds, 1, gHue, 7);
    }
  }

  if (iemand_kijkt) {
    fill_solid( leds, NUM_LEDS, CRGB::Red);
  }

  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
  EVERY_N_MILLISECONDS( 20 ) {
    gHue++;  // slowly cycle the "base color" through the rainbow
  }
  EVERY_N_SECONDS( 30 ) {
    nextPattern();  // change patterns periodically
  }

}

