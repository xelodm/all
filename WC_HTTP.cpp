/**
* Реализация HTTP сервера
* 
* Copyright (C) 2016 Алексей Шихарбеев
* http://samopal.pro
*/
#include "WC_HTTP.h"
#include "WC_EEPROM.h"
#include "logo.h"


//#include <fstream>  
//#include <iterator>
//using namespace std;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;


bool isAP = false;
bool isConnect  = false;

String authPass = "";
String HTTP_User = "";
String WiFi_List;
int    UID       = -1;
int n_file;
//int *f_dir = new int [n_file];
//char srv[] = "127.0.0.1"; // IP Adres (or name) of server to dump data to
//int prt = 80; //Port server send data
File fsUploadFile;              // a File object to temporarily store the received file
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
//void handleFileUpload();                // upload a new file to the SPIFFS
void handleFileDelete();                // delete file to the SPIFFS     
void handleFileCreate();                // create a new file to the SPIFFS
void handleFileListJ();                  // listing file to the SPIFFS
void handleFileList();    
bool handleUploadForm();
FS* filesystem = &SPIFFS;
//void fsort();


String cfg = "conf_pm";
String upl = "upload.html";
/**
 * Старт WiFi
 */
void WiFi_begin(void){ 
// Определяем список сетей
  ListWiFi(WiFi_List);
// Подключаемся к WiFi

  isAP = false;
  if( ! ConnectWiFi()  ){
      //strcpy(NetConfig.ESP_NAME,"PowerMeter_1.1");
      //strcpy(NetConfig.ESP_PASS,"admin");
      DebugSerial.printf("Start AP %s\n",NetConfig.ESP_NAME);
      WiFi.mode(WIFI_STA);
      WiFi.softAP(NetConfig.ESP_NAME, NetConfig.ESP_PASS);
      isAP = true;
      DebugSerial.printf("Open http://192.168.4.1 in your browser\n");
      isConnect = false; 
 }
  else {
// Получаем статический IP если нужно  
      if( NetConfig.IP[0] != 0 && NetConfig.IP[1] != 0 && NetConfig.IP[2] != 0 && NetConfig.IP[3] != 0 ){

         WiFi.config(NetConfig.IP,NetConfig.GW,NetConfig.MASK);
         DebugSerial.print("Open http://");
         DebugSerial.print(WiFi.localIP());
         DebugSerial.println(" in your browser");
      }
   }
// Запускаем MDNS
    MDNS.begin(NetConfig.ESP_NAME);
    DebugSerial.printf("Or by name: http://%s.local\n",NetConfig.ESP_NAME);
    isConnect = true; 
    

   
}

/**
 * Соединение с WiFi
 */
bool ConnectWiFi(void) {

  // Если WiFi не сконфигурирован
  if ( strcmp(NetConfig.AP_SSID, "none")==0 ) {
     DebugSerial.printf("WiFi is not config ...\n");
     return false;
  }

  WiFi.mode(WIFI_STA);

  // Пытаемся соединиться с точкой доступа
  DebugSerial.printf("\nConnecting to: %s/%s\n", NetConfig.AP_SSID, NetConfig.AP_PASS);
  WiFi.begin(NetConfig.AP_SSID, NetConfig.AP_PASS);
  delay(1000);

  // Максиммум N раз проверка соединения (12 секунд)
  for ( int j = 0; j < 15; j++ ) {
  if (WiFi.status() == WL_CONNECTED) {
      DebugSerial.print("\nWiFi connect: ");
      DebugSerial.print(WiFi.localIP());
      DebugSerial.print("/");
      DebugSerial.print(WiFi.subnetMask());
      DebugSerial.print("/");
      DebugSerial.println(WiFi.gatewayIP());
      return true;
    }
    delay(1000);
    DebugSerial.print(WiFi.status());
  }
  DebugSerial.printf("\nConnect WiFi failed ...\n");
  return false;
}

/**
 * Найти список WiFi сетей
 */
void ListWiFi(String &out){
  int n = WiFi.scanNetworks();
  if( n == 0 )out += "<p>Сетей WiFi не найдено";
  else {
     out = "<select name=\"AP_SSID\">\n";
     for (int i=0; i<n; i++){
       out += "    <option value=\"";
       out += WiFi.SSID(i);
       out += "\"";
       if( strcmp(WiFi.SSID(i).c_str(),NetConfig.AP_SSID) == 0 )out+=" selected";
       out += ">";
       out += WiFi.SSID(i);
       out += " [";
       out += WiFi.RSSI(i);
       out += "dB] ";
       out += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
       out += "</option>\n";
     }     
     out += "</select>\n";
  }
}

 

/**
 * Старт WEB сервера
 */
void HTTP_begin(void){
// Поднимаем WEB-сервер  
   server.on ( "/", HTTP_handleRoot );
   server.on ( "/cnet", HTTP_handleConfigNet );
   server.on ( "/config", HTTP_handleConfig );
   server.on ( "/default", HTTP_handleDefault );
   server.on ( "/login", HTTP_handleLogin );
   server.on ( "/logo", HTTP_handleLogo );
   //server.on ( "/view", HTTP_handleView );
   server.on ( "/view", handleFileView);
   server.on ( "/files", HTTP_GET, handleFileList);
  // server.on ( "/upload", HTTP_handleUpload );
   server.on ( "/upload", HTTP_GET, []() {
    if (!handleUploadForm()) {
      server.send(404, "text/plain", "FileNotFound");
    }
    });
   server.on ( "/upload", HTTP_POST, []() {  
             server.send(200, "text/plain", "");
             }, handleFileUpload);                        //first callback is called after the request has ended with all parsed arguments                          
   server.on ( "/del", HTTP_GET, handleFileDelete);                                 //second callback handles file uploads at that location
   server.on ( "/reboot", HTTP_handleReboot );
   server.on ( "/format", [](){
             SPIFFS.format(); server.sendHeader("Location", "/files",true);
             server.send(302, "text/plane","");} );

   server.onNotFound ( HTTP_handleDownload );
  //here the list of headers to be recorded
   const char * headerkeys[] = {"User-Agent","Cookie"} ;
   size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
   server.collectHeaders(headerkeys, headerkeyssize );
   httpUpdater.setup(&server,"/update");
   
//SERVER INIT только для FSBrowser.ino
  //list directory
  server.on("/list", HTTP_GET, handleFileList_1);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead_1("/edit.htm")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate_1);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete_1);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload_1);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead_1(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
//Конец для FSBrowser.ino
   
   server.begin();
   DebugSerial.printf( "HTTP server started ...\n" );
  
}


/**
 * Вывод в буфер одного поля формы
 */
void HTTP_printInput(String &out,const char *label, const char *name, const char *value, int size, int len, bool is_pass){
   char str[10];
   if( strlen( label ) > 0 ){
      out += "<td>";
      out += label;
      out += "</td>\n";
   }
   out += "<td><input name ='";
   out += name;
   out += "' value='";
   out += value;
   out += "' size=";
   sprintf(str,"%d",size);  
   out += str;
   out += " length=";    
   sprintf(str,"%d",len);  
   out += str;
   if( is_pass )out += " type='password'";
   out += "></td>\n";  
}

