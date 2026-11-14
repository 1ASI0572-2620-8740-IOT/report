// HydroGuard ESP32: mismo firmware para Wokwi y prototipo academico.
// Serial 115200: configuracion JSON en RAM; nunca se imprimen/guardan credenciales.
// Wokwi: {"edgeUrl":"http://host.wokwi.internal:5000","deviceId":"UUID","credential":"SECRETO","mode":"SIMULATION","phInput":"WOKWI_POTENTIOMETER"}
// WiFi opcional: ssid/password; CA publica incluida; rootCa opcional para otra CA.
// PH4502C: ajuste de dos buffers pH 4.003 / 6.864, medidos cerca de 25 C.
// Referencias fijas del montaje medido; verificar nuevamente con todos los LED.
// mode debe coincidir con el registro: SIMULATION (Wokwi) o ACADEMIC_PROTOTYPE (fisico).
// Ambos representan la correccion mediante LED; no acreditan dosificacion ni calidad real.
// "bench" habilita pruebas aisladas c/v. "e" y boton rojo cierran localmente; "r" retira bloqueo local.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <mbedtls/md.h>
#include <time.h>
#include <math.h>
#include <atomic>
#include <algorithm>
#include <mbedtls/version.h>
// Limit ECDHE to P-256: X25519 in the captured handshakes was still too slow
// under emulation. Certificate/hostname validation remains in the standard client.
class EdgeTlsClient : public WiFiClientSecure {
 public:
  using WiFiClientSecure::connect;
  int connect(const char* host,uint16_t port) override {
    // Prepare TCP + TLS configuration only; do not write HTTP in this phase.
    setPlainStart();
    if(!WiFiClientSecure::connect(host,port)){_stillinPlainStart=false;return 0;}
#if MBEDTLS_VERSION_MAJOR >= 3
    static const uint16_t groups[]={0x0017,0}; // IANA secp256r1
    mbedtls_ssl_conf_groups(&sslclient->ssl_conf,groups);
#else
    static const mbedtls_ecp_group_id curves[]={MBEDTLS_ECP_DP_SECP256R1,MBEDTLS_ECP_DP_NONE};
    mbedtls_ssl_conf_curves(&sslclient->ssl_conf,curves);
#endif
    int ready=startTLS();
    if(!ready){stop();_stillinPlainStart=false;}
    return ready; // HTTPClient must not send headers unless TLS completed.
  }
  int connect(const char* host,uint16_t port,int32_t timeout) override {
    _timeout=timeout;return connect(host,port);
  }
};
// Public intermediate CA pin, NOT a device credential. Same policy on both targets.
// Google Trust Services WE1 (official pki.goog/repo/certs/we1.pem), expires 2029-02-20.
// Shortens validation to the server leaf; hostname and certificate dates still apply.
// Update rootCa explicitly if the tunnel changes issuer; never fall back to insecure.
const char EDGE_ROOT_CA[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIICjjCCAjOgAwIBAgIQf/NXaJvCTjAtkOGKQb0OHzAKBggqhkjOPQQDAjBQMSQw
IgYDVQQLExtHbG9iYWxTaWduIEVDQyBSb290IENBIC0gUjQxEzARBgNVBAoTCkds
b2JhbFNpZ24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMjMxMjEzMDkwMDAwWhcN
MjkwMjIwMTQwMDAwWjA7MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRy
dXN0IFNlcnZpY2VzMQwwCgYDVQQDEwNXRTEwWTATBgcqhkjOPQIBBggqhkjOPQMB
BwNCAARvzTr+Z1dHTCEDhUDCR127WEcPQMFcF4XGGTfn1XzthkubgdnXGhOlCgP4
mMTG6J7/EFmPLCaY9eYmJbsPAvpWo4IBAjCB/zAOBgNVHQ8BAf8EBAMCAYYwHQYD
VR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIGA1UdEwEB/wQIMAYBAf8CAQAw
HQYDVR0OBBYEFJB3kjVnxP+ozKnme9mAeXvMk/k4MB8GA1UdIwQYMBaAFFSwe61F
uOJAf/sKbvu+M8k8o4TVMDYGCCsGAQUFBwEBBCowKDAmBggrBgEFBQcwAoYaaHR0
cDovL2kucGtpLmdvb2cvZ3NyNC5jcnQwLQYDVR0fBCYwJDAioCCgHoYcaHR0cDov
L2MucGtpLmdvb2cvci9nc3I0LmNybDATBgNVHSAEDDAKMAgGBmeBDAECATAKBggq
hkjOPQQDAgNJADBGAiEAokJL0LgR6SOLR02WWxccAq3ndXp4EMRveXMUVUxMWSMC
IQDspFWa3fj7nLgouSdkcPy1SdOR2AGm9OQWs7veyXsBwA==
-----END CERTIFICATE-----
)PEM";

