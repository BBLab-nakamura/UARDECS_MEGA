//ソースコード中のコメントについては、基本的に中村による


#include <Uardecs_mega.h>

//バッファを確保　→使用するマイコンのEEPROMによって変わる
char UECSbuffer[BUF_SIZE];//main buffer
//おそらく文字列用のバッファを確保　文字の配列で２０個確保
char UECStempStr20[MAX_TYPE_CHAR];//sub buffer

//イーサネットのインスタンス
EthernetUDP UECS_UDP16520;
EthernetUDP UECS_UDP16529;
EthernetUDP UECS_UDP16521;

//指定したポートをlistenするサーバを生成して、接続の要求に対応します。
// http://www.musashinodenpa.com/arduino/ref/index.php?f=1&pos=1273
//引数：リッスンするポートの番号
EthernetServer UECSlogserver(80);
//指定されたインターネットIPアドレスとポート（client.connect（）関数で定義）に接続できるクライアントを作成します。
EthernetClient UECSclient;

//ccmオブジェクトの作成
struct UECSTEMPCCM UECStempCCM;

//時間の把握用のカウンター
unsigned char UECSsyscounter60s;
unsigned long UECSsyscounter1s;
unsigned long UECSnowmillis;
unsigned long UECSlastmillis;

//bool flag_programUpdate;



//Form CCM.cpp
//##############################################################################
//##############################################################################

//uecs用の文字列の準備
const char *UECSattrChar[] = { UECSccm_ROOMTXT, UECSccm_REGIONTXT, UECSccm_ORDERTXT, UECSCCM_PRIOTXT,};
//送信頻度を準備（文字列として準備、使い方は後で把握する必要がある）
const char *UECSCCMLEVEL[]={UECS_A1S0, UECS_A1S1, UECS_A10S0, UECS_A10S1, UECS_A1M0, UECS_A1M1, UECS_S1S, UECS_S1M, UECS_B0_, UECS_B1_, };

//
//第一引数：CCMオブジェクトのポインタ
//第二引数：
//返り値：
boolean UECSparseRec( struct UECSTEMPCCM *_tempCCM,int *matchCCMID){
	

	int i;
	int progPos = 0;
	int startPos = 0;
	short shortValue=0;
	
	//
	if(!UECSFindPGMChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){return false;}
	startPos+=progPos;
	
	if(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos))
		{
		startPos+=progPos;
			//E10 packet
		}
	else
		{
			//other ver packet
			if(!(U_orgAttribute.flags&ATTRFLAG_LOOSELY_VERCHECK)){return false;}
		}
	
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_DATATYPE[0],&progPos)){return false;}
	startPos+=progPos;
	
	
	
	//copy ccm type string
	for(i=0;i<MAX_TYPE_CHAR;i++)
	{
	_tempCCM->type[i]=UECSbuffer[startPos+i];
	if(_tempCCM->type[i]==ASCIICODE_DQUOT || _tempCCM->type[i]=='\0')
		{_tempCCM->type[i]='\0';break;}
	}
	_tempCCM->type[MAX_CCMTYPESIZE]='\0';
	startPos=startPos+i;
	
	//In a practical environment ,packets of 99% are irrelevant CCM.
	//matching top 3 chars for acceleration 
	*matchCCMID=-1;
	for(i=0;i<U_MAX_CCM;i++)
	{
      if(U_ccmList[i].ccmLevel != NONE && U_ccmList[i].sender == false)//check receive ccm
      	{
        if(_tempCCM->type[0]==U_ccmList[i].typeStr[0] && 
           _tempCCM->type[1]==U_ccmList[i].typeStr[1] && 
       	   _tempCCM->type[2]==U_ccmList[i].typeStr[2])
        		{
        		*matchCCMID=i;
        		break;
        		}
        }
	}
	if(*matchCCMID<0){return false;}//my ccm was not match ->cancel packet reading


	for(i=0;i<MAX_ATTR_NUMBER;i++)
	{
	if(!UECSGetValPGMStrAndChr(&UECSbuffer[startPos],UECSattrChar[i],'\"',&(_tempCCM->attribute[i]),&progPos)){return false;}
	startPos+=progPos;
	}

	//find tag end
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CLOSETAG[0],&progPos)){return false;}
	startPos+=progPos;
	
	//get value
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&(_tempCCM->value),&(_tempCCM->decimal),&progPos)){return false;}
	startPos+=progPos;
	
	//ip tag
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_IPTAG[0],&progPos))
		{
			//ip tag not found(old type packet)
			//ip address is already filled
			if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_FOOTER0[0],&progPos)){return false;}
			return true;
		}
	startPos+=progPos;
	
	
	unsigned char ip[4];
	if(!UECSGetIPAddress(&UECSbuffer[startPos],ip,&progPos)){return false;}

	_tempCCM->address[0]=ip[0];
	_tempCCM->address[1]=ip[1];
	_tempCCM->address[2]=ip[2];
	_tempCCM->address[3]=ip[3];
	
	startPos+=progPos;

	if(U_orgAttribute.flags&ATTRFLAG_LOOSELY_VERCHECK){return true;}//Ignore information after ip
	
	//check footer
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_FOOTER1[0],&progPos)){return false;}
	/*
	Serial.println("type");
	Serial.println(_tempCCM->type);
	Serial.println("attribute[AT_ROOM]");
	Serial.println(_tempCCM->attribute[AT_ROOM]);
	Serial.println("attribute[AT_REGI]");
	Serial.println(_tempCCM->attribute[AT_REGI]);
	Serial.println("attribute[AT_ORDE]");
	Serial.println(_tempCCM->attribute[AT_ORDE]);
	Serial.println("attribute[AT_PRIO]");
	Serial.println(_tempCCM->attribute[AT_PRIO]);
	Serial.println("value");
	Serial.println(_tempCCM->value);
	Serial.println("decimal");
	Serial.println(_tempCCM->decimal);
	Serial.println("ip[0]");
	Serial.println(_tempCCM->address[0]);
	Serial.println("ip[1]");
	Serial.println(_tempCCM->address[1]);
	Serial.println("ip[2]");
	Serial.println(_tempCCM->address[2]);
	Serial.println("ip[3]");
	Serial.println(_tempCCM->address[3]);
	*/
	return true;


}
//----------------------------------------------------------------------------
void UECSCreateCCMPacketAndSend( struct UECSCCM* _ccm){
ClearMainBuffer();
UDPAddPGMCharToBuffer(&(UECSccm_XMLHEADER[0]));
UDPAddPGMCharToBuffer(&(UECSccm_UECSVER_E10[0]));
UDPAddPGMCharToBuffer(&(UECSccm_DATATYPE[0]));
UDPAddCharToBuffer(_ccm->typeStr);

for(int i=0;i<4;i++)
	{
	UDPAddPGMCharToBuffer(UECSattrChar[i]); 
	UDPAddValueToBuffer(_ccm->baseAttribute[i]);
	}



UDPAddPGMCharToBuffer(&(UECSccm_CLOSETAG[0]));
  dtostrf(((double)_ccm->value) / pow(10, _ccm->decimal), -1, _ccm->decimal, UECStempStr20);
UDPAddCharToBuffer(UECStempStr20);
UDPAddPGMCharToBuffer(&(UECSccm_IPTAG[0]));

if(U_orgAttribute.status & STATUS_SAFEMODE)
	{
	UDPAddPGMCharToBuffer(&(UECSdefaultIPAddress[0]));
	}
else
	{
	sprintf(UECStempStr20, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2],U_orgAttribute.ip[3]);
	UDPAddCharToBuffer(UECStempStr20);
	}

UDPAddPGMCharToBuffer(&(UECSccm_FOOTER1[0]));

//send ccm
  UECS_UDP16520.beginPacket(_ccm->address, 16520);
  UECS_UDP16520.write(UECSbuffer);
  
  if(UECS_UDP16520.endPacket()==0)
  		{
  			UECSresetEthernet();//when udpsend failed,reset ethernet status
  		}

}

void UECSupRecCCM(UECSCCM* _ccm, UECSTEMPCCM* _ccmRec){
  boolean success = false;

	int i;
	for(i=0;i<MAX_ATTR_NUMBER;i++)
		{_ccm->attribute[i] = _ccmRec->attribute[i];}

	for(i=0;i<MAX_IPADDRESS_NUMBER;i++)
		{_ccm->address[i] = _ccmRec->address[i];}

    
    int dif_decimal=_ccm->decimal-_ccmRec->decimal;
    if(dif_decimal>=0){_ccm->value = _ccmRec->value*pow(10,dif_decimal);}
    else{_ccm->value = _ccmRec->value/pow(10,-dif_decimal);}
    

    _ccm->recmillis = 0;
    _ccm->validity=true;
    _ccm->flagStimeRfirst = true;

}
//---------------------------------------------------------------

void UECScheckUpDate(UECSTEMPCCM* _tempCCM, unsigned long _time,int startid){

    for(int i = startid; i < U_MAX_CCM; i++){
      if(U_ccmList[i].ccmLevel == NONE || U_ccmList[i].sender == true){continue;}
      
      
          if(!((_tempCCM->attribute[AT_ROOM] == 0 || U_ccmList[i].baseAttribute[AT_ROOM]==0 || _tempCCM->attribute[AT_ROOM] == U_ccmList[i].baseAttribute[AT_ROOM]) &&
               (_tempCCM->attribute[AT_REGI] == 0 || U_ccmList[i].baseAttribute[AT_REGI]==0 || _tempCCM->attribute[AT_REGI] == U_ccmList[i].baseAttribute[AT_REGI]) &&
               (_tempCCM->attribute[AT_ORDE] == 0 || U_ccmList[i].baseAttribute[AT_ORDE]==0 || _tempCCM->attribute[AT_ORDE] == U_ccmList[i].baseAttribute[AT_ORDE]))){continue;}

        //type 
        if(strcmp(U_ccmList[i].typeStr, _tempCCM->type) != 0){continue;}
        
          boolean up = false;
                if(U_ccmList[i].ccmLevel==B_0 || U_ccmList[i].ccmLevel==B_1){up = true;}
                else if(!U_ccmList[i].validity){up = true;} //fresh ccm 
                else if(_tempCCM->attribute[AT_PRIO] < U_ccmList[i].attribute[AT_PRIO])
                    {up = true;}//lower priority number is strong
                else{
					
                  if(_tempCCM->attribute[AT_ROOM] == U_ccmList[i].attribute[AT_ROOM]){
                    if(_tempCCM->attribute[AT_REGI] == U_ccmList[i].attribute[AT_REGI]){
                      if(_tempCCM->attribute[AT_ORDE] == U_ccmList[i].attribute[AT_ORDE]){
                        
                        //if(_tempCCM->address <= U_ccmList[i].address)
                        //convert big endian
                        unsigned long address_t=_tempCCM->address[0];
                        address_t=(address_t<<8)|_tempCCM->address[1];
                        address_t=(address_t<<8)|_tempCCM->address[2];
                        address_t=(address_t<<8)|_tempCCM->address[3];
                        unsigned long address_b=U_ccmList[i].address[0];
                        address_b=(address_b<<8)|U_ccmList[i].address[1];
                        address_b=(address_b<<8)|U_ccmList[i].address[2];
                        address_b=(address_b<<8)|U_ccmList[i].address[3];
                        
                        if(address_t<=address_b)
                        {up = true;} 
                      }
                      else if (_tempCCM->attribute[AT_ORDE] == U_ccmList[i].baseAttribute[AT_ORDE] || U_ccmList[i].baseAttribute[AT_ORDE]==0)
                      {up = true;}                         
                    }
                    else if (_tempCCM->attribute[AT_REGI] == U_ccmList[i].baseAttribute[AT_REGI] || U_ccmList[i].baseAttribute[AT_REGI]==0)
                    {up = true;}                       
                  }
                  else if (_tempCCM->attribute[AT_ROOM] == U_ccmList[i].baseAttribute[AT_ROOM]  || U_ccmList[i].baseAttribute[AT_ROOM]==0)
                  {up = true;} 
                }

                if(up){UECSupRecCCM(&U_ccmList[i], _tempCCM);}
      }  
}

