# YetAnotherESP32WeatherStation
Like the name of the repo describes, this is yet another ESP32 based weather station project made for school.

This project displays inside and outside temperature and inside humidity along with weather forecast for next 3 hours.
Values are displayed on a 1,3 inch OLED display. 

Sensors used for this project are DHT22 for inside and DS18B20 waterproofed version for outside. This project uses Adafruit GFX, BUSIO, Unified Sensor and SSD1106 libraries and DHT sensor library for display and DHT22, OneWire and DallasTemperature libraries for DS18B20 sensor, Arduino_JSON library for parsing weather forecast and elapsedMillis for timing tasks. All thanks to the amazing people behind these libraries.

Weather forecast is requested using HTTP GET method from OpenWeatherMap API.
