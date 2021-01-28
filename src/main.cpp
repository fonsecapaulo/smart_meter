#include <Arduino.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>

#include "crc16.h"
#include "config.h"
#include "secrets.h"

const bool outputOnSerial = false;

// Vars to store meter readings
long mEVLT = 0; //Meter reading Electrics - consumption low tariff
long mEVHT = 0; //Meter reading Electrics - consumption high tariff
long mEOLT = 0; //Meter reading Electrics - return low tariff
long mEOHT = 0; //Meter reading Electrics - return high tariff
long mEAV = 0;  //Meter reading Electrics - Actual consumption
long mEAT = 0;  //Meter reading Electrics - Actual return
long mGAS = 0;  //Meter reading Gas
long mGASEpoch = 0;
long prevGASEpoch = 0;

char telegram[MAXLINELENGTH];

unsigned int currentCRC = 0;

/******************************************************
*SETUP WIFI + HTTP  
******************************************************/
void setupWifiHTTP(){
    #ifndef DHCP
        // Serial.println("Static IP Config!");
        // Set up Static IP address
        IPAddress local_IP(192, 168, 2, IP_ENDING);
        // Set up Gateway IP address
        IPAddress gateway(192, 168, 2, 254);
        // Set up subnet
        IPAddress subnet(255, 255, 0, 0);
        // Set up DNS
        IPAddress primaryDNS(8, 8, 8, 8);   //optional
        IPAddress secondaryDNS(8, 8, 4, 4); //optional
        // Configures static IP address
    
        if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
            
        }
    #endif
        
    WiFi.begin(SECRET_SSID, SECRET_PASSWORD);             //WiFi connection
    WiFi.mode(WIFI_STA);
    
    while (WiFi.status() != WL_CONNECTED) {               //Wait for the WiFI connection completion
        delay(500);
        #ifdef DEBUG 
            Serial.println("Waiting for connection");
        #endif
    }
    
    #ifdef DEBUG        
        //Prints MAC address
        Serial.print("MAC address:\t");
        Serial.println(WiFi.macAddress());
        //Prints IP address
        Serial.print("IP address:\t");
        Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
        //Prints IP address
        Serial.print("Channel:\t");
        Serial.println(WiFi.channel());         // Send the IP address of the ESP8266 to the computer
    #endif
}

void updateElectricity()
{
    char data[512];
    sprintf(
        data, INFLUX_DB_LINE_PROTOCOL_ELECTRICITY, 
        mEVLT,
        mEVHT,
        mEOLT,
        mEOHT,
        mEAV,
        mEAT
    );
    #ifdef DEBUG 
        Serial.println(data);
    #endif
    //WiFiClient wifi_client;
    std::unique_ptr<BearSSL::WiFiClientSecure> wifi_client (new BearSSL::WiFiClientSecure);
    wifi_client->setInsecure();
    
    HTTPClient https;            //Declare object of class HTTPClient

    https.begin(*wifi_client, INFLUX_DB_ENDPOINT);                //Specify request destination
    https.addHeader("Content-Type", "text/plain; charset=utf-8");  //Specify content-type header

    int httpsCode = https.POST(data);
    #ifdef DEBUG 
        Serial.println(httpsCode);
        String payload = https.getString();      //Get the response payload
        Serial.println(payload);
    #endif
    https.end();  //Close connection

}

long getTimestampEpoch (char* input, int start_index, int length, const char* date_format)
{
    char timestamp[length + 1];
    struct tm tm;
    char dst;
    
    char * start_pointer = input + start_index;
    strncpy(timestamp, start_pointer, length);

    sscanf(timestamp, date_format, &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &dst);
    //Necessary adjustments to get the correct epoch timestamp
    tm.tm_year = tm.tm_year + 2000 - 1900;
    tm.tm_mon-= 1;
    tm.tm_hour-= 1; // CET is 1 hour ahead of UTC
    tm.tm_isdst = -1;
    if (dst == 'S')
        tm.tm_hour-= 1; //on Summertime CET is 2 ahead of UTC

    return (long)mktime(&tm);
}

