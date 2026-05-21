#include <DHT.h>
#include <DHT_U.h>


#define DHTPIN 2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

float humidity;
float temperature;
int count = 1;

void getTempAndHumi() {
  float tempTemperature = dht.readTemperature();
  float tempHumidity = dht.readHumidity();

  while (isnan(tempTemperature) || isnan(tempHumidity))
  {
    Serial.println("센서 읽는 중 . . .");
  }
  
  if ((int)tempTemperature != (int)temperature)
  {
    Serial.print("T:");
    Serial.println((int)tempTemperature);
    temperature = tempTemperature;
  }

  if ((int)tempHumidity != (int)humidity)
  {
    Serial.print("H:");
    Serial.println((int)tempHumidity);
    humidity = tempHumidity;
  }
}

void setup() {
  Serial.begin(9600);
  dht.begin();

  delay(2000);

  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  Serial.print("T:");
  Serial.println((int)temperature);
  Serial.print("H");
  Serial.println((int)humidity);
}

void loop() {
  getTempAndHumi();
  delay(500);
}