/********************************/
/* 16529 Response   *************/
/********************************/

boolean UECSresNodeScan(){
   	int i;
	int progPos = 0;
	int startPos = 0;
	
	if(!UECSFindPGMChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){return false;}
	startPos+=progPos;

	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos))
	{
		//other ver
		return false;
	}
	startPos+=progPos;
	
	//NODESCAN
	if(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_NODESCAN1[0],&progPos) || UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_NODESCAN2[0],&progPos))
	{
		
    //NODESCAN response
	ClearMainBuffer();
    UDPAddPGMCharToBuffer(&(UECSccm_XMLHEADER[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_UECSVER_E10[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_NODENAME[0])); 
    UDPAddPGMCharToBuffer(&(U_name[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_VENDER[0])); 
    UDPAddPGMCharToBuffer(&(U_vender[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_UECSID[0])); 
    UDPAddPGMCharToBuffer(&(U_uecsid[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_UECSID_IP[0]));
    
    
    
    if(U_orgAttribute.status & STATUS_SAFEMODE)
		{
		UDPAddPGMCharToBuffer(&(UECSdefaultIPAddress[0]));
		}
	else
		{
	    sprintf(UECStempStr20, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2], U_orgAttribute.ip[3]);
	    UDPAddCharToBuffer(UECStempStr20);
	    }
    
    UDPAddPGMCharToBuffer(&(UECSccm_MAC[0]));
    sprintf(UECStempStr20, "%02X%02X%02X%02X%02X%02X", U_orgAttribute.mac[0], U_orgAttribute.mac[1], U_orgAttribute.mac[2], U_orgAttribute.mac[3], U_orgAttribute.mac[4], U_orgAttribute.mac[5]);
    UDPAddCharToBuffer(UECStempStr20);
    UDPAddPGMCharToBuffer(&(UECSccm_NODECLOSE[0]));
    
    return true;
	}
    
	//CCMSCAN
	if(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CCMSCAN[0],&progPos))
	{

		
	short pageNum=0;
	startPos+=progPos;
	
	
	if(UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_PAGE[0],'\"',&pageNum,&progPos))//format of page number written type
		{
		startPos+=progPos;
		//check close tag
		if(!(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CCMSCANCLOSE2[0],&progPos))){return false;}
		}
	else if(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CCMSCANCLOSE0[0],&progPos)) //format of page number abridged type
		{
		pageNum=1;
		}
	else
		{return false;}
	
	//CCMSCAN response
	ClearMainBuffer();
	UDPAddPGMCharToBuffer(&(UECSccm_XMLHEADER[0]));
	UDPAddPGMCharToBuffer(&(UECSccm_UECSVER_E10[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_CCMNUM[0]));
	
	//count enabled ccm
	short enabledCCMNum=0;
	short returnCCMID=-1;
		for(i = 0; i < U_MAX_CCM; i++)
		{
	    if(U_ccmList[i].ccmLevel != NONE)
	    	{
	    	enabledCCMNum++;
	    	if(enabledCCMNum==pageNum){returnCCMID=i;}
	    	}
	    
	    
	    }
	    

	
	if(enabledCCMNum==0 || returnCCMID<0){return false;}//page num over
	
	UDPAddValueToBuffer(pageNum);
	UDPAddPGMCharToBuffer(&(UECSccm_TOTAL[0]));
	UDPAddValueToBuffer(enabledCCMNum);
    UDPAddPGMCharToBuffer(&(UECSccm_CLOSETAG[0]));
	UDPAddValueToBuffer(1);//Column number is always 1
    UDPAddPGMCharToBuffer(&(UECSccm_CCMNO[0]));
	UDPAddValueToBuffer(pageNum);//page number = ccm number
	
	
	for(i=0;i<3;i++)
	{
	 UDPAddPGMCharToBuffer(UECSattrChar[i]);
	 UDPAddValueToBuffer(U_ccmList[returnCCMID].baseAttribute[i]);
	}
	 UDPAddPGMCharToBuffer(UECSattrChar[AT_PRIO]);
	 UDPAddValueToBuffer(U_ccmList[returnCCMID].baseAttribute[AT_PRIO]);

	 
	 UDPAddPGMCharToBuffer(&(UECSccm_CAST[0]));
	 UDPAddValueToBuffer(U_ccmList[returnCCMID].decimal);
	 UDPAddPGMCharToBuffer(&(UECSccm_UNIT[0]));                          
	 UDPAddPGMCharToBuffer((U_ccmList[returnCCMID].unit));
	 UDPAddPGMCharToBuffer(&(UECSccm_SR[0]));
     if(U_ccmList[returnCCMID].sender){
     	 		UDPAddPGMCharToBuffer(UECSTxtPartS);
              }
              else{
     	 		UDPAddPGMCharToBuffer(UECSTxtPartR);
              }              
	 UDPAddPGMCharToBuffer(&(UECSccm_LV[0]));                          
	 UDPAddPGMCharToBuffer((UECSCCMLEVEL[U_ccmList[returnCCMID].ccmLevel]));
	 UDPAddPGMCharToBuffer(&(UECSccm_CLOSETAG[0]));
	 UDPAddCharToBuffer(U_ccmList[returnCCMID].typeStr);
	 UDPAddPGMCharToBuffer(&(UECSccm_CCMRESCLOSE[0]));  
    
    return true;
    }
    
    return false;
  
  
}


//------------------------------------------------------------------
void UECSautomaticSendManager()
{
/*
UDP transmission load balancing
Improved UDP send timing not to be concentrated in Ver2.0 or later.
Packets sent at 10-second and 60-second intervals were sent together in past versions, but are sent separately in the new version.
Transmission timing is distributed according to the order of registration of the lower one of the IP addresses and the CCM.
However, this load balancing is ineffective for packets sent every second.
*/
int timing_count;

for(int id=0;id<U_MAX_CCM;id++)
	{
  if(U_ccmList[id].ccmLevel == NONE || !U_ccmList[id].sender){continue;}

  	//Timeout judgment of the sending CCM
   	if(U_ccmList[id].flagStimeRfirst == true)
	   		{
	   		U_ccmList[id].recmillis=0;
    		U_ccmList[id].validity=true;
    		}
     else
     	{
     	if((U_ccmList[id].ccmLevel==A_1S_0 || U_ccmList[id].ccmLevel==A_1S_1)&& U_ccmList[id].recmillis>1000){U_ccmList[id].validity=false;}
     	else if((U_ccmList[id].ccmLevel==A_10S_0 || U_ccmList[id].ccmLevel==A_10S_1)&& U_ccmList[id].recmillis>10000){U_ccmList[id].validity=false;}
     	else if((U_ccmList[id].ccmLevel==A_1M_0 || U_ccmList[id].ccmLevel==A_1M_1)&& U_ccmList[id].recmillis>60000){U_ccmList[id].validity=false;}
     	}
   	if(U_ccmList[id].recmillis<36000000){U_ccmList[id].recmillis+=1000;}

		//Determination of CCM transmission timing
	  timing_count=U_orgAttribute.ip[3]+id;
	  if((U_ccmList[id].ccmLevel == A_10S_1 || U_ccmList[id].ccmLevel == A_1M_1 ) && U_ccmList[id].old_value!=U_ccmList[id].value)
  	  {
  	  U_ccmList[id].flagStimeRfirst = true;
  	  }
      else if(U_ccmList[id].ccmLevel == A_1S_0 || U_ccmList[id].ccmLevel == A_1S_1 || U_ccmList[id].ccmLevel == S_1S_0)
      {
        U_ccmList[id].flagStimeRfirst = true;
       }
       else if((UECSsyscounter60s % 10 == (timing_count % 10)) && (U_ccmList[id].ccmLevel == A_10S_0 || U_ccmList[id].ccmLevel == A_10S_1))
       {
         U_ccmList[id].flagStimeRfirst = true;
       }
       else if(UECSsyscounter60s == (timing_count % 60) && (U_ccmList[id].ccmLevel == A_1M_0 || U_ccmList[id].ccmLevel == A_1M_1 || U_ccmList[id].ccmLevel == S_1M_0))
       {
         U_ccmList[id].flagStimeRfirst = true;
       }
       else
       {
         U_ccmList[id].flagStimeRfirst = false;
       }
       	U_ccmList[id].old_value=U_ccmList[id].value;
	}
}
//----------------------------------------------------------------------
void UECSautomaticValidManager(unsigned long td)
{
for(int id=0;id<U_MAX_CCM;id++)
	{
  if(U_ccmList[id].ccmLevel == NONE|| U_ccmList[id].sender){continue;}
    	
    	
    	if(U_ccmList[id].recmillis>86400000)//over 24hour
    		{
    			continue;//stop count
    		}

  		U_ccmList[id].recmillis+=td;//count time(ms) since last recieved
  		
  	  unsigned long validmillis=0;
  	  if(U_ccmList[id].ccmLevel == A_1S_0 || U_ccmList[id].ccmLevel == A_1S_1 || U_ccmList[id].ccmLevel == S_1S_0)
  	  {
	    validmillis = 3000;  
  		}
	  	else if(U_ccmList[id].ccmLevel == A_10S_0 || U_ccmList[id].ccmLevel == A_10S_1)
	  	{
    	validmillis = 30000;
  		}
  		else if(U_ccmList[id].ccmLevel == A_1M_0 || U_ccmList[id].ccmLevel == A_1M_1 || U_ccmList[id].ccmLevel == S_1M_0)
  		{
    	validmillis = 180000;
 	 	}
    	
       if(U_ccmList[id].recmillis > validmillis || U_ccmList[id].flagStimeRfirst == false)
       {
         U_ccmList[id].validity = false;  
       }
	}
}

//##############################################################################
//##############################################################################

//EEPROMに書き込む
//第一引数：書き込む場所のアドレス
//第二引数：書き込みたいデータのサイズ
void UECS_EEPROM_writeLong(int ee, long value)
{
		//必要な容量の把握？（書き込む文字数？）
    byte* p = (byte*)(void*)&value;
		//
    for (unsigned int i = 0; i < sizeof(value); i++)
    {
		//書き込まれている内容はスキップする
    if(EEPROM.read(ee)!=p[i])//same value skip
    		{
    		EEPROM.write(ee, p[i]);	//書き込む
    		}
    		ee++;
	}

}

