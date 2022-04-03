/*
 Name:		Meteostanice.ino
 Created:	30. 10. 2021 21:24:16
 Author:	Draven
*/					//www.theengineeringprojects.com/wp-content/uploads/2017/02/Arduino-Data-Types.jpg					


// Soubor s obrázky poèasí pro LCD
#include "pictures.h"

// Jak èasto chceme mìøit z èidel
uint myMeasureDelay = 20000;		// èetnost mìøení

// WiFi - název SSDI wifi sítì a heslo k ní
const char* ssid = "BBi13";					//Zadání názvu sítì do promìnné SSID.
const char* password = "smashlandwantedineurope";				//Zadání hesla k síti.

// pøipojení potøebnıch knihoven k modulu BME280, BH1750, knihovnu pro wifi.
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>

// Komunikaèní knihovny wifi, http, UDP a mqtt
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Knihovna pro RTC hodiny ESP32
#include <ESP32Time.h>

// Knihovna pro práci s grafikou LCD
// GxEPD_MinimumExample by Jean-Marc Zingg
#include <GxEPD.h>

// Nastavení typu LCD pro správnou funkci zobrazení
#include <GxGDEH029A1/GxGDEH029A1.h>      // 2.9" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4

// Tvorba objektù pro síové sluby
WiFiClient espClient;
WiFiUDP ntpUDP;
PubSubClient client(espClient);
NTPClient timeClient(ntpUDP);

// Knihovna pro HTTP klienta, kterı stahuje data poèasí z WEBu
HTTPClient http;
HTTPClient httpGeo;

// Inicializace objektu RTC pro uchování aktuálního èasu
// RTC nastavujeme pomocí NTP
ESP32Time rtc;

// nastavení adresy senzoru BME280
#define BME280_ADRESA (0x76)

// nastavení adresy senzoru intenzity svìtla BH1750
#define BH1750_ADRESA (0x23)

// inicializace senzoru BME280 a senzoru BH1750 z knihovny
Adafruit_BME280 bme;
BH1750 luxSenzor;

//Pøidání MQTT broker adresy
const char mqtt_server[] = "45.159.118.171";
const char mqtt_user[] = "bastleni";
const char mqtt_pass[] = "docelasloziteheslo.";
const char mqtt_client[] = "bobo-esp32-home";


// Knihovna pro práci s JSON strukturou
// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
String jsonBuffer;
// Alokace pamìti pro JSON documenty - menší blok znamená, e se
// data nenaètou celá! Mìnit opatrnì.
// Use arduinojson.org/v6/assistant to compute the capacity.
DynamicJsonDocument doc(1024);
DynamicJsonDocument geo(512);

String currentTime;
int updateTimeFreq = 20;    // Aktualizace èasu RTC z NTP jednou za N minut
unsigned long lastMillis;


// Nastavení oblasti pro poèasí - OpenWeatherMap
const String openWeatherMapApiKey = "869af24b268eeeaf1e4409348fa86665";
const String lang = "en";
String serverPath;

byte fontClockSize = 5;
byte fontTempSize = 3;
byte fontHmdSize = 2;
byte fontStatusSize = 0;
String weatherIco = "NaN";

// globální promìnné pro vlhkost, teplotu a svìtlo
byte myHumidity;
int myTemperature;
uint myLux;

int tempOutdoor;
byte hmdOutdoor;


//setup se provadi pouze jednou v behu programu a slouzi k nastaveni zakladnich
void setup() {

	// Nastavuje datovı tok pro pøenos v bitech za vteøinu na sériové lince
	// Vıstup vyuíváme pro zobrazení diagnostickıch dat
	Serial.begin(9600);

	// Nastavení èidla teploty, tlaku a vlhkosti
	if (!bme.begin(BME280_ADRESA)) {
		Serial.println("BME280 senzor nenalezen, zkontrolujte zapojeni!");
	}
	else {
		Serial.println("Senzor BME280 nalezen :D");
	}

	// Nastavení èidla intenzity osvìtlení
	if (!luxSenzor.begin())
	{
		Serial.println(F("BH1750 senzor nenalezen, zkontrolujte zapojeni nebo adresu senzoru!"));
		//while (1);
	}
	else
	{
		Serial.println(F("Senzor BH1750 nalezen :D"));
	}

	/* 
	Procedura pøipojení k wifi, mqtt a ntp
	Dokud nìní úspìšná, program nebìí dále!
	*/
	wifiSetup();

		// prodleva pro správné propojení
		delay(2000);
		
		// po pøipojení se spoj s MQTT a synchronizuj èas
		client.setServer(mqtt_server, 1883);
		client.setCallback(callback);

		// nastavení DST - zmìnu letního/zimního èasu
		//setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
		//tzset();

		// Nastavení posunu èasu o hodinu +1, respektování letního èasu (DST), NTP servery
		configTime(3600, 3600, "tik.cesnet.cz", "tak.cesnet.cz");
		timeClient.begin();

		// Získání èasu a prvotní nastavení RTC
		updateRtc();

		// Zjistíme, kde se nachází meteostanice pomocí
		// GeoIp a vytvoøíme string pro dotaz na openweathermap
		getGeoIp();

	/* 
	 Jak èasto cheme aktualizovat RTC z NTP. Protoe RTC udruje èas automaticky, je 
	 potøeba aktualizaci z Internetu dìlat jen z dùvodu upøesnìní èasu. Obvykle staèí
	 Aktualizovat jednou za hodinu a jsou-li hodiny pøesnìjší, pak za delší dobu.

	 Pøevedeme minuty pro RTC aktualizaci na milisekundy.
	*/
	updateTimeFreq = updateTimeFreq * 1000L * 60;

	// uloíme aktuální dobu bìhu arduina pro potøebu rùznıch period aktivit.
	lastMillis = millis();

	// Základní nastavení LCD
	displaySetup();

}