/**
 * Вывод заголовка файла HTML
 */
void HTTP_printHeader(String &out,const char *title, uint16_t refresh){
  out += "<html>\n<head>\n<meta charset=\"utf-8\" />\n";
  if( refresh ){
     char str[10];
     sprintf(str,"%d",refresh);
     out += "<meta http-equiv='refresh' content='";
     out +=str;
     out +="'/>\n"; 
  }
  out += "<title>";
  out += title;
  out += "</title>\n";
  out += "<style>body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>\n</head>\n";
  out += "<body>\n";

  //out += "<img src=/logo>\n";
  out += "<p><b>Device: ";
  out += NetConfig.ESP_NAME;

  if( UID < 0 )out +=" <a href=\"/login\">Authorisation</a>\n";
  else out +=" <a href=\"/login?DISCONNECT=YES\">Exit</a>\n";
  out += "</b></p>";
}   
 
/**
 * Выаод окнчания файла HTML
 */
void HTTP_printTail(String &out){
  out += "</body>\n</html>\n";
}

/**
 * Ввод имени и пароля
 */
void HTTP_handleLogin(){
  String msg;
// Считываем куки  
  if (server.hasHeader("Cookie")){   
//    DebugDebugSerial.print("Found cookie: ");
    String cookie = server.header("Cookie");
//    DebugDebugSerial.println(cookie);
  }
  if (server.hasArg("DISCONNECT")){
    DebugSerial.println("Disconnect");
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESP_PASS=\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }
  if ( server.hasArg("PASSWORD") ){
    String pass = server.arg("PASSWORD");
    
    if ( HTTP_checkAuth((char *)pass.c_str()) >=0 ){
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESP_PASS="+pass+"\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      DebugSerial.println("Login Success");
      return;
    }
  msg = "Bad password";
  DebugSerial.println("Login Failed");
  }
  String out = "";
  HTTP_printHeader(out,"Authorization");
  out += "<form action='/login' method='POST'>\
    <table border=0 width='600'>\
      <tr>\
        <td width='200'>Введите пароль:</td>\
        <td width='400'><input type='password' name='PASSWORD' placeholder='password' size='32' length='32'></td>\
      </tr>\
      <tr>\
        <td width='200'><input type='submit' name='SUBMIT' value='Ввод'></td>\
        <td width='400'>&nbsp</td>\
      </tr>\
    </table>\
    </form><b>" +msg +"</b><br>";
  HTTP_printTail(out);  
  server.send(200, "text/html", out);
}

/**
 * Обработчик событий WEB-сервера
 */
void HTTP_loop(void){
  server.handleClient();
}

/**
 * Перейти на страничку с авторизацией
 */
void HTTP_gotoLogin(){
  String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
  server.sendContent(header);
}
/**
 * Отображение логотипа
 */
void HTTP_handleLogo(void) {
  //server.send_P(200, PSTR("image/png"), logo, sizeof(logo));
  server.send ( 200, "text/html", "" );
}
  
//void Sort_f() {
//   n_file = 0;
//   int z=0;
//   int count = 1;
//   Dir dir_s = SPIFFS.openDir("/");
//   while (dir_s.next()) {count++;}
//   n_file = count;
//   int f_dir[count];
//   dir_s = SPIFFS.openDir("/");
//   while (dir_s.next()) {    
//      String fn = dir_s.fileName();
//      fn.replace(".txt","");
//      fn.replace("/","");
//      int k = fn.toInt();
//      //if (k>0){f_dir[z] = k; z++; count++}
//      f_dir[z] = k;
//      z++;
//      //count++;
//      
//   }
// 
//   int swap;
//   //int t=sizeof(f_dir)/sizeof(int);
//   int t = n_file;
//   for (byte i = 0; i <= t-1; i++) {
//     for (byte j = 0; j <= t-1; j++) {
//       if (f_dir[i] < f_dir[j]) {
//        swap = f_dir[i];
//        f_dir[i] = f_dir[j];
//        f_dir[j] = swap;
//       }
//     }
//   } 
////   size_t f_dir_size = sizeof(f_dir) / sizeof(f_dir[0]);
////   qsort(f_dir, f_dir_size, sizeof(f_dir[0]), ArrayCompare);
////f_dir[5]=787;
//}


/*
 * Оработчик главной страницы сервера
 */
