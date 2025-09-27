#define POWER_LED_PIN D6
#define POWER_BUTTON_PIN D10

#define POWER_LED_ON LOW
#define POWER_LED_OFF HIGH

void setup() {
  Serial.begin(9600);
  pinMode(POWER_LED_PIN, INPUT);
  pinMode(POWER_BUTTON_PIN, OUTPUT);
}

void loop() {
  Serial.println(digitalRead(POWER_LED_PIN));
  digitalWrite(POWER_BUTTON_PIN, HIGH);
  delay(1000);
  digitalWrite(POWER_BUTTON_PIN, LOW);
  delay(1000);
}
