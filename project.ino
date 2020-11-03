#include <WiFi.h>
#include <PubSubClient.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <TridentTD_LineNotify.h>
#include "SPI.h" //การสื่อสารอนุกรม sca sdl 
#include <Wire.h>
#include <Adafruit_ADS1015.h>
Adafruit_ADS1115 ads(0x48);
#include "time.h"

#define RELAY1 15 //ปั๊ม
#define RELAY2 32 //ไฟ
#define RELAY3 13 //tds
#define RELAY4 12 //ph

//TdsSensorPin 
#define VREF 5.0      // analog reference voltage(Volt) of the ADC
#define SCOUNT  30           // sum of sample point
int analogBuffer[SCOUNT];    // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0, tdsValue = 0, ecValue = 0, phValue = 0, temperature = 25;
float Voltage = 0.0;
const char* ssid = "lnwza";
const char* password = "aaaaaaaa";
const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
const char* mqtt_Client = "44c05117-fe60-42d1-89ed-3c5465185582";
const char* mqtt_username = "hRzgZhQ6oaZDnDp2G2xio5Jmv12ABtsh";
const char* mqtt_password = "o5esrt(5#68JgSKjyo93ow2k5(N#j(i)";

const char* mqtt_Client_pump = "4d243bdd-dc28-4eab-b0e9-f89eebcac73c";
const char* mqtt_username_pump = "d6kBgybFzSwoeEw6KmZdSxiEZFiB5jFX";
const char* mqtt_password_pump = "jLcg~VS3wt(VXI#y*BiOi#z2hb~)D0QA";

#define LINE_TOKEN  "Vu8RDDnBcxq4vvdvUy3jg4DdIXOEWyU6llxdhGhSVAh"


IPAddress server_addr(64,227,10,179);             // IP ของ MySQL server
char dbuser[] = "lnwlnw";                         // MySQL username
char dbpassword[] = "555555";                      // MySQL password

char query[] = "SELECT * FROM project.pumps ";      //กำหนดชื่อฐานข้อมูลและชื่อตาราง  DBName.TableName
char query2[] = "SELECT * FROM project.lights ";
char INSERT_PUMP_ON[] = "INSERT INTO project.histories (event, status) VALUES ('PUMP','ON')";
char INSERT_PUMP_OFF[] = "INSERT INTO project.histories (event, status) VALUES ('PUMP','OFF')";
char INSERT_LIGHT_ON[] = "INSERT INTO project.histories (event, status) VALUES ('grow light','ON')";
char INSERT_LIGHT_OFF[] = "INSERT INTO project.histories (event, status) VALUES ('grow light','OFF')";

WiFiClient client;
WiFiClient espclient;
WiFiClient espclient2;
PubSubClient mqtt(espclient);//led
PubSubClient mqtt2(espclient2);//pump

MySQL_Connection conn(& client);
MySQL_Cursor * cursor;
MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);

char msg[50];

String strPUMP;
String strLED;
String checkPUMP;
String checkLED;
int sensorValue;

const char* ntpServer = "asia.pool.ntp.org"; //เรียกเน็ตมาจากเซิฟ
const long  gmtOffset_sec = 21600; //ตั้งทามโซน
const int   daylightOffset_sec = 3600; // ตั้งหน่วยเวลา

int nowhour; //หน่วยเวลาปัจจุบัน
int nowmin;


void setup() {
  
    xTaskCreate(func1_Task,"func1_Task",10000,NULL,1,NULL);//สร้างทาส(ฟังชั่น,ชื่อฟังชัน,)
    xTaskCreate(func2_Task,"func2_Task",10000,NULL,8,NULL);
    ads.setGain(GAIN_ONE);
    ads.begin();
    Wire.begin();

    Serial.begin(115200);
    pinMode(RELAY1,OUTPUT);
    digitalWrite(RELAY1, 0);
    pinMode(RELAY2,OUTPUT);
    digitalWrite(RELAY2, 0);
    pinMode(RELAY3,OUTPUT);
    digitalWrite(RELAY3, 0);
    pinMode(RELAY4,OUTPUT);
    digitalWrite(RELAY4, 0);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);//setทามโซน,หน่วยเวลา,เซิฟที่ดึงเวลา
    printLocalTime();//เรียกฟังชันเพื่อนคอนเนก
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(ledcallback);
    mqtt2.setServer(mqtt_server, mqtt_port);
    mqtt2.setCallback(pumpcallback);

    LINE.setToken(LINE_TOKEN);
    
 //MySQL Connection
  Serial.println("Connecting...");
  if (conn.connect(server_addr, 3306, dbuser, dbpassword)) {
    Serial.println("MySQL Connected.");
  }
  else
    Serial.println("Connection failed.");
    //conn.close();
  // create the MySQL object cursor
    cursor = new MySQL_Cursor (& conn);
    //MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);

      //init and get the tim
}

void loop() {
        sensorValue = analogRead(33);
        printLocalTime();
        dbPump();
        dbLight();
        LDR();
      if (!mqtt.connected())
        {
          reconnectmqtt();
        }
      mqtt.loop();
      if (!mqtt2.connected())
        {
          reconnectmqtt2();
        }
      mqtt2.loop();
}

