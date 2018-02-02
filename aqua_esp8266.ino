#include <Wire.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

File f;

#define DEBUG

#ifdef DEBUG
#  define DEBUG_LOG(x) Serial.print(x)
#  define DEBUG_LOGLN(x) Serial.println(x)
#else
#  define DEBUG_LOG(x)
#  define DEBUG_LOGLN(x)
#endif

time_t getNtpTime();
unsigned long currentTime, vyk_pradzia, vyk_pabaiga, likes_laikas;
byte timeZone = 2;
byte LED_t =  15;   
byte MAIST_t = 5;   
byte BURB_t = 60; 
byte LED_t1 = 30; 
byte delay_t= 5; 
int total_t = LED_t + BURB_t + LED_t1;
byte like_prod;

//LCD
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define ONE_WIRE_BUS 0
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress AQV = { 0x28, 0x12, 0x75, 0x44, 0x06, 0x00, 0x00, 0xAB }; 
//DeviceAddress KAM = { 0x28, 0xFF, 0x3B, 0xDC, 0x63, 0x16, 0x03, 0x4A }; mazeikiu
DeviceAddress KAM = { 0x28, 0xFF, 0x8A, 0xD2, 0x63, 0x16, 0x03, 0xDD };
float t_KAM, t_AQV;

//SERVO
Servo myservo;
#define Srv 15 //defining servo pin

//Reles 
#define SVIES 13
#define BURB 12
#define HEAT 14
#define REL4 16

#define butt A0 //buttons
#define MDoze 2000

int buttVal = 0;
//float maxAQVTemp = 27;
//float tempKAM, tempAQV;

boolean MAISTAS = true;
boolean VYKDYMAS = false;
boolean RUNNING = false;
boolean HEATER = false; 

void sendNTPpacket(IPAddress &address);
/*------------------ NTP SERVERS ------------------*/
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServer(188, 227, 227, 31);

const char ssid[] = "Namai40";
const char pass[] = "Zygios5505";

WiFiUDP Udp;
unsigned int localPort = 3232;  // Local port to listen for UDP packets
ESP8266WebServer server ( 80 );

void setup()
{
  Serial.begin(115200);
  
  lcd.begin(); //Init with pin default ESP8266 or ARDUINO
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("ESP8266 Akvariumas"));
  
  WiFi.begin(ssid, pass);
  IPAddress ip(192, 168, 1, 200);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);
  DEBUG_LOG("WiFi connecting: ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_LOG(".");
  }
  DEBUG_LOGLN("WiFi connected ");
  DEBUG_LOGLN(WiFi.localIP());
  SPIFFS.begin();

  formatMEM();
  readConfig();

  sensors.begin();
  sensors.setResolution(KAM, 9);
  sensors.setResolution(AQV, 9);
  myservo.attach(Srv);
  myservo.detach(); 
  
  server.begin();
  DEBUG_LOGLN(F("Starting UDP"));
  Udp.begin(localPort);
  DEBUG_LOG(F("Local port: "));
  DEBUG_LOGLN(Udp.localPort());
  DEBUG_LOGLN(F("waiting for sync"));
  setSyncProvider(getNtpTime);
  setSyncInterval(300); //300/60 = 5 minutes

  DEBUG_LOG(F("IP: http://"));
  DEBUG_LOGLN(WiFi.localIP());
  server.on ( "/", handleRoot );
  server.begin();
  DEBUG_LOGLN(F("HTTP server started" ));
  delay(1000);
  lcd.clear();
}

String printMinutes (int minutes)
{ String minPrint;
  if (minutes < 10) {
    minPrint = "0" + String(minutes);
  }
  else {
    minPrint = minutes;
  }
  return minPrint;
}

void LED_on(){
  DEBUG_LOGLN(F("Turn LED ON"));
  printTime();
  digitalWrite(SVIES, HIGH);
}

void MAISTAS_on(){
  DEBUG_LOGLN(F("Turn Maistas"));
  printTime();
}

void BURB_on(){
  DEBUG_LOGLN(F("Turn BURB ON"));  
  printTime();
  digitalWrite(BURB, HIGH);
}

