#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <ClosedCube_HDC1080.h>
#include <Arduino.h>


#pragma region Variables globlales
// Declarar variables
static const int RXPin = 2, TXpin=0;
static const uint32_t GPSBaud = 9600;

static int timesMeasure = 0;

//Temporizador para enviar el mensaje
unsigned long lastSendTime = 0;
unsigned long currentTime = 0;
const unsigned long SEND_INTERVAL = 10000;

int state = 0; // estado que se encuentra la maquina
enum Estados {
  ESTADO_INICIAL = 0,
  LEER_HyT = 1,
  LEER_GPS = 2,
  IMPRIMIR_VALORES = 3,
  RECOLETAR_VALORES = 4,
  HACER_PRUNNING = 5
};

//creacion de todos los objetos necesarios para los sensores
SoftwareSerial ss(RXPin,TXpin);
TinyGPSPlus gps; ClosedCube_HDC1080 sensor;


//Cosas de internet
const char* ssid = "UPBWiFi";
const char* password = "";
const char* serverIp = "98.85.128.81";
WiFiClient client;

int id = 16;

// Variables para almacenar los datos recibidos
float temp, hum, lat, lng;
float tempT, humT, latT, lngT; //Acumulado pa luego hacer promedios

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

//Se encarga de imprimir los valores que recibe en consola y darle el formato requerido para el servidor
void PrintValues(float lat, float lng, float temp, float hum){

  //Creacion del texto en formato para el Json
  String PostData="{";
  PostData += "\"id\":\"point"+String(id) + "\",";
  PostData += "\"lat\":" + String(lat,6) + ",";
  PostData += "\"lon\":" + String(lng,6) + ",";
  PostData += "\"temperatura\":" + String(temp,5) + ",";
  PostData += "\"humedad\":" + String(hum,5) + ",";
  PostData += "\"metadata\":{}";
  PostData += "}";

  Serial.println("Json a enviar al servidor: -----");
  Serial.println(PostData); //Poder ver que si este bien escrito
  Serial.println();
  //Iniciar la conexion con el servidor
  if (client.connect(serverIp,80)){
    Serial.print("conectado");
    client.println("POST /update_data HTTP/1.1");
    client.print("Host: ");
    client.println(serverIp);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(PostData.length());
    client.println();
    client.print(PostData);

    client.stop(); //No parece necesario pero la IA lo quiso poner
  }
  else {Serial.println("ERROR de conexion");}
}

//Leer el sensor de temperatura
void ReadTempHum(){
  double temperatura = sensor.readTemperature();
  double humedad = sensor.readHumidity();

  hum = humedad;
  temp = temperatura;

  //verificar
  Serial.print(hum, 5); Serial.print("-- y --") ;Serial.print(temp, 5);
  Serial.println();
}

//Leer el sensor GPS
void ReadGPS(){
  for (size_t i = 0; i < 10; i++)
  {
  smartDelay(50);
  }
  
  lat = gps.location.lat();
  lng = gps.location.lng();

  //verificar
  Serial.print(lat,6); Serial.print("-- y --") ;Serial.print(lng,6);
  Serial.println();
}

//Depues de recibir los chirp de ambos tipos de sensores almacena los datos
void CollectingProcess(){
  //En vez de almacenarlo en un array, directamente los sumo en una variable TOTAL,
  //Ya que en el plazo de 10 segundos entre envios puede haber desfaces de cantidad
  //de veces que se envia, ya que no tengo calculado lo que demora el proceso completo de medicion
  // y fin de cuentas es lo mismo pero saltandome el paso de guardar todos los chirps ya que no los necesito para este caso
  tempT += temp;
  humT += hum;
  latT += lat;
  lngT += lng;

  timesMeasure++; //Suma a las rondas de mediciones que se hallan hecho
}

//Proceso de podado de datos antes de mandar el valor definitivo al server, simplemente una MEDIA de los recolectados
void PrunningProcess(){
  if (timesMeasure != 0){ 
    tempT /= timesMeasure;
    humT /= timesMeasure;
    latT /=  timesMeasure; 
    lngT /= timesMeasure;
  }  // evitar errores
}

//Reiniciar los datos necesarios para el proceso
void ResetValues(){
  //para volver a comenzar mediciones...
  tempT = 0;
  humT = 0;
  latT = 0;
  lngT = 0;
  timesMeasure = 0;
}

//Saber si han pasado o no los 10 segundo pedidos entre envios
//Si pasaron, pasamos a hacerlo, sino pasa al estado que se desee seguir
void CheckTime(int stateToGoIfNot){
  currentTime = millis();
  
  if (currentTime - lastSendTime >= SEND_INTERVAL){
    state = HACER_PRUNNING;
  } else {
    state = stateToGoIfNot;
  }
}
#pragma endregion



void setup() {
  Serial.begin(115200); //Comunicacion con la board
  ss.begin(GPSBaud); //GPS

  sensor.begin(0x40); Wire.begin(); //Temp y H

  Serial.println("Starting sensors");
  state = LEER_HyT;
  //Cosas WiFi
  pinMode(A0, INPUT);
  WiFi.begin(ssid, password);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  delay(100);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");

  lastSendTime = millis(); //Comenzar el temporizador del envio
  delay(1000);
}

void loop() {
  delay(500);
  smartDelay(10);
  //Empezando el ciclo
  Serial.println("Comenzando procesos");
  switch (state)
{
  case LEER_HyT:
  //Leer humendad y temp
    Serial.println("Leyendo H y T");
    ReadTempHum();
    
    smartDelay(50);
    state = LEER_GPS;
  break;

  case LEER_GPS:
  //Leyendo posición GPS
    Serial.println("Leyendo GPS");
    ReadGPS();

    smartDelay (50);
    state = RECOLETAR_VALORES;
  break;
  case IMPRIMIR_VALORES:
    PrintValues(latT, lngT, tempT, humT); // enviar e imprimir los datos recogidos
    smartDelay(100);

    ResetValues(); // vaciar la recoleccion de dato que ya fueron enviados
    lastSendTime = millis(); // reiniciar temporizador
    state = LEER_HyT;
  break;
  case RECOLETAR_VALORES:
  //simplemente despues de medir, juntar todo en un mismo lugar
    CollectingProcess();
    
    smartDelay(20);
    CheckTime(LEER_HyT);//saber si enviar o seguir leyendo
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