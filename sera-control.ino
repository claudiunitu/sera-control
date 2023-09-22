
#include "DHT.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define LOOP_INTERVAL 2000
#define INTAKE_FAN_MOSFET_PWM_PIN 11
#define RECIRCULATION_FAN_MOSFET_PWM_PIN 10
#define HUMIDIFIER_MOSFET_PIN 3
#define BUTTON_TOGGLE_PASSIVE_HUMIDIFIER 4
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

// pwm fan values (for approx 500Hz when using 5v mini fans)
byte FAN_MIN_PWM_DUTY = 90;
byte FAN_MAX_PWM_DUTY = 255;
byte FAN_MIN_SPEED = 0;
byte FAN_MAX_SPEED = 10;

byte getFanPwmDuty(byte speed) {
  if(speed > FAN_MAX_SPEED){
    return FAN_MAX_SPEED;
  }
  if(speed < 1){
    return 0;
  }
  return (speed * (FAN_MIN_PWM_DUTY / FAN_MAX_SPEED)) + FAN_MIN_PWM_DUTY;
}

int currentSpeedRecirculationFan = getFanPwmDuty(0);
unsigned long millisOnStateChangeRecirculationFan = 0;
// unsigned long millisStateWhenRecirculationFanOff = 0;

bool forceHumidityEvacuationUntillTargetReached = false;
int currentSpeedIntakeFan = getFanPwmDuty(0);
unsigned long millisOnStateChangeIntakeFan = 0;
// unsigned long millisStateWhenIntakeFanOff = 0;

bool forceHumidityIncreaseUntillTargetReached = false;
bool isHumidifierOn = false;
unsigned long millisOnStateChangeHumidifier = 0;
unsigned long millisWhenLastHumidificationEnded = 0;

bool isPasiveHumidifierOn = false;
unsigned long millisOnStateChangePasiveHumidifier = 0;


unsigned long millisStateOnLastScreensaver = 0;



// BIOME PARAMS
float BIOME_HUMIDITY_TARGET = 88;
float BIOME_HUMIDITY_TARGET_TOLERANCE = 2;
unsigned long BIOME_FORCE_PASIVE_HUMIDIFIER_CYCLE_ON_MILLIS = 14ul*60ul*60ul*1000ul;
unsigned long BIOME_FORCE_PASIVE_HUMIDIFIER_CYCLE_OFF_MILLIS = 14ul*60ul*60ul*1000ul;



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

  // display.println("    SERA  CONTROL    ");
  // display.setCursor(display.getCursorX(), display.getCursorY() + 2);

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
  humidityDisplay += " (";
  humidityDisplay += BIOME_HUMIDITY_TARGET;
  humidityDisplay += ")";


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


  // recirculation info
  if (currentSpeedRecirculationFan > 0) {
    display.print("R Fan 1 ");
  } else {
    display.print("R Fan 0 ");
  }
  display.print(millisToTimeFormat((millis() - millisOnStateChangeRecirculationFan), 3));
  display.print(":");
  display.print(millisToTimeFormat((millis() - millisOnStateChangeRecirculationFan), 2));
  display.print(":");
  display.print(millisToTimeFormat((millis() - millisOnStateChangeRecirculationFan), 1));
  display.print(":");
  display.println(millisToTimeFormat((millis() - millisOnStateChangeRecirculationFan), 0));

  // air intake info
  if (currentSpeedIntakeFan > 0) {
    display.print("I Fan 1 ");
  } else {
    display.print("I Fan 0 ");
  }

  display.print(millisToTimeFormat((millis() - millisOnStateChangeIntakeFan), 3));
  display.print(":");
  display.print(millisToTimeFormat((millis() - millisOnStateChangeIntakeFan), 2));
  display.print(":");
  display.print(millisToTimeFormat((millis() - millisOnStateChangeIntakeFan), 1));
  display.print(":");
  display.println(millisToTimeFormat((millis() - millisOnStateChangeIntakeFan), 0));

  // passive humidifier  info
  if (isPasiveHumidifierOn == true) {
    display.print("P Hum 1 ");
  } else {
    display.print("P Hum 0 ");
  }

  display.print(millisToTimeFormat((millis() - millisOnStateChangePasiveHumidifier), 3));
  display.print(":");
  display.print(millisToTimeFormat((millis() - millisOnStateChangePasiveHumidifier), 2));
  display.print(":");
  display.print(millisToTimeFormat((millis() - millisOnStateChangePasiveHumidifier), 1));
  display.print(":");
  display.println(millisToTimeFormat((millis() - millisOnStateChangePasiveHumidifier), 0));

  display.display();

  // active humidifier  info
  if (isHumidifierOn == true) {
    display.print("A Hum 1 ");
  } else {
    display.print("A Hum 0 ");
  }

  display.print(millisToTimeFormat((millis() - millisOnStateChangeHumidifier), 3));
  display.print(":");
  display.print(millisToTimeFormat((millis() - millisOnStateChangeHumidifier), 2));
  display.print(":");
  display.print(millisToTimeFormat((millis() - millisOnStateChangeHumidifier), 1));
  display.print(":");
  display.println(millisToTimeFormat((millis() - millisOnStateChangeHumidifier), 0));

  display.display();
}

