// Header File include
#include <SoftwareSerial.h>     // WIFI 통신
#include <Wire.h>
#include <AHTxx.h>              // AHT21
#include "SparkFun_ENS160.h"

// Pin define
#define TX 4
#define RX 5
#define Echo_pin 8
#define Trig_pin 9

// Class define
SoftwareSerial espSerial(TX, RX); // TX 4, RX 5
SparkFun_ENS160 ens160;
AHTxx aht(AHTXX_ADDRESS_X38, AHT2x_SENSOR);

// Variable declare
bool state = 0;                     // Power mode(1 - On, 0 - Off)
bool preState = 0;                  // Last state value
bool pendingState = 0;              // Save last power mode
bool isPending = 0;                 // 3sec waiting
unsigned long transitionStart = 0;  // Start time
float humidity;                     // AHT21 - Humidity
float temperature;                  // AHT21 - Temperature
int aqi;                            // AHT21 - AQI
int brightness = 0;                 // CDS - Brightness
float cycletime;                    // HC-SR04 - Ultrasonic shoot time
float distance;                     // HC-SR04 - distance to stuff
bool isDiff = false;                // Temperature/Humidity/AQI compare to last value
String payload;                     // Data store string
String on = "ON";
String off = "OFF";

// ⬇️ WIFI&TCP/IP settings
const char* SSID = "abcd";                // WIFI SSID
const char* PASSWORD = "abcdabcd";        // WIFI PASSWORD
const char* SERVER_IP = "192.168.0.2";    // Jetson Nano Server IP
// const char* SERVER_IP = "192.168.0.4";    // Test Server IP(Virtual Machine + Port Forwarding)
const int SERVER_PORT = 9000;             // Server access PORT number\

// Function declare
void getAirCondition();                   // Get Humidity&Temperature by DHT11
void getDistance();                       // Get distance of stuff
void getBrightness();                     // Get brightness of room
void sendAT(String cmd, int timeout);     // ESP8266 Control
void sendData(String cmd, int len);       // Data throw

void setup() {
  Serial.begin(9600);         // Serial Monitor 
  espSerial.begin(9600);      // ESP8266 Serial
  pinMode(Trig_pin, OUTPUT);  // Ultrasonic Sensor(throw)
  pinMode(Echo_pin, INPUT);   // Ultrasonic Sensor(receive)
  aht.begin();                // AHT21 begin
  ens160.begin();             // ENS160 begin
  Wire.begin();               // Wire begin
  ens160.setOperatingMode(SFE_ENS160_STANDARD);

  // 최초 Wifi 연결
  sendAT("AT", 5000);                                                               // ESP8266 상태 확인
  sendAT(String("AT+CWJAP=\"") + SSID + "\",\"" + PASSWORD + "\"", 5000);           // WIFI 연결 시도
  sendAT("AT+CIFSR", 5000);                                                        // IP 주소 확인
  delay(2000);
  
  while (!ens160.checkDataStatus())
  {
    Serial.println("ENS160 ready to...");
  }  
}

