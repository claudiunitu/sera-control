
#include "DHT.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define LOOP_INTERVAL 500
#define INTAKE_FAN_MOSFET_PWM_PIN 6
#define RECIRCULATION_FAN_MOSFET_PWM_PIN 5
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

// pwm fan values
int SPEED_100 = 255;
int SPEED_75 = 227;
int SPEED_50 = 200;
int SPEED_0 = 0;

int currentSpeedRecirculationFan = SPEED_0;
unsigned long millisStateWhenRecirculationFanOn = 0;
unsigned long millisStateWhenRecirculationFanOff = 0;

bool forceHumidityEvacuationUntillTargetReached = false;
int currentSpeedIntakeFan = SPEED_0;
unsigned long millisStateWhenIntakeFanOn = 0;
unsigned long millisStateWhenIntakeFanOff = 0;

unsigned long millisStateOnLastScreensaver = 0;


// BIOME PARAMS
float BIOME_HUMIDITY_TARGET = 80;
float BIOME_HUMIDITY_TARGET_TOLERANCE = 5;


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
  if (currentSpeedRecirculationFan > 0) {
    takeMillisForm = millisStateWhenRecirculationFanOn;
    display.print("R Fan ON ");
  } else {
    takeMillisForm = millisStateWhenRecirculationFanOff;
    display.print("R Fan OFF ");
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

void initIntakeFan(void) {
  // showDebugMessageOnDisplay("INITIALIZING I FAN...");
  // delay(500);
  pinMode(INTAKE_FAN_MOSFET_PWM_PIN, OUTPUT);
	analogWrite(INTAKE_FAN_MOSFET_PWM_PIN, SPEED_0);
  // showDebugMessageOnDisplay("I FAN INITIALIZED");
  // delay(500);
}

void testIntakeFan(void) {
  showDebugMessageOnDisplay("TESTING  I FAN..");
  showDebugMessageOnDisplay("I FAN 100");
  analogWrite(INTAKE_FAN_MOSFET_PWM_PIN, SPEED_100);
  delay(1000);
  showDebugMessageOnDisplay("I FAN 75");
  analogWrite(INTAKE_FAN_MOSFET_PWM_PIN, SPEED_75);
  delay(1000);
  showDebugMessageOnDisplay("I FAN 50");
  analogWrite(INTAKE_FAN_MOSFET_PWM_PIN, SPEED_50);
  delay(1000);
  analogWrite(INTAKE_FAN_MOSFET_PWM_PIN, SPEED_0);
  showDebugMessageOnDisplay("I FAN TEST DONE");
  delay(1000);
}
void setIntakeFanSpeedDuty(int speed){
  analogWrite(INTAKE_FAN_MOSFET_PWM_PIN, speed);
}
int decideIntakeFanSpeed( int currentSpeed, 
                                  unsigned long currentMillis, 
                                  unsigned long _millisStateWhenIntakeFanOn, 
                                  unsigned long _millisStateWhenIntakeFanOff) {

  
  if (stateHumidity > BIOME_HUMIDITY_TARGET + BIOME_HUMIDITY_TARGET_TOLERANCE) {
    forceHumidityEvacuationUntillTargetReached = true;
  }

  if(forceHumidityEvacuationUntillTargetReached) {
    float humidityDifference = stateHumidity - BIOME_HUMIDITY_TARGET;
    if(humidityDifference > BIOME_HUMIDITY_TARGET_TOLERANCE*2){
      return SPEED_100;
    } else if(humidityDifference > BIOME_HUMIDITY_TARGET_TOLERANCE) {
      return SPEED_75;
    } else if(humidityDifference > 0) {
      return SPEED_50;
    } else {
      forceHumidityEvacuationUntillTargetReached = false;
      return SPEED_0;
    }
  }


  // if humidity is in check then just do maintenance if needed
  bool isIntakeFanOn = currentSpeedIntakeFan > 0;

  unsigned long MAXIMUM_MILLIS_INTAKE_FAN_ON = 2ul*60ul*1000ul; // millis to not allow fan to stay ON for more than specified value 
  unsigned long MINIMUM_MILLIS_INTAKE_FAN_ON = 10ul*1000ul; // millis to force fan to remain ON once it started

  unsigned long MAXIMUM_MILLIS_INTAKE_FAN_OFF = 15ul*60ul*1000ul;  // millis until fan ON for maintenance airflow. Will stay on until MINIMUM_MILLIS_FAN_ON
  unsigned long MINIMUM_MILLIS_INTAKE_FAN_OFF = 1ul*60ul*1000ul; // millis to force fan to stay OFF
  
  
  if(isIntakeFanOn && currentMillis - _millisStateWhenIntakeFanOn > MAXIMUM_MILLIS_INTAKE_FAN_ON) {
   return SPEED_0;
  }
  if(isIntakeFanOn && currentMillis - _millisStateWhenIntakeFanOn < MINIMUM_MILLIS_INTAKE_FAN_ON) {
    return currentSpeed;
  }

  if(!isIntakeFanOn && currentMillis - _millisStateWhenIntakeFanOff > MAXIMUM_MILLIS_INTAKE_FAN_OFF) {
    return SPEED_75;
  }
  if(!isIntakeFanOn && currentMillis - _millisStateWhenIntakeFanOff < MINIMUM_MILLIS_INTAKE_FAN_OFF) {
    return SPEED_0;
  }

  return SPEED_0;

}
void setIntakeFanState(void) {
  int prevFanSpeed = currentSpeedIntakeFan;
  currentSpeedIntakeFan = decideIntakeFanSpeed(prevFanSpeed, millis(), millisStateWhenIntakeFanOn, millisStateWhenIntakeFanOff); 
  
  if(currentSpeedIntakeFan > 0) {
    if (prevFanSpeed != currentSpeedIntakeFan) {
      millisStateWhenIntakeFanOn = millis();
    }
  } else {
    if (prevFanSpeed > 0) {
      millisStateWhenIntakeFanOff = millis();
    }
  }
  setIntakeFanSpeedDuty(currentSpeedIntakeFan);
  // Serial.print("Intake pwm: ");
  // Serial.println(currentSpeedIntakeFan);

  
}


void initRecirculationFan(void) {
  // showDebugMessageOnDisplay("INITIALIZING R FAN...");
  // delay(500);
  pinMode(RECIRCULATION_FAN_MOSFET_PWM_PIN, OUTPUT);
	analogWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, SPEED_0);
  // showDebugMessageOnDisplay("R FAN INITIALIZED");
  // delay(500);
}
void testRecirculationFan(void) {
  showDebugMessageOnDisplay("TESTING R FAN..");
  showDebugMessageOnDisplay("R FAN 100");
  analogWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, SPEED_100);
  delay(1000);
  showDebugMessageOnDisplay("R FAN 75");
  analogWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, SPEED_75);
  delay(1000);
  showDebugMessageOnDisplay("R FAN 50");
  analogWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, SPEED_50);
  delay(1000);
  analogWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, SPEED_0);
  showDebugMessageOnDisplay("R FAN TEST DONE");
  delay(1000);
}
void setRecirculationFanSpeedDuty(int speed){
  analogWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, speed);
}

