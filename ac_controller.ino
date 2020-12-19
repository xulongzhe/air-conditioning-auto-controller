#include <OneWire.h>
#include <DallasTemperature.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ir_Haier.h> // 此处是海尔空调，可以根据自己空调型号自行更换

#define IR_PIN 5   // 红外发光二极管正极引脚 (D1)
#define TEMP_PIN 4 // 温度传感器引脚 (D2)

/********************* 配置项 START ************************/

// 空调启动时间
const int start_hour = 7;
const int start_minute = 10;

// 空调关闭时间
const int end_hour = 7;
const int end_minute = 40;

// 温度阈值，低于这个温度才会启动空调
const int maxTemprature = 15;

// 空调温度
const int acTemp = 26;

// WIFI账号密码
const char *ssid = "lot-ap";
const char *password = "xulongzhe";

// 时区偏移(东八区)
const long utcOffsetInSeconds = 8 * 3600;

/********************* 配置项 END ************************/

// 我的为海尔中央空调，因此使用的
// 你可以根据自己的空调型号切换
IRHaierACYRW02 airCond(IR_PIN);

// 空调当前状态
bool isAcOn = false;

// ntp
WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, utcOffsetInSeconds);

// 温度检测
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// Web服务器
ESP8266WebServer server(80);

// 计时器, 最近一次触发的时间（毫秒)
unsigned long lastTriggerTime = millis();

void setup(void)
{
  // 启动串口
  Serial.begin(9600);

  // 初始化板载LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // 启动WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // 等待WIFI就绪
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // 启动NTP客户端
  ntp.begin();

  // 注册web请求处理函数
  registerHttpHandlers();

  // 启动mDNS, 直接在浏览器输入http://ac-ctrl即可访问webui
  MDNS.begin("ac-ctrl"); 

  // 启动web服务器
  server.begin();

  // 启动空调控制器
  airCond.begin();

  // 启动温度传感器
  sensors.begin();
}

void loop(void)
{
  server.handleClient();
  MDNS.update();
  scheduleBySeconds(10);
}

void acOn(bool force = false)
{
  if (!force && isAcOn)
  {
    return;
  }
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("AC on, time: " + ntp.getFormattedTime());
  airCond.setFan(kHaierAcFanAuto);
  airCond.setMode(kHaierAcHeat);
  airCond.setTemp(acTemp);
  airCond.on();
  airCond.send();
  isAcOn = true;
  digitalWrite(LED_BUILTIN, HIGH);
}

void acOff(bool force = false)
{
  if (!force && !isAcOn)
  {
    return;
  }
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("AC off, time: " + ntp.getFormattedTime());
  airCond.off();
  airCond.send();
  isAcOn = false;
  digitalWrite(LED_BUILTIN, HIGH);
}

/* 注册web请求处理器 */
void registerHttpHandlers()
{
  // 主页
  server.on("/", []() {
    server.send(200, "text/html", R"===(
    <html>
      <head>
        <meta http-equiv="content-type" content="text/html;charset=UTF-8">
        <title>Hello ESP8266</title>
      </head> 
      <body>
        <ul>
          <li><a href='/on'>开空调</a></li>
          <li><a href='/off'>关空调</a></li>
          <li><a href='/temp'>温度</a></li>
          <li><a href='/time'>时间</a></li>
        </ul>
      </body>
    </html>
   )===");
  });
  // 开空调
  server.on("/on", []() {
    acOn(true);
    server.send(200, "text/plain", "ok");
  });
  // 关空调
  server.on("/off", []() {
    acOff(true);
    server.send(200, "text/plain", "ok");
  });
  // 查看当前时间
  server.on("/time", []() {
    ntpUpdate();
    String now = ntp.getFormattedTime();
    server.send(200, "text/plain", now);
  });
  // 查看当前温度
  server.on("/temp", []() {
    float tempC = fetchTemperature();
    server.send(200, "text/plain", String(tempC));
  });
}

void switchAC()
{
  ntpUpdate();

  // 周末不启动
  if (ntp.getDay() == 0 || ntp.getDay() == 6)
  {
    Serial.println("Today is weekday, skip");
    return;
  }

  if (ntp.getHours() == start_hour && ntp.getMinutes() == start_minute)
  {
    float tempC = fetchTemperature();
    if (tempC != DEVICE_DISCONNECTED_C && tempC < maxTemprature)
    {
      acOn();
    }
  }
  else if (ntp.getHours() == end_hour && ntp.getMinutes() == end_minute)
  {
    acOff();
  }
}

void scheduleBySeconds(unsigned long second)
{
  unsigned long now = millis();
  if (now - lastTriggerTime > second * 1000)
  {
    switchAC();
    lastTriggerTime = now;
  }
}

void ntpUpdate()
{
  while (!ntp.update())
  {
    ntp.forceUpdate();
  }
}

float fetchTemperature()
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