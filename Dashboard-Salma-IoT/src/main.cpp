#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>

// --- 1. KONFIGURASI WIFI (SESUAIKAN HOTSPOT HP KAMU) ---
const char* ssid = "april";
const char* pass = "iyaituaja";

// --- 2. LINK SCRIPT GOOGLE (YANG SUDAH BERHASIL TADI) ---
const char* script_url = "https://script.google.com/macros/s/AKfycbyUCKYNDIIC3OIFN7O8rRhGWNt3Nn8YcYlupHNzbz0J921zQllxOxjC3_KrBQVLfR40UQ/exec";

// --- 3. LINK UNTUK BUKA TABEL (DOCS GOOGLE) ---
const char* docs_url = "https://docs.google.com/spreadsheets/d/1vHjQFeK7bhKAYXuU0TNy03ICNKp3ZnirjR116SdKUmM/edit";

// --- PIN HARDWARE ---
#define DHTPIN 4
#define ONE_WIRE_BUS 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

DHT dht(DHTPIN, DHT11);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

float t_dht = 0, h_dht = 0, t_ds = 0;
unsigned long lastTime = 0;
unsigned long timerDelay = 15000; // Kirim data tiap 15 detik biar Google nggak pusing

void refreshOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("  IOT MONITOR APRIL");
  display.println("--------------------");
  display.setCursor(0, 20);
  display.printf("DHT11 : %.1f C\n", t_dht);
  display.printf("Humid : %.1f %%\n", h_dht);
  display.printf("DS18B : %.1f C\n", t_ds);
  display.setCursor(0, 54);
  display.print("IP: ");
  display.print(WiFi.localIP());
  display.display();
}

// --- TAMPILAN WEB (DASHBOARD) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>APRIL IOT DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { background: #f0f2f5; font-family: 'Segoe UI', sans-serif; text-align: center; padding:20px; margin:0; }
    .container { max-width: 400px; margin: auto; background: white; padding: 25px; border-radius: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.1); }
    h2 { color: #1877f2; margin-bottom: 20px; }
    .card { background: #f8f9fa; border-radius: 15px; padding: 15px; margin-bottom: 15px; border-left: 6px solid #1877f2; text-align: left; }
    .label { font-size: 0.8rem; color: #65676b; font-weight: bold; text-transform: uppercase; }
    .val { font-size: 1.8rem; color: #1c1e21; font-weight: bold; margin-top: 5px; }
    .btn { background: #15803d; color: white; border: none; padding: 16px; border-radius: 12px; width: 100%; font-size: 1rem; font-weight: bold; cursor: pointer; text-decoration: none; display: block; margin-top: 15px; transition: 0.3s; }
    .btn:hover { background: #1a9e4b; transform: scale(1.02); }
  </style>
</head>
<body>
  <div class="container">
    <h2>IOT SYSTEM APRIL</h2>
    <div class="card"><div class="label">Suhu DHT11</div><div class="val"><span id="t1">0</span>°C</div></div>
    <div class="card" style="border-left-color: #00a400;"><div class="label">Kelembapan</div><div class="val"><span id="h1">0</span>%</div></div>
    <div class="card" style="border-left-color: #f02849;"><div class="label">Suhu DS18B20</div><div class="val"><span id="t2">0</span>°C</div></div>
    
    <a href="https://docs.google.com/spreadsheets/d/1vHjQFeK7bhKAYXuU0TNy03ICNKp3ZnirjR116SdKUmM/edit" target="_blank" class="btn">LIHAT DATA DI SPREADSHEET</a>
  </div>

  <script>
    setInterval(function() {
      fetch('/data').then(r => r.json()).then(d => {
        document.getElementById("t1").innerText = d.t1;
        document.getElementById("h1").innerText = d.h1;
        document.getElementById("t2").innerText = d.t2;
      });
    }, 2000);
  </script>
</body>
</html>)rawliteral";

void setup(){
  Serial.begin(115200);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED Gagal");
  display.clearDisplay();
  display.display();

  dht.begin();
  sensors.begin();
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
  }
  
  Serial.println("\nWiFi Connected!");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    sensors.requestTemperatures();
    t_dht = dht.readTemperature();
    h_dht = dht.readHumidity();
    t_ds = sensors.getTempCByIndex(0);
    
    // Filter jika DS18B20 tidak terpasang
    if(t_ds < -50) t_ds = 0.0;
    
    refreshOLED();
    
    String j = "{\"t1\":\""+String(t_dht,1)+"\",\"h1\":\""+String(h_dht,1)+"\",\"t2\":\""+String(t_ds,1)+"\"}";
    request->send(200, "application/json", j);
  });

  server.begin();
  refreshOLED();
}

void loop(){
  // Kirim data otomatis ke Google Sheets setiap 15 detik
  if ((millis() - lastTime) > timerDelay) {
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      // Membentuk URL: script_url + parameter data
      String full_url = String(script_url) + "?dht=" + String(t_dht,1) + "&ds=" + String(t_ds,1) + "&hum=" + String(h_dht,1);
      
      http.begin(full_url.c_str());
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      
      int httpResponseCode = http.GET();
      
      Serial.print("Kirim ke Sheets... Response: ");
      Serial.println(httpResponseCode);
      
      http.end();
    }
    lastTime = millis();
  }
}