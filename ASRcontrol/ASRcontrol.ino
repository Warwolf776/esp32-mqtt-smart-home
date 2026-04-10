#include <Wire.h>
#include <WiFi.h>         
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "ASR_module.h"
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DHT_PIN 19
#define TRIG_PIN 17
#define ECHO_PIN 18
#define DHTTYPE DHT11
#define OLED_RESET    -1
#define SDA_PIN 33
#define SCL_PIN 32
// ------------------ 舵机 ------------------
Servo airServo;
const int servoPin = 12; // GPIO接舵机信号线
bool acPower = false;    // 空调开关
bool isHeat = false;     // 制热/制冷
int acTemp = 25;         // 温度
// ------------------ 光敏 ------------------
#define LDR_PIN 34  // 光敏连接的GPIO
static unsigned long lastLDRTime = 0;
const unsigned long LDR_INTERVAL = 5000; // 每 2 秒发送一次
const int LIGHT_THRESHOLD = 500; // 可根据实际亮度调整
int lightValue = 0;
// ------------------ 按钮 ------------------
bool pageFlag = false;            // false: 首页, true: 空调+光强页
unsigned long lastButtonDebounce = 0;
const unsigned long BUTTON_DEBOUNCE_DELAY = 50; // 消抖
#define BUTTON_PIN 14

TwoWire Wire_OLED = TwoWire(1);
DHT dht(DHT_PIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire_OLED, OLED_RESET);
unsigned long lastDHTTime = 0;
unsigned long lastDistTime = 0;
float temperature = 0;
float humidity = 0;
float distance_cm = 0;
const unsigned long DHT_INTERVAL = 5000;
const unsigned long DIST_INTERVAL = 5000;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned int water=1;
unsigned long lastOLEDTime = 0;
const unsigned long OLED_INTERVAL = 1000;

int led_pin =2;
const char* ssid = "warwolf";
const char* password = "88888888";

String clientId = "ESP32_ASR_" + String((uint32_t)ESP.getEfuseMac(), HEX);
const char* mqtt_server ="h130af33.ala.cn-hangzhou.emqxsl.cn";
const int mqtt_port =8883;
const char* mqtt_user = "wzt";
const char* mqtt_pass = "88888888";
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----


)EOF";
WiFiClientSecure espClient;  // TLS 客户端
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("收到主题: ");
  Serial.print(topic);
  Serial.print(" 消息: ");
  Serial.println(message);

  // 判断是否是控制 LED 的主题
  if (String(topic) == "home/light") {
    message.trim();
    if (message == "1") {
      digitalWrite(led_pin, HIGH);
      Serial.println("LED 已打开");
    } else if (message == "0") {
      digitalWrite(led_pin, LOW);
      Serial.println("LED 已关闭");
    } else {
      Serial.println("未知命令");
    }
  }
  if(String(topic)=="home/water"){
    message.trim();
    water=message.toInt();
  }
  if(String(topic) == "home/fan/fun"){
    if(message == "0") isHeat = true;
    else if(message == "1") isHeat = false;
  }
  else if(String(topic) == "home/fan/power"){
    if(message == "1") acPower = true;
    else acPower = false;
  }
  else if(String(topic) == "home/fan"){
    int tempVal = message.toInt();
    if(tempVal >=17 && tempVal <=30) acTemp = tempVal;
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("尝试连接 EMQX MQTT...");
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("连接成功！");
      client.setCallback(callback);
      client.subscribe("home/light");
      client.subscribe("home/water");
      client.subscribe("home/fan/fun");
      client.subscribe("home/fan/power");
      client.subscribe("home/fan");
    } else {
      Serial.print("失败，rc=");
      Serial.print(client.state());
      Serial.println(" 5秒后重试...");
      delay(5000);
    }
  }
}

void updateServo() {
  int angle = 90; // 默认中间
  if(acPower){
    angle = mapTemperatureToAngle(acTemp, isHeat);
  }
  airServo.write(angle);
}