bool wifiSetup() {

	WiFi.begin(ssid, password);		// Zahájí pøipojování k síti.
	Serial.print("Connecting WiFI");
	
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}

		Serial.print("connected: ");
		Serial.println(ssid);

		Serial.print("IP: ");
		Serial.println(WiFi.localIP());

		Serial.print("GW: ");
		Serial.println(WiFi.gatewayIP());
		return true;
}

void displaySetup() {

	display.init();
	//display.eraseDisplay();
	display.setRotation(3);
	display.setTextColor(GxEPD_BLACK);
	display.fillScreen(GxEPD_WHITE);

	//zobrazení loga školy po dobu 5s
	display.drawBitmap(sps_logo, 30, 30, 253, 60, GxEPD_BLACK, 1);
	display.update();
	delay(5000);

	display.eraseDisplay();

	display.fillScreen(GxEPD_WHITE);
	// vıznam: x, y, velikost, veliskost, barva
	display.drawBitmap(obrDomek, 10, 65, 60, 60, GxEPD_BLACK, 1);
	display.drawBitmap(obrOutside, 150, 75, 50, 50, GxEPD_BLACK, 1);

	display.writeFastVLine(165, 5, 50, GxEPD_BLACK);
	display.writeFastHLine(10, 65, 276, GxEPD_BLACK);
	
	// display.update();
	
}


// Funkce loop bìí stále dokola, dokud se èip nevypne nebo neresetuje.
void loop() {

	// Následující øádek zajistí, e po aktualizaci LCD nedojde k jeho
	// probliknutí a displej je aktualizovanı jen v èásti, která je zmìnìná.
	// 
	// Musí probìhnout kadım loopem
	display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);


	if (millis() - lastMillis >= updateTimeFreq)
	{
		// pokud je update úspìšnı, provede se pøenastavení timeoutu
		// pokud není, provede se v dalším kroku, aby se neresetoval stav RTC
		if (updateRtc())
			lastMillis = millis();
		Serial.println("RTC Updated from NTP");
	}
	else {
		Serial.println("No time to update RTC yet");
	}

	// Vykreslení èasu na LCD
	// Mìli bychom èas vykreslit pouze tehdy, pokud se zmìní hodiny
	// nikoliv kadım prùbìhem cyklu.
	drawClock();

	if (WiFi.status() == WL_CONNECTED) {

		// Pozor, klient public musi mit predavanou hodnotu jako string!!!   
		if (client.connect(mqtt_client, mqtt_user, mqtt_pass)) {
			Serial.println("MQTT: connected");
			client.loop();
		}
		else {
			Serial.println("MQTT: problem");
		}

		// získáme data z OpenWeaherMaps
		getInternetWeather();
	}

	// získáme data z lokálních senzorù
	getLocalSensorsData();

	// pauza pøed další aktualizací dat
	delay(myMeasureDelay);
}


int httpCode;
String payload;

// získáme Geo informace pro sestavení øetìzce  OpenWearherMaps
// Automaticky tam obdíme stát a mìso, odkud pøichází naše pøipojení.
void getGeoIp() {
	httpGeo.begin("https://api.freegeoip.app/json/?apikey=c25a1550-94d0-11ec-b8b9-f50eef404dbc");
	httpCode = httpGeo.GET();
	if (httpCode > 0) {
		Serial.print("HTTP Code: ");
		Serial.println(httpCode);
		payload = httpGeo.getString();
		DeserializationError error = deserializeJson(geo, payload);
		String countryCode = geo["country_code"];
		String city = geo["city"];
		serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&units=metric&appid=" + openWeatherMapApiKey + "&lang=" + lang;
		Serial.print("GeoIP Country:");
		Serial.println(countryCode);
		Serial.print("GeoIP City:");
		Serial.println(city);
		Serial.println(serverPath);
		geo.clear();
	}
	httpGeo.end();
	delay(1000);
}

