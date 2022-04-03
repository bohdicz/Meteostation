/*
 Name:		Meteostanice.ino
 Created:	30. 10. 2021 21:24:16
 Author:	Draven
*/					//www.theengineeringprojects.com/wp-content/uploads/2017/02/Arduino-Data-Types.jpg					


// Soubor s obr�zky po�as� pro LCD
#include "pictures.h"

// Jak �asto chceme m��it z �idel
uint myMeasureDelay = 20000;		// �etnost m��en�

// WiFi - n�zev SSDI wifi s�t� a heslo k n�
const char* ssid = "BBi13";					//Zad�n� n�zvu s�t� do prom�nn� SSID.
const char* password = "smashlandwantedineurope";				//Zad�n� hesla k s�ti.

// p�ipojen� pot�ebn�ch knihoven k modulu BME280, BH1750, knihovnu pro wifi.
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>

// Komunika�n� knihovny wifi, http, UDP a mqtt
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Knihovna pro RTC hodiny ESP32
#include <ESP32Time.h>

// Knihovna pro pr�ci s grafikou LCD
// GxEPD_MinimumExample by Jean-Marc Zingg
#include <GxEPD.h>

// Nastaven� typu LCD pro spr�vnou funkci zobrazen�
#include <GxGDEH029A1/GxGDEH029A1.h>      // 2.9" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4

// Tvorba objekt� pro s�ov� slu�by
WiFiClient espClient;
WiFiUDP ntpUDP;
PubSubClient client(espClient);
NTPClient timeClient(ntpUDP);

// Knihovna pro HTTP klienta, kter� stahuje data po�as� z WEBu
HTTPClient http;
HTTPClient httpGeo;

// Inicializace objektu RTC pro uchov�n� aktu�ln�ho �asu
// RTC nastavujeme pomoc� NTP
ESP32Time rtc;

// nastaven� adresy senzoru BME280
#define BME280_ADRESA (0x76)

// nastaven� adresy senzoru intenzity sv�tla BH1750
#define BH1750_ADRESA (0x23)

// inicializace senzoru BME280 a senzoru BH1750 z knihovny
Adafruit_BME280 bme;
BH1750 luxSenzor;

//P�id�n� MQTT broker adresy
const char mqtt_server[] = "45.159.118.171";
const char mqtt_user[] = "bastleni";
const char mqtt_pass[] = "docelasloziteheslo.";
const char mqtt_client[] = "bobo-esp32-home";


// Knihovna pro pr�ci s JSON strukturou
// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
String jsonBuffer;
// Alokace pam�ti pro JSON documenty - men�� blok znamen�, �e se
// data nena�tou cel�! M�nit opatrn�.
// Use arduinojson.org/v6/assistant to compute the capacity.
DynamicJsonDocument doc(1024);
DynamicJsonDocument geo(512);

String currentTime;
int updateTimeFreq = 20;    // Aktualizace �asu RTC z NTP jednou za N minut
unsigned long lastMillis;


// Nastaven� oblasti pro po�as� - OpenWeatherMap
const String openWeatherMapApiKey = "869af24b268eeeaf1e4409348fa86665";
const String lang = "en";
String serverPath;

byte fontClockSize = 5;
byte fontTempSize = 3;
byte fontHmdSize = 2;
byte fontStatusSize = 0;
String weatherIco = "NaN";

// glob�ln� prom�nn� pro vlhkost, teplotu a sv�tlo
byte myHumidity;
int myTemperature;
uint myLux;

int tempOutdoor;
byte hmdOutdoor;


