/////////////////////////////////////////
//UARDECS Sample Program "Skeleton"
//By H.kurosaki 2016/1/14
//コンパイルを通るだけの何もしないプログラム
//////////////////////////////////////////

#include <SPI.h>
#include <Ethernet2.h> //Arduino IDE Ver1.7.2以降でW5500搭載機種
//#include <Ethernet.h> //Ver1.7.2以降でW5100搭載機種
#include <avr/pgmspace.h>		//espでは使用できないかも
#include <EEPROM.h>
#include <Uardecs_mega.h>

const byte U_InitPin = 3;//このピンは変更可能です
const byte U_InitPin_Sense=HIGH;


const char U_name[] PROGMEM= "UARDECS Node v.1.0";
const char U_vender[] PROGMEM= "XXXXXX Co.";
const char U_uecsid[] PROGMEM= "000000000000";
const char U_footnote[] PROGMEM= "Test node";
char U_nodename[20] = "Sample";

UECSOriginalAttribute U_orgAttribute;

const int U_HtmlLine = 0;
struct UECSUserHtml U_html[U_HtmlLine]={};

const int U_MAX_CCM = 0;
UECSCCM U_ccmList[U_MAX_CCM];


void UserInit(){
U_orgAttribute.mac[0] = 0x00;
U_orgAttribute.mac[1] = 0x00;
U_orgAttribute.mac[2] = 0x00;
U_orgAttribute.mac[3] = 0x00;
U_orgAttribute.mac[4] = 0x00;
U_orgAttribute.mac[5] = 0x00;

//本当はここでCCMを初期化する

}


void OnWebFormRecieved(){}
void UserEverySecond(){}
void UserEveryMinute(){}
void UserEveryLoop(){}

void loop()
	{
	UECSloop();
	}

void setup()
	{
	UECSsetup();
  	}