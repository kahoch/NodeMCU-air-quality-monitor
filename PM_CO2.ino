#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
//#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

#define OLED_RESET LED_BUILTIN
#define wifi_ssid "Your Wifi"
#define wifi_password "Your Wifi password"
#define mqtt_server "Your mqtt sever"
#define mqtt_user "mqtt username"
#define mqtt_password "mqtt user password"
#define AQI_topic "sensor/aqi"
#define PM25_topic "sensor/pm25"
#define PM10_topic "sensor/pm10"
#define temperature_topic "sensor/temperature"
#define humidity_topic "sensor/humidity"
#define hcho_topic "sensor/hcho"
#define co2_topic "sensor/co2"

static unsigned char ucRxBuf[50];
static unsigned char ucRxCnt = 0;
static unsigned char ucCO2RxBuf[50];
static unsigned char ucCO2RxCnt = 0;
static unsigned int aqi = 0;
static unsigned int aqi25 = 0;
static unsigned int aqi10 = 0;
static unsigned int pm25 = 0;
static unsigned int pm10 = 0;
static float temp = 0;
static float rh = 0;
static float hcho = 0;
static unsigned int co2 = 0;
static long lastMsg = 0;
static bool isMQTTConnected = false;
static unsigned int retry = 0;

SoftwareSerial pmSerial(D7, D8);
SoftwareSerial co2Serial(D5, D6);
Adafruit_SSD1306 display(OLED_RESET);
//ESP8266WiFiMulti WiFiMulti;

void(* resetFunc) (void) = 0;
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
	// put your setup code here, to run once:
	Serial.begin(9600);
	pmSerial.begin(9600);
	co2Serial.begin(9600);
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	display.display();
	setup_wifi();
	client.setServer(mqtt_server, 1883);  
}

void setup_wifi() {
	delay(10);
	// We start by connecting to a WiFi network
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(wifi_ssid);

	WiFi.begin(wifi_ssid, wifi_password);

//	while (WiFi.status() != WL_CONNECTED) {
//  If cannot connect, use below 
  while (WiFi.localIP().toString() == "0.0.0.0") {
		delay(500);
		Serial.print(".");
		displayErr();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("AirMonitor1", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      isMQTTConnected = true;
      retry = 0;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.print(" try again in few seconds, retry ");
      Serial.println(retry);
      isMQTTConnected = false;
      readSensor(); 
      retry++;  
      displayInfo(); 
      if (retry > 3){
        retry = 0;
//        WiFi.disconnect(); 
//        setup_wifi();
          resetFunc();  //call reset
      }
      // Wait about 4 seconds before retrying
      delay(3000);
     // yield();
    }
  }
}