//EEPROMから読み込む　※
//引数：読み込みを開始する位置
//返り値：読み込んだデータのアドレス？
long UECS_EEPROM_readLong(int ee)
{
		//
    long value = 0;
    byte* p = (byte*)(void*)&value;
    for (unsigned int i = 0; i < sizeof(value); i++)
	  *p++ = EEPROM.read(ee++);		//読み込んだ値を用意した領域に入れていく

    return value;		//読み込んだ値が入っているポインタ
}
//-----------------------------------------------------------new
//
void HTTPsetInput(short _value){		//表示するアドレスの文字列を作成する
    HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));		//文字列の追加
    HTTPAddValueToBuffer(_value);
    HTTPAddPGMCharToBuffer(&(UECShtmlINPUTCLOSE3[0]));
}
//-----------------------------------------------------------new
//ipアドレスをwebブラウザに表示する
//引数：ipアドレスの配列
void HTTPprintIPtoHtml(byte address[])
{
	//ex)255:255:255:255<br>　になる
  for(int i = 0; i < 3; i++)
  {
	 //表示するipアドレスを詰め込む
   HTTPsetInput(address[i]);
   HTTPAddPGMCharToBuffer(UECSTxtPartColon);		//ipアドレスを:で繋いでいく
  }      
  HTTPsetInput(address[3]);
  HTTPAddPGMCharToBuffer(&(UECSbtrag[0]));			//改行コード

}
//-----------------------------------------------------------new
//---------------------------------------------------------------
//
//引数：
void HTTPPrintRedirect(char page){
 ClearMainBuffer();		//バッファの初期化
 //表示する文字列を作成していく
 HTTPAddPGMCharToBuffer(&(UECShttpHead200_OK[0]));
 HTTPAddPGMCharToBuffer(&(UECShttpHeadContentType[0]));
 HTTPAddPGMCharToBuffer(&(UECShttpHeadConnection[0]));
 HTTPAddPGMCharToBuffer(&(UECShtmlHEADER[0]));
 //表示するhtmlを作成していく
 if(page==3)
 	{
 	HTTPAddPGMCharToBuffer(&(UECShtmlREDIRECT3[0]));
 	}
 else
 	{
	 HTTPAddPGMCharToBuffer(&(UECShtmlREDIRECT1[0]));
 	}
 
 HTTPCloseBuffer();			//uecsクライアントに表示
}
/*
void HTTPPrintRedirectP3(){
 ClearMainBuffer();
 HTTPAddPGMCharToBuffer(&(UECShttpHead200_OK[0]));
 HTTPAddPGMCharToBuffer(&(UECShttpHeadContentType[0]));
 HTTPAddPGMCharToBuffer(&(UECShttpHeadConnection[0]));
 HTTPAddPGMCharToBuffer(&(UECShtmlHEADER[0]));
 HTTPAddPGMCharToBuffer(&(UECShtmlREDIRECT3[0]));
 HTTPCloseBuffer();
}*/
//-----------------------------------------------------------
//html用のヘッダーの作成
void HTTPPrintHeader(){
 ClearMainBuffer();		//バッファのクリア

//ヘッダー用のhtmを作成していく 
HTTPAddPGMCharToBuffer(&(UECShttpHead200_OK[0]));
HTTPAddPGMCharToBuffer(&(UECShttpHeadContentType[0]));
HTTPAddPGMCharToBuffer(&(UECShttpHeadConnection[0]));
 
 HTTPAddPGMCharToBuffer(&(UECShtmlHEADER[0]));
 HTTPAddPGMCharToBuffer(&(UECShtmlNORMAL[0]));

 //メモリがリークしてる時　※
 //ビット演算？　詳しく調べる必要がある https://www.javadrive.jp/cstart/ope/index6.html
 //両方の同じビットが１の時だけ１になる
 //ビットごとにステータスが割り当てられている（ヘッダー参照）
 if(U_orgAttribute.status & STATUS_MEMORY_LEAK)
 	{HTTPAddPGMCharToBuffer(&(UECShtmlATTENTION_INTERR[0]));}		//err表示
	//safemodeの時
 if(U_orgAttribute.status & STATUS_SAFEMODE)
 	{HTTPAddPGMCharToBuffer(&(UECShtmlATTENTION_SAFEMODE[0]));}		//safemode表示

 HTTPAddPGMCharToBuffer(&(U_name[0]));
 
  HTTPAddPGMCharToBuffer(&(UECShtmlTITLECLOSE[0]));
  
  HTTPAddPGMCharToBuffer(&(UECShtml1A[0]));   // </script></HEAD><BODY><CENTER><H1>
}
//-----------------------------------------------------------
//ccmが送信できなかった時に表示されるページ？
void HTTPsendPageError(){
  HTTPPrintHeader();		//ヘッダーの作成
	//表示用のhtmlの作成
  HTTPAddPGMCharToBuffer(&(UECSpageError[0]));      
  HTTPAddPGMCharToBuffer(&(UECShtmlH1CLOSE[0]));
  HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0])); 
  HTTPCloseBuffer();		//uecsクライアントに表示
}
//-------------------------------------------------------------
//GETに対して返すページ？
void HTTPsendPageIndex(){

  HTTPPrintHeader();		//ヘッダーの作成
  HTTPAddCharToBuffer(U_nodename);
  HTTPAddPGMCharToBuffer(&(UECShtmlH1CLOSE[0]));
  HTTPAddPGMCharToBuffer(&(UECShtmlIndex[0]));
  HTTPAddPGMCharToBuffer(&(UECShtmlHR[0]));
  HTTPAddPGMCharToBuffer(&(U_footnote[0]));		//空？

#if defined(_ARDUINIO_MEGA_SETTING)
HTTPAddPGMCharToBuffer(&(LastUpdate[0]));
HTTPAddPGMCharToBuffer(&(ProgramDate[0]));
HTTPAddPGMCharToBuffer(&(UECSTxtPartHyphen[0]));
HTTPAddPGMCharToBuffer(&(ProgramTime[0]));
#endif

  HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0]));
  HTTPCloseBuffer();			//実際に表示
}
//--------------------------------------------------
void HTTPsendPageLANSetting(){
  UECSinitOrgAttribute();

  HTTPPrintHeader();
  HTTPAddCharToBuffer(U_nodename);
  HTTPAddPGMCharToBuffer(UECShtmlH1CLOSE); 
  HTTPAddPGMCharToBuffer(UECShtmlH2TAG);
  HTTPAddPGMCharToBuffer(UECShtmlLANTITLE);
  HTTPAddPGMCharToBuffer(UECShtmlH2CLOSE);  
  HTTPAddPGMCharToBuffer(&(UECShtmlLAN2[0]));  // <form action=\"./h2.htm\" name=\"f\"><p>
  HTTPAddPGMCharToBuffer(&(UECShtmlLAN3A[0]));   // address: 
 byte UECStempByte[4];
  for(int i = 0; i < 4; i++){
    UECStempByte[i] = U_orgAttribute.ip[i];
  }
  HTTPprintIPtoHtml(UECStempByte); // XXX:XXX:XXX:XXX <br>
  
  HTTPAddPGMCharToBuffer(&(UECShtmlLAN3B[0]));  // subnet: 
  HTTPprintIPtoHtml( U_orgAttribute.subnet); // XXX:XXX:XXX:XXX <br>
  
  HTTPAddPGMCharToBuffer(&(UECShtmlLAN3C[0]));  // gateway: 
  HTTPprintIPtoHtml( U_orgAttribute.gateway); // XXX:XXX:XXX:XXX <br>
  
  HTTPAddPGMCharToBuffer(&(UECShtmlLAN3D[0]));  // dns: 
  HTTPprintIPtoHtml( U_orgAttribute.dns); // XXX:XXX:XXX:XXX <br>

  HTTPAddPGMCharToBuffer(&(UECShtmlLAN3E[0]));  // mac: 
  sprintf(UECStempStr20, "%02X%02X%02X%02X%02X%02X", U_orgAttribute.mac[0], U_orgAttribute.mac[1], U_orgAttribute.mac[2], U_orgAttribute.mac[3], U_orgAttribute.mac[4], U_orgAttribute.mac[5]);
  UDPAddCharToBuffer(UECStempStr20);

  /*
  HTTPAddPGMCharToBuffer(UECShtmlH2TAG);// <H2>
  HTTPAddPGMCharToBuffer(UECShtmlUECSTITLE); // UECS
  HTTPAddPGMCharToBuffer(UECShtmlH2CLOSE); // </H2>
  
  HTTPAddPGMCharToBuffer(UECShtmlRoom);   
  HTTPsetInput( U_orgAttribute.room);  
  HTTPAddPGMCharToBuffer(UECShtmlRegion);
  HTTPsetInput( U_orgAttribute.region);  
  HTTPAddPGMCharToBuffer(UECShtmlOrder);
  HTTPsetInput( U_orgAttribute.order);
  HTTPAddPGMCharToBuffer(&(UECSbtrag[0]));
  */

  HTTPAddPGMCharToBuffer(&(UECShtmlUECSID[0]));  // uecsid:
  UDPAddPGMCharToBuffer(&(U_uecsid[0]));
  
  HTTPAddPGMCharToBuffer(UECShtmlH2TAG);// <H2>
  HTTPAddPGMCharToBuffer(UECShtmlNAMETITLE); //Node Name
  HTTPAddPGMCharToBuffer(UECShtmlH2CLOSE); // </H2>
  
  HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));
  HTTPAddCharToBuffer(U_nodename);
  HTTPAddPGMCharToBuffer(UECShtmlINPUTCLOSE19);
  HTTPAddPGMCharToBuffer(&(UECSbtrag[0]));  
  
  HTTPAddPGMCharToBuffer(&(UECShtmlSUBMIT[0])); // <input name=\"b\" type=\"submit\" value=\"send\">
  HTTPAddPGMCharToBuffer(&(UECSformend[0]));   //</form>
  
    if(U_orgAttribute.status & STATUS_NEEDRESET)
		 {
		 	HTTPAddPGMCharToBuffer(&(UECShtmlATTENTION_RESET[0]));
		 }
		 
  
  HTTPAddPGMCharToBuffer(&(UECShtmlRETURNINDEX[0]));
  HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0]));    //</BODY></HTML>
  HTTPCloseBuffer();

}

