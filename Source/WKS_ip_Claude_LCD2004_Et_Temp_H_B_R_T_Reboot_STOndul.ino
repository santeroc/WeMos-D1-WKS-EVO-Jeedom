#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include <TimeLib.h>
#include <time.h>        // https://github.com/PaulStoffregen/Time
#include <Timezone.h>    // https://github.com/JChristensen/Timezone
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>

#include <DHT.h>

#include<LiquidCrystal_I2C.h>

  // Paramètres WIFI...
  static const String monSSID = "MON-SSID";
  static const String monPass = "MON-PASSWORD-WIFI";
  static const String HostName = "WeMos_UPS";

  static const String WeMos_UPS_Version = "7.8";

  //Paramètres NTP...
  //  String months[12]={"Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", "Août", "Septembre", "Octobre", "Novembre", "Decembre"};
  String DateFR, TimeFR;
  int NTPUpdateResult; // Résultat Requète NTP : 0 Tant Qu'une requète NTP n'a pas aboutit, 1 Lorsqu'on a obtenu une requète NTP. 
  int NTPLastHourUpdate; // Heure de la dernière MAJ réussi de la requète NTP (MAJ demandé toutes les heures).
  int NTPLastDayUpdate; // Jour du dernier envoie de la dernière MAJ de la date et de l'heure de l'onduleur.
  int GTMOffset = 0; // SET TO UTC TIME + 1 Hour.
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", GTMOffset*60*60, 60*60*1000); // NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
  // Obtention du NTP depuis une source locale en 192.168.0.10 :
  //NTPClient timeClient(ntpUDP, "192.168.0.10", GTMOffset*60*60, 60*60*1000); // NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
  // Central European Time (Frankfurt, Paris)
  TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
  TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
  Timezone CE(CEST, CET);
  
  static tm getDateTimeByParams(long time){
    struct tm *newtime;
    const time_t tim = time;
    newtime = localtime(&tim);
    return *newtime;
  }
  
  // Paramètres LCD...
  LiquidCrystal_I2C lcd(0x27,20,4);
  static int QPOS;
   
  // Définition pour afficher un symbole degré sur le LCD...
  byte degre[8] = { // déclaration d’un tableau de 8 octets
        B00111, // définition de chaque octet au format binaire
        B00101, // 1 pour pixel affiché – 0 pour pixel éteint
        B00111, // les 3 bits de poids forts ne sont pas écrits car inutiles
        B00000,
        B00000,
        B00000,
        B00000,
      };
  static int caractereDegre;

  // Paramètre Du Relais Ventilos...
  static const float TempStartWKS = 32.50;  // Température de déclenchement des ventilos pour le WKS. 31.00
  static const float TempStopWKS = 29.50;   // Température d'arrêt des ventilos pour le WKS si ventilos démarrés. 27.00
  static const float TempStartBat = 25.50;  // Température de déclenchement des ventilos pour les batteries.
  static const float TempStopBat = 22.50;   // Température d'arrêt des ventilos pour les batteries si ventilos démarrés.
  static const int HStart = 2;              // Heure de lancement des ventilos forcée.
  static const int HStop = 7;               // Heure d'arrêt des ventilos forcée (heure comprise donc 7 signifie arrêt à 8H).
  static const int RelayFan = D7;           // Digital pin connected to Relay
  static int RelayFanStatus;

  // Paramètres De La Sonde De Température DHT...
  #define DHTPIN_H D4     // Digital pin connected to the DHT sensor (DHT Onduleur)
  #define DHTPIN_B D3     // Digital pin connected to the DHT sensor (DHT Batteries)

  // Uncomment the type of sensor in use:
  //#define DHTTYPE    DHT11     // DHT 11
  #define DHTTYPE    DHT22     // DHT 22 (AM2302)
  //#define DHTTYPE    DHT21     // DHT 21 (AM2301)

  DHT dht_H(DHTPIN_H, DHTTYPE);
  DHT dht_B(DHTPIN_B, DHTTYPE);

  // Utiliser isnan() permet de vérifier si la valeur n'est pas un nombre.
  float newT_H;   // Température Lu / isnan(newT) pour vérifier la valeur.
  float newH_H;   // Humidité Lu / isnan(newH) pour vérifier la valeur.

  float newT_B;   // Température Lu / isnan(newT) pour vérifier la valeur.
  float newH_B;   // Humidité Lu / isnan(newH) pour vérifier la valeur.

