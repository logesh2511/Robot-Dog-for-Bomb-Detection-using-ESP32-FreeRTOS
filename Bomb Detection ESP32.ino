 #include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TinyGPS++.h>
#include <SPIFFS.h> // Internal Flash File System Library

// ================= CONFIGURATION =================
#define MQ2_PIN 34
#define METAL_DETECTOR_PIN 25
#define ALARM_PIN 26

#define RL_VALUE 5.0             
#define RO_CLEAN_AIR_FACTOR 9.83 

const char *ssid = "DOG";
const char *password = "12345678";

#define CO_ALERT_THRESHOLD 50        
#define METHANE_ALERT_THRESHOLD 1000 

const float CO_curve[2] = {584.28, -2.18};    
const float Methane_curve[2] = {4190.15, -2.45}; 

// ================= OBJECTS & SERIAL =================
TinyGPSPlus gps;
HardwareSerial SerialGPS(2); 

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float Ro = 10.0; 

// ================= DATA PACKET STRUCT =================
struct TacticalData {
    int coPPM;
    int methanePPM;
    bool metalDetected;
    bool coDangerous;
    bool methaneDangerous;
    double latitude;
    double longitude;
    int satellites;
    bool gpsValid;
};

// ================= FREERTOS HANDLES =================
QueueHandle_t telemetryQueue;

TaskHandle_t TaskSensorsHandle;
TaskHandle_t TaskGPSHandle;
TaskHandle_t TaskWebHandle;

// ================= ASYNC WEB UI INTERFACE (PROGMEM) =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Tactical Node Dashboard</title>
<style>
body{
    font-family: 'Courier New', monospace;
    background:#0d0d0d;
    color:#00ff00;
    margin:0;
    padding:15px;
    transition: background 0.5s ease;
}
.header{
    text-align:center;
    border-bottom:2px solid #00ff00;
    margin-bottom:20px;
    padding-bottom:10px;
}
.grid{
    display:grid;
    grid-template-columns:1fr;
    gap:15px;
}
@media(min-width:600px){ .grid{ grid-template-columns:1fr 1fr; } }
.card{
    border:1px solid #00ff00;
    background:#121212;
    padding:15px;
    border-radius:5px;
}
.val{
    font-size:1.6rem;
    margin-top:5px;
    font-weight:bold;
}
.danger{
    background:#330000;
    border-color:red;
    color:red;
    animation:blink 1s infinite;
}
.notification-banner {
    display: none;
    background: red;
    color: white;
    font-weight: bold;
    padding: 15px;
    text-align: center;
    font-size: 1.2rem;
    border-radius: 5px;
    margin-bottom: 15px;
    border: 2px solid white;
}
.btn{
    display:inline-block;
    margin-top:10px;
    background:#00ff00;
    color:#0d0d0d;
    padding:8px 12px;
    text-decoration:none;
    font-weight:bold;
    border-radius:3px;
    margin-right: 5px;
}
.btn-download{
    background: #007bff;
    color: white;
}
@keyframes blink{
    0%{opacity:1;}
    50%{opacity:0.4;}
    100%{opacity:1;}
}
</style>
</head>
<body>

<div class="header">
    <h2>DOG BOMB DETECTION </h2>
    <div id="status">INITIALIZING MESH CORE SYSTEMS...</div>
    <a href="/download" class="btn btn-download" style="margin-top:10px;"> DOWNLOAD EXCEL/CSV THREAT LOGS</a>
</div>

<div id="alertBanner" class="notification-banner">
    &#128680; WARNING: CRITICAL SYSTEM BREACH DETECTED &#128680;
</div>

<div class="grid">
    <div id="threatCard" class="card">
        <div>BOMB / METAL THREAT DETECTION</div>
        <div id="metal" class="val">SCANNING TARGET...</div>
    </div>

    <div id="coCard" class="card">
        <div>CARBON MONOXIDE (CO) STATUS</div>
        <div id="co" class="val">-- PPM</div>
    </div>

    <div id="methaneCard" class="card">
        <div>EXPLOSIVE/METHANE GAS STATUS</div>
        <div id="methane" class="val">-- PPM</div>
    </div>

    <div class="card">
        <div>GPS FIELD GEOLOCATION</div>
        <div id="gps" class="val" style="font-size:1.1rem; white-space: pre-line;">SEARCHING ENVELOPE SATELLITES...</div>
        <div id="mapsContainer"></div>
    </div>
</div>

<script>
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', initWebSocket);

function initWebSocket(){
    websocket = new WebSocket(gateway);
    websocket.onopen = function(event){
        document.getElementById('status').innerText = "LINK ESTABLISHED ";
    };
    websocket.onclose = function(event){
        document.getElementById('status').innerText = "LINK DROPPED - RECONNECTING...";
        setTimeout(initWebSocket, 2000);
    };
    websocket.onmessage = onMessage;
}