void setPassiveHumidifierState(void) {
  
  if(isPasiveHumidifierOn == true){
    if(millis() - millisOnStateChangePasiveHumidifier > BIOME_FORCE_PASIVE_HUMIDIFIER_CYCLE_ON_MILLIS) {
      isPasiveHumidifierOn = false;
      millisOnStateChangePasiveHumidifier = millis();
    }
  } else {
    if(millis() - millisOnStateChangePasiveHumidifier > BIOME_FORCE_PASIVE_HUMIDIFIER_CYCLE_OFF_MILLIS) {
      isPasiveHumidifierOn = true;
      millisOnStateChangePasiveHumidifier = millis();
    }
  }
  
}
void forceTogglePassiveHumidifierState(void) {
  if(isPasiveHumidifierOn == true) {
    isPasiveHumidifierOn = false;
  } else {
    isPasiveHumidifierOn = true;
  }
  millisOnStateChangePasiveHumidifier = millis();
}

void initTogglePassiveHumidifierbutton(void) {
  pinMode(BUTTON_TOGGLE_PASSIVE_HUMIDIFIER, INPUT_PULLUP);
}
void handlePassiveHumidifierButtonState(void) {
  int state = digitalRead(BUTTON_TOGGLE_PASSIVE_HUMIDIFIER);
  if(state == 0) {
    forceTogglePassiveHumidifierState();
  }
}

void initIntakeFan(void) {
  // verify if pin supports pwm
  // digitalPinHasPWM(INTAKE_FAN_MOSFET_PWM_PIN)
  pinMode(INTAKE_FAN_MOSFET_PWM_PIN, OUTPUT);
	analogWrite(INTAKE_FAN_MOSFET_PWM_PIN, getFanPwmDuty(0));
}

