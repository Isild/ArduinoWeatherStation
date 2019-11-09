#include <stdlib.h>
#include <SoftwareSerial.h>
#include <dht11.h>
#include <LiquidCrystal.h>
#include <string.h>
#include <OneWire.h>
#include <LPS331.h>
#include <Wire.h>

///----------------------PINY---------------------------------//
#define DHT11PIN 13       //termometr z higrometrem
#define LCD_V0 12         //LCD
#define LCD_E 11
//#define LCD_RW 10
#define LCD_D4 5
#define LCD_D5 4
#define LCD_D6 3
#define LCD_D7 2
#define RX 0              //RX na arduino
#define TX 1
#define DS18B20 10        //prosty termometr
#define RainSensorD 7     //czujnik deszczu
#define RainSensorA A0
#define HumiGrSensorD 8   //czujnik wilgotności gleby
#define HumiGrSensorA A1
//Czujnik ciśnienia na A5 i A4

///------------------PARAMETRY USTWIEŃ------------------------//
#define ESP_Bod 115200   //prędkość nadawania modułu WiFi (9600/115200)

///----------------------LCD----------------------------------//
LiquidCrystal lcd(LCD_V0, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

///----------------------WiFi---------------------------------//
SoftwareSerial monitor(0, 1);// RX, TX
String GET;
String Thingspeak_KEY = ""; //Tu wpisz swój Thingspeak KEY.
String NazwaSieci = ""; //Tu podaj nazwe swojej sieci.
String HasloSieci = ""; //Tu hasło do sieci.

///----------------------CZUJNIKI------------------------------//
dht11 DHT11;          //termometr z wilgotnością
OneWire ds(DS18B20);  //prosty termometr
LPS331 ps;               //czujnik ciśnienia

///------------------ODCZYT PARAMETRÓW-------------------------//
float TempC = 0;        //Temperatura chwilowa
float Temp = 0;         //Temperatura
float Humi = 0;         //wilgotność powietrza
float Rain = 0;         //opady deszczu
float isRain = 0;       //czy są opady deszczu
float Pres = 0;         //ciśnienie
float HumiGr = 0;       //wilgotność gleby
float isGrounWet = 0;   //czy ziemia jest wilgotna

void setup() {
    setLCD();
    setEspOptions();
    pinMode(RainSensorD, INPUT);    // ustawienie pinu RainSensorD jako wejście czujnika opadów
    Wire.begin();
    if (!ps.init()){                // inicjalizacja ciśnieniomierza
      writeMessage("Czujnik cisnienia", "Blad wykrycia sensora", 450);
      while (1);
    }                      
    ps.enableDefault();
    connect();
}

void loop() {
  //Temperatura i wilgotność
  /*if(turnOnMeasurment == -1 || turnOnMeasurment == 0){  //jeżeli poprawnie pobrano dane
    updateTemp(readTemp());
    updateHumi(readHumi());
  }
  else {
    updateTemp(TempC);
    updateHumi(Humi);
  }/*

  updateTemp(readTemp());
  updateHumi(readHumi());//*/

  turnOnMeasurment();

  readTemp();
  readHumi();
  readPressure();
  readHumiGrValue();
  readRainValue();
  readTempDS18B20();
  updateParameters((int)TempC, (int)Humi, (int)Temp, (int)Pres, (int)HumiGr, (int)Rain);

  showParameters(20000);
  //delay(30000);//30sek czekania na termometr
}

//Funkcja wyzwalająca pomiar na termometrze
//Return
//1 - zwrócone dane są poprawne
//0 - wystąpił błąd 
//-1 - błąd sumy kontrolnej
//-2 - koniec czasu oczekiwania
//-3 - nieznany błąd
short turnOnMeasurment(){
  int chk = DHT11.read(DHT11PIN);

  delay(250);
  switch (chk)
  {
    case DHTLIB_OK: 
      //writeMessage("Termometr", "OK", 200); 
      return 1;
    break;
    case DHTLIB_ERROR_CHECKSUM: 
      //writeMessage("Termometr", "blad sumy kontrolnej", 200);  
      return -1;
    break;
    case DHTLIB_ERROR_TIMEOUT: 
      //writeMessage("Termometr", "koniec czasu oczekiwania - brak odpowiedzi", 200);
      return -2;
    break;
    default: 
      //writeMessage("Termometr", "nieznany blad", 200);
      return -3;
    break;
  }
  return 0;
}

//Funkcja czytająca temperaturę z termometru dth11
float readTemp(){
  float tempF;
  tempF = (float)DHT11.temperature;
  TempC = tempF;

  delay(50);
  return TempC;
}

//Funkcja czytająca wilgotność z termometru dth11
float readHumi(){
  //int humiF;
  Humi = (float)DHT11.humidity;
  //Humi = Humi + humiF;

  delay(50);
  return Humi;
}

//Funkcja czytająca temperaturę z termometru DS18B20
float readTempDS18B20(){
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float tempF;

  if ( !ds.search(addr)) {
    ds.reset_search();
    delay(250);
    return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);
  delay(1000);
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);
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
  Temp = (float)raw / 16.0;
  
  //Temp = Temp + (int)tempF;
  delay(50);
  return Temp;
}