//setup se provadi pouze jednou v behu programu a slouzi k nastaveni zakladnich
void setup() {

	// Nastavuje datov� tok pro p�enos v bitech za vte�inu na s�riov� lince
	// V�stup vyu��v�me pro zobrazen� diagnostick�ch dat
	Serial.begin(9600);

	// Nastaven� �idla teploty, tlaku a vlhkosti
	if (!bme.begin(BME280_ADRESA)) {
		Serial.println("BME280 senzor nenalezen, zkontrolujte zapojeni!");
	}
	else {
		Serial.println("Senzor BME280 nalezen :D");
	}

	// Nastaven� �idla intenzity osv�tlen�
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
	Procedura p�ipojen� k wifi, mqtt a ntp
	Dokud n�n� �sp�n�, program neb�� d�le!
	*/
	wifiSetup();

		// prodleva pro spr�vn� propojen�
		delay(2000);
		
		// po p�ipojen� se spoj s MQTT a synchronizuj �as
		client.setServer(mqtt_server, 1883);
		client.setCallback(callback);

		// nastaven� DST - zm�nu letn�ho/zimn�ho �asu
		//setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
		//tzset();

		// Nastaven� posunu �asu o hodinu +1, respektov�n� letn�ho �asu (DST), NTP servery
		configTime(3600, 3600, "tik.cesnet.cz", "tak.cesnet.cz");
		timeClient.begin();

		// Z�sk�n� �asu a prvotn� nastaven� RTC
		updateRtc();

		// Zjist�me, kde se nach�z� meteostanice pomoc�
		// GeoIp a vytvo��me string pro dotaz na openweathermap
		getGeoIp();

	/* 
	 Jak �asto cheme aktualizovat RTC z NTP. Proto�e RTC udr�uje �as automaticky, je 
	 pot�eba aktualizaci z Internetu d�lat jen z d�vodu up�esn�n� �asu. Obvykle sta��
	 Aktualizovat jednou za hodinu a jsou-li hodiny p�esn�j��, pak za del�� dobu.

	 P�evedeme minuty pro RTC aktualizaci na milisekundy.
	*/
	updateTimeFreq = updateTimeFreq * 1000L * 60;

	// ulo��me aktu�ln� dobu b�hu arduina pro pot�ebu r�zn�ch period aktivit.
	lastMillis = millis();

	// Z�kladn� nastaven� LCD
	displaySetup();

}


bool wifiSetup() {

	WiFi.begin(ssid, password);		// Zah�j� p�ipojov�n� k s�ti.
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

	//zobrazen� loga �koly po dobu 5s
	display.drawBitmap(sps_logo, 30, 30, 253, 60, GxEPD_BLACK, 1);
	display.update();
	delay(5000);

	display.eraseDisplay();

	display.fillScreen(GxEPD_WHITE);
	// v�znam: x, y, velikost, veliskost, barva
	display.drawBitmap(obrDomek, 10, 65, 60, 60, GxEPD_BLACK, 1);
	display.drawBitmap(obrOutside, 150, 75, 50, 50, GxEPD_BLACK, 1);

	display.writeFastVLine(165, 5, 50, GxEPD_BLACK);
	display.writeFastHLine(10, 65, 276, GxEPD_BLACK);
	
	// display.update();
	
}


// Funkce loop b�� st�le dokola, dokud se �ip nevypne nebo neresetuje.
void loop() {

	// N�sleduj�c� ��dek zajist�, �e po aktualizaci LCD nedojde k jeho
	// probliknut� a displej je aktualizovan� jen v ��sti, kter� je zm�n�n�.
	// 
	// Mus� prob�hnout ka�d�m loopem
	display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);


	if (millis() - lastMillis >= updateTimeFreq)
	{
		// pokud je update �sp�n�, provede se p�enastaven� timeoutu
		// pokud nen�, provede se v dal��m kroku, aby se neresetoval stav RTC
		if (updateRtc())
			lastMillis = millis();
		Serial.println("RTC Updated from NTP");
	}
	else {
		Serial.println("No time to update RTC yet");
	}

	// Vykreslen� �asu na LCD
	// M�li bychom �as vykreslit pouze tehdy, pokud se zm�n� hodiny
	// nikoliv ka�d�m pr�b�hem cyklu.
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

		// z�sk�me data z OpenWeaherMaps
		getInternetWeather();
	}

	// z�sk�me data z lok�ln�ch senzor�
	getLocalSensorsData();

	// pauza p�ed dal�� aktualizac� dat
	delay(myMeasureDelay);
}


int httpCode;
String payload;

