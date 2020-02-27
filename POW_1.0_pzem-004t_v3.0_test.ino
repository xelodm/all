/**
* Измеритель электроэнергии PowerMeter_1.0
* 
* Контроллер ESP8266F (1-16Мбайт)
* Модуль измерения электроэнергии PZEM004Tv30
* 
При недоступности WiFi подключения старт в режиме точки дступа
Авторизованный доступ к странице настроек. Пароль к WEB по умолчанию "admin". Настройки сохраняются в EEPROM
Обновление прошивки через WEB

* 
* Copyright (C) 2016 Алексей Шихарбеев
* http://samopal.pro
*/
#include <ArduinoJson.h>
#include <arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>
#include "WC_EEPROM.h"
#include "WC_HTTP.h"
#include "WC_NTP.h"
#include "sav_button.h"

#ifdef ESP8266
extern "C" {
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "cont.h"
}
#endif
#define WIFI_TX_POWER 45 // TX power of ESP module (0 -> 0.25dBm) (0...85)




#include <SPI.h> 
#include "PZEM004Tv30.h"

#define PIN_DC      5
#define PIN_RESET   4
#define PIN_CS      15

#define PAGE_SIZE   300

// Перегружаться, если в точке доступа
#define AP_REBOOT_TM 600

#define PIN_BUTTON 0

PZEM004Tv30 pzem(&Serial);
//IPAddress ip(192,168,1,1);
uint32_t ms, ms1 = 0,ms2 = 0, ms3 = 0, ms4 = 0, ms_ok = 0;
uint32_t tm         = 0;
uint32_t t_cur      = 0;    
long  t_correct     = 0;


char     m_file[16];
uint16_t m_count = 0;
uint32_t m_size  = 0;
bool     m_write = true;
uint32_t m_tm    = 60000;
#define M_PAGE_SIZE 150

int m_page0[M_PAGE_SIZE];
int m_page_count0 = 0;


bool debug;
char s[100];


SButton b1(PIN_BUTTON ,100,2000,0,0,0); 

float u1=0.0,i1=0.0,p1=0.0,e1=0.0,hz1=0.0,pf1=0.0;
float p_max = 0, p_min = 99999999;
float u_avg=0.0,i_avg=0.0,p_avg=0.0,hz_avg=0.0,pf_avg=0.0;
int u_count=0,i_count=0,p_count=0,hz_count=0,pf_count=0;
int k_count=0,f_count=0;
int fn_allow=150; //разрешённое количество файлов
int fs_allow=70;  //разрешённая заполняемость файловой spiffs в процентах
int st_count=1000;  //разрешённое количество строк в файле
char     f_del[16];
//size_t f_dir_size=150;
//int n_file;
//int *f_dir = new int [n_file];
//int f_dir[150];

char srv[64] = "127.0.0.1"; // IP Adres (or name) of server to dump data to
//int srv_int[] = {127,0,0,2};
int prt = 80; //Port server send data

struct Config {
  int files_allow;
  int filesys_allow;
  int string_count;
  uint32_t min_tm;
  char server[64];
  int port;
};
const char *filename = "/config.js";  // <- SD library uses 8.3 filenames
Config config;                         // <- global configuration object
//config.fn_allow = 150;
//config.fs_allow = 70;
//config.st_count = 1000;
//config.m_tm = 60000;
//config.srv[64] = "127.0.0.1";
//config.prt = 80;