void func1_Task(void * parameter){
  for(;;){
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  digitalWrite(RELAY3, 1);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  tdsSensor();
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  digitalWrite(RELAY3, 0);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  digitalWrite(RELAY4, 1);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  phSensor();
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  digitalWrite(RELAY4, 0);
  vTaskDelay(5000 / portTICK_PERIOD_MS); 
  String data = " EC:" + String(ecValue) + "     pH:" + String(phValue) + " ";
  Serial.println(data);
  data.toCharArray(msg, (data.length() + 1));
  mqtt.publish("@msg/ecph", msg);
  }
}

void func2_Task(void * parameter){
  for(;;){

    vTaskDelay(600000 / portTICK_PERIOD_MS);
      if(ecValue<1.1){
    String str1 = "EC:"+ (String)ecValue +" ค่า EC ต่ำกว่ากำหนด";
    LINE.notify(str1);

    }if(ecValue>1.7){
    String str2 = "EC:"+ (String)ecValue +" ค่า EC สูงกว่ากำหนด";
    LINE.notify(str2);
    
    }if(phValue<6){
    String str3 = "pH:"+ (String)phValue +" ค่า pH ต่ำกว่ากำหนด";
    LINE.notify(str3);

    }if(phValue>7){
    String str4 = "pH:"+ (String)phValue +" ค่า pH สูงกว่ากำหนด";
    LINE.notify(str4);
    }
    vTaskDelay(600000 / portTICK_PERIOD_MS);
  }
}



void reconnectmqtt() {
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection…");
    if (mqtt.connect(mqtt_Client, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      mqtt.subscribe("@msg/led");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
      return;
    }
  }
}
void reconnectmqtt2() {
        while (!mqtt2.connected()) {
        Serial.print("Attempting MQTT connection…");
        if (mqtt2.connect(mqtt_Client_pump, mqtt_username_pump, mqtt_password_pump)) {
            Serial.println("connected");
            mqtt2.subscribe("@msg/pump");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt2.state());
            Serial.println("try again in 5 seconds");
            delay(5000);
            return;
        }
    }
}

void ledcallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrive [");
    Serial.print(topic);
    Serial.print("]");
    String message;
    for (int i = 0; i < length; i++) {
        message = message + (char)payload[i];
    }
    Serial.println(message);
    checkLED = message;
    if (checkLED == "GET") {
        mqtt.publish("@msg/led",(char*)strLED.c_str());
        Serial.println("Send !");
        Serial.println((char*)strLED.c_str());
        return;
    }
    if (checkLED == "LEDON") {
        digitalWrite(RELAY2, 1);
        strLED = "LEDON";
        cur_mem->execute(INSERT_LIGHT_ON);
        Serial.println(message);
    }
    else {
        digitalWrite(RELAY2, 0);
        strLED = "LEDOFF";
        cur_mem->execute(INSERT_LIGHT_OFF);
        Serial.println(message);
    }
//    if (message == "TIMER") {
//        Serial.println(message);
//        strLED = "TIMER";
//    }
//    if (message == "LDR") {        
//        strLED = "LDR";
//        if(sensorValue < 50)
//        {
//          digitalWrite(RELAY2,HIGH);
//          Serial.print(sensorValue, DEC);
//          Serial.println("LEDON");
//        }
//        else
//        {
//          digitalWrite(RELAY2,LOW);
//          Serial.print(sensorValue , DEC);
//          Serial.println("LEDOFF");
//          
//        }   
//        }
}

void pumpcallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrive [");
    Serial.print(topic);
//    Serial.print(payload);
    Serial.print("]");
    String message;
    for (int i = 0; i < length; i++) {
        message = message + (char)payload[i];
    }
    Serial.println(message);
      checkPUMP = message;
    if (checkPUMP == "GET") {
        mqtt2.publish("@msg/pump",(char*)strPUMP.c_str() );
        Serial.println("Send !");
        return;
    }
    if (checkPUMP == "PUMPON") {
        digitalWrite(RELAY1, 1);
        strPUMP = "PUMPON";
        cur_mem->execute(INSERT_PUMP_ON);
        Serial.println(message);
    }
    else {
        digitalWrite(RELAY1, 0);
        strPUMP = "PUMPOFF";
        cur_mem->execute(INSERT_PUMP_OFF);
        Serial.println(message);
    }
//    if (message == "TIMER") {
//        strPUMP = "TIMER";
//        dbPump();
//        Serial.println(message);
        
    
}



