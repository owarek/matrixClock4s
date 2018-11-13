/*
  YAEC-4 (Yet Another Esp8266 Clock with 4 "segments" of dot matrix 8x8 display)

  NTP Matrix clock with WiFiManager support, daylightsaving support, LDR backlight adjustment and MQTT marquee message
*/
#include <FS.h>                  //this needs to be first, or it all crashes and burns...
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Timezone.h>            //https://github.com/JChristensen/Timezone
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>         //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>        //https://github.com/knolleary/pubsubclient
#include "TimerObject.h"         //https://playground.arduino.cc/Code/ArduinoTimerObject
#include <LEDMatrixDriver.hpp>   //https://github.com/bartoszbielawski/LEDMatrixDriver/blob/master/src/LEDMatrixDriver.hpp
#include <Time.h>

#define DEBUG_LEVEL 1 // 0,1 or 2

// values for WiFiManager setup
// define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server_adress[40] = "";
char mqtt_port[6] = "";
char mqtt_user[40] = "";
char mqtt_password[40] = "";
char mqtt_message_topic[40] = "";
char ntp_server_adress[40] = "";
char offline_mode[40] = "";
char mqtt_only_mode[40] = "";

bool MQTT_ONLY_MODE = false;

//LED matrix definition
// Define the ChipSelect pin for the led matrix (Dont use the SS or MISO pin of your Arduino!)
// Other pins are arduino specific SPI pins (MOSI=DIN of the LEDMatrix and CLK) see https://www.arduino.cc/en/Reference/SPI
const uint8_t LEDMATRIX_CS_PIN = D3;
// Define LED Matrix dimensions (0-n) - eg: 32x8 = 31x7
const int LEDMATRIX_SEGMENTS = 4;
const int LEDMATRIX_HEIGHT = 7;
const int LEDMATRIX_WIDTH = (LEDMATRIX_SEGMENTS * 8) - 1;

// TIMEZONE AND DLS SETUP
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 60};        // Central European Standard Time
Timezone CE(CEST, CET);                                     // Central European Time (PRAGUE)

// DISPLAY SPEED
const int ANIM_DELAY = 40;      // marquee animation speed
const int DOT_DELAY = 500;      // dot blink speed
//const int SWITCH_DELAY = 5000;  // switch between time and marquee speed
const int REPETITIONS = 2;      // number of marquee repetition