constexpr int PH_PIN=35, TEMPERATURE_PIN=4, SERVO_PIN=18;
constexpr int ACTIVITY_LED_PIN=25, CORRECTION_LED_PIN=26, ALARM_LED_PIN=27;
constexpr int CONFIRM_PIN=32, EMERGENCY_PIN=33;
constexpr uint32_t CONTROL_GRACE_MS=8000, SAMPLE_MS=5000, HEARTBEAT_MS=30000;
constexpr uint32_t ACTIVITY_PULSE_MS=100;
constexpr bool activityPulseExpired(uint32_t now,uint32_t started){
  return (int32_t)(now-started)>=(int32_t)ACTIVITY_PULSE_MS;
}
static_assert(!activityPulseExpired(1099,1000),"Keep pulse before 100 ms");
static_assert(activityPulseExpired(1100,1000),"End pulse at 100 ms");
static_assert(!activityPulseExpired(999,1000),"Ignore earlier watchdog snapshot");
static_assert(activityPulseExpired(49,UINT32_MAX-50),"Pulse survives millis wraparound");
constexpr int SLOTS=8;
OneWire oneWire(TEMPERATURE_PIN);
DallasTemperature sensors(&oneWire);
Servo valve;
Preferences journal;
String edgeUrl,deviceId,credential,organizationId,rootCa=EDGE_ROOT_CA,mode="SIMULATION";
String phInput="PH4502C";
// ADC voltage AFTER the two-equal-resistor divider, in millivolts.
// Calibration constants retain D34 names because the buffers were measured on GPIO34.
// GPIO35 is now under comparison; the original coefficients remain unchanged.
// Buffer readings supplied on 2026-11-09: 24.94 C / 24.88 C, respectively.
// Use the actual divider, supply and LED configuration used for these readings.
// No automatic baseline reset or temperature compensation is applied here.
// Accuracy outside the two reference points still requires verification.
constexpr double PH_ACID_VALUE=4.003;
constexpr double PH_ACID_D34_MV=(2265.4+2264.4+2264.4+2266.8+2266.0+2264.0+2264.0+2263.5+2265.5)/9.0;
constexpr double PH_NEUTRAL_VALUE=6.864;
constexpr double PH_NEUTRAL_D34_MV=(2014.7+2015.4+2015.1+2015.5+2016.4+2017.9+2015.7)/7.0;
constexpr double PH_D34_MV_PER_UNIT=(PH_ACID_D34_MV-PH_NEUTRAL_D34_MV)/(PH_NEUTRAL_VALUE-PH_ACID_VALUE);
constexpr double phFromMillivolts(double mv){
  if(!(mv>=150 && mv<=3100))return NAN;
  double value=PH_ACID_VALUE+(PH_ACID_D34_MV-mv)/PH_D34_MV_PER_UNIT;
  return value>=0 && value<=14?value:NAN;
}
constexpr double phFromPotentiometer(double raw){
  return raw>=0 && raw<=4095?raw*14.0/4095.0:NAN;
}
// Arithmetic checks run at compile time, without controlling hardware.
static_assert(PH_D34_MV_PER_UNIT>87.05 && PH_D34_MV_PER_UNIT<87.07,"Measured buffer slope");
static_assert(fabs(phFromMillivolts(PH_ACID_D34_MV)-4.003)<0.000001,"Acid buffer anchor");
static_assert(fabs(phFromMillivolts(PH_NEUTRAL_D34_MV)-6.864)<0.000001,"Neutral buffer anchor");
static_assert(fabs(phFromMillivolts(PH_ACID_D34_MV-PH_D34_MV_PER_UNIT)-5.003)<0.000001,"One unit increase");
static_assert(fabs(phFromMillivolts(PH_ACID_D34_MV+PH_D34_MV_PER_UNIT)-3.003)<0.000001,"One unit decrease");
static_assert(fabs(phFromMillivolts((PH_ACID_D34_MV+PH_NEUTRAL_D34_MV)/2)-5.4335)<0.000001,"Interpolation");
static_assert(phFromMillivolts(2200)>phFromMillivolts(2300),"Voltage direction");
static_assert(phFromMillivolts(2266.8)>3.98 && phFromMillivolts(2266.8)<3.99,"Acid buffer high voltage");
static_assert(phFromMillivolts(2263.5)>4.01 && phFromMillivolts(2263.5)<4.02,"Acid buffer low voltage");
static_assert(phFromMillivolts(2017.9)>6.84 && phFromMillivolts(2017.9)<6.85,"Neutral buffer high voltage");
static_assert(phFromMillivolts(2014.7)>6.87 && phFromMillivolts(2014.7)<6.88,"Neutral buffer low voltage");
static_assert(!(phFromMillivolts(0)>=0),"Electrical zero rejected");
static_assert(!(phFromMillivolts(3200)>=0),"ADC range rejected");
static_assert(!(phFromMillivolts(150)>=0),"Invalid result not clamped");
static_assert(!(phFromMillivolts(2800)>=0),"Negative result not clamped");
static_assert(!(phFromMillivolts(NAN)>=0),"NaN rejected");
static_assert(!(phFromMillivolts(INFINITY)>=0),"Infinity rejected");
static_assert(phFromPotentiometer(0)==0,"Virtual low endpoint");
static_assert(phFromPotentiometer(4095)==14,"Virtual high endpoint");
static_assert(phFromPotentiometer(2047.5)==7,"Virtual midpoint");
static_assert(!(phFromPotentiometer(4096)>=0),"Virtual invalid input");
bool identityReady=false;
// Keep transport objects alive across requests so HTTP/1.1 can reuse verified TLS.
WiFiClient edgePlain;
EdgeTlsClient edgeSecure;
HTTPClient edgeHttp;
String wifiSsid="Wokwi-GUEST",wifiPassword,serialLine,pendingMeasurement,bootId;
String ntpServerAddress;
String activeId,activeType;
int activeSlot=-1,appliedVersion=0;
bool benchMode=false,localLock=false,remoteStopped=false,sensorFault=true,pwmReady=false,nvsReady=false,valveOpen=false,correcting=false;
bool ntpStarted=false;
double phValue=NAN,temperatureValue=NAN;
double phMillivolts=0,phRaw=0,phSpanMv=0;
// Temporary read-only diagnostics for the A-E measurement protocol.
char diagnosticPhase='?';
uint32_t diagnosticSample=0;
int diagnosticLeds(){
  return (digitalRead(ACTIVITY_LED_PIN)==HIGH?1:0)
    | (digitalRead(CORRECTION_LED_PIN)==HIGH?2:0)
    | (digitalRead(ALARM_LED_PIN)==HIGH?4:0);
}
uint32_t sequence=0,lastSample=0,lastPoll=0,lastHeartbeat=0,lastConfig=0,lastAuth=0,lastControl=0;
uint32_t valveOpened=0,correctionStarted=0,correctionDuration=5000,activeExpires=0;
uint32_t lastWifiAttempt=0,lastGreenChange=0;
int previousGreen=HIGH,stableGreen=HIGH;
constexpr int VALVE_CLOSED_ANGLE=180, VALVE_OPEN_ANGLE=135;
std::atomic<int> requestedAngle{VALVE_CLOSED_ANGLE};
std::atomic<bool> requestedCorrection{false},watchdogTrip{false},benchGuard{false};
std::atomic<uint32_t> openDeadline{0},correctionDeadline{0},controlSeen{0};
std::atomic<uint32_t> activityPulseStarted{0};
std::atomic<bool> activityPulseActive{false};

