
#include "DHT.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define LOOP_INTERVAL 1000

#define FAN_MOSFET_PWM_PIN 5
#define DHT_PIN 2
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


float stateTemperature = 0;
float stateHumidity = 0;
float stateFeelsLike = 0;
bool isStateError = true;

byte fanSpeed = 0;
unsigned long millisStateWhenFanOn = 0;
unsigned long millisStateWhenFanOff = 0;

unsigned long millisStateOnLastScreensaver = 0;


// BIOME PARAMS
float BIOME_HUMIDITY_TARGET = 85;


// char printDigits(int digits){
//   char buffer[5];
//   if(digits < 10) {
//     sprintf(buffer, "0:%u", digits);
//   } else {
//     sprintf(buffer, "%u", digits);
//   }
  
  
  
//   return buffer;
// }

int millisToTimeFormat(unsigned long millis, int type){
  
  long day = 86400000; // 86400000 milliseconds in a day
  long hour = 3600000; // 3600000 milliseconds in an hour
  long minute = 60000; // 60000 milliseconds in a minute
  long second =  1000; // 1000 milliseconds in a second

  if(type == 0) {
    return (((millis % day) % hour) % minute) / second;
  }
  if(type == 1) {
    return ((millis % day) % hour) / minute ;
  }
  if(type == 2) {
    return (millis % day) / hour; 
  }
  if(type == 3) {
    return millis / day;
  }


}


void showDebugMessageOnDisplay(String message, bool append = false) {
  if(!append) {
    display.clearDisplay();
  }
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(SSD1306_WHITE);
  
  display.println(message);
  display.display();
}




void testDisplay(void) {
  showDebugMessageOnDisplay("DISPLAY TEST");
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  for (int i = 0; i < 256; i++) {
    display.print((char)i);
    // display.print(i);
    // display.print(" ");
  }
  display.display();

  delay(1000);
}

void displayWelcome(void) {
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 25);
  display.println(F("Hello!"));

  display.display();

  delay(1000);
}

void screensaver() {

  byte LOGO_HEIGHT = 16;
  byte LOGO_WIDTH = 16;

  byte NUMFLAKES = 10;

  byte XPOS = 0;
  byte YPOS = 1;
  byte DELTAY = 2;

  unsigned char logo_bmp[] = { 0b00000000, 0b11000000,
                               0b00000001, 0b11000000,
                               0b00000001, 0b11000000,
                               0b00000011, 0b11100000,
                               0b11110011, 0b11100000,
                               0b11111110, 0b11111000,
                               0b01111110, 0b11111111,
                               0b00110011, 0b10011111,
                               0b00011111, 0b11111100,
                               0b00001101, 0b01110000,
                               0b00011011, 0b10100000,
                               0b00111111, 0b11100000,
                               0b00111111, 0b11110000,
                               0b01111100, 0b11110000,
                               0b01110000, 0b01110000,
                               0b00000000, 0b00110000 };

  int8_t f, icons[NUMFLAKES][3];

  // Initialize 'snowflake' positions
  for (f = 0; f < NUMFLAKES; f++) {
    icons[f][XPOS] = random(1 - LOGO_WIDTH, display.width());
    icons[f][YPOS] = -LOGO_HEIGHT;
    icons[f][DELTAY] = random(1, 6);
  }

  for (int i = 0; i < 70; i++) {  //frames
    display.clearDisplay();       // Clear the display buffer

    // Draw each snowflake:
    for (f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], logo_bmp, 16, 16, SSD1306_WHITE);
    }

    display.display();
    delay(5);

    // Then update coordinates of each flake...
    for (f = 0; f < NUMFLAKES; f++) {
      icons[f][YPOS] += icons[f][DELTAY];
      // If snowflake is off the bottom of the screen...
      if (icons[f][YPOS] >= display.height()) {
        // Reinitialize to a random position, just off the top
        icons[f][XPOS] = random(1 - LOGO_WIDTH, display.width());
        icons[f][YPOS] = -LOGO_HEIGHT;
        icons[f][DELTAY] = random(1, 6);
      }
    }
  }
}

void readDHTSensor(void) {


  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  stateHumidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  stateTemperature = dht.readTemperature();
  

  // Check if any reads failed and exit early (to try again).
  if (isnan(stateHumidity) || isnan(stateTemperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    isStateError = true;
    return;
  } else {
    isStateError = false;
  }
  // Compute heat index in Celsius (isFahreheit = false)
  stateFeelsLike = dht.computeHeatIndex(stateTemperature, stateHumidity, false);
  // Check if any reads failed and exit early (to try again).
  if (isnan(stateFeelsLike)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    isStateError = true;
    return;
  } else {
    isStateError = false;
  }
}

void displayState() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);

  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);  // Draw 'inverse' text

  display.println("    SERA  CONTROL    ");
  display.setCursor(display.getCursorX(), display.getCursorY() + 2);

  if (isStateError) {
    display.println("State error!");
    return;
  }


  display.setTextColor(SSD1306_WHITE);

  String humidityDisplay = "";
  humidityDisplay += stateHumidity;
  char humiditySymbol = (char)37;
  humidityDisplay += humiditySymbol;
  humidityDisplay += " RH";


  display.println(humidityDisplay);

  String temperatureDisplay = "";
  temperatureDisplay += (String)(stateTemperature);
  char degreeSymbol = (char)9;

  temperatureDisplay += degreeSymbol;
  temperatureDisplay += "C Temp";


  display.println(temperatureDisplay);

  display.print(stateFeelsLike);
  display.print(degreeSymbol);
  display.println("C Temp Feels");


  // String fanStateDescription = "";
  unsigned long takeMillisForm;
  if (fanSpeed > 0) {
    takeMillisForm = millisStateWhenFanOn;
    display.print("Fan ON ");
  } else {
    takeMillisForm = millisStateWhenFanOff;
    display.print("Fan OFF ");
  }

  display.print(millisToTimeFormat((millis() - takeMillisForm), 3));
  display.print(":");
  display.print(millisToTimeFormat((millis() - takeMillisForm), 2));
  display.print(":");
  display.print(millisToTimeFormat((millis() - takeMillisForm), 1));
  display.print(":");
  
  display.println(millisToTimeFormat((millis() - takeMillisForm), 0));

  display.display();
}