// This is the font definition. You can use http://gurgleapps.com/tools/matrix to create your own font or sprites.
// If you like the font feel free to use it. I created it myself and donate it to the public domain.
byte font[95][8] = { {0, 0, 0, 0, 0, 0, 0, 0}, // SPACE
  {0x10, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18}, // !
  {0x28, 0x28, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00}, // :
  {0x00, 0x0a, 0x7f, 0x14, 0x28, 0xfe, 0x50, 0x00}, // #
  {0x10, 0x38, 0x54, 0x70, 0x1c, 0x54, 0x38, 0x10}, // $
  {0x00, 0x60, 0x66, 0x08, 0x10, 0x66, 0x06, 0x00}, // %
  {0, 0, 0, 0, 0, 0, 0, 0}, // &
  {0x00, 0x10, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00}, // '
  {0x02, 0x04, 0x08, 0x08, 0x08, 0x08, 0x08, 0x04}, // (
  {0x40, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20}, // )
  {0x00, 0x10, 0x54, 0x38, 0x10, 0x38, 0x54, 0x10}, // *
  {0x00, 0x08, 0x08, 0x08, 0x7f, 0x08, 0x08, 0x08}, // +
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08}, // ,
  {0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00}, // -
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}, // .
  {0x00, 0x04, 0x04, 0x08, 0x10, 0x20, 0x40, 0x40}, // /
  {0x38, 0x44, 0x4c, 0x5c, 0x74, 0x64, 0x44, 0x38}, // 0
  {0x04, 0x0c, 0x14, 0x24, 0x04, 0x04, 0x04, 0x04}, // 1
  {0x78, 0x44, 0x04, 0x04, 0x3c, 0x40, 0x40, 0x7c}, // 2
  {0x7c, 0x44, 0x04, 0x3c, 0x04, 0x04, 0x44, 0x7c}, // 3
  {0x40, 0x40, 0x40, 0x48, 0x48, 0x7c, 0x08, 0x08}, // 4
  {0x7c, 0x40, 0x40, 0x78, 0x04, 0x04, 0x04, 0x78}, // 5
  {0x38, 0x44, 0x40, 0x78, 0x44, 0x44, 0x44, 0x38}, // 6
  {0x7c, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10}, // 7
  {0x38, 0x44, 0x44, 0x38, 0x38, 0x44, 0x44, 0x38}, // 8
  {0x38, 0x44, 0x44, 0x44, 0x3c, 0x04, 0x04, 0x38}, // 9
  {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00}, // :
  {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x08}, // ;
  {0x00, 0x10, 0x20, 0x40, 0x80, 0x40, 0x20, 0x10}, // <
  {0x00, 0x00, 0x7e, 0x00, 0x00, 0xfc, 0x00, 0x00}, // =
  {0x00, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}, // >
  {0x00, 0x38, 0x44, 0x04, 0x08, 0x10, 0x00, 0x10}, // ?
  {0x00, 0x30, 0x48, 0xba, 0xba, 0x84, 0x78, 0x00}, // @
  {0x00, 0x1c, 0x22, 0x42, 0x42, 0x7e, 0x42, 0x42}, // A
  {0x00, 0x78, 0x44, 0x44, 0x78, 0x44, 0x44, 0x7c}, // B
  {0x00, 0x3c, 0x44, 0x40, 0x40, 0x40, 0x44, 0x7c}, // C
  {0x00, 0x7c, 0x42, 0x42, 0x42, 0x42, 0x44, 0x78}, // D
  {0x00, 0x78, 0x40, 0x40, 0x70, 0x40, 0x40, 0x7c}, // E
  {0x00, 0x7c, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40}, // F
  {0x00, 0x3c, 0x40, 0x40, 0x5c, 0x44, 0x44, 0x78}, // G
  {0x00, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42}, // H
  {0x00, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x7e}, // I
  {0x00, 0x7e, 0x02, 0x02, 0x02, 0x02, 0x04, 0x38}, // J
  {0x00, 0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44}, // K
  {0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7c}, // L
  {0x00, 0x82, 0xc6, 0xaa, 0x92, 0x82, 0x82, 0x82}, // M
  {0x00, 0x42, 0x42, 0x62, 0x52, 0x4a, 0x46, 0x42}, // N
  {0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x44, 0x38}, // O
  {0x00, 0x78, 0x44, 0x44, 0x48, 0x70, 0x40, 0x40}, // P
  {0x00, 0x3c, 0x42, 0x42, 0x52, 0x4a, 0x44, 0x3a}, // Q
  {0x00, 0x78, 0x44, 0x44, 0x78, 0x50, 0x48, 0x44}, // R
  {0x00, 0x38, 0x40, 0x40, 0x38, 0x04, 0x04, 0x78}, // S
  {0x00, 0x7e, 0x90, 0x10, 0x10, 0x10, 0x10, 0x10}, // T
  {0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3e}, // U
  {0x00, 0x42, 0x42, 0x42, 0x42, 0x44, 0x28, 0x10}, // V
  {0x80, 0x82, 0x82, 0x92, 0x92, 0x92, 0x94, 0x78}, // W
  {0x00, 0x42, 0x42, 0x24, 0x18, 0x24, 0x42, 0x42}, // X
  {0x00, 0x44, 0x44, 0x28, 0x10, 0x10, 0x10, 0x10}, // Y
  {0x00, 0x7c, 0x04, 0x08, 0x7c, 0x20, 0x40, 0xfe}, // Z
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //
  {0x00, 0x1c, 0x22, 0x02, 0x02, 0x3e, 0x22, 0x3e}, // a
  {0x00, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x22, 0x3e}, // b
  {0x00, 0x1e, 0x20, 0x20, 0x20, 0x20, 0x20, 0x1e}, // c
  {0x00, 0x02, 0x02, 0x02, 0x1e, 0x22, 0x22, 0x1e}, // d
  {0x00, 0x1c, 0x22, 0x22, 0x3c, 0x20, 0x20, 0x1e}, // e
  {0x00, 0x3c, 0x24, 0x20, 0x70, 0x20, 0x20, 0x20}, // f
  {0x00, 0x1c, 0x22, 0x22, 0x22, 0x1e, 0x02, 0x1c}, // g
  {0x00, 0x20, 0x20, 0x20, 0x3c, 0x22, 0x22, 0x22}, // h
  {0x00, 0x20, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20}, // i
  {0x00, 0x04, 0x00, 0x04, 0x04, 0x04, 0x24, 0x18}, // j
  {0x00, 0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44}, // k
  {0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40}, // l
  {0x00, 0x00, 0x28, 0x54, 0x54, 0x54, 0x54, 0x54}, // m
  {0x00, 0x00, 0x20, 0x50, 0x50, 0x50, 0x50, 0x50}, // n
  {0x00, 0x30, 0x48, 0x48, 0x48, 0x48, 0x48, 0x30}, // o
  {0x00, 0x30, 0x48, 0x48, 0x70, 0x40, 0x40, 0x40}, // p
  {0x00, 0x00, 0x30, 0x48, 0x48, 0x38, 0x08, 0x08}, // q
  {0x00, 0x00, 0x30, 0x48, 0x40, 0x40, 0x40, 0x40}, // r
  {0x00, 0x18, 0x24, 0x20, 0x18, 0x04, 0x24, 0x18}, // s
  {0x00, 0x00, 0x20, 0x70, 0x20, 0x20, 0x24, 0x18}, // t
  {0x00, 0x00, 0x00, 0x24, 0x24, 0x24, 0x24, 0x18}, // u
  {0x00, 0x00, 0x00, 0x28, 0x28, 0x28, 0x28, 0x10}, // v
  {0x00, 0x00, 0x00, 0x2a, 0x2a, 0x2a, 0x2a, 0x14}, // w
  {0x00, 0x00, 0x00, 0x28, 0x28, 0x10, 0x28, 0x28}, // x
  {0x00, 0x00, 0x00, 0x28, 0x28, 0x10, 0x10, 0x10}, // y
  {0x00, 0x00, 0x00, 0x78, 0x48, 0x10, 0x28, 0x78}, // z
  // (the font does not contain any lower case letters. you can add your own.)
};    // {}, //
// OBJECT INITIALISATIONS
// The LEDMatrixDriver class instance
LEDMatrixDriver lmd(LEDMATRIX_SEGMENTS, LEDMATRIX_CS_PIN);
WiFiUDP Udp;
TimeChangeRule *tcr;
WiFiClient espClient;
PubSubClient mqtt(espClient);
// Create timer object, init with time, callback and singleshot=false
TimerObject *timerDot = new TimerObject(DOT_DELAY, &updateDot, false);
//TimerObject *timerSwitch = new TimerObject(SWITCH_DELAY, &updateSwitch, false);
TimerObject *timerMarquee = new TimerObject(ANIM_DELAY, &updateMarquee, false);