function onMessage(event){
    var data = JSON.parse(event.data);
    var overallThreat = false;

    // 1. CO Card
    document.getElementById('co').innerText = data.co + " PPM";
    var coCard = document.getElementById('coCard');
    if(data.coDangerous){ 
        coCard.classList.add('danger');
        overallThreat = true;
    } else { 
        coCard.classList.remove('danger'); 
    }

    // 2. Methane Card
    document.getElementById('methane').innerText = data.methane + " PPM";
    var methaneCard = document.getElementById('methaneCard');
    if(data.methaneDangerous){ 
        methaneCard.classList.add('danger');
        overallThreat = true;
    } else { 
        methaneCard.classList.remove('danger'); 
    }

    // 3. Threat Card
    var threatCard = document.getElementById('threatCard');
    if(data.metal){
        document.getElementById('metal').innerHTML = "&#9888; BOMB THREAT DETECTED";
        threatCard.classList.add('danger');
        overallThreat = true;
    } else {
        document.getElementById('metal').innerText = "CLEAR";
        threatCard.classList.remove('danger');
    }

    // 4. Banner Control
    var banner = document.getElementById('alertBanner');
    if(overallThreat) {
        banner.style.display = "block";
        document.body.style.background = "#210000"; 
    } else {
        banner.style.display = "none";
        document.body.style.background = "#0d0d0d";
    }

    // 5. GPS Integration
    var mapsContainer = document.getElementById('mapsContainer');
    if(!data.gpsValid){
        document.getElementById('gps').innerText = "NO GPS FIX DETECTED\nSATELLITES IN RANGE: " + data.sat;
        mapsContainer.innerHTML = "";
} else {
        document.getElementById('gps').innerText = "Lat: " + data.lat.toFixed(6) + "\nLon: " + data.lon.toFixed(6) + "\nLOCK ACTIVE (" + data.sat + " SATS)";
        mapsContainer.innerHTML = ""; // Clears the container layout so no map button is rendered
    }
}
</script>
</body>
</html>
)rawliteral";

// ================= MATH COEFFICIENT UTILITIES =================
float calculate_Rs(int raw_adc) {
    if (raw_adc == 0) return 999.0;
    float v_out = (raw_adc * 3.3) / 4095.0;
    return ((5.0 - v_out) * RL_VALUE) / v_out; 
}

float calibrate_Ro() {
    float val = 0;
    for (int i = 0; i < 50; i++) {
        val += calculate_Rs(analogRead(MQ2_PIN));
        delay(30);
    }
    return (val / 50.0) / RO_CLEAN_AIR_FACTOR;
}

int get_Gas_PPM(float rs_ro_ratio, const float *curve) {
    return (int)(curve[0] * pow(rs_ro_ratio, curve[1]));
}

// ================= LOCAL SPIFFS HARD DRIVE LOG ENGINE =================
void logThreatToCSV(TacticalData record) {
    // Open file in Append Mode ("a") to write to the bottom of the list
    File logFile = SPIFFS.open("/threat_logs.csv", FILE_APPEND);
    if(!logFile){
        Serial.println("Failed to mount internal flash file system partition row!");
        return;
    }

    // Comma Separated Format: Runtime_Clock, CO, Methane, Metal_Binary, Lat, Lon, Satellites
    logFile.print(millis()); logFile.print(",");
    logFile.print(record.coPPM); logFile.print(",");
    logFile.print(record.methanePPM); logFile.print(",");
    logFile.print(record.metalDetected ? "1" : "0"); logFile.print(",");
    logFile.print(record.latitude, 6); logFile.print(",");
    logFile.print(record.longitude, 6); logFile.print(",");
    logFile.println(record.satellites);

    logFile.close();
    Serial.println("[SYSTEM-DISK]: Event Row Committed to Local Storage Database.");
}

// ================= INTERRUPT SERVICE ROUTINE =================
void IRAM_ATTR onMetalDetected() {
    digitalWrite(ALARM_PIN, HIGH); 
}