void initFan(void) {
  showDebugMessageOnDisplay("INITIALIZING FAN...");
  delay(500);
  showDebugMessageOnDisplay("FAN INITIALIZED");
  delay(500);
}
void testFan(void) {
  showDebugMessageOnDisplay("TESTING FAN..");
  showDebugMessageOnDisplay("FAN 100%", true);
  analogWrite(FAN_MOSFET_PWM_PIN, 255);
  delay(1000);
  showDebugMessageOnDisplay("FAN 75%", true);
  analogWrite(FAN_MOSFET_PWM_PIN, (int)255*0.75);
  delay(1000);
  showDebugMessageOnDisplay("FAN 50%", true);
  analogWrite(FAN_MOSFET_PWM_PIN, (int)255*0.5);
  delay(1000);
  analogWrite(FAN_MOSFET_PWM_PIN, 0);
}
void setFanSpeedDuty(byte speed){
  analogWrite(FAN_MOSFET_PWM_PIN, speed);
}

byte decideFanSpeed(byte currentSpeed, unsigned long currentMillis, unsigned long _millisStateWhenFanOn, unsigned long _millisStateWhenFanOff) {

  

  bool isFanOn = currentSpeed > 0;

  

  byte SPEED_100 = 255;
  byte SPEED_75 = 227;
  byte SPEED_50 = 200;
  byte SPEED_0 = 0;

// TODO: this can be optimized so it does not calculate on every loop

  unsigned long MAXIMUM_MILLIS_FAN_ON = 10ul*60ul*1000ul; // millis to not allow fan to stay ON for more than specified value (has priority)
  unsigned long MINIMUM_MILLIS_FAN_ON = 1ul*60ul*1000ul; // millis to force fan to remain ON once it started
  
  unsigned long MAXIMUM_MILLIS_FAN_OFF = 5ul*60ul*1000ul;  // millis until fan ON for maintenance airflow. Will stay on until MINIMUM_MILLIS_FAN_ON (has priority)
  unsigned long MINIMUM_MILLIS_FAN_OFF = 1ul*60ul*1000ul; // millis to force fan to stay OFF
  

  if(isFanOn && currentMillis - _millisStateWhenFanOn > MAXIMUM_MILLIS_FAN_ON) {
    return SPEED_0;
  }
  if(isFanOn && currentMillis - _millisStateWhenFanOn < MINIMUM_MILLIS_FAN_ON) {
    return currentSpeed;
  }

  if(!isFanOn && currentMillis - _millisStateWhenFanOff > MAXIMUM_MILLIS_FAN_OFF) {
    return SPEED_50;
  }
  if(!isFanOn && currentMillis - _millisStateWhenFanOff < MINIMUM_MILLIS_FAN_OFF) {
    return SPEED_0;
  }

  
  if (stateHumidity > BIOME_HUMIDITY_TARGET) {
    float humidityDifference = stateHumidity - BIOME_HUMIDITY_TARGET;
    if(humidityDifference > 10){
      return SPEED_100;
    } else if(humidityDifference > 5) {
      return SPEED_75;
    } else {
      return SPEED_50;
    }
      
  } else {
    return SPEED_0;
  }


  return currentSpeed;

}
void setFanState(void) {
  byte prevFanSpeed = fanSpeed;
  fanSpeed = decideFanSpeed(prevFanSpeed, millis(), millisStateWhenFanOn, millisStateWhenFanOff); 
  
  if(fanSpeed > 0) {
    if (prevFanSpeed != fanSpeed) {
      millisStateWhenFanOn = millis();
    }
  } else {
    if (prevFanSpeed > 0) {
      millisStateWhenFanOff = millis();
    }
  }
  setFanSpeedDuty(fanSpeed);

  
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Display error"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  initFan();

  //testDisplay();
  //displayWelcome();
  // testFan();

  dht.begin();
}

void loop() {

  readDHTSensor();

  setFanState();

  if (millis() - millisStateOnLastScreensaver < 120000) {
    displayState();
  } else {
    millisStateOnLastScreensaver = millis();
    screensaver();
  }




  delay(LOOP_INTERVAL);
}