// z�sk�me Geo informace pro sestaven� �et�zce  OpenWearherMaps
// Automaticky tam obd��me st�t a m�so, odkud p�ich�z� na�e p�ipojen�.
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

		// weather je vno�en� json a je nutn� ho explicitn� extrahovat
		JsonObject weather = doc["weather"][0];
		JsonObject main = doc["main"];

		/* 
		�prava �et�zce pro ikonu po�as�. Z OWM z�sk�me nap��klad 04n, jako zkratku ikony a my ji
		zkr�t�me jen na ��slo, proto�e 90% ikon jsou shodn� pro den a noc. P�smeno "n" v n�zvu ikon
		znamen� Noc, p�smeno "d" pak Den.

		Jedinou ikonu, kterou chceme m�t pro noc, je symbol �ist�ho neme, tedy m�s�c "01n". Pokud tato
		nastane, nastav�me ikonu na hodnotu 100 a d�le v�b�rem ikonu z�sk�me z hlavi�kov�ho souboru.
		*/
		String weatherIco = weather["icon"];

		int iconNumber;
		if (weatherIco != "01n") {
			weatherIco.remove(2, 1);
			iconNumber = weatherIco.toInt();
			iconNumber = iconNumber * 1;		// ��slo 01 vyn�som�me jedni�kou a odstran�me t�m nulu.
		}
		else {
			iconNumber = 100;
		}

		// vykreslen� IKONY po�as�
		display.fillRect(190, 0, 60, 60, GxEPD_WHITE);
		//display.setCursor(210, 0);


		// p�evod ��sla ikony na obr�zek. Lze z�ejm� elegantn�ji, ale ...
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
		
		// LCD - vykreslen� extern� teploty
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

		// LCD - vykreslen� vlhkosti
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
	Metoda z�sk� data z lok�ln�ch senzor� a vyp�e
	na s�riov� rozhran�. V budoucnu by m�la b�t �e�it jen jednu
	funkci, nikoliv aktualizaci LCD.

*/
void getLocalSensorsData() {

	myHumidity = bme.readHumidity();
	myTemperature = bme.readTemperature();
	myLux = luxSenzor.readLightLevel();

	// Zobrazen� hodnoty lux
	Serial.print("Lux: ");
	Serial.println(myLux);

	// v�pis v�ech dostupn�ch informac� ze senzoru BMP
	// v�pis teploty
	Serial.print("Teplota: ");
	Serial.print(myTemperature);
	Serial.println(" stupnu Celsia.");

	// v�pis relativn� vlhkosti
	Serial.print("Vlhkost: ");
	Serial.print(myHumidity);
	Serial.println(" %");

	// v�pis tlaku s p�epo�tem na hektoPascaly
	Serial.print("Tlak:    ");
	Serial.print(bme.readPressure() / 100.0F);
	Serial.println(" hPa.");

	// vyti�t�n� pr�zdn�ho ��dku a pauza po dobu 2 vte�in
	Serial.println();

	// LCD Vykreslen� teploty
	// display.fillRect(x, y, GxEPD_WIDTH,GxEPD_HEIGHT, GxEPD_WHITE);
	display.fillRect(85, 75, 45, 20, GxEPD_WHITE);
	display.setCursor(85, 75);
	display.setTextSize(fontTempSize);
	display.print(myTemperature);
	display.setTextSize(0);
	display.print("'C");
	display.updateWindow(85, 75, 45, 20, true);


	// LCD Vykreslen� vlhkosti
	// display.fillRect(0, 0, GxEPD_WIDTH,GxEPD_HEIGHT, GxEPD_WHITE);
	display.fillRect(90, 105, 45, 20, GxEPD_WHITE);
	display.setCursor(90, 105);
	display.setTextSize(fontHmdSize);
	display.print(myHumidity);
	display.setTextSize(0);
	display.print("%");
	display.updateWindow(90, 105, 45, 20, true);

	/* 
	z�sk�n� informace, jak dlouho syst�m b�� pro 
	p�ed�n� informace do MQTT ke kontrole funk�nosti
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

	
	// Poskl�d� �et�zec, kde bude doba b�hu ve dnech, hodin�ch a minut�ch
	String myRuntime = "Uptime: ";
	myRuntime = myRuntime + days;
	myRuntime = myRuntime + " days, ";

	myRuntime = myRuntime + hours;
	myRuntime = myRuntime + " hours, ";

	myRuntime = myRuntime + minutes;
	myRuntime = myRuntime + " minutes";

	Serial.println(myRuntime);

	// Publikuje nam��en� data na MQTT server
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

// metoda aktualizuje RTC z NTP, pokud je s� dostupn�
bool updateRtc() {

	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("S� nen� dostupn�... ");
		//pokud nebude dostupn� s�, nebude se �as aktualizovat a pob�� d�le na RTC
		return false;
	} else {

		// NTP z�sk� aktu�ln� �as
		if (timeClient.update()) {
			Serial.println("NTP has been updated ");
		}
		// Z�skan� NTP �as ulo��me do RTC ESP32
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
