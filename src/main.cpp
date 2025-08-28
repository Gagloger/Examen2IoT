#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <TinyGPSPlus.h>
#include <ClosedCube_HDC1080.h>
#include <Arduino.h>


#pragma region Variables globlales
// Declarar variables
static const int RXPin = 2, TXpin=0;
static const uint32_t GPSBaud = 9600;

static int timesMeasure = 0;

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 10000;

int state = 0; // estado que se encuentra la maquina
enum Estados {
ESTADO_INICIAL = 0,
LEER_HyT = 1,
LEER_GPS = 2,
IMPRIMIR_VALORES = 3,
RECOLETAR_VALORES = 4,
HACER_PRUNNING = 5,
REVISAR_TIEMPO = 6
};

//creacion de todos los objetos necesarios para los sensores
SoftwareSerial ss(RXPin,TXpin);
TinyGPSPlus gps; ClosedCube_HDC1080 sensor;

// Variables para almacenar los datos recibidos
float temp, hum, lat, lng;
float tempT, humT, latT, lngT; //pa lueo hacer promedios

#pragma endregion

#pragma region Funciones creadas
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
  while (ss.available())
    gps.encode(ss.read());
  } while (millis() - start < ms);
}

void PrintValues(float lat, float lng, float temp, float hum){
  // Mirar como hacer el Json
  Serial.print("id:point16,");
  Serial.print("lat:");
  Serial.print(lat, 6);
  Serial.print(",lon:");
  Serial.print(lng, 6);
  Serial.print(",Temperatura:");
  Serial.print(temp, 4);
  Serial.print(",Humedad:");
  Serial.print(hum, 4);
  Serial.print("metadata:");

  Serial.println();
}

void ReadTempHum(){
  double temperatura = sensor.readTemperature();
  double humedad = sensor.readHumidity();

  hum = humedad;
  temp = temperatura;

  for (size_t i = 0; i < 10; i++)
  {
  smartDelay(50);
  }
}

void ReadGPS(){
  for (size_t i = 0; i < 10; i++)
  {
  smartDelay(300);
  }
  
  lat = gps.location.lat();
  lng = gps.location.lng();
}

void CollectingProcess(){
  tempT += temp;
  humT += hum;
  latT += lat;
  lngT += lng;

  timesMeasure++;
}

void PrunningProcess(){
  if (timesMeasure = 0) return; // evitar errores

  tempT /= timesMeasure;
  humT /= timesMeasure;
  latT /=  timesMeasure; 
  lngT /= timesMeasure;
}

void ResetValues(){
  //para volver a comenzar mediciones...
  tempT = 0;
  humT = 0;
  latT = 0;
  lngT = 0;
  timesMeasure = 0;
}
#pragma endregion



void setup() {
  Serial.begin(115200);
  ss.begin(GPSBaud);

  sensor.begin(0x40);

  delay(100);
  Serial.println("Starting sensors");
  state = LEER_HyT;

  lastSendTime = millis(); //Comenzar el temporizador del envio
  delay(1000);
}

void loop() {
  delay(1000);
  smartDelay(10);
  //Empezando el ciclo
  Serial.println("Comenzando procesos");
  switch (state)
{
  case LEER_HyT:
  //Leer humendad y temp
    Serial.println("Leyendo H y T");
    ReadTempHum();
    state = LEER_GPS;
  break;

  case LEER_GPS:
  //Leyendo posición GPS
    Serial.println("Leyendo GPS");
    ReadGPS();
  
    state = RECOLETAR_VALORES;
  break;
  case IMPRIMIR_VALORES:
//Cuando este conectado enviarlos al server?
    PrintValues(latT, lngT, tempT, humT); // enviar o imprimir los datos recogidos hasta el momento
    smartDelay(100);

    ResetValues(); // vaciar la recoleccion de dato que ya fueron enviados
    lastSendTime = millis(); // reiniciar temporizador
    state = LEER_HyT;
  break;
  case RECOLETAR_VALORES:
  //simplemente despues de medir, juntar todo en un mismo lugar
    CollectingProcess();
    
    smartDelay(50);
    state = REVISAR_TIEMPO;
  break;
  case REVISAR_TIEMPO:
  unsigned long currentTime = millis();

  if (currentTime - lastSendTime >= SEND_INTERVAL){
    state = HACER_PRUNNING;
  } else {
    state = LEER_HyT;
  }
    smartDelay(50);
  break;
  case HACER_PRUNNING:
  // Si entra aqui se preparan los valores pa subirlos al servidor
    PrunningProcess();
    state = IMPRIMIR_VALORES;
  break;
  default:
    Serial.println("por aqui no era, abortar mision, ERROR EN EL SENSOR");
    delay(1000);
    //supongo que devolver al estado inicial o parar programa...
    break;
  }
}