//--------------------------------------------------
void HTTPsendPageCCM(){

 HTTPPrintHeader();
HTTPAddCharToBuffer(U_nodename); 
HTTPAddPGMCharToBuffer(&(UECShtmlCCMRecRes0[0])); // </H1><H2>CCM Status</H2><TABLE border=\"1\"><TBODY align=\"center\"><TR><TH>Info</TH><TH>S/R</TH><TH>Type</TH><TH>SR Lev</TH><TH>Value</TH><TH>Valid</TH><TH>Sec</TH><TH>Atr</TH><TH>IP</TH></TR>

 for(int i = 0; i < U_MAX_CCM; i++){
 if(U_ccmList[i].ccmLevel != NONE){
 	
HTTPAddPGMCharToBuffer(&(UECStrtd[0])); //<tr><td>

HTTPAddPGMCharToBuffer(U_ccmList[i].name); 
HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
 if(U_ccmList[i].sender){
 HTTPAddPGMCharToBuffer(UECSTxtPartS);
 }else{
 HTTPAddPGMCharToBuffer(UECSTxtPartR);
 } 

HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>

HTTPAddCharToBuffer(U_ccmList[i].typeStr);
HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 

HTTPAddPGMCharToBuffer((UECSCCMLEVEL[U_ccmList[i].ccmLevel])); //******

HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 

 dtostrf(((double)U_ccmList[i].value) / pow(10, U_ccmList[i].decimal), -1, U_ccmList[i].decimal,UECStempStr20);
HTTPAddCharToBuffer(UECStempStr20);
HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>

 if(U_ccmList[i].sender)
 {/*
	 if(U_ccmList[i].validity)
		 {HTTPAddPGMCharToBuffer(UECSTxtPartSend);}
	 else
	 	{HTTPAddPGMCharToBuffer(UECSTxtPartStop);}
	 	*/
 }
 else
 {
	 if(U_ccmList[i].validity)
		 {HTTPAddPGMCharToBuffer(UECSTxtPartOK);}
	 else
	 	{HTTPAddPGMCharToBuffer(UECSTxtPartHyphen);}
 } 

HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 
 
if(U_ccmList[i].flagStimeRfirst && U_ccmList[i].sender == false)
 {
 	 if(U_ccmList[i].recmillis<36000000)
 	 	{HTTPAddValueToBuffer(U_ccmList[i].recmillis / 1000);}
 }
 else{
 	 //over 10hour
 } 

HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 
 
 if(U_ccmList[i].flagStimeRfirst && U_ccmList[i].sender == false){
 sprintf(UECStempStr20, "%d-%d-%d-%d", U_ccmList[i].attribute[AT_ROOM], U_ccmList[i].attribute[AT_REGI], U_ccmList[i].attribute[AT_ORDE], U_ccmList[i].attribute[AT_PRIO]);
HTTPAddCharToBuffer(UECStempStr20); 
 }
 sprintf(UECStempStr20, "(%d-%d-%d)", U_ccmList[i].baseAttribute[AT_ROOM], U_ccmList[i].baseAttribute[AT_REGI], U_ccmList[i].baseAttribute[AT_ORDE]); 
HTTPAddCharToBuffer(UECStempStr20);
HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 
 
 
HTTPAddPGMCharToBuffer(&(UECSAHREF[0])); 
 sprintf(UECStempStr20, "%d.%d.%d.%d", U_ccmList[i].address[0], U_ccmList[i].address[1], U_ccmList[i].address[2], U_ccmList[i].address[3]); 
HTTPAddCharToBuffer(UECStempStr20);
HTTPAddPGMCharToBuffer(&(UECSTagClose[0]));
HTTPAddCharToBuffer(UECStempStr20);
HTTPAddPGMCharToBuffer(&(UECSSlashA[0]));
 
HTTPAddPGMCharToBuffer(&(UECStdtr[0])); //</td></tr> 
  
 } //NONE route
 }
 
 
HTTPAddPGMCharToBuffer(&(UECShtmlTABLECLOSE[0])); //</TBODY></TABLE>
 
if(U_HtmlLine>0) {
HTTPAddPGMCharToBuffer(&(UECShtmlUserRes0[0])); // </H1><H2>Status</H2><TABLE border=\"1\"><TBODY align=\"center\"><TR><TH>Name</TH><TH>Val</TH><TH>Unit</TH><TH>Detail</TH></TR>

 
 for(int i = 0; i < U_HtmlLine; i++)
	{
		
		HTTPAddPGMCharToBuffer(&(UECStrtd[0]));
		HTTPAddPGMCharToBuffer(U_html[i].name);
		HTTPAddPGMCharToBuffer(&(UECStdtd[0]));
 // only value
 
 if(U_html[i].pattern == UECSSHOWDATA)
		{
	 if(U_html[i].decimal != 0){
	 dtostrf(((double)*U_html[i].data) / pow(10, U_html[i].decimal), -1, U_html[i].decimal,UECStempStr20);
	 }else{
	 sprintf(UECStempStr20, "%ld", *(U_html[i].data));
	 }
	HTTPAddCharToBuffer(UECStempStr20);
	HTTPAddPGMCharToBuffer(&(UECShtmlInputHidden0[0]));
 }
		else if(U_html[i].pattern == UECSINPUTDATA)
			{
	HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));
	 dtostrf(((double)*U_html[i].data) / pow(10, U_html[i].decimal), -1, U_html[i].decimal,UECStempStr20);
	HTTPAddCharToBuffer(UECStempStr20);
	HTTPAddPGMCharToBuffer(UECSSlashTagClose);
		 }
		else if(U_html[i].pattern == UECSSELECTDATA)
			{
	HTTPAddPGMCharToBuffer(&(UECShtmlSelect[0]));

	 
	 for(int j = 0; j < U_html[i].selectnum; j++)
	 {
	 
	HTTPAddPGMCharToBuffer(&(UECShtmlOption[0]));


	HTTPAddValueToBuffer(j);
	 if((int)(*U_html[i].data) == j)
			 	{
			    HTTPAddPGMCharToBuffer(UECSTxtPartSelected);
			 	}
			 		else
			 	{ 
			    HTTPAddPGMCharToBuffer(UECSTagClose);
			 	}
	HTTPAddPGMCharToBuffer(U_html[i].selectname[j]);//************
	 } 
	HTTPAddPGMCharToBuffer(&(UECShtmlSelectEnd[0])); 
	 }
		else if(U_html[i].pattern == UECSSHOWSTRING)
			{
	HTTPAddPGMCharToBuffer(U_html[i].selectname[(int)*(U_html[i].data)]);//************
	HTTPAddPGMCharToBuffer(&(UECShtmlInputHidden0[0])); 
		 }

 
 
HTTPAddPGMCharToBuffer(&(UECStdtd[0])); 
HTTPAddPGMCharToBuffer(U_html[i].unit); 
HTTPAddPGMCharToBuffer(&(UECStdtd[0])); 
HTTPAddPGMCharToBuffer(U_html[i].detail); 
HTTPAddPGMCharToBuffer(&(UECStdtr[0])); //</td></tr> 
  
 }


HTTPAddPGMCharToBuffer(&(UECShtmlTABLECLOSE[0])); //</TBODY></TABLE>
HTTPAddPGMCharToBuffer(&(UECShtmlSUBMIT[0])); // <input name=\"b\" type=\"submit\" value=\"send\">
HTTPAddPGMCharToBuffer(&(UECSformend[0])); //</form> 
 
}
HTTPAddPGMCharToBuffer(&(UECShtmlRETURNINDEX[0])); //<P align=\"center\">return <A href=\"index.htm\">Top</A></P>
HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0])); 
 
  
 HTTPCloseBuffer();
}
//--------------------------------------------------
void HTTPsendPageEDITCCM(int ccmid){


HTTPPrintHeader();
HTTPAddCharToBuffer(U_nodename);

HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMTableHeader[0]));

//----------
 for(int i = 0; i < U_MAX_CCM; i++){
// if(U_ccmList[i].ccmLevel != NONE)

	HTTPAddPGMCharToBuffer(&(UECStrtd[0])); //<tr><td>

	//table 2nd start
//	HTTPAddPGMCharToBuffer(&(UECStdtd[0]));
	//ccm name
	HTTPAddPGMCharToBuffer(U_ccmList[i].name);
	HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
	//SR
	 if(U_ccmList[i].sender){
	 	HTTPAddPGMCharToBuffer(UECSTxtPartS);
	 }else{
	 	HTTPAddPGMCharToBuffer(UECSTxtPartR);
	 } 
	//level
	HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
	HTTPAddPGMCharToBuffer((UECSCCMLEVEL[U_ccmList[i].ccmLevel]));
	
	//unit
	HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
	HTTPAddPGMCharToBuffer(U_ccmList[i].unit);
	HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
	
				if(i==ccmid && U_ccmList[i].ccmLevel != NONE)
				{
				HTTPAddPGMCharToBuffer(&(UECShtmlInputHiddenValue[0])); //hidden value(ccmid)
				sprintf(UECStempStr20,"%d",i);
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(&(UECSSlashTagClose[0]));
					
				//room region order
				HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));
				sprintf(UECStempStr20,"%d",U_ccmList[i].baseAttribute[AT_ROOM]);
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(UECShtmlINPUTCLOSE3);

				HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));
				sprintf(UECStempStr20,"%d",U_ccmList[i].baseAttribute[AT_REGI]);
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(UECShtmlINPUTCLOSE3);

				HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));
				sprintf(UECStempStr20,"%d",U_ccmList[i].baseAttribute[AT_ORDE]);
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(UECShtmlINPUTCLOSE3);
				
				//priority
				HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));
				sprintf(UECStempStr20,"%d",U_ccmList[i].baseAttribute[AT_PRIO]);
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(UECShtmlINPUTCLOSE3);
				HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
				
				//TypeInput
				HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));
				HTTPAddCharToBuffer(U_ccmList[i].typeStr);
				HTTPAddPGMCharToBuffer(UECShtmlINPUTCLOSE19);

				HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
				//default
				HTTPAddPGMCharToBuffer(U_ccmList[i].type);

				HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
				//submit btn
				HTTPAddPGMCharToBuffer(&(UECShtmlSUBMIT[0])); 
				}
		else
				{
				sprintf(UECStempStr20,"%d-%d-%d-%d",U_ccmList[i].baseAttribute[AT_ROOM],U_ccmList[i].baseAttribute[AT_REGI],U_ccmList[i].baseAttribute[AT_ORDE],U_ccmList[i].baseAttribute[AT_PRIO]);
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
				HTTPAddCharToBuffer(U_ccmList[i].typeStr);
				HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
				HTTPAddPGMCharToBuffer(U_ccmList[i].type);
				HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
				
				//Edit Link
				HTTPAddPGMCharToBuffer(&(UECSAHREF3[0]));
				sprintf(UECStempStr20,"%d",i);
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(&(UECSTagClose[0]));
				HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMEditTxt[0]));
				HTTPAddPGMCharToBuffer(&(UECSSlashA[0])); //</a> 
				}
				HTTPAddPGMCharToBuffer(&(UECStdtr[0])); //</td><tr>

	 }

//-------------

HTTPAddPGMCharToBuffer(&(UECShtmlTABLECLOSE[0])); 
HTTPAddPGMCharToBuffer(&(UECSformend[0]));

HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMCmdBtn1[0]));
sprintf(UECStempStr20,"%d-%d-%d-%d",U_ccmList[ccmid].baseAttribute[AT_ROOM],U_ccmList[ccmid].baseAttribute[AT_REGI],U_ccmList[ccmid].baseAttribute[AT_ORDE],U_ccmList[ccmid].baseAttribute[AT_PRIO]);
HTTPAddCharToBuffer(UECStempStr20);
HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMCmdBtn2[0]));