bool clockReady(){return time(nullptr)>1700000000;}
String utcNow(){
  time_t now=time(nullptr);struct tm t;gmtime_r(&now,&t);
  char result[25];strftime(result,sizeof(result),"%Y-%m-%dT%H:%M:%SZ",&t);return String(result);
}
time_t isoTime(const String& value){
  struct tm t={};if(value.length()<20 || value.charAt(value.length()-1)!='Z' || !strptime(value.c_str(),"%Y-%m-%dT%H:%M:%S",&t))return 0;
  return mktime(&t); // setup fixes TZ=UTC; fractional seconds are conservatively truncated.
}
void alarms(){digitalWrite(ALARM_LED_PIN,localLock || remoteStopped || sensorFault || !pwmReady);}
void closeValve(){requestedAngle.store(VALVE_CLOSED_ANGLE);valveOpen=false;}
void safeOutputs(){closeValve();correcting=false;requestedCorrection.store(false);}
// Independent GPIO/PWM watchdog: HTTP/TLS timeouts cannot postpone the local close.
// This task is the only writer to Servo PWM after setup.
void hardwareWatchdog(void*){
  int lastAngle=-1;
  for(;;){
    uint32_t now=millis();
    // End the activity pulse even while the main loop is blocked in HTTP/TLS.
    if(activityPulseActive.load() && activityPulseExpired(now,activityPulseStarted.load())){
      digitalWrite(ACTIVITY_LED_PIN,LOW);activityPulseActive.store(false);
    }
    if(digitalRead(EMERGENCY_PIN)==LOW || ((!benchGuard.load()) && (requestedAngle.load()!=VALVE_CLOSED_ANGLE || requestedCorrection.load()) && now-controlSeen.load()>CONTROL_GRACE_MS)){
      requestedAngle.store(VALVE_CLOSED_ANGLE);requestedCorrection.store(false);watchdogTrip.store(true);
    }
    if(requestedAngle.load()!=VALVE_CLOSED_ANGLE && (int32_t)(now-openDeadline.load())>=0)requestedAngle.store(VALVE_CLOSED_ANGLE);
    if(requestedCorrection.load() && correctionDeadline.load()!=0 && (int32_t)(now-correctionDeadline.load())>=0)requestedCorrection.store(false);
    int angle=requestedAngle.load();if(angle!=lastAngle){valve.write(angle);lastAngle=angle;}
    digitalWrite(CORRECTION_LED_PIN,requestedCorrection.load()?HIGH:LOW);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
String slotKey(int slot){return "cmd"+String(slot);}
String digest(const String& value){
  unsigned char bytes[32];mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),(const unsigned char*)value.c_str(),value.length(),bytes);
  char hex[65];for(int i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",bytes[i]);return String(hex);
}
bool writeSlot(int slot,JsonDocument& doc){
  String value;serializeJson(doc,value);return nvsReady && journal.putString(slotKey(slot).c_str(),value)==value.length();
}
void finishActive(const char* status,const char* evidence){
  if(activeSlot<0)return;
  JsonDocument doc;if(deserializeJson(doc,journal.getString(slotKey(activeSlot).c_str(),""))){safeOutputs();activeSlot=-1;return;}
  JsonObject ack=doc["ack"].to<JsonObject>();ack["executionNumber"]=doc["executionNumber"].as<int>();
  ack["status"]=status;ack["occurredAt"]=utcNow();ack["executionMode"]=mode;ack["evidence"]=evidence;
  doc["phase"]="DONE";doc["sent"]=false;
  if(!writeSlot(activeSlot,doc))Serial.println("No se pudo persistir el resultado; actuacion bloqueada.");
  activeSlot=-1;activeId="";activeType="";
}
void localStop(){
  safeOutputs();localLock=true;
  if(activeSlot>=0 && clockReady())finishActive("FAILED","Parada local: salidas cerradas, no acredita conformidad.");
  alarms();Serial.println("PARADA LOCAL: valvula cerrada y actuacion bloqueada.");
}
void confirmCorrection(){
  if(!correcting || activeSlot<0 || sensorFault || localLock){Serial.println("No hay correccion manual valida pendiente de confirmar.");return;}
  correcting=false;requestedCorrection.store(false);
  finishActive("COMPLETED","Intervencion manual confirmada; calidad pendiente de nueva medicion y reevaluacion.");
}
bool localHttp(const String& url){
  if(!url.startsWith("http://"))return false;
  String host=url.substring(7);int end=host.indexOf('/');if(end>=0)host=host.substring(0,end);
  int colon=host.indexOf(':');if(colon>=0)host=host.substring(0,colon);
  if(host=="host.wokwi.internal" || host=="localhost")return true;
  IPAddress ip;if(!ip.fromString(host))return false;
  return ip[0]==127 || ip[0]==10 || (ip[0]==192 && ip[1]==168) || (ip[0]==172 && ip[1]>=16 && ip[1]<=31);
}
void resetTransport(){
  edgeHttp.setReuse(false);edgeHttp.end();edgeSecure.stop();edgePlain.stop();
  edgeHttp.setReuse(true);
}
int request(const char* method,const String& path,const String& body,String& result){
  result="";
  if(WiFi.status()!=WL_CONNECTED || edgeUrl.isEmpty() || deviceId.isEmpty() || credential.isEmpty())return -1;
  WiFiClientSecure& secure=edgeSecure;HTTPClient& http=edgeHttp;
  http.setReuse(true);
  http.setConnectTimeout(5000);http.setTimeout(10000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  bool opened=false;
  if(edgeUrl.startsWith("https://")){
    if(rootCa.indexOf("BEGIN CERTIFICATE")<0)return -2;
    // Don't reset a live TLS client's configuration on every polling request.
    if(!secure.connected()){secure.setCACert(rootCa.c_str());secure.setHandshakeTimeout(15);}
    opened=http.begin(secure,edgeUrl+path);
  }else if(localHttp(edgeUrl))opened=http.begin(edgePlain,edgeUrl+path);
  if(!opened)return -2;
  http.addHeader("Content-Type","application/json");
  // Standard Basic credentials over verified HTTPS (HTTP only on trusted local LAN).
  // Edge owns the backend JWT lifecycle; firmware never receives or parses tokens.
  http.setAuthorization(deviceId.c_str(),credential.c_str());
  int code=String(method)=="GET"?http.GET():http.POST(body);
  if(code<0){
    Serial.printf("Transporte HTTP (%d): %s\n",code,HTTPClient::errorToString(code).c_str());
    if(edgeUrl.startsWith("https://")){char tlsDetail[160]={};int tlsCode=secure.lastError(tlsDetail,sizeof(tlsDetail));Serial.printf("TLS (%d): %s\n",tlsCode,tlsDetail);}
  }
  // Always consume the response before reusing the connection. Oversized responses
  // invalidate the socket instead of leaving bytes to corrupt the next HTTP reply.
  if(code>0){
    if(http.getSize()>16384)code=-3;
    else {result=http.getString();if(result.length()>16384){result="";code=-3;}}
  }
  if(code<0 || code==401 || code==403)resetTransport();
  else http.end(); // setReuse(true) keeps a fully consumed keep-alive socket open.
  if(code==401 || code==403){identityReady=false;safeOutputs();}return code;
}

void authenticate(){
  if(!clockReady() || deviceId.isEmpty() || credential.isEmpty() || millis()-lastAuth<5000)return;
  lastAuth=millis();String response;
  int code=request("GET","/edge/v1/device-auth/session","",response);
  if(code==200){JsonDocument result;if(!deserializeJson(result,response) && result["deviceId"].as<String>()==deviceId){
    organizationId=result["organizationId"].as<String>();identityReady=!organizationId.isEmpty();}}
  if(!identityReady)Serial.printf("Conexion de dispositivo no disponible (%d); salidas retenidas.\n",code);
  else Serial.println("Dispositivo conectado. Edge gestiona la autenticacion; credenciales no se imprimen.");
}
void sample(){
  if(millis()-lastSample<SAMPLE_MS)return;lastSample=millis();
  // The activity LED stays off throughout temperature and pH acquisition.
  activityPulseActive.store(false);digitalWrite(ACTIVITY_LED_PIN,LOW);
  sensors.requestTemperatures();temperatureValue=sensors.getTempCByIndex(0);
  // Short burst: trim 5 samples at either end to reject isolated ADC spikes.
  // No multi-minute moving average: a real change must not be hidden by filtering.
  uint32_t rawSamples[31],mvSamples[31];
  const uint32_t acquisitionStart=millis();
  const int ledsBefore=diagnosticLeds();
  const bool faultBefore=sensorFault;
  bool ledsChanged=false;
  for(int i=0;i<31;i++){
    ledsChanged|=diagnosticLeds()!=ledsBefore;
    rawSamples[i]=analogRead(PH_PIN);mvSamples[i]=analogReadMilliVolts(PH_PIN);
    ledsChanged|=diagnosticLeds()!=ledsBefore;
    delay(2);
  }
  const int ledsAfter=diagnosticLeds();
  ledsChanged|=ledsAfter!=ledsBefore;
  const uint32_t acquisitionMs=millis()-acquisitionStart;
  std::sort(rawSamples,rawSamples+31);std::sort(mvSamples,mvSamples+31);
  uint32_t rawSum=0,mvSum=0;for(int i=5;i<26;i++){rawSum+=rawSamples[i];mvSum+=mvSamples[i];}
  phRaw=rawSum/21.0;phMillivolts=mvSum/21.0;phSpanMv=mvSamples[25]-mvSamples[5];
  // Separate the virtual potentiometer from the physical module's voltage model.
  // Never clamp a failed conversion to an apparently valid pH.
  bool simulatedInput=phInput=="WOKWI_POTENTIOMETER";
  phValue=simulatedInput?phFromPotentiometer(phRaw):phFromMillivolts(phMillivolts);
  // Rail checks catch some electrical faults, not every disconnected electrode.
  if(!simulatedInput && (phRaw<=1 || phRaw>=4094))phValue=NAN;
  sensorFault=temperatureValue==DEVICE_DISCONNECTED_C || !isfinite(temperatureValue) || temperatureValue< -55 || temperatureValue>125 || !isfinite(phValue) || phValue<0 || phValue>14;
  // Diagnostics remain visible even when pH cannot be published. No secrets.
  Serial.printf("ADC=%.1f D35=%.1f mV Po_estimado=%.1f mV dispersion=%.1f mV T=%.2f C modo=%s entrada=%s\n",
    phRaw,phMillivolts,phMillivolts*2,phSpanMv,temperatureValue,mode.c_str(),phInput.c_str());
  Serial.printf("DIAG fase=%c muestra=%lu inicio_ms=%lu ventana_ms=%lu D35_min=%lu mediana=%lu max=%lu recortado_min=%lu recortado_max=%lu mV\n",
    diagnosticPhase,(unsigned long)++diagnosticSample,(unsigned long)acquisitionStart,(unsigned long)acquisitionMs,
    (unsigned long)mvSamples[0],(unsigned long)mvSamples[15],(unsigned long)mvSamples[30],
    (unsigned long)mvSamples[5],(unsigned long)mvSamples[25]);
  Serial.printf("DIAG LED_antes_GYR=%d%d%d LED_fin_GYR=%d%d%d cambio_observado=%d fallo_previo=%d fallo_actual=%d bloqueo_local=%d parada_remota=%d pwm_ok=%d banco=%d wifi_estado=%d RSSI_dBm=%d identidad=%d config=%d\n",
    !!(ledsBefore&1),!!(ledsBefore&2),!!(ledsBefore&4),
    !!(ledsAfter&1),!!(ledsAfter&2),!!(ledsAfter&4),(int)ledsChanged,
    (int)faultBefore,(int)sensorFault,(int)localLock,(int)remoteStopped,(int)pwmReady,(int)benchMode,
    (int)WiFi.status(),WiFi.status()==WL_CONNECTED?(int)WiFi.RSSI():0,(int)identityReady,appliedVersion);
  if(temperatureValue==DEVICE_DISCONNECTED_C)Serial.println("DS18B20 no detectado: revisar D4, tierra, alimentacion y pull-up.");
  if(!isfinite(phValue))Serial.println("Lectura pH fuera de rango: revisar senal electrica; no se recorta ni se publica el resultado.");
  if(sensorFault){pendingMeasurement="";safeOutputs();if(activeSlot>=0 && clockReady())finishActive("FAILED","Lectura de sensor no valida; salidas cerradas.");}
  else {
    Serial.printf("pH=%.2f T=%.2f C (%s) entrada=%s\n",phValue,temperatureValue,mode.c_str(),phInput.c_str());
    if(!benchMode && clockReady() && pendingMeasurement.isEmpty()){
      JsonDocument doc;doc["measurementId"]=bootId+"-"+String(++sequence);doc["ph"]=phValue;doc["temperature"]=temperatureValue;doc["measuredAt"]=utcNow();serializeJson(doc,pendingMeasurement);
    }
  }
  // Indicate acquisition, not successful authentication or cloud delivery.
  // pwmReady also confirms that the periodic watchdog task was started.
  if(pwmReady){
    activityPulseStarted.store(millis());digitalWrite(ACTIVITY_LED_PIN,HIGH);activityPulseActive.store(true);
  }
  alarms();
}
void flushResults(){
  if(!identityReady || !clockReady())return;
  for(int slot=0;slot<SLOTS;slot++){
    JsonDocument doc;if(deserializeJson(doc,journal.getString(slotKey(slot).c_str(),"")))continue;
    if(doc["deviceId"].as<String>()!=deviceId || doc["organizationId"].as<String>()!=organizationId)continue;
    if(doc["phase"].as<String>()=="STARTED" && slot!=activeSlot){
      activeSlot=slot;finishActive("FAILED","Reinicio del firmware: cierre local ordenado; no se repite la actuacion.");return;
    }
    if(doc["phase"].as<String>()!="DONE" || doc["sent"].as<bool>())continue;
    String body,response;serializeJson(doc["ack"],body);
    int code=request("POST","/edge/v1/commands/"+doc["id"].as<String>()+"/acknowledgements",body,response);
    if(code>=200 && code<300){doc["sent"]=true;writeSlot(slot,doc);}return;
  }
}
void execute(JsonDocument& c){
  String id=c["commandId"].as<String>(),type=c["type"].as<String>();time_t expires=isoTime(c["expiresAt"].as<String>());
  bool safety=type=="CLOSE_VALVE" || type=="EMERGENCY_STOP";
  if(c["deviceId"].as<String>()!=deviceId || c["organizationId"].as<String>()!=organizationId || id.isEmpty() || id.length()>128 || expires<=time(nullptr)
      || !c["executionNumber"].is<int>() || !c["configurationVersion"].is<int>() || c["configurationVersion"].as<int>()!=appliedVersion)return;
  if(type!="CORRECT" && type!="OPEN_VALVE" && !safety)return;
  String canonical;serializeJson(c,canonical);String signature=digest(canonical);int freeSlot=-1;
  for(int i=0;i<SLOTS;i++){
    String saved=journal.getString(slotKey(i).c_str(),"");
    if(saved.isEmpty()){if(freeSlot<0)freeSlot=i;continue;}
    JsonDocument old;if(deserializeJson(old,saved))continue;
    if(old["id"].as<String>()==id){
      if(old["signature"].as<String>()!=signature){safeOutputs();Serial.println("Conflicto de commandId; no se ejecuta.");return;}
      if(old["phase"].as<String>()=="DONE"){old["sent"]=false;writeSlot(i,old);}return;
    }
    if(freeSlot<0 && old["phase"].as<String>()=="DONE" && old["sent"].as<bool>())freeSlot=i;
  }
  if(!nvsReady || freeSlot<0){if(safety)safeOutputs();Serial.println("Sin espacio de deduplicacion; no se autoriza una nueva apertura/correccion.");return;}
  if(safety && activeSlot>=0){safeOutputs();finishActive("FAILED","Actuacion interrumpida por orden de cierre/emergencia.");}
  if(!safety && activeSlot>=0)return;
  JsonDocument record;record["id"]=id;record["signature"]=signature;record["deviceId"]=deviceId;record["organizationId"]=organizationId;
  record["executionNumber"]=c["executionNumber"].as<int>();record["phase"]="STARTED";record["sent"]=false;
  if(!writeSlot(freeSlot,record)){safeOutputs();return;}
  activeSlot=freeSlot;activeId=id;activeType=type;activeExpires=(uint32_t)expires;
  if(safety){safeOutputs();remoteStopped=type=="EMERGENCY_STOP";finishActive("COMPLETED","Cierre ordenado/representado a 180 grados; no hay medicion de caudal.");alarms();return;}
  if(localLock || sensorFault || !pwmReady || digitalRead(EMERGENCY_PIN)==LOW || millis()-lastControl>CONTROL_GRACE_MS){
    safeOutputs();finishActive("REJECTED","Bloqueo local, sensor/PWM o comunicacion no validos.");return;
  }
  remoteStopped=false;uint32_t duration=c["payload"]["durationMs"] | 0;
  if(duration<1 || duration>5000){finishActive("REJECTED","Duracion fuera del perfil demostrativo.");return;}
  if(type=="OPEN_VALVE"){
    valveOpened=millis();openDeadline.store(valveOpened+5000);requestedAngle.store(VALVE_OPEN_ANGLE);valveOpen=true;
    finishActive("COMPLETED","Apertura ordenada/representada a 135 grados; cierre local maximo 5 s, sin medicion de flujo.");
  }else {
    correcting=true;correctionStarted=millis();correctionDuration=duration;
    correctionDeadline.store(correctionStarted+duration);requestedCorrection.store(true);
    Serial.println("CORRECCION: LED activo. Cambiar sensor representa el efecto; no demuestra dosificacion fisica.");
  }
  alarms();
}
void networkStep(){
  if(benchMode)return;
  if(WiFi.status()!=WL_CONNECTED){safeOutputs();if(millis()-lastWifiAttempt>10000){lastWifiAttempt=millis();WiFi.begin(wifiSsid.c_str(),wifiPassword.c_str());}return;}
  if(!ntpStarted){
    // Resolve BEFORE starting SNTP. Arduino-ESP32's DNS-cache clearing after
    // reconnect can run an outstanding SNTP DNS callback without the TCPIP lock
    // (upstream #13055, udp_new_ip_type:1278). Numeric SNTP addresses avoid it.
    IPAddress server;
    if(WiFi.hostByName("time.google.com",server)!=1)return;
    ntpServerAddress=server.toString(); // Persistent storage for SNTP's pointer.
    configTime(0,0,ntpServerAddress.c_str());ntpStarted=true;
  }
  if(!clockReady())return;
  if(!identityReady){authenticate();if(!identityReady)return;}
  flushResults();
  String response;
  if(!pendingMeasurement.isEmpty()){
    int code=request("POST","/edge/v1/telemetry",pendingMeasurement,response);
    if(code==202)pendingMeasurement="";
    else if(code==400 || code==409 || code==422){Serial.printf("Lectura rechazada (%d); revisar contrato. No se cambia su fecha para reintentar.\n",code);pendingMeasurement="";}
  }
  if(millis()-lastHeartbeat>=HEARTBEAT_MS){
    lastHeartbeat=millis();JsonDocument h;h["messageId"]=bootId+"-hb-"+String(++sequence);h["occurredAt"]=utcNow();String body;serializeJson(h,body);
    request("POST","/edge/v1/heartbeat",body,response);
  }
  if(millis()-lastConfig>=5000){
    lastConfig=millis();int code=request("GET","/edge/v1/configuration","",response);
    if(code==200){JsonDocument cfg;if(!deserializeJson(cfg,response) && cfg["configurationVersion"].is<int>()){
      int version=cfg["configurationVersion"].as<int>();
      if(version!=appliedVersion && !correcting && !valveOpen){safeOutputs();JsonDocument ack;ack["configurationVersion"]=version;ack["occurredAt"]=utcNow();String body;serializeJson(ack,body);
        if(request("POST","/edge/v1/configuration/acknowledgements",body,response)==200){appliedVersion=version;Serial.printf("Configuracion v%d recibida/aplicada.\n",version);}
      }
    }}
  }
  if(appliedVersion>0 && millis()-lastPoll>=1000){
    lastPoll=millis();int code=request("GET","/edge/v1/commands/next","",response);
    if(code==200 || code==204){lastControl=millis();controlSeen.store(lastControl);}
    if(code==200){JsonDocument command;if(!deserializeJson(command,response))execute(command);}
  }
}
void configure(const String& line){
  JsonDocument config;auto parseError=deserializeJson(config,line);
  if(parseError){Serial.printf("Configuracion JSON no valida (%s, %u bytes). Envie una sola linea completa.\n",parseError.c_str(),(unsigned)line.length());return;}
  safeOutputs();if(activeSlot>=0 && clockReady())finishActive("FAILED","Cambio local de configuracion de transporte.");
  String url=config["edgeUrl"] | edgeUrl;
  while(url.endsWith("/"))url.remove(url.length()-1);
  if(!url.isEmpty() && (url.indexOf('@')>=0 || url.indexOf('?')>=0 || url.indexOf('#')>=0 || (!url.startsWith("https://") && !localHttp(url)))){Serial.println("URL rechazada: HTTP solo en LAN/gateway local; HTTPS requiere CA.");return;}
  String nextMode=config["mode"] | mode;
  if(nextMode!="SIMULATION" && nextMode!="ACADEMIC_PROTOTYPE"){
    Serial.println("mode debe ser SIMULATION o ACADEMIC_PROTOTYPE y coincidir con el registro del dispositivo.");return;
  }
  String nextInput=config["phInput"] | phInput;
  if(nextInput!="PH4502C" && nextInput!="WOKWI_POTENTIOMETER"){
    Serial.println("phInput debe ser PH4502C o WOKWI_POTENTIOMETER.");return;
  }
  if(nextInput=="WOKWI_POTENTIOMETER" && nextMode!="SIMULATION"){
    Serial.println("La escala del potenciometro solo se admite en SIMULATION.");return;
  }
  // Close before changing URL, identity or CA: no session may cross configurations.
  resetTransport();
  edgeUrl=url;mode=nextMode;phInput=nextInput;
  sensorFault=true;phValue=NAN;
  deviceId=config["deviceId"] | deviceId;credential=config["credential"] | credential;
  rootCa=config["rootCa"] | rootCa;
  String nextSsid=config["ssid"] | wifiSsid,nextPassword=config["password"] | wifiPassword;
  bool wifiChanged=nextSsid!=wifiSsid || nextPassword!=wifiPassword;
  wifiSsid=nextSsid;wifiPassword=nextPassword;
  identityReady=false;organizationId="";appliedVersion=0;benchMode=false;benchGuard.store(false);pendingMeasurement="";lastAuth=millis()-5000;lastWifiAttempt=millis();
  // Updating the Edge URL/identity does not require dropping an existing WiFi link.
  if(wifiChanged){WiFi.disconnect();WiFi.begin(wifiSsid.c_str(),wifiPassword.c_str());}
  else if(WiFi.status()!=WL_CONNECTED)WiFi.begin(wifiSsid.c_str(),wifiPassword.c_str());
  Serial.println("Transporte configurado en RAM. No publicar credenciales ni capturar esta entrada.");
  Serial.printf("Entorno de confirmaciones: %s. Debe coincidir con el registrado en la web.\n",mode.c_str());
}
void input(){
  while(Serial.available()){
    char ch=(char)Serial.read();if(ch=='\r')continue;
    if(ch!='\n'){if(serialLine.length()<12000)serialLine+=ch;else serialLine="";continue;}
    String command=serialLine;serialLine="";command.trim();
    if(command.startsWith("{"))configure(command);
    else if(command.length()==6 && command.startsWith("fase ") && command[5]>='A' && command[5]<='E'){
      diagnosticPhase=command[5];diagnosticSample=0;
      Serial.printf("DIAG MARCA fase=%c instante_ms=%lu (etiqueta manual; no cambia sensores ni actuacion)\n",diagnosticPhase,(unsigned long)millis());
    }
    else if(command=="confirm")confirmCorrection();
    else if(command=="e")localStop();
    else if(command=="r" && digitalRead(EMERGENCY_PIN)==HIGH){localLock=false;alarms();Serial.println("Bloqueo local retirado; el backend conserva su estado.");}
    else if(command=="bench"){localStop();benchMode=true;benchGuard.store(true);identityReady=false;Serial.println("BANCO AISLADO. Soltar rojo y usar r; c=LED, v=servo. Sin autorizacion del producto.");}
    else if(benchMode && !sensorFault && !localLock && pwmReady){
      if(command=="c"){correcting=true;correctionStarted=millis();correctionDeadline.store(correctionStarted+5000);requestedCorrection.store(true);}
      if(command=="v"){valveOpened=millis();openDeadline.store(valveOpened+5000);requestedAngle.store(VALVE_OPEN_ANGLE);valveOpen=true;}
    }
  }
}
void setup(){
  // A pasted PEM/JSON exceeds the default 256-byte UART buffer. Sensor/HTTP calls
  // can block the loop while bytes arrive; retain the entire bounded input line.
  Serial.setRxBufferSize(16384);
  Serial.begin(115200);setenv("TZ","UTC0",1);tzset();
  pinMode(ACTIVITY_LED_PIN,OUTPUT);pinMode(CORRECTION_LED_PIN,OUTPUT);pinMode(ALARM_LED_PIN,OUTPUT);pinMode(CONFIRM_PIN,INPUT_PULLUP);pinMode(EMERGENCY_PIN,INPUT_PULLUP);
  digitalWrite(ACTIVITY_LED_PIN,LOW);
  analogReadResolution(12);analogSetPinAttenuation(PH_PIN,ADC_11db);
  valve.setPeriodHertz(50);valve.attach(SERVO_PIN,500,2400);pwmReady=valve.attached();safeOutputs();
  valve.write(VALVE_CLOSED_ANGLE);
  if(pwmReady && xTaskCreate(hardwareWatchdog,"hydroguard-safety",3072,nullptr,2,nullptr)!=pdPASS)pwmReady=false;
  sensors.begin();sensors.setResolution(12);nvsReady=journal.begin("hydroguard",false);
  char id[24];snprintf(id,sizeof(id),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());bootId=id;
  WiFi.mode(WIFI_STA);WiFi.begin(wifiSsid.c_str(),wifiPassword.c_str());alarms();
  Serial.println("HydroGuard: salidas cerradas. Pegue configuracion JSON por Serial; credenciales solo en RAM.");
  Serial.println("Transporte TLS: WE1 + P-256 + keep-alive (revision 2).");
  Serial.printf("Conversion PH4502C vigente: pH = %.3f + (%.4f - mV_D35) / %.7f.\n",PH_ACID_VALUE,PH_ACID_D34_MV,PH_D34_MV_PER_UNIT);
}
void loop(){
  if(watchdogTrip.exchange(false))localStop();
  if(digitalRead(EMERGENCY_PIN)==LOW && !localLock)localStop();
  input();sample();
  int green=digitalRead(CONFIRM_PIN);if(green!=previousGreen){previousGreen=green;lastGreenChange=millis();}
  if(green!=stableGreen && millis()-lastGreenChange>=40){stableGreen=green;if(stableGreen==LOW && correcting){if(activeSlot>=0)confirmCorrection();else {correcting=false;requestedCorrection.store(false);}}}
  if(correcting && millis()-correctionStarted>=correctionDuration){correcting=false;requestedCorrection.store(false);if(activeSlot>=0)finishActive("COMPLETED","Pulso LED correctivo finalizado; intervencion representada, no acredita conformidad.");}
  if(valveOpen && millis()-valveOpened>=5000)closeValve();
  if(activeSlot>=0 && clockReady() && (uint32_t)time(nullptr)>=activeExpires){safeOutputs();finishActive("FAILED","Orden vencida: salidas cerradas.");}
  if(!benchMode && (correcting || valveOpen) && (WiFi.status()!=WL_CONNECTED || millis()-lastControl>CONTROL_GRACE_MS)){safeOutputs();if(activeSlot>=0 && clockReady())finishActive("FAILED","Perdida de comunicacion de control: cierre local seguro.");}
  networkStep();alarms();delay(5);
}