void setIntakeFanSpeedDuty(int speed){
  analogWrite(INTAKE_FAN_MOSFET_PWM_PIN, speed);
}
int decideIntakeFanSpeed( int currentSpeed, 
                                  unsigned long currentMillis, 
                                  unsigned long _millisOnStateChangeIntakeFan) {
  unsigned long MINIMUM_MILLIS_TO_AVOID_INTAKE_AFTER_HUMIDIFICATION = 1ul*60ul*1000ul;
  unsigned long millisSinceLastHumidification = millis() - millisWhenLastHumidificationEnded;

  if(isHumidifierOn || isPasiveHumidifierOn || millisSinceLastHumidification < MINIMUM_MILLIS_TO_AVOID_INTAKE_AFTER_HUMIDIFICATION  ) {
    return false;
  }
  
  if (stateHumidity > BIOME_HUMIDITY_TARGET + BIOME_HUMIDITY_TARGET_TOLERANCE) {
    forceHumidityEvacuationUntillTargetReached = true;
  }

  if(forceHumidityEvacuationUntillTargetReached) {
    float humidityDifference = stateHumidity - BIOME_HUMIDITY_TARGET;
    if(humidityDifference > BIOME_HUMIDITY_TARGET_TOLERANCE*3){
      return getFanPwmDuty(10);
    } else if(humidityDifference > BIOME_HUMIDITY_TARGET_TOLERANCE*2) {
      return getFanPwmDuty(7);
    } else if(humidityDifference > BIOME_HUMIDITY_TARGET_TOLERANCE) {
      return getFanPwmDuty(5);
    } else if(humidityDifference > 0) {
      return getFanPwmDuty(2);
    } else {
      forceHumidityEvacuationUntillTargetReached = false;
      return getFanPwmDuty(0);
    }
  }

  // TODO: this can be wrapped in decideIntakeFanMaintenanceSpeed()
  // if humidity is in check then just do maintenance if needed
  bool isIntakeFanOn = currentSpeedIntakeFan > 0;

  unsigned long MAXIMUM_MILLIS_INTAKE_FAN_ON = 2ul*60ul*1000ul; // millis to not allow fan to stay ON for more than specified value 
  unsigned long MINIMUM_MILLIS_INTAKE_FAN_ON = 10ul*1000ul; // millis to force fan to remain ON once it started

  unsigned long MAXIMUM_MILLIS_INTAKE_FAN_OFF = 15ul*60ul*1000ul;  // millis until fan ON for maintenance airflow. Will stay on until MINIMUM_MILLIS_FAN_ON
  unsigned long MINIMUM_MILLIS_INTAKE_FAN_OFF = 1ul*60ul*1000ul; // millis to force fan to stay OFF
  
  
  if(isIntakeFanOn && currentMillis - _millisOnStateChangeIntakeFan > MAXIMUM_MILLIS_INTAKE_FAN_ON) {
   return getFanPwmDuty(0);
  }
  if(isIntakeFanOn && currentMillis - _millisOnStateChangeIntakeFan < MINIMUM_MILLIS_INTAKE_FAN_ON) {
    return currentSpeed;
  }

  if(!isIntakeFanOn && currentMillis - _millisOnStateChangeIntakeFan > MAXIMUM_MILLIS_INTAKE_FAN_OFF) {
    return getFanPwmDuty(1);
  }
  if(!isIntakeFanOn && currentMillis - _millisOnStateChangeIntakeFan < MINIMUM_MILLIS_INTAKE_FAN_OFF) {
    return getFanPwmDuty(0);
  }

  return getFanPwmDuty(0);

}
void setIntakeFanState(void) {
  int prevFanSpeed = currentSpeedIntakeFan;
  currentSpeedIntakeFan = decideIntakeFanSpeed(prevFanSpeed, millis(), millisOnStateChangeIntakeFan); 
  
  if(currentSpeedIntakeFan > 0) {
    if (prevFanSpeed != currentSpeedIntakeFan) {
      millisOnStateChangeIntakeFan = millis();
    }
  } else {
    if (prevFanSpeed > 0) {
      millisOnStateChangeIntakeFan = millis();
    }
  }
  setIntakeFanSpeedDuty(currentSpeedIntakeFan);
  // Serial.print("Intake pwm: ");
  // Serial.println(currentSpeedIntakeFan);

  
}


void initRecirculationFan(void) {
  // verify if pin supports pwm
  // digitalPinHasPWM(RECIRCULATION_FAN_MOSFET_PWM_PIN)
  
  pinMode(RECIRCULATION_FAN_MOSFET_PWM_PIN, OUTPUT);
	analogWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, getFanPwmDuty(0));
}

void setRecirculationFanSpeedDuty(int speed){
  analogWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, speed);
}

int decideRecirculationFanSpeed( int currentSpeed, 
                                  unsigned long currentMillis, 
                                  unsigned long _millisOnStateChangeReirculationFan) {

  
  // recirculate as long as intake or humidifier is on
  if(currentSpeedIntakeFan > 0 || isHumidifierOn == true) {
    return getFanPwmDuty(1);
  }

  // TODO: this can be wrapped in decideRecirculationFanMaintenanceSpeed()
  unsigned long MAXIMUM_MILLIS__RECIRCULATION_FAN_ON = 2ul*60ul*1000ul; // millis to not allow fan to stay ON for more than specified value (has priority)
  unsigned long MINIMUM_MILLIS__RECIRCULATION_FAN_ON = 1ul*60ul*1000ul; // millis to force fan to remain ON once it started
  
  unsigned long MAXIMUM_MILLIS_RECIRCULATION_FAN_OFF = 5ul*60ul*1000ul;  // millis until fan ON for maintenance airflow. Will stay on until MINIMUM_MILLIS_FAN_ON (has priority)
  unsigned long MINIMUM_MILLIS_RECIRCULATION_FAN_OFF = 1ul*60ul*1000ul; // millis to force fan to stay OFF
  

  bool isRecirculationFanOn = currentSpeedRecirculationFan > 0;

  if(isRecirculationFanOn && currentMillis - _millisOnStateChangeReirculationFan > MAXIMUM_MILLIS__RECIRCULATION_FAN_ON) {
    return getFanPwmDuty(0);
  }
  if(isRecirculationFanOn && currentMillis - _millisOnStateChangeReirculationFan < MINIMUM_MILLIS__RECIRCULATION_FAN_ON) {
    return currentSpeed;
  }

  if(!isRecirculationFanOn && currentMillis - _millisOnStateChangeReirculationFan > MAXIMUM_MILLIS_RECIRCULATION_FAN_OFF) {
    return getFanPwmDuty(1);
  }
  if(!isRecirculationFanOn && currentMillis - _millisOnStateChangeReirculationFan < MINIMUM_MILLIS_RECIRCULATION_FAN_OFF) {
    return getFanPwmDuty(0);
  }

  return getFanPwmDuty(0);

}

