#include <SPI.h>
#include <LoRa.h>               //Biblioteca para comunicação LoRa
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>
#include <time.h>               // Library to get time

#define WIFI_SSID "Cequa-ABBC"
#define WIFI_PASSWORD "irapuca11"
#define FIREBASE_HOST "https://testemonit-b47a7-default-rtdb.firebaseio.com/"  // e.g., yourproject.firebaseio.com
#define FIREBASE_API_KEY "AIzaSyCftQh36nj5qAUFHE6jtW_wcpwCzhPRon0"
#define USER_EMAIL "diogo.soares@icomp.ufam.edu.br"
#define USER_PASSWORD "123123"

// Firebase and authentication
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbData;

// Set up NTP server
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800; // Adjust if needed
const int daylightOffset_sec = 3600;

#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26
#define BAND 915E6 //Frequencia de banda, 915Mhz
// Esp32 Heltec V3
// SX1262 has the following connections:
// NSS pin:   8
// DIO1 pin:  14
// NRST pin:  12
// BUSY pin:  11113
//SX1262 radio = new Module(8, 14, 12, 13);

//Parâmetros da Comunicação LoRa
const int SF = 12; //Spreading Factor : {7, 8, 9, 10, 11, 12}
const int CR = 5; //Coding Rate 4/: {5, 6, 7, 8}
const int BW = 125000; //BandWidth:{125000, 500000}

unsigned long sendDataPrevMillis = 0;
unsigned long count = 0;
int RSSI = 0;

float temperature, ph, tds, turbidez, orp;
String temperatureS, phS, tdsS, turbidezS, orpS;

String getCurrentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "N/A";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);

  // Add milliseconds if you need high precision
  unsigned long ms = millis() % 1000;
  sprintf(buffer + strlen(buffer), ".%03lu", ms);

  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
      Serial.print(".");
      delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = FIREBASE_API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = FIREBASE_HOST;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  // Since Firebase v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbData.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

  // Or use legacy authenticate method
  // config.database_url = DATABASE_URL;
  // config.signer.tokens.legacy_token = "<database secret>";

  Firebase.begin(&config, &auth);

  // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
  Firebase.reconnectNetwork(true);
  Serial.println("Firebase initialized.");

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.println("LoRa Sender...");

  //LoRa Setup
  SPI.begin(SCK, MISO, MOSI, SS); //SPI LoRa pins
  LoRa.setPins(SS, RST, DIO0); //setup LoRa transceiver module

  //Confirmação de que o módulo lora foi iniciado corretamente
  if (!LoRa.begin(BAND)) { //Inicializa o módulo LoRa com a Banda selecionada
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  //Configuração dos Parâmetros LoRa
  LoRa.setSpreadingFactor(SF); //Spreading Factor
  Serial.print("Spreading Factor: ");
  Serial.println(SF);
  LoRa.setCodingRate4(CR);//Coding Rate 
  Serial.print("Coding Rate: ");
  Serial.println(CR);
  LoRa.setSignalBandwidth(BW); //Largura de banda
  Serial.print("Bandwidth: ");
  Serial.println(BW);

  Serial.println("LoRa init succeeded.");
}