int decideRecirculationFanSpeed( int currentSpeed, 
                                  unsigned long currentMillis, 
                                  unsigned long _millisStateWhenReirculationFanOn, 
                                  unsigned long _millisStateWhenRecirculationFanOff) {

  // recirculate as long as intake is on
  if(currentSpeedIntakeFan > 0) {
    return SPEED_50;
  }

  
  unsigned long MAXIMUM_MILLIS__RECIRCULATION_FAN_ON = 2ul*60ul*1000ul; // millis to not allow fan to stay ON for more than specified value (has priority)
  unsigned long MINIMUM_MILLIS__RECIRCULATION_FAN_ON = 1ul*60ul*1000ul; // millis to force fan to remain ON once it started
  
  unsigned long MAXIMUM_MILLIS_RECIRCULATION_FAN_OFF = 5ul*60ul*1000ul;  // millis until fan ON for maintenance airflow. Will stay on until MINIMUM_MILLIS_FAN_ON (has priority)
  unsigned long MINIMUM_MILLIS_RECIRCULATION_FAN_OFF = 1ul*60ul*1000ul; // millis to force fan to stay OFF
  

  bool isRecirculationFanOn = currentSpeedRecirculationFan > 0;

  if(isRecirculationFanOn && currentMillis - _millisStateWhenReirculationFanOn > MAXIMUM_MILLIS__RECIRCULATION_FAN_ON) {
    return SPEED_0;
  }
  if(isRecirculationFanOn && currentMillis - _millisStateWhenReirculationFanOn < MINIMUM_MILLIS__RECIRCULATION_FAN_ON) {
    return currentSpeed;
  }

  if(!isRecirculationFanOn && currentMillis - _millisStateWhenRecirculationFanOff > MAXIMUM_MILLIS_RECIRCULATION_FAN_OFF) {
    return SPEED_75;
  }
  if(!isRecirculationFanOn && currentMillis - _millisStateWhenRecirculationFanOff < MINIMUM_MILLIS_RECIRCULATION_FAN_OFF) {
    return SPEED_0;
  }

  return SPEED_0;

}

void setRecirculationFanState(void) {
  int prevFanSpeed = currentSpeedRecirculationFan;
  currentSpeedRecirculationFan = decideRecirculationFanSpeed(prevFanSpeed, millis(), millisStateWhenRecirculationFanOn, millisStateWhenRecirculationFanOff); 
  
  if(currentSpeedRecirculationFan > 0) {
    if (prevFanSpeed != currentSpeedRecirculationFan) {
      millisStateWhenRecirculationFanOn = millis();
    }
  } else {
    if (prevFanSpeed > 0) {
      millisStateWhenRecirculationFanOff = millis();
    }
  }
  setRecirculationFanSpeedDuty(currentSpeedRecirculationFan);
  // Serial.print("Recirculation pwm: ");
  // Serial.println(currentSpeedRecirculationFan);

  
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Display error"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  // testDisplay();

  // displayWelcome();

  initIntakeFan();
  // testIntakeFan();

  initRecirculationFan();
  // testRecirculationFan();

  

  dht.begin();
}

void loop() {

  readDHTSensor();

  setRecirculationFanState();

  setIntakeFanState();

  if (millis() - millisStateOnLastScreensaver < 120000) {
    displayState();
  } else {
    millisStateOnLastScreensaver = millis();
    screensaver();
  }




  delay(LOOP_INTERVAL);
}