HTTPAddPGMCharToBuffer(&(UECShtmlRETURNINDEX[0])); 

//Javascript
HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMCmdBtnScript1[0])); 
sprintf(UECStempStr20,"%d",ccmid+100);
HTTPAddCharToBuffer(UECStempStr20);
HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMCmdBtnScript2[0])); 

HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0])); 
 
  
 HTTPCloseBuffer();
}
//---------------------------------------------####################
//---------------------------------------------####################
//---------------------------------------------####################
void HTTPGetFormDataCCMPage()
{
if(U_HtmlLine==0){return;}

int i;
int startPos=0;
int progPos=0;
long tempValue;
unsigned char tempDecimal;


for(i=0;i<U_HtmlLine;i++)
	{
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){continue;}
	startPos+=progPos;

		if(U_html[i].pattern == UECSINPUTDATA)
				{
				
				if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&tempValue,&tempDecimal,&progPos)){return;}
				startPos+=progPos;
				if(UECSbuffer[startPos]!='&'){return;}//last '&' not found
				
			    int dif_decimal=U_html[i].decimal-tempDecimal;
			    if(dif_decimal>=0){*U_html[i].data = tempValue*pow(10,dif_decimal);}
			    else{*U_html[i].data = tempValue/pow(10,-dif_decimal);}

				if(*U_html[i].data<U_html[i].minValue){*U_html[i].data=U_html[i].minValue;}
				if(*U_html[i].data>U_html[i].maxValue){*U_html[i].data=U_html[i].maxValue;}
				}
		else if(U_html[i].pattern == UECSSELECTDATA)
				{
				
				if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&tempValue,&tempDecimal,&progPos)){return;}
				startPos+=progPos;
				
				if(UECSbuffer[startPos]!='&'){return;}//last '&' not found
				
				if(tempDecimal!=0){return;}
				*U_html[i].data=tempValue%U_html[i].selectnum;//cut too big value
				}

	
				
				
	}
	
	
	OnWebFormRecieved();
	
for(i = 0; i < U_HtmlLine; i++)
	       {UECS_EEPROM_writeLong(EEPROM_WEBDATA + i * 4, *(U_html[i].data));}


}
int HTTPGetFormDataEDITCCMPage()
{
	int i;
	int startPos=0;
	int progPos=0;
	unsigned char tempDecimal;
	long ccmid,room,region,order,priority;

	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return -1;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&ccmid,&tempDecimal,&progPos)){return -1;}
	startPos+=progPos;
	//if(UECSbuffer[startPos]!='&'){return -1;}//last '&' not found
	if(tempDecimal!=0){return -1;}
	
	//Room
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&room,&tempDecimal,&progPos)){return ccmid;}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){return ccmid;}//last '&' not found
	if(tempDecimal!=0){return ccmid;}
	room=constrain(room,0,127);
	
	//Region
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&region,&tempDecimal,&progPos)){return ccmid;}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){return ccmid;}//last '&' not found
	if(tempDecimal!=0){return ccmid;}
	region=constrain(region,0,127);
	
	//Order
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&order,&tempDecimal,&progPos)){return ccmid;}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){return ccmid;}//last '&' not found
	if(tempDecimal!=0){return ccmid;}
	order=constrain(order,0,30000);
	
	//Priority
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&priority,&tempDecimal,&progPos)){return ccmid;}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){return ccmid;}//last '&' not found
	if(tempDecimal!=0){return ccmid;}
	priority=constrain(priority,0,30);
	
	U_ccmList[ccmid].baseAttribute[AT_ROOM]=room;
	U_ccmList[ccmid].baseAttribute[AT_REGI]=region;
	U_ccmList[ccmid].baseAttribute[AT_ORDE]=order;
	U_ccmList[ccmid].baseAttribute[AT_PRIO]=priority;
	
	UECS_EEPROM_SaveCCMAttribute(ccmid);
	
	 //---------------------------Type
     if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	
	//copy type
	int count=0;
	for(i=0;i<MAX_CCMTYPESIZE;i++)
		{
		if(UECSbuffer[startPos+i]=='&'){break;}
		if(UECSbuffer[startPos+i]=='\0' || i==MAX_CCMTYPESIZE){return ccmid;}//���[���Ȃ�

		if( (UECSbuffer[startPos+i]>='0' && UECSbuffer[startPos+i]<='9')||
			(UECSbuffer[startPos+i]>='A' && UECSbuffer[startPos+i]<='Z')||
			(UECSbuffer[startPos+i]>='a' && UECSbuffer[startPos+i]<='z')||
			 UECSbuffer[startPos+i]=='.' || UECSbuffer[startPos+i]=='_' )
			{
			}
		else
			{UECSbuffer[startPos+i]='x';}

		UECStempStr20[i]=UECSbuffer[startPos+i];
		count++;
		}
		
		UECStempStr20[i]='\0';//set end code

	
	if(count>=3 && count<=19)
		{
		
		strcpy(U_ccmList[ccmid].typeStr,UECStempStr20);
		UECS_EEPROM_SaveCCMType(ccmid);
		}
	
	
	return ccmid;
}

//--------------------------------------------------------------------------
void HTTPGetFormDataLANSettingPage()
{
	int i;
	int startPos=0;
	int progPos=0;
	long UECStempValue[16];
	unsigned char tempDecimal;
	//
	//MYIP      4
	//SUBNET    4
	//GATEWAY   4
	//DNS       4
	//-------------
	//total    16

	//get value
	for(i=0;i<16;i++)
	{
		if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ;}
		startPos+=progPos;
		if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&UECStempValue[i],&tempDecimal,&progPos)){return ;}
		startPos+=progPos;
		if(UECSbuffer[startPos]!='&'){return ;}//last '&' not found
		if(tempDecimal!=0){return ;}
		
		//check value and write
		if(UECStempValue[i]<0 || UECStempValue[i]>255){return ;}//IP address
	}

       for(int i = 0; i < 16; i++)
       		{
           	if(EEPROM.read(EEPROM_DATATOP + i)!=(unsigned char)UECStempValue[i])//skip same value
           		{
           	        EEPROM.write(EEPROM_DATATOP + i, (unsigned char)UECStempValue[i]);
           	        U_orgAttribute.status |= STATUS_NEEDRESET;
           	        
           		}
       		}
       		
       	//---------------------------NODE NAME
       	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ;}
		startPos+=progPos;
		
		//copy node name
		for(i=0;i<20;i++)
		{
		
		if(UECSbuffer[startPos+i]=='<' || UECSbuffer[startPos+i]=='>')//eliminate tag
			{UECSbuffer[startPos+i]='*';}
			
			if(UECSbuffer[startPos+i]=='&'){break;}
			if(UECSbuffer[startPos+i]=='\0' || i==19){return;}//�I�[�������̂Ŗ���
			//prevention of Cutting multibyte UTF-8 code
			if(i>=16 && (unsigned char)UECSbuffer[startPos+i]>=0xC0)//UTF-8 multibyte code header
				{

				break;
				}

			UECStempStr20[i]=UECSbuffer[startPos+i];
		}

		UECStempStr20[i]='\0';//set end code

       for(int i = 0; i < 20; i++)
       		{
       		U_nodename[i]=UECStempStr20[i];
       			
           	if(EEPROM.read(EEPROM_NODENAME + i)!=U_nodename[i])//skip same value
           		{
           	        EEPROM.write(EEPROM_NODENAME + i, U_nodename[i]);
           		}
       		}

	return ;

}
//--------------------------------------------------------------------
void HTTPGetFormDataFillCCMAttributePage()
{
	int i;
	int startPos=0;
	int progPos=0;
	unsigned char tempDecimal;
	long ccmid;

	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&ccmid,&tempDecimal,&progPos)){return;}
	startPos+=progPos;
	if(tempDecimal!=0){return;}
	ccmid-=100;
	if(ccmid<0 || ccmid>=U_MAX_CCM){return;}
	
	for(i=0;i<U_MAX_CCM;i++)
	{
	U_ccmList[i].baseAttribute[AT_ROOM]=U_ccmList[ccmid].baseAttribute[AT_ROOM];
	U_ccmList[i].baseAttribute[AT_REGI]=U_ccmList[ccmid].baseAttribute[AT_REGI];
	U_ccmList[i].baseAttribute[AT_ORDE]=U_ccmList[ccmid].baseAttribute[AT_ORDE];
	U_ccmList[i].baseAttribute[AT_PRIO]=U_ccmList[ccmid].baseAttribute[AT_PRIO];
	UECS_EEPROM_SaveCCMAttribute(i);
	}
	
	

}
//---------------------------------------------####################
void HTTPcheckRequest(){
UECSclient=UECSlogserver.available();
  //Caution! This function can not receive only up to the top 299 bytes.
  //Dropped string will be ignored.
  
  if(UECSclient)
  {
      
      //Add null termination 
      UECSbuffer[UECSclient.read((uint8_t *)UECSbuffer,BUF_SIZE-1)]='\0';
      

      HTTPFilterToBuffer();//Get first line before "&S=" and eliminate unnecessary code
			int progPos = 0;
			if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP0[0]),&progPos))
			{HTTPsendPageIndex();}
		else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP1[0]),&progPos))
			{HTTPsendPageCCM();}
		else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP2[0]),&progPos))
			{HTTPsendPageLANSetting();}
		else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP1A[0]),&progPos))//include form data
				{
				HTTPGetFormDataCCMPage();
				HTTPPrintRedirect(1);
				}
		else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP2A[0]),&progPos))//include form data
				{
				HTTPGetFormDataLANSettingPage();
				HTTPsendPageLANSetting();
				}
		else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP3A[0]),&progPos))
				{
				int ccmid=HTTPGetFormDataEDITCCMPage();
				//Type Reset
				if(ccmid==999)
						{
						for(int i=0;i<U_MAX_CCM;i++)
									{
									strcpy_P(U_ccmList[i].typeStr, U_ccmList[i].type);
									UECS_EEPROM_SaveCCMType(i);
									}
						HTTPPrintRedirect(3);
						}
				//Attribute Reset
				else if(ccmid-100>=0 && ccmid-100<U_MAX_CCM)
						{
						HTTPGetFormDataFillCCMAttributePage();
						HTTPPrintRedirect(3);
						}
				//Err
				else if(ccmid<0 || ccmid>=U_MAX_CCM){HTTPsendPageError();}
				//CCM Edit
				else{HTTPsendPageEDITCCM(ccmid);}
				
				}
		/*
		else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP4[0]),&progPos))//reset CCM type
				{
				for(int i=0;i<U_MAX_CCM;i++)
					{
					strcpy_P(U_ccmList[i].typeStr, U_ccmList[i].type);
					UECS_EEPROM_SaveCCMType(i);
					}
					
					HTTPPrintRedirectP3();
				}
		else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP5A[0]),&progPos))//reset CCM type
				{
					HTTPGetFormDataFillCCMAttributePage();
					HTTPPrintRedirectP3();
				}*/
		else {HTTPsendPageError();}
  }
  UECSclient.stop();
}



