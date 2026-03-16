# CIJE Weather Station
Weather affects us all—shaping our daily routines, the seasons we experience, and the environment around us. It’s also part of a bigger picture: the patterns that unfold over time. By collecting and sharing local weather data, we can begin to see those patterns, compare conditions across communities, and learn more about the world we live in. The [Weather Station Hub](https://thecije.org/weather-hub/) is built to make that possible—powered by student-made stations across schools. Each station collects real-time data that feeds into this hub, where you can explore maps, graphs, and news. Build your own station, register it, and join the growing community of schools tracking weather together.

## 2 tracks, One hub
There are 2 tracks to build a weather station: 
- Either an ESP32 based station with WiFi that uploads data to the Weather Station Hub or 
- an Uno based station with nrf24 transceiver and an LCD for a localized weather station.

## Hardware

Track 1: WiFi enabled / Post to Weather Hub
- ACEBOTT ESP32 development board
- DHT22 temperature and humidity sensor
OR
  BME280 (Pressure, Temperature and Humidity)
- Analog wind speed sensor (DC motor turbine)

The ESP32 will need to be setup in the Arduino IDE. Follow the [ESP32 Board Setup on Arduino](https://github.com/CIJE-STEM/cije-weather-station/blob/main/docs/ESP32setup.pdf)

Track 2: Local weather station displaying in classroom
- nRF24 Tranceiver
- Arduino Uno
- 16x2 LCD Screen or other display unit
- DHT22 temperature and humidity sensor
- Analog wind speed sensor (DC motor turbine)
- BME280 (Pressure, Temperature and Humidity)

## Weather Station Build
Use the Weather Station Build instructions using the DHT22
[Weather Station with DHT22 Build](https://github.com/CIJE-STEM/cije-weather-station/blob/main/docs/Weather%20Station%20Build%20DHT22%20Basic.pdf)
OR the BME280
[Weather Station with BME280 Build](https://github.com/CIJE-STEM/cije-weather-station/blob/main/docs/Weather%20Station%20Build%20BME280%20Basic.pdf)
OR the local nRF24 weather station

## Register Station
Register station at [CIJE Weather Station Hub](https://thecije.org/weather-hub/)  to obtain a passkey and station ID required for uploading data from your weather station.

## Software Usage
There is both library code and non-library (designated as "Basic") code designated by the filename.
Build the weather station by adding components and testing with code one by one using example code designated as "test"

In addition to the test code for each component, there is main (no "test" suffix) code for your specific build that contains all the hardware elements. Choose the program based on your hardware configuration and preference (library vs. non-library):

## Examples

### Non-Library "Basic" code
#### Tests
- weather_station_esp32_basic_dht22_test  // Tests the DHT22
- weather_station_esp32_basic_bme280_test // Tests the BME280
- weather_station_esp32_basic_anemometer_test // Tests the anemometer setup
- weather_station_library_esp32_basicdata_test  // Tests the connection to the cloud weather database

#### Main
- weather_station_esp32_basic_bme280.ino  // Full weather station using the BME280 and anemometer to upload wind speed, temperature, humidity and pressure
- weather_station_esp32_basic_dht22.ino  // Full weather station using the DHT22 and anemometer to upload wind speed, temperature and humidity

### Library code
- weather_station_library_esp32_connection_test  //Library code to test the connection to the weather hub database

### nRF24 Local Weather Station
weather_station_uno_nrf24_receive_lcd_datalogging
weather_station_uno_nrf24_transmit_dht22_voltage



## Library Configuration

### Library Features
- **Easy Configuration**: Simple API for setting up WiFi, sensors, and station credentials
- **Multiple Sensors**: Support for DHT22 (temperature/humidity) and analog wind sensors
- **Automatic Submission**: Configurable intervals for data collection and API submission
- **Status Monitoring**: LED status indicators and comprehensive system information
- **Error Handling**: Automatic retry logic and failure recovery
- **Serial Commands**: Interactive debugging and testing commands

### Network Configuration
station.setWiFiCredentials("SSID", "PASSWORD");
station.setAPIURL("https://your-api-url.com/api/weather/submit");

### Station Credentials
station.setStationCredentials(stationID, "passkey");

### Hardware Configuration
station.setDHTPin(32, DHT22);        // DHT sensor on GPIO 32
station.setStatusLEDPin(2);          // Status LED on GPIO 2
station.setWindPin(36);              // Wind sensor on GPIO 36

### Timing Configuration
station.setReadingInterval(3600000); // 1 hour in milliseconds
station.setTimeouts(30000, 15000);   // WiFi and HTTP timeouts

### Wind Sensor Calibration
station.setWindCalibration(3.3, 32.4); // 3.3V max = 32.4 MPH max

## Serial Commands
When connected to Serial Monitor, you can use these commands:
- `test` - Force an immediate sensor reading
- `info` - Display system information
- `wifi` - Show WiFi connection status
- `status` - Show current operational status
- `reading` - Show last sensor reading
- `sensors` - Test sensor readings
- `restart` - Restart the ESP32
- `help` - Show available commands

## Status LED Indicators
- **Slow blink (2s)**: Normal operation, connected
- **Fast blink (200ms)**: Connecting to WiFi
- **Very fast blink (100ms)**: Error condition
- **Triple flash**: Successful data submission

## API Integration
The library automatically formats and submits data to the CIJE Weather Hub API:
POST /api/weather/submit
Content-Type: application/x-www-form-urlencoded

station_id=1&passkey=YOUR_PASSKEY&temperature=72.5&humidity=45.2&wind_speed=5.3


## Error Handling
- Automatic WiFi reconnection
- Sensor reading validation
- HTTP retry logic
- Consecutive failure tracking
- Automatic restart after 5 consecutive failures


## Troubleshooting

### Common Issues

1. **Getting an error when trying to upload to the ESP32
   - Confirm the Upload speed (in Tools) is 115200
   - Make sure no wires are in GPIO 0, 2, 12 or 15
   - Not likely for this model ESP32, but may have to put the board in Boot mode
     	1.	Disconnect USB.
	   2.	Insert a jumper wire between GPIO0 ? GND.
	   3.	Reconnect USB while they are still connected.
	   4.	Immediately start upload (Tools ? Upload).
	   5.	When upload starts showing “Connecting…”, press and release RESET (if you have one).
	   6.	When flashing begins, you can remove the jumper.
   
2. **DHT sensor not responding**
   - Check wiring: VCC to 5V, GND to GND, DATA to GPIO 32
   - There are 3.3V pins and the DHT won't work

3. **WiFi connection fails**
   - Verify SSID and password are correct
   - Check signal strength
   - Ensure 2.4GHz network (ESP32 doesn't support 5GHz)

4. **API submission fails**
   - Verify station ID and passkey are correct
   - Check internet connectivity
   - Confirm API URL is accessible

5. **Wind sensor readings incorrect**
   - Calibrate using `setWindCalibration()`
   - Check analog pin connection (GPIO 36)
   - Verify sensor voltage range (0-5V)

## Library Structure

```
cije-weather-station/
+-- src/
¦   +-- CijeWeatherStation.h
¦   +-- CijeWeatherStation.cpp
+-- examples/
¦   +-- weather_station_esp32_basic_anemometer_test.ino
¦   +-- weather_station_esp32_basic_bme280.ino
¦   +-- weather_station_esp32_basic_bme280_test.ino
¦   +-- weather_station_esp32_basic_dht22.ino
¦   +-- weather_station_esp32_basic_dht22_test.ino
¦   +-- weather_station_esp32_library_anemometerpulse.ino
¦   +-- weather_station_esp32_library_dht22_test.ino
¦   +-- weather_station_library_esp32_basicdata_test.ino
¦   +-- weather_station_library_esp32_connection_test.ino
¦   +-- weather_station_uno_nrf24_receive_lcd_datalogging.ino
¦   +-- weather_station_uno_nrf24_transmit_dht22_voltage.ino
+-- data/
¦   +-- anemometer_data-V_vs_RPM.jpg
+-- docs/
¦   +-- Cool_Neighborhoods_NYC_Report.pdf
¦   +-- DHT_adafruit_primer.pdf
¦   +-- ESP32-layout.png
¦   +-- ESP32pinout.png
¦   +-- ESP32setup.pdf
¦   +-- Weather and Wireless communication Teacher Guide
¦   +-- Weather and Wireless Communication Teacher Guide.pdf
¦   +-- Weather and Wireless Communication.pdf
¦   +-- Weather Station Build BME280 Basic.pdf
¦   +-- Weather Station Build DHT22 Basic.pdf
¦   +-- Weather Station Build.pdf
¦   +-- weather_station_cije_anemometer.pdf
+-- library.properties
+-- keywords.txt
+-- README.md
```

## Version History

- **v1.0.0** - Initial release with DHT22, BME280 and wind sensor support

## License

MIT License - see LICENSE file for details.

## Support

For support and questions:
- GitHub Issues: https://github.com/cije/cije-weather-station/issues
- Email: support@cije.org
