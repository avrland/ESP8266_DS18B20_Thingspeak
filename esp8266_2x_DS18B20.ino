//Prosty przykład implementacji ESP8266 z termometrem DS18B20 x2
//Wysyłka pomiarów na serwery Thingspeak
//avrland.it

#include <ESP8266WiFi.h>
#include <OneWire.h>


//USTAWIENIA
OneWire  ds(2);  // numer pinu do którego podpięto czujnik
const char* ssid     = ""; //<- SSID Twojej sieci
const char* password = ""; //<- Hasło do Twojej sieci
const char* host = "184.106.153.149"; //adres ThingSpeak
const String api_key = ""; //klucz API
//KONIEC USTAWIEŃ

//ZMIENNE DLA POMIARÓW
byte i;
byte present = 0;
byte type_s;
byte data[12];
byte addr[8];
float celsius;
float kanal1, kanal2; //zmienne przechowywujące wartość pomiaru
byte czujnik1[8], czujnik2[8]; //zmienne na potrzeby przechowywania adresów obecnych czujników 
//KONIEC ZMIENNYCH DLA POMIARÓW


void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Termometr DS18B20 IoT @ Thingspeak");
  Serial.println();
  Serial.print("Lacze do ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA); //we don't want to broadcast SSID so we put ESP8266 to standalone mode
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi polaczone");  
  Serial.println("Adres IP: ");
  Serial.println(WiFi.localIP());
  /*
   * Sekwencja uzyskiwania adresów czujników
   */
  ds.search(czujnik1);
  ds.search(czujnik2);
  Serial.print("ROM czujnik pierwszy =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(czujnik1[i], HEX);
  }
  Serial.println("");
  Serial.print("ROM czujnik drugi =");
   for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(czujnik2[i], HEX);
  }
  Serial.println("");
}
/*##############
*	PĘTLA GŁÓWNA
*/
void loop() {
    kanal1 = pomiar(czujnik1); //zmierz temperature na pierwszym czujniku
    kanal2 = pomiar(czujnik2); //zmierz temperature na drugim czujniku
    wyslij_pomiar(kanal1, kanal2, api_key); //wyslij pomiar
    kanal1 = pomiar(czujnik1);
    kanal2 = pomiar(czujnik2);
    wyslij_pomiar(kanal2, kanal1, api_key);
}

/*
 * Wysyłka pomiaru do Thingspeak
 * field1 - wartość na pierwszy wykres
 * field2 - wartość na pierwszy wykres
 * api_key - klucz api
 */
void wyslij_pomiar(float field1, float field2, String api_key){
    Serial.print("Laczymy do: ");
    Serial.println(host);
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    //return 0;
    }
    String url = "/update?key=" + api_key + "&field1="; //<- TU WKLEJ KOD DO API
    url += (String)field1;
    url+= "&field2=" + (String)field2;
    Serial.print("Requesting URL: ");
    Serial.println(url);
    
    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" + 
             "Connection: close\r\n\r\n");
    delay(10);
    
    // Read all the lines of the reply from server and print them to Serial
    while(client.available()){
         String line = client.readStringUntil('\r');
         Serial.print(line);
    }
    
    Serial.println();
    Serial.println("##############################");
    Serial.println("KONIEC POMIARU");
    Serial.println("##############################");
    delay(3000);
}
/*
 * POMIAR DS18B20
 * 
 * adres - 8 bitowy adres czujnika
 */
float pomiar(byte adres[8]){ 
  ds.reset();
  ds.select(adres);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(adres);    
  ds.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
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
  celsius = (float)raw / 16.0;
  Serial.print("  Temperature = ");
  Serial.println(celsius);
  return celsius;
}