// variables
char message[100] = "";                   // recieved marquee message
int messageSize = 0;                      // size of recieved message
unsigned int localPort = 2390;            // local port to listen for UDP packets
bool shouldSaveConfig = false;            // flag for saving data
int x = 0, y = 0;                         // marquee coordinates start top left
bool dot = 0;                             // state of the dot
int repetitionsCounter = 0;               // counter for marquee repetition
bool boolDot  = false;                    // dot switch flag
bool boolSwitch  = false;                 // time/marquee switch flag
bool boolMarquee  = false;                // marquee animation flag
int h = 00;                               // actual hour
int m = 00;                               // actual minute
int s = 00;                               // actual second
char text[] = "000000";                   // display text holder
time_t prevDisplay = 0;                   // when the digital clock was displayed
const int NTP_PACKET_SIZE = 48;           // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];       // buffer to hold incoming & outgoing packets
int WiFireconnectCounter = 0;             // counter for WiFi reconnection
int MQTTreconnectCounter = 0;             // counter for MQTT reconnection
int intensity = 0;                        // intensity of display backlight
//-------------------------------------------------------------------------------------------
void setup()
{
  messageSize = 100;
  clearMessage();
  pinMode(A0, INPUT_PULLUP);
  Serial.begin(115200);
  while (!Serial) ; // Needed for Leonardo only
  delay(250);
  Serial.println("MatrixClock");

  lmd.setEnabled(true);
  lmd.setIntensity(10);   // 0 = low, 10 = high

  drawString("FS    ", LEDMATRIX_SEGMENTS, 0, 0);
  lmd.display();

  //clean FS, for testing
  //SPIFFS.format();
  //read configuration from FS json
  reconnectWiFi();

  Serial.print("port: ");
  Serial.println(Udp.localPort());
  Serial.print("NTP server: ");
  Serial.print(ntp_server_adress);
  Serial.println(" waiting for sync");

  drawString("NTP   ", LEDMATRIX_SEGMENTS, 0, 0);
  lmd.display();

  delay(1000);
  setSyncProvider(getNtpTime);
  while (timeStatus() == timeNotSet) {
    drawString("DEAD  ", LEDMATRIX_SEGMENTS, 0, 0);
    lmd.display();
    delay(5000);
    getNtpTime();
  }

  int port = atoi(mqtt_port);

  drawString("MQTT  ", LEDMATRIX_SEGMENTS, 0, 0);
  lmd.display();

  mqtt.setServer(mqtt_server_adress, port);
  mqtt.setCallback(mqttCallback);

  reconnectMQTT();

  Serial.println();
  Serial.println("1...2...3...GO");
  Serial.println("--------------");
  drawString(" GO   ", LEDMATRIX_SEGMENTS, 0, 0);
  lmd.display();
  delay(1000);

  timerDot->Start();
  timerMarquee->Start();
  pinMode(A0, INPUT);
}