void HTTP_handleRoot(void) {
  char str[20];
// Проверка авторизации  

  int gid = HTTP_isAuth();
  if ( gid < 0 ){
    HTTP_gotoLogin();
    return;
  } 
  
   if ( server.hasArg("mode") ){
       if( server.arg("mode") == "delete" ){
          if( server.arg("file") == "all" ){
             Dir dir = SPIFFS.openDir("/");
             while (dir.next()) {    
               if( dir.fileName().indexOf(".txt") > 0 )SPIFFS.remove(dir.fileName().c_str());
             }
             strcpy(m_file,"/00000001.txt");
          }
          else {
             SPIFFS.remove(server.arg("file").c_str());
            
          }   
          String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
          server.sendContent(header);
          return;
             
       }                    
    }
//// Переключение режима измерения
//   if ( server.hasArg("OK") ){
//       m_tm = atoi( server.arg("TM").c_str());
//       if( m_tm <=0 || m_tm > 300 )m_tm = 60;
//       m_tm*=1000;
//       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
//       server.sendContent(header);
//       return;
//   }
//   // Изменение количества строк в лог файле
//   if ( server.hasArg("OKI") ){
//       st_count = atoi( server.arg("SK").c_str());
//       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
//       server.sendContent(header);
//       return;
//   }
//   // Изменение количества строк в лог файле
//   if ( server.hasArg("OKF") ){
//       fn_allow = atoi( server.arg("FN").c_str());
//       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
//       server.sendContent(header);
//       return;
//   }
//   // Изменение ip сервера передачи данных
//   if ( server.hasArg("OKS") ){
//       //srv[] = atoi(server.arg("SERV").c_str());
//       strcpy(srv, server.arg("SERV").c_str());
//       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
//       server.sendContent(header);
//       return;
//   }
//   // Изменение порта сервера передачи данных
//   if ( server.hasArg("OKP") ){
//       prt = atoi( server.arg("PORT").c_str());
//       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
//       server.sendContent(header);
//       return;
//   }
// Переключение режима измерения
   if ( server.hasArg("STOP") ){
       m_write = false;
       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   }
// Переключение режима измерения
   if ( server.hasArg("START") ){
       m_write = true;
       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   }

// Сброс значений кВт
   if ( server.hasArg("RESET") ){
       pzem.resetEnergy();
       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   } 

//// Загрузка конфигурации из файла конфигурации
//   if ( server.hasArg("LOAD") ){
//       loadConfig();
//       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
//       server.sendContent(header);
//       return;
//   } 
//
//// Запись переменных в файл конфигурации
//   if ( server.hasArg("SAVE") ){
//       saveConfig();
//       String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
//       server.sendContent(header);
//       return;
//   } 
  
  String out = "";

  HTTP_printHeader(out,"PowerMeter_v1.0",10);
/*
  out += "<html>\n<head>\n<meta charset=\"utf-8\" />\n\
  <title>PowerMeter v1.0</title>\n\
  <style>body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>\n\
  </head>\n";
  out += "<body>\n";
*/  

   
   if( isAP ){
      out += "<p>Access point: ";
      out += NetConfig.ESP_NAME;
   }
   else {
      out += "<p>Connect to ";
      out += NetConfig.AP_SSID;
   }   
   out += "</p>\n";
   sprintf(str, "%02d:%02d", (int)( tm/3600 )%24, (int)( tm/60 )%60);
   out += "\n<h1>";
   out += str;
   out += "</h1>\n";
   out += "SystemUptime (";
   out += SystemUptime();
   out +=  ")\n";
   

   out += "<p><a href=\"/cnet\">Set Config</a></p><br>\n";

   out += "<table border=1>\n<tr>\n<td><h1>";
   sprintf(str,"%d.%02d V",(int)u1,((int)(u1*100))%100);
   out += str;
   out += "</h1></td><td><h1>"; 
   sprintf(str,"%d.%02d A",(int)i1,((int)(i1*100))%100);
   out += str;
   out += "</h1></td><td><h1>";
   sprintf(str, "%d.%02d W", (int)p1 ,((int)(p1*100))%100);
   out += str;
   out += "</h1></td>\n</tr>\n<tr>\n<td><h1>";    
   sprintf(str,"%d.%02d kW/h",(int)e1,((int)(e1*100))%100);
   out += str;
   out += "</h1></td><td><h1>";       
   sprintf(str,"%d Hz",(int)hz1); 
   out += str;
   out += "</h1></td><td><h1>"; 
   sprintf(str,"%d.%02d PF",(int)pf1,((int)(pf1*100))%100); 
   out += str;
   out += "</h1></td>\n</tr>\n</table>\n";    



   out +="<form action='/' method='GET'>";
//   out += "<p>Log timeout, s <input type='text' name='TM' value='";
//   sprintf(str,"%ld",m_tm/1000);
//   out += str;
//   out += "'><input type='submit' name='OK' value='Apply'></p>\n";
//   out += "<p>String count <input type='text' name='SK' value='";
//   sprintf(str,"%1d",st_count);
//   out += str;
//   out += "'><input type='submit' name='OKI' value='Apply'></p>\n";
//   out += "<p>Files count <input type='text' name='FN' value='";
//   sprintf(str,"%1d",fn_allow);
//   out += str;
//   out += "'><input type='submit' name='OKF' value='Apply'></p>\n";
//   
//   out += "<p>Data server <input type='text' name='SERV' value='";
//   //sprintf(str,"%1d",st_count);
//   out += String(srv);
//   out += "'><input type='submit' name='OKS' value='Apply'></p>\n";
//   out += "<p>Server port <input type='text' name='PORT' value='";
//   sprintf(str,"%1d",prt);
//   out += str;
//   out += "'><input type='submit' name='OKP' value='Apply'></p>\n";
   
   out += "<p>Logger: ";
   if( m_write )out += "<input type='submit' name='STOP' value='STOP'>";
   else out += "<input type='submit' name='START' value='START'></p>\n";
   out += "<p>kW/h: ";
   out += "<input type='submit' name='RESET' value='RESET'></p>\n";
//   out += "<p>Config load: ";
//   out += "<input type='submit' name='LOAD' value='LOAD'></p>";
//   out += "Config save: ";
//   out += "<input type='submit' name='SAVE' value='SAVE'>\n";
   
   out+= "</form>\n";

   out += "<p>Log file: ";
   out += m_file;
   out += "</p>\n";
//   out += "<p>String count: ";
//   out += st_count;
//   out += "</p>\n";
   
   out += "<ul>\n";

   n_file = 0;
   int z=0;
   int count = 0;
   Dir dir_s = SPIFFS.openDir("/");
   while (dir_s.next()) { if (dir_s.fileName().indexOf(".txt") > 0) {count++;}}
   n_file = count;
   int f_dir[count];
   dir_s = SPIFFS.openDir("/");
   while (dir_s.next()) {    
      String fn = dir_s.fileName();
      if (dir_s.fileName().indexOf(".txt") > 0) {
      fn.replace(".txt","");
      fn.replace("/","");
      int k = fn.toInt();
      //if (k>0){f_dir[z] = k; z++; count++}
      f_dir[z] = k;
      z++;
      //count++;  
      }
   }
   int swap;
   int t = n_file;
   for (byte i = 0; i <= t-1; i++) {
     for (byte j = 0; j <= t-1; j++) {
       if (f_dir[i] < f_dir[j]) {
        swap = f_dir[i];
        f_dir[i] = f_dir[j];
        f_dir[j] = swap;
       }
     }
   } 

   //Dir dir = SPIFFS.openDir("/");
  // while (dir.next()) {  
  for (byte i = 0; i < n_file; i++) { 
      char  f_link[16];
      sprintf(f_link,"/%08d.txt",f_dir[i]);
      String fileName = f_link;
      File myFile = SPIFFS.open(f_link, "r");
      String f_size = String(myFile.size());
      myFile.close();
      //if( fileName.indexOf(".txt") > 0 ){
         out += "<li><a href=\"/view?file=";
         out += fileName;
         out += "\">";
         out += fileName;
         out += " (";
         out += f_size;
         out += ") </a>  <a href=\"";
         out += fileName;
         out += "\">[Download]</a> <a href=\"/?mode=delete&file=";
         out += fileName;
         out += "\">[Delete]</a><br>\n";
     }  
     //}
   //}
   
   out += "</ul>\n<p><a href=\"/?mode=delete&file=all\">Delete all</a></p>";

  // out += "<br>\n";
  
   out += "Total Flash (";
   out +=  String(ESP.getFlashChipSize());
   out +=  ")";
   FSInfo fs_info;
   SPIFFS.info(fs_info);
   out += "Total FS spiffs (";
   out += String(fs_info.totalBytes);
   out +=  ")";
   out += "Used FS spiffs (";
   out += String(fs_info.usedBytes);
   out +=  ")";  
//   out += "<br>"; 
//   out += "fn_allow (";
//   out += String(fn_allow);
//   out +=  ")";
//   out += "fs_allow (";
//   out += String(fs_allow);
//   out +=  ")";
//   out += "srv (";
//   out += String(srv);
//   out +=  ")";
//   out += "prt (";
//   out += String(prt);
//   out +=  ")";  
//   out += "st_count (";
//   out += String(st_count);
//   out +=  ")";  
     
   //SPIFFS.gc();
  
  //for (byte z = 0; z <= (sizeof(f_dir)-1); z++){
    //sprintf(str,"%d",f_dir[z]);sizeof(f_dir)/sizeof(int)
    //out += 33333333;
    //out += "<br>\n";
    //out += n_file;
    //out += "<p>n_file</p><br>\n";
    //out += f_dir[4];
    //out += "<p>f_dir[4]</p><br>\n";
    
    //for (byte z = 0; z <n_file ; z++){out += f_dir[z];out += "<br>\n";};
 // }

 
   HTTP_printTail(out);
   
     
   server.send ( 200, "text/html", out );
   
   //HTTP_handleUpload();
}



