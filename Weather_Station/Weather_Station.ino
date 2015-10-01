//Program: Multi Sensor (DHT22, BH1750, BMP180)
//Programmer: Kunal Lanjewar
//Copyright (c) 2015 Kunal Lanjewar.  All right reserved.


#include <SPI.h>
#include <MySensor.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Wire.h>
#include <BH1750.h>

//-----------------------Baro/DHT/Light--------------------------//
#define BARO_CHILD 0
#define CHILD_ID_HUM 1
#define CHILD_ID_TEMP 2
#define CHILD_ID_LIGHT 3
#define HUMIDITY_SENSOR_DIGITAL_PIN 3
#define LIGHT_SENSOR_ANALOG_PIN 0
unsigned long SLEEP_TIME = 30000; // Sleep time between reads (in milliseconds)


BH1750 lightSensor;
Adafruit_BMP085 bmp = Adafruit_BMP085();      // Digital Pressure Sensor

MySensor gw;

//-----------------------LUX---------------------------------------------------//

MyMessage msg(CHILD_ID_LIGHT, V_LIGHT_LEVEL);
uint16_t lastlux;

//----------Barometer---------------------------//
float lastPressure = -1;
int lastForecast = -1;
char *weather[] = {"stable", "sunny", "cloudy", "unstable", "thunderstorm", "unknown"};
int minutes;
float pressureSamples[180];
int minuteCount = 0;
bool firstRound = true;
float pressureAvg[7];
float dP_dt;
MyMessage pressureMsg(BARO_CHILD, V_PRESSURE);
MyMessage forecastMsg(BARO_CHILD, V_FORECAST);

//-----------DHT--------------------------------------//

DHT dht;
float lastTemp;
float lastHum;
boolean metric = true;
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);


//--------------------------------------------------//

void setup()
{
  gw.begin();

  gw.sendSketchInfo("Weather Station", "1.0");   // Send the Sketch Version Information to the Gateway


  //..........BaroMeter............//

  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    while (1) { }
  }

  // Register sensors to gw (they will be created as child devices)

  gw.present(BARO_CHILD, S_BARO);

  //-----------DHT--------------//

  dht.setup(HUMIDITY_SENSOR_DIGITAL_PIN);

  // Register all sensors to gw (they will be created as child devices)

  gw.present(CHILD_ID_HUM, S_HUM);
  gw.present(CHILD_ID_TEMP, S_TEMP);
  metric = gw.getConfig().isMetric;

  //----------------------LUX-----------------------------//

  gw.present(CHILD_ID_LIGHT, S_LIGHT_LEVEL);
  lightSensor.begin();

}

void loop()
{

  //-----------Barometer-----------------------------------//

  float pressure = bmp.readSealevelPressure(205) / 100; // 205 meters above sealevel

  int forecast = sample(pressure);

  Serial.print("Pressure = ");
  Serial.print(pressure);
  Serial.println(" Pa");
  Serial.println(weather[forecast]);

  if (pressure != lastPressure) {
    gw.send(pressureMsg.set(pressure, 0));
    lastPressure = pressure;
  }
  
    if (forecast != lastForecast) {
      gw.send(forecastMsg.set(weather[forecast]));
      lastForecast = forecast;
   
}

/*
 DP/Dt explanation

 0 = "Stable Weather Pattern"
 1 = "Slowly rising Good Weather", "Clear/Sunny "
 2 = "Slowly falling L-Pressure ", "Cloudy/Rain "
 3 = "Quickly rising H-Press",     "Not Stable"
 4 = "Quickly falling L-Press",    "Thunderstorm"
 5 = "Unknown (More Time needed)
*/

//----------DHT--------------------//

delay(dht.getMinimumSamplingPeriod());

float temperature = dht.getTemperature();
if (isnan(temperature)) {
  Serial.println("Failed reading temperature from DHT");
} else if (temperature != lastTemp) {
  lastTemp = temperature;
  if (!metric) {
    temperature = dht.toFahrenheit(temperature);
  }
  gw.send(msgTemp.set(temperature, 1));
  Serial.print("T: ");
  Serial.println(temperature);
}

float humidity = dht.getHumidity();
if (isnan(humidity)) {
  Serial.println("Failed reading humidity from DHT");
} else if (humidity != lastHum) {
  lastHum = humidity;
  gw.send(msgHum.set(humidity, 1));
  Serial.print("H: ");
  Serial.println(humidity);
}

//-----------------------LUX---------------------------------//
uint16_t lux = lightSensor.readLightLevel();// Get Lux value
Serial.println(lux);
if (lux != lastlux) {
  gw.send(msg.set(lux));
  lastlux = lux;
}

//------------------------------------------------------------//

gw.sleep(SLEEP_TIME); //sleep a bit


}


//-------------------- Barometer------------------------//

