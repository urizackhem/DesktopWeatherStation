/* 
 Weather Shield Example
 By: Nathan Seidle
 SparkFun Electronics
 Date: November 16th, 2013
 Modified: Uri ZACKHEM, August 2015
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 Much of this is based on Mike Grusin's USB Weather Board code: https://www.sparkfun.com/products/10586
 
 This code reads all the various sensors (wind speed, direction, rain gauge, humidty, pressure, light, batt_lvl)
 and reports it over the serial comm port. This can be easily routed to an datalogger (such as OpenLog) or
 a wireless transmitter (such as Electric Imp).
 
 Measurements are reported once a second but windspeed and rain gauge are tied to interrupts that are
 calcualted at each report.
 
 This example code assumes the GPS module is not used.
 
 */
#include <math.h>
#include <Wire.h> //I2C needed for sensors
#include <LiquidCrystal.h>
#include "MPL3115A2.h" //Pressure sensor
#include "HTU21D.h" //Humidity sensor
LiquidCrystal lcd(12,11,5,4,3,2);
MPL3115A2 myPressure; //Create an instance of the pressure sensor
HTU21D myHumidity; //Create an instance of the humidity sensor

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

float humidity = 0; // [%]
float tempf = 0; // [temperature C]
float pressure = 0; // Pascal
float dewptf; // [dewpoint C] - It's hard to calculate dewpoint locally, do this in the agent
float heat_index;// Deg C.
int   heat_colour;// 0-4

void setup()
{
  //Configure the pressure sensor
  myPressure.begin(); // Get sensor online
  myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  myPressure.setOversampleRate(7); // Set Oversample to the recommended 128
  myPressure.enableEventFlags(); // Enable all three pressure and temp event flags 
  //Configure the humidity sensor
  myHumidity.begin();
  lcd.begin(16, 2);
  lcd.clear();
  int   Countdown   = 3;// Wait
  while(Countdown >= 0)
  {
     Countdown--;
     delay(500);
  }
  // Just read to clean spurious measurements
  calcWeather();
  if(Serial)
    Serial.begin(9600);
}

void loop()
{
  printWeather();
  LCDweather();
  delay(6000);
}

float Celsius2Fahrenheit(float cin)
{
  float Fahrenheit = cin * 1.8 + 32.0;
  return Fahrenheit; 
}

float Fahrenheit2celcius(float fin)
{
   float Celsius = (fin - 32.0) / 1.8;
   return Celsius;
}

// http://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml

float SimpleHI(float fin , float rh)// RH in %
{
  float HI = 0.5 * (fin + 61.0 + ((fin-68.0)*1.2) + rh * 0.094);
  return HI;
}

float Rothfusz(float t , float rh)// RH in %, t in Fahrenheit
{
  float tt   = t * t;
  float rhrh = rh * rh;
  float HI = -42.379 + 2.04901523*t + 10.14333127*rh - .22475541*t*rh - .00683783*tt - .05481717*rhrh + .00122874*tt*rh + .00085282*t*rhrh - .00000199*tt*rhrh;
  if(rh < 13.0 && t > 80.0 && t < 112.0){
    float adj = 0.25 * (13-rh) * sqrt( (17.0 - abs(t - 95.0)) / 17.0);
    HI = HI - adj;
  }else if(rh > 85 && t > 80.0 && t < 87){
    float adj = 0.1 * (rh - 85) * 0.2 * (87.0 - t);
    HI = HI + adj;
  }
  return HI;
}

float HeatIndex(float cin , float rh)// RH in %
{
  float fin = Celsius2Fahrenheit(cin);
  float HI = SimpleHI(fin , rh);
  if(HI >= 80.0){
    HI = Rothfusz(fin , rh);
  }
  float HIcelsius = Fahrenheit2celcius(HI);
  return HIcelsius;
}

int   HeatIndex2Danger(float hicelsius) // 0-4, 0 no danger
{
  if (hicelsius < 27.0)
    return 0;
  if (hicelsius < 32.0)
    return 1;
   if (hicelsius < 41.0)
     return 2;
   if (hicelsius < 54.0)
      return 3;
   return 4;
}

//Calculates each of the variables that wunderground is expecting
void calcWeather()
{
  //Calc humidity
  humidity = myHumidity.readHumidity();
  tempf    = myPressure.readTemp();
  pressure = myPressure.readPressure();
  // http://andrew.rsmas.miami.edu/bmcnoldy/Humidity.html
  dewptf = 243.04 * (log(humidity * 0.01) + ( ( 17.625 * tempf) / ( 243.04 + tempf))) / (17.625 - log(humidity * 0.01) - ((17.625 * tempf)/(243.04 + tempf)));
  heat_index  = HeatIndex(tempf , humidity);
  heat_colour = HeatIndex2Danger(heat_index);
}
// The pressure sensor is not great...
// You may want to change it. Compare the reading to local 
// meteorologically certified data.
#define PRESSURE_FIX   5.0

void LCDweather()
{
  lcd.clear();
  lcd.setCursor(0,0);
  int  p_lcd = pressure * 0.01 + PRESSURE_FIX;
  int  t_lcd = tempf + 0.5;
  int  h_lcd = humidity + 0.5;
  int  dplcd = dewptf + 0.5;
  int  hilcd = heat_index + 0.5;
  String toprow = String(p_lcd) + "mb " + String(t_lcd) + String((char)223) + String("C ") + h_lcd + String("%");
  lcd.print(toprow);
  delay(300);
  lcd.setCursor(0,1);
  String botrow = String("Feels like ") + hilcd + String((char)223) + String("C ");
  lcd.print(botrow);
}
//Prints the various variables directly to the port
//I don't like the way this function is written but Arduino doesn't support floats under sprintf
void printWeather()
{
  char tbuff[10] , pbuff[10] , hbuff[10] , hpcbuff[10];
  char fealslike[10];
  calcWeather(); //Go calc all the various sensors
  dtostrf(tempf           , 5 , 1 , tbuff);
  dtostrf(pressure * 0.01 + PRESSURE_FIX , 6 , 1 , pbuff);
  dtostrf(humidity        , 4 , 1 , hpcbuff);
  dtostrf(dewptf          , 5 , 1 , hbuff);
  dtostrf(heat_index      , 5 , 1 , fealslike);
  String s = "H=";
  s += (char*)hpcbuff;
  s += "|T=";
  s += (char*)tbuff;
  s += "|P=";
  s += (char*)pbuff;
  s += "|Tdew=";
  s += hbuff;
  s += "|Feels like:";
  s += fealslike;
  if(Serial)
    Serial.println(s);
}