/*
 * Оработчик страницы настройки сервера
 */
void HTTP_handleConfigNet(void) {
// Проверка прав администратора  
  char s[65];
  if ( HTTP_isAuth() != 0 ){
    HTTP_gotoLogin();
    return;
  } 

// Сохранение контроллера
  if ( server.hasArg("ESP_NAME") ){

     if( server.hasArg("ESP_NAME")     )strcpy(NetConfig.ESP_NAME,server.arg("ESP_NAME").c_str());
     if( server.hasArg("ESP_PASS")     )strcpy(NetConfig.ESP_PASS,server.arg("ESP_PASS").c_str());
     if( server.hasArg("AP_SSID")      )strcpy(NetConfig.AP_SSID,server.arg("AP_SSID").c_str());
     if( server.hasArg("AP_PASS")      )strcpy(NetConfig.AP_PASS,server.arg("AP_PASS").c_str());
     if( server.hasArg("IP1")          )NetConfig.IP[0] = atoi(server.arg("IP1").c_str());
     if( server.hasArg("IP2")          )NetConfig.IP[1] = atoi(server.arg("IP2").c_str());
     if( server.hasArg("IP3")          )NetConfig.IP[2] = atoi(server.arg("IP3").c_str());
     if( server.hasArg("IP4")          )NetConfig.IP[3] = atoi(server.arg("IP4").c_str());
     if( server.hasArg("MASK1")        )NetConfig.MASK[0] = atoi(server.arg("MASK1").c_str());
     if( server.hasArg("MASK2")        )NetConfig.MASK[1] = atoi(server.arg("MASK2").c_str());
     if( server.hasArg("MASK3")        )NetConfig.MASK[2] = atoi(server.arg("MASK3").c_str());
     if( server.hasArg("NASK4")        )NetConfig.MASK[3] = atoi(server.arg("MASK4").c_str());
     if( server.hasArg("GW1")          )NetConfig.GW[0] = atoi(server.arg("GW1").c_str());
     if( server.hasArg("GW2")          )NetConfig.GW[1] = atoi(server.arg("GW2").c_str());
     if( server.hasArg("GW3")          )NetConfig.GW[2] = atoi(server.arg("GW3").c_str());
     if( server.hasArg("GW4")          )NetConfig.GW[3] = atoi(server.arg("GW4").c_str());
     if( server.hasArg("WEB_PASS")   ){
         if( strcmp(server.arg("WEB_PASS").c_str(),"*") != 0 ){
             strcpy(NetConfig.WEB_PASS,server.arg("WEB_PASS").c_str());
         }
     }
     EC_save();
     
     String header = "HTTP/1.1 301 OK\r\nLocation: /cnet \r\nCache-Control: no-cache\r\n\r\n";
     server.sendContent(header);
     return;
  }

  String out = "";
  char str[10];
  HTTP_printHeader(out,"ConfigNet");
  out += "\
     <ul>\
     <li><a href=\"/\">Главная</a>\
     <li><a href=\"/files\">Files</a>\
     <li><a href=\"/config\">Config</a>\
     <li><a href=\"/default\">Set config default</a>\
     <li><a href=\"/update\">Firmware update</a>\
     <li><a href=\"/reboot\">Reset</a>\
     </ul>\n";

// Печатаем время в форму для корректировки

// Форма для настройки параметров
   out += "<h2>Config</h2>";
   out += "<h3>Access point params</h3>";
   out += "<form action='/cnet' method='POST'><table><tr>";
   HTTP_printInput(out,"AP Name:","ESP_NAME",NetConfig.ESP_NAME,32,32);
   out += "</tr><tr>";
   HTTP_printInput(out,"Password:","ESP_PASS",NetConfig.ESP_PASS,32,32,true);
   out += "</tr></table>";

   out += "<h3>WiFi params</h3>";
   out += "<table><tr>";
   out += "<td>WiFi Network</td><td>"+WiFi_List+"<td>";
//   HTTP_printInput(out,"Сеть WiFi:","AP_SSID",NetConfig.AP_SSID,32,32);
   out += "</tr><tr>";
   HTTP_printInput(out,"WiFi Password: &nbsp;&nbsp;&nbsp;","AP_PASS",NetConfig.AP_PASS,32,32,true);
   out += "</tr></table><table><tr>";
   sprintf(str,"%d",NetConfig.IP[0]); 
   HTTP_printInput(out,"Static IP:","IP1",str,3,3);
   sprintf(str,"%d",NetConfig.IP[1]); 
   HTTP_printInput(out,".","IP2",str,3,3);
   sprintf(str,"%d",NetConfig.IP[2]); 
   HTTP_printInput(out,".","IP3",str,3,3);
   sprintf(str,"%d",NetConfig.IP[3]); 
   HTTP_printInput(out,".","IP4",str,3,3);
   out += "</tr><tr>";
   sprintf(str,"%d",NetConfig.MASK[0]); 
   HTTP_printInput(out,"Netmask","MASK1",str,3,3);
   sprintf(str,"%d",NetConfig.MASK[1]); 
   HTTP_printInput(out,".","MASK2",str,3,3);
   sprintf(str,"%d",NetConfig.MASK[2]); 
   HTTP_printInput(out,".","MASK3",str,3,3);
   sprintf(str,"%d",NetConfig.MASK[3]); 
   HTTP_printInput(out,".","MASK4",str,3,3);
   out += "</tr><tr>";
   sprintf(str,"%d",NetConfig.GW[0]); 
   HTTP_printInput(out,"Gateway","GW1",str,3,3);
   sprintf(str,"%d",NetConfig.GW[1]); 
   HTTP_printInput(out,".","GW2",str,3,3);
   sprintf(str,"%d",NetConfig.GW[2]); 
   HTTP_printInput(out,".","GW3",str,3,3);
   sprintf(str,"%d",NetConfig.GW[3]); 
   HTTP_printInput(out,".","GW4",str,3,3);
   out += "</tr><table>";

   out += "<h3>Controller auth</h3>";
   out += "<table><tr>";
   HTTP_printInput(out,"Password:","WEB_PASS","*",32,32,true);
   out += "</tr><table>";
   
   out +="<input type='submit' name='SUBMIT_CONF' value='Save'>"; 
   out += "</form></body></html>";
   server.send ( 200, "text/html", out );
  
}        

/*
 * Оработчик страницы настроек
 */
