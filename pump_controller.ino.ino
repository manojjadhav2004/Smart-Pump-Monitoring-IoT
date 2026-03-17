#define BLYNK_TEMPLATE_ID "TMPL3SNKGpRV2"
#define BLYNK_TEMPLATE_NAME "Manoj Jadhav"
#define BLYNK_AUTH_TOKEN "vhMg_6y3Q2-xaw7GbfrSPCjMW_nwnMPD"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <WebServer.h>

char ssid[] = "MANOJ";
char pass[] = "MANOJ123";

#define RELAY_PIN 26
#define FLOW_PIN 27
#define DHT_PIN 4
#define DHTTYPE DHT11

#define TEMP_LIMIT 45

DHT dht(DHT_PIN, DHTTYPE);

WebServer server(80);

String motorStopReason = "System Ready";

WidgetTerminal terminal(V1);

volatile int flowPulse = 0;

bool motorRunning = false;
unsigned long motorStartTime = 0;

float flowRate = 0;
float temperature = 0;

BlynkTimer timer;

void IRAM_ATTR flowISR()
{
  flowPulse++;
}

/* ---------------- BLYNK CONTROL ---------------- */

BLYNK_WRITE(V0)
{
  int state = param.asInt();

  if(state == 1)
  {
    digitalWrite(RELAY_PIN, LOW);
    motorRunning = true;
    motorStartTime = millis();

    motorStopReason = "Running Normally";

    terminal.println("Motor Started");
    terminal.flush();

    Blynk.virtualWrite(V2,1);
  }
  else
  {
    digitalWrite(RELAY_PIN, HIGH);
    motorRunning = false;

    motorStopReason = "Manual Stop";

    terminal.println("Motor Stopped");
    terminal.flush();

    Blynk.virtualWrite(V2,0);
  }
}

/* ---------------- WEB DASHBOARD APIs ---------------- */

void motorOn()
{
  digitalWrite(RELAY_PIN, LOW);
  motorRunning = true;
  motorStartTime = millis();

  motorStopReason = "Running Normally";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Motor ON");
}

void motorOff()
{
  digitalWrite(RELAY_PIN, HIGH);
  motorRunning = false;

  motorStopReason = "Manual Stop";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Motor OFF");
}

void sendData()
{
  String json = "{";
  json += "\"motor\":" + String(motorRunning);
  json += ",\"flow\":" + String(flowRate);
  json += ",\"temp\":" + String(temperature);
  json += ",\"fault\":\"" + motorStopReason + "\"";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

/* ---------------- SENSOR CHECK ---------------- */

void checkSensors()
{
  int pulses = flowPulse;
  flowPulse = 0;

  temperature = dht.readTemperature();

  flowRate = pulses / 7.5;

  Blynk.virtualWrite(V3, flowRate);
  Blynk.virtualWrite(V4, temperature);
  Blynk.virtualWrite(V6, motorStopReason);

  if(motorRunning)
  {

    if(pulses == 0 && millis() - motorStartTime > 10000)
    {
      digitalWrite(RELAY_PIN, HIGH);
      motorRunning = false;

      motorStopReason = "Electricity Supply Fault";

      terminal.println("Electricity Fault Detected");
      terminal.flush();

      Blynk.virtualWrite(V2,0);
      return;
    }

    if(flowRate < 30 && millis() - motorStartTime > 10000)
    {
      digitalWrite(RELAY_PIN, HIGH);
      motorRunning = false;

      motorStopReason = "Dry Run Detected";

      terminal.println("Dry Run Detected");
      terminal.flush();

      Blynk.virtualWrite(V2,0);
      return;
    }
  }

  if(temperature > TEMP_LIMIT && motorRunning)
  {
    digitalWrite(RELAY_PIN, HIGH);
    motorRunning = false;

    motorStopReason = "Starter Overheat";

    terminal.println("Starter Overheat Detected");
    terminal.flush();

    Blynk.virtualWrite(V2,0);
  }
}

/* ---------------- SETUP ---------------- */

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, HIGH);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, FALLING);

  dht.begin();

  WiFi.begin(ssid, pass);

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Blynk.syncVirtual(V0);

  timer.setInterval(2000L, checkSensors);

  /* WEB SERVER ROUTES */

  server.on("/motor/on", motorOn);
  server.on("/motor/off", motorOff);
  server.on("/data", sendData);

  server.begin();

  Serial.println("Web server started");
}

/* ---------------- LOOP ---------------- */

void loop()
{
  Blynk.run();
  timer.run();
  server.handleClient();
}