void getInternetWeather() {
	
	http.begin(serverPath);
	httpCode = http.GET();
	Serial.print("HTTP Code: ");
	Serial.println(httpCode);


	if (httpCode > 0) {
		payload = http.getString();

		DeserializationError error = deserializeJson(doc, payload);

		// weather je vnoøenı json a je nutné ho explicitnì extrahovat
		JsonObject weather = doc["weather"][0];
		JsonObject main = doc["main"];

		/* 
		Úprava øetìzce pro ikonu poèasí. Z OWM získáme napøíklad 04n, jako zkratku ikony a my ji
		zkrátíme jen na èíslo, protoe 90% ikon jsou shodné pro den a noc. Písmeno "n" v názvu ikon
		znamená Noc, písmeno "d" pak Den.

		Jedinou ikonu, kterou chceme mít pro noc, je symbol èistého neme, tedy mìsíc "01n". Pokud tato
		nastane, nastavíme ikonu na hodnotu 100 a dále vıbìrem ikonu získáme z hlavièkového souboru.
		*/
		String weatherIco = weather["icon"];

		int iconNumber;
		if (weatherIco != "01n") {
			weatherIco.remove(2, 1);
			iconNumber = weatherIco.toInt();
			iconNumber = iconNumber * 1;		// èíslo 01 vynásomíme jednièkou a odstraníme tím nulu.
		}
		else {
			iconNumber = 100;
		}

		// vykreslení IKONY poèasí
		display.fillRect(190, 0, 60, 60, GxEPD_WHITE);
		//display.setCursor(210, 0);


		// pøevod èísla ikony na obrázek. Lze zøejmì elegantnìji, ale ...
		switch (iconNumber) {
		case 1:
			display.drawBitmap(ico01, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 100:
			display.drawBitmap(ico01n, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 2:
			display.drawBitmap(ico02, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 3:
			display.drawBitmap(ico03, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 4:
			display.drawBitmap(ico04, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 9:
			display.drawBitmap(ico09, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 10:
			display.drawBitmap(ico10, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 11:
			display.drawBitmap(ico11, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 13:
			display.drawBitmap(ico13, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		case 50:
			display.drawBitmap(ico50, 200, 5, 50, 50, GxEPD_BLACK, 1);
			break;
		}
		display.updateWindow(190, 0, 60, 60, true);


		Serial.print("zkraceni pocasi: ");
		Serial.println(iconNumber);
		
		// LCD - vykreslení externí teploty
		tempOutdoor = main["temp"];
		Serial.print("Teplota ext.: ");
		Serial.println(tempOutdoor);

		display.setTextSize(fontTempSize);
		//	display.fillRect(0, 0, GxEPD_WIDTH,GxEPD_HEIGHT, GxEPD_WHITE);
		display.fillRect(220, 75, 45, 30, GxEPD_WHITE);
		display.setTextColor(GxEPD_BLACK);
		display.setCursor(220, 75);
		display.print(tempOutdoor);
		display.setTextSize(0);
		display.print("'C");
		display.updateWindow(220, 70, 45, 30, true);

		// LCD - vykreslení vlhkosti
		//	display.fillRect(0, 0, GxEPD_WIDTH,GxEPD_HEIGHT, GxEPD_WHITE);
		display.fillRect(230, 105, 45, 30, GxEPD_WHITE);
		display.setCursor(230, 105);
		hmdOutdoor = main["humidity"];
		display.setTextSize(fontHmdSize);
		display.print(hmdOutdoor);
		display.setTextSize(0);
		display.print("%");
		display.updateWindow(230, 105, 45, 30, true);
	}

	else {
		Serial.println("Error on HTTP request");
	}
	http.end();
}

/*
	Metoda získá data z lokálních senzorù a vypíše
	na sériové rozhraní. V budoucnu by mìla bıt øešit jen jednu
	funkci, nikoliv aktualizaci LCD.

*/
void getLocalSensorsData() {

	myHumidity = bme.readHumidity();
	myTemperature = bme.readTemperature();
	myLux = luxSenzor.readLightLevel();

	// Zobrazení hodnoty lux
	Serial.print("Lux: ");
	Serial.println(myLux);

	// vıpis všech dostupnıch informací ze senzoru BMP
	// vıpis teploty
	Serial.print("Teplota: ");
	Serial.print(myTemperature);
	Serial.println(" stupnu Celsia.");

	// vıpis relativní vlhkosti
	Serial.print("Vlhkost: ");
	Serial.print(myHumidity);
	Serial.println(" %");

	// vıpis tlaku s pøepoètem na hektoPascaly
	Serial.print("Tlak:    ");
	Serial.print(bme.readPressure() / 100.0F);
	Serial.println(" hPa.");

	// vytištìní prázdného øádku a pauza po dobu 2 vteøin
	Serial.println();

	// LCD Vykreslení teploty
	// display.fillRect(x, y, GxEPD_WIDTH,GxEPD_HEIGHT, GxEPD_WHITE);
	display.fillRect(85, 75, 45, 20, GxEPD_WHITE);
	display.setCursor(85, 75);
	display.setTextSize(fontTempSize);
	display.print(myTemperature);
	display.setTextSize(0);
	display.print("'C");
	display.updateWindow(85, 75, 45, 20, true);


	// LCD Vykreslení vlhkosti
	// display.fillRect(0, 0, GxEPD_WIDTH,GxEPD_HEIGHT, GxEPD_WHITE);
	display.fillRect(90, 105, 45, 20, GxEPD_WHITE);
	display.setCursor(90, 105);
	display.setTextSize(fontHmdSize);
	display.print(myHumidity);
	display.setTextSize(0);
	display.print("%");
	display.updateWindow(90, 105, 45, 20, true);

	/* 
	získání informace, jak dlouho systém bìí pro 
	pøedání informace do MQTT ke kontrole funkènosti
	*/
	unsigned long currentMillis = millis();
	unsigned long seconds = currentMillis / 1000;
	unsigned long minutes = seconds / 60;
	unsigned long hours = minutes / 60;
	unsigned long days = hours / 24;
	currentMillis %= 1000;
	seconds %= 60;
	minutes %= 60;
	hours %= 24;

	
	// Poskládá øetìzec, kde bude doba bìhu ve dnech, hodinách a minutách
	String myRuntime = "Uptime: ";
	myRuntime = myRuntime + days;
	myRuntime = myRuntime + " days, ";

	myRuntime = myRuntime + hours;
	myRuntime = myRuntime + " hours, ";

	myRuntime = myRuntime + minutes;
	myRuntime = myRuntime + " minutes";

	Serial.println(myRuntime);

	// Publikuje namìøená data na MQTT server
	client.publish("/home/meteostanice/temperature", String(myTemperature).c_str(), true);
	client.publish("/home/meteostanice/humidity", String(myHumidity).c_str(), true);
	client.publish("/home/meteostanice/lux", String(myLux).c_str(), true);
	client.publish("/home/meteostanice/runtime", String(myRuntime).c_str(), true);

	client.publish("/home/meteostanice/temperature_out", String(tempOutdoor).c_str(), true);
	client.publish("/home/meteostanice/humidity_out", String(hmdOutdoor).c_str(), true);
}

void callback(char* topic, byte* payload, unsigned int length)
{
	Serial.print("Message arrived in topic: ");
	Serial.println(topic);
	Serial.print("Message:");
	for (int i = 0; i < length; i++)
	{
		Serial.print((char)payload[i]);
	}
	Serial.println();
	Serial.println("-----------------------");

	if (String(topic) == "/home/meteostanice/backmessage") {
		Serial.print("messeage out there");

	}
}

// metoda aktualizuje RTC z NTP, pokud je sí dostupná
bool updateRtc() {

	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("Sí není dostupná... ");
		//pokud nebude dostupná sí, nebude se èas aktualizovat a pobìí dále na RTC
		return false;
	} else {

		// NTP získá aktuální èas
		if (timeClient.update()) {
			Serial.println("NTP has been updated ");
		}
		// Získanı NTP èas uloíme do RTC ESP32
		rtc.setTime(timeClient.getEpochTime());
		Serial.print("Set RTC to: ");
		Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));

	} 
	return true;
}

String lastClockTime;
void drawClock() {
	if (lastClockTime != rtc.getTime("%H:%M")) {
		lastClockTime = rtc.getTime("%H:%M");
		display.fillRect(10, 15, 150, 50, GxEPD_WHITE);
		display.setCursor(10, 15);
		display.setTextSize(fontClockSize);

		display.print(lastClockTime);
		display.updateWindow(10, 15, 150, 50, true);
		display.setTextSize(1);
		Serial.println("Current time differ, draw TIME");
	}
	else {
		Serial.println("Current time is same, no draw TIME");
	}
}
