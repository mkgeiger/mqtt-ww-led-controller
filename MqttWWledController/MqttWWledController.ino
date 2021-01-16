#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> 
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <EEPROM.h>

// eeprom
#define MQTT_IP_OFFSET         0
#define MQTT_IP_LENGTH        16
#define MQTT_USER_OFFSET      16
#define MQTT_USER_LENGTH      32
#define MQTT_PASSWORD_OFFSET  48
#define MQTT_PASSWORD_LENGTH  32
#define WHITEC_OFFSET         80
#define WHITEC_LENGTH          1
#define WHITEW_OFFSET         81
#define WHITEW_LENGTH          1

// pins
#define WHITEC_GPIO  12
#define WHITEW_GPIO   0
#define BUTTON_GPIO  13

// access point
#define AP_NAME "ZX-2842"
#define AP_TIMEOUT 300
#define MQTT_PORT 1883

// topics
char topic_whtc[30] = "/";
char topic_whtw[30] = "/";
char topic_whtc_fb[30] = "/";
char topic_whtw_fb[30] = "/";

// channel values
uint8_t whitec = 0x00;
uint8_t whitew = 0x00;
uint8_t whitec_old = 0x00;
uint8_t whitew_old = 0x00;
char whtc_str[4];
char whtw_str[4];

// mqtt
IPAddress mqtt_server;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

char mqtt_ip_pre[MQTT_IP_LENGTH] = "";
char mqtt_user_pre[MQTT_USER_LENGTH] = "";
char mqtt_password_pre[MQTT_PASSWORD_LENGTH] = "";

char mqtt_ip[MQTT_IP_LENGTH] = "";
char mqtt_user[MQTT_USER_LENGTH] = "";
char mqtt_password[MQTT_PASSWORD_LENGTH] = "";

// wifi
WiFiManager wifiManager;
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;
char mac_str[13];

String readEEPROM(int offset, int len)
{
    String res = "";
    for (int i = 0; i < len; ++i)
    {
      res += char(EEPROM.read(i + offset));
    }
    return res;
}
  
void writeEEPROM(int offset, int len, String value)
{
    for (int i = 0; i < len; ++i)
    {
      if (i < value.length()) {
        EEPROM.write(i + offset, value[i]);
      } else {
        EEPROM.write(i + offset, 0x00);
      }
    }
}
  
void connectToWifi()
{
  Serial.println("Re-Connecting to Wi-Fi...");
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.mode(WIFI_STA);
  WiFi.begin();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event)
{
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event)
{
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  
  mqttClient.subscribe(topic_whtc, 2);
  mqttClient.subscribe(topic_whtw, 2);

  mqttClient.publish(topic_whtc, 0, true, whtc_str);
  mqttClient.publish(topic_whtw, 0, true, whtw_str);
  mqttClient.publish(topic_whtc_fb, 0, false, whtc_str);
  mqttClient.publish(topic_whtw_fb, 0, false, whtw_str);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  char pl[7];

  strncpy(pl, payload, 6);
  pl[len] = 0;
  Serial.printf("Publish received. Topic: %s Payload: %s\n", topic, pl);

  if (0 == strcmp(topic, topic_whtc))
  {
    char val[4];

    if (len <= 3)
    {
      strncpy(val, (char*)payload, 3);
      val[len] = 0;
      whitec = (uint8_t)strtol(val, NULL, 10);
      if (whitec != whitec_old)
      {
        analogWrite(WHITEC_GPIO, ((int)whitec) * 4);
        EEPROM.write(WHITEC_OFFSET, whitec);
        EEPROM.commit();
      }
      
      whitec_old = whitec;
    }
  }

  if (0 == strcmp(topic, topic_whtw))
  {
    char val[4];

    if (len <= 3)
    {
      strncpy(val, (char*)payload, 3);
      val[len] = 0;
      whitew = (uint8_t)strtol(val, NULL, 10);
      if (whitew != whitew_old)
      {
        analogWrite(WHITEW_GPIO, ((int)whitew) * 4);
        EEPROM.write(WHITEW_OFFSET, whitew);
        EEPROM.commit();
      }
      
      whitew_old = whitew;
    }
  }
}

