/*

# Publish
mosquitto_pub -h "j9a12027.ala.us-east-1.emqxsl.com" -t "message" \
-m "My Message" --username "ChipId" --pw "ChipId" -p 8883 -q 2

# Subscribe
mosquitto_sub -h "j9a12027.ala.us-east-1.emqxsl.com" -t "#" \
--username "ChipId" --pw "ChipId" -p 8883

*/

#include <LCD_I2C.h>
#include <NTPClient.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <Timezone.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <Wire.h>

WiFiUDP udp;
Ticker clock_timer;

// Display
LCD_I2C lcd(0x27, 16,
            2); // Default address of most PCF8574 modules, change accordingly

char mqtt_server[40];
int mqtt_port;
char mqtt_topic[40] = "message";

Preferences preferences;

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastWifiConnectionTime = 0;
unsigned long lastMQTTConnectionTime = 0;

// Define the variables to check if the button has been pressed
static bool button_pressed = false;
static unsigned long button_pressed_time = 0;
const int BUTTON_PIN =
    0; // GPIO0 has a button attached to it on the WeMos NodeMCU

// This is hardcoded for Germany.
// Germany observes DST from the last Sunday in March to the last Sunday in
// October.
// TODO: Make this configurable via the web interface.
NTPClient timeClient(udp, "pool.ntp.org",
                     0); // NTP client object
TimeChangeRule CEST = {"CEST", Last, Sun,
                       Mar,    2,    120}; // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun,
                      Oct,    3,    60}; // Central European Standard Time
Timezone timezone(CEST, CET);            // Timezone object for Germany

WiFiManager wifiManager;

void IRAM_ATTR button_isr_handler() {
  // Interrupt service routine that is triggered when the button is pressed
  // Measure how long the button is pressed
  unsigned long start = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    delay(1);
  }
  button_pressed_time = millis() - start;

  // If the button is pressed for more than 2 seconds, reset the device
  if (button_pressed_time > 3000) {
    // Reset the device
    ESP.restart();
  }

  // If the button was pressed for more than 10 ms, set button_pressed to true
  if (button_pressed_time > 10) {
    button_pressed = true;
  }
}

void reboot() {
  // Reboot
#if CONFIG_IDF_TARGET_ESP32
  esp_restart();
#else
  ESP.restart();
#endif
}

void printTime() {
  lcd.setCursor(0, 1);
  time_t utc =
      timeClient.getEpochTime(); // Get the current UTC time from the NTP client
  time_t local = timezone.toLocal(
      utc); // Convert the UTC time to local time observing DST if needed

  // Construct a string in the format "HH:MM:SS"
  char timeString[9];
  sprintf(timeString, "%02d:%02d:%02d", hour(local), minute(local),
          second(local));
  lcd.print(timeString);
  lcd.setCursor(0, 0);

  // Reboot every day at 03:30
  if (timeClient.getHours() == 3 && timeClient.getMinutes() == 30) {
    reboot();
  }
}

void lcdPrint(String text) {
  String line1;
  String line2;

  if (text.length() > 32) {
    // Truncate text to 32 characters
    text = text.substring(0, 32);
  }

  if (text.length() <= 16) {
    // Text is shorter than or equal to 16 characters
    line1 = text;
    line2 = "";
  } else {
    // Text is longer than 16 characters
    int spaceIndex = text.lastIndexOf(" ", 16);
    if (spaceIndex == -1) {
      // No space found, split at 16 characters
      line1 = text.substring(0, 16);
      line2 = text.substring(16, text.length());
    } else if (spaceIndex <= 16) {
      // Space found before or at 16 characters, split at space
      line1 = text.substring(0, spaceIndex);
      line2 = text.substring(spaceIndex + 1, text.length());
    } else {
      // Space found after 16 characters, split at 16 characters
      line1 = text.substring(0, 16);
      line2 = text.substring(16, text.length());
    }
  }

  // Print to Serial
  Serial.println("Line 1: " + line1);
  Serial.println("Line 2: " + line2);

  // Print to LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void saveConfigCallback() {
  preferences.putString("mqtt_server", mqtt_server);
  preferences.putInt("mqtt_port", mqtt_port);
  // preferences.putString("mqtt_topic", mqtt_topic);
}

