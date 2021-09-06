#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "OLED.h"
#define Speaker 12

OLED display(4, 5);

const char *ssid = "TP-LINK_B26836";
const char *password = "86594325";

double vt = 0;                  //Tidal volume (VT)
double peep = 0;                //Positive end-expiratory pressure (PEEP)
double respiratory_rate = 0;    //Respiratory rate.
double inspiratory_airflow = 0; //Inspiratory airflow.

unsigned long timer = 0;
unsigned long prevTimer = 0;
double sensorValue = 0;
int sensorCutOffMin = 200;
int sensorCutOffMax = 700;
#define P_HIGH 1
#define P_LOW -1
#define P_OK 0
int sensorState = 0;

boolean buzState = false;
unsigned long buzTimer = 0;
unsigned long buzInterval = 250;
int frequency = 73;

boolean displayAlertState = false;
unsigned long disTimer = 0;
unsigned long disInterval = 500;
String alertText = "";
char alertTextChar[16];

ESP8266WebServer server(80);

//GET request for sensor data
void getSensorData()
{
  DynamicJsonDocument doc(256);
  doc["pressure"] = String((double)sensorValue, 2);

  if (sensorState == P_HIGH)
    doc["pressure_state"] = "high";
  else if (sensorState == P_LOW)
    doc["pressure_state"] = "low";
  else
    doc["pressure_state"] = "normal";

  Serial.print(F("Stream..."));
  String buf;
  serializeJson(doc, buf);
  server.send(200, "application/json", buf);
  Serial.print(F("done."));
}

// GET request for data
void getData()
{
  DynamicJsonDocument doc(512);
  doc["vt"] = vt;
  doc["peep"] = peep;
  doc["respiratory_rate"] = respiratory_rate;
  doc["inspiratory_airflow"] = inspiratory_airflow;

  Serial.print(F("Stream..."));
  String buf;
  serializeJson(doc, buf);
  server.send(200, "application/json", buf);
  Serial.print(F("done."));
}

// POST request for data
void setData()
{
  String postBody = server.arg("plain");
  Serial.println(postBody);

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, postBody);
  if (error)
  {
    // if the file didn't open, print an error:
    Serial.print(F("Error parsing JSON "));
    Serial.println(error.c_str());

    String msg = error.c_str();

    server.send(400, F("text/html"),
                "Error in parsin json body! <br>" + msg);
  }
  else
  {
    JsonObject postObj = doc.as<JsonObject>();

    Serial.print(F("HTTP Method: "));
    Serial.println(server.method());

    if (server.method() == HTTP_POST)
    {
      if (postObj.containsKey("vt") && postObj.containsKey("peep") && postObj.containsKey("respiratory_rate") && postObj.containsKey("inspiratory_airflow"))
      {

        vt = postObj["vt"];
        peep = postObj["peep"];
        ; //Positive end-expiratory pressure (PEEP)
        respiratory_rate = postObj["respiratory_rate"];
        ; //Respiratory rate.
        inspiratory_airflow = postObj["inspiratory_airflow"];
        ; //Inspiratory airflow.

        DynamicJsonDocument doc(512);
        doc["status"] = "OK";

        Serial.print(F("Stream..."));
        String buf;
        serializeJson(doc, buf);

        server.send(201, F("application/json"), buf);
        Serial.print(F("done."));
        displayData();
      }
      else
      {
        DynamicJsonDocument doc(512);
        doc["status"] = "KO";
        doc["message"] = F("No data found, or incorrect!");

        Serial.print(F("Stream..."));
        String buf;
        serializeJson(doc, buf);

        server.send(400, F("application/json"), buf);
        Serial.print(F("done."));
      }
    }
  }
}

// Define routing
void restServerRouting()
{
  server.on("/", HTTP_GET, []()
            { server.send(200, F("text/html"),
                          F("Welcome to the REST Web Server")); });
  server.on("/sensor", HTTP_GET, getSensorData);
  server.on("/data", HTTP_GET, getData);
  server.on("/data", HTTP_POST, setData);
}

// Manage not found URL
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void reconnectWIFI()
{

  if (WiFi.status() != WL_CONNECTED)
  {
    display_Connecting(true);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    display_Connecting(false);
    displayData();
  }
}

//-----------OLED DISPLAY--------------------------------

void display_Connecting(boolean isConnecting)
{
  if (isConnecting)
  {
    display.clear();
    display.print("CONNECTING", 3, 3);
  }

  else
  {
    display.clear();
  }
}

void displayData()
{
  display.clear();
  char dataChar[4][16];
  String vtS = "VT     :";
  String peepS = "PEEP   :";
  String r_rateS = "RR     :";
  String i_airflowS = "FiO2   :";

  vtS += String(vt, 2);
  peepS += String(peep, 2);
  r_rateS += String(respiratory_rate, 2);
  i_airflowS += String(inspiratory_airflow, 2);

  vtS.toCharArray(dataChar[0], 16);
  peepS.toCharArray(dataChar[1], 16);
  r_rateS.toCharArray(dataChar[2], 16);
  i_airflowS.toCharArray(dataChar[3], 16);

  display.print(dataChar[0], 2);
  display.print(dataChar[1], 3);
  display.print(dataChar[2], 4);
  display.print(dataChar[3], 5);
  //      display.print(peepS, 2);
  //      display.print(r_rate, 2);
  //      display.print(i_airflow, 2);
}

//-------------------------------------------------------

void setup(void)
{
  Serial.begin(115200);
  display.begin();
  //  WiFi.mode(WIFI_STA);
  //  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  //  reconnectWIFI();
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop(void)
{
  //timer = millis();
  //
  //  reconnectWIFI();
  //
  //  server.handleClient();
  //
  //
  //if(timer - prevTimer > 500 )  {
  sensorValue = getPressure();
  //  if(sensorValue < sensorCutOffMin){
  //    sensorState = P_LOW;
  //    }
  //  else if (sensorValue > sensorCutOffMax){
  //    sensorState = P_HIGH;
  //    }
  //  else {
  //    sensorState = P_OK;
  //
  //    }
  //
  //  prevTimer = timer ;
  alertText = " an val :";
  alertText += String((double)sensorValue, 2);
  alertText.toCharArray(alertTextChar, 16);
  display.print(alertTextChar, 7);
  delay(300);
}

//makeToneAlert();
//makeDisplayAlert();

void makeToneAlert()
{
  if (timer - buzTimer < buzInterval)
    return;
  if (sensorState == P_OK)
  {
    buzState = false;
  }
  else
  {
    buzState = !buzState;
  }

  if (buzState)
  {
    tone(Speaker, frequency);
  }
  else
  {
    noTone(Speaker);
  }
  buzTimer = timer;
}

void makeDisplayAlert()
{
  if (timer - disTimer < disInterval)
    return;

  if (sensorState == P_OK)
  {
    displayAlertState = false;
  }
  else
  {
    if (sensorState == P_HIGH)
      alertText = "  P. HIGH :";
    else if (sensorState == P_LOW)
      alertText = "  P. LOW  :";
    alertText += String((double)sensorValue, 2);
    alertText.toCharArray(alertTextChar, 16);
    displayAlertState = !displayAlertState;
  }

  if (displayAlertState)
  {
    display.print(alertTextChar, 7);
  }
  else
  {
    display.print("               ", 7);
  }
  disTimer = timer;
}

double getPressure()
{

  double voltage = analogRead(A0) * 0.00148108652507479;

  double pressure = voltage * 51.06209150326797;

  return pressure * 0.145037738; // convert KPa to PSI
}
