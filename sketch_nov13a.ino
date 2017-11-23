#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>

const char* ssid     = "moiverkko";
const char* password = "penis123";

const char* mqtt_server = "tvt16spl5.ipt.oamk.fi";
const char* topic = "koulu/lämpötila";
//DS18B20
OneWire ds(2);
byte addr[8];

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
char msg[50];
int value = 0;
const unsigned long wait_time = 180000;
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived");
  Serial.print(topic);
  for (int i = 0; i < length; i++){
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect(){
  while(!client.connected()){
    Serial.print("Attempting MQTT con");
    String clientId = "ESP-client";
    if(client.connect(clientId.c_str())){
      Serial.println("connected");
      client.publish("test", "Yhteys tehty");
      client.subscribe(topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("uudelleen 5 sek päästä");
      delay(5000);
    }
  }
}

void wifi_connect(){
  WiFi.mode(WIFI_STA);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void tempSearch(){
  byte i;
  bool done = false;
  while(!done){
    if ( !ds.search(addr)) {
      Serial.println("No more addresses.");
      Serial.println();
      ds.reset_search();
      delay(250);
      return;
    }
    
    Serial.print("Unique ID = ");
    for( i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);
    }
    
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
    }else{
      done = !done;
    }
  }
}

float readTemp() { //TODO: tämä syö addressin ja hakee sen perusteella lämpötilan
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  return (float)raw / 16.0;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  wifi_connect();
  tempSearch();
  //mqtt servu conf
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  long now = millis();
  if (now - lastMsg > wait_time) {
    float temp = readTemp();
    Serial.print(temp);
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "%3.2f", temp);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(topic, msg);
  }
  
}