void setup() {

  #ifdef ESP8266
  system_phy_set_max_tpw(WIFI_TX_POWER); //установка мощности через функцию SDK
  #endif
 
// Последовательный порт для отладки
   DebugSerial.begin(115200);
   DebugSerial.println("PowerMonitor v 1.0");
// Считываем параметры из EEPROM   

   SPIFFS.begin();
   EC_begin();  
   EC_read();

//   saveConfig();
   if (!loadConfig()) {
     saveConfig();
   }
// Инициализируем кнопку
   b1.begin();   

// Подключаемся к WiFi  
   WiFi_begin();
   delay(1000);

  
// Старт внутреннего WEB-сервера
   if( isAP){ 
     
   }
  
   
   SetMFile();

   
// Запуск внутреннего WEB-сервера  
   HTTP_begin(); 
// Запуск модуля измерения ЭЭ   
//   pzem.setAddress(ip);
// Инициализация UDP клиента для NTP
  NTP_begin();  
     
}

void loop() {
   ms = millis();
// Обработка кнопки    
   switch( b1.Loop() ){
      case  SB_CLICK :
         WriteMFile(true);
         break;
      case  SB_LONG_CLICK :
         if( m_write == true )m_write = false;
         else m_write = true;
         break;
   }
// Опрос показаний ЭЭ
   if( ms1 == 0 || ms < ms1 || (ms - ms1)>500 ){
       ms1    = ms;
       t_cur  = ms/1000;
       tm     = t_cur + t_correct;

       float u2,i2,p2,e2,hz2,pf2;
       for( int i=0; i<3; i++ ){
           u2 = pzem.voltage();
           if( u2 != NAN ){u1 = u2; u_avg+=u1; u_count++; break; }
       }
       for( int i=0; i<3; i++ ){
           i2 = pzem.current();
           if( i2 != NAN ){i1 = i2; i_avg+=i1; i_count++; break; }
       }
       for( int i=0; i<3; i++ ){
           p2 = pzem.power();
           if( p2 != NAN ){p1 = p2; p_avg+=p1; p_count++; break; }
       }
       for( int i=0; i<3; i++ ){
           e2 = pzem.energy();
           if( e2 >= 9999){pzem.resetEnergy(); };
           if( e2 != NAN ){e1 = e2; break; }         
       }
       for( int i=0; i<3; i++ ){
           hz2 = pzem.frequency();
           if( hz2 != NAN ){hz1 = hz2; hz_avg+=hz1; hz_count++; break; }
       }
       for( int i=0; i<3; i++ ){
           pf2 = pzem.pf();
           if( pf2 != NAN ){pf1 = pf2; pf_avg+=pf1; pf_count++; break; }
       }
       if( p_max < p1 )p_max = p1;
       if( p_min > p1 )p_min = p1;
   
       
       

   }

   if( ms2 == 0 || ms < ms2 || (ms - ms2)>m_tm ){
       ms2 = ms;
       WriteMFile(false);
       if (k_count > st_count){
          SetMFile(); 
          k_count=0; 
       }
       k_count++;
   }
   
   if( ms4 == 0 || ms < ms4 || (ms - ms4)>2000 ){
      ms4 = ms;
      if( m_page_count0 >= M_PAGE_SIZE-1 ){
        p_max = 0;p_min = 99999999;
// Сдвигаем график        
        for( int i=0; i<M_PAGE_SIZE-1; i++ ){
             m_page0[i] = m_page0[i+1];
             if( p_max < m_page0[i] )p_max = m_page0[i];
             if( p_min > m_page0[i] )p_min = m_page0[i];
        }
        m_page0[M_PAGE_SIZE-1] = p1;
        if( p_max < p1 )p_max = p1;
        if( p_min > p1 )p_min = p1;
      }
      else {
        m_page0[m_page_count0++] = p1;
      }
      
       
   }


// Опрос NTP сервера
   if( !isAP && ( ms3 == 0 || ms < ms3 || (ms - ms3)>NTP_TIMEOUT )){
       uint32_t t = GetNTP();
       if( t!=0 ){
          ms3 = ms;
          t_correct = t - t_cur;
       }
   }

 
   if( isAP && ms > AP_REBOOT_TM*1000 ){
       DebugSerial.printf("TIMEOUT. REBOOT. \n");
       ESP.reset();
   }
   HTTP_loop();

}




