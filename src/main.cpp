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

#define OLED_SDA 21
#define OLED_SCL 22

#define DHTPIN 14
#define DHTTYPE DHT22

const int oneWireBus = 4;

/*Replace ssid and password with your network creditentials, or define them 
in config.h file like I did*/

const char* ssid = MY_SSID;
const char* password = MY_PASSWORD;

String weatherApiKey = MY_WEATHER_APIKEY;
String thingsApiKey = MY_THINGS_APIKEY;

String httpGETRequest(const char* serverName);
void displayInsideTemp(float insideTemp, float hum);
void displayOutsideTemp(float outsideTemp);
void displayFirstForecast(int forecastInterval, int weatherID, const char * time_1, int temperature1);
void displaySecondForecast(int forecastInterval, int weatherID, const char * time_2, int temperature2);
void displayThirdForecast(int forecastInterval, int weatherID, const char * time_3, int temperature3);
bool inRange(int val, int min, int max);

String city = "Helsinki";
String countryCode = "FI";

/*defines how many 3-hour forecasts are requested from API.
Note that changing this value has effect on displaying forecast*/
int timeStamps = 3; 

/*intervals for timers. Updateinterval defines in which interval sensor
readings are updated to ThingsSpeak, dhtInterval rate of dht measurements
and dallasTempInterval rate of DS18B20 measurements. Loop takes 17s to get
to update sequence so any interval under that is good as nothing. Also ThingsSpeak
doesn't allow more than 1 request /15s anyways for free subscription*/
int updateInterval = 600000; //10 minutes
int dhtInterval = 2000; 
int dallasTempInterval = 1000;

/*variables to hold counters for measurements done in loop, total counts of
inside temp and humidity and outside temp measured between updating data to cloud*/
int measurementCnt = 0, insideTotalCnt = 0, outsideTotalCnt = 0;

/*variables to hold temporary data and data to send*/
float outsideTempSum = 0, insideTempSum = 0, humiditySum = 0,
      outsideTempSend, insideTempSend, humiditySend;

String jsonBuffer;

/*creating instances of sensors and timers*/
OneWire oneWire(oneWireBus);
DallasTemperature outTempSens(&oneWire);
DHT dht(DHTPIN, DHTTYPE); //init DHT22 sensor
elapsedMillis displayTimer; //timer for each diplay item
elapsedMillis measurementTimer; //timer for each temp request from DS18B20
elapsedMillis updateSensorsTime; //Timer for HTTP POST requests

HTTPClient http;