//* Time Function *

static String getDateTimeStringByParams(tm *newtime, char* pattern = (char *)"%d/%m/%Y %H:%M:%S"){
    char buffer[30];
    strftime(buffer, 30, pattern, newtime);
    return buffer;
}

static String getEpochStringByParams(long time, char* pattern = (char *)"%d/%m/%Y %H:%M:%S"){
//    struct tm *newtime;
    tm newtime;
    newtime = getDateTimeByParams(time);
    return getDateTimeStringByParams(&newtime, pattern);
}

void UpdateRTCWithNTP() {
  if (timeClient.update()){
     unsigned long epoch = timeClient.getEpochTime();
     setTime(epoch);
     NTPUpdateResult = 1; // 1 Requète NTP a aboutit.
     NTPLastHourUpdate = getEpochStringByParams(CE.toLocal(now()),(char*)"%H").toInt();
     Serial.println("Horloge RTC Mise A Jour Avec Temps NTP @ " + String(NTPLastHourUpdate) + " H.");
  } else{
         Serial.println("Echec De La Requète NTP." );
        }
}

void ReadNTPTime() {
  DateFR = getEpochStringByParams(CE.toLocal(now()),(char*)"%d/%m/%Y"); //"%d/%m/%Y %H:%M:%S"
  TimeFR = getEpochStringByParams(CE.toLocal(now()),(char*)"%H:%M"); //"%d/%m/%Y %H:%M:%S"
}

//* End Of Time Function *

// * Temp Function *

  void FanCTRL()
  {
    String HR = getEpochStringByParams(CE.toLocal(now()),(char*)"%H"); //"%d/%m/%Y %H:%M:%S"
    int HH = HR.toInt();
    
    if (HH >= HStart && HH <= HStop)
    {
      pinMode(RelayFan , INPUT); // Démarre ventilation sur NC.
      RelayFanStatus = 2;
      Serial.println("Ventilos Démarrés en mode horraire.");
    }
    else
    { // ***********************************************************************************************
     if (newT_B < 0 || newT_H < 0)
     {
      // Absence de données des sondes on met les ventilos en service... 
      pinMode(RelayFan , INPUT); // Démarre ventilation sur NC.
      RelayFanStatus = 1;
      Serial.println("Ventilos Démarrés.");
     }
     else
     {
      if (newT_B >= TempStartBat || newT_H >= TempStartWKS) 
      {
       // Température suppérieure ou égale à au moins un TempStart : On démarre les ventilos...
       pinMode(RelayFan , INPUT); // Démarre ventilation sur NC.
       RelayFanStatus = 1;
       Serial.println("Ventilos Démarrés.");
      }
      else 
      { 
       // Température strictement inférieure aux TempStart on arrête les ventilos si les températures sont inférieures ou égales aux seuils TempStopForBat ET TempStopWKS...
       if (newT_B <= TempStopBat && newT_H <= TempStopWKS)
       {
        pinMode(RelayFan , OUTPUT); // Arrêt ventilation sur NC.
        RelayFanStatus = 0;
        Serial.println("Ventilos Arrêtés.");
       }
      }
     }
    } // ***********************************************************************************************
  }
  
  void ReadTempHum()
  {
    newT_H = dht_H.readTemperature();
    newT_B = dht_B.readTemperature();
    
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float newT = dht.readTemperature(true);
    // if temperature read failed, don't change t value
    //lcd.setCursor(0,2);
    if (isnan(newT_H)) // isnan() permet de vérifier si la valeur n'est pas un nombre.
    { 
      newT_H = -1;
      Serial.println("Failed to read from DHT_H sensor!");
    }
    else 
    {
     Serial.print("Reading Temp. H : ");
     Serial.println(newT_H);
    }

    if (isnan(newT_B)) // isnan() permet de vérifier si la valeur n'est pas un nombre.
    { 
      newT_B = -1;
      Serial.println("Failed to read from DHT_B sensor!");
    }
    else 
    {
     Serial.print("Reading Temp. B : ");
     Serial.println(newT_B);
    }
    
    // Read Humidity
    newH_H = dht_H.readHumidity();
    newH_B = dht_B.readHumidity();
    // if humidity read failed, don't change h value 
    if (isnan(newH_H)) 
    { 
      newH_H = -1;
      Serial.println("Failed to read from DHT_H sensor!");
    }
    else 
    {
     Serial.print("Reading Humidity H : ");
     Serial.println(newH_H);
    }

    if (isnan(newH_B)) 
    { 
      newH_B = -1;
      Serial.println("Failed to read from DHT_B sensor!");
    }
    else 
    {
     Serial.print("Reading Humidity B : ");
     Serial.println(newH_H);
    }

    FanCTRL();
        
    lcd.setCursor(0,2);
    if (newT_H < 0 || newH_H < 0) lcd.print("TH : DHT Failed.   *");
     else {
           lcd.print("TH : " + String(newT_H));
           lcd.write(caractereDegre); // affiche le caractère
           lcd.print("C/"+String(newH_H)+"%");
           if (RelayFanStatus == 1 && newT_H >= TempStartWKS) lcd.print("*");
            else lcd.print("."); 
          }
    lcd.setCursor(0,3);
    if (newT_B < 0 || newH_B < 0) lcd.print("TB : DHT Failed.   *");
     else {
           lcd.print("TB : " + String(newT_B));
           lcd.write(caractereDegre); // affiche le caractère
           lcd.print("C/"+String(newH_B)+"%");
           if (RelayFanStatus == 1 && newT_B >= TempStartBat) lcd.print("*");
            else lcd.print("."); 
          }
  }

