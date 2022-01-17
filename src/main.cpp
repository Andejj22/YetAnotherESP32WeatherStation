#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <elapsedMillis.h>
#include <Arduino_JSON.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Icons.h>
#include <config.h>

#define cold2_width  64
#define cold2_heigth 64

#define OLED_SDA 21
#define OLED_SCL 22

#define DHTPIN 14
#define DHTTYPE DHT22

const int oneWireBus = 4;

const char* ssid = MY_SSID;
const char* password = MY_PASSWORD;

String apiKey = MY_APIKEY;
String httpGETRequest(const char* serverName);
void displayInsideTemp(int isDispInterval);
void displayOutsideTemp(int osDispInterval, int tempRequestInterval);
void displayForecast(int forecastInterval, int weatherID);
bool inRange(int val, int min, int max);

String city = "Helsinki";
String countryCode = "FI";

int timeStamps = 1; //defines how many 3-hour steps in forecast is requested.

String jsonBuffer;

OneWire oneWire(oneWireBus);
DallasTemperature outTempSens(&oneWire);
elapsedMillis displayTimer; //timer for each diplay item
elapsedMillis tempTimer; //timer for each temp request from DS18B20

Adafruit_SH1106 display(OLED_SDA, OLED_SCL); //construct a display object
DHT dht(DHTPIN, DHTTYPE); //init DHT22 sensor

void setup()   {                
  Serial.begin(115200);
  /* initialize OLED with I2C address 0x3C */
  display.begin(SH1106_SWITCHCAPVCC, 0x3C); 
  display.clearDisplay();
  delay(2000);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  dht.begin();
  outTempSens.begin();
  outTempSens.setResolution(12);

  WiFi.begin(ssid, password);
  Serial.print("Connecting...");
  
  while(WiFi.status() != WL_CONNECTED){
    display.setCursor(0,0);
    display.setTextSize(1);
    display.print("Connecting...");
    display.drawBitmap(30, 10,  wifi_icon, wifi_icon_width, wifi_icon_height, 1);
    display.display();
    delay(500);
    Serial.print(".");
  }
  display.clearDisplay();
  display.drawBitmap(30, 10,  wifi_icon, wifi_icon_width, wifi_icon_height, 1);
  display.setCursor(0,0);
  display.print("Connected!");
  display.display();
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  delay(1000);
}

void loop() { 
  /* TODO!

  Make timer for API calls! Now they are requested every 10s,
  but OpenWeather only updates them every 10 minutes!
  
  */
  
  displayInsideTemp(5000);
  displayOutsideTemp(5000, 1000);
  

  if(WiFi.status() == WL_CONNECTED){
    String serverPath = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "," + countryCode + "&cnt="+ timeStamps + "&APPID=" + apiKey;

    jsonBuffer = httpGETRequest(serverPath.c_str());

    JSONVar weatherForecast = JSON.parse(jsonBuffer);

    if(JSON.typeof(weatherForecast) == "undefined"){
      Serial.println("Parsing JSON failed!");
      return;
    }else{
      Serial.print("JSON object = ");
      Serial.println(weatherForecast);
      Serial.println(weatherForecast["list"][0]["weather"][0]["id"]);
      Serial.println(weatherForecast["list"][0]["dt_txt"]);

      /*weatherId is unique ID number from API to define weather.
      weatherId is passed as a parameter to function displayForecast
      to determine the weather icon*/

      int weatherId = weatherForecast["list"][0]["weather"][0]["id"]; 
      displayForecast(5000, weatherId);
      

    }
    
  }
}

// Method to get weather forecast for today from OpenWeather API

String httpGETRequest(const char* serverName){ 
  
  HTTPClient http;

  http.begin(serverName);

  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode>0){
    Serial.print("HTTP Response code: ");
    Serial.print(httpResponseCode);
    payload = http.getString();
  }else{
    Serial.print("Error code:");
    Serial.print(httpResponseCode);
  }

  http.end();

  return payload;

}

//Method to display inside temperature
void displayInsideTemp(int isDispInterval){

  displayTimer = 0;

  while(displayTimer < isDispInterval){

      float humidity = dht.readHumidity();
      float temperature = dht.readTemperature();

      if(isnan(humidity) || isnan(temperature)){
        Serial.println("Failed to read from DHT sensor!");

      }else{
        display.clearDisplay();
          // display temperature
        display.drawBitmap(0,0,  temperature_icon, temperature_width, temperature_height, 1);
        display.setTextSize(1);
        display.setCursor(24,22);
        display.print("in");
        display.setTextSize(2);
        display.setCursor(32,0);
        display.print(temperature);
        display.print(" ");
        display.setTextSize(1);
        display.cp437(true);
        display.write(167);
        display.setTextSize(2);
        display.print("C");

          // display humidity
        display.drawBitmap(0,32, humidity_icon, humidity_width, humidity_height, 1);
        display.setTextSize(2);
        display.setCursor(32,45);
        display.print(humidity);
        display.print(" %"); 
           
        display.display(); 

    }
  }
}

//Method to display outside temperature
void displayOutsideTemp(int osDispInterval, int tempRequestInterval){

  displayTimer = 0;

  while(displayTimer < osDispInterval){

    if(tempTimer > tempRequestInterval){

      tempTimer = 0;
      outTempSens.requestTemperatures();
      float outSideTemp = outTempSens.getTempCByIndex(0);
        display.clearDisplay();
          // display temperature
        display.drawBitmap(0,0,  temperature_icon, temperature_width, temperature_height, 1);
        display.setTextSize(1);
        display.setCursor(24,22);
        display.print("out");
        display.setTextSize(2);
        display.setCursor(32,0);
        display.print(outSideTemp);
        display.print(" ");
        display.setTextSize(1);
        display.cp437(true);
        display.write(167);
        display.setTextSize(2);
        display.print("C");

        display.display();

    }
  }  
}

void displayForecast(int forecastInterval, int id){
  displayTimer = 0;
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print("Next 3 hours:");

  Serial.println(id);

  /*range 200-299 = thunderstorm class
    range 300-399 = drizzle class
    range 500-599 = rain class
    range 600-699 = snow class
    range 700-799 = describes atmostphere (fog etc.)
    value 800 = clear sky
    range 801-899 = clouds class */
  while(displayTimer<forecastInterval){

    if(inRange(id,200,299)){ 

      display.drawBitmap(30, 5,  thunderstorm, 64, 64, 1);
      display.display();

    }

    if(inRange(id,200,299)){ 

      display.drawBitmap(30, 5,  thunderstorm, 64, 64, 1);
      display.display();

    }

    if(inRange(id,300,501)){ 

      display.drawBitmap(30, 5,  drizzle, 64, 64, 1);
      display.display();

    }

    if(inRange(id,600,699)){ 

      display.drawBitmap(30, 5,  snow, 64, 64, 1);
      display.display();

    }

    if(id == 800){ 

      display.drawBitmap(30, 5,  clear_sky, 64, 64, 1);
      display.display();

    }

    if(id == 801){
      display.drawBitmap(30, 5, cloud1, 64, 64, 1);
      display.display();
    }

    if(id == 802){
      display.drawBitmap(30, 5, cloud2, 64, 64, 1);
      display.display();
    }

    if(id == 803 || id == 804){
      display.drawBitmap(30, 5, cloud3, 64, 64, 1);
      display.display();
    }
  }
}

/*function inRange() checks if value is between given min and max
returns boolean true or false*/
bool inRange(int val, int min, int max){
  return ((min <= val) && (val <= max));
}