void BURB_off(){
  DEBUG_LOGLN(F("Turn BURB OFF"));
  DEBUG_LOGLN(F("Leaving LED ON"));
  printTime();
  digitalWrite(BURB, LOW);
}

void LED_off(){
  DEBUG_LOGLN(F("Turn LED OFF"));
  DEBUG_LOGLN(F("LED OFF"));
  DEBUG_LOGLN(F("Delay ON"));
  printTime();
  digitalWrite(SVIES, LOW);
  VYKDYMAS = false;
}

void printTime(){
  Serial.print("Laikas:");
  Serial.print(hour());
  Serial.print(":");
  Serial.println(printMinutes(minute()));
}

String LeftTime(){
  int HH = likes_laikas / 3600;
  int MM = (likes_laikas / 60) % 60;
  String HHMM = ""+String(HH)+":"+String(printMinutes(MM));
  return HHMM;
}

int ShowHours()
{
  return likes_laikas / 3600;
}

int ShowMinutes()
{
  return (likes_laikas / 60) % 60;
}

void upd_LCD_1Line(){
  char curr_TimeBuffer[5];
  sprintf(curr_TimeBuffer,"%02u:%02u",hour(),minute());    
  lcd.setCursor(0,0);
  lcd.print(curr_TimeBuffer);
  lcd.setCursor(10,0);
  lcd.print("K:");
  lcd.print(t_KAM);
  lcd.setCursor(10,1);
  lcd.print("A:");
  lcd.print(t_AQV);
}

void getTemp(){
  sensors.requestTemperatures();
  t_KAM = sensors.getTempC(KAM);
  t_AQV = sensors.getTempC(AQV);
}



void loop()
{
  
  currentTime = (3600 * int(hour())) + (60*int(minute()))+int(second()); 
  getTemp();
  lcd.clear();
  upd_LCD_1Line();

  if(RUNNING == false && VYKDYMAS == true){
    RUNNING = true;
    vyk_pradzia = currentTime;
    vyk_pabaiga = vyk_pradzia + (60*total_t);
    DEBUG_LOGLN(vyk_pradzia);
    DEBUG_LOG("Vykdymo pabaiga:");
    DEBUG_LOGLN(vyk_pabaiga);
    LED_on();
    server.send ( 200, "text/html", getPage() );
  }else if (RUNNING == true && VYKDYMAS == true){
    likes_laikas = vyk_pabaiga - currentTime;
    //Serial.print("Likes laikas:");
    //Serial.println(likes_laikas);
    like_prod = 100 - ((likes_laikas*100)/(total_t *60));
    //Serial.println(like_prod);
    lcd.setCursor(0,1);
    char left_TimeBuffer[5];
    sprintf(left_TimeBuffer,"%02u:%02u",ShowHours(),ShowMinutes());    
    lcd.print(left_TimeBuffer);
  }else{
    lcd.setCursor(0,1);
    if(MAISTAS == true){
      lcd.print(F("MAIST:ON"));
    }else{
      lcd.print(F("MAIST:OFF"));
    }
  }
  
  //ciklas per vykdymas
  if(MAISTAS == true  && currentTime == (vyk_pradzia + (60*MAIST_t))){
      MAISTAS_on();
  }
  if(currentTime == (vyk_pradzia + (60*LED_t)) ){
    BURB_on();
  }
  if(currentTime == (vyk_pradzia+ ((LED_t + BURB_t)*60)) ){
    BURB_off();
  }
  if(currentTime == vyk_pabaiga){
    LED_off();
    RUNNING = false;
  }
  if(currentTime == (vyk_pabaiga + (delay_t *60))){
    Serial.println(F("Delay OFF"));
    VYKDYMAS = false;
    printTime();
  }
  
  server.handleClient();
  delay(1000);
}