int sample(float pressure) {
	// Algorithm found here
	// http://www.freescale.com/files/sensors/doc/app_note/AN3914.pdf
	if (minuteCount == 180)
		minuteCount = 5;

	pressureSamples[minuteCount] = pressure;
	minuteCount++;

	if (minuteCount == 5) {
		// Avg pressure in first 5 min, value averaged from 0 to 5 min.
		pressureAvg[0] = ((pressureSamples[0] + pressureSamples[1]
				+ pressureSamples[2] + pressureSamples[3] + pressureSamples[4])
				/ 5);
	} else if (minuteCount == 35) {
		// Avg pressure in 30 min, value averaged from 0 to 5 min.
		pressureAvg[1] = ((pressureSamples[30] + pressureSamples[31]
				+ pressureSamples[32] + pressureSamples[33]
				+ pressureSamples[34]) / 5);
		float change = (pressureAvg[1] - pressureAvg[0]);
		if (firstRound) // first time initial 3 hour
			dP_dt = ((65.0 / 1023.0) * 2 * change); // note this is for t = 0.5hour
		else
			dP_dt = (((65.0 / 1023.0) * change) / 1.5); // divide by 1.5 as this is the difference in time from 0 value.
	} else if (minuteCount == 60) {
		// Avg pressure at end of the hour, value averaged from 0 to 5 min.
		pressureAvg[2] = ((pressureSamples[55] + pressureSamples[56]
				+ pressureSamples[57] + pressureSamples[58]
				+ pressureSamples[59]) / 5);
		float change = (pressureAvg[2] - pressureAvg[0]);
		if (firstRound) //first time initial 3 hour
			dP_dt = ((65.0 / 1023.0) * change); //note this is for t = 1 hour
		else
			dP_dt = (((65.0 / 1023.0) * change) / 2); //divide by 2 as this is the difference in time from 0 value
	} else if (minuteCount == 95) {
		// Avg pressure at end of the hour, value averaged from 0 to 5 min.
		pressureAvg[3] = ((pressureSamples[90] + pressureSamples[91]
				+ pressureSamples[92] + pressureSamples[93]
				+ pressureSamples[94]) / 5);
		float change = (pressureAvg[3] - pressureAvg[0]);
		if (firstRound) // first time initial 3 hour
			dP_dt = (((65.0 / 1023.0) * change) / 1.5); // note this is for t = 1.5 hour
		else
			dP_dt = (((65.0 / 1023.0) * change) / 2.5); // divide by 2.5 as this is the difference in time from 0 value
	} else if (minuteCount == 120) {
		// Avg pressure at end of the hour, value averaged from 0 to 5 min.
		pressureAvg[4] = ((pressureSamples[115] + pressureSamples[116]
				+ pressureSamples[117] + pressureSamples[118]
				+ pressureSamples[119]) / 5);
		float change = (pressureAvg[4] - pressureAvg[0]);
		if (firstRound) // first time initial 3 hour
			dP_dt = (((65.0 / 1023.0) * change) / 2); // note this is for t = 2 hour
		else
			dP_dt = (((65.0 / 1023.0) * change) / 3); // divide by 3 as this is the difference in time from 0 value
	} else if (minuteCount == 155) {
		// Avg pressure at end of the hour, value averaged from 0 to 5 min.
		pressureAvg[5] = ((pressureSamples[150] + pressureSamples[151]
				+ pressureSamples[152] + pressureSamples[153]
				+ pressureSamples[154]) / 5);
		float change = (pressureAvg[5] - pressureAvg[0]);
		if (firstRound) // first time initial 3 hour
			dP_dt = (((65.0 / 1023.0) * change) / 2.5); // note this is for t = 2.5 hour
		else
			dP_dt = (((65.0 / 1023.0) * change) / 3.5); // divide by 3.5 as this is the difference in time from 0 value
	} else if (minuteCount == 180) {
		// Avg pressure at end of the hour, value averaged from 0 to 5 min.
		pressureAvg[6] = ((pressureSamples[175] + pressureSamples[176]
				+ pressureSamples[177] + pressureSamples[178]
				+ pressureSamples[179]) / 5);
		float change = (pressureAvg[6] - pressureAvg[0]);
		if (firstRound) // first time initial 3 hour
			dP_dt = (((65.0 / 1023.0) * change) / 3); // note this is for t = 3 hour
		else
			dP_dt = (((65.0 / 1023.0) * change) / 4); // divide by 4 as this is the difference in time from 0 value
		pressureAvg[0] = pressureAvg[5]; // Equating the pressure at 0 to the pressure at 2 hour after 3 hours have past.
		firstRound = false; // flag to let you know that this is on the past 3 hour mark. Initialized to 0 outside main loop.
	}

	if (minuteCount < 35 && firstRound) //if time is less than 35 min on the first 3 hour interval.
		return 5; // Unknown, more time needed
	else if (dP_dt < (-0.25))
		return 4; // Quickly falling LP, Thunderstorm, not stable
	else if (dP_dt > 0.25)
		return 3; // Quickly rising HP, not stable weather
	else if ((dP_dt > (-0.25)) && (dP_dt < (-0.05)))
		return 2; // Slowly falling Low Pressure System, stable rainy weather
	else if ((dP_dt > 0.05) && (dP_dt < 0.25))
		return 1; // Slowly rising HP stable good weather
	else if ((dP_dt > (-0.05)) && (dP_dt < 0.05))
		return 0; // Stable weather
	else
		return 5; // Unknown
}