void updateGas()
{
    //Only update if we have a new reading
    if (mGASEpoch != prevGASEpoch)
    {
        prevGASEpoch = mGASEpoch;
        char data[256];
        sprintf(
            data, INFLUX_DB_LINE_PROTOCOL_GAS, 
            mGAS,
            mGASEpoch
        );
        #ifdef DEBUG 
            Serial.println(data);
        #endif
        //WiFiClient wifi_client;
        std::unique_ptr<BearSSL::WiFiClientSecure> wifi_client (new BearSSL::WiFiClientSecure);
        wifi_client->setInsecure();
        
        HTTPClient https;            //Declare object of class HTTPClient

        https.begin(*wifi_client, INFLUX_DB_ENDPOINT_GAS);                //Specify request destination
        https.addHeader("Content-Type", "text/plain; charset=utf-8");  //Specify content-type header

        int httpsCode = https.POST(data);
        #ifdef DEBUG 
            Serial.println(httpsCode);
            String payload = https.getString();      //Get the response payload
            Serial.println(payload);
        #endif
        https.end();  //Close connection  
    }
}

bool isNumber(char *res, int len)
{
    for (int i = 0; i < len; i++)
    {
        if (((res[i] < '0') || (res[i] > '9')) && (res[i] != '.' && res[i] != 0))
        {
            return false;
        }
    }
    return true;
}

int findCharInArrayRev(char array[], char c, int len)
{
    for (int i = len - 1; i >= 0; i--)
    {
        if (array[i] == c)
        {
            return i;
        }
    }
    return -1;
}

long getValidVal(long valNew, long valOld, long maxDiffer)
{
    //check if the incoming value is valid
    if (valOld > 0 && ((valNew - valOld > maxDiffer) && (valOld - valNew > maxDiffer)))
        return valOld;
    return valNew;
}

long getValue(char *buffer, int maxlen)
{
    int s = findCharInArrayRev(buffer, '(', maxlen - 2);
    if (s < 8)
        return 0;
    if (s > 32)
        s = 32;
    int l = findCharInArrayRev(buffer, '*', maxlen - 2) - s - 1;
    if (l < 4)
        return 0;
    if (l > 12)
        return 0;
    char res[16];
    memset(res, 0, sizeof(res));

    if (strncpy(res, buffer + s + 1, l))
    {
        if (isNumber(res, l))
        {
            return (1000 * atof(res));
        }
    }
    return 0;
}