void readPM(unsigned char ucData) {
	byte i = 0;
	unsigned short sum = 0;
	if(ucRxCnt == 0 && ucData != 0x42)
	{
		ucRxCnt = 0;
		return;
	}
	if(ucRxCnt == 1 && ucData != 0x4d)
	{
		ucRxCnt = 0;
		return;
	}
	if(ucRxCnt == 2 && ucData != 0x00)
	{
		ucRxCnt = 0;
		return;
	}
	if(ucRxCnt == 3 && ucData != 0x24)
	{
		ucRxCnt = 0;
		return;
	}
	ucRxBuf[ucRxCnt++] = ucData;
	if(ucRxCnt > 39) {
		for(i=0;i<38;i++)
		{
			sum+= ucRxBuf[i];
		}
		if(sum == ucRxBuf[38] * 256 + ucRxBuf[39]) {
			pm25 = ucRxBuf[12] * 256 + ucRxBuf[13];
			pm10 = ucRxBuf[14] * 256 + ucRxBuf[15];
			hcho = (ucRxBuf[28] * 256 + ucRxBuf[29])/1000.0;
			temp = (ucRxBuf[30] * 256 + ucRxBuf[31])/10.0;
			rh = (ucRxBuf[32] * 256 + ucRxBuf[33])/10.0;
		}
		ucRxCnt = 0;
  }
}
int aqipm25(int t){
	if (t <= 12) return t * 50 / 12;
	else if (t <= 35)  return 50 + (t - 12) * 50 / 23;
	else if (t <= 55)  return 100 + (t - 35) * 50 / 20;
	else if (t <= 150)  return 150 + (t - 55) * 50 / 95;
	else if (t <= 250)  return 200 + (t - 150) * 100 / 100;
	else if (t <= 350)  return 300 + (t - 250) * 100 / 100;
	else return 400 + (t - 350) * 100 / 150; 
}
int aqipm10(int t){
  if (t<= 54) return t * 50 / 54;
  else if (t <= 154)  return 50 + (t - 54) * 50 / 100;
  else if (t <= 254)  return 100 + (t - 154) * 50 / 100;
  else if (t <= 354)  return 150 + (t - 254) * 50 / 100;
  else if (t <= 424)  return 200 + (t - 354) * 100 / 70;
  else if (t <= 504)  return 300 + (t - 424) * 100 / 80;
  else return 400 + (t - 504) * 100 / 96;
}
void getaqi(){
	aqi25 = aqipm25(pm25);
	aqi10 = aqipm10(pm10);
	if (aqi25 > aqi10) {
		aqi = aqi25;
	}
		else 
		aqi = aqi10;
}
void readCO2(unsigned char ucData) {
	ucCO2RxBuf[ucCO2RxCnt++] = ucData;
	//if(ucCO2RxBuf[0] != 0xFF && ucCO2RxBuf[1] != 0x86) {
	//	ucCO2RxCnt = 0;
	//}
	if(ucCO2RxCnt > 6) {
		co2 = ucCO2RxBuf[3] * 256 + ucCO2RxBuf[4];
		ucCO2RxCnt = 0;
	}
}

void readSensor(){
	pmSerial.enableRx(true);
	co2Serial.enableRx(false);
	delay(500);
	while(pmSerial.available() > 0) {
		readPM(pmSerial.read());
	}
  getaqi();
	pmSerial.enableRx(false);
	co2Serial.enableRx(true);
	byte request[] = {0xFE, 0X44, 0X00, 0X08, 0X02, 0X9F, 0X25};
	co2Serial.write(request, 7),
	delay(500);
	while(co2Serial.available() > 0) {
		readCO2(co2Serial.read());
	}
}

void displayErr() {
	display.setTextSize(2);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	display.clearDisplay();
	display.print("No Wifi");
	display.println();
	display.print("Connection");
	display.display();
}
  
void displayInfo() {
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	display.clearDisplay();

	display.print("AQI:      ");
	display.print(aqi);
	display.println();
		
	display.print("PM2.5/10: ");
	display.print(pm25);
	display.print("  ");
	display.print(pm10);
	display.println();

	display.print("Temp:     ");
	display.print(temp);
	display.println();

	display.print("RH:       ");
	display.print(rh);
	display.println();

	display.print("Hcho:     ");
	display.print(hcho);
	display.println();

	display.print("CO2:      ");
	display.print(co2);
	display.println();
	display.println();
  if(isMQTTConnected) {
    display.print("MQTT Connected");
  } else {
    display.print("Reconnect to MQTT-");
    display.print(retry);
  }
  
  display.display();
}


void loop() {
	
	if (!client.connected()) {
		reconnect();
	}

	client.loop();
  
	long now = millis();
	if (now - lastMsg > 2000) {
		lastMsg = now;  
    readSensor();
    displayInfo();         
		client.publish(AQI_topic, String(aqi).c_str(), true);
		client.publish(PM25_topic, String(pm25).c_str(), true);
		client.publish(PM10_topic, String(pm10).c_str(), true);
		client.publish(temperature_topic, String(temp).c_str(), true);  
		client.publish(humidity_topic, String(rh).c_str(), true);   
		client.publish(hcho_topic, String(hcho).c_str(), true);
		client.publish(co2_topic, String(co2).c_str(), true); 
	}
}