void loop() {
  getDistance();

  bool targetState = (distance <= 15) ? 1 : 0;

  if (targetState != state && !isPending)
  {
    isPending = true;
    pendingState = targetState;
    transitionStart = millis();
  }

  if (isPending && targetState != pendingState)
  {
    isPending = false;
  }

  if (isPending && millis() - transitionStart >= 3000)
  {
    state = pendingState;
    isPending = false;
  }

  if (state)  // Power mode: ON
  {
    if (preState != state)  // OFF -> ON 이면 최초 센서값 전송해야함
    {
      sendAT(String("AT+CIPSTART=\"TCP\",\"") + SERVER_IP + "\"," + SERVER_PORT, 5000); // 서버 접속 시도
      sendData(on, on.length());
      Serial.println("Power On.");
      getBrightness();
      getAirCondition();
      while ((temperature < 10.0) || (temperature > 40.0) || (humidity > 100.0) || (humidity == 0.0) || (aqi == 0))
      {
        payload = String("TEMP:") + temperature + ",HUMI:" + humidity + ",AQI:" + aqi + ",BRI:" + brightness + "\n";
        Serial.println(payload);
        getAirCondition();
      }
      
      payload = String("TEMP:") + temperature + ",HUMI:" + humidity + ",AQI:" + aqi + ",BRI:" + brightness + "\n";
      Serial.println(payload);
      sendData(payload, payload.length());
    }
    else                    // ON -> ON
    {
      getAirCondition();
      getBrightness();

      if (isDiff)
      {
        payload = String("TEMP:") + temperature + ",HUMI:" + humidity + ",AQI:" + aqi + ",BRI:" + brightness + "\n";
        Serial.println(payload);
        sendData(payload, payload.length());
        isDiff = false;
      }
      Serial.println(payload);
      payload = String("BRI:") + brightness;
      Serial.println(payload);
      sendData(payload, payload.length());
    }
  }
  else        // Power mode: OFF
  {
    if (preState != state)
    {
      
      Serial.println("OFF");
      sendData(off, off.length());
      sendAT("AT+CIPCLOSE", 3000);
      delay(1000);  // ← 종료 완료 대기
    }
    else
    {
      Serial.println("System already Off");
    }
    
  }
  preState = state;
}

void getDistance() {
  digitalWrite(Trig_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(Trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(Trig_pin, LOW);

  cycletime = pulseIn(Echo_pin, HIGH);
  if (cycletime == 0) return;

  distance = cycletime * 0.034 / 2; // = Distance = ((340 * cycletime) / 10000) / 2
}

void getAirCondition() {
  float tempTemperature = aht.readTemperature();
  float tempHumidity = aht.readHumidity();

  while (isnan(tempTemperature) || isnan(tempHumidity))
  {
    Serial.println("Sensor ready to start...");
  }

  int tempAqi = ens160.getAQI();

  Serial.println(String("TEMP:") + temperature + ",HUMI:" + humidity + ",AQI:" + aqi + ",BRI:" + brightness + "\n");

  // 온도나 습도가 255면(오류 상태) 보내면 안됨
  if (((int)tempTemperature == 255) || ((int)tempHumidity == 255))
  {
    isDiff = false;
    return;
  }
  // 온습도/AQI가 모두 이전 값과 똑같다면 보낼 필요 없음
  else if ((tempTemperature == temperature) && (tempHumidity == humidity) && (tempAqi == aqi))
  {
    isDiff = false;
    return;
  }
  // 온습도 값이 정상적이고, 온습도/AQI 중 하나라도 다르다면 값을 보냄
  else
  {
    temperature = tempTemperature;
    humidity = tempHumidity;
    aqi = tempAqi;
    isDiff = true;
  }
}

void getBrightness() {
  brightness = analogRead(A0);
}

void sendAT(String cmd, int timeout) {
  String res = "";

  if ((cmd == "AT") || (cmd.indexOf("AT+CWJAP") == 0) || (cmd.indexOf("AT+CIPSTART") == 0) || (cmd == "AT+CIPCLOSE"))
  {
    while (1)
    {
      res = "";
      Serial.println(">> " + cmd);
      espSerial.println(cmd);
      long start = millis();
      while (millis() - start < timeout)
      {
        while (espSerial.available())
        {
          res += (char)espSerial.read();
        }
        if (res.indexOf("OK") != -1)
        {
          Serial.println("OK sign >> " + res);
          return;
        }
      }
      Serial.println(res);
    }
  }
  else
  {
    res = "";
    Serial.println(">> " + cmd);
    espSerial.println(cmd);
    long start = millis();
    while (millis() - start < timeout)
    {
      while (espSerial.available())
      {
        res += (char)espSerial.read();
      }
    }
    Serial.println(res);
  }
}

void sendData(String cmd, int len) {
  // 전송 바이트 수
  sendAT("AT+CIPSEND=" + String(len), 3000);
  Serial.println(">> " + cmd);
  // 데이터 전송
  espSerial.print(cmd);
}