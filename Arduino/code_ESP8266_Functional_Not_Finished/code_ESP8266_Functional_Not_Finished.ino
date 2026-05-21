#include <SoftwareSerial.h> // WIFI 통신

#define TX 4
#define RX 5

SoftwareSerial espSerial(TX, RX); // TX 4, RX 5

String result = "";

const char* SSID = "Galaxy S23+";
const char* PASSWORD = "syc28142814";
const char* SERVER_IP = "10.209.80.248";  // 서버 IP(젯슨보드)
const int SERVER_PORT = 9000; // 서버 포트
const int SEND_INTERVAL = 2000;

void sendAT(String cmd, int timeout, String ack) {
  while (1)
  {
    String res = "";
    Serial.println(">> " + cmd);
    espSerial.println(cmd);
    long start = millis();
    while (millis() - start < timeout)
    {
      while (espSerial.available())
      {
        res += (char)espSerial.read();
      }
      break;

      
    }
    if (res.indexOf(ack) != -1)
    {
      Serial.println(res);
      return;
    }
  }
}

// void sendData(float temp, float humi) {
//   // 1. TCP 연결
//   String connCmd = String("AT+CIPSTART=\"TCP\",\"") + SERVER_IP + "\"," + SERVER_PORT;
//   sendAT(connCmd, 5000);
//   delay(2000);  // ← 연결 완전히 맺힐 때까지 충분히 대기

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
//   sendAT("AT+CIPCLOSE", 3000);
//   delay(1000);  // ← 종료 완료 대기
// }

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);  // 이제 9600 고정

  sendAT("AT", 15000, "OK");

  // sendAT(String("AT+CWJAP=\"") + SSID + "\",\"" + PASSWORD + "\"", "OK");
  // sendAT("AT+CIFSR", "OK");  // IP 주소 확인
}

void loop() {
  // sendData(24.5, 40.1);
  delay(SEND_INTERVAL);
}