#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include "time.h"

#define LED 25
#define BOTON1 35
#define BOTON2 34
#define DHTPIN 23

#define P1 0
#define P2 1
#define E1 2
#define E2 3
#define SegA 4
#define SegD 5

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#define WIFI_SSID "ORT-IoT"
#define WIFI_PASSWORD "OrtIOT24"

// Insert Firebase project API Key
#define API_KEY "AIzaSyAXNjSVaqoo2Eo0HNr8DQG2qpXg0ubGLjw"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "edgar@gmail.com"
#define USER_PASSWORD "edgar123"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://tp5-firebase-957cf-default-rtdb.firebaseio.com"

// Define Firebase objects
FirebaseData fbdo; // Comunicación
FirebaseAuth auth; // Autenticación
FirebaseConfig config; // Configuracion

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
String databasePath;
// Database child nodes
String tempPath = "/temperature";
String timePath = "/timestamp";

// Parent Node (to be updated in every loop)
String parentPath;

int timestamp;
FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

void initWifi(void) {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
  }
  time(&now);
  return now;
}

// Timer variables (send new readings every three minutes)
unsigned long sendDataPrevMillis = 0; 
unsigned long cicloGuardado = 30;  // es el ciclo de guardado en segundos
unsigned long timerDelay = cicloGuardado;

int estado = 0;
float temperaturaActual;
char stringTemperaturaActual[5];  // lo que lee el DHT
char stringcicloGuardado[5];
// arreglo de caracteres para almacenar valoresde numeros convertidos a texto

#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

void setup() {
  Serial.begin(115200);
  dht.begin();
  u8g2.begin();
  pinMode(BOTON1, INPUT_PULLUP);
  pinMode(BOTON2, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  WiFi.mode(WIFI_STA);
  initWifi();

  configTime(0, 0, ntpServer);

  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true); // el true habilita la reconexión automática a Wi-Fi en caso de que la conexión se pierda
  fbdo.setResponseSize(4096); //establece el tamaño máximo de la respuesta que el dispositivo puede recibir de Firebase 4096 bytes permite comuniccion entre el dispositivo y firebase

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  //  establece una función de callback para monitorear el estado del token.

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5; //número máximo de intentos que el sistema hará para generar o renovar el token de autenticación

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/readings";
}

void loop() {
  
  
  //Serial.print("hola");
  u8g2.clearBuffer();
  temperaturaActual = dht.readTemperature();
  
  // Verificar si la lectura de la temperatura es válida
  if (isnan(temperaturaActual)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  maqde();
  u8g2.sendBuffer();
}

void maqde (void) {
  switch (estado) {
    case P1:
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(0, 15, "Temperatura Actual: ");
      sprintf(stringTemperaturaActual, "%.2f", temperaturaActual);
      u8g2.drawStr(10, 30, stringTemperaturaActual);
      u8g2.drawStr(50, 30, "°C");

      u8g2.drawStr(0, 45, "Ciclo de guardado: ");
      sprintf(stringcicloGuardado, "%d", cicloGuardado);
      u8g2.drawStr(5, 60, stringcicloGuardado);
      u8g2.drawStr(30, 60, "Segundos");

      if ((digitalRead(BOTON1) == LOW) && (digitalRead(BOTON2) == LOW)) {
        estado = E1;
      }

      // Send new readings to database
      if (Firebase.ready() && (millis() - sendDataPrevMillis > cicloGuardado * 1000 || sendDataPrevMillis == 0)) {
        sendDataPrevMillis= millis();//Tiempo Actual
       //variable que almacena el momento en que los datos fueron enviados por última vez millis
       //tiempo de ultimo envío pervmillis

        // Get current timestamp
        timestamp = getTime();
        Serial.print("time: ");
        Serial.println(timestamp);

        parentPath = databasePath + "/" + String(timestamp); //Construcción de la ruta de la base de datos en donde se guardan y se envian esos datos

        json.set(tempPath, temperaturaActual);  // Se prepara para enviar un objeto a firebase
        Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
      }
      // Intenta enviar el objeto al realtimedatabase de la base de datos
      break;

    case P2:
      u8g2.drawStr(0, 15, "Ciclo de guardado: ");
      sprintf(stringcicloGuardado, "%d", cicloGuardado);
      u8g2.drawStr(5, 30, stringcicloGuardado);
      u8g2.drawStr(30, 30, "Segundos");

      if ((digitalRead(BOTON1) == LOW) && (digitalRead(BOTON2) == LOW)) {
        estado = E2;
      }
      if (digitalRead(BOTON1) == LOW) {
        estado = SegA;
      }
      if (digitalRead(BOTON2) == LOW) {
        estado = SegD;
      }
      break;

    case E1:
      if ((digitalRead(BOTON1) == HIGH) && (digitalRead(BOTON2) == HIGH)) {
        estado = P2;
      }
      break;

    case E2:
      if ((digitalRead(BOTON1) == HIGH) && (digitalRead(BOTON2) == HIGH)) {
        estado = P1;
      }
      break;

    case SegA:
      if (digitalRead(BOTON1) == HIGH) {
        cicloGuardado += 30;  // Aumenta ciclo de guardado en 30 segundos
        estado = P2;
      }
      if (digitalRead(BOTON2) == LOW) {
        estado = E2;
      }
      break;

    case SegD:
      if (digitalRead(BOTON2) == HIGH) {
        cicloGuardado -= 30;  // Disminuye ciclo de guardado en 30 segundos
        if (cicloGuardado < 30) {
          cicloGuardado = 30;  // No permitir que sea menor de 30 segundos
        }
        estado = P2;
      }
      if (digitalRead(BOTON1) == LOW) {
        estado = E2;
      }
      break;
  }
}
// el token  es una pieza de información digital que sirve para autenticar y autorizar a un usuario o dispositivo para acceder a ciertos recursos o servicios
//un token de acceso es una cadena de caracteres que contiene datos sobre la identidad del usuario, permisos y tiempo de validez, 
//permitiendo que el usuario acceda a los recursos del sistema sin tener que ingresar su contraseña en cada solicitud.