void HTTP_handleConfig(void) {
// Проверка прав администратора  
  char s[65];
  if ( HTTP_isAuth() != 0 ){
    HTTP_gotoLogin();
    return;
  } 


  // Изменение периода записи в лог файл
   if ( server.hasArg("OK") ){
       m_tm = atoi( server.arg("TM").c_str());
       if( m_tm <=0 || m_tm > 300 )m_tm = 60;
       m_tm*=1000;

//       st_count = atoi( server.arg("SK").c_str());
//       fn_allow = atoi( server.arg("FN").c_str());
//       if( server.hasArg("IP1")          )srv_int[0] = atoi(server.arg("IP1").c_str());
//       if( server.hasArg("IP2")          )srv_int[1] = atoi(server.arg("IP2").c_str());
//       if( server.hasArg("IP3")          )srv_int[2] = atoi(server.arg("IP3").c_str());
//       if( server.hasArg("IP4")          )srv_int[3] = atoi(server.arg("IP4").c_str());
//       sprintf(srv,"%d.%d.%d.%d",srv_int[0],srv_int[1],srv_int[2],srv_int[3]);
//       prt = atoi( server.arg("PORT").c_str());
       
       String header = "HTTP/1.1 301 OK\r\nLocation: /config \r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;       
   }
   // Изменение количества строк в лог файле
   if ( server.hasArg("OKI") ){
       st_count = atoi( server.arg("SK").c_str());
       String header = "HTTP/1.1 301 OK\r\nLocation: /config \r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   }
   // Изменение количества лог файлов
   if ( server.hasArg("OKF") ){
       fn_allow = atoi( server.arg("FN").c_str());
       String header = "HTTP/1.1 301 OK\r\nLocation: /config \r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   }
   // Изменение ip сервера передачи данных
   if ( server.hasArg("OKS")){
//       if( server.hasArg("IP1")          )srv_int[0] = atoi(server.arg("IP1").c_str());
//       if( server.hasArg("IP2")          )srv_int[1] = atoi(server.arg("IP2").c_str());
//       if( server.hasArg("IP3")          )srv_int[2] = atoi(server.arg("IP3").c_str());
//       if( server.hasArg("IP4")          )srv_int[3] = atoi(server.arg("IP4").c_str());
//       sprintf(srv,"%d.%d.%d.%d",srv_int[0],srv_int[1],srv_int[2],srv_int[3]);
       //srv[] = atoi(server.arg("SERV").c_str());
       //srv = server.arg("SERV");
       strcpy(srv, server.arg("SERV").c_str());
       String header = "HTTP/1.1 301 OK\r\nLocation: /config \r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   }
   // Изменение порта сервера передачи данных
   if ( server.hasArg("OKP") ){
       prt = atoi( server.arg("PORT").c_str());
       String header = "HTTP/1.1 301 OK\r\nLocation: /config \r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   }

// Загрузка конфигурации из файла конфигурации
   if ( server.hasArg("LOAD") ){
       loadConfig();
       String header = "HTTP/1.1 301 OK\r\nLocation: /config \r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   } 

// Запись переменных в файл конфигурации
   if ( server.hasArg("SAVE") ){
       saveConfig();
       String header = "HTTP/1.1 301 OK\r\nLocation: /config \r\nCache-Control: no-cache\r\n\r\n";
       server.sendContent(header);
       return;
   } 
//// Сохранение контроллера
//  if ( server.hasArg("ESP_NAME") ){
//
//     if( server.hasArg("ESP_NAME")     )strcpy(NetConfig.ESP_NAME,server.arg("ESP_NAME").c_str());
//     if( server.hasArg("ESP_PASS")     )strcpy(NetConfig.ESP_PASS,server.arg("ESP_PASS").c_str());
//     if( server.hasArg("AP_SSID")      )strcpy(NetConfig.AP_SSID,server.arg("AP_SSID").c_str());
//     if( server.hasArg("AP_PASS")      )strcpy(NetConfig.AP_PASS,server.arg("AP_PASS").c_str());
//     if( server.hasArg("IP1")          )NetConfig.IP[0] = atoi(server.arg("IP1").c_str());
//     if( server.hasArg("IP2")          )NetConfig.IP[1] = atoi(server.arg("IP2").c_str());
//     if( server.hasArg("IP3")          )NetConfig.IP[2] = atoi(server.arg("IP3").c_str());
//     if( server.hasArg("IP4")          )NetConfig.IP[3] = atoi(server.arg("IP4").c_str());
//     if( server.hasArg("MASK1")        )NetConfig.MASK[0] = atoi(server.arg("MASK1").c_str());
//     if( server.hasArg("MASK2")        )NetConfig.MASK[1] = atoi(server.arg("MASK2").c_str());
//     if( server.hasArg("MASK3")        )NetConfig.MASK[2] = atoi(server.arg("MASK3").c_str());
//     if( server.hasArg("NASK4")        )NetConfig.MASK[3] = atoi(server.arg("MASK4").c_str());
//     if( server.hasArg("GW1")          )NetConfig.GW[0] = atoi(server.arg("GW1").c_str());
//     if( server.hasArg("GW2")          )NetConfig.GW[1] = atoi(server.arg("GW2").c_str());
//     if( server.hasArg("GW3")          )NetConfig.GW[2] = atoi(server.arg("GW3").c_str());
//     if( server.hasArg("GW4")          )NetConfig.GW[3] = atoi(server.arg("GW4").c_str());
//     if( server.hasArg("WEB_PASS")   ){
//         if( strcmp(server.arg("WEB_PASS").c_str(),"*") != 0 ){
//             strcpy(NetConfig.WEB_PASS,server.arg("WEB_PASS").c_str());
//         }
//     }
//     EC_save();
//     
//     String header = "HTTP/1.1 301 OK\r\nLocation: /config\r\nCache-Control: no-cache\r\n\r\n";
//     server.sendContent(header);
//     return;
//  }

  String out = "";
  char str[10];
  HTTP_printHeader(out,"Config");
  out += "\
     <ul>\
     <li><a href=\"/\">Главная</a>\
     <li><a href=\"/files\">Files</a>\
     <li><a href=\"/cnet\">Netconfig</a>\
     <li><a href=\"/default\">Set config default</a>\
     <li><a href=\"/update\">Firmware update</a>\
     <li><a href=\"/reboot\">Reset</a>\
     </ul>\n";


   out +="<form action='/config' method='GET'>";
   out += "<p>Log timeout, s <input type='text' name='TM' value='";
   sprintf(str,"%ld",m_tm/1000);
   out += str;
   out += "'><input type='submit' name='OK' value='Apply'></p>\n";
   out+= "</form>\n";
   out +="<form action='/config' method='GET'>";
   out += "<p>String count <input type='text' name='SK' value='";
   sprintf(str,"%1d",st_count);
   out += str;
   out += "'><input type='submit' name='OKI' value='Apply'></p>\n";
   out+= "</form>\n";
   out +="<form action='/config' method='GET'>";
   out += "<p>Files count <input type='text' name='FN' value='";
   sprintf(str,"%1d",fn_allow);
   out += str;
   out += "'><input type='submit' name='OKF' value='Apply'></p>\n";
   out+= "</form>\n";
   out +="<form action='/config' method='GET'>";
   out += "<p>Data server <input type='text' name='SERV' value='";
   //sprintf(str,"%1d",st_count);
   out += srv;
//     out += "<table><tr>";
//     sprintf(str,"%d",srv_int[0]); 
//     //sprintf(s,"%d%d%d",srv[0],srv[1],srv[2]);
//     HTTP_printInput(out,"Data Server IP:","IP1",str,3,3);
//     sprintf(str,"%d",srv_int[1]); 
//     HTTP_printInput(out,".","IP2",str,3,3);
//     sprintf(str,"%d",srv_int[2]); 
//     HTTP_printInput(out,".","IP3",str,3,3);
//     sprintf(str,"%d",srv_int[3]); 
//     HTTP_printInput(out,".","IP4",str,3,3);
//     out += "</tr></table>";
     out += "'><input type='submit' name='OKS' value='Apply'>\n";
   out+= "</form>\n";  
   //out += "<p>Data server <input type='text' name='SERV' value='";
   //sprintf(str,"%1d",st_count);
   //out += srv;
   //out += "'><input type='submit' name='OKS' value='Apply'></p>\n";
   out +="<form action='/config' method='GET'>";
   out += "<p>Server port <input type='text' name='PORT' value='";
   sprintf(str,"%1d",prt);
   out += str;
   out += "'><input type='submit' name='OKP' value='Apply'></p>\n";
   out+= "</form>\n"; 
   out +="<form action='/config' method='GET'>";
   out += "<p>Config load: ";
   out += "<input type='submit' name='LOAD' value='LOAD'></p>";
   out+= "</form>\n";
   out +="<form action='/config' method='GET'>";
   out += "Config save: ";
   out += "<input type='submit' name='SAVE' value='SAVE'>\n";
   
   out+= "</form>\n";
//// Печатаем время в форму для корректировки
//
//// Форма для настройки параметров
//   out += "<h2>Config</h2>";
//   out += "<h3>Access point params</h3>";
//   out += "<form action='/config' method='POST'><table><tr>";
//   HTTP_printInput(out,"AP Name:","ESP_NAME",NetConfig.ESP_NAME,32,32);
//   out += "</tr><tr>";
//   HTTP_printInput(out,"Password:","ESP_PASS",NetConfig.ESP_PASS,32,32,true);
//   out += "</tr></table>";
//
//   out += "<h3>WiFi params</h3>";
//   out += "<table><tr>";
//   out += "<td>WiFi Network</td><td>"+WiFi_List+"<td>";
////   HTTP_printInput(out,"Сеть WiFi:","AP_SSID",NetConfig.AP_SSID,32,32);
//   out += "</tr><tr>";
//   HTTP_printInput(out,"WiFi Password: &nbsp;&nbsp;&nbsp;","AP_PASS",NetConfig.AP_PASS,32,32,true);
//   out += "</tr></table><table><tr>";
//   sprintf(str,"%d",NetConfig.IP[0]); 
//   HTTP_printInput(out,"Static IP:","IP1",str,3,3);
//   sprintf(str,"%d",NetConfig.IP[1]); 
//   HTTP_printInput(out,".","IP2",str,3,3);
//   sprintf(str,"%d",NetConfig.IP[2]); 
//   HTTP_printInput(out,".","IP3",str,3,3);
//   sprintf(str,"%d",NetConfig.IP[3]); 
//   HTTP_printInput(out,".","IP4",str,3,3);
//   out += "</tr><tr>";
//   sprintf(str,"%d",NetConfig.MASK[0]); 
//   HTTP_printInput(out,"Netmask","MASK1",str,3,3);
//   sprintf(str,"%d",NetConfig.MASK[1]); 
//   HTTP_printInput(out,".","MASK2",str,3,3);
//   sprintf(str,"%d",NetConfig.MASK[2]); 
//   HTTP_printInput(out,".","MASK3",str,3,3);
//   sprintf(str,"%d",NetConfig.MASK[3]); 
//   HTTP_printInput(out,".","MASK4",str,3,3);
//   out += "</tr><tr>";
//   sprintf(str,"%d",NetConfig.GW[0]); 
//   HTTP_printInput(out,"Gateway","GW1",str,3,3);
//   sprintf(str,"%d",NetConfig.GW[1]); 
//   HTTP_printInput(out,".","GW2",str,3,3);
//   sprintf(str,"%d",NetConfig.GW[2]); 
//   HTTP_printInput(out,".","GW3",str,3,3);
//   sprintf(str,"%d",NetConfig.GW[3]); 
//   HTTP_printInput(out,".","GW4",str,3,3);
//   out += "</tr><table>";
//
//   out += "<h3>Controller auth</h3>";
//   out += "<table><tr>";
//   HTTP_printInput(out,"Password:","WEB_PASS","*",32,32,true);
//   out += "</tr><table>";
//   
//   out +="<input type='submit' name='SUBMIT_CONF' value='Save'>"; 
//   out += "</form></body></html>";

   out += "<br>"; 
   out += "fn_allow (";
   out += String(fn_allow);
   out +=  ")";
   out += "fs_allow (";
   out += String(fs_allow);
   out +=  ")";
//   out += "srv (";
//   sprintf(str,"%d.%d.%d.%d",srv[0],srv[1],srv[2],srv[3]);
//   out += str;
//   out +=  ")";
   out += "srv_asis (";
   out += srv;
   out +=  ")";
   out += "srv[]_asis (";
   out += srv[0];
   //out +=  "";
   out += srv[1];
   //out +=  "";
   out += srv[2];
   //out +=  ".";
   out += srv[3];
   //out +=  "";
   out += srv[4];
   //out +=  "";
   out += srv[5];
   //out +=  ".";
   out += srv[6];
   //out +=  "";
   out += srv[7];
   //out +=  ".";
   out += srv[8];
   //out +=  ".";
   out += srv[9];
   //out +=  "";
   out += srv[10];
   //out +=  "";
   out += srv[11];
   //out +=  "";
   out += srv[12];
   out +=  "";
   out += srv[13];
   out +=  "";
   out += srv[14];
   //out +=  "";
   out += srv[15];
   //out +=  "";
   out +=  ")";
   out += "prt (";
   out += String(prt);
   out +=  ")";  
   out += "st_count (";
   out += st_count;
   out +=  ")";  

   HTTP_printTail(out);
   server.send ( 200, "text/html", out );
  
}        

/*
 * Сброс настрое по умолчанию
 */
void HTTP_handleDefault(void) {
// Проверка прав администратора  
  if ( HTTP_isAuth() != 0 ){
    HTTP_gotoLogin();
    return;
  } 

  EC_default();
  HTTP_handleConfig();  
}



/**
 * Проверка авторизации
 */
int HTTP_isAuth(){
//  DebugDebugSerial.print("AUTH ");
  if (server.hasHeader("Cookie")){   
//    DebugDebugSerial.print("Found cookie: ");
    String cookie = server.header("Cookie");
//    DebugDebugSerial.print(cookie);
 
    if (cookie.indexOf("ESP_PASS=") != -1) {
      authPass = cookie.substring(cookie.indexOf("ESP_PASS=")+9);       
      return HTTP_checkAuth((char *)authPass.c_str());
    }
  }
  return -1;  
}


/**
 * Функция проверки пароля
 * возвращает 0 - админ, 1 - оператор, -1 - Не авторизован
 */
int  HTTP_checkAuth(char *pass){
   char s[32];
   strcpy(s,pass);
   if( strcmp(s,NetConfig.WEB_PASS) == 0 ){ 
       UID = 0;
       HTTP_User = "Admin";
   }
    else {
       UID = -1;
       HTTP_User = "User";
   }
   return UID;
}


/*
 * Перезагрузка часов
 */
void HTTP_handleReboot(void) {
// Проверка прав администратора  
  if ( HTTP_isAuth() != 0 ){
    HTTP_gotoLogin();
    return;
  } 


  String out = "";

  out = 
"<html>\
  <head>\
    <meta charset='utf-8' />\
    <meta http-equiv='refresh' content='10;URL=/'>\
    <title>ESP8266 sensor 1</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Controller reset... </h1>\
    </body>\
</html>";
   server.send ( 200, "text/html", out );
   ESP.reset();  
  
}

/*
 * Оработчик просмотра одного файла
 */
void HTTP_handleView(void) {
// Проверка авторизации  

  int gid = HTTP_isAuth();
  if ( gid < 0 ){
    HTTP_gotoLogin();
    return;
  } 
   String file = server.arg("file");
   if( SPIFFS.exists(file) ){
      File f = SPIFFS.open(file, "r");
      size_t sent = server.streamFile(f, "text/plain");
      f.close();
   }
   else {
      server.send(404, "text/plain", "FileNotFound");
   }
}

/*
 * Оработчик скачивания одного файла
 */
void HTTP_handleDownload(void) {
// Проверка авторизации  

  int gid = HTTP_isAuth();
  if ( gid < 0 ){
    HTTP_gotoLogin();
    return;
  } 
   String file = server.uri();
   if( SPIFFS.exists(file) ){
      File f = SPIFFS.open(file, "r");
      size_t sent = server.streamFile(f, "application/octet-stream");
      f.close();
   }
   else {
      server.send(404, "text/plain", "FileNotFound");
   }
}

//Загрузка файлов в SPIFFS
void HTTP_handleUpload(void){
   

       
  //SERVER INIT
  //list directory
  //server.on("/list", HTTP_GET, handleFileList);
  //view file
  //server.on("/view2", HTTP_GET, handleFileRead(server.uri()));
  //load upload
  //server.on("/upload", HTTP_GET, []() {
    //if (!handleUploadForm()) {
     // server.send(404, "text/plain", "FileNotFound");
   // }
  //});
   //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  //server.on("/upload", HTTP_POST, []() {
   // server.send(200, "text/plain", "");
 // }, handleFileUpload);
  //create file
  //server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  //server.on("/edit", HTTP_DELETE, handleFileDelete);
  //server.on("/del", HTTP_GET, handleFileDelete);
 

  //called when the url is not defined here
  //use it to load content from SPIFFS
  //server.onNotFound([]() {
   // if (!handleFileRead(server.uri())) {
    //  server.send(404, "text/plain", "FileNotFound");
   // }
 // });
     
  

}

//Создание формы загрузки файла
bool handleUploadForm(){
   //File fi; //<form  action='/edit'
       //if( !SPIFFS.exists("/upload.html") ){
        String out = "";
        out += 
         "<html>\
          <head>\
          <meta charset='utf-8' />\
          <title>Upload files</title>\
          </head>\
          <body>\
           <form method='post' enctype='multipart/form-data'>\          
             <input type='file' name='name'>\
             <input class='button' type='submit' value='Upload'>\
           </form>\
          </body>\
          </html>";
          
          server.send ( 200, "text/html", out );
          return true;
          //    File fi; //<form  action='/edit'
//       //if( !SPIFFS.exists("/upload.html") ){
//        String out = "";
//        out += 
//         "<html>\
//          <head>\
//          <meta charset='utf-8' />\
//          <title>Upload files</title>\
//          </head>\
//          <body>";
//           out += "<form method='post' enctype='multipart/form-data'>";
//           out += 
//            "<input type='file' name='name'>\
//             <input class='button' type='submit' value='Upload'>\
//             </form>\
//          </body>\
//          </html>";
//        fi = SPIFFS.open("/conf/edit.htm", "w");
//        fi.print(out);
//        fi.close();
//     //  }
        //fi = SPIFFS.open("/conf/edit.htm", "w");
        //fi.print(out);
        //fi.close();
     //  }
}

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}
//Определение типа файла
String getContentType(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}

//Чтение файла
bool handleFileRead(String path) {
  
  DebugSerial.println("handleFileRead: " + path);
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (filesystem->exists(pathWithGz) || filesystem->exists(path)) {
    if (filesystem->exists(pathWithGz)) {
      path += ".gz";
    }
    File file = filesystem->open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

//Чтение файла и вывод в браузер
void handleFileView(void){
  int gid = HTTP_isAuth();
  if ( gid < 0 ){
    HTTP_gotoLogin();
    return;
  }
  String path = server.arg("file");
  if ( path > "") {
    handleFileRead(path);
  }else {server.send(404, "text/plain", "FileNotFound");}    
  
}

//Загрузка файла в SPIFFS
void handleFileUpload(void) {
  if (server.uri() != "/upload" ) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (filename == "" || filename == " " || filename == "/") {return server.send(500, "text/plain", "BAD FILE NAME");}
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    DebugSerial.print("handleFileUpload Name: "); DebugSerial.println(filename);
    fsUploadFile = filesystem->open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DebugSerial.print("handleFileUpload Data: "); DebugSerial.println(upload.currentSize);
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
      DebugSerial.print("handleFileUpload Size: "); DebugSerial.println(upload.totalSize);
      server.sendHeader("Location", "/files",true);
      server.send(302, "text/plane","");
      //server.send(200, "text/plain", "File success upload");
      //server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      //server.sendHeader("Location","/list?dir=/");
      //server.send(303);
      
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
      }
    }
}

//Удаление файла из SPIFFS
void handleFileDelete() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg("dir");
  DebugSerial.println("handleFileDelete: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (!filesystem->exists(path)) {
    return server.send(404, "text/plain", "FileNotFound");
  }
  filesystem->remove(path);
  server.sendHeader("Location", "/files",true);
  server.send(302, "text/plane","");
  //server.send(200, "text/plain", "File deleted");
  path = String();
}

//Создание файла в SPIFFS
void handleFileCreate() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  DebugSerial.println("handleFileCreate: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (filesystem->exists(path)) {
    return server.send(500, "text/plain", "FILE EXISTS");
  }
  File file = filesystem->open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, "text/plain", "CREATE FAILED");
  }
  server.send(200, "text/plain", "");
  path = String();
}