//Funkcja odczytująca parametry z czujnika deszczu
//Return
//Rain - natężenie opadów
int readRainValue(){
  int value_A0;       //opisuje liczbami natężenie deszczu
  int value_D0;       //wykrywa wartości 0,1 pada, nie pada

  value_A0 = analogRead(RainSensorA);
  value_D0 = digitalRead(RainSensorD);

  Rain = 1000 - value_A0;              //950 to wartość gdy nie jest wilgotno, zwracana jest rezystancja
  
  if(value_D0 == 1){  //nie ma deszczu
    isRain = 0;
  }
  else if(value_D0 == 0){//jest deszcz
    isRain = 1;
  }
  else{             //błąd
    isRain = -1;
  }
  
  delay(50);
  return Rain;
}

//Funkcja odczytująca parametry z czujnika wilgotnosci gleby
//Return
//HumiGr - wilgotność gleby
//  0  ~300     sucha gleba
//  300~700     wilgotna gleba
//  700~1000     w wodzie
int readHumiGrValue(){
  int value_A0;       //opisuje liczbami natężenie wilgoci w glebie
  int value_D0;       //wykrywa wartości 0,1 wilgotno, nie wilgotno

  value_A0 = analogRead(HumiGrSensorA);
  value_D0 = digitalRead(HumiGrSensorD);

  HumiGr = 1000 - value_A0;
  if(value_D0 == 1){
    //ziemia sucha
    isGrounWet = 0;
  }
  else{
    //ziemia mokra
    isGrounWet = 1;
  }

  delay(50);
  return HumiGr;
}

//Funkcja czytająca dane z czujnika ciśnienia. Może czytać także temperaturę oraz ciśnienie przeliczać na wysokość.
int readPressure(){
  float pressure = ps.readPressureMillibars();
  //float altitude = ps.pressureToAltitudeMeters(pressure);
  //float temperature = ps.readTemperatureC();
  
  Pres = (int)pressure;

  delay(50);
  return Pres;
}

//Funkcja łącząca urządzenie z WiFi
void connect(){
  writeMessage("Siec", "Rozpoczeto", 10);
  monitor.println("AT+CWMODE=1");
  delay(250);
  String cmd = "AT+CWJAP=\""+NazwaSieci+"\", \""+HasloSieci+"\"";
  monitor.println(cmd);
  writeMessage("Siec", "Polaczono", 250);
}