//----------------------------------
void UECSupdate16520portReceive( UECSTEMPCCM* _tempCCM, unsigned long _millis){
  int packetSize = UECS_UDP16520.parsePacket();
  int matchCCMID=-1;
  if(packetSize){
  	  
   	ClearMainBuffer();
    _tempCCM->address = UECS_UDP16520.remoteIP();   
    UECSbuffer[UECS_UDP16520.read(UECSbuffer, BUF_SIZE-1)]='\0';
    UDPFilterToBuffer();

    if(UECSparseRec( _tempCCM,&matchCCMID))
    	{UECScheckUpDate( _tempCCM, _millis,matchCCMID);}


  }

}
//----------------------------------
void UECSupdate16529port( UECSTEMPCCM* _tempCCM){

  int packetSize = UECS_UDP16529.parsePacket();
   if(packetSize){
   	   
   	   ClearMainBuffer();
      _tempCCM->address = UECS_UDP16529.remoteIP();   
      UECSbuffer[UECS_UDP16529.read(UECSbuffer, BUF_SIZE-1)]='\0';
	  UDPFilterToBuffer();
	  
	  
      if(UECSresNodeScan()){
         UECS_UDP16529.beginPacket(_tempCCM->address, 16529);
         UECS_UDP16529.write(UECSbuffer);
         
         if(UECS_UDP16529.endPacket()==0)
         	{
  			UECSresetEthernet(); //when udpsend failed,reset ethernet status
         	}
      }     
   }
}

//----------------------------------
void UECSupdate16521port( UECSTEMPCCM* _tempCCM){

  int packetSize = UECS_UDP16521.parsePacket();
   if(packetSize){
   	   
   	   ClearMainBuffer();
      _tempCCM->address = UECS_UDP16521.remoteIP();   
      UECSbuffer[UECS_UDP16521.read(UECSbuffer, BUF_SIZE-1)]='\0';
	  UDPFilterToBuffer();
	  UECSresCCMSearchAndSend(_tempCCM);
   }
}


//----------------------------------


void UECSsetup(){

UECSCheckProgramUpdate();
delay(300);


pinMode(U_InitPin,INPUT_PULLUP);

if(digitalRead(U_InitPin) == U_InitPin_Sense || UECS_EEPROM_readLong(EEPROM_IP)==-1)
	{
	U_orgAttribute.status|=STATUS_SAFEMODE;
    }
	 else
	 {
	 U_orgAttribute.status&=STATUS_SAFEMODE_MASK;//Release safemode
	 }

  UECSinitOrgAttribute();
  UECSinitCCMList();
  UserInit();
  UECSstartEthernet();
  
}
//---------------------------------------------
void UECSCheckProgramUpdate()
{
//Program Upadate Check
ClearMainBuffer();
UDPAddPGMCharToBuffer(&(ProgramDate[0]));
UDPAddPGMCharToBuffer(&(ProgramTime[0]));
//check and write datetime
int i;
for(i=0;i<(EEPROM_CCMTOP-EEPROM_PROGRAMDATETIME);i++)
	{
	
	if(EEPROM.read(i+EEPROM_PROGRAMDATETIME)!=UECSbuffer[i])
		{
		U_orgAttribute.status|=STATUS_PROGRAMUPDATE;
		EEPROM.write(i+EEPROM_PROGRAMDATETIME,UECSbuffer[i]);
		}
	if(UECSbuffer[i]=='\0'){break;}
	}
}
//---------------------------------------------

//--------------------------------------------------------------
void UECS_EEPROM_SaveCCMType(int ccmid)
{
#if defined(_ARDUINIO_MEGA_SETTING)
if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory
#endif

int i;
//type��������
for(i=0;i<=MAX_CCMTYPESIZE;i++)
	{
	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i)!=U_ccmList[ccmid].typeStr[i])
		{
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i,U_ccmList[ccmid].typeStr[i]);
		}
	if(U_ccmList[ccmid].typeStr[i]=='\0'){break;}
	}
}
//--------------------------------------------------------------
void UECS_EEPROM_SaveCCMAttribute(int ccmid)
{
#if defined(_ARDUINIO_MEGA_SETTING)
if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory
#endif

if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ROOM]))
	{EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ROOM]));}

if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_REGI]))
	{EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_REGI]));}

if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]))
	{EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]));}

if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]/256))
	{EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]/256));}

if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_PRIO]))
	{EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_PRIO]));}

}

//--------------------------------------------------------------
void UECS_EEPROM_LoadCCMSetting(int ccmid)
{
#if defined(_ARDUINIO_MEGA_SETTING)
if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory
#endif

int i;
for(i=0;i<MAX_CCMTYPESIZE;i++)
	{
	U_ccmList[ccmid].typeStr[i]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i);
	if(U_ccmList[ccmid].typeStr[i]==255){U_ccmList[ccmid].typeStr[i]='x';break;}
	if(U_ccmList[ccmid].typeStr[i]=='\0'){break;}
	}
	U_ccmList[ccmid].typeStr[i]='\0';
	
/*
U_ccmList[ccmid].baseAttribute[AT_ROOM]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM) & 127;
U_ccmList[ccmid].baseAttribute[AT_REGI]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI) & 127;
U_ccmList[ccmid].baseAttribute[AT_ORDE]=
	(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L)+
	EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H)*256) & 32767;
U_ccmList[ccmid].baseAttribute[AT_PRIO]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO) & 31;
U_ccmList[ccmid].attribute[AT_PRIO] =U_ccmList[ccmid].baseAttribute[AT_PRIO];
*/
U_ccmList[ccmid].baseAttribute[AT_ROOM]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM);
U_ccmList[ccmid].baseAttribute[AT_REGI]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI);
U_ccmList[ccmid].baseAttribute[AT_ORDE]=(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L)+EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H)*256);
U_ccmList[ccmid].baseAttribute[AT_PRIO]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO);

//Prepare the correct values for the Arduino where the data will be written for the first time.
if(U_ccmList[ccmid].baseAttribute[AT_ROOM]==0xff)
	{
	U_ccmList[ccmid].baseAttribute[AT_ROOM]=1;
	U_ccmList[ccmid].baseAttribute[AT_REGI]=1;
	U_ccmList[ccmid].baseAttribute[AT_ORDE]=1;
	U_ccmList[ccmid].baseAttribute[AT_PRIO]=U_ccmList[ccmid].attribute[AT_PRIO];
	UECS_EEPROM_SaveCCMAttribute(ccmid);
	}
U_ccmList[ccmid].attribute[AT_PRIO] =U_ccmList[ccmid].baseAttribute[AT_PRIO];

}

//---------------------------------------------
void UECSstartEthernet(){
  
  if(U_orgAttribute.status&STATUS_SAFEMODE)
  	{
  	byte defip[]     = {192,168,1,7};
  	//byte defsubnet[] = {255,255,255,0};
  	byte defdummy[] = {0,0,0,0};
  	Ethernet.begin(U_orgAttribute.mac, defip, defdummy,defdummy,defdummy);
  	}
  	else
    {
    Ethernet.begin(U_orgAttribute.mac, U_orgAttribute.ip, U_orgAttribute.dns, U_orgAttribute.gateway, U_orgAttribute.subnet);
    }

  UECSlogserver.begin();
  UECS_UDP16520.begin(16520);
  UECS_UDP16529.begin(16529);
  UECS_UDP16521.begin(16521);
}

//---------------------------------------------------------
void UECSresetEthernet()
{
  	//UECS_UDP16520.stop();
  	//UECS_UDP16529.stop();
  	//SPI.end();
  	UECSstartEthernet();
}
//------------------------------------------------------------------------
//Software reset command
// call 0
typedef int (*CALLADDR)(void);
void SoftReset(void)
{
CALLADDR resetAddr=0;
(resetAddr)();
}
//---------------------------------------------------------
//---------------------------------------------------------
//----------------------------------------------------------------------

void UECSloop(){

  // network Check
  // 1. http request
  // 2. udp16520Receive
  // 3. udp16529Receive and Send
  // 4. udp16521Receive and Send
  // << USER MANAGEMENT >>
  // 5. udp16520Send
  HTTPcheckRequest();
  UECSupdate16520portReceive(&UECStempCCM, UECSnowmillis);
  UECSupdate16529port(&UECStempCCM);
  UECSupdate16521port(&UECStempCCM);
  UserEveryLoop();  

 UECSnowmillis = millis();
 if(UECSnowmillis==UECSlastmillis){return;}
 
//how long elapsed?
unsigned long td=UECSnowmillis-UECSlastmillis;
//check overflow 
if(UECSnowmillis<UECSlastmillis)
	{
	td=(4294967295-UECSlastmillis)+UECSnowmillis;
	}

//over 1msec
	UECSsyscounter1s+=td;
	UECSlastmillis=UECSnowmillis;
	UECSautomaticValidManager(td);


  if(UECSsyscounter1s < 999){return;}
//over 1000msec
    UECSsyscounter1s = 0;
    UECSsyscounter60s++;
    
    if(UECSsyscounter60s >= 60)
    {
      UserEveryMinute();
      UECSsyscounter60s = 0;  
    }   
    
    
    UECSautomaticSendManager();
    UserEverySecond();
    
    for(int i = 0; i < U_MAX_CCM; i++)
      {
     if(U_ccmList[i].sender && U_ccmList[i].flagStimeRfirst && U_ccmList[i].ccmLevel != NONE)
     	{
        UECSCreateCCMPacketAndSend(&U_ccmList[i]);
       }

    }   

}
//------------------------------------------------------
//
void UECSinitOrgAttribute(){

		//接続情報を把握する
	  for(int i = 0; i < 4; i++)
	  	{
	  	U_orgAttribute.ip[i]		= EEPROM.read(i + EEPROM_IP);
	  	U_orgAttribute.subnet[i]	= EEPROM.read(i + EEPROM_SUBNET);
	  	U_orgAttribute.gateway[i]	= EEPROM.read(i + EEPROM_GATEWAY);
	  	U_orgAttribute.dns[i]		= EEPROM.read(i + EEPROM_DNS);
	  	}


	//reset web form
	  	for(int i = 0; i < U_HtmlLine; i++)
	  	{
	     *(U_html[i].data) = U_html[i].minValue;
	 	} 


//	  U_orgAttribute.room 	=  EEPROM.read(EEPROM_ROOM);
//	  U_orgAttribute.region =  EEPROM.read(EEPROM_REGION);
//	  U_orgAttribute.order 	=  EEPROM.read(EEPROM_ORDER_L)+ (unsigned short)(EEPROM.read(EEPROM_ORDER_H))*256;
//	 if(U_orgAttribute.order>30000){U_orgAttribute.order=30000;}
//セーフモードの時は終了
if(U_orgAttribute.status & STATUS_SAFEMODE){return;}

		//nodenameの確認
	  for(int i = 0; i < 20; i++)
	  	  {
		     U_nodename[i] = EEPROM.read(EEPROM_NODENAME + i);
		  }
		  
	 for(int i = 0; i < U_HtmlLine; i++)
		 {
	     *(U_html[i].data) = UECS_EEPROM_readLong(EEPROM_WEBDATA + i * 4);
		 } 
}
//------------------------------------------------------
void UECSinitCCMList(){
  for(int i = 0; i < U_MAX_CCM; i++){
    U_ccmList[i].ccmLevel = NONE;
    U_ccmList[i].validity = false;
    U_ccmList[i].flagStimeRfirst = false;
    U_ccmList[i].recmillis = 0; 
  }
}
/*
signed char UECSsetCCM(boolean _sender,
signed char _num,
const char* _name,
const char* _type,
const char* _unit,
unsigned short _room,
unsigned short _region,
unsigned short _order,
unsigned short _priority,
unsigned char _decimal,
char _ccmLevel)
{
 
}
*/
void UECSsetCCM(boolean _sender, signed char _num, const char* _name, const char* _type, const char* _unit, unsigned short _priority, unsigned char _decimal, char _ccmLevel)
{
 if(_num > U_MAX_CCM || _num < 0){
    return;
  }
  U_ccmList[_num].sender = _sender;
  U_ccmList[_num].ccmLevel = _ccmLevel;
  U_ccmList[_num].name = _name;    
  U_ccmList[_num].type = _type;
  U_ccmList[_num].unit = _unit;
  U_ccmList[_num].decimal = _decimal;
  U_ccmList[_num].ccmLevel = _ccmLevel;
  U_ccmList[_num].address[0] = 255;
  U_ccmList[_num].address[1] = 255;
  U_ccmList[_num].address[2] = 255;
  U_ccmList[_num].address[3] = 255;
  U_ccmList[_num].attribute[AT_ROOM] = 0;
  U_ccmList[_num].attribute[AT_REGI] = 0;
  U_ccmList[_num].attribute[AT_ORDE] = 0;
  U_ccmList[_num].attribute[AT_PRIO] = _priority;
//  U_ccmList[_num].baseAttribute[AT_ROOM] = 1;
//  U_ccmList[_num].baseAttribute[AT_REGI] = 1;
//  U_ccmList[_num].baseAttribute[AT_ORDE] = 1;
  U_ccmList[_num].baseAttribute[AT_PRIO] = _priority;
  strcat_P(U_ccmList[_num].typeStr,U_ccmList[_num].type);

if(U_orgAttribute.status&STATUS_PROGRAMUPDATE)
	{
  	UECS_EEPROM_SaveCCMType(_num);
	}

  	UECS_EEPROM_LoadCCMSetting(_num);

  return;
	
	
//  return  UECSsetCCM(_sender, _num, _name, _type, _unit, U_orgAttribute.room, U_orgAttribute.region, U_orgAttribute.order, _priority, _decimal, _ccmLevel);
}