void mqttReconnect() {
  if (WiFi.isConnected() && !mqttClient.connected()) {
    clock_timer.detach(); // Don't print the time while connecting to MQTT
    lcdPrint("Connecting to MQTT...");

    String mqtt_client_id = String(ESP.getChipId(), HEX);

    // MQTT username and password are the last 3 bytes from the MAC address
    // without ":" For example: MAC: aa:bb:cc:dd:ee:ff
    //             Username and password: ddeeff
    // The MAC address is shown by esptool.py during flashing.
    if (mqttClient.connect(mqtt_client_id.c_str(),
                           String(ESP.getChipId(), HEX).c_str(),
                           String(ESP.getChipId(), HEX).c_str())) {
      Serial.println("MQTT connected");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcdPrint("MQTT connected");
      lcd.clear();
      clock_timer.attach(1,
                         printTime); // Call printTime function every 1 second
      int qos = 1;                   // With 2, we don't get any messages
      mqttClient.subscribe(mqtt_topic, qos);
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" trying again in 5 seconds");
      lcdPrint("MQTT connection failed, retrying");
      delay(5000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {

  // If payload is empty, print the time
  if (length == 0) {
    lcd.noBacklight();
    lcd.clear();
    clock_timer.attach(1, printTime);
    return;
  }

  lcd.backlight();
  clock_timer.detach(); // Don't print the time while a message is displayed

  char *message = (char *)malloc(length + 1);
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(message);

  lcd.backlight();
  lcdPrint(message);
}

void setup() {

  // Attach the interrupt to the button pin
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr_handler,
                  FALLING);

  Serial.begin(115200);
  lcd.begin(); // If you are using more I2C devices using the Wire library use
               // lcd.begin(false)
  lcd.noBacklight();
  lcd.clear();

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(180);

  preferences.begin("mqtt_config", false);
  strcpy(
      mqtt_server,
      preferences.getString("mqtt_server", "j9a12027.ala.us-east-1.emqxsl.com")
          .c_str());
  mqtt_port = preferences.getInt("mqtt_port", 8883);
  // strcpy(mqtt_topic, preferences.getString("mqtt_topic", "message").c_str());

  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server,
                                          40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port",
                                        String(mqtt_port).c_str(), 6);
  // WiFiManagerParameter custom_mqtt_topic("topic", "MQTT Topic", mqtt_topic,
  // 40);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  // wifiManager.addParameter(&custom_mqtt_topic);

  wifiManager.setCustomHeadElement(
      "<style>body{max-width:500px;margin:auto}</style>");

  lcdPrint("Connecting to WLAN...");
  wifiManager.setConnectTimeout(60); // Set the connection timeout to 60 seconds
  wifiManager.setConfigPortalTimeout(
      180); // Set the configuration portal timeout to 180 seconds
  wifiManager.autoConnect(); // Try to connect to the WLAN with saved
                             // credentials, or create an AP with a default name
                             // if not successful for 180 seconds

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  mqtt_port = atoi(custom_mqtt_port.getValue());
  // strcpy(mqtt_topic, custom_mqtt_topic.getValue());

  preferences.putString("mqtt_server", mqtt_server);
  preferences.putInt("mqtt_port", mqtt_port);
  // preferences.putString("mqtt_topic", mqtt_topic);

  // Disable certificate verification
  wifiClient.setInsecure();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
}

void configModeCallback(WiFiManager *myWiFiManager) {
  lcdPrint(myWiFiManager->getConfigPortalSSID());
}

void loop() {

  if (!WiFi.isConnected() || !mqttClient.connected()) {
    clock_timer.detach();
    if (!WiFi.isConnected()) {
      lcdPrint("Retrying WLAN...");
      lastWifiConnectionTime = millis();
      wifiManager.setConfigPortalTimeout(0); // Disable the configuration portal
      while (WiFi.status() !=
             WL_CONNECTED) { // Continue attempting to connect until a
                             // successful connection is made
        delay(5000);
        WiFi.begin(); // Try to connect to the WLAN again
      }
    } else {
      if (!mqttClient.connected()) {
        Serial.println("MQTT disconnected");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcdPrint("MQTT disconnected");
        lastMQTTConnectionTime = millis();
        mqttReconnect();
      } else {
        Serial.println("WLAN and MQTT connected");
        // lcdPrint("WLAN and MQTT connected");
        lcd.clear();
        clock_timer.attach(1,
                           printTime); // Call printTime function every 1 second
      }
    }
  }

  if (!WiFi.isConnected() || !mqttClient.connected()) {
    // Reboot if WLAN or MQTT connection is lost for more than 30 minutes
    if (millis() - lastWifiConnectionTime > 30 * 60 * 1000 ||
        millis() - lastMQTTConnectionTime > 30 * 60 * 1000) {
      Serial.println("Rebooting...");
      reboot();
    }
  }

  if (mqttClient.connected()) {
    mqttClient.loop();
  } else {
    mqttReconnect();
  }

  // If the button is pressed, send a message to the MQTT server
  if (button_pressed) {
    button_pressed = false;
    lcd.clear();
    lcd.noBacklight();
    clock_timer.attach(1, printTime); // Show the time again
    // Publish the current time
    timeClient.update();
    // Construct the message: "Button pressed at <current date and time>" with
    // date and time in the format "YYYY-MM-DD HH:MM:SS"
    String message = "Button pressed at " +
                     timeClient.getFormattedDateTime("%Y-%m-%d %H:%M:%S") +
                     "UTC";

    bool retained = true;
    // mqtt_confirm_topic contains the ESP id, e.g. "confirm/ab3456";
    String mqtt_client_id = String(ESP.getChipId(), HEX);
    String mqtt_confirm_topic = "confirm/" + mqtt_client_id;
    mqttClient.publish(mqtt_confirm_topic.c_str(), message.c_str(), retained);
  }

  timeClient.update();
}
