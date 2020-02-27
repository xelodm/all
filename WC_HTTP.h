/**
* Реализация HTTP сервера
* 
* Copyright (C) 2016 Алексей Шихарбеев
* http://samopal.pro
*/
#ifndef WS_HTTP_h
#define WS_HTTP_h
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include "FS.h"
#include "PZEM004Tv30.h"

extern PZEM004Tv30 pzem;
extern ESP8266WebServer server;
extern ESP8266HTTPUpdateServer httpUpdater;
extern bool isAP;
extern bool isConnect;
extern char m_file[];
extern bool     m_write;
extern uint16_t m_count;
extern uint32_t m_tm;
extern uint32_t tm;
extern float u1,i1,p1,e1,hz1,pf1;
//extern int n_file;
//extern int *f_dir;
//extern void Sort_f();
extern char srv[];
extern int srv_int[];
extern int prt;
extern int st_count;
extern int fn_allow;
extern int fs_allow;
extern  bool loadConfig();
extern  bool saveConfig();
#define DebugSerial Serial1


bool ConnectWiFi(void);
void ListWiFi(String &out);
void HTTP_begin(void);
void HTTP_loop();
void WiFi_begin(void);
void Time2Str(char *s,time_t t);
bool SetParamHTTP();
int  HTTP_isAuth();
void HTTP_handleRoot(void);
void HTTP_handleConfigNet(void);
void HTTP_handleConfig(void);
void HTTP_handleLogin(void);
void HTTP_handleReboot(void);
void HTTP_handleView(void);
void HTTP_handleDownload(void);
void HTTP_handleDefault(void);
void HTTP_handleLogo(void);
void HTTP_gotoLogin();
int  HTTP_checkAuth(char *user);
void HTTP_printInput(String &out,const char *label, const char *name, const char *value, int size, int len,bool is_pass=false);
void HTTP_printHeader(String &out,const char *title, uint16_t refresh=0);
void HTTP_printTail(String &out);
//void SetPwm();
void HTTP_device(void);
void HTTP_handleGraph(void);
void HTTP_handleData(void);
//void Sort_f();
void HTTP_get(void);
String SystemUptime();
void HTTP_handleUpload(void);
void handleFileView(void);
void handleFileUpload(void);
//Только для FSBrowsr.ino
String formatBytes_1(size_t bytes);
String getContentType_1(String filename);
bool handleFileRead_1(String path);
void handleFileUpload_1();
void handleFileDelete_1();
void handleFileCreate_1();
void handleFileList_1();
//Конец функций FSBrowser.ino


#endif
