//#define DEBUG                   1
#define DHCP                    1

#define LED_WIFI                2                               //GPIO2
#define LED_TELEGRAM            16                              //GPIO16

#define READING_DELAY_SECONDS   10

#define INFLUX_DB_ADDRESS       "https://192.168.2.170:8086"    //Modify
#define INFLUX_DB_NAME          "clima-domus-demo"              //Modify
#define INFLUX_DB_QUERY         "/write?db="
#define INFLUX_DB_PRECISION     "&precision=s" 

#define INFLUX_DB_ENDPOINT INFLUX_DB_ADDRESS INFLUX_DB_QUERY INFLUX_DB_NAME
#define INFLUX_DB_ENDPOINT_GAS INFLUX_DB_ADDRESS INFLUX_DB_QUERY INFLUX_DB_NAME INFLUX_DB_PRECISION

const char* INFLUX_DB_LINE_PROTOCOL_ELECTRICITY = "electricity consumption_low_tariff=%ld,consumption_high_tariff=%ld,return_low_tariff=%ld,return_high_tariff=%ld,actual_consumption=%ld,actual_return=%ld";
const char* INFLUX_DB_LINE_PROTOCOL_GAS = "gas actual_consumption=%ld %ld";

#define GAS_DATE_START_INDEX    11
#define GAS_DATE_LENGTH         13
const char* GAS_DATE_FORMAT = "%2d%2d%2d%2d%2d%2d%c";

#define MAXLINELENGTH 256 // longest normal line is 47 char (+3 for \r\n\0)
