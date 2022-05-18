/////////////////////////////////////////
//UARDECS Sample Program "TempHumidSensor_SHT3x"
//I2C connection test with SHT3x Ver1.1
//By H.kurosaki 2018/05/30
//////////////////////////////////////////

//Sensirion SHT3xシリーズから温度と湿度を読み出します。
//SHT3xは3.3V/5V両用なので、バス電圧5VのUNOやMEGAに直結できます。


//秋月電子のＩ２Ｃバス用双方向電圧レベル変換モジュールPCA9306を使う場合
//結線方法
//Arduino側-SHT3x側
//[SCL]-[SCL]
//[SDA]-[SDA]
//[5V]-[VCC]
//[GND]-[GND]


//注意！
//湿度センサは有機溶剤や酸・アルカリで損傷することがあります。
//アルコールなどの溶剤を近くで使わないこと。
//梱包時に使用するビニール袋やケースのプラスチック素材から揮発する有機ガスで
//損傷することがあるので注意すること。


#include <SPI.h>
#include <Ethernet2.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <Uardecs_mega.h>
#include <Wire.h>
#include "SHT3x.h"

#define OPRMODE_ERR_SHT3xSENSERR       0x20000000 //0 01000 00000 0 0000 0000000000000000 //センサ異常

SHT3x sht3x = SHT3x();

/////////////////////////////////////
//IP reset jupmer pin setting
//IPアドレスリセット用ジャンパーピン設定
/////////////////////////////////////
const byte U_InitPin = 3;
const byte U_InitPin_Sense=HIGH;


////////////////////////////////////
//Node basic infomation
//ノードの基本情報
///////////////////////////////////
const char U_name[] PROGMEM= "TempHumid Node";//MAX 20 chars
const char U_vender[] PROGMEM= "WARC/NARO";//MAX 20 chars
const char U_uecsid[] PROGMEM= "000000000000";//12 chars fixed
const char U_footnote[] PROGMEM= "SHT3x Sensor Node";
char U_nodename[20] = "node";//MAX 19chars (This value enabled in safemode)
UECSOriginalAttribute U_orgAttribute;//この定義は弄らないで下さい
//////////////////////////////////
// html page1 setting
//Web上の設定画面に関する宣言
//////////////////////////////////

//Total number of HTML table rows.
//web設定画面で表示すべき項目の総数
const int U_HtmlLine = 0;

//表示素材の登録
struct UECSUserHtml U_html[U_HtmlLine]={
};

//////////////////////////////////
// UserCCM setting
// CCM用の素材
//////////////////////////////////

enum {
CCMID_InAirTemp,
CCMID_InAirHumid,
CCMID_InAirHD,
CCMID_cnd,
CCMID_dummy, //CCMID_dummyは必ず最後に置くこと
};


//CCM格納変数の宣言
//ここはこのままにして下さい
const int U_MAX_CCM = CCMID_dummy;
UECSCCM U_ccmList[U_MAX_CCM];

//CCM定義用の素材
const char ccmNameTemp[] PROGMEM= "Temperature";
const char ccmTypeTemp[] PROGMEM= "InAirTemp";
const char ccmUnitTemp[] PROGMEM= "C";

const char ccmNameHumid[] PROGMEM= "Humid";
const char ccmTypeHumid[] PROGMEM= "InAirHumid";
const char ccmUnitHumid[] PROGMEM= "%";

const char ccmNameHD[] PROGMEM= "HumidDiff";
const char ccmTypeHD[] PROGMEM= "InAirHD.mIC";
const char ccmUnitHD[] PROGMEM= "g m-3";

const char ccmNameCnd[] PROGMEM= "NodeCondition";
const char ccmTypeCnd[] PROGMEM= "cnd.mIC";
const char ccmUnitCnd[] PROGMEM= "";

//------------------------------------------------------
void UserInit(){
//MAC address is printed on sticker of Ethernet Shield.
//You must assign unique MAC address to each nodes.
//MACアドレス設定、必ずEthernet Shieldに書かれた値を入力して下さい。
//全てのノードに異なるMACアドレスを設定する必要があります。
U_orgAttribute.mac[0] = 0x12;
U_orgAttribute.mac[1] = 0x34;
U_orgAttribute.mac[2] = 0x56;
U_orgAttribute.mac[3] = 0x78;
U_orgAttribute.mac[4] = 0x9A;
U_orgAttribute.mac[5] = 0xBC;

  
UECSsetCCM(true, CCMID_InAirTemp, ccmNameTemp, ccmTypeTemp, ccmUnitTemp, 29, 1, A_10S_0);
UECSsetCCM(true, CCMID_InAirHumid, ccmNameHumid, ccmTypeHumid, ccmUnitHumid, 29, 1, A_10S_0);
UECSsetCCM(true, CCMID_InAirHD, ccmNameHD, ccmTypeHD, ccmUnitHD, 29, 2, A_10S_0);
UECSsetCCM(true, CCMID_cnd      , ccmNameCnd , ccmTypeCnd , ccmUnitCnd , 29,0, A_1S_0);
}

//---------------------------------------------------------
void OnWebFormRecieved(){
}
//---------------------------------------------------------
void UserEverySecond(){

//------------------------------------------------------温度計測(2秒に一度)  
static char count=0;
static double t=-999;
static double rh=-999;


//SHT3xは計測コマンドの後、データが準備されるのに時間がかかる
//1秒間隔で計測コマンドと読み出しコマンドを交互に送信している
if(count ==0)
  {
    sht3x.startMeasure();
    }
else if(count==1)
    {
     if(sht3x.getTempHumid())
        {
          t=sht3x.temp;
          rh=sht3x.humidity;
	  U_ccmList[CCMID_cnd].value=0;
         }
      else 
        {//エラー
          t=-999;
          rh=-999;
	  U_ccmList[CCMID_cnd].value|=OPRMODE_ERR_SHT3xSENSERR;
        }
    }

U_ccmList[CCMID_InAirTemp].value=(long)(t*10);
U_ccmList[CCMID_InAirHumid].value=(long)(rh*10);


count++;
count%=2;


//------------------------------------------------エラー時にCCM送信を停止する
if(U_ccmList[CCMID_InAirTemp].value<=-999 || U_ccmList[CCMID_InAirHumid].value<=-999)
    {
        U_ccmList[CCMID_InAirTemp].flagStimeRfirst=false;
        U_ccmList[CCMID_InAirHumid].flagStimeRfirst=false;
        U_ccmList[CCMID_InAirHD].flagStimeRfirst=false;
        U_ccmList[CCMID_cnd].value|=OPRMODE_ERR_SHT3xSENSERR;
        return;
    }

  
  //飽差計算
  double humidpress=6.1078*pow(10,(7.5*t/(t+237.3)));
  double humidvol=217*humidpress/(t+273.15);
  U_ccmList[CCMID_InAirHD].value=(100-rh)*humidvol;//少数が下位2桁なのでそのまま出力

  
  

}

//---------------------------------------------------------
void UserEveryMinute(){
}
//---------------------------------------------------------
void UserEveryLoop(){
}

//---------------------------------
void loop(){
UECSloop();
}

//---------------------------------
void setup(){

sht3x.begin();
UECSsetup();
}