void setup(void)
{
  uint8_t mac[6];
  
  // init UART
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // init EEPROM
  EEPROM.begin(128);

  // init button
  pinMode(BUTTON_GPIO, INPUT);

  // check if button is pressed
  if (LOW == digitalRead(BUTTON_GPIO))
  {
    Serial.println("reset wifi settings and restart.");
    wifiManager.resetSettings();
    delay(1000);
    ESP.restart();    
  }
  
  // init WIFI
  readEEPROM(MQTT_IP_OFFSET, MQTT_IP_LENGTH).toCharArray(mqtt_ip_pre, MQTT_IP_LENGTH);
  readEEPROM(MQTT_USER_OFFSET, MQTT_USER_LENGTH).toCharArray(mqtt_user_pre, MQTT_USER_LENGTH);
  readEEPROM(MQTT_PASSWORD_OFFSET, MQTT_PASSWORD_LENGTH).toCharArray(mqtt_password_pre, MQTT_PASSWORD_LENGTH);
  
  WiFiManagerParameter custom_mqtt_ip("ip", "MQTT ip", mqtt_ip_pre, MQTT_IP_LENGTH);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT user", mqtt_user_pre, MQTT_USER_LENGTH);
  WiFiManagerParameter custom_mqtt_password("passord", "MQTT password", mqtt_password_pre, MQTT_PASSWORD_LENGTH, "type=\"password\"");
  
  wifiManager.addParameter(&custom_mqtt_ip);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  wifiManager.setConfigPortalTimeout(AP_TIMEOUT);
  wifiManager.setAPStaticIPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
  if (!wifiManager.autoConnect(AP_NAME))
  {
    Serial.println("failed to connect and restart.");
    delay(1000);
    // restart and try again
    ESP.restart();
  }

  strcpy(mqtt_ip, custom_mqtt_ip.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());

  if ((0 != strcmp(mqtt_ip, mqtt_ip_pre)) || 
      (0 != strcmp(mqtt_user, mqtt_user_pre)) || 
      (0 != strcmp(mqtt_password, mqtt_password_pre)))
  {
    Serial.println("Parameters changed, need to update EEPROM.");
    writeEEPROM(MQTT_IP_OFFSET, MQTT_IP_LENGTH, mqtt_ip);
    writeEEPROM(MQTT_USER_OFFSET, MQTT_USER_LENGTH, mqtt_user);
    writeEEPROM(MQTT_PASSWORD_OFFSET, MQTT_PASSWORD_LENGTH, mqtt_password);
    
    EEPROM.commit();
  }

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  // construct MQTT topics with MAC
  WiFi.macAddress(mac);
  sprintf(mac_str, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  strcat(topic_whtc, mac_str);
  strcat(topic_whtc, "/whtc");
  strcat(topic_whtw, mac_str);
  strcat(topic_whtw, "/whtw"); 
  strcat(topic_whtc_fb, mac_str);
  strcat(topic_whtc_fb, "/whtc_fb");
  strcat(topic_whtw_fb, mac_str);
  strcat(topic_whtw_fb, "/whtw_fb");   

  // read stored channels
  whitec = EEPROM.read(WHITEC_OFFSET);
  whitew = EEPROM.read(WHITEW_OFFSET);
  whitec_old = whitec;
  whitew_old = whitew;

  // set PWM channels
  sprintf(whtc_str, "%d", whitec);
  sprintf(whtw_str, "%d", whitew);
  analogWrite(WHITEC_GPIO, ((int)whitec) * 4);   
  analogWrite(WHITEW_GPIO, ((int)whitew) * 4);

  if (mqtt_server.fromString(mqtt_ip))
  {
    char mqtt_id[30] = AP_NAME;

    strcat(mqtt_id, "-");
    strcat(mqtt_id, mac_str);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.setServer(mqtt_server, MQTT_PORT);
    mqttClient.setCredentials(mqtt_user, mqtt_password);
    mqttClient.setClientId(mqtt_id);
  
    connectToMqtt();
  }
  else
  {
    Serial.println("invalid MQTT Broker IP.");
  }
}

void loop(void)
{
  
}
