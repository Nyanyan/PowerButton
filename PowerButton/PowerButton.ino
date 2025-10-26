#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi password & Slack token
#include "token.h"


// hardware constants
#define POWER_LED_PIN D6
#define POWER_BUTTON_PIN D10

#define POWER_LED_ON LOW
#define POWER_LED_OFF HIGH


// slack url
#define SLACK_URL_SEND "https://slack.com/api/chat.postMessage"
#define SLACK_URL_RECEIVE "https://slack.com/api/conversations.history"


// json
StaticJsonDocument<1024> doc;

void init_wifi() {
  for (;;) {
    unsigned long strt = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("WiFi starting");
    while (millis() - strt < 10000) {
      Serial.print(".");
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        return;
      }
      delay(100);
    }
    Serial.println("timeout");
  }
}



String slack_get_message() {
  HTTPClient http;
  String res;

  if (http.begin(SLACK_URL_RECEIVE)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = String("token=") + SLACK_TOKEN + "&channel=" + SLACK_CHANNEL_ID + "&limit=1";
    int status_code = http.POST(body);
    Serial.printf("[HTTPS] POST... code: %d\n", status_code);
    if (status_code == HTTP_CODE_OK || status_code == HTTP_CODE_MOVED_PERMANENTLY) {
      String raw_json = http.getString();
      deserializeJson(doc, raw_json);
      const char* text = doc["messages"][0]["text"];
      res = text;
    } else{
      Serial.printf("[HTTPS] POST... failed, error: %s\n", http.errorToString(status_code).c_str());
    }
    http.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
  return res;
}



char* slack_send_message(String str){
  static char ts[20];
  char buf[2048];

  // Slack Messaging API
  HTTPClient http;
  if (!http.begin(SLACK_URL_SEND)) {
    Serial.println(String("[ERROR] cannot begin ") + SLACK_URL_SEND);
    return "";
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  const char *str_c = str.c_str();
  Serial.println(str_c);

  sprintf(buf, ""
               "token=%s"
               "&channel=%s"
               "&text=%s",
          SLACK_TOKEN,
          SLACK_CHANNEL,
          str_c);

  strcpy(ts, "");

  int status_code = http.POST((uint8_t*)buf, strlen(buf));
  Serial.print(status_code);
  if (status_code == HTTP_CODE_OK || status_code == HTTP_CODE_MOVED_PERMANENTLY) {
    String json = http.getString();
    Serial.println(json);
    deserializeJson(doc, json);
    if (doc.containsKey("ts")) {
      strcpy(ts, doc["ts"]);
      Serial.println(ts);
    }
  } else {
    Serial.printf("ERR %d", status_code);
  }
  http.end();
  return ts;
}


void setup() {
  Serial.begin(9600);
  pinMode(POWER_LED_PIN, INPUT);
  pinMode(POWER_BUTTON_PIN, OUTPUT);
  digitalWrite(POWER_BUTTON_PIN, LOW);
  init_wifi();
  slack_send_message("PowerButton Started!");
}


String command_info = 
"commands:\n"
"- press (p)\n"
"- longpress (lp)\n"
"- checkled (cl)\n"
"- help (?)\n";


void loop() {
  String last_message = slack_get_message();
  if (last_message == "press" || last_message == "p") { // press button
    digitalWrite(POWER_BUTTON_PIN, HIGH);
    delay(700);
    digitalWrite(POWER_BUTTON_PIN, LOW);
    slack_send_message("button pressed");
  } else if (last_message == "longpress" || last_message == "lp") { // press button
    digitalWrite(POWER_BUTTON_PIN, HIGH);
    delay(10000);
    digitalWrite(POWER_BUTTON_PIN, LOW);
    slack_send_message("button long-pressed");
  } else if (last_message == "checkled" || last_message == "cl") { // check LED
    if (digitalRead(POWER_LED_PIN) == POWER_LED_ON) {
      slack_send_message("LED on");
    } else {
      slack_send_message("LED off");
    }
  } else if (last_message == "help" || last_message == "?") { // help
    slack_send_message(command_info);
  }
  delay(10000);

  // Serial.println(digitalRead(POWER_LED_PIN));
  // digitalWrite(POWER_BUTTON_PIN, HIGH);
  // delay(1000);
  // digitalWrite(POWER_BUTTON_PIN, LOW);
  // delay(1000);
}