void setupOLED() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // 根据扫描结果确认地址
    Serial.println(F("SSD1306 初始化失败"));
    for(;;); // 停在这里
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("OLED Ready!");
  display.display();
  delay(500);
}

void displayPage() {
    display.clearDisplay();

    if (!pageFlag) {
        // 第一页：温湿水位
        display.setTextSize(1);
        display.setCursor(0,0); display.print("Temp:");
        display.setTextSize(2);
        display.setCursor(40,0); display.print(temperature,1); display.print("C");

        display.setTextSize(1);
        display.setCursor(0,20); display.print("Hum:");
        display.setTextSize(2);
        display.setCursor(40,20); display.print(humidity,1); display.print("%");

        display.setTextSize(1);
        display.setCursor(0,40); display.print("Water:");
        display.setTextSize(2);
        display.setCursor(40,40); display.print(distance_cm,0); display.print("cm");

        display.setTextSize(1);
        display.setCursor(0,56); display.print("Th:"); display.print(water);
        display.setCursor(80,56); display.print(distance_cm <= water ? "ALARM" : "SAFE");

    } else {
        // 第二页：空调状态 + 光敏，更醒目显示
        display.setTextSize(1);
        display.setCursor(0,0); display.print("AC Power:");
        display.setTextSize(2);
        display.setCursor(70,0); display.print(acPower ? "ON" : "OFF");

        display.setTextSize(1);
        display.setCursor(0,20); display.print("Mode:");
        display.setTextSize(2);
        display.setCursor(70,20); display.print(isHeat ? "Heat" : "Cool");

        display.setTextSize(1);
        display.setCursor(0,40); display.print("Temp Set:");
        display.setTextSize(2);
        display.setCursor(70,40); display.print(acTemp);

        display.setTextSize(1);
        display.setCursor(0,56); display.print("LightVal:");
        display.setTextSize(1);
        display.setCursor(70,56); display.print(lightValue);
    }

    display.display();
}

int mapTemperatureToAngle(int temp, bool isHeat) {
  // temp 范围 17~30
  if(isHeat){
    // 制热 17~30 映射到 90~180
    return map(temp, 17, 30, 90, 180);
  } else {
    // 制冷 17~30 映射到 0~90
    return map(temp, 17, 30, 0, 90);
  }
}
void checkButton() {
    static bool buttonState = HIGH;       // 当前稳定状态
    static bool lastReading = HIGH;       // 上次读取值
    static unsigned long lastDebounceTime = 0;

    bool reading = digitalRead(BUTTON_PIN);

    // 检测按键状态变化，开始计时
    if (reading != lastReading) {
        lastDebounceTime = millis();
    }

    // 如果状态保持稳定超过消抖时间
    if ((millis() - lastDebounceTime) > BUTTON_DEBOUNCE_DELAY) {
        // 如果稳定状态变化了
        if (reading != buttonState) {
            buttonState = reading;
            // 只在按下瞬间触发翻页
            if (buttonState == LOW) {
                pageFlag = !pageFlag; // 翻页
                Serial.print("翻页: ");
                Serial.println(pageFlag ? "空调页" : "首页");
            }
        }
    }

    lastReading = reading; // 保存本次读取值
}
ASR_MOUDLE asr;
uint8_t result = 0;

