#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include "timer.h"

#define PWMPin 10

Adafruit_ADS1115 ads;
Timer<2> timer; // Timer with 2 task slots

void Halt();
void StartMeasurement(int current);
double ReadVoltage();
double ReadCurrent();
void secondTick(void *);
void measureESR();

void setup(void)
{
  pinMode(PWMPin, OUTPUT);
  Serial.begin(115200);
  Serial.println("Hello!");
  
  //Serial.println("Getting differential reading from AIN0 (P) and AIN1 (N)");
  //Serial.println("ADC Range: +/- 6.144V (1 bit = 3mV/ADS1015, 0.1875mV/ADS1115)");
  
  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  // Setting these values incorrectly may destroy your ADC!
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  
  ads.begin();
  analogWrite(PWMPin, 1);
  TCCR1A = _BV(COM1A1) | _BV(COM1B1)  /* non-inverting PWM */
        | _BV(WGM11);                 /* mode 14: fast PWM, TOP=ICR1 */
  TCCR1B = _BV(WGM13) | _BV(WGM12)
        | _BV(CS10);                  /* prescaler 1 */
  ICR1 = 6500;                        /* TOP counter value (freeing OCR1A*/


  //timer.every(20000, measureESR);
  timer.every(1000, secondTick);
}

int outputSignal = 2230;
double milliwatthours = 0.0;

double volts = 0.0;
double amps = 0.0;
double esr = 0.0;

double ReadVoltage()
{
  double multiplier = 0.1875;
  ads.setGain(GAIN_TWOTHIRDS);
  int16_t results = ads.readADC_Differential_0_1();
  double volts = results * multiplier;
  if(volts < 0.0)
  {
    return -volts;
  }
  return volts;
}

double ReadCurrent()
{
  double currentMultiplier = 0.078125;
  ads.setGain(GAIN_SIXTEEN);
  int16_t currentResults = ads.readADC_Differential_2_3();
  if(abs(currentResults) < 4)
  {
    return 0.0;
  }
  double amps = currentResults * currentMultiplier;
  if(amps < 0.0)
  {
    return -amps;
  }
  return amps;
}

void measureESR()
{
  double loadVolts = volts;
  double loadAmps = amps;
  analogWrite(PWMPin, 0);
  delay(800);
  double voltsDifference = ReadVoltage() - loadVolts;
  esr = (voltsDifference * 1000.0d) / loadAmps;
  analogWrite(PWMPin, outputSignal);
}

int counter = 0;
unsigned long dischargeTime = 0;
void secondTick(void *)
{
  milliwatthours += volts * amps * 0.000000277777777777777777777777777d;
  counter++;
  dischargeTime++;
  if(counter == 20)
  {
    counter = 0;
    measureESR();
  }
}

void loop(void)
{
  timer.tick();

  volts = ReadVoltage();
  amps = ReadCurrent();

  StaticJsonDocument<64> doc;
  doc["esr"] = esr;
  doc["h"] = milliwatthours;
  doc["U"] = volts;
  doc["I"] = amps;
  doc["T"] = dischargeTime;
  serializeJson(doc, Serial);
  Serial.println("");
  
  if(amps < 960.0){
    outputSignal++;
  }else if(amps > 1040.0){
    outputSignal--;
  }
  if(volts < 3000)
  {
    outputSignal=0;
  }
  analogWrite(PWMPin, outputSignal);
}
