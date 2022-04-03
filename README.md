# Meteostanice
Dlouhodobá maturitní práce pro SPŠEOL

<!-- O projektu meteostanice -->
## O projektu meteostanice


 V rámci Dlouhodobé maturitní práce vytvářím projekt <b>meteostanice</b>. Díky tomuto projektu bude uživatel moci se vzdáleně připojit na stránku, kde se budou postupně zobrazovat hodnoty, které meteostanice změří
 a uživatel bude moci tyto hodnoty vidět na jeho vlastním zařízení, a zároveň bude moct sledovat dlouhodobý vývoj změřených hodnot na grafech.

Funkce meteostanice:
* Měření vnitřní teploty, vlhkosti a tlaku
* Připojení k síti, kterou si uživatel nastaví
* Aktuální stav na display
* Čas synchronizovaný s internetem
* Snímání okolního světla

Vývoj:
* 03.04.2022 - úprava kódu pro optimalizaci připojení k wifi
* 01.04.2022 - Doplnění funkce GeoIP a dynamického zjišťování počasí dle polohy připojení
* 06.01.2022 - Zprovoznení "Partial Update" - Informační bloky s teplotou, vlhkostí a časem se na displeji meteostanice aktualizují nezávisle na sobě a bez nežádoucího probliknutí LCD při překreslení obsahu.
* 20.01.2022 - OpenWeather - Nově získáváme informace o teplotě a vlhkosti z internetové předpovědi počasí www.openweathermaps.org
* 15.02.2022 - Zobrazení venkovního počasí pomocí bitmap.
* 25.02.2022 - Začlenění funkce geoip. Získáme název města a státu dle přístupového bodu do internetu. To následně použijeme při získávání informací o počasí.
* 06.03.2022 - realizace krabičky.

### Zdroje

V rámci tohoto projektu využívám následující zdroje:

* [ESP32](https://www.espressif.com/en/products/socs/esp32)
* [Arduino](https://www.arduino.cc/)
* [Senzor tlaku, teploty a vlhkosti](https://www.laskarduino.cz/arduino-senzor-tlaku--teploty-a-vlhkosti-bme280/?gclid=Cj0KCQjw_fiLBhDOARIsAF4khR2fvlQXnq_xO4DAD73dtq50rHdLeThwb6clQdZHK3EN6LC8-JH3x-kaAu79EALw_wcB)
* [Snímač intenzity osvětlení](https://www.laskarduino.cz/snimac-intenzity-osvetleni-bh1750/?gclid=Cj0KCQjw_fiLBhDOARIsAF4khR38uxkTHjLC6kHs7Lx63dY76mJ1IqyA4cC-VUMG-MU0f4YmAvNsvpsaAqRwEALw_wcB)
* [WiFi](https://techtutorialsx.com/2017/04/24/esp32-connecting-to-a-wifi-network/) - zprovoznění připojení k WiFi.
* [Useful WiFi library functions](https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/)
* [openweathermaps](https://openweathermap.org/)
