/* 
 * Project Smart Planter
 * Author: Zuyva Sevilla
 * Date: NOV 6 2023
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

#include "Particle.h"
#include "credentials.h"
#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_BME280.h"
#include "Air_Quality_Sensor.h"
#include "neopixel.h"
#include "IoTClassroom_CNM.h"

//Sensors
//Dust
const int dustData = 6;
unsigned long duration;
unsigned long starttime;
unsigned long sampletime_ms = 30000; //sample time
unsigned long lowpulseoccupancy = 0;
float ratio = 0;
float concentration = 0;
unsigned long dustTimer;
unsigned long dustCheckTime = 60000;
//Air Quality
AirQualitySensor AQS(14);
int airQuality = -1;
int AQvoltage;
//Temp
Adafruit_BME280 bme;
int BMEstatus;
int BMEhex = 0x76;
int tempC;
float tempF;
int pressPA;
float pressinHG;
int humidRH;
char pCent = 0x25;
char degree = 0xF8;
//Dirt
const int moistProbe = 13; 
int moisture;

//Time and Timers
String DateTime, TimeOnly;
int currentTime;
int screenFlipTime;

//OLED
int OLED_RESET = D4;
Adafruit_SSD1306 display(OLED_RESET);
int currentOLED;
void setupScreen();

//Neopixels
int pixelCount = 16;
Adafruit_NeoPixel strip(pixelCount, SPI1, WS2812B);
Adafruit_NeoPixel ring(24, SPI, WS2812);
int plantLightState;

void ringFill(int startPixel, int endPixel, int fillColor);
void stripFill(int startPixel, int endPixel, int fillColor);

//Feeds
TCPClient TheClient; 
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIR_SERVERPORT,AIO_USERNAME,AIO_KEY); 

Adafruit_MQTT_Subscribe buttonFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/waterbutton"); 
Adafruit_MQTT_Publish AQfeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/airquality");
Adafruit_MQTT_Publish AQvoltageFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/airqualityvolts");
Adafruit_MQTT_Publish moistureFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/soilmoisture");
Adafruit_MQTT_Publish dustfeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/dustsensor");
Adafruit_MQTT_Publish BMEfeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/plantbme");
Adafruit_MQTT_Publish plantLightFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/plantlight");

unsigned long pubTime; 
int MQTTupdateTimer;
void MQTT_connect();
bool MQTT_ping();

Button waterButton(5);
const int waterPump = D19;
int waterCheckTimer;
int waterCheckInterval = 1800000; //how often to check if plant needs water
void waterPlant();



SYSTEM_MODE(AUTOMATIC);
//SYSTEM_THREAD(ENABLED);


void setup() {
  Serial.begin(9600);
  WiFi.on();
  WiFi.connect();
  while(WiFi.connecting()) {
     Serial.printf(".");
  }
  Serial.printf("\n\n");
  Time.zone(-7);
  Particle.syncTime(); 
  
  // Setup MQTT subscription
  mqtt.subscribe(&buttonFeed);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
 	display.display();
	display.setRotation(0);
 	delay(500);
  display.clearDisplay();
    
  BMEstatus = bme.begin(BMEhex);
 	if (BMEstatus==false){
    	Serial.printf("BME280 at address 0x%02X failed to start", BMEhex);
 	}
    
  pinMode(dustData, INPUT);
  pinMode(moistProbe, INPUT);
  pinMode(waterPump, OUTPUT);
  starttime = millis();
  dustTimer = millis();
  screenFlipTime = millis();
  waterCheckTimer = millis();
  MQTTupdateTimer = millis();

	strip.begin();  //Init Neopixels
  ring.begin();
	strip.setBrightness(255);
  ring.setBrightness(75);
	strip.show();

  delay(500);
  ringFill(0,24,red);
  stripFill(0,12,red);
  delay(500);
  ringFill(0,24,green);
  stripFill(0,12,green);
  delay(500);
  ringFill(0,24,blue);
  stripFill(0,12,purple);
  //delay(500);
  //stripFill(0,12,black);

    

}

void loop() {
 
  //MQTT sub/pub
  MQTT_connect();
  MQTT_ping();

  // this is our 'wait for incoming subscription packets' busy subloop 
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &buttonFeed) {
      if (atof((char *)buttonFeed.lastread) == 1){
      waterPlant();
      Serial.printf("Plant Watered through the Net!");
      setupScreen();
      display.printf("PLANT\n eSAVED!");
      display.display();
      delay(2000);
      }
    }
  }  
  if((currentTime-MQTTupdateTimer > 12000)) {
    if(mqtt.Update()) {
      AQfeed.publish(airQuality);
      AQvoltageFeed.publish(AQvoltage);
      moistureFeed.publish(moisture);
      BMEfeed.publish(tempF);
      plantLightFeed.publish(plantLightState);
      Serial.printf("MQTT Published");
    } 
    MQTTupdateTimer = millis();
  }



  if (waterButton.isClicked()){
    waterPlant();
    Serial.printf("Plant Saved!");
    setupScreen();
    display.printf("PLANT\n SAVED!");
    display.display();
    delay(2000);
  }

  if ((currentTime - waterCheckTimer) > waterCheckInterval){
    if (moisture>2500){  //change this for dryness trigger
        waterPlant();
        Serial.printf("Plant Saved!");
        setupScreen();
        display.printf("PLANT\n SAVED!");
        display.display();
        delay(2000);
    }
  }


//Read Sensors
  DateTime = Time.timeStr();
  TimeOnly = DateTime.substring(11,16);
  currentTime = millis();

	tempC = bme.readTemperature();
  tempF = ((tempC*1.8)+32);
  pressPA = bme.readPressure();
  pressinHG=pressPA/3386;
  humidRH = bme.readHumidity();
  airQuality = AQS.slope();
  AQvoltage = AQS.getValue();
  moisture = analogRead(moistProbe);
  Serial.printf("Time: %s\nTemp: %0.2f\nAir Quality: %i\nSoil Moisture: %i\n",TimeOnly,tempF,airQuality,moisture);
  Serial.printf("Current Air Quality: %i\nVoltage Reading:%i\n\n",airQuality,AQvoltage);

//Daylight
  if (Time.hour()>7 && Time.hour()<18){
    ringFill(0,24, white);
    plantLightState = 1;
  }
  else{
    ringFill(0,24,black);
    plantLightState = 0;
  }  


//OLED Screen
  if((millis()-screenFlipTime)>5000){
    currentOLED++;

    if(currentOLED>5){
      currentOLED = 0;
    }
    switch(currentOLED){
      case 0:
        setupScreen();
        display.printf("Temp:\n  %0.1f%cF",tempF,degree);
        display.display();
      break;
      case 1:
        setupScreen();
        display.printf("Pressure:\n %0.1f inHG ",pressinHG);
        display.display();
      break;
      case 2:
        setupScreen();
        display.printf("Humidity:\n  %i%c",humidRH, pCent);
        display.display();
      break;
      case 3:
        setupScreen();
        display.printf("Time:\n %s",TimeOnly.c_str());
        display.display();
      break;
      case 4:
        if (airQuality == 1){
          setupScreen();
          display.printf("Air:\n BAD");
          display.display();
        } 
        else if (airQuality == 2){
          setupScreen();
          display.printf("Air:\n NOT GREAT");
          display.display();
        } 
        else if (airQuality == 3){
          setupScreen();
          display.printf("Air:\n DECENT");
          display.display();
        } 
      break;
      case 5:
        setupScreen();
        display.printf("Dirt:\n %i",moisture);
        display.display();
      break;
    }
    screenFlipTime = millis();
  } 



}

//MQTT Functions 
void MQTT_connect() {
  int8_t ret;
 
  // Return if already connected.
  if (mqtt.connected()) {
    return;
  }
 
  Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.printf("Error Code %s\n",mqtt.connectErrorString(ret));
       Serial.printf("Retrying MQTT connection in 5 seconds...\n");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds and try again
  }
  Serial.printf("MQTT Connected!\n");
}
bool MQTT_ping() {
  static unsigned int last;
  bool pingStatus;

  if ((millis()-last)>120000) {
      Serial.printf("Pinging MQTT \n");
      pingStatus = mqtt.ping();
      if(!pingStatus) {
        Serial.printf("Disconnecting \n");
        mqtt.disconnect();
      }
      last = millis();
  }
  return pingStatus;
}

void ringFill(int startPixel, int endPixel, int fillColor){
 int i;

  for(i=startPixel; i<=endPixel; i++){
    ring.setPixelColor(i, fillColor);
  } 

  ring.show();
}

void stripFill(int startPixel, int endPixel, int fillColor){
 int i;

  for(i=startPixel; i<=endPixel; i++){
    strip.setPixelColor(i, fillColor);
  } 

  strip.show();
}

void waterNeoFill(float r, float g, float b){
 	int i;
  for(i=0; i<=13; i++){
    strip.setPixelColor(i, r, g, b);
  } 
  strip.show();
}

void setupScreen(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(8,10);
}

void waterPlant(){
    float r, g, b;
    int i;
    for (i=0; i<255; i++){
        r = (i/256.0)*0;
        g = (i/256.0)*255;
        b = (i/256.0)*255;
        waterNeoFill(r, g, b);
    }

      digitalWrite(waterPump,HIGH);
      delay(500);
      digitalWrite(waterPump,LOW);

    for (i=255; i>0; i--){
        r = (i/256.0)*0;
        g = (i/256.0)*255;
        b = (i/256.0)*255;
        waterNeoFill(r, g, b);
    }

}