void loop() {
  mqtt.loop();
  if  (!mqtt.connected() && (strcmp ("ONLINE", offline_mode) == 0)) {
    Serial.println("mqtt dead");
    drawString("OFFLINE", LEDMATRIX_SEGMENTS, 0, 0);
    lmd.display();
    reconnectMQTT();
  }
  String textStr = "";

  if (boolSwitch == true) {
    // display marquee

    intensity += 2;
    intensity = constrain(intensity, 0, 10);
    lmd.setIntensity(intensity); // 0 = low, 10 = high

    drawString("    ", LEDMATRIX_SEGMENTS, 0, 0);

    if (checkMessage()) {
      if ((repetitionsCounter < REPETITIONS) || strcmp ("TRUE", mqtt_only_mode) == 0) {
        if (boolMarquee == true) {
          boolMarquee = false;
          if (DEBUG_LEVEL > 1)
            Serial.print(".");
          drawString(message, messageSize, x, 0);
          lmd.display();
          // Advance to next coordinate
          if ( --x < messageSize * -8 ) {
            x = LEDMATRIX_WIDTH;
            repetitionsCounter++;
          }
        }
      }
      else {
        clearMessage();
        repetitionsCounter = 0;
        boolSwitch = false;
      }
    }
    else {
      boolSwitch = false;
    }
  }
  else {
    // display time
    if (timeStatus() != timeNotSet) {
      if (now() != prevDisplay) { //update the display only if time has changed
        prevDisplay = now();
        if (DEBUG_LEVEL > 0)
          digitalClockDisplay();

        h = hour();
        m = minute();
        s = second();

        if (h < 10) {
          textStr += String(0);
        }
        textStr += String(h);
        if (m < 10) {
          textStr += String(0);
        }
        textStr += String(m);
        if (s < 10) {
          textStr += String(0);
        }
        textStr += String(s);

        for (int i = 0; i < LEDMATRIX_SEGMENTS; i++) {
          text[i] = textStr[i];
        }
        drawString(text, LEDMATRIX_SEGMENTS, 0, 0);
      }
    }
    if (boolDot == true) {
      boolDot = false;
      if (DEBUG_LEVEL > 1)
        Serial.println("--DOT--");
      if (dot == 0) {
        lmd.setPixel(15, 1, true);
        lmd.setPixel(15, 2, true);
        lmd.setPixel(15, 5, true);
        lmd.setPixel(15, 6, true);
        if (LEDMATRIX_SEGMENTS > 4) {
          lmd.setPixel(31, 1, false);
          lmd.setPixel(31, 2, false);
          lmd.setPixel(31, 5, false);
          lmd.setPixel(31, 6, false);
        }
        dot = 1;
      }
      else {
        lmd.setPixel(15, 1, false);
        lmd.setPixel(15, 2, false);
        lmd.setPixel(15, 5, false);
        lmd.setPixel(15, 6, false);
        if (LEDMATRIX_SEGMENTS > 4) {
          lmd.setPixel(31, 1, true);
          lmd.setPixel(31, 2, true);
          lmd.setPixel(31, 5, true);
          lmd.setPixel(31, 6, true);
        }
        dot = 0;
      }
      intensity = map(analogRead(A0), 0, 1023, 0, 10);
      intensity = constrain(intensity, 0, 10);
      lmd.setIntensity(intensity);
      lmd.display();
    }
  }
  TimerUpdate();
}

