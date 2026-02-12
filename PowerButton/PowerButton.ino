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

bool off_to_on_alert = true;
bool on_to_off_alert = true;

bool led_state = false;

// track repeated HTTP errors to trigger self-recovery
uint8_t consecutive_http_failures = 0;

bool ensure_wifi_connected(unsigned long timeout_ms = 10000) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("WiFi disconnected, reconnecting");
  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (millis() - start < timeout_ms) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi reconnected");
      return true;
    }
    delay(200);
  }

  Serial.println("WiFi reconnect timeout");
  return false;
}

void recover_network_if_needed() {
  if (consecutive_http_failures >= 3 || WiFi.status() != WL_CONNECTED) {
    Serial.println("Network unstable, forcing re-init");
    init_wifi();
    consecutive_http_failures = 0;
  }
}

void init_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

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
  if (!ensure_wifi_connected()) {
    return "";
  }

  HTTPClient http;
  String res;

  http.setTimeout(5000);

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
      consecutive_http_failures = 0;
    } else{
      Serial.printf("[HTTPS] POST... failed, error: %s\n", http.errorToString(status_code).c_str());
      if (consecutive_http_failures < 10) {
        consecutive_http_failures++;
      }
    }
    http.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
    if (consecutive_http_failures < 10) {
      consecutive_http_failures++;
    }
  }
  return res;
}



char* slack_send_message(String str){
  static char ts[20];
  char buf[2048];

  if (!ensure_wifi_connected()) {
    return "";
  }

  // Slack Messaging API
  HTTPClient http;
  if (!http.begin(SLACK_URL_SEND)) {
    Serial.println(String("[ERROR] cannot begin ") + SLACK_URL_SEND);
    return "";
  }
  http.setTimeout(5000);
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
    consecutive_http_failures = 0;
  } else {
    Serial.printf("ERR %d", status_code);
    if (consecutive_http_failures < 10) {
      consecutive_http_failures++;
    }
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
"- checkstate (cs)\n"
"- help (?)\n"
"- offtoon (ofton)\n"
"- ontooff (ontof)\n"
;


void loop() {
  recover_network_if_needed();

  bool new_led_state = (digitalRead(POWER_LED_PIN) == POWER_LED_ON);
  if (led_state == false && new_led_state == true) {
    if (off_to_on_alert) {
      slack_send_message("<!channel>\noff to ON");
    } else {
      slack_send_message("off to ON");
    }
  }
  if (led_state == true && new_led_state == false) {
    if (on_to_off_alert) {
      slack_send_message("<!channel>\nON to off");
    } else {
      slack_send_message("ON to off");
    }
  }
  led_state = new_led_state;

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
  } else if (last_message == "checkstate" || last_message == "cs") { // check state
    String message = "";
    if (led_state) {
      message += "LED: ON";
    } else {
      message += "LED: off";
    }
    message += "\n";
    if (off_to_on_alert) {
      message += "off to ON: ABLED";
    } else {
      message += "off to ON: disabled";
    }
    message += "\n";
    if (on_to_off_alert) {
      message += "ON to off: ABLED";
    } else {
      message += "ON to off: disabled";
    }
    slack_send_message(message);
  } else if (last_message == "help" || last_message == "?") { // help
    slack_send_message(command_info);
  } else if (last_message == "offtoon" || last_message == "ofton") { // off to on
    off_to_on_alert = !off_to_on_alert;
    String message = "off to ON alert: ";
    if (off_to_on_alert) {
      message += "ABLED";
    } else {
      message += "disabled";
    }
    slack_send_message(message);
  } else if (last_message == "ontooff" || last_message == "ontof") { // on to off
    on_to_off_alert = !on_to_off_alert;
    String message = "ON to off alert: ";
    if (on_to_off_alert) {
      message += "ABLED";
    } else {
      message += "disabled";
    }
    slack_send_message(message);
  }

  recover_network_if_needed();

  delay(10000);

  // Serial.println(digitalRead(POWER_LED_PIN));
  // digitalWrite(POWER_BUTTON_PIN, HIGH);
  // delay(1000);
  // digitalWrite(POWER_BUTTON_PIN, LOW);
  // delay(1000);
}