String getPage(){
  String page = "<html lang='lt'><head><meta http-equiv='refresh' content='20' name='viewport' content='width=device-width, initial-scale=1' /><meta charset='utf-8'>";
  page += "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'><script src='https://ajax.googleapis.com/ajax/libs/jquery/3.1.1/jquery.min.js'></script><script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js'></script>";
  page += "<link href='https://gitcdn.github.io/bootstrap-toggle/2.2.2/css/bootstrap-toggle.min.css' rel='stylesheet'/><script src='https://gitcdn.github.io/bootstrap-toggle/2.2.2/js/bootstrap-toggle.min.js'></script>";
  page += "<title>ESP8266 Akvariumas</title></head><body>";
  page += "<div class='container-fluid'>";
  page +=   "<div class='row'>";
  page +=     "<div class='col-md-12'>";
  page +=       "<h1>Mažeikių akvariumas</h1>";
  page +=       "<h3>Internetinė versija</h3>";
  page +=       "<h2>"+String(hour())+":"+String(printMinutes(minute()))+"</h2>";
  page +=       "</div>";
  page +=       "<div class='col-md-6'>";
  page +=       "<br>";
  page +=       "<table class='table table-bordered table-striped'>";
  page +=         "<colgroup><col class='col-md-1'><col class='col-md-1'></colgroup>";
  page +=         "<thead><tr><th scope='col' class='col-md-1'></th>";
  page +=         "<th scope='col' class='col-md-1'>Kambario</th>";
  page +=         "<th scope='col' class='col-md-1'>Akvariumo</th>";
  page +=         "</tr></thead><tbody>";
  page +=         "<tr><th class='col-md-1'>Temperatūra</th><td class='col-md-1'>"+String(t_KAM)+"&deg;C</td><td class='col-md-1'>"+String(t_AQV)+"&deg;C</td>";
  page +=         "</tr></tbody></table>";
  page +=         "</div></div>";
  if(RUNNING == true && VYKDYMAS == true){
    page +=       "<h3>Vykdoma</h3>";
    page +=       "<div class='progress'>";
    page +=         "<div class='progress-bar progress-bar-striped active' role='progressbar' aria-valuenow='45' aria-valuemin='0' aria-valuemax='100' style='width: "+String(like_prod)+"%'>";
    page +=         "<span class='sr-only'>45% Complete</span>";
    page +=         "</div></div>";
    //page +=         "<h4>Likęs laikas "+String(ShowHours())+":"+String(ShowMinutes())+"</h4>";
    page +=         "<h4>"+LeftTime()+"</h4>";
    //page +=       "</div>";
  }else if (RUNNING == false && VYKDYMAS == false){
   page +=         " <div class='row'><div class='col-md-2'>";
    if(MAISTAS == true){
        page +=       "<h4>Maistas Įjungtas</h4><form action='/' method='POST'><div class='col-sm-1'><button type='button submit' name='MAISTAS' value='1' class='btn btn-danger'>Išjungti</button></div></form>";
    }else{
        page +=       "<h4>Maistas Išjungtas</h4><form action='/' method='POST'><div class='col-sm-1'><button type='button submit' name='MAISTAS' value='0' class='btn btn-success'>Įjungti</button></div></form>";
    }
    page +=      "</div></div>";
    page +=      "<div class='row'><div class='col-sm-2'><br><form action='/' method='POST'><button type='button submit' name='VYK' value='1' class='btn btn-success btn-lg'>Paleisti</button></form></div>";  
    page +=      "<br></div>";
    page +=       "<div class='row'>";
    page +=       "<div class='col-md-12'>";
    page +=       "<h3>Keisti laikus:</h3>";
    page +=       "<form><div class='form-group row'>";
    page +=         "<label for='inputLED' class='col-sm-1 col-form-label text-success'>Šviesa</label>";
    page +=         "<div class='col-sm-1'>";
    page +=         "<input type='text' class='form-control is-valid' id='inputLED' name='inputLED' value='"+String(LED_t)+"'>";
    page +=         "</div></div>";
    page +=         "<div class='form-group row'>";
    page +=         "<label for='inputMAIST' class='col-sm-1 col-form-label text-success'>Maistas</label>";
    page +=         "<div class='col-sm-1'>";
    page +=         "<input type='text' class='form-control is-valid' id='inputMAIST' name='inputMAIST' value='"+String(MAIST_t)+"'>";
    page +=         "</div></div>";
    page +=       "<div class='form-group row'>";
    page +=       "<label for='inputBURB' class='col-sm-1 col-form-label text-success'>Burbuliatorius</label>";
    page +=       "<div class='col-sm-1'>";
    page +=       "<input type='text' class='form-control is-invalid' id='inputBURB' name='inputBURB' value='"+String(BURB_t)+"'>";
    page +=       "</div></div>";
    page +=       "<div class='form-group row'>";
    page +=       "<label for='inputLED1' class='col-sm-1 col-form-label text-success' >Šviesa1</label>";
    page +=       "<div class='col-sm-1'>";
    page +=       "<input type='text' class='form-control is-valid' id='inputLED1' name='inputLED1'  value='"+String(LED_t1)+"'>";
    page +=       "</div></div>";
    page +=       "<button type='submit' class='btn btn-primary'  name='KEISTI'>Keisti</button>";
    page +=       "</form>";
  }
  page += "</div></div></div>";
  page += "</body></html>";
  return page;
}


 void formatMEM(){

  if (!SPIFFS.exists("/config.txt")) {
    DEBUG_LOGLN("Please wait 30 secs for SPIFFS to be formatted");
    SPIFFS.format();
    DEBUG_LOGLN("Spiffs formatted");
  } else {
    DEBUG_LOGLN("SPIFFS is formatted. Moving along...");
  }
 }

 void writeConfig(){
  f = SPIFFS.open("/config.txt", "w");
  if (!f) {
    DEBUG_LOGLN("file open failed");
  } else {
    DEBUG_LOGLN("=======Wrtiting to config file =======");
//    f.print("LED__t=");
//    f.println(LED_t);
//    f.print("MAIS_t=");
//    f.println(MAIST_t);
//    f.print("BURB_t=");
//    f.println(BURB_t);
//    f.print("LED_t1=");
//    f.println(LED_t1);
    f.println(LED_t);
    f.println(MAIST_t);
    f.println(BURB_t);
    f.println(LED_t1);
    f.close();
  }
 }

 void readConfig(){
  f = SPIFFS.open("/config.txt", "r");
  if (!f) {
    DEBUG_LOGLN("File open fail");
  }else{
    DEBUG_LOGLN("=======Reading file =======");
    for (int i=1; i<=4; i++){
      String line=f.readStringUntil('\n');
      DEBUG_LOGLN(line);
      if(i==1){
        LED_t = line.toInt();
      }else if(i == 2){
        MAIST_t = line.toInt();
      }else if( i ==3){
        BURB_t = line.toInt();
      }else if (i == 4){
        LED_t1 = line.toInt();
      }
    }
  }
  f.close();
  total_t = LED_t + BURB_t + LED_t1;
 }