// * End Of Temp Function *

//* CRC-CCITT Function *

    // CRC16-CITT - Xmodem implement
    uint16_t calc_crc(const char *msg,int n)
    {
      // Initial value. xmodem uses 0xFFFF but this example
      // requires an initial value of zero.
      uint16_t x = 0;
      
      while(n--) {
        x = crc_xmodem_update(x, (uint16_t)*msg++);
      }
      return(x);
    }
    
    // See bottom of this page: http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
    // Polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
    uint16_t crc_xmodem_update (uint16_t crc, uint8_t data)
    {
      int i;
      
      crc = crc ^ ((uint16_t)data << 8);
      for (i=0; i<8; i++) {
        if (crc & 0x8000)
          crc = (crc << 1) ^ 0x1021; //(polynomial = 0x1021)
        else
          crc <<= 1;
      }
      return crc;
    }
    
// * End Of CRC-CCITT Function *

    // Paramètres Onduleurs WKS & WebServer...
    String QPIGS = "\x51\x50\x49\x47\x53\xB7\xA9\x0D"; //Lecture live data (QPIGS + CRC + CR)
    String QPIWS = "\x51\x50\x49\x57\x53\xB4\xDA\x0D";  
    String QDI = "\x51\x44\x49\x71\x1B\x0D";
    String QMOD = "\x51\x4D\x4F\x44\x49\xC1\x0D"; 
    String QVFW =  "\x51\x56\x46\x57\x62\x99\x0D"; 
    String QVFW2 = "\x51\x56\x46\x57\x32\xC3\xF5\x0D"; 
    
    float PIP_Grid_Voltage=0;
    float PIP_Grid_frequence=0;
    float PIP_output_Voltage=0;
    float PIP_output_frequence=0;
    int PIP_output_apparent_power=0;
    int PIP_output_active_power=0;
    int PIP_output_load_percent=0;
    int PIP_BUS_voltage=0;
    float PIP_Battery_voltage=0;
    float PIP_battery_charge_current=0;
    float PIP_battery_capacity=0;
    float PIP_Inverter_heat_sink_temperature=0;
    float PIP_PV_current=0;
    float PIP_PV_voltage=0;
    float PIP_Battery_voltage_from_SCC =0;
    float PIP_battery_discharge_current=0;
    int PIP_status;
    String Resultat="";

    String LastInverterCMD_Date_Time;
    String LastInverterCMD_Response;
    
    ESP8266WebServer server(80);
    // MDNSResponder mdns; // Retiré par Claude.
    SoftwareSerial Serial2(D6,D5); //GPOI12(D6)->Rx & GPOI14(D5)->Tx

    /*
     Sending Date Time to Inverter :
     
     3.29 DAT<YYMMDDHHMMSS><cr>: Date and time
     Computer: DAT<YYMMDDHHMMSS><cr>
     <Y, M, D, H, S> is an integer number 0 to 9.
     Device: (ACK<cr> if Device accepts this command, otherwise, responds (NAK<cr>
    */
    void SetInverterDateTime()
    {
     static String CMD;
     static uint16_t crc_out;
     static char lo;
     static char hi;

     Serial.println("Calling SetInverterDateTime...");
     const int CurrentDay = getEpochStringByParams(CE.toLocal(now()),(char*)"%d").toInt(); 
     if (NTPLastDayUpdate == 0 && NTPUpdateResult)
     {
      // Démarrage du module on met l'onduleur à l'heure...
      Serial.println("1er lancement.");
      CMD = getEpochStringByParams(CE.toLocal(now()),(char*)"%Y%m%d%H%M%S");
      CMD = "DAT" + CMD.substring(2);
      LastInverterCMD_Date_Time = CMD;
      const char *ccmd = CMD.c_str();
      crc_out = calc_crc(ccmd,CMD.length());
      lo = crc_out & 0xFF;
      hi = crc_out >> 8;
      CMD = CMD + lo + hi + "\x0D"; // CMD = CMD + lo + hi + "\x0D";
      Serial.println("CMD :: "+CMD);
      Serial2.print(CMD);
      Serial2.flush(); 
      if (Serial2.find("ACK"))
      {
       Serial.println("ACK");
       LastInverterCMD_Response = "ACK"; 
      }
      else
      {
       Serial.println("NAK");
       LastInverterCMD_Response = "NAK"; 
      } 
      Serial.println("Inverter Answer : " + LastInverterCMD_Response + ".");
      NTPLastDayUpdate = CurrentDay; 
     }
     else
     {
      // Mise à l'heure 1 fois par jour à 6H du matin...
      Serial.println("Mise à l'heure horodaté tous les jours à 6H...");
      Serial.println("CurrentDay = " + String(CurrentDay) +".");
      Serial.println("NTPLastDayUpdate = " + String(NTPLastDayUpdate)+".");
      if (NTPLastDayUpdate != CurrentDay && NTPUpdateResult)
      {
       // Mise à jour de l'onduleur tous les jours à 6 heure du matin...
       const int CurrentHour = getEpochStringByParams(CE.toLocal(now()),(char*)"%H").toInt();
       Serial.println("CurrentHour = " + String(CurrentHour)+".");
       if (CurrentHour == 6 && NTPUpdateResult)
       {
        CMD = getEpochStringByParams(CE.toLocal(now()),(char*)"%Y%m%d%H%M%S");
        CMD = "DAT" + CMD.substring(2);
        LastInverterCMD_Date_Time = CMD;
        const char *ccmd = CMD.c_str();
        crc_out = calc_crc(ccmd,CMD.length());
        lo = crc_out & 0xFF;
        hi = crc_out >> 8;  
        CMD = CMD + lo + hi + "\x0D"; // CMD = CMD + lo + hi + "\x0D";
        Serial.println("CMD :: "+CMD);
        Serial2.print(CMD);
        Serial2.flush(); 
        if (Serial2.find("ACK"))
        {
         Serial.println("ACK");
         LastInverterCMD_Response = "ACK"; 
        }
        else
        {
         Serial.println("NAK");
         LastInverterCMD_Response = "NAK"; 
        } 
        Serial.println("Inverter Answer : " + LastInverterCMD_Response + ".");
        NTPLastDayUpdate = CurrentDay;
       }
      }
     }
    }
    
    void read_PIP() {
        Serial2.print(QPIGS);
        Serial2.flush(); 
           
        if (Serial2.find("(")) {
            PIP_Grid_Voltage=Serial2.parseFloat();
            PIP_Grid_frequence=Serial2.parseFloat();
            PIP_output_Voltage=Serial2.parseFloat();
            PIP_output_frequence=Serial2.parseFloat();
            PIP_output_apparent_power=Serial2.parseInt();
            PIP_output_active_power=Serial2.parseInt();
            PIP_output_load_percent=Serial2.parseInt();
            PIP_BUS_voltage=Serial2.parseFloat();
            PIP_Battery_voltage=Serial2.parseFloat();
            PIP_battery_charge_current=Serial2.parseFloat();
            PIP_battery_capacity=Serial2.parseInt();
            PIP_Inverter_heat_sink_temperature =Serial2.parseInt();
            PIP_PV_current=Serial2.parseFloat();
            PIP_PV_voltage=Serial2.parseFloat();
            PIP_Battery_voltage_from_SCC =Serial2.parseFloat();
            PIP_battery_discharge_current=Serial2.parseFloat();
            PIP_status=Serial2.parseInt();
    
            Serial.println(PIP_Grid_Voltage);
            Serial.println(PIP_Grid_frequence);
            Serial.println(PIP_output_Voltage);
            Serial.println(PIP_output_frequence);
            Serial.println(PIP_output_apparent_power);
            Serial.println(PIP_output_active_power);
            Serial.println(PIP_output_load_percent);
            Serial.println(PIP_BUS_voltage);
            Serial.println(PIP_Battery_voltage);
            Serial.println(PIP_battery_charge_current);
            Serial.println(PIP_battery_capacity);
            Serial.println(PIP_Inverter_heat_sink_temperature);
            Serial.println(PIP_PV_current);
            Serial.println(PIP_PV_voltage);
            Serial.println(PIP_Battery_voltage_from_SCC);
            Serial.println(PIP_battery_discharge_current);
            Serial.println(PIP_status);
      }
    }

    void handleRoot(){
      int CurrentTime = getEpochStringByParams(CE.toLocal(now()),(char*)"%H").toInt();
      if (CurrentTime != NTPLastHourUpdate) UpdateRTCWithNTP();
      ReadNTPTime();
      
      if (!NTPUpdateResult)
      {
        DateFR = "*";
        TimeFR = "*";
        lcd.setCursor(0,0);
        lcd.print("NTP Failed.         ");
      }
      
      Serial.print("Lecture Du Port COM De L'onduleur à ");
      Serial.println(DateFR+" - "+TimeFR+".");
      
      read_PIP();
      delay(5000);
      Serial.println("Creation De La Page Web De Resultat.");

      // Lecture Des Informations DHT (Température & Humidité)...
      ReadTempHum();
      String TemperatureH;
      if (newT_H < 0) TemperatureH = "-1";
       else TemperatureH = String(newT_H);
      String TemperatureB;
      if (newT_B < 0) TemperatureB = "-1";
       else TemperatureB = String(newT_B);
      
      String HumidityH;
      if (newH_H < 0) HumidityH = "-1";
       else HumidityH = String(newH_H);
      String HumidityB;
      if (newH_B < 0) HumidityB = "-1";
       else HumidityB = String(newH_B);
       
      String INDEX_HTML = "";
      INDEX_HTML +="<!DOCTYPE HTML>";
      INDEX_HTML +="<html>";
      INDEX_HTML +="<head></head>";
      INDEX_HTML +="<body style='text-align:center;'>";
      INDEX_HTML +="<div class='infos_output_frequence'>"; INDEX_HTML += String(PIP_output_frequence); INDEX_HTML +="</div><br />"; // Frequence sortante
      INDEX_HTML +="<div class='infos_output_apparent_power'>"; INDEX_HTML += String(PIP_output_apparent_power); INDEX_HTML +="</div><br />"; // Puissance Apparente sortante
      INDEX_HTML +="<div class='infos_output_active_power'>"; INDEX_HTML += String(PIP_output_active_power); INDEX_HTML +="</div><br />"; // Puissance Active sortante
      INDEX_HTML +="<div class='infos_output_load_percent'>"; INDEX_HTML += String(PIP_output_load_percent); INDEX_HTML +="</div><br />"; // Charge sortante (%)
      INDEX_HTML +="<div class='infos_batterie'>"; INDEX_HTML += String(PIP_Battery_voltage); INDEX_HTML += "</div><br />"; // Tension de batterie
      INDEX_HTML +="<div class='infos_batterie_charge_current'>"; INDEX_HTML += String(PIP_battery_charge_current); INDEX_HTML += "</div><br />"; // Courant de chargement de Batterie
      INDEX_HTML +="<div class='infos_batterie_capacity'>"; INDEX_HTML += String(PIP_battery_capacity); INDEX_HTML +="</div><br />"; // Capacite de batterie
      INDEX_HTML +="<div class='infos_PV_current'>"; INDEX_HTML += String(PIP_PV_current); INDEX_HTML += "</div><br />"; // Courant photovoltaique
      INDEX_HTML +="<div class='infos_PV_voltage'>"; INDEX_HTML += String(PIP_PV_voltage); INDEX_HTML += "</div><br />"; // Tension photovoltaique entrante
      INDEX_HTML +="<div class='infos_PV_power'>"; INDEX_HTML += String(PIP_PV_voltage * PIP_PV_current); INDEX_HTML += "</div><br />"; // Tension photovoltaique entrante
      INDEX_HTML +="<div class='infos_batterie_discharge_current'>"; INDEX_HTML += String(PIP_battery_discharge_current); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie
      
      INDEX_HTML +="<div class='infos_Grid_Voltage'>"; INDEX_HTML += String(PIP_Grid_Voltage); INDEX_HTML += "</div><br />";
      INDEX_HTML +="<div class='infos_Grid_frequence'>"; INDEX_HTML += String(PIP_Grid_frequence); INDEX_HTML += "</div><br />";
      INDEX_HTML +="<div class='infos_output_Voltage'>"; INDEX_HTML += String(PIP_output_Voltage); INDEX_HTML += "</div><br />";
//      INDEX_HTML +="<div class='infos_BUS_voltage'>"; INDEX_HTML += String(PIP_BUS_voltage); INDEX_HTML += "</div><br />";
      INDEX_HTML +="<div class='infos_Inverter_heat_sink_temperature'>"; INDEX_HTML += String(PIP_Inverter_heat_sink_temperature); INDEX_HTML += "</div><br />";
      INDEX_HTML +="<div class='infos_Battery_voltage_from_SCC'>"; INDEX_HTML += String(PIP_Battery_voltage_from_SCC); INDEX_HTML += "</div><br />";
      INDEX_HTML +="<div class='infos_PIP_status'>"; INDEX_HTML += String(PIP_status); INDEX_HTML += "</div><br />";
      
      INDEX_HTML +="<div class='infos_dht_H_temp'>"; INDEX_HTML += String(TemperatureH); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie
      INDEX_HTML +="<div class='infos_dht_H_humidity'>"; INDEX_HTML += String(HumidityH); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie
      INDEX_HTML +="<div class='infos_dht_B_temp'>"; INDEX_HTML += String(TemperatureB); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie
      INDEX_HTML +="<div class='infos_dht_B_humidity'>"; INDEX_HTML += String(HumidityB); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie
      INDEX_HTML +="<div class='infos_RelayFanStatus'>"; INDEX_HTML += String(RelayFanStatus); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie

      INDEX_HTML +="<div class='infos_Inverter_LastCMD_Date_Time'>"; INDEX_HTML += String(LastInverterCMD_Date_Time); INDEX_HTML += "</div><br />"; // Dernière commande de mise à l'heure de l'onduleur
      INDEX_HTML +="<div class='infos_Inverter_LastCMD_Answer'>"; INDEX_HTML += String(LastInverterCMD_Response); INDEX_HTML += "</div><br />"; // Réponse de l'onduleur à la commande de mise à l'heure
      
      INDEX_HTML +="<div class='infos_date_stamp'>"; INDEX_HTML += String(DateFR); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie
      INDEX_HTML +="<div class='infos_time_stamp'>"; INDEX_HTML += String(TimeFR); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie
      INDEX_HTML +="<div class='infos_systeme'>"; INDEX_HTML += String("WUI Version " + WeMos_UPS_Version); INDEX_HTML += "</div><br />"; // Courant de decharge de batterie
      INDEX_HTML +="</body>";
      INDEX_HTML +="</html>"; 
      Serial.println("Envoie De La Page Web Au Client...");
      server.send(200, "text/html", INDEX_HTML);
      Serial.println("Page Web Envoyee.");
       
      if (QPOS >= 10) QPOS = 0;
      String Ligne[10];
      //               01234567890123456789
           Ligne[0] = "        <-->        ";
           Ligne[1] = "       <---->       ";
           Ligne[2] = "      <------>      ";
           Ligne[3] = "     <-------->     ";
           Ligne[4] = "    <---------->    ";
           Ligne[5] = "   <------------>   ";
           Ligne[6] = "  <-------------->  ";
           Ligne[7] = " <----------------> ";
           Ligne[8] = "<------------------>";
           Ligne[9] = "                    ";
      lcd.setCursor(0,1);
      lcd.print(Ligne[QPOS]);
      QPOS++;
/*
      if (QPOS >= 20) QPOS = 0;
      //              01234567890123456789
      char Ligne[] = "                    ";
      Ligne[QPOS] = '>';
      lcd.setCursor(0,1);
      lcd.print(String(Ligne));
      QPOS++;
*/      
    }

    void handleCommand()
    {
     int RBT;
     Serial.println("handleCommand...");

     RBT = 0;
     if (server.hasArg("Reboot"))
     {
      RBT = 1;
     }
     
     String INDEX_HTML = "";

     INDEX_HTML +="<!DOCTYPE HTML>";
     INDEX_HTML +="<html>";
     INDEX_HTML +="<head>";
     INDEX_HTML +="<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate' />";
     INDEX_HTML +="<meta http-equiv='Pragma' content='no-cache' />";
     INDEX_HTML +="<meta http-equiv='Expires' content='0' />";
     INDEX_HTML +="</head>";
     INDEX_HTML +="<body style='text-align:center;'>";

     INDEX_HTML +="<div class='Reboot'>"; INDEX_HTML += String(RBT); INDEX_HTML +="</div><br/>"; 
     INDEX_HTML +="</body>";
     INDEX_HTML +="</html>"; 
     server.send(200, "text/html", INDEX_HTML);
     Serial.println("Page Web Envoyee.");
     delay(3000);
     if (RBT == 1) ESP.restart();
    }

    void handleNotFound()
    {
      String message = "File Not Found\n\n";
      message += "URI: ";
      message += server.uri();
      message += "\nMethod: ";
      message += (server.method() == HTTP_GET)?"GET":"POST";
      message += "\nArguments: ";
      message += server.args();
      message += "\n";
      for (uint8_t i=0; i<server.args(); i++){
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
      }
      server.send(404, "text/plain", message);
    }