void setup()
{
  Serial.begin(115200);
  delay(300);  // 给串口一点时间稳定
  Wire_OLED.begin(SDA_PIN, SCL_PIN); 
  airServo.attach(servoPin, 500, 2400); // 最小脉宽500us, 最大脉宽2400us
  airServo.write(90); // 默认中位角
  //setupOLED();
  pinMode(BUTTON_PIN, INPUT_PULLUP); 
 if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, &Wire_OLED)) {
    Serial.println("OLED 初始化失败！检查接线/地址/上拉电阻");
    // 可以加 while(1); 如果想卡住 debug
  } else {
    Serial.println("OLED 初始化成功（用第二总线）");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OLED on second I2C OK!");
    display.display();
    delay(1000);
  }

  // 继续原来的其他初始化：dht.begin(); pinMode 等...
  // ASR 部分不用动，它用 Wire

  Serial.println("初始化完成");


  pinMode(led_pin,OUTPUT);
  Serial.println("Start - Trying to connect WiFi...");
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect failed (timeout). Continuing without WiFi...");
  }

  
  espClient.setCACert(ca_cert);
  client.setServer(mqtt_server, mqtt_port);
   
  Serial.println("DHT11 测试开始...");
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);    // Trig 输出
  pinMode(ECHO_PIN, INPUT);     // Echo 输入



}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  checkButton();
  result = asr.rec_recognition();
  if (result != 0) {
    switch (result) {
      case 1:   Serial.println("go");        break;
      case 2:   Serial.println("back");      break;
      case 3:   Serial.println("left");      break;
      case 4:   Serial.println("right");     break;
      case 9:   Serial.println("stop");      break;
      case 0x12: Serial.println("light on");
      digitalWrite(led_pin,HIGH);            break;  
      case 0x13: Serial.println("light off");
      digitalWrite(led_pin,LOW);             break;     
      case 0x80: Serial.println("fan on");
        acPower = true;                      break;
      case 0x81: Serial.println("fan off"); 
      acPower = false;                       break;
      default:
        Serial.print("Unknown result: 0x");
        Serial.println(result, HEX);
        break;
    }
  }
  
 
  unsigned long currentTime = millis();
  if (currentTime - lastDHTTime >= DHT_INTERVAL) {
    lastDHTTime = currentTime;   // 更新时间戳

    humidity = dht.readHumidity();
    temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      Serial.print("湿度: ");
      Serial.print(humidity);
      Serial.print(" %\t 温度: ");
      Serial.print(temperature);
      Serial.println(" °C");

      
      char tempStr[8];
      char humpStr[8];
      dtostrf(temperature, 1, 1, tempStr);
      dtostrf(humidity, 1, 1, humpStr);    
      client.publish("home/temperature", tempStr,true);
      client.publish("home/humidity", humpStr,true);
      
    
    }
 }
 if (currentTime - lastDistTime >= DIST_INTERVAL) {
    lastDistTime = currentTime;

    // 触发超声波（标准流程：低→高10us→低）
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(3);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // 读取Echo高电平持续时间（微秒）
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // 超时30ms，防卡死（约5m距离）
    // 计算距离（cm）：声速 ≈ 0.0343 cm/us，往返除2
    distance_cm = (duration * 0.0343) / 2.0;

    // 检查有效值（duration=0 或太小表示无回波）
    if (duration == 0 || distance_cm > 400 || distance_cm < 2) {
      Serial.println("HC-SR04: 无有效距离（超出范围或无物体）");
    } else {
      Serial.print("距离: ");
      Serial.print(distance_cm);
      Serial.println(" cm");
      char distStr[10];
      dtostrf(distance_cm, 1, 1, distStr);
      client.publish("home/distance", distStr,true);
      if(distance_cm<=water)
      {
        asr.speak(0xFF, 0x05);
      }
    }
  }
  
  if (millis() - lastOLEDTime > OLED_INTERVAL) {
  lastOLEDTime = millis();
    displayPage();    // 根据 pageFlag 显示对应页面
    updateServo();    // 舵机控制
   }
   if(currentTime - lastLDRTime >= LDR_INTERVAL){
    lastLDRTime = currentTime;

    lightValue = analogRead(LDR_PIN);  // 0~4095
    Serial.print("光敏值: "); Serial.println(lightValue);

    // 发送亮度值到主题
    char lightStr[6];
    itoa(lightValue, lightStr, 10);
    client.publish("home/lightpower", lightStr, true);

    // 可选：根据阈值判断开关灯状态
    if(lightValue < LIGHT_THRESHOLD){
        Serial.println("光线暗，建议开灯");
    } else {
        Serial.println("光线亮，建议关灯");
    }
    checkButton();   // 检测翻页按钮
    displayPage();   // 根据 pageFlag 显示对应页面
}

}
