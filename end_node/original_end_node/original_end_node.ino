// Imports //
#include <EEPROM.h>
#include <SPI.h>                //Dependencia da biblioteca LoRa.h
#include <LoRa.h>               //Biblioteca para comunicação LoRa
#include "thermistor.h"         //Biblioteca para uso de Thermistors (Sensor de Temperatura)
#include <Adafruit_ADS1X15.h>   //Biblioteca para o uso do Analogic to Digital Serial (ADS 1115)
#include "GravityTDS.h"         //Biblioteca para o uso do TDS
//#include "log.hh"               //Biblioteca para o uso do SD
//End Imports/

//Global Variables
#define TAMANHO_MAXIMO 5
#define EEPROM_SIZE 512
#define S_To_uS_Factor 1000000ULL
#define Time_To_Sleep 60

struct Limite {
    const char* categoria;
    float limite_inferior;
    float limite_superior;
};

Limite outliers[] = {
    {"Temperatura", 0, 100},
    {"PH", 3, 9}
};


// Variáveis armazenadas na memória RTC
RTC_DATA_ATTR float valores_temperatura[TAMANHO_MAXIMO] = {0};
RTC_DATA_ATTR float valores_ph[TAMANHO_MAXIMO] = {0};
RTC_DATA_ATTR int num_valores_temperatura = 0;
RTC_DATA_ATTR int num_valores_ph = 0;

//PH Variables
float valor_calibracao = 21.5; // Fator de calibração para o ajuste fino
//End PH Variables

//Lora Variables

//Pinos utilizados na comunicação LoRa
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26
#define BAND 915E6 //Frequencia de banda, 915Mhz

//Parâmetros da Comunicação LoRa
const int SF = 12; //Spreading Factor : {7, 8, 9, 10, 11, 12}
const int CR = 8; //Coding Rate 4/: {5, 6, 7, 8}
const int BW = 125000; //BandWidth:{125000, 500000}

// //Demais variáveis
RTC_DATA_ATTR byte msgCount = 0;            // Contador do número de mensagens, utilizado para distriuição de ID da mensagem
bool ACK = false;             //Utilizado para repetir o envio até receber o ACK
byte localAddress = 0x1E;     //Endereço do dispositivo, 20
byte destination = 0xFF;      //Endereço de Destino, FF = Broadcast (255)
long lastSendTime = 0;        // last send time
int interval = 3000;          // interval between sends

//End LoRa Variables

//Thermistor Variables
#define NTC_PIN 13 // Analog pin used to read the NTC

// Thermistor object
THERMISTOR thermistor(NTC_PIN,        // Analog pin
                      10000,          // Nominal resistance at 25 ºC
                      3950,           // thermistor's beta coefficient
                      10000);         // Value of the series resistor
float temp; //Variável para armazenar o valor da temperatura

//End Thermistor Variables


// ADS Variables
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
//End ADS Variables


//TDS Variables
#define TdsSensorPin 12 //Pino onde está o TDS
GravityTDS gravityTds;  //Variável utilizada pela biblioteca para realizar as medições
//End TDS Variables

void setup() {
  //Serial Setup
  Serial.begin(115200);
  delay(1000);
  while (!Serial);
  //End Serial Setup
  esp_sleep_enable_timer_wakeup(Time_To_Sleep * S_To_uS_Factor);
  //

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
  //End LoRa Setup

  //ADS Setup
  Serial.println("Getting single-ended readings from AIN0");
  Serial.println("ADC Range: +/- 6.144V (1 bit = 3mV/ADS1015, 0.1875mV/ADS1115)");
  if (!ads.begin()){
    Serial.println("Failed to initialize ADS.");
    while (1);
  }
  //END ADS Setup

  //TDS Setup
  gravityTds.setPin(TdsSensorPin); //define o pino para medição
  gravityTds.setAref(3.3);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(4096);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization
  gravityTds.setKvalue(1.18);
  //End TDS Setup


  //SD Setup
//  log.logInit(dadosINPA.txt);
}

void loop() {
  String outgoing = coleta(); 
  Serial.print("Dados dos sensores: ");
  Serial.println(outgoing);            // outgoing message
  // while(!ACK){
  sendMessage(outgoing);  //Inicializa o pacote que vai ser enviado atráves de LoRa
   // onReceive(LoRa.parsePacket());
    // delay(10000);
  // }
  msgCount++;
  // ACK = false;
  // delay(300000);
  esp_deep_sleep_start();
//  log.logPrintf("%d",outgoing);
}

void sendMessage(String msg){
  Serial.print("Inicializando Transmissão, id ");
  Serial.println(msgCount);
  Serial.print("Mensagem: ");
  Serial.println(msg);

  LoRa.beginPacket();               // start packet
  // LoRa.write(destination);         // add destination address
  // LoRa.write(localAddress);        // add sender address
  LoRa.write(msgCount);            // add message ID
  LoRa.write(msg.length());        // add payload length
  LoRa.print(msg);                 // add payload
  LoRa.endPacket();                // send packet
}

String coleta(){
  String msg;
  msg += String(coleta_temp()) + "ºC;";
  msg += coleta_PH();
  msg += "0.0ppm;";
  msg += "0.0ntu";
  return msg;
}

String coleta_PH(){
  float valor_pH = -5.70 * ads.computeVolts(ads.readADC_SingleEnded(0)) + valor_calibracao; //Calcula valor de pH
  return "pH "+ String(valor_pH) + ";";
}

String coleta_TDS(){
  temp = coleta_temp();
  gravityTds.setTemperature(temp);
  gravityTds.update();
  float tdsValue = gravityTds.getTdsValue();
  Serial.print(tdsValue,0);
  return String(tdsValue) + "ppm;";
}

float coleta_temp() {
    int leitura = thermistor.read();
    float temp = (float)leitura / 100.0; // Ajuste da leitura
    return temp;
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  String incoming = "";

  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }
  Serial.println(incoming);
  if (incomingLength != incoming.length() && 1==0) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             // skip rest of function
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }

  // if message is for this device, or broadcast, print details:
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
  ACK = true;
  return;
}

