#define Touch_pin 7

bool touchState = 0;

bool isTouched() {
  while (1)
  {
    touchState = digitalRead(Touch_pin);
    if (touchState) return;
    else continue;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(Touch_pin, INPUT);

  isTouched();
  Serial.println("Power On");

}

void loop() {
  Serial.println("Loop function entry.");
  delay(10000);

}