bool decodeTelegram(int len)
{
    //need to check for start
    int startChar = findCharInArrayRev(telegram, '/', len);
    int endChar = findCharInArrayRev(telegram, '!', len);
    bool validCRCFound = false;
    if (startChar >= 0)
    {
        //start found. Reset CRC calculation
        currentCRC = CRC16(0x0000, (unsigned char *)telegram + startChar, len - startChar);
        if (outputOnSerial)
        {
           for (int cnt = startChar; cnt < len - startChar; cnt++)
               Serial1.print(telegram[cnt]);
        }
        //Serial.println("Start found!");
    }
    else if (endChar >= 0)
    {
        //add to crc calc
        currentCRC = CRC16(currentCRC, (unsigned char *)telegram + endChar, 1);
        char messageCRC[5];
        strncpy(messageCRC, telegram + endChar + 1, 4);
        messageCRC[4] = 0; //thanks to HarmOtten (issue 5)
        if (outputOnSerial)
        {
            // for (int cnt = 0; cnt < len; cnt++)
            //     Serial1.print(telegram[cnt]);
        }
        validCRCFound = (strtol(messageCRC, NULL, 16) == currentCRC);
        // if (validCRCFound)
        //     Serial1.println("\nVALID CRC FOUND!");
        // else
        //     Serial1.println("\n===INVALID CRC FOUND!===");
        currentCRC = 0;
    }
    else
    {
        currentCRC = CRC16(currentCRC, (unsigned char *)telegram, len);
        if (outputOnSerial)
        {
            for (int cnt = 0; cnt < len; cnt++)
                Serial1.print(telegram[cnt]);
        }
    }

    //long val = 0;
    //long val2 = 0;
    // 1-0:1.8.1(000992.992*kWh)
    // 1-0:1.8.1 = Elektra verbruik laag tarief (DSMR v4.0)
    if (strncmp(telegram, "1-0:1.8.1", strlen("1-0:1.8.1")) == 0)
        mEVLT = getValue(telegram, len);

    // 1-0:1.8.2(000560.157*kWh)
    // 1-0:1.8.2 = Elektra verbruik hoog tarief (DSMR v4.0)
    if (strncmp(telegram, "1-0:1.8.2", strlen("1-0:1.8.2")) == 0)
        mEVHT = getValue(telegram, len);

    // 1-0:2.8.1(000348.890*kWh)
    // 1-0:2.8.1 = Elektra opbrengst laag tarief (DSMR v4.0)
    if (strncmp(telegram, "1-0:2.8.1", strlen("1-0:2.8.1")) == 0)
        mEOLT = getValue(telegram, len);

    // 1-0:2.8.2(000859.885*kWh)
    // 1-0:2.8.2 = Elektra opbrengst hoog tarief (DSMR v4.0)
    if (strncmp(telegram, "1-0:2.8.2", strlen("1-0:2.8.2")) == 0)
        mEOHT = getValue(telegram, len);

    // 1-0:1.7.0(00.424*kW) Actueel verbruik
    // 1-0:2.7.0(00.000*kW) Actuele teruglevering
    // 1-0:1.7.x = Electricity consumption actual usage (DSMR v4.0)
    if (strncmp(telegram, "1-0:1.7.0", strlen("1-0:1.7.0")) == 0)
        mEAV = getValue(telegram, len);

    if (strncmp(telegram, "1-0:2.7.0", strlen("1-0:2.7.0")) == 0)
        mEAT = getValue(telegram, len);

    // 0-1:24.2.1(150531200000S)(00811.923*m3)
    // 0-1:24.2.1 = Gas (DSMR v4.0) on Kaifa MA105 meter
    if (strncmp(telegram, "0-1:24.2.1", strlen("0-1:24.2.1")) == 0){
        mGAS = getValue(telegram, len);
        mGASEpoch = getTimestampEpoch (telegram, GAS_DATE_START_INDEX, GAS_DATE_LENGTH, GAS_DATE_FORMAT);
    }
    return validCRCFound;
}

void readTelegram()
{
    if (Serial.available())
    {
        
        memset(telegram, 0, sizeof(telegram));
        while (Serial.available())
        {
            int len = Serial.readBytesUntil('\n', telegram, MAXLINELENGTH);
            telegram[len] = '\n';
            telegram[len + 1] = 0;
            yield();
            if (decodeTelegram(len + 1))
            {
                updateElectricity();
                updateGas();
                delay(READING_DELAY_SECONDS * 1000);
            }
        }
    }
}

void setup()
{
    pinMode(LED_WIFI, OUTPUT);
    pinMode(LED_TELEGRAM, OUTPUT);
    digitalWrite(LED_TELEGRAM, HIGH);
    
    digitalWrite(LED_WIFI, LOW);
    /*Setup UART*/
    Serial.begin(115200);
    #ifndef DEBUG
        Serial.swap();              //Swap Uart to RX GPIO13 (D7)
    #endif
    delay(1000); // Wait for a second
    digitalWrite(LED_TELEGRAM, LOW);
    
    setupWifiHTTP();
    
    digitalWrite(LED_WIFI, HIGH);
    digitalWrite(LED_TELEGRAM, HIGH);
}

void loop()
{
    readTelegram();      
}