void setRecirculationFanState(void) {
  int prevFanSpeed = currentSpeedRecirculationFan;
  currentSpeedRecirculationFan = decideRecirculationFanSpeed(prevFanSpeed, millis(), millisOnStateChangeRecirculationFan); 
  
  if(currentSpeedRecirculationFan > 0) {
    if (prevFanSpeed != currentSpeedRecirculationFan) {
      millisOnStateChangeRecirculationFan = millis();
    }
  } else {
    if (prevFanSpeed > 0) {
      millisOnStateChangeRecirculationFan = millis();
    }
  }
  setRecirculationFanSpeedDuty(currentSpeedRecirculationFan);
  // Serial.print("Recirculation pwm: ");
  // Serial.println(currentSpeedRecirculationFan);

  
}

void initHumidifier(void) {
  pinMode(HUMIDIFIER_MOSFET_PIN, OUTPUT);
	digitalWrite(RECIRCULATION_FAN_MOSFET_PWM_PIN, LOW);
}
int decideHumidifierState(  bool _currentState, 
                            unsigned long currentMillis, 
                            unsigned long _millisOnStateChangeHumidifier) {

  bool isIntakeFanOn = currentSpeedIntakeFan > 0;
  
  if(isIntakeFanOn) {
    return false;
  }

  if (stateHumidity < BIOME_HUMIDITY_TARGET - BIOME_HUMIDITY_TARGET_TOLERANCE) {
    forceHumidityIncreaseUntillTargetReached = true;
  }

  unsigned long PULSE_MILLIS_HUMIDIFIER_ON = 5ul*1000ul; 
  unsigned long PULSE_MILLIS_HUMIDIFIER_OFF = 10ul*1000ul; 
  
  if(forceHumidityIncreaseUntillTargetReached) {

    float humidityDifference = stateHumidity - BIOME_HUMIDITY_TARGET;
    if(humidityDifference > 0) {
      forceHumidityIncreaseUntillTargetReached = false;
      return false;
    }


    if(_currentState == true && currentMillis - _millisOnStateChangeHumidifier > PULSE_MILLIS_HUMIDIFIER_ON) {
      return false;
    }
    if(_currentState == true && currentMillis - _millisOnStateChangeHumidifier < PULSE_MILLIS_HUMIDIFIER_ON) {
      return true;
    }
    if(_currentState == false && currentMillis - _millisOnStateChangeHumidifier > PULSE_MILLIS_HUMIDIFIER_OFF) {
      return true;
    }
    if(_currentState == false && currentMillis - _millisOnStateChangeHumidifier < PULSE_MILLIS_HUMIDIFIER_OFF) {
      return false;
    }
    
   
  }




  return false;

}
void setHumidifierState(void) {
  int prevHumidifierState = isHumidifierOn;
  isHumidifierOn = decideHumidifierState(prevHumidifierState, millis(), millisOnStateChangeHumidifier); 
  
  if (prevHumidifierState != isHumidifierOn) {
    millisOnStateChangeHumidifier = millis();
    if(isHumidifierOn == false) {
      millisWhenLastHumidificationEnded = millis();
    }
  }

  if(isHumidifierOn) {
    digitalWrite(HUMIDIFIER_MOSFET_PIN, HIGH);
  } else {
    digitalWrite(HUMIDIFIER_MOSFET_PIN, LOW);
  }
  
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


  initIntakeFan();

  initRecirculationFan();

  // initHumidifier();

  initTogglePassiveHumidifierbutton();
  

  dht.begin();
}

void loop() {

  readDHTSensor();

  setPassiveHumidifierState();

  setRecirculationFanState();

  setIntakeFanState();

  // setHumidifierState();

  if (millis() - millisStateOnLastScreensaver < 120000) {
    displayState();
  } else {
    millisStateOnLastScreensaver = millis();
    screensaver();
  }


  handlePassiveHumidifierButtonState();

  delay(LOOP_INTERVAL);
}