//####################################String Buffer control
static int wp;
void ClearMainBuffer()
{
for(int i=0;i<BUF_SIZE;i++)
	{UECSbuffer[i]='\0';}
wp=0;
}
//-----------------------------------
void UDPAddPGMCharToBuffer(const char* _romword)
{
   for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++)
   {
    UECSbuffer[wp]=pgm_read_byte(&_romword[i]);
    if(UECSbuffer[wp]=='\0'){break;}
    wp++;
  }
  #if defined(_ARDUINIO_MEGA_SETTING)
      MemoryWatching();
  #endif

}
//-----------------------------------
void UDPAddCharToBuffer(char* word){
	strcat(UECSbuffer,word);
	wp=strlen(UECSbuffer);
  #if defined(_ARDUINIO_MEGA_SETTING)
	 MemoryWatching();
  #endif

}
//-----------------------------------
void UDPAddValueToBuffer(long value){
	sprintf(&UECSbuffer[wp], "%ld", value);
	wp=strlen(UECSbuffer);
	
  #if defined(_ARDUINIO_MEGA_SETTING)
	MemoryWatching();
  #endif
}
//-----------------------------------
void HTTPAddPGMCharToBuffer(const char* _romword)
{
   for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++)
   {
    UECSbuffer[wp]=pgm_read_byte(&_romword[i]);
    if(UECSbuffer[wp]=='\0'){break;}
    wp++;
    //auto send
    if(wp>BUF_HTTP_REFRESH_SIZE)
    		{
			HTTPRefreshBuffer();
    		}
  }
  
  #if defined(_ARDUINIO_MEGA_SETTING)
      MemoryWatching();
  #endif
}
//---------------------------------------------
void HTTPAddCharToBuffer(char* word)
{
   for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++)
   {
    UECSbuffer[wp]=word[i];
    if(UECSbuffer[wp]=='\0'){break;}
    wp++;
    //auto send
    if(wp>BUF_HTTP_REFRESH_SIZE)
    		{
			HTTPRefreshBuffer();
    		}
  }
  #if defined(_ARDUINIO_MEGA_SETTING)
	    MemoryWatching();
  #endif

}
//---------------------------------------------
void HTTPAddValueToBuffer(long value)
{
sprintf(&UECSbuffer[wp], "%ld", value);
wp=strlen(UECSbuffer);

if(wp>BUF_HTTP_REFRESH_SIZE)
    		{
			HTTPRefreshBuffer();
    		}
   
  #if defined(_ARDUINIO_MEGA_SETTING)
   MemoryWatching();
  #endif
}
void HTTPRefreshBuffer(void)
{
	
	
    UECSbuffer[wp]='\0';
    UECSclient.print(UECSbuffer);
    ClearMainBuffer();
    

}
void HTTPCloseBuffer(void)
{
	
if(strlen(UECSbuffer)>0)
	{UECSclient.print(UECSbuffer);}

}
//------------------------------------
//delete \r,\n and contorol code
//replace the continuous space character to 1 space
//Attention:first one character is ignored without delete.
//------------------------------------
void UDPFilterToBuffer(void)
{

int s_size=strlen(UECSbuffer);
int write=0,read=0;
int i,j;
  for(i=1;i<s_size;i++)
  {
    if(UECSbuffer[i]=='\0'){break;}
    
    //find space
    if(UECSbuffer[i]<ASCIICODE_SPACE || (UECSbuffer[i]==ASCIICODE_SPACE && UECSbuffer[i-1]==ASCIICODE_SPACE))
      {
        write=i;
        //find end of space
        for(j=write;j<=s_size;j++)
          {
          if(UECSbuffer[j]>ASCIICODE_SPACE || UECSbuffer[j]=='\0')
            {
            read=j;
            break;
            }
          }
        //copy str to fill space
        for(j=read;j<=s_size;j++)
          {
          UECSbuffer[write+(j-read)]=UECSbuffer[j];
          }
        }
        
        
  }
  #if defined(_ARDUINIO_MEGA_SETTING)
  MemoryWatching();
  #endif
}
//------------------------------------
//delete space and contorol code for http response
//\r,\n is regarded to end
//Attention:first one character is ignored without delete.
//decode URL like %nn to char code
//------------------------------------
void HTTPFilterToBuffer(void)
{

int s_size=strlen(UECSbuffer);
int write=0,read=0;
int i,j;

  //decode after %
  for(i=1;i<s_size;i++)
  {
    if((unsigned char)UECSbuffer[i]<ASCIICODE_SPACE || (UECSbuffer[i-1]=='&' && UECSbuffer[i]=='S' && UECSbuffer[i+1]=='=')){UECSbuffer[i]='\0';break;}
    
    if(UECSbuffer[i]=='%')
    	{
    	unsigned char decode=0;
    	if(UECSbuffer[i+1]>='A' && UECSbuffer[i+1]<='F'){decode=UECSbuffer[i+1]+10-'A';}
    	else if(UECSbuffer[i+1]>='a' && UECSbuffer[i+1]<='f'){decode=UECSbuffer[i+1]+10-'a';}
    	else if(UECSbuffer[i+1]>='0' && UECSbuffer[i+1]<='9'){decode=UECSbuffer[i+1]-'0';}
    	
    	if(decode!=0)
	    	{
	    	decode=decode*16;
	    	if(UECSbuffer[i+2]>='A' && UECSbuffer[i+2]<='F'){decode+=UECSbuffer[i+2]+10-'A';}
	    	else if(UECSbuffer[i+2]>='a' && UECSbuffer[i+2]<='f'){decode+=UECSbuffer[i+2]+10-'a';}
	    	else if(UECSbuffer[i+2]>='0' && UECSbuffer[i+2]<='9'){decode+=UECSbuffer[i+2]-'0';}
	    	else {decode=0;}

	    	if(decode!=0)
		    		{
		    		if(decode=='&'){decode='*';}
		    		UECSbuffer[i]=decode;
		    		UECSbuffer[i+1]=' ';
		    		UECSbuffer[i+2]=' ';
		    		}
	    	}
    	
    	}
  
  }

  s_size=strlen(UECSbuffer);
  
  for(i=1;i<s_size;i++)
  {
  	//eliminate tag
	//if(UECSbuffer[i]=='<' || UECSbuffer[i]=='>'){UECSbuffer[i]=' ';}
    
    //find space
    if(UECSbuffer[i]<=ASCIICODE_SPACE)
      {
        write=i;
        //find end of space
        for(j=write;j<=s_size;j++)
          {
          if((unsigned char)(UECSbuffer[j])>ASCIICODE_SPACE || UECSbuffer[j]=='\0')
            {
            read=j;
            break;
            }
          }
        //copy str to fill space
        for(j=read;j<=s_size;j++)
          {
          UECSbuffer[write+(j-read)]=UECSbuffer[j];
          }
        }
  }
  #if defined(_ARDUINIO_MEGA_SETTING)
  MemoryWatching();
  #endif
  
}

//------------------------------------
//指定した文字列を探す？
//
//
//
//
bool UECSFindPGMChar(char* targetBuffer,const char *_romword_startStr,int *lastPos)
{
//初期化
*lastPos=0;
int startPos=-1;
//文字列の大きさを把握
int _targetBuffersize=strlen(targetBuffer);
//引数のPROGMEMの長さを把握
int _startStrsize=strlen_P(_romword_startStr);
//
if(_targetBuffersize<_startStrsize){return false;}


int i,j;

//-------------start string check
unsigned char startchr=pgm_read_byte(&_romword_startStr[0]);
for(i=0;i<_targetBuffersize-_startStrsize+1;i++)
	{
	//not hit
	if(targetBuffer[i]!=startchr){continue;}
	
	//if hit 1 chr ,more check
	for(j=0;j<_startStrsize;j++)
			{
			if(targetBuffer[i+j]!=pgm_read_byte(&_romword_startStr[j])){break;}//not hit!
			}
	//hit all chr
	if(j==_startStrsize)
		{
		startPos=i;
		break;
		}
	
	}

if(startPos<0){return false;}
*lastPos=startPos+_startStrsize;
return true;

}