void SetMFile(){
   //SPIFFS.gc();
   f_count = 0;
   int file_num = 0;
   Dir dir = SPIFFS.openDir("/");
   while (dir.next()) {    
      String fileName = dir.fileName();
      fileName.replace(".txt","");
      fileName.replace("/","");
      int n = fileName.toInt();
      if( n > file_num )file_num = n;
      f_count++;
   }
   file_num++;
  // f_count=file_num;
   sprintf(m_file,"/%08d.txt",file_num);
   m_count = 0;
   HTTP_get();
}

//int ArrayCompare(const int *AFirst, const int *ASecond) {
//   if (*AFirst < *ASecond) return -1;
//   return (*AFirst == *ASecond) ? 0 : 1;
//}


/*void Sort_f() {
   n_file=0;
   Dir dir_s = SPIFFS.openDir("/");
   while (dir_s.next()) {    
      String fn = dir_s.fileName();
      fn.replace(".txt","");
      fn.replace("/","");
      int k = fn.toInt();
      if (k>0){f_dir[n_file]=k;};
      n_file++;
   }
  
   int swap;
   //int t=sizeof(f_dir)/sizeof(int);
   int t=n_file-1;
   for (byte i = 0; i <= t-1; i++) {
     for (byte j = 0; j <= t-1; j++) {
       if (f_dir[i] < f_dir[j]) {
        swap = f_dir[i];
        f_dir[i] = f_dir[j];
        f_dir[j] = swap;
       }
     }
   } 
//   size_t f_dir_size = sizeof(f_dir) / sizeof(f_dir[0]);
//   qsort(f_dir, f_dir_size, sizeof(f_dir[0]), ArrayCompare);
//f_dir[5]=787;
}
*/

void WriteMFile(bool flag){
    
    if(m_write || flag ){
       if( u_count > 0 )u_avg/=u_count;
       if( i_count > 0 )i_avg/=i_count;
       if( p_count > 0 )p_avg/=p_count;
       if( hz_count > 0 )hz_avg/=hz_count;
       if( pf_count > 0 )pf_avg/=pf_count;
       File f;
       if( SPIFFS.exists(m_file) )f = SPIFFS.open(m_file, "a");
       else f = SPIFFS.open(m_file, "w");
       if( flag ){
          sprintf(s,"%ld;%d.%02d;%d.%02d;%d.%02d;%d.%02d;%d.%02d;%d.%02d;x\n",tm,
             (int)u1,((int)(u1*100))%100,
             (int)i1,((int)(i1*100))%100,
             (int)p1,((int)(p1*100))%100,
             (int)e1,((int)(e1*100))%100,
             (int)hz1,((int)(hz1*100))%100,
             (int)pf1,((int)(pf1*100))%100);
        
       }
       else {
          sprintf(s,"%ld;%d.%02d;%d.%02d;%d.%02d;%d.%02d;%d.%02d;%d.%02d;\n",tm,
             (int)u_avg,((int)(u_avg*100))%100,
             (int)i_avg,((int)(i_avg*100))%100,
             (int)p_avg,((int)(p_avg*100))%100,
             (int)e1,((int)(e1*100))%100,
             (int)hz_avg,((int)(hz_avg*100))%100,
             (int)pf_avg,((int)(pf_avg*100))%100);
       }
       m_size = f.size();
       int t =  f.print(s);             
       if (t == 0 || m_size == f.size() ){ f.close(); F_clear();};
       m_size = f.size();
       f.close();
       u_avg=0; u_count=0;
       i_avg=0; i_count=0;
       p_avg=0; p_count=0;
       hz_avg=0; hz_count=0;
       pf_avg=0; pf_count=0;
       m_count++;
       
       FSInfo fs_info;
       SPIFFS.info(fs_info);
       int t_b = fs_info.totalBytes;
       int u_b = fs_info.usedBytes;
       int used = (int)((u_b*100)/t_b);
       while (f_count > fn_allow || (used > fs_allow)){
          F_clear(); 
          f_count--;
          SPIFFS.info(fs_info);
          t_b = fs_info.totalBytes;
          u_b = fs_info.usedBytes;
          used = (int)((u_b*100)/t_b); 
       }
       
       HTTP_get();
    }
    
}