void dbPump(){
      if (checkPUMP == "TIMER") {
        strPUMP = "TIMER";  
    //Serial.println("\nRunning SELECT and printing results\n");
  //   Initiate the query class instance
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  //   Execute the query
    cur_mem->execute(query);
//     Fetch the columns and print them
    column_names *cols = cur_mem->get_columns();
    for (int f = 0; f < cols->num_fields; f++) {
      Serial.print(cols->fields[f]->name);
      if (f < cols->num_fields-1) {
        Serial.print(", ");
      }
    }
  Serial.println();
//     Read the rows and print them
    row_values *row = NULL;
    do {
      row = cur_mem->get_next_row();
      if (row != NULL) {    
        String u3[10];
        for (int f = 0; f < cols->num_fields; f++) {
          Serial.print(row->values[f]);
          if (f < cols->num_fields-1) {
            Serial.print(", ");
          }
          u3[f] = row->values[f];
              if (nowhour == u3[1].toInt() && nowmin == u3[2].toInt()) {
                digitalWrite(RELAY1, HIGH);
                strPUMP = "PUMPON";
                Serial.println("LIGHT ON");
            }
          
              else if(nowhour == u3[3].toInt() && nowmin == u3[4].toInt()) {
              digitalWrite(RELAY1, LOW);
              strPUMP = "PUMPOFF";
              Serial.println("LIGHT Off");
            }
              
            
        
        }
        Serial.println();
//        Serial.println(u3[2]); 
  //      char* uu = row->values[1];
               
      }
    } 
    while (row != NULL);
    // Deleting the cursor also frees up memory used
    delete cur_mem;
}
}


void dbLight(){
        if (checkLED == "TIMER") {
        strLED = "TIMER";
    Serial.println("\nRunning SELECT and printing results\n");
  
  //   Initiate the query class instance
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  //   Execute the query
    cur_mem->execute(query2);
//     Fetch the columns and print them
    column_names *cols = cur_mem->get_columns();
    for (int f = 0; f < cols->num_fields; f++) {
      Serial.print(cols->fields[f]->name);
      if (f < cols->num_fields-1) {
        Serial.print(", ");
      }
    }
  Serial.println();
//     Read the rows and print them
    row_values *row = NULL;
    do {
      row = cur_mem->get_next_row();
      if (row != NULL) {
        
        
        String u3[10];
        
        for (int f = 0; f < cols->num_fields; f++) {
          
          Serial.print(row->values[f]);
          
  //        int onHour = u3[1].toInt();
  //        int onMin = u3[2].toInt();
  //        int offHour = u3[3].toInt();
  //        int offMin = u3[4].toInt();
          if (f < cols->num_fields-1) {
            Serial.print(", ");
          }
          u3[f] = row->values[f];
          if (nowhour == u3[1].toInt() && nowmin == u3[2].toInt()) {
              digitalWrite(RELAY2, HIGH);
              strLED = "LEDON";
              Serial.println("LIGHT ON");
            }
          
            else if(nowhour == u3[3].toInt() && nowmin == u3[4].toInt()) {
              digitalWrite(RELAY2, LOW);
              strLED = "LEDOFF";
              Serial.println("LIGHT Off");
            }
            
        }
        Serial.println();
//        Serial.println(u3[2]); 
  //      char* uu = row->values[1];
               
      }
    } 
    while (row != NULL);
    // Deleting the cursor also frees up memory used
    delete cur_mem;
}
}

void printLocalTime(){
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    ESP.restart();
    return;
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  nowhour = timeinfo.tm_hour;
  nowmin =  timeinfo.tm_min;
  Serial.print(nowhour);
  Serial.print(":");
  Serial.println(nowmin);
}

void LDR(){
     if (checkLED == "LDR") {        
        strLED = "LDR";
        if(sensorValue < 50)
        {
          digitalWrite(RELAY2,HIGH);
          Serial.print(sensorValue, DEC);
          strLED = "LEDON";
          cur_mem->execute(INSERT_LIGHT_ON);
          Serial.println("LEDON");
        }
        else
        {
          digitalWrite(RELAY2,LOW);
          Serial.print(sensorValue , DEC);
          strLED = "LEDOFF";
          cur_mem->execute(INSERT_LIGHT_OFF);
          Serial.println("LEDOFF");
          
        }
         delay(500);  
}
}

void phSensor() {
  int16_t adc1;
  adc1 = ads.readADC_SingleEnded(1);
  Voltage = ((adc1 * 0.1875) / 1240)+0.65;
  phValue = (7 + (Voltage - 2.5) * (-5.5));
  Serial.print("pH Value==");
  Serial.print("AIN1: ");
  Serial.print(adc1);
  Serial.print("\tVoltage: ");
  Serial.print(Voltage, 7);
  Serial.print(" ");
  Serial.print("phValue: ");
  Serial.println(phValue);

}

void tdsSensor()
{
  int16_t adc0;
  adc0 = ads.readADC_SingleEnded(0);
  int Voltage = (adc0 * 0.1875) / 100;
  averageVoltage = ((adc0 * 0.1875) / 1240)+0.2;                                                      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);                                  //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
  float compensationVolatge = averageVoltage / compensationCoefficient;                               //temperature compensation
  tdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 
  * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5;                  //convert voltage value to tds value
  ecValue = tdsValue / 650;
  Serial.print("EC Value==");
  Serial.print(averageVoltage, 2);
  Serial.print("V   ");
  Serial.print("EC Value: ");
  Serial.print(ecValue, 2);
  Serial.println(" mS/cm.");
}