void setup(){
  // Vitesse Du Port à Configurer dans le moniteur de Port...
  Serial.begin(115200);
  delay(1000); // On attend un peut pour tout voir dans ke moniteur de Port.

  // Activation Du LCD...
  Serial.println("Activation LCD.");
  lcd.init(); // lcd.begin();
  lcd.backlight();
  lcd.createChar(caractereDegre, degre); // Création du caractère personnalisé degré.

  // Activation De la Sonde De Température DHT...
  Serial.println("Activation Sonde DHT.");
  dht_H.begin();
  dht_B.begin();
   
  IPAddress LAN_ADDR(192,168,0,6);       // Adresse IP du périphérique.
  IPAddress LAN_GW(192,168,0,1);         // Adresse IP de la passerelle.
  IPAddress LAN_SUBNET(255,255,255,0);   // Masque de sous réseau.
  IPAddress LAN_DNS(192,168,0,1);        // Adresse IP du serveur DNS.
  WiFi.mode(WIFI_STA);
  WiFi.config(LAN_ADDR,LAN_GW,LAN_SUBNET,LAN_DNS);
  WiFi.setHostname(HostName.c_str()); // Placer le Nom d'hote après WiFi.Config().
  WiFi.begin(monSSID, monPass);
  Serial.print("Etablissement de la connection WIFI");
  int Point = 0;
  while (WiFi.status() != WL_CONNECTED) {
        lcd.setCursor(0,0); 
        switch (Point)
        {
         case 0:
          lcd.print("Wait For WiFi   ");
          lcd.setCursor(0,1);
          lcd.print(monSSID); 
          break;
         
         case 1:
          lcd.print("Wait For WiFi.  ");
          break;
         
         case 2:
          lcd.print("Wait For WiFi . ");
          break;
         
         case 3:
          lcd.print("Wait For WiFi  .");
          break;
         
         default:
          Point = -1;
          break;
        }
        Point++;
        delay(500);
        Serial.print(".");
      }
      lcd.setCursor(0,1);
      lcd.print("                ");
  Serial.println("");    
  Serial.print("Connection etablie - Adresse IP : ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true); // Permet la reconnection automatique après une perte du WIFI.
  WiFi.persistent(true);
  
  // Initialisation Du Client NTP...
  NTPUpdateResult = 0;
  Serial.println("Lancement De La Requète NTP...");
  timeClient.begin();
  delay(1000);
  UpdateRTCWithNTP();
  
  // Activation Du Port Série Pour L'Onduleur...
  Serial.println("Activation Communication Avec L'Onduleur.");
  Serial2.begin(2400);
  pinMode(D6, INPUT);
  pinMode(D5, OUTPUT);

  // Par défaut les ventilos sont démarrés...
  RelayFanStatus = 1;
  
  // Mise à l'heure de l'onduleur au démarrage du module...
  Serial.println("Mise à l'heure de l'onduleur.");
  NTPLastDayUpdate = 0;
  LastInverterCMD_Date_Time = "";
  LastInverterCMD_Response = "";
  SetInverterDateTime();
  
  // Activation Du Serveur Web Local...
  Serial.println("Activation Du Serveur Web Local.");
  //server.on("/", handleRoot);
  
  // On pose une fonction handle pour l'appel de l'URL / ...
  server.on("/", HTTP_GET,handleRoot);
  // On pose une fonction handle pour l'appel de l'URI /Command ...
  server.on("/Command", HTTP_GET, handleCommand);
  // Enfin on pose une fonction handle pour une URI / [pas de handle correspondant] ...
  server.onNotFound(handleNotFound);
  server.begin();
  QPOS = 0;
}

void loop() {
  server.handleClient();

  // Une Requète NTP toutes les heures...
  int CurrentTime = getEpochStringByParams(CE.toLocal(now()),(char*)"%H").toInt();
  if (CurrentTime != NTPLastHourUpdate) UpdateRTCWithNTP();
  ReadNTPTime();
  lcd.setCursor(0,0);
  if (NTPUpdateResult) 
  { 
   SetInverterDateTime();
   if (RelayFanStatus == 1) lcd.print("*");
    else if (RelayFanStatus == 2) lcd.print("+");
          else lcd.print(" ");
    lcd.print(DateFR + " - " + TimeFR);
  }
   else
  {  
   lcd.print("NTP Failed.    "); 
   UpdateRTCWithNTP();
  }
  if (RelayFanStatus == 1) lcd.print("*");
   else if (RelayFanStatus == 2) lcd.print("+");
         else lcd.print(" ");
  ReadTempHum(); 
//  Serial.print("Current date: ");
//  Serial.println(DateFR);
//  Serial.println(timeClient.getFormattedTime());
//  delay(100);
}