//Создание вывод списка файлов из SPIFFS в формате json
void handleFileListJ() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  DebugSerial.println("handleFileList: " + path);
  Dir dir = filesystem->openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") {
      output += ',';
    }
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    if (entry.name()[0] == '/') {
      output += &(entry.name()[1]);
    } else {
      output += entry.name();
    }
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

//Создание и вывод списка файлов из SPIFFS (аналог DIR,LS)
void handleFileList() {
  int gid = HTTP_isAuth();
  if ( gid < 0 ){
    HTTP_gotoLogin();
    return;
  }
  //if (!server.hasArg("dir")) {
   // server.send(500, "text/plain", "BAD ARGS");
    //return;
 // }
  String path = server.arg("dir");
  if (!server.hasArg("dir")) {
    path = "/";
  }
  String type = server.arg("type");
  if (type == "txt" ){type = ".txt";}
  
  DebugSerial.println("handleFileList: " + path);
  Dir dir = filesystem->openDir(path);
  path = String();

  String output = "";
  String fileName = "";
  output += "<meta charset='utf-8'>";
  output += "<style>body { background-color: rgba(220, 220, 220, 0.5); font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>\n";
  output += "<pre><a href=\"/\">Главная</a>  ";
  output += "<a href=\"/files?type=txt\">txt</a>  ";
  output += "<a href=\"/files?type=txtoff\">txtoff</a>  ";
  output += "<a href=\"/files\">all</a>  ";
  output += "<a href=\"/upload\">upload</a>  ";
  output += "<a href=\"/format\">format</a></pre><br>";
  while (dir.next()) {
    File entry = dir.openFile("r");
    
    if ( type == "txtoff" && dir.fileName().indexOf(".txt") == -1 ){
        if (entry.name()[0] == '/') {
       fileName = &(entry.name()[1]);
    } else {  
      fileName = entry.name(); 
      }
    String f_size = String(entry.size()); 
    entry.close();  
      
      output += "<li><a href=\"/view?file=/";
      output += fileName;
      output += "\">";
      output += fileName;
      output += " (";
      output += f_size;
      output += ") </a>  <a href=\"";
      output += fileName;
      output += "\">[Download]</a>  <a href=\"/del?dir=/";
      output += fileName;
      output += "\">[Delete]</a><br>\n";  
    }
    
    if ( type != "" && type != "txtoff" && dir.fileName().indexOf(type) != -1 ) {
    if (entry.name()[0] == '/') {
      //if (dir.fileName().indexOf(type) != -1){
      fileName = &(entry.name()[1]);
     // }
    } else { 
      //if (dir.fileName().indexOf(type) != -1){ 
      fileName = entry.name(); 
     // }
      }
    String f_size = String(entry.size()); 
    entry.close();  
      
      output += "<li><a href=/view?file=/";
      output += fileName;
      output += ">";
      output += fileName;
      output += " (";
      output += f_size;
      output += ") </a>  <a href=";
      output += fileName;
      output += ">[Download]</a>  <a href=/del?dir=/";
      output += fileName;
      output += ">[Delete]</a><br>\n";  
    }
    if (type == "") {
           if (entry.name()[0] == '/') {
           fileName = &(entry.name()[1]);
           } else {
             fileName = entry.name();
           }
    String f_size = String(entry.size()); 
    entry.close();  
      
      output += "<li><a href=/view?file=/";
      output += fileName;
      output += ">";
      output += fileName;
      output += " (";
      output += f_size;
      output += ") </a>  <a href=";
      output += fileName;
      output += ">[Download]</a>  <a href=/del?dir=/";
      output += fileName;
      output += ">[Delete]</a><br>\n";           
   }
       
   // } 
//    String f_size = String(entry.size()); 
//    entry.close();  
//      
//      output += "<li><a href=/view?file=/";
//      output += fileName;
//      output += ">";
//      output += fileName;
//      output += " (";
//      output += f_size;
//      output += ") </a>  <a href=";
//      output += fileName;
//      output += ">[Download]</a>  <a href=/del?dir=/";
//      output += fileName;
//      output += ">[Delete]</a><br>\n";
   // output += "\"}";
    
  }

  //output += "]";
  server.send(200, "text/html", output);
//  fsort();
}