Adafruit_SH1106 display(OLED_SDA, OLED_SCL); //construct a display object


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
  outTempSens.setResolution(12); //Setting outside temperature to use 12bit resolution. 

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

  Make timer for API calls! Now they are requested every 15s,
  but OpenWeather only updates them every 10 minutes!
  
  */
  measurementTimer = 0;

  /* This loop is used to measure inside temp and humidity 3 times
  between 2 second intervals and display them. DHT22 updates sensor 
  values every 2 seconds so requesting values more often would be useless*/

  while(measurementCnt<3){

    if(measurementTimer > dhtInterval){
      float humidity = dht.readHumidity();
      float temperature = dht.readTemperature();
      measurementCnt++;
      measurementTimer = 0; 
      insideTempSum += temperature;
      humiditySum += humidity;
      insideTotalCnt++;
      displayInsideTemp(temperature, humidity);

      if(isnan(humidity) || isnan(temperature)){
        Serial.println("Failed to read from DHT sensor!");
      }  
    }
  }
  measurementCnt = 0;

  /*This loop is used to measure outside temp 5 times with 1s intervals
  DS18B20 is capable of doing so with 12bit resolution*/

  while(measurementCnt < 5){

    if(measurementTimer > dallasTempInterval){
      outTempSens.requestTemperatures();
      float outsideTemp = outTempSens.getTempCByIndex(0);
      displayOutsideTemp(outsideTemp);
      measurementCnt++;
      measurementTimer = 0;
      outsideTempSum += outsideTemp;
      outsideTotalCnt++;
    }

  }

  measurementCnt = 0;


  if(WiFi.status() == WL_CONNECTED){


    String weatherServerPath = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "," + countryCode 
                        + "&cnt="+ timeStamps + "&APPID=" + weatherApiKey;

    jsonBuffer = httpGETRequest(weatherServerPath.c_str());

    JSONVar weatherForecast = JSON.parse(jsonBuffer);

    if(JSON.typeof(weatherForecast) == "undefined"){
      Serial.println("Parsing JSON failed!");
      return;
    }else{
      Serial.print("JSON object = ");
      Serial.println(weatherForecast);

      /*to display time of the forecasted weather access to
      dt_txt variable of JSON is needed. This contains the date and
      the time but only time is needed. Time is in form HH:MM:SS 
      but this is too long to be displayed in small oled. That's
      why it's parsed to HH:MM form*/

      /*pointers to memory where date and time of forecasts
      are stored*/
      const char *dateOne = weatherForecast["list"][0]["dt_txt"];
      const char *dateTwo = weatherForecast["list"][1]["dt_txt"];
      const char *dateThree = weatherForecast["list"][2]["dt_txt"];



      /*pointers to access elements of arrays */
      const char *p1 = dateOne;
      const char *p2 = dateTwo;
      const char *p3 = dateThree;

      
      /*each char takes 1 byte of program memory so time + 11 should
      end up to first element of time part of the dt_txt, as long as
      openWeather doesn't change api. Increasing value of pointer to match 
      memory location of first element of time */
      p1 += 11; 
      p2 += 11;
      p3 += 11;

      /*array to store parsed time. Since this is a C string it needs
      one extra element for terminating null character*/
      char time1[6];
      char time2[6];
      char time3[6];

      /*parsing array to contain only hours and minutes*/
      for(int i = 0; i < 5; i++){
        time1[i] = *p1;
        time2[i] = *p2;
        time3[i] = *p3;
        p1++;
        p2++;
        p3++;
      }
      time1[5] = '\0';
      time2[5] = '\0';
      time3[5] = '\0';

      /*weatherId is unique ID number from API to define weather.
      weatherId is passed as a parameter to function displayForecast
      to determine the weather icon*/

      int weatherId1 = weatherForecast["list"][0]["weather"][0]["id"]; 
      int weatherId2 = weatherForecast["list"][1]["weather"][0]["id"];
      int weatherId3 = weatherForecast["list"][2]["weather"][0]["id"];

      int forecastTemp1 = weatherForecast["list"][0]["main"]["temp"];
      int forecastTemp2 = weatherForecast["list"][1]["main"]["temp"];
      int forecastTemp3 = weatherForecast["list"][2]["main"]["temp"];

      forecastTemp1 -= 273;
      forecastTemp2 -= 273;
      forecastTemp3 -= 273;

    
      displayFirstForecast(2000, weatherId1, time1, forecastTemp1);
      displaySecondForecast(2000, weatherId2, time2, forecastTemp2);
      displayThirdForecast(1000, weatherId3, time3, forecastTemp3); //third forecast is already displayed for 2s because of measurementTimer
      
    }

    if(updateSensorsTime > updateInterval){

      humiditySend = humiditySum/insideTotalCnt;
      insideTempSend = insideTempSum/insideTotalCnt;
      outsideTempSend = outsideTempSum/outsideTotalCnt;
      

      String thingsServerPath = "http://api.thingspeak.com/update?api_key=" + thingsApiKey + "&field1=" + outsideTempSend
                              + "&field2=" + insideTempSend + "&field3=" + humiditySend;
      httpGETRequest(thingsServerPath.c_str());
      updateSensorsTime = 0;
      outsideTempSum = 0;
      insideTempSum = 0;
      humiditySum = 0;
      insideTotalCnt = 0;
      outsideTotalCnt = 0;
    }  
  }
}

// Method to get weather forecast for today from OpenWeather API

String httpGETRequest(const char* serverName){ 
  
  http.begin(serverName);

  int httpWeatherResponseCode = http.GET();

  String payload = "{}";

  if (httpWeatherResponseCode>0){
    Serial.print("HTTP Response code: ");
    Serial.print(httpWeatherResponseCode);
    payload = http.getString();
  }else{
    Serial.print("Error code:");
    Serial.print(httpWeatherResponseCode);
  }

  http.end();

  return payload;

}


//Method to display inside temperature

void displayInsideTemp(float insideTemp, float hum){

    display.clearDisplay();
     // display temperature
    display.drawBitmap(0,0,  temperature_icon, temperature_width, temperature_height, 1);
    display.setTextSize(1);
    display.setCursor(24,22);
    display.print("in");
    display.setTextSize(2);
    display.setCursor(32,0);
    display.print(insideTemp);
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
    display.print(hum);
    display.print(" %"); 
           
    display.display();         
   
  }

//Method to display outside temperature

void displayOutsideTemp(float outsideTemp){

  display.clearDisplay();
    // display temperature
  display.drawBitmap(0,0,  temperature_icon, temperature_width, temperature_height, 1);
  display.setTextSize(1);
  display.setCursor(24,22);
  display.print("out");
  display.setTextSize(2);
  display.setCursor(32,0);
  display.print(outsideTemp);
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(2);
  display.print("C");
  display.display();      

 }
  
  


void displayFirstForecast(int forecastInterval, int id, const char * time_1, int temperature1){
  displayTimer = 0;
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print(time_1);
  display.setCursor(90,50);
  display.print(temperature1);
  display.print(" ");
  display.cp437(true);
  display.write(167);
  display.print("C");


  //Serial.println(id);

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

void displaySecondForecast(int forecastInterval, int id, const char * time_2, int temperature2){
  displayTimer = 0;
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print(time_2);
  display.setCursor(90,50);
  display.print(temperature2);
  display.print(" ");
  display.cp437(true);
  display.write(167);
  display.print("C");

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

void displayThirdForecast(int forecastInterval, int id, const char * time_3, int temperature3){
  displayTimer = 0;
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print(time_3);
  display.setCursor(90,50);
  display.print(temperature3);
  display.print(" ");
  display.cp437(true);
  display.write(167);
  display.print("C");

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
