#include <OneWire.h>
#include <DallasTemperature.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ir_Haier.h>

#define IR_PIN 5   // D1
#define TEMP_PIN 4 // D2
#define LED 13

// WIFI账号密码
const char *ssid = "lot-ap";
const char *password = "xulongzhe";
// 时区偏移
const long utcOffsetInSeconds = 8 * 3600;

// 启动时间
int start_hour = 7;
int start_minute = 15;
// 关闭时间
int off_hour = 7;
int end_minute = 30;
// 温度阈值
int maxTemprature = 15;

// ntp
WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, utcOffsetInSeconds);

// 空调控制
bool isAcOn = false;
IRHaierACYRW02 haier(IR_PIN);

// 温度检测
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

void setup(void)
{
  Serial.begin(9600);
  Serial.println("start");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  ntp.begin();
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  haier.begin();
  sensors.begin();
}

void loop(void)
{
  // update();
  // float tempC = getTemprature();
  // if (isTimeUp(start_hour, start_minute) && tempC != DEVICE_DISCONNECTED_C && tempC < maxTemprature)
  // {
  //   acOn();
  // }
  // else if (isTimeUp(off_hour, end_minute))
  // {
  //   acOff();
  // }
  // delay(10000);
  acOn();
  delay(10000);
  acOff();
  delay(10000);
}

void acOn()
{
  if (isAcOn)
  {
    return;
  }
  Serial.println("AC on");
  haier.setFan(kHaierAcFanAuto);
  haier.setMode(kHaierAcHeat);
  haier.on();
  haier.send();
  isAcOn = true;
}

void acOff()
{
  if (!isAcOn)
  {
    return;
  }
  Serial.println("AC off");
  haier.off();
  haier.send();
  isAcOn = false;
}

void update()
{
  while (!ntp.update())
  {
    ntp.forceUpdate();
  }
}

bool isTimeUp(int hour, int minute)
{
  Serial.println("current time: " + ntp.getFormattedTime());
  return hour == ntp.getHours() && minute == ntp.getMinutes();
}

float getTemprature()
{
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures();
  Serial.println("DONE");
  float tempC = sensors.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C)
  {
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.println(tempC);
    return tempC;
  }
  else
  {
    Serial.println("Error: Could not read temperature data");
    return DEVICE_DISCONNECTED_C;
  }
}