// Header File include
#include <SoftwareSerial.h> // WIFI 통신
// #include <DHT.h>
// #include <DHT_U.h>
#include <Wire.h>
#include <AHTxx.h>
#include "SparkFun_ENS160.h"

// Pin define
// #define DHTPIN 2
#define TX 4
#define RX 5
#define Touch_pin 7
#define Echo_pin 8
#define Trig_pin 9
// #define SCL_pin A5
// #define SDA_pin A4
// #define DELAY 30

// etc define
// #define DHTTYPE DHT11

SoftwareSerial espSerial(TX, RX); // TX 4, RX 5
// DHT dht(DHTPIN, DHTTYPE);
SparkFun_ENS160 ens160;
AHTxx aht(AHTXX_ADDRESS_X38, AHT2x_SENSOR);

// int touchState = 0;      // Touch Sensor state
bool state = 0;          // Power mode(1 - On, 0 - Off)
bool preState = 0;
bool pendingState = 0;       // Save last power mode
bool isPending = 0;      // 3sec waiting
unsigned long transitionStart = 0;
float humidity = 0.0;    // AHT21 - Humidity
float temperature = 0.0; // AHT21 - Temperature
int aqi = 0;             // AHT21 - AQI
int brightness = 0;      // CDS - Brightness
float cycletime;         // HC-SR04 - Ultrasonic shoot time
float distance; // HC-SR04 - distance to stuff
bool isDiff = false;
String on = "ON";
String off = "OFF";
String payload;          // Data store string

// ⬇️ WIFI&TCP/IP settings
const char* SSID = "abcd";                // WIFI SSID
const char* PASSWORD = "abcdabcd";        // WIFI PASSWORD
// const char* SERVER_IP = "10.209.80.248";  // Server IP
const char* SERVER_IP = "192.168.0.2";    // Jetson Nano Server IP
const int SERVER_PORT = 9000;             // Server access PORT number
const int SEND_INTERVAL = 2000;           // TCP send interval

// void getTouched();                        // SmartMirror Power On/Off
void getAirCondition();                   // Get Humidity&Temperature by DHT11
void getDistance();                       // Get distance of stuff
void getBrightness();                     // Get brightness of room
void sendAT(String cmd, int timeout);     // ESP8266 Control
void sendData(String cmd, int len);       // Data throw






void setup() {
  Serial.begin(9600);         // Serial Monitor 
  espSerial.begin(9600);      // ESP8266 Serial
  pinMode(Touch_pin, INPUT);  // Touch Sensor
  pinMode(Trig_pin, OUTPUT);  // Ultrasonic Sensor(throw)
  pinMode(Echo_pin, INPUT);   // Ultrasonic Sensor(receive)
  // dht.begin();                // DHT11 begin
  aht.begin();                // AHT21 begin
  ens160.begin();             // ENS160 begin
  Wire.begin();               // Wire begin

  int ensStatus;
  ens160.setOperatingMode(SFE_ENS160_STANDARD);
  ensStatus = ens160.getFlags();

  sendAT("AT", 5000);                                                               // ESP8266 상태 확인
  sendAT(String("AT+CWJAP=\"") + SSID + "\",\"" + PASSWORD + "\"", 5000);           // WIFI 연결 시도
  sendAT("AT+CIFSR", 15000);                                                        // IP 주소 확인
  sendAT(String("AT+CIPSTART=\"TCP\",\"") + SERVER_IP + "\"," + SERVER_PORT, 5000); // 서버 접속 시도
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
      sendData(on, on.length());
      Serial.println("Power On.");
      getAirCondition();
      getBrightness();
      // delay(1000);
      Serial.println(String("TEMP:") + temperature + ",HUMI:" + humidity + ",AQI:" + aqi + ",BRI:" + brightness);
      payload = String("TEMP:") + temperature + ",HUMI:" + humidity + ",AQI:" + aqi + ",BRI:" + brightness + "\n";
      sendData(payload, payload.length());
    }
    else                    // ON -> ON
    {
      getAirCondition();
      getBrightness();

      if (isDiff)
      {
        payload = String("TEMP:") + temperature + ",HUMI:" + humidity + ",AQI:" + aqi + ",BRI:" + brightness + "\n";
        sendData(payload, payload.length());
      }
      payload = String("BRI:") + brightness;
      sendData(payload, payload.length());
    }
  }
  else        // Power mode: OFF
  {
    if (preState != state)
    {
      Serial.println("OFF");
      sendData(off, off.length());
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
  if (cycletime == 0)
  {
    // Serial.println(distance);
    return;
  }


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

  
  if ((tempTemperature != temperature) || (tempHumidity != humidity) || (tempAqi != aqi))
  {
    // Serial.println("TEMP:" + (int)tempTemperature);
    // Serial.println("HUMI:" + (int)tempHumidity);
    // Serial.println("AQI:" + tempAqi);
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

  if ((cmd == "AT") || (cmd.indexOf("AT+CWJAP") == 0) || (cmd == "AT+CIFSR") || (cmd == "AT+CIPSTART"))
  {
    Serial.println("This is basic command.");
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
          Serial.println(res);
          return;
        }
        
      }
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
  // 3. 전송 바이트 수 예고
  sendAT("AT+CIPSEND=" + String(len), 3000);
  // delay(1000);  // ← > 프롬프트 완전히 뜰 때까지 대기

  // 4. 실제 데이터 전송
  Serial.println(">> " + cmd);
  espSerial.print(cmd);
  // delay(2000);  // ← 전송 + 서버 응답 완료 대기

  // 5. 연결 종료
  // sendAT("AT+CIPCLOSE", 3000);
  // delay(1000);  // ← 종료 완료 대기
}

// void sendData(String cmd) {
//   int len = cmd.length();

//   sendAT("AT+CIPSEND=" + String(len), 3000);
//   delay(1000);

//   Serial.println(">> " + cmd);
//   espSerial.print(cmd);
//   delay(1000);
// }

// void sendData(float temp, float humi) {
//   // 2. 페이로드 구성
//   String payload = String("TEMP:") + temp + ",HUMI:" + humi + "\n";
//   int len = payload.length();

//   // 3. 전송 바이트 수 예고
//   sendAT("AT+CIPSEND=" + String(len), 3000);
//   delay(1000);  // ← > 프롬프트 완전히 뜰 때까지 대기

//   // 4. 실제 데이터 전송
//   Serial.println(">> " + payload);
//   espSerial.print(payload);
//   delay(2000);  // ← 전송 + 서버 응답 완료 대기

//   // 5. 연결 종료
//   // sendAT("AT+CIPCLOSE", 3000);
//   // delay(1000);  // ← 종료 완료 대기
// }

// void getTouched() {
//   static unsigned long lastTouchTime = 0;

//   if (digitalRead(Touch_pin))
//   {
//     if (millis() - lastTouchTime > DELAY)
//     {
//       touchState += 1;
//       lastTouchTime = millis();
//     }
//     // touchState += 1;
//   }
//   // touchState += digitalRead(Touch_pin);
//   // Serial.println("touchState: " + touchState);
// }