//------------------------------------
bool UECSGetValPGMStrAndChr(char* targetBuffer,const char *_romword_startStr, char end_asciiCode,short *shortValue,int *lastPos)
{
int _targetBuffersize=strlen(targetBuffer);
*lastPos=-1;
int startPos=-1;
int i;
if(!UECSFindPGMChar(targetBuffer,_romword_startStr,&startPos)){false;}



if(targetBuffer[startPos]<'0' || targetBuffer[startPos]>'9'){return false;}
//------------end code check
for(i=startPos;i<_targetBuffersize;i++)
{
if(targetBuffer[i]=='\0'){return false;}//no end code found
if(targetBuffer[i]==end_asciiCode)
	{break;}
	
}


if(i>=_targetBuffersize){return false;}//not found
*lastPos=i;

//*shortValue=UECSAtoI(&targetBuffer[startPos]);
long longVal;
unsigned char decimal;
int progPos;
if(!(UECSGetFixedFloatValue(&targetBuffer[startPos],&longVal,&decimal,&progPos))){return false;}

if(decimal!=0){return false;}
*shortValue=(short)longVal;
if(*shortValue != longVal){return false;}//over flow!

return true;

}

/*
//------------------------------------
bool UECSGetValueBetweenChr(char* targetBuffer,char start_asciiCode, char end_asciiCode,short *shortValue,int *lastPos)
{
int _targetBuffersize=strlen(targetBuffer);
*lastPos=-1;
int startPos=-1;
int i;

//-------------start string check
for(i=0;i<_targetBuffersize;i++)
	{
	if(targetBuffer[i]==start_asciiCode){startPos=i+1;break;}
	}

*lastPos=i;
if(startPos<0){return false;}

if(targetBuffer[startPos]<'0' || targetBuffer[startPos]>'9'){return false;}

//------------end code check
for(i=startPos;i<_targetBuffersize;i++)
{
if(targetBuffer[i]=='\0'){return false;}//no end code found
if(targetBuffer[i]==end_asciiCode)
	{break;}
}

if(i>=_targetBuffersize){return false;}//not found
*lastPos=i;
*shortValue=atoi(&targetBuffer[startPos]);
return true;

}
*/
bool UECSGetIPAddress(char *targetBuffer,unsigned char *ip,int *lastPos)
{
int _targetBuffersize=strlen(targetBuffer);
int i;
int progPos=0;
(*lastPos)=0;

//find first number
for((*lastPos);i<_targetBuffersize;(*lastPos)++)
	{
	if(targetBuffer[(*lastPos)]>='0' && targetBuffer[(*lastPos)]<='9')
		{
		break;
		}
	}
if((*lastPos)==_targetBuffersize){return false;}//number not found

//decode ip address
for(i=0;i<=2;i++)
{
if(!UECSAtoI_UChar(&targetBuffer[(*lastPos)],&ip[i],&progPos)){return false;}
(*lastPos)+=progPos;
if(targetBuffer[(*lastPos)]!='.'){return false;}
(*lastPos)++;
}

//last is not '.'
if(!UECSAtoI_UChar(&targetBuffer[(*lastPos)],&ip[3],&progPos)){return false;}
(*lastPos)+=progPos;

return true;
}

//------------------------------------------------------------------------------
bool UECSAtoI_UChar(char *targetBuffer,unsigned char *ucharValue,int *lastPos)
{
unsigned int newValue=0;
bool validFlag=false;

int i;

for(i=0;i<MAX_DIGIT;i++)
{
if(targetBuffer[i]>='0' && targetBuffer[i]<='9')
	{
	validFlag=true;
	newValue=newValue*10;
	newValue+=(targetBuffer[i]-'0');
	if(newValue>255){return false;}//over flow!
	continue;
	}

	break;
}

*lastPos=i;
*ucharValue=newValue;

return validFlag;


}
//------------------------------------------------------------------------------
bool UECSGetFixedFloatValue(char* targetBuffer,long *longVal,unsigned char *decimal,int *lastPos)
{
*longVal=0;
*decimal=0;
int i;
int validFlag=0;
bool floatFlag=false;
char signFlag=1;
unsigned long newValue=0;
unsigned long prvValue=0;


for(i=0;i<MAX_DIGIT;i++)
{
if(targetBuffer[i]=='.')
	{
	if(floatFlag){return false;}//Multiple symbols error
	floatFlag=true;
	continue;
	}
else if(targetBuffer[i]=='-')
	{
	if(validFlag){return false;}//Symbol is after number
	if(signFlag<0){return false;}//Multiple symbols error
	signFlag=-1;
	continue;
	}
else if(targetBuffer[i]>='0' && targetBuffer[i]<='9')
	{
	
	validFlag=true;
	if(floatFlag){(*decimal)++;}
	newValue=newValue*10;
	newValue+=(targetBuffer[i]-'0');
	if(prvValue>newValue || newValue>2147483646){return false;}//over flow!
	prvValue=newValue;
	continue;
	}

	break;
}

*longVal=((long)newValue) * signFlag;
*lastPos=i;

return validFlag;
}


//------------------------------------
#if defined(_ARDUINIO_MEGA_SETTING)
void MemoryWatching()
{
if(UECStempStr20[MAX_TYPE_CHAR-1]!=0 || UECSbuffer[BUF_SIZE-1]!=0)
	{U_orgAttribute.status|=STATUS_MEMORY_LEAK;}
}
#endif
//------------------------------------
//In Safemode this function is not working properly because Web value will be reset.
bool ChangeWebValue(signed long* data,signed long value)
{
for(int i=0;i<U_HtmlLine;i++)
{
if(U_html[i].data==data)
	{
	*(U_html[i].data)=value;
	UECS_EEPROM_writeLong(EEPROM_WEBDATA + i * 4, *(U_html[i].data));
	return true;
	}
}
return false;
}

/********************************/
/* 16521 Response   *************/
/********************************/
//CCMのサーチに対する反応？
//引数：一時保存CCMのアドレス
//返り値：
boolean UECSresCCMSearchAndSend(UECSTEMPCCM* _tempCCM){
	//CCM provider search response
	/*
	In periodic transmission, only CCMs with a proven track record of transmission will return a response.
	Among regularly sent CCMs, CCMs that have not been sent at the specified frequency will not return a response even if they are registered. 
	This is to prevent accidental reference to broken sensors.
	定期送信では送信実績のあるCCMのみ応答を返す
	*/
  
	int i;
	int progPos = 0;
	int startPos = 0;
	short room=0;
	short region=0;
	short order=0;
	
	if(!UECSFindPGMChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){return false;}
	startPos+=progPos;

	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos)){return false;}
	startPos+=progPos;

	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CCMSEARCH[0],&progPos)){return false;}
	startPos+=progPos;
	
	
	//copy ccm type string
	for(i=0;i<MAX_TYPE_CHAR;i++)
	{
	UECStempStr20[i]=UECSbuffer[startPos+i];
	if(UECStempStr20[i]==ASCIICODE_DQUOT || UECStempStr20[i]=='\0')
		{UECStempStr20[i]='\0';break;}
	}
	UECStempStr20[MAX_CCMTYPESIZE]='\0';
	startPos=startPos+i;
	
	//Extract "room", "region", and "order". If not found, it is assumed to be zero.
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_ROOM[0],'\"',&room,&progPos);
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_REGION[0],'\"',&region,&progPos);
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_ORDER[0],'\"',&order,&progPos);
	//Serial.print(room);Serial.print("-");
	//Serial.print(region);Serial.print("-");
	//Serial.print(order);Serial.print("\n");
	
	
for(int id=0;id<U_MAX_CCM;id++)
	{
	if(UECSCCMSimpleHitcheck(id,room,region,order))
		{
		//Serial.print(id);Serial.print("/");
		//Serial.println(U_ccmList[id].typeStr);
		//packet create
		ClearMainBuffer();
    	UDPAddPGMCharToBuffer(&(UECSccm_XMLHEADER[0]));
    	UDPAddPGMCharToBuffer(&(UECSccm_UECSVER_E10[0]));
    	UDPAddPGMCharToBuffer(&(UECSccm_CCMSERVER[0]));
    	UDPAddCharToBuffer(UECStempStr20);
    	UDPAddPGMCharToBuffer(&(UECSccm_ROOMTXT[0]));
		//room
		UDPAddValueToBuffer(U_ccmList[id].baseAttribute[AT_ROOM]);
    	UDPAddPGMCharToBuffer(&(UECSccm_REGIONTXT[0]));
		//region
		UDPAddValueToBuffer(U_ccmList[id].baseAttribute[AT_REGI]);
    	UDPAddPGMCharToBuffer(&(UECSccm_ORDERTXT[0]));
		//order
		UDPAddValueToBuffer(U_ccmList[id].baseAttribute[AT_ORDE]);
    	UDPAddPGMCharToBuffer(&(UECSCCM_PRIOTXT[0]));
		//priority
		UDPAddValueToBuffer(U_ccmList[id].baseAttribute[AT_PRIO]);
    	UDPAddPGMCharToBuffer(&(UECSccm_CLOSETAG[0]));
		//IP
		char iptxt[20];
	    if(U_orgAttribute.status & STATUS_SAFEMODE)
			{
			UDPAddPGMCharToBuffer(&(UECSdefaultIPAddress[0]));
			}
		else
			{
		    sprintf(iptxt, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2], U_orgAttribute.ip[3]);
		    UDPAddCharToBuffer(iptxt);
		    }
    	UDPAddPGMCharToBuffer(&(UECSccm_CCMSERVERCLOSE[0]));

    	
    	//----------------------------------------------send 
		UECS_UDP16521.beginPacket(_tempCCM->address, 16521);
        UECS_UDP16521.write(UECSbuffer);
	        if(UECS_UDP16521.endPacket()==0)
	         	{
	  			UECSresetEthernet(); //when udpsend failed,reset ethernet status
	         	}

		}
	
	}

    return true;
}
//------------------------------------------------------------------------------
//CCMを受信するかどうか確認？
//第一引数：属性
//第二引数：属性
//第三引数：属性
//返り値：受信する時 true, 無視する時　false
boolean UECSCCMSimpleHitcheck(int ccmid,short room,short region,short order)
{
//送信頻度が０の時とセンダーがないときはfalse
if(U_ccmList[ccmid].ccmLevel == NONE || !U_ccmList[ccmid].sender){return false;}
//ccmのtypeが、一時保存と違うときはfalse
if(strcmp(U_ccmList[ccmid].typeStr,UECStempStr20) != 0){return false;}
//規定の送信頻度を満たさず、遠隔操作がないときと送信要求がないとき
if(!U_ccmList[ccmid].validity && U_ccmList[ccmid].ccmLevel<=A_1M_1){return false;}
//基本属性が異なる時はfalse？
if(!(room==0 || U_ccmList[ccmid].baseAttribute[AT_ROOM]==0 || U_ccmList[ccmid].baseAttribute[AT_ROOM]==room)){return false;}
//
if(!(region==0 || U_ccmList[ccmid].baseAttribute[AT_REGI]==0 || U_ccmList[ccmid].baseAttribute[AT_REGI]==region)){return false;}
//
if(!(order==0 || U_ccmList[ccmid].baseAttribute[AT_ORDE]==0 || U_ccmList[ccmid].baseAttribute[AT_ORDE]==order)){return false;}
return true;
}