//void fsort(){
//  ifstream in("/infile.txt");
//    vector<string> vs;
//    string s;
//    while(getline(in,s)) vs.push_back(s);
//    sort(vs.begin(),vs.end());
//    ofstream on("/onfile.txt");
//    copy(vs.begin(),vs.end(),ostream_iterator<string>(on,"\n"));  
//}

//Отправка данных на сервер с php
void HTTP_get(void) {
     char str[20];
     WiFiClient client; 
     if (client.connect(srv, prt)) {
     //Serial.println("-> Connected");
     // Make a HTTP request:
     client.print( "GET /add_data.php?");
     client.print("unix_tm=");
     sprintf(str,"%ld",tm);
     client.print(str);
     client.print( "&&" );
     client.print("serial=");
     client.print( "pzem-004t_1" );
     client.print( "&&" );
     client.print("voltage=");
     sprintf(str,"%d.%02d",(int)u1,((int)(u1*100))%100);
     client.print(str);
     //str [20]= "";
     client.print( "&&" );
     client.print("amper=");
     sprintf(str,"%d.%02d",(int)i1,((int)(i1*100))%100);
     client.print(str); 
     client.print( "&&" );
     client.print("watt=");
     sprintf(str, "%d.%02d",(int)p1 ,((int)(p1*100))%100);
     client.print(str); 
     client.print( "&&" );
     client.print("kwatt=");
     sprintf(str,"%d.%02d",(int)e1,((int)(e1*100))%100);
     client.print(str); 
     client.print( "&&" );
     client.print("hz=");
     sprintf(str,"%d.%02d",(int)hz1,((int)(hz1*100))%100);
     client.print(str); 
     client.print( "&&" );
     client.print("pf=");
     sprintf(str,"%d.%02d",(int)pf1,((int)(pf1*100))%100);
     client.print(str);
     client.println( " HTTP/1.1");
     client.print( "Host: " );
     client.println(srv);
     client.println( "Connection: close" );
     client.println(); 
     client.println(); 
     client.stop();

  }
}  