//-------------------------------------------------------------------------------------------
//dot timer callback
void updateDot(void) {
  boolDot = true;
}

//switch timer callback
void updateSwitch(void) {
  boolSwitch = true;
  if (DEBUG_LEVEL > 1)
    Serial.println("--MARQUEE--");
}

//marquee timer callback
void updateMarquee(void) {
  boolMarquee = true;
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// callback for incomming MQTT messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String top = topic;

  if (top == mqtt_message_topic) {
    for (int i = 0; i < length; i++) {
      message[i + LEDMATRIX_SEGMENTS] = (char)payload[i];
      messageSize = length + LEDMATRIX_SEGMENTS;
    }

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]");

    Serial.print("Size [");
    Serial.print(messageSize);
    Serial.print("] ");

    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }

    Serial.println();

    updateSwitch();
  }
  delay(100);
}

// clearing of mqtt message
void clearMessage(void) {
  for (int i = 0; i < messageSize - 1; i++) {
    message[i] = ' ';
  }
  messageSize = 0;
}

// checking if there is some mqtt message to marquee
bool checkMessage(void) {
  //  for (int i = 0; i < 19; i++) {
  //    if (message[i] != ' ')
  //      return true;
  //  }
  //  return false;
  if (messageSize > 0) {
    return true;
  }
  return false;
}

// formating and printing actual time to Serial
void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

