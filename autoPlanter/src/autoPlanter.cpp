/* 
 * Project Smart Planter
 * Author: Zuyva Sevilla
 * Date: NOV 6 2023
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

#include "Particle.h"
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

//Time
String DateTime, TimeOnly;

//OLED
int OLED_RESET = D4;
Adafruit_SSD1306 display(OLED_RESET);

//Neopixels
int pixelCount = 16;
Adafruit_NeoPixel strip(pixelCount, SPI1, WS2812B);

//Feeds
TCPClient TheClient; 
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIR_SERVERPORT,AIO_USERNAME,AIO_KEY); 

Adafruit_MQTT_Subscribe buttonFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/buttononoff"); 
Adafruit_MQTT_Publish AQfeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/airquality");
Adafruit_MQTT_Publish AQvoltageFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/airqualityvolts");
Adafruit_MQTT_Publish dustfeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/dustsensor");
unsigned long pubTime; 
void MQTT_connect();
bool MQTT_ping();

Button waterButton(10);

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);


void setup() {
    Serial.begin(9600);
    WiFi.on();
    WiFi.connect();
    while(WiFi.connecting()) {
        Serial.printf(".");
    }
    Serial.printf("\n\n");
    Time.zone(-6);
    Particle.syncTime();

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
    starttime = millis();
    dustTimer = millis();

	strip.begin();  //Init Neopixels
	strip.setBrightness(255);
	strip.show();
    
    // Setup MQTT subscription
    mqtt.subscribe(&buttonFeed);
}

void loop() {
 
    //Check Sensors
    DateTime = Time.timeStr();
    TimeOnly = DateTime.substring(11,19);

	tempC = bme.readTemperature();
  	tempF = ((tempC*1.8)+32);
  	pressPA = bme.readPressure();
  	pressinHG=pressPA/3386;
  	humidRH = bme.readHumidity();

    airQuality = AQS.slope();
    AQvoltage = AQS.getValue();

    moisture = analogRead(moistProbe);



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