//Вычисляем uptime работы
String  SystemUptime() {
     char timestring[25];
     int mi,hh,dddd;
     long ss = millis();
     mi = int((ss / (1000 * 60)) % 60);
     hh = int((ss / (1000 * 60 * 60)) % 24);
     dddd = int((ss / (1000 * 60 * 60 * 24)) % 365);
     sprintf(timestring,"%d days %02d:%02d", dddd, hh, mi);
     return (timestring);
}

//Функции для редактирования файлов FSBrowser.ino

//format bytes
String formatBytes_1(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType_1(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  } else if (filename.endsWith(".json")) {
    return "application/json";
  }
  return "text/plain";
}

bool handleFileRead_1(String path) {
  DebugSerial.println("handleFileRead: " + path);
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType_1(path);
  String pathWithGz = path + ".gz";
  if (filesystem->exists(pathWithGz) || filesystem->exists(path)) {
    if (filesystem->exists(pathWithGz)) {
      path += ".gz";
    }
    File file = filesystem->open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload_1() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    DebugSerial.print("handleFileUpload Name: "); DebugSerial.println(filename);
    fsUploadFile = filesystem->open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DebugSerial.print("handleFileUpload Data: "); DebugSerial.println(upload.currentSize);
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    DebugSerial.print("handleFileUpload Size: "); DebugSerial.println(upload.totalSize);
  }
}

void handleFileDelete_1() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  DebugSerial.println("handleFileDelete: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (!filesystem->exists(path)) {
    return server.send(404, "text/plain", "FileNotFound");
  }
  filesystem->remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate_1() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  DebugSerial.println("handleFileCreate: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (filesystem->exists(path)) {
    return server.send(500, "text/plain", "FILE EXISTS");
  }
  File file = filesystem->open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, "text/plain", "CREATE FAILED");
  }
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList_1() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  DebugSerial.println("handleFileList: " + path);
  Dir dir = filesystem->openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") {
      output += ',';
    }
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    if (entry.name()[0] == '/') {
      output += &(entry.name()[1]);
    } else {
      output += entry.name();
    }
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}
//Конец функций FSBrowser.ino
