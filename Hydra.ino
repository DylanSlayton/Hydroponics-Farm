#include <ESP8266WiFi.h>

// WiFi credentials
const char* ssid     = "SlayFi";
const char* password = "Kaptn101!";

// Web server
WiFiServer server(80);
String header;

// GPIO
const int pumpPin = 5;

// Pump state
bool pumpOn = true;
bool manualOverride = false;
bool hydraHappy = false;


// Timer configuration (SECONDS)
unsigned long onDurationSec  = 5*60;
unsigned long offDurationSec = 60*60;

// Timing
unsigned long lastToggleMillis = 0;

// HTTP timeout
const long timeoutTime = 2000;

// ✅ Single source of truth for GPIO
void applyPumpState() {
  digitalWrite(pumpPin, pumpOn ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(pumpPin, OUTPUT);
  applyPumpState();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  server.begin();
  lastToggleMillis = millis();
}

void handleTimer() {
  if (manualOverride) return;

  unsigned long now = millis();
  unsigned long interval = (pumpOn ? onDurationSec : offDurationSec) * 1000UL;

  if (now - lastToggleMillis >= interval) {
    pumpOn = !pumpOn;
    applyPumpState();
    lastToggleMillis = now;
  }
}

unsigned long secondsUntilNextToggle() {
  unsigned long now = millis();
  unsigned long interval = (pumpOn ? onDurationSec : offDurationSec) * 1000UL;

  if (now - lastToggleMillis >= interval) return 0;
  return (interval - (now - lastToggleMillis)) / 1000UL;
}

void loop() {
  handleTimer();

  WiFiClient client = server.available();
  if (!client) return;

  unsigned long startTime = millis();
  header = "";
  hydraHappy = false; // reset message each page load

  while (client.connected() && millis() - startTime <= timeoutTime) {
    if (!client.available()) continue;

    char c = client.read();
    header += c;

    if (c == '\n' && header.endsWith("\r\n\r\n")) {

      // Manual override ON
      if (header.indexOf("GET /override/on") >= 0) {
        manualOverride = true;
        pumpOn = true;
        applyPumpState();
      }

      // Manual override OFF (return to timer cleanly)
      if (header.indexOf("GET /override/off") >= 0) {
        manualOverride = false;

      // Restart timer from OFF state so timing is predictable
        pumpOn = false;
        applyPumpState();
        lastToggleMillis = millis();
      }

      // Headpat button
      if (header.indexOf("GET /headpat") >= 0) {
        hydraHappy = true;
      }

      // Timer values from form
      int onIdx = header.indexOf("on=");
      int offIdx = header.indexOf("off=");

      if (onIdx > 0 && offIdx > 0) {
        onDurationSec  = header.substring(onIdx + 3).toInt();
        offDurationSec = header.substring(offIdx + 4).toInt();
        lastToggleMillis = millis();
      }

      // HTTP response
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/html");
      client.println("Connection: close");
      client.println();

      client.println("<!DOCTYPE html><html><head>");
      client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
      client.println("<style>");
      client.println("body{font-family:Helvetica;text-align:center;}");
      client.println(".button{padding:16px 40px;font-size:24px;margin:5px;}");
      client.println(".on{background:#195B6A;color:white;}");
      client.println(".off{background:#77878A;color:white;}");
      client.println("</style></head><body>");

      client.println("<h1>Hydra</h1>");

      client.print("<p>Status: <b>");
      client.print(pumpOn ? "ON" : "OFF");
      client.println("</b></p>");

      client.print("<p>Mode: ");
      client.print(manualOverride ? "MANUAL OVERRIDE" : "TIMER");
      client.println("</p>");

      if (manualOverride) {
        client.println("<a href='/override/off'><button class='button off'>Disable Override</button></a>");
      } else {
        client.println("<a href='/override/on'><button class='button on'>Force ON</button></a>");
      }

      client.println("<hr>");
      client.println("<h3>Timer Settings (seconds)</h3>");
      client.println("<form action='/' method='GET'>");
      client.print("ON time: <input type='number' name='on' value='");
      client.print(onDurationSec);
      client.println("'><br><br>");
      client.print("OFF time: <input type='number' name='off' value='");
      client.print(offDurationSec);
      client.println("'><br><br>");
      client.println("<input type='submit' value='Update Timer'>");
      client.println("</form>");

      client.println("<hr>");
      client.print("<p>Next toggle in <b>");
      client.print(secondsUntilNextToggle());
      client.print(" second(s)</b><br>Will turn <b>");
      client.print(pumpOn ? "OFF" : "ON");
      client.println("</b></p>");

      client.println("<hr>");
      client.println("<a href='/headpat'><button class='button fun'>Headpat the Hydra</button></a>");

      if (hydraHappy) {
        client.println("<p><b>Hydra is Happy :)</b></p>");
      }

      client.println("</body></html>");
      client.println();
      break;
    }
  }

  client.stop();
}