//Lampada de potencia do RSSI
void lampPot(){
  Serial.println("Testando...");
  Serial.println(map(RSSI, -139, -50, 0, 255));
  analogWrite(13, map(RSSI, -139, -50, 0, 255));
  unsigned long aux = millis();
  while(millis() - aux < 2000);
  Serial.println("zerando;;;");
  analogWrite(13,0);
}
void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    float randomValue = 6.5 + static_cast<float>(random(0, 3001)) / 1000.0;
    String receivedMessage = String(randomValue, 2);  // Convert to String with 2 decimal places
    //String receivedMessage = "999.99";
    // received a packet
    while (LoRa.available()) {
      //Serial.print((char)LoRa.read());
      receivedMessage = LoRa.readString();
    }
    Serial.print("Received packet '");
    Serial.println(receivedMessage);

    char data[receivedMessage.length() + 1];
    receivedMessage.toCharArray(data, receivedMessage.length() + 1);

    char *token = strtok(data, ";");
    int tokenIndex = 0;
    while (token != NULL) {
      switch (tokenIndex) {
        Serial.println(token);
        case 0:
          
          sscanf(token, "%*[^0123456789.]%f", &temperature);
          break;
        case 1:
          sscanf(token, "pH %f", &ph);
          break;
        case 2:
          sscanf(token, "%fppm", &tds);
          break;
        case 3:
          sscanf(token, "%fntu", &turbidez);
          break;
        case 4:
          sscanf(token, "OPR %f", &orp);
          break;
      }
      
      Serial.print("Token: ");
      Serial.println(token);
      tokenIndex++;
      token = strtok(NULL, ";");
    }

    temperatureS = String(temperature, 2);  // 2 decimal places
    phS = String(ph, 2);
    tdsS = String(tds, 2);
    turbidezS = String(turbidez, 2);
    orpS = String(orp, 2);

    // Print the String values
    Serial.print("Temperature (String): ");
    Serial.println(temperatureS);
    Serial.print("pH (String): ");
    Serial.println(phS);
    Serial.print("TDS (String): ");
    Serial.println(tdsS);
    Serial.print("Turbidity (String): ");
    Serial.println(turbidezS);
    Serial.print("ORP (String): ");
    Serial.println(orpS);

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
    RSSI = LoRa.packetRssi();
    lampPot();

    Serial.print("Firebase ok? ");
    Serial.println(Firebase.ready());

    //tenta enviar a cada 30s
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 30000 || sendDataPrevMillis == 0))
    {
        sendDataPrevMillis = millis();

        FirebaseJson json, jsonTemp, jsonTurb, jsonORP, jsonTDS;
        //json.setDoubleDigits(3);
        //json.add("value", count);
        json.set("PH", ph);  // Use receivedMessage as the dtype value
        json.set("time_created", getCurrentTimestamp()); // Replace with actual timestamp if needed

        // Send data to Firebase at path /PAI/Sensor
        if (Firebase.RTDB.pushJSON(&fbData, "/PAI/Sensor/PH", &json)) {
          Serial.println("Data sent to Firebase successfully.");
        } else {
          Serial.print("Failed to send data to Firebase: ");
          Serial.println(fbData.errorReason());
        }

        jsonTemp.set("Temperatura", temperatureS);  // Use receivedMessage as the dtype value
        jsonTemp.set("time_created", getCurrentTimestamp()); // Replace with actual timestamp if needed

        // Send data to Firebase at path /PAI/Sensor
        if (Firebase.RTDB.pushJSON(&fbData, "/PAI/Sensor/Temperatura", &jsonTemp)) {
          Serial.println("Data sent to Firebase successfully.");
        } else {
          Serial.print("Failed to send data to Firebase: ");
          Serial.println(fbData.errorReason());
        }

        jsonTurb.set("Turbidez", turbidezS);  // Use receivedMessage as the dtype value
        jsonTurb.set("time_created", getCurrentTimestamp()); // Replace with actual timestamp if needed

        // Send data to Firebase at path /PAI/Sensor
        if (Firebase.RTDB.pushJSON(&fbData, "/PAI/Sensor/Turbidez", &jsonTurb)) {
          Serial.println("Data sent to Firebase successfully.");
        } else {
          Serial.print("Failed to send data to Firebase: ");
          Serial.println(fbData.errorReason());
        }

        jsonTDS.set("TDS", tdsS);  // Use receivedMessage as the dtype value
        jsonTDS.set("time_created", getCurrentTimestamp()); // Replace with actual timestamp if needed

        // Send data to Firebase at path /PAI/Sensor
        if (Firebase.RTDB.pushJSON(&fbData, "/PAI/Sensor/TDS", &jsonTDS)) {
          Serial.println("Data sent to Firebase successfully.");
        } else {
          Serial.print("Failed to send data to Firebase: ");
          Serial.println(fbData.errorReason());
        }

        // Send the receivedMessage to Firebase at path /PAI/Sensor/LastRecord
        if (Firebase.RTDB.setString(&fbData, "/PAI/LastRecord/Temperatura", temperatureS) &&
            Firebase.RTDB.setString(&fbData, "/PAI/LastRecord/PH", phS) &&
            Firebase.RTDB.setString(&fbData, "/PAI/LastRecord/TDS", tdsS) &&
            Firebase.RTDB.setString(&fbData, "/PAI/LastRecord/Turbidez", turbidezS) &&
            Firebase.RTDB.setString(&fbData, "/PAI/LastRecord/ORP", orpS)) {
          Serial.println("LastRecord sent to Firebase successfully.");
        } else {
          Serial.print("Failed to send LastRecord to Firebase: ");
          Serial.println(fbData.errorReason());
        }

        //Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, "/test/json", &json) ? "ok" : fbdo.errorReason().c_str());
        //Serial.printf("Get json... %s\n", Firebase.RTDB.getJSON(&fbdo, "/test/json") ? fbdo.to<FirebaseJson>().raw() : fbdo.errorReason().c_str());
        //FirebaseJson jVal;
        //Serial.printf("Get json ref... %s\n", Firebase.RTDB.getJSON(&fbdo, "/test/json", &jVal) ? jVal.raw() : fbdo.errorReason().c_str());
        /*
        FirebaseJsonArray arr;
        arr.setFloatDigits(2);
        arr.setDoubleDigits(4);
        arr.add("a", "b", "c", true, 45, (float)6.1432, 123.45692789);

        Serial.printf("Set array... %s\n", Firebase.RTDB.setArray(&fbdo, "/test/array", &arr) ? "ok" : fbdo.errorReason().c_str());

        Serial.printf("Get array... %s\n", Firebase.RTDB.getArray(&fbdo, "/test/array") ? fbdo.to<FirebaseJsonArray>().raw() : fbdo.errorReason().c_str());

        Serial.printf("Push json... %s\n", Firebase.RTDB.pushJSON(&fbdo, "/test/push", &json) ? "ok" : fbdo.errorReason().c_str());

        json.set("value", count + 0.29745);
        Serial.printf("Update json... %s\n\n", Firebase.RTDB.updateNode(&fbdo, "/test/push/" + fbdo.pushName(), &json) ? "ok" : fbdo.errorReason().c_str());
        */
        count++;
    }
  }

  delay(200);
}