// formating of time
void printDigits(int digits) {
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// NTP time getter
time_t getNtpTime() {
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.print("Transmit NTP Request ");
  sendNTPpacket();
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.print("NTP OK: Receive NTP Response ");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      time_t utc = secsSince1900 - 2208988800UL;
      printDateTime(utc);
      Serial.print(" ---day light saving---> ");
      printDateTime(CE.toLocal(utc, &tcr));
      Serial.println();
      return CE.toLocal(utc, &tcr);
    }
  }
  Serial.println("NOK: No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(void) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(ntp_server_adress, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// draw string to matrix display buffer
void drawString(char* text, int len, int x, int y ) {
  for ( int idx = 0; idx < len; idx ++ )
  {
    int c = text[idx] - 32;

    // stop if char is outside visible area
    if ( x + idx * 8  > LEDMATRIX_WIDTH )
      return;

    // only draw if char is visible
    if ( 8 + x + idx * 8 > 0 )
      drawSprite( font[c], x + idx * 8, y, 8, 8 );
  }
}

// draw sprite to matrix display buffer
void drawSprite( byte* sprite, int x, int y, int width, int height ) {
  // The mask is used to get the column bit from the sprite row
  byte mask = B10000000;

  for ( int iy = 0; iy < height; iy++ )
  {
    for ( int ix = 0; ix < width; ix++ )
    {
      lmd.setPixel(x + ix, y + iy, (bool)(sprite[iy] & mask ));

      // shift the mask by one pixel to the right
      mask = mask >> 1;
    }

    // reset column mask
    mask = B10000000;
  }
}

// update all timer objects
void TimerUpdate(void) {
  timerDot->Update();
  timerMarquee->Update();
}

// reconnect to WiFi
void reconnectWiFi(void) {
  if (DEBUG_LEVEL > 0)
    Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    if (DEBUG_LEVEL > 0)
      Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      if (DEBUG_LEVEL > 0)
        Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        if (DEBUG_LEVEL > 0)
          Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        if (DEBUG_LEVEL > 1) {
          json.printTo(Serial);
          Serial.println();
        }
        if (json.success()) {
          if (DEBUG_LEVEL > 0)
            Serial.println("parsed json");
          drawString("FSOK  ", LEDMATRIX_SEGMENTS, 0, 0);
          lmd.display();

          strcpy(mqtt_server_adress, json["mqtt_server_adress"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(mqtt_message_topic, json["mqtt_message_topic"]);
          strcpy(ntp_server_adress, json["ntp_server_adress"]);
          strcpy(offline_mode, json["offline_mode"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  drawString("WIFI  ", LEDMATRIX_SEGMENTS, 0, 0);
  lmd.display();

  WiFiManagerParameter custom_mqtt_server_adress("mqtt_server_adress", "mqtt server address", mqtt_server_adress, 40);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt server port", mqtt_port, 5);
  WiFiManagerParameter custom_mqtt_user("mqtt_user", "mqtt server username", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_password("mqtt_password", "mqtt server password", mqtt_password, 40);
  WiFiManagerParameter custom_mqtt_message_topic("mqtt_message_topic", "mqtt message topic", mqtt_message_topic, 40);
  WiFiManagerParameter custom_ntp_server_adress("ntp_server_adress", "ntp server address", ntp_server_adress, 40);
  WiFiManagerParameter custom_offline_mode("offline_mode", "OFFLINE or ONLINE mode", offline_mode, 40);
  WiFiManagerParameter custom_mqtt_only_mode("mqtt_only_mode", "TRUE or FALSE mqtt only mode", mqtt_only_mode, 40);

  WiFiManager wifiManager;

  wifiManager.resetSettings();

  if (DEBUG_LEVEL < 1) {
    wifiManager.setDebugOutput(false);
  }

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server_adress);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_message_topic);
  wifiManager.addParameter(&custom_ntp_server_adress);
  wifiManager.addParameter(&custom_offline_mode);
  wifiManager.addParameter(&custom_mqtt_only_mode);

  wifiManager.autoConnect("MatrixClock");

  //read updated parameters
  strcpy(mqtt_server_adress, custom_mqtt_server_adress.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_message_topic, custom_mqtt_message_topic.getValue());
  strcpy(ntp_server_adress, custom_ntp_server_adress.getValue());
  strcpy(offline_mode, custom_offline_mode.getValue());
  strcpy(mqtt_only_mode, custom_mqtt_only_mode.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server_adress"] = mqtt_server_adress;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    json["mqtt_message_topic"] = mqtt_message_topic;
    json["ntp_server_adress"] = ntp_server_adress;
    json["offline_mode"] = offline_mode;
    json["mqtt_only_mode"] = mqtt_only_mode;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println();
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.print("WiFi connected ");

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.print("Starting UDP, ");
  Udp.begin(localPort);

  delay(500);
  WiFireconnectCounter++;
}

// reconnect to MQTT
void reconnectMQTT(void) {
  // Loop until we're reconnected
  if (strcmp ("ONLINE", offline_mode) == 0) {
    Serial.print("MQTT-Connecting to: ");
    Serial.print(mqtt_server_adress);
    Serial.print(":");
    int port = atoi(mqtt_port);
    Serial.print(port);
    while (!mqtt.connected()) {
      Serial.print(" _*_");
      String client_ID = "ESP-";
      client_ID += long(ESP.getChipId());
      char msg[20];
      client_ID.toCharArray(msg, client_ID.length() + 1);
      if (mqtt.connect(msg, mqtt_user, mqtt_password)) {

        mqtt.subscribe(mqtt_message_topic);
      }
      delay(500);
      MQTTreconnectCounter ++;
    }
    Serial.print(" connected");
    if (MQTTreconnectCounter > (10000 / 500)) {
      reconnectWiFi();
    }
    if (WiFireconnectCounter > (10000 / 500)) {
      ESP.restart();
    }
  }
  else {
    Serial.print("MQTT disabled");
  }
}

void printDateTime(time_t t)
{
  Serial.print(hour(t));
  printDigits(minute(t));
  printDigits(second(t));

  Serial.print(" ");
  Serial.print(dayShortStr(weekday(t)));
  Serial.print("/");
  Serial.print(day(t));
  Serial.print("/");
  Serial.print(monthShortStr(month(t)));
  Serial.print("/");
  Serial.print(year(t));
}