void F_clear(){
//       Dir dir = SPIFFS.openDir("/");
//             if (dir.next()) {    
//               if( dir.fileName().indexOf(".txt") > 0 )SPIFFS.remove(dir.fileName().c_str());
//             }
       int i=1;
       sprintf(f_del,"/%08d.txt",i);
       while (!SPIFFS.remove(f_del)){
              i++; 
              sprintf(f_del,"/%08d.txt",i); 
       }
  
}  


bool loadConfig() {
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    DebugSerial.println("Failed to open config file");
    return false;
  }

//  size_t size = configFile.size();
//  if (size > 1024) {
//    DebugSerial.println("Config file size is too large");
//    return false;
//  }

  // Allocate a buffer to store contents of the file.
  //std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  //configFile.readBytes(buf.get(), size);

  StaticJsonDocument<512> doc;
  //auto error = deserializeJson(doc, buf.get());
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    DebugSerial.println("Failed to parse config file");
    return false;
  }
  config.files_allow = doc["fn_allow"] | 20;
  config.filesys_allow = doc["fs_allow"] | 70;
  config.string_count = doc["st_count"] | 1000;
  config.min_tm = doc["m_tm"] | 60000;
  config.port = doc["port"] | 2731;
  strlcpy(config.server,                  // <- destination
          doc["server"] | "127.0.0.1",  // <- source
          sizeof(config.server));         // <- destination's capacity

  // Close the file (Curiously, File's destructor doesn't close the file)
  configFile.close();
  fn_allow = config.files_allow;
  fs_allow = config.filesys_allow;
  st_count = config.string_count;
  m_tm = config.min_tm;
  prt = config.port;
  strlcpy(srv,config.server,sizeof(config.server));
  
  //const char* serverName = doc["serverName"];
  //const char* accessToken = doc["accessToken"];
  //strcpy(cloudmqtt_server, json["cloudmqtt_server"]);
//  fn_allow = doc["fn_allow"].as<int>();
//  fs_allow = doc["fs_allow"].as<int>();
//  st_count = doc["st_count"].as<int>();
//  m_tm = doc["m_tm"].as<uint32_t>();
//  strcpy(srv,doc["server"]);
//  prt = doc["port"].as<int>();

  // Real world application would store these values in some variables for
  // later use.

  DebugSerial.print("Loaded serverName: ");
  //DebugSerial.println(serverName);
  DebugSerial.print("Loaded accessToken: ");
  //DebugSerial.println(accessToken);
  return true;
}

bool saveConfig() {
  
  File configFile = SPIFFS.open(filename, "w");
  if (!configFile) {
    DebugSerial.println("Failed to open config file for writing");
    
    return false;
  }
  
  config.files_allow = fn_allow;
  config.filesys_allow = fs_allow;
  config.string_count = st_count;
  config.min_tm = m_tm;
  config.port = prt;
  strlcpy(config.server,srv,sizeof(srv));         
  
  StaticJsonDocument<256> doc;
  
  doc["fn_allow"] = config.files_allow;
  doc["fs_allow"] = config.filesys_allow;
  doc["st_count"] = config.string_count;
  doc["m_tm"] = config.min_tm;
  //sprintf(s,"%d.%d.%d.%d",srv[0],srv[1],srv[2],srv[3]);
  doc["server"] = config.server;
  doc["port"] = config.port;
    
  // Serialize JSON to file
  if (serializeJson(doc, configFile) == 0) {
    DebugSerial.println(F("Failed to write to file"));
  }

  // Close the file
  configFile.close();
  return true;
}