//Funkcja wysyłająca wszystkie dane 
//humi - odczytana wilgotność
//tempC - temperatura chwilowa
//temp - temperatura
//humi - wilgotność powietrza
//pres - ciśnienie
//humiGr - wilgotność gleby
//rain - opady
void updateParameters(int tempC, int humi, int temp, int pres, int humiGr, int rain){
  short lon=0;
  //writeMessage("Update", "Rozpoczeto", 250);/*
  delay(250);//*/
  GET = "GET https://api.thingspeak.com/update?key=";
        GET+= Thingspeak_KEY;
        GET += "&field1=";
  String command = GET;
  command += String(tempC) + "&field2=" + String(humi) + "&field3=" + String(temp) + "&field4=" + String(pres) + "&field5=" + String(humiGr) + "&field6=" + String(rain) + "&field7=" + String(isRain) + "&field8=" + String(isGrounWet);
  lon = command.length() + 3;
  command += "\n\r\n\r";
  command += "\n";
  monitor.println("AT+CIPSTART=4, \"TCP\",\"api.thingspeak.com\",80");
  delay(1000);
  //writeMessage("AT+CIPSEND=4," + String(lon), ":)", 2000);/*
  delay(2000);//*/
  monitor.println("AT+CIPSEND=4," + String(lon));
  delay(1000);
  monitor.println(command);
  delay(1000);
  monitor.println(command);
  //writeMessage("Update", "Zakonczono", 250);/*
  delay(250);//*/
  monitor.println("AT+CIPCLOSE");
  delay(1000);
}

//Funkcja uruchamiająca LCD
void setLCD(){
  lcd.begin(16, 2); // ustawienie typu wyświetlacza, w tym wypadku 16x2
  lcd.setCursor(0, 1); // kolumna nr 0, wiersz nr 1 (numerowanie rozpoczyna się od zera)
  lcd.clear();
  writeMessage("LCD Zasilanie", "OK", 250);
}

//Funkcja ustawiająca parametry ESP-01S za pomocą komend AT
void setEspOptions(){
  monitor.begin(ESP_Bod);
  writeMessage("Monitor", "115200", 450);
  monitor.println("AT+CIPMUX=1");
  writeMessage("Monitor", "CIPMUX", 450);
}

//Funkcja wypisująca wiadomość
//mess_name - nazwa komunikatu
//mess - wiadomosc
//mSec - czas oczekiwania po komunikacie w mili sekundach
void writeMessage(String mess_name, String mess, int mSec){
  int len = mess.length();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(mess_name + ":");
  lcd.setCursor(0, 1);
  lcd.print(mess);
  
  delay(150);
  if(len>16){
    for(int i=0; i<len-16; i++){
      lcd.setCursor(0, 0);
      lcd.print(mess_name + ":");
      lcd.setCursor(0, 1);
      lcd.scrollDisplayLeft();
      delay(150);
    }
  }
  delay(mSec);
}

//TODO: żeby zmieściły się skróty parametrów zamienić na angielki i będzie ok
//Funkcja wyświetlająca parametry pogody na ekranie LCD
//mSec - czas oczekiwania po komunikacie w mili sekundach
void showParameters(int mSec){
  char stC = 223;//znak odpowiadający za stopień
  lcd.clear();
  lcd.setCursor(0, 0);                  //Temperatura
  lcd.print(String((int)Temp) + stC + "C"); //9 znaków
  lcd.setCursor(0, 1);                  //Ciśnienie
  lcd.print(String((int)Pres) + "hPa"); //9 znaków
  lcd.setCursor(8, 0);                  //Wilgotność powietrza
  lcd.print(String((int)Humi) + "%"); //5 znaków
  
  lcd.setCursor(8, 1);                  //Wilgotność gleby
  //lcd.print("Wg:" + String(HumiGr) + "%"); //7 znaków 
  if(HumiGr >= 0 && HumiGr < 300){
    lcd.print("Sucho"); //5 znaków 
  }
  else if(HumiGr >= 300 && HumiGr < 700){
    lcd.print("Wilgotno"); //8 znaków
  }
  else if(HumiGr >= 700){
    lcd.print("Przelane"); //8 znaków
  }
  else {
    lcd.print("Err 404"); //7 znaków
  }
  delay(mSec);
}