// ================= TASKS PROTOTYPES =================
void vSensorTask(void *pvParameters);
void vGPSTask(void *pvParameters);
void vWebTask(void *pvParameters);

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    SerialGPS.begin(9600, SERIAL_8N1, 16, 17);

    pinMode(METAL_DETECTOR_PIN, INPUT_PULLUP);
    pinMode(ALARM_PIN, OUTPUT);
    digitalWrite(ALARM_PIN, LOW);

    attachInterrupt(digitalPinToInterrupt(METAL_DETECTOR_PIN), onMetalDetected, RISING);

    // ===== INITIALIZE FILE SYSTEM DRIVE =====
    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Storage Mounting Error!");
    } else {
        // If file doesn't exist yet, create it and write spreadsheet column headers
        if(!SPIFFS.exists("/threat_logs.csv")){
            File logFile = SPIFFS.open("/threat_logs.csv", FILE_WRITE);
            if(logFile){
                logFile.println("System_Time_ms,CO_PPM,Methane_PPM,Metal_Detected,Latitude,Longitude,Satellites");
                logFile.close();
            }
        }
    }

    Serial.println("\nCalibrating Ambient Reference Air Envelope... Please wait.");
    Ro = calibrate_Ro();
    Serial.print("Air Reference Calibration Complete. Base Ro: "); Serial.println(Ro);

    // ===== INITIALIZE HIGH-RANGE WI-FI AP =====
    WiFi.softAP(ssid, password);
    WiFi.setTxPower(WIFI_POWER_19_5dBm); 

    // ===== WEB SERVER ROUTING =====
    server.addHandler(&ws);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    // --- CSV LOG DOWNLOAD ENDPOINT ROUTE ---
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
        if(SPIFFS.exists("/threat_logs.csv")){
            // Intercept headers and force browser to process packet as an Excel/Text spreadsheet stream
            request->send(SPIFFS, "/threat_logs.csv", "text/csv");
        } else {
            request->send(404, "text/plain", "Log File Database Uninitialized.");
        }
    });

    server.begin();

    telemetryQueue = xQueueCreate(5, sizeof(TacticalData));

    if(telemetryQueue != NULL){
        xTaskCreatePinnedToCore(vSensorTask, "Sensors", 4096, NULL, 3, &TaskSensorsHandle, 1);
        xTaskCreatePinnedToCore(vGPSTask, "GPS", 2560, NULL, 2, &TaskGPSHandle, 1);
        xTaskCreatePinnedToCore(vWebTask, "WebTransmission", 4096, NULL, 4, &TaskWebHandle, 0);
    }
}

void loop() {
    vTaskDelete(NULL); 
}

// ================= SENSOR TASK (CORE 1) =================
void vSensorTask(void *pvParameters) {
    TacticalData localMetrics;

    for(;;){
        int rawGas = analogRead(MQ2_PIN);
        float Rs = calculate_Rs(rawGas);
        float ratio = Rs / Ro;

        localMetrics.coPPM = get_Gas_PPM(ratio, CO_curve);
        localMetrics.methanePPM = get_Gas_PPM(ratio, Methane_curve);
        localMetrics.metalDetected = (digitalRead(METAL_DETECTOR_PIN) == HIGH);
        
        localMetrics.coDangerous = (localMetrics.coPPM > CO_ALERT_THRESHOLD);
        localMetrics.methaneDangerous = (localMetrics.methanePPM > METHANE_ALERT_THRESHOLD);

        if(localMetrics.metalDetected || localMetrics.coDangerous || localMetrics.methaneDangerous){
            digitalWrite(ALARM_PIN, HIGH);
            
            // AUTOMATIC STORAGE COMIT ACTION LOG
            // Every time an alert loop cycles, appends the metrics + coordinates to memory disk
            logThreatToCSV(localMetrics);
        } else {
            digitalWrite(ALARM_PIN, LOW);
        }

        if(gps.location.isValid()){
            localMetrics.latitude = gps.location.lat();
            localMetrics.longitude = gps.location.lng();
            localMetrics.gpsValid = true;
        } else {
            localMetrics.latitude = 0.0;
            localMetrics.longitude = 0.0;
            localMetrics.gpsValid = false;
        }
        localMetrics.satellites = gps.satellites.value();

        // Print Logs to Serial Monitor
        Serial.print("[DATA LOG] CO: "); Serial.print(localMetrics.coPPM);
        Serial.print(" PPM | Methane: "); Serial.print(localMetrics.methanePPM);
        Serial.print(" PPM | Metal: "); Serial.println(localMetrics.metalDetected ? "THREAT" : "OK");

        xQueueSend(telemetryQueue, &localMetrics, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ================= GPS SERIAL BUFFER TASK (CORE 1) =================
void vGPSTask(void *pvParameters) {
    for(;;){
        while(SerialGPS.available() > 0){
            gps.encode(SerialGPS.read());
        }
        vTaskDelay(pdMS_TO_TICKS(40)); 
    }
}

// ================= ASYNC WEB TRANSMITTER TASK (CORE 0) =================
void vWebTask(void *pvParameters) {
    TacticalData outboundPayload;
    char jsonBuffer[256];

    for(;;){
        if(xQueueReceive(telemetryQueue, &outboundPayload, portMAX_DELAY) == pdPASS){
            
            snprintf(jsonBuffer, sizeof(jsonBuffer),
                "{\"co\":%d,\"methane\":%d,\"metal\":%s,\"coDangerous\":%s,\"methaneDangerous\":%s,\"lat\":%.6f,\"lon\":%.6f,\"sat\":%d,\"gpsValid\":%s}",
                outboundPayload.coPPM,
                outboundPayload.methanePPM,
                outboundPayload.metalDetected ? "true" : "false",
                outboundPayload.coDangerous ? "true" : "false",
                outboundPayload.methaneDangerous ? "true" : "false",
                outboundPayload.latitude,
                outboundPayload.longitude,
                outboundPayload.satellites,
                outboundPayload.gpsValid ? "true" : "false"
            );

            ws.textAll(jsonBuffer);
        }
    }
}