void handleRoot(){ 
  if ( server.hasArg("VYK") ) {
    DEBUG_LOGLN("Paleidom Vykdyma is interneto");
    VYKDYMAS = true;
    server.send ( 200, "text/html", getPage() );
  } else if( server.hasArg("MAISTAS") ){
    if(MAISTAS == true){
       MAISTAS = false;
    }else{
      MAISTAS = true;
    }
    server.send ( 200, "text/html", getPage() );
  } else if( server.hasArg("KEISTI") ){
    LED_t = server.arg("inputLED").toInt();
    MAIST_t = server.arg("inputMAIST").toInt();
    BURB_t = server.arg("inputBURB").toInt();
    LED_t1 = server.arg("inputLED1").toInt();
    //writing to config file
    writeConfig();
    
//    inLED = server.arg("inputLED");
//    inBURB = server.arg("inputBURB");
//    inLED1 = server.arg("inputLED1");
//    Serial.println("InputLed:"+inLED);
//    Serial.println("InputBurb:"+inBURB);
//    Serial.println("InputLed1:"+inLED1);
   
    server.send ( 200, "text/html", getPage() );
  }else if( server.hasArg("READ") ){
    readConfig();
    server.send ( 200, "text/html", getPage() );
  }else {
    server.send ( 200, "text/html", getPage() );
  }  
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  DEBUG_LOGLN("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 5000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      DEBUG_LOGLN("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress & address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}


