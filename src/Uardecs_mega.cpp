//ソースコード中のコメントについては、基本的に中村による
//作成者によるコメントは英語になっている


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

//ccmの情報とipアドレスをオブジェクトに入れる
//第一引数：CCMオブジェクトのポインタ
//第二引数：
//返り値：
boolean UECSparseRec( struct UECSTEMPCCM *_tempCCM,int *matchCCMID){
	

	int i;
	int progPos = 0;
	int startPos = 0;
	short shortValue=0;
	
	//GETメソッドの確認
	if(!UECSFindPGMChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){return false;}
	startPos+=progPos;
	//UECSのバージョンを確認
	if(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos))
		{
		//次の領域に移動する
		startPos+=progPos;
			//E10 packet
		}
	else
		{
			//other ver packet
			//バージョンが違うときは終了
			if(!(U_orgAttribute.flags&ATTRFLAG_LOOSELY_VERCHECK)){return false;}
		}
	//データのタイプのアドレスを把握する
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_DATATYPE[0],&progPos)){return false;}
	startPos+=progPos;
	
	//copy ccm type string
	for(i=0;i<MAX_TYPE_CHAR;i++)
	{
		//ccmオブジェクトにtypeを入れていく
	_tempCCM->type[i]=UECSbuffer[startPos+i];
	//ダブルクォーテーションかnullがあるときはnullを入れて終了
	if(_tempCCM->type[i]==ASCIICODE_DQUOT || _tempCCM->type[i]=='\0')
		{_tempCCM->type[i]='\0';break;}
	}
	//終端にnullを入れる
	_tempCCM->type[MAX_CCMTYPESIZE]='\0';
	//次の領域に移動
	startPos=startPos+i;
	
	//In a practical environment ,packets of 99% are irrelevant CCM.
	//matching top 3 chars for acceleration 
	//高速化するために上位3文字でマッチさせていく
	*matchCCMID=-1;			//なぜ-1を代入させるのか？※

	for(i=0;i<U_MAX_CCM;i++)
	{
			//送受信頻度が0ではなくて、受信モードの時の処理
      if(U_ccmList[i].ccmLevel != NONE && U_ccmList[i].sender == false)//check receive ccm
      	{
				//上位三文字を比較してマッチしたら
        if(_tempCCM->type[0]==U_ccmList[i].typeStr[0] && 
           _tempCCM->type[1]==U_ccmList[i].typeStr[1] && 
       	   _tempCCM->type[2]==U_ccmList[i].typeStr[2])
        		{
						//CCMがマッチしていることの確認処理を終了
        		*matchCCMID=i;		//3が入るはず
        		break;
        		}
        }
	}
	//上位3文字がマッチしていなかったらfalseを返して終了
	if(*matchCCMID<0){return false;}//my ccm was not match ->cancel packet reading
	
	//4回繰り返す　(room, region, order, priorityがあるか確認する)
	for(i=0;i<MAX_ATTR_NUMBER;i++)
	{
	//必要な情報がなければそこで終了してfalseを返す
	if(!UECSGetValPGMStrAndChr(&UECSbuffer[startPos],UECSattrChar[i],'\"',&(_tempCCM->attribute[i]),&progPos)){return false;}
	startPos+=progPos;
	}

	//find tag end
	//タグの終わりを確認する「>」
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CLOSETAG[0],&progPos)){return false;}
	startPos+=progPos;
	
	//get value
	//値と小数の桁数をオブジェクトに入れる
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&(_tempCCM->value),&(_tempCCM->decimal),&progPos)){return false;}
	startPos+=progPos;		//アドレスの確認
	
	//ip tag
	//ipアドレス用のタグが見つからないときは
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_IPTAG[0],&progPos))
		{
			//ip tag not found(old type packet)
			//ip address is already filled
			//UECSを閉じるタグを探す</UECS> 見つからなかったらfalse 見つかったらtrue
			if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_FOOTER0[0],&progPos)){return false;}
			return true;
		}
	//アドレスの確認
	startPos+=progPos;
	
	
	unsigned char ip[4];
	//ipアドレスを確認できなかったら処理の中断
	if(!UECSGetIPAddress(&UECSbuffer[startPos],ip,&progPos)){return false;}
	//ipアドレスをオブジェクトに入れていく
	_tempCCM->address[0]=ip[0];
	_tempCCM->address[1]=ip[1];
	_tempCCM->address[2]=ip[2];
	_tempCCM->address[3]=ip[3];
	//アドレスの確認
	startPos+=progPos;
	//ccmが確認できたら終了？※
	if(U_orgAttribute.flags&ATTRFLAG_LOOSELY_VERCHECK){return true;}//Ignore information after ip
	
	//check footer
	//UECSの閉じタグが確認できなかったら中断してfalseを返す
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
	//終了
	return true;


}
//----------------------------------------------------------------------------
//ccmを送信する
//引数：
void UECSCreateCCMPacketAndSend( struct UECSCCM* _ccm){
ClearMainBuffer();			//バッファのクリア
//UECSccmを作成してバッファの中に詰めていく
UDPAddPGMCharToBuffer(&(UECSccm_XMLHEADER[0]));		//ヘッダーを加える
UDPAddPGMCharToBuffer(&(UECSccm_UECSVER_E10[0]));
UDPAddPGMCharToBuffer(&(UECSccm_DATATYPE[0]));
UDPAddCharToBuffer(_ccm->typeStr);

for(int i=0;i<4;i++)
	{
	//ルーム、リージョン、オーダー、プライオリティの情報を詰めていく
	UDPAddPGMCharToBuffer(UECSattrChar[i]); 
	UDPAddValueToBuffer(_ccm->baseAttribute[i]);
	}
//小数を文字列に変換してccmに詰める？
UDPAddPGMCharToBuffer(&(UECSccm_CLOSETAG[0]));
  dtostrf(((double)_ccm->value) / pow(10, _ccm->decimal), -1, _ccm->decimal, UECStempStr20);
UDPAddCharToBuffer(UECStempStr20);
UDPAddPGMCharToBuffer(&(UECSccm_IPTAG[0]));			//タグを閉じる
//セーフモードとそれ以外のモードでipアドレスを分ける
if(U_orgAttribute.status & STATUS_SAFEMODE)
	{
	UDPAddPGMCharToBuffer(&(UECSdefaultIPAddress[0]));
	}
else
	{
	sprintf(UECStempStr20, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2],U_orgAttribute.ip[3]);
	UDPAddCharToBuffer(UECStempStr20);
	}
//ccm(XML)のタグを閉じる
UDPAddPGMCharToBuffer(&(UECSccm_FOOTER1[0]));

//send ccm
	//16520ポート（UECSデフォルト）に対して書き込みを開始する
  UECS_UDP16520.beginPacket(_ccm->address, 16520);
	//16520ポートにデータを書き出す
  UECS_UDP16520.write(UECSbuffer);
  //書き込みに失敗したらイーサネットを初期化する
  if(UECS_UDP16520.endPacket()==0)
  		{
  			UECSresetEthernet();//when udpsend failed,reset ethernet status
  		}

}

//更新フラグを立てる？
//
//
void UECSupRecCCM(UECSCCM* _ccm, UECSTEMPCCM* _ccmRec){
	//フラグを初期化
  boolean success = false;

	int i;
	//属性の取り込み
	for(i=0;i<MAX_ATTR_NUMBER;i++)
		{_ccm->attribute[i] = _ccmRec->attribute[i];}
	//ipアドレスの取り込み
	for(i=0;i<MAX_IPADDRESS_NUMBER;i++)
		{_ccm->address[i] = _ccmRec->address[i];}

    //小数の桁数を把握
    int dif_decimal= _ccm->decimal - _ccmRec->decimal;
    if(dif_decimal>=0){_ccm->value = _ccmRec->value*pow(10,dif_decimal);}
    else{_ccm->value = _ccmRec->value/pow(10,-dif_decimal);}
    

    _ccm->recmillis = 0;
    _ccm->validity=true;
    _ccm->flagStimeRfirst = true;

}
//---------------------------------------------------------------
//ccmが更新されいないか確認する？※
//
//
//
void UECScheckUpDate(UECSTEMPCCM* _tempCCM, unsigned long _time,int startid){
		//
    for(int i = startid; i < U_MAX_CCM; i++){
			//受信頻度が0か送信モードの時は終了
      if(U_ccmList[i].ccmLevel == NONE || U_ccmList[i].sender == true){continue;}
      
					//ccmの設定内容が0があるか、初期設定でないときは終了
          if(!((_tempCCM->attribute[AT_ROOM] == 0 || U_ccmList[i].baseAttribute[AT_ROOM]==0 || _tempCCM->attribute[AT_ROOM] == U_ccmList[i].baseAttribute[AT_ROOM]) &&
               (_tempCCM->attribute[AT_REGI] == 0 || U_ccmList[i].baseAttribute[AT_REGI]==0 || _tempCCM->attribute[AT_REGI] == U_ccmList[i].baseAttribute[AT_REGI]) &&
               (_tempCCM->attribute[AT_ORDE] == 0 || U_ccmList[i].baseAttribute[AT_ORDE]==0 || _tempCCM->attribute[AT_ORDE] == U_ccmList[i].baseAttribute[AT_ORDE]))){continue;}

        //type 
				//typeが異なる時は終了
        if(strcmp(U_ccmList[i].typeStr, _tempCCM->type) != 0){continue;}
					///変数の初期化？
          boolean up = false;
								//送信頻度が定期的な更新出ない場合は終了
                if(U_ccmList[i].ccmLevel==B_0 || U_ccmList[i].ccmLevel==B_1){up = true;}
								//validityがfalseの時は更新？※
                else if(!U_ccmList[i].validity){up = true;} //fresh ccm 
								//入ってきたccmの方がプライオリティが低ければ更新？
                else if(_tempCCM->attribute[AT_PRIO] < U_ccmList[i].attribute[AT_PRIO])
                    {up = true;}//lower priority number is strong　低い数字の方が強い
                else{
									//ccmの設定が同じであれば
                  if(_tempCCM->attribute[AT_ROOM] == U_ccmList[i].attribute[AT_ROOM]){
                    if(_tempCCM->attribute[AT_REGI] == U_ccmList[i].attribute[AT_REGI]){
                      if(_tempCCM->attribute[AT_ORDE] == U_ccmList[i].attribute[AT_ORDE]){
                        
                        //if(_tempCCM->address <= U_ccmList[i].address)
                        //convert big endian
												//アドレスを比較する
                        unsigned long address_t=_tempCCM->address[0];
                        address_t=(address_t<<8)|_tempCCM->address[1];
                        address_t=(address_t<<8)|_tempCCM->address[2];
                        address_t=(address_t<<8)|_tempCCM->address[3];
                        unsigned long address_b=U_ccmList[i].address[0];
                        address_b=(address_b<<8)|U_ccmList[i].address[1];
                        address_b=(address_b<<8)|U_ccmList[i].address[2];
                        address_b=(address_b<<8)|U_ccmList[i].address[3];
                        //何を意味しているかよくわからない？※
                        if(address_t<=address_b)
                        {up = true;} 
                      }
											//オーダーが0か設定と同じであれば更新
                      else if (_tempCCM->attribute[AT_ORDE] == U_ccmList[i].baseAttribute[AT_ORDE] || U_ccmList[i].baseAttribute[AT_ORDE]==0)
                      {up = true;}                         
                    }
										//リージョンが0か設定と等しければ更新
                    else if (_tempCCM->attribute[AT_REGI] == U_ccmList[i].baseAttribute[AT_REGI] || U_ccmList[i].baseAttribute[AT_REGI]==0)
                    {up = true;}                       
                  }
									//ルームが0か設定と等しければ更新
                  else if (_tempCCM->attribute[AT_ROOM] == U_ccmList[i].baseAttribute[AT_ROOM]  || U_ccmList[i].baseAttribute[AT_ROOM]==0)
                  {up = true;} 
                }
								//フラグが立っていれば更新？
                if(up){UECSupRecCCM(&U_ccmList[i], _tempCCM);}
      }  
}

/********************************/
/* 16529 Response   *************/
/********************************/
//スキャンに対する応答

//測定値以外のノードの情報をバッファに詰め込む
//返り値：ccmscanかかnodescanに返信した時はtrue
boolean UECSresNodeScan(){
	int i;
	int progPos = 0;
	int startPos = 0;
	//ヘッダーがあるか確認する
	if(!UECSFindPGMChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){
		return false;
	}
	//アドレスの確認
	startPos+=progPos;
	//バージョンの確認
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos)){
		//other ver
		return false;
	}
	startPos+=progPos;
	
	//NODESCAN
	//nodescanに対する応答（タイプ、ルーム、リージョン、オーダーが一致した場合に応答）
	if(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_NODESCAN1[0],&progPos) || UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_NODESCAN2[0],&progPos)){
	
    //NODESCAN response ノードスキャンへの反応
		ClearMainBuffer();	//バッファのクリア
		//ccmを作成してバッファに詰めていく
    UDPAddPGMCharToBuffer(&(UECSccm_XMLHEADER[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_UECSVER_E10[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_NODENAME[0])); 
    UDPAddPGMCharToBuffer(&(U_name[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_VENDER[0])); 
    UDPAddPGMCharToBuffer(&(U_vender[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_UECSID[0])); 
    UDPAddPGMCharToBuffer(&(U_uecsid[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_UECSID_IP[0]));
    //セーフモードとそれ以外ではipアドレスを切り替える 
    if(U_orgAttribute.status & STATUS_SAFEMODE){
			UDPAddPGMCharToBuffer(&(UECSdefaultIPAddress[0]));
		}else{
	    sprintf(UECStempStr20, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2], U_orgAttribute.ip[3]);
	    UDPAddCharToBuffer(UECStempStr20);
		}
    //Macアドレスをバッファに入れる
    UDPAddPGMCharToBuffer(&(UECSccm_MAC[0]));
    sprintf(UECStempStr20, "%02X%02X%02X%02X%02X%02X", U_orgAttribute.mac[0], U_orgAttribute.mac[1], U_orgAttribute.mac[2], U_orgAttribute.mac[3], U_orgAttribute.mac[4], U_orgAttribute.mac[5]);
    UDPAddCharToBuffer(UECStempStr20);
    UDPAddPGMCharToBuffer(&(UECSccm_NODECLOSE[0]));
    ///成功したらtrueを返す
    return true;
	}
    

	//CCMSCAN
	//ccmscanの確認　想定通りでないときは中断
	if(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CCMSCAN[0],&progPos)){		
		short pageNum=0;
		startPos+=progPos;
		//分割数の把握（UDPなので1回の送信では480バイト以下の必要がある）
		//ダブルクォーテーションがあるときの処理
		if(UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_PAGE[0],'\"',&pageNum,&progPos)){//format of page number written type
			startPos+=progPos;
			//check close tag　閉じかっこの確認
			if(!(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CCMSCANCLOSE2[0],&progPos))){
				return false
			}
		//ダブルクォーテーションがないときの閉じかっこの確認
		}else if(UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CCMSCANCLOSE0[0],&progPos)){ //format of page number abridged type
			pageNum=1;
		}else{
			return false;
		}
	
		//CCMSCAN response
		//ccmスキャンに対する応答
		ClearMainBuffer();			//バッファのクリア
		///ccmを作成してバッファに詰めていく
		UDPAddPGMCharToBuffer(&(UECSccm_XMLHEADER[0]));
		UDPAddPGMCharToBuffer(&(UECSccm_UECSVER_E10[0]));
    UDPAddPGMCharToBuffer(&(UECSccm_CCMNUM[0]));
	
		//count enabled ccm
		short enabledCCMNum=0;
		short returnCCMID=-1;
		//指定した要素の数だけ繰り返す
		for(i = 0; i < U_MAX_CCM; i++){
			//送信頻度が0でないとき
	    if(U_ccmList[i].ccmLevel != NONE){
	    	enabledCCMNum++;	//繰り返し回数？
	    	if(enabledCCMNum==pageNum){		//繰り返した回数がページ数と等しければ
					returnCCMID=i;		//おそらくチェック用
				}
			}  
		}
	  
		//繰り返しがなければfalseで終了  
		if(enabledCCMNum==0 || returnCCMID<0){
			return false;
		}//page num over
	
		//返信する内容をバッファに詰めていく
		UDPAddValueToBuffer(pageNum);		//ページ番号をバッファに詰める
		UDPAddPGMCharToBuffer(&(UECSccm_TOTAL[0]));
		UDPAddValueToBuffer(enabledCCMNum);
    UDPAddPGMCharToBuffer(&(UECSccm_CLOSETAG[0]));
		UDPAddValueToBuffer(1);//Column number is always 1
    UDPAddPGMCharToBuffer(&(UECSccm_CCMNO[0]));
		UDPAddValueToBuffer(pageNum);//page number = ccm number
	
		for(i=0;i<3;i++){
			//ルーム、リージョン、オーダー、プライオリティの情報を詰めていく
			UDPAddPGMCharToBuffer(UECSattrChar[i]);
			UDPAddValueToBuffer(U_ccmList[returnCCMID].baseAttribute[i]);
		}
		//プライオリティを詰める
		UDPAddPGMCharToBuffer(UECSattrChar[AT_PRIO]);
		UDPAddValueToBuffer(U_ccmList[returnCCMID].baseAttribute[AT_PRIO]);
	 
	 	//メタ情報を詰めていく
		UDPAddPGMCharToBuffer(&(UECSccm_CAST[0]));
		UDPAddValueToBuffer(U_ccmList[returnCCMID].decimal);
		UDPAddPGMCharToBuffer(&(UECSccm_UNIT[0]));                          
		UDPAddPGMCharToBuffer((U_ccmList[returnCCMID].unit));
		UDPAddPGMCharToBuffer(&(UECSccm_SR[0]));		
		if(U_ccmList[returnCCMID].sender){
			UDPAddPGMCharToBuffer(UECSTxtPartS);
		}else{
			UDPAddPGMCharToBuffer(UECSTxtPartR);
		}              
		UDPAddPGMCharToBuffer(&(UECSccm_LV[0]));                          
		UDPAddPGMCharToBuffer((UECSCCMLEVEL[U_ccmList[returnCCMID].ccmLevel]));
		UDPAddPGMCharToBuffer(&(UECSccm_CLOSETAG[0]));
		UDPAddCharToBuffer(U_ccmList[returnCCMID].typeStr);
		UDPAddPGMCharToBuffer(&(UECSccm_CCMRESCLOSE[0]));  
    
    return true;		//バッファに詰め終わったら true 
	}
	return false;  //ccmscanもnodescanも届いていないときはfalse
}


//------------------------------------------------------------------
/*
UDP transmission load balancing
Improved UDP send timing not to be concentrated in Ver2.0 or later.
Packets sent at 10-second and 60-second intervals were sent together in past versions, but are sent separately in the new version.
Transmission timing is distributed according to the order of registration of the lower one of the IP addresses and the CCM.
However, this load balancing is ineffective for packets sent every second.
UDP送信のロードバランシング Ver2.0以降に集中しないよう、UDP送信タイミングを改善。
10秒間隔と60秒間隔で送信するパケットを、過去のバージョンではまとめて送信していましたが、新バージョンでは別々に送信するようにしました。
送信タイミングは，IPアドレスの下位1つとCCMの登録順によって分散される．ただし，1秒間隔で送信されるパケットには，この負荷分散の効果はない．
*/
//ロードバランサー？
void UECSautomaticSendManager(){

	int timing_count;
	//ccm_dummyの数だけ繰り返す
	for(int id=0;id<U_MAX_CCM;id++){
		//受信モードでないことと、送信頻度が0でないことを確認
		if(U_ccmList[id].ccmLevel == NONE || !U_ccmList[id].sender){
			continue;
		}

		//Timeout judgment of the sending CCM
		//送信の一時停止状態でないことを確認する falseで一時停止
		if(U_ccmList[id].flagStimeRfirst == true){
			U_ccmList[id].recmillis=0;		//最後に送信してからの経過時間
			U_ccmList[id].validity=true;	//送信実績
		}else{		//flagStimeRfirstがfalseのとき
			//送信頻度と実際のインターバルを比較して送信実績の確認
			//1秒に1回の送信頻度なのに前回送信から1秒以上経過した時は送信実績をfalseにする
			if((U_ccmList[id].ccmLevel==A_1S_0 || U_ccmList[id].ccmLevel==A_1S_1)&& U_ccmList[id].recmillis>1000){
				U_ccmList[id].validity=false;
			}else if((U_ccmList[id].ccmLevel==A_10S_0 || U_ccmList[id].ccmLevel==A_10S_1)&& U_ccmList[id].recmillis>10000){
				U_ccmList[id].validity=false;
			}else if((U_ccmList[id].ccmLevel==A_1M_0 || U_ccmList[id].ccmLevel==A_1M_1)&& U_ccmList[id].recmillis>60000){
				U_ccmList[id].validity=false;
			}
		}
		//経過時間が10時間より小さければ1秒足す
		if(U_ccmList[id].recmillis<36000000){
			U_ccmList[id].recmillis+=1000;
		}

		//Determination of CCM transmission timing
		//ccmの送信タイミングの判定
		timing_count=U_orgAttribute.ip[3]+id;
		//送信間隔未満でも値が変動したら送信
		if((U_ccmList[id].ccmLevel == A_10S_1 || U_ccmList[id].ccmLevel == A_1M_1 ) && U_ccmList[id].old_value!=U_ccmList[id].value){
			U_ccmList[id].flagStimeRfirst = true;
		//送信間隔が1秒の時は送信
		}else if(U_ccmList[id].ccmLevel == A_1S_0 || U_ccmList[id].ccmLevel == A_1S_1 || U_ccmList[id].ccmLevel == S_1S_0){
			U_ccmList[id].flagStimeRfirst = true;
		//時間を把握するカウンターで10秒ごとに10秒間隔の頻度のモノを送信
		}else if((UECSsyscounter60s % 10 == (timing_count % 10)) && (U_ccmList[id].ccmLevel == A_10S_0 || U_ccmList[id].ccmLevel == A_10S_1)){
			U_ccmList[id].flagStimeRfirst = true;
		//時間を把握するカウンターで1分毎に1分間隔のモノを送信
		}else if(UECSsyscounter60s == (timing_count % 60) && (U_ccmList[id].ccmLevel == A_1M_0 || U_ccmList[id].ccmLevel == A_1M_1 || U_ccmList[id].ccmLevel == S_1M_0)){
			U_ccmList[id].flagStimeRfirst = true;
		//それ以外のモノは送信の一時停止
		}else{
			U_ccmList[id].flagStimeRfirst = false;
		}
		U_ccmList[id].old_value=U_ccmList[id].value;	//値の変動を確認するため
	}
}

//----------------------------------------------------------------------
//受信パケットが有効か確認する
//引数：前回のループからの経過時間
void UECSautomaticValidManager(unsigned long td){
	//ccm_dummyで指定した回数だけ繰り返す
	for(int id=0;id<U_MAX_CCM;id++){
		//受信モードの時はスキップ（次のccmにいく）
  	if(U_ccmList[id].ccmLevel == NONE|| U_ccmList[id].sender){
			continue;
		}
		//24時間を超えたらスキップ
		if(U_ccmList[id].recmillis>86400000){		//over 24hour
    	continue;//stop count
    }
		//経過時間の把握
		U_ccmList[id].recmillis+=td;//count time(ms) since last recieved
  		
		unsigned long validmillis=0;
		//1秒のレベルだと受信した情報が3秒間有効
		if(U_ccmList[id].ccmLevel == A_1S_0 || U_ccmList[id].ccmLevel == A_1S_1 || U_ccmList[id].ccmLevel == S_1S_0){
	    validmillis = 3000;
		//10秒のレベルだと受信した情報が30秒間有効
		}else if(U_ccmList[id].ccmLevel == A_10S_0 || U_ccmList[id].ccmLevel == A_10S_1){
    	validmillis = 30000;
  	//1分のレベルだと受信した情報が3分間有効
		}else if(U_ccmList[id].ccmLevel == A_1M_0 || U_ccmList[id].ccmLevel == A_1M_1 || U_ccmList[id].ccmLevel == S_1M_0){
    	validmillis = 180000;
 	 	}
    //パケット受信の有効時間を過ぎるかflagStimeRfirstがfalseの時はvalidをfalseにする
		if(U_ccmList[id].recmillis > validmillis || U_ccmList[id].flagStimeRfirst == false){
			U_ccmList[id].validity = false;  
		}
	}
}

//##############################################################################
//##############################################################################

//EEPROMに書き込む
//第一引数：書き込む場所のアドレス
//第二引数：書き込みたいデータのサイズ
void UECS_EEPROM_writeLong(int ee, long value){
	//必要な容量の把握？（書き込む文字数？）
	byte* p = (byte*)(void*)&value;
	//書き込み
	for (unsigned int i = 0; i < sizeof(value); i++){
		if(EEPROM.read(ee)!=p[i]){	//same value skip　書き込まれている内容はスキップする
			EEPROM.write(ee, p[i]);	//書き込む
		}
		ee++;
	}
}

//EEPROMから読み込む　※
//引数：読み込みを開始する位置
//返り値：読み込んだデータのアドレス？
long UECS_EEPROM_readLong(int ee){
	long value = 0;
	byte* p = (byte*)(void*)&value;
	for (unsigned int i = 0; i < sizeof(value); i++)
	*p++ = EEPROM.read(ee++);		//読み込んだ値を用意した領域に入れていく
	return value;		//読み込んだ値？
}

//-----------------------------------------------------------new
//引数をhtmlに書き込む
//書き込みたい値
void HTTPsetInput(short _value){		//表示するアドレスの文字列を作成する
    HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));		//文字列の追加
    HTTPAddValueToBuffer(_value);		//バッファに保存
    HTTPAddPGMCharToBuffer(&(UECShtmlINPUTCLOSE3[0]));	//閉じかっこ
}
//-----------------------------------------------------------new
//ipアドレスをwebブラウザに表示する
//引数：ipアドレスの配列
void HTTPprintIPtoHtml(byte address[]){
	//ex)255:255:255:255<br>　になる
  for(int i = 0; i < 3; i++){
	 //表示するipアドレスを詰め込む
   HTTPsetInput(address[i]);
   HTTPAddPGMCharToBuffer(UECSTxtPartColon);		//ipアドレスを:で繋いでいく
  }      
  HTTPsetInput(address[3]);
  HTTPAddPGMCharToBuffer(&(UECSbtrag[0]));			//改行コード
}
//-----------------------------------------------------------new
//---------------------------------------------------------------
//リダイレクトページ（特に何も表示されない）
//引数：
void HTTPPrintRedirect(char page){
	ClearMainBuffer();		//バッファの初期化
	//表示する文字列を作成していく
	HTTPAddPGMCharToBuffer(&(UECShttpHead200_OK[0]));
	HTTPAddPGMCharToBuffer(&(UECShttpHeadContentType[0]));
	HTTPAddPGMCharToBuffer(&(UECShttpHeadConnection[0]));
	HTTPAddPGMCharToBuffer(&(UECShtmlHEADER[0]));
	//表示するhtmlを作成していく
	if(page==3){
 		HTTPAddPGMCharToBuffer(&(UECShtmlREDIRECT3[0]));
 	}else{
	 HTTPAddPGMCharToBuffer(&(UECShtmlREDIRECT1[0]));
 	}
 HTTPCloseBuffer();			//バッファをクライアントに表示する
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
//html用のヘッダーの作成してバッファに入れていく
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
	if(U_orgAttribute.status & STATUS_MEMORY_LEAK){		//エラー表示
		HTTPAddPGMCharToBuffer(&(UECShtmlATTENTION_INTERR[0]));
	}	
 	if(U_orgAttribute.status & STATUS_SAFEMODE){		//safemodeの表示
		 HTTPAddPGMCharToBuffer(&(UECShtmlATTENTION_SAFEMODE[0]));
	}	
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
//ページを作成してクライアントに表示する（バッファはクリアされない）
void HTTPsendPageIndex(){
  HTTPPrintHeader();		//ヘッダーの作成
	//表示ページを作成してUECSバッファに保存する
  HTTPAddCharToBuffer(U_nodename);
  HTTPAddPGMCharToBuffer(&(UECShtmlH1CLOSE[0]));
  HTTPAddPGMCharToBuffer(&(UECShtmlIndex[0]));
  HTTPAddPGMCharToBuffer(&(UECShtmlHR[0]));
  HTTPAddPGMCharToBuffer(&(U_footnote[0]));		//センサーネーム

#if defined(_ARDUINIO_MEGA_SETTING)
	//日時の表示？
	HTTPAddPGMCharToBuffer(&(LastUpdate[0]));
	HTTPAddPGMCharToBuffer(&(ProgramDate[0]));
	HTTPAddPGMCharToBuffer(&(UECSTxtPartHyphen[0]));
	HTTPAddPGMCharToBuffer(&(ProgramTime[0]));
#endif
  HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0]));	//htmlを閉じる</html>
  HTTPCloseBuffer();			//クライアントに表示する
}
//--------------------------------------------------
//ネットワーク設定ページを作成して表示する
void HTTPsendPageLANSetting(){
	//接続情報を確認してwebフォームを確認
  UECSinitOrgAttribute();
	//表示するページの作成
  HTTPPrintHeader();	//ヘッダーの作成
  HTTPAddCharToBuffer(U_nodename);		//ノードネーム
  HTTPAddPGMCharToBuffer(UECShtmlH1CLOSE); 
  HTTPAddPGMCharToBuffer(UECShtmlH2TAG);
  HTTPAddPGMCharToBuffer(UECShtmlLANTITLE);
  HTTPAddPGMCharToBuffer(UECShtmlH2CLOSE);  
  HTTPAddPGMCharToBuffer(&(UECShtmlLAN2[0]));  // <form action=\"./h2.htm\" name=\"f\"><p>
  HTTPAddPGMCharToBuffer(&(UECShtmlLAN3A[0]));   // address: 
 	byte UECStempByte[4];
  for(int i = 0; i < 4; i++){		//ipアドレス？
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
  UDPAddCharToBuffer(UECStempStr20);		//Macアドレスの追加

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
  
	if(U_orgAttribute.status & STATUS_NEEDRESET){
		HTTPAddPGMCharToBuffer(&(UECShtmlATTENTION_RESET[0]));
	}

  HTTPAddPGMCharToBuffer(&(UECShtmlRETURNINDEX[0]));
  HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0]));    //</BODY></HTML>
  HTTPCloseBuffer();		//クライアントに表示する
}

//--------------------------------------------------
//ccmの送受信データの表示と設定のページ
void HTTPsendPageCCM(){
	//表示するページを作成していく
 	HTTPPrintHeader();
	HTTPAddCharToBuffer(U_nodename); 
	HTTPAddPGMCharToBuffer(&(UECShtmlCCMRecRes0[0])); // </H1><H2>CCM Status</H2><TABLE border=\"1\"><TBODY align=\"center\"><TR><TH>Info</TH><TH>S/R</TH><TH>Type</TH><TH>SR Lev</TH><TH>Value</TH><TH>Valid</TH><TH>Sec</TH><TH>Atr</TH><TH>IP</TH></TR>
	//ccmの数を確認
 	for(int i = 0; i < U_MAX_CCM; i++){
		 //送信頻度が0でないときのページを作成する
		if(U_ccmList[i].ccmLevel != NONE){		
			HTTPAddPGMCharToBuffer(&(UECStrtd[0])); //<tr><td>
			HTTPAddPGMCharToBuffer(U_ccmList[i].name); 
			HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
			if(U_ccmList[i].sender){
				HTTPAddPGMCharToBuffer(UECSTxtPartS);
			}else{
				HTTPAddPGMCharToBuffer(UECSTxtPartR);		//R
			} 
			HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
			HTTPAddCharToBuffer(U_ccmList[i].typeStr);
			HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 
			HTTPAddPGMCharToBuffer((UECSCCMLEVEL[U_ccmList[i].ccmLevel])); //******
			HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 
			dtostrf(((double)U_ccmList[i].value) / pow(10, U_ccmList[i].decimal), -1, U_ccmList[i].decimal,UECStempStr20);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>

			if(U_ccmList[i].sender){/*	//送信モードの時の処理
				if(U_ccmList[i].validity)
					{HTTPAddPGMCharToBuffer(UECSTxtPartSend);}
				else
				{HTTPAddPGMCharToBuffer(UECSTxtPartStop);}
				*/
			}else{		//送信モード以外の時
				if(U_ccmList[i].validity){
					HTTPAddPGMCharToBuffer(UECSTxtPartOK);		//OK
				}else{
					HTTPAddPGMCharToBuffer(UECSTxtPartHyphen);	//"-"
				}
			} 
			HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 表のところ？？ 
			//送信可能かつ受信モードの時
			if(U_ccmList[i].flagStimeRfirst && U_ccmList[i].sender == false){
				if(U_ccmList[i].recmillis<36000000){			//10時間未満であれば　　10時間でflagStimeRfirstが変わる？
					HTTPAddValueToBuffer(U_ccmList[i].recmillis / 1000);		//秒数を表示
				}
			}else{
				//over 10hour
			} 
			HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td> 
			//送信可能かつ受信モードの時
			if(U_ccmList[i].flagStimeRfirst && U_ccmList[i].sender == false){
				//room, region, order, priorityを文字列に詰め込む
				sprintf(UECStempStr20, "%d-%d-%d-%d", U_ccmList[i].attribute[AT_ROOM], U_ccmList[i].attribute[AT_REGI], U_ccmList[i].attribute[AT_ORDE], U_ccmList[i].attribute[AT_PRIO]);
				HTTPAddCharToBuffer(UECStempStr20); 
			}
			//基本のroom, region, order, priorityを文字列に詰め込む？
			sprintf(UECStempStr20, "(%d-%d-%d)", U_ccmList[i].baseAttribute[AT_ROOM], U_ccmList[i].baseAttribute[AT_REGI], U_ccmList[i].baseAttribute[AT_ORDE]); 
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>  表の終了
		
			HTTPAddPGMCharToBuffer(&(UECSAHREF[0])); //href
			//ハイパーリンク用のアドレス？
			sprintf(UECStempStr20, "%d.%d.%d.%d", U_ccmList[i].address[0], U_ccmList[i].address[1], U_ccmList[i].address[2], U_ccmList[i].address[3]); 
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddPGMCharToBuffer(&(UECSTagClose[0]));
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddPGMCharToBuffer(&(UECSSlashA[0]));		//</a>
	
			HTTPAddPGMCharToBuffer(&(UECStdtr[0])); //</td></tr> 
		
		} //NONE route
	}
	HTTPAddPGMCharToBuffer(&(UECShtmlTABLECLOSE[0])); //</TBODY></TABLE>
	//webの設定画面に表示する項目があるとき
	if(U_HtmlLine>0) {
		HTTPAddPGMCharToBuffer(&(UECShtmlUserRes0[0])); // </H1><H2>Status</H2><TABLE border=\"1\"><TBODY align=\"center\"><TR><TH>Name</TH><TH>Val</TH><TH>Unit</TH><TH>Detail</TH></TR>
		//表示する項目を作成していく
		for(int i = 0; i < U_HtmlLine; i++){
			HTTPAddPGMCharToBuffer(&(UECStrtd[0]));
			HTTPAddPGMCharToBuffer(U_html[i].name);
			HTTPAddPGMCharToBuffer(&(UECStdtd[0]));
			// only value
			//数値表示カラム
			if(U_html[i].pattern == UECSSHOWDATA){
				if(U_html[i].decimal != 0){		//小数に変換する
					dtostrf(((double)*U_html[i].data) / pow(10, U_html[i].decimal), -1, U_html[i].decimal,UECStempStr20);
				}else{		//小数でないときはそのまま表示する
					sprintf(UECStempStr20, "%ld", *(U_html[i].data));
				}
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(&(UECShtmlInputHidden0[0]));		//hiden 
			}else if(U_html[i].pattern == UECSINPUTDATA){		//数値入力欄の表示
				HTTPAddPGMCharToBuffer(&(UECShtmlInputText[0]));		//htmlから入力する
				//おそらく仮に表示する
				dtostrf(((double)*U_html[i].data) / pow(10, U_html[i].decimal), -1, U_html[i].decimal,UECStempStr20);	
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddPGMCharToBuffer(UECSSlashTagClose);		//タグを閉じる
			}else if(U_html[i].pattern == UECSSELECTDATA){		//ドロップダウンリストの表示
				HTTPAddPGMCharToBuffer(&(UECShtmlSelect[0]));	//htmlでselectタグを使用する

				for(int j = 0; j < U_html[i].selectnum; j++){
					HTTPAddPGMCharToBuffer(&(UECShtmlOption[0]));		//htmlのoption表示
					HTTPAddValueToBuffer(j);		//valueになる
					if((int)(*U_html[i].data) == j){
						HTTPAddPGMCharToBuffer(UECSTxtPartSelected);	//</selected>
					}else{ 
						HTTPAddPGMCharToBuffer(UECSTagClose);
					}
					HTTPAddPGMCharToBuffer(U_html[i].selectname[j]);//************
				} 
				HTTPAddPGMCharToBuffer(&(UECShtmlSelectEnd[0])); 
			}else if(U_html[i].pattern == UECSSHOWSTRING){		//文字列表示カラム
				//dataメンバに入っている内容を表示する？　※
				HTTPAddPGMCharToBuffer(U_html[i].selectname[(int)*(U_html[i].data)]);//************
				HTTPAddPGMCharToBuffer(&(UECShtmlInputHidden0[0])); 
			}
			//htmlの表を作成
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
	HTTPAddPGMCharToBuffer(&(UECShtmlRETURNINDEX[0])); 	//<P align=\"center\">return <A href=\"index.htm\">Top</A></P>
	HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0])); 		//</html>
 
 	HTTPCloseBuffer();	//クライアントに表示する
}
//--------------------------------------------------
//CCMeditページの作成
//引数：ccmid
void HTTPsendPageEDITCCM(int ccmid){
	//表示するページを作成していく
	HTTPPrintHeader();
	HTTPAddCharToBuffer(U_nodename);
	HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMTableHeader[0]));

	//ccmの数の把握
	for(int i = 0; i < U_MAX_CCM; i++){
	// if(U_ccmList[i].ccmLevel != NONE)
		HTTPAddPGMCharToBuffer(&(UECStrtd[0])); //<tr><td>
		//table 2nd start
		//	HTTPAddPGMCharToBuffer(&(UECStdtd[0]));
		//ccm name
		HTTPAddPGMCharToBuffer(U_ccmList[i].name);
		HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
		//送受信モードの確認
	 	if(U_ccmList[i].sender){		//送信モードの時はS
	 		HTTPAddPGMCharToBuffer(UECSTxtPartS);
	 	}else{											//受信モードの時はR
	 		HTTPAddPGMCharToBuffer(UECSTxtPartR);
	 	} 
		//level　送信頻度
		HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
		HTTPAddPGMCharToBuffer((UECSCCMLEVEL[U_ccmList[i].ccmLevel]));
		//unit　単位
		HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
		HTTPAddPGMCharToBuffer(U_ccmList[i].unit);
		HTTPAddPGMCharToBuffer(&(UECStdtd[0])); //</td><td>
		//送信頻度が0でないとき　iとccmidが一致しないときはエラーと判断？
		if(i==ccmid && U_ccmList[i].ccmLevel != NONE){
			HTTPAddPGMCharToBuffer(&(UECShtmlInputHiddenValue[0])); //hidden value(ccmid)
			sprintf(UECStempStr20,"%d",i);				//ccmidの表示
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddPGMCharToBuffer(&(UECSSlashTagClose[0]));
					
			//room region orderを確認
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
		}else{
			//room, region, order, priorityを表示する
			sprintf(UECStempStr20,"%d-%d-%d-%d",U_ccmList[i].baseAttribute[AT_ROOM],U_ccmList[i].baseAttribute[AT_REGI],U_ccmList[i].baseAttribute[AT_ORDE],U_ccmList[i].baseAttribute[AT_PRIO]);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
			HTTPAddCharToBuffer(U_ccmList[i].typeStr);
			HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
			HTTPAddPGMCharToBuffer(U_ccmList[i].type);
			HTTPAddPGMCharToBuffer(&(UECStdtd[0]));//</td><td>
			
			//Edit Linkを作成する　CCMの編集ボタンになる
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
	HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMCmdBtn2[0]));	//ボタンを押したときのonclick
	HTTPAddPGMCharToBuffer(&(UECShtmlRETURNINDEX[0])); 

	//Javascript
	//ボタンに応じたJSの処理を行う　値の設定とデフォルトに戻す
	HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMCmdBtnScript1[0])); 
	sprintf(UECStempStr20,"%d",ccmid+100);
	HTTPAddCharToBuffer(UECStempStr20);
	HTTPAddPGMCharToBuffer(&(UECShtmlEditCCMCmdBtnScript2[0])); 
	HTTPAddPGMCharToBuffer(&(UECShtmlHTMLCLOSE[0])); 

 	HTTPCloseBuffer();		//クライアントに表示する
}

//---------------------------------------------####################
//---------------------------------------------####################
//---------------------------------------------####################
//CCMの設定画面
void HTTPGetFormDataCCMPage(){
	//web設定画面で表示すべき項目がないときは終了
	if(U_HtmlLine==0){
		return;
	}

	int i;
	int startPos=0;
	int progPos=0;
	long tempValue;
	unsigned char tempDecimal;
	//表示項目だけ繰り返す
	for(i=0;i<U_HtmlLine;i++){
		if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){		//Lを探す
			continue;
		}
		startPos+=progPos;
		if(U_html[i].pattern == UECSINPUTDATA){	//数値入力欄の表示
			//小数を扱える形に変換する
			if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&tempValue,&tempDecimal,&progPos)){
				return;
			}
			startPos+=progPos;
			//入力値の確認用変数
			if(UECSbuffer[startPos]!='&'){
				return;
			}//last '&' not found
			//小数の桁数の違いを把握する	
			int dif_decimal=U_html[i].decimal-tempDecimal;
			//小数の桁数の違いによって処理を振り分ける
			if(dif_decimal>=0){
				*U_html[i].data = tempValue*pow(10,dif_decimal);
			}else{
				*U_html[i].data = tempValue/pow(10,-dif_decimal);
			}
			//最小値よりも小さいデータは使用できない
			if(*U_html[i].data<U_html[i].minValue){
				*U_html[i].data=U_html[i].minValue;
			}
			//最大値よりも大きいデータは使用できない
			if(*U_html[i].data>U_html[i].maxValue){
				*U_html[i].data=U_html[i].maxValue;
			}
		//ドロップダウンリストの表示
		}else if(U_html[i].pattern == UECSSELECTDATA){
			//小数を扱える形に変換
			if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&tempValue,&tempDecimal,&progPos)){
				return;
			}
			startPos+=progPos;
			//入力値の確認用の変数
			if(UECSbuffer[startPos]!='&'){
				return;
			}//last '&' not found
			//小数の時ははじく？	
			if(tempDecimal!=0){
				return;
			}
			*U_html[i].data=tempValue%U_html[i].selectnum;//cut too big value
		}			
	}
	OnWebFormRecieved();	//webの中で更新されたデータを受け取る
	for(i = 0; i < U_HtmlLine; i++){
		//EEPROMに書き込む
		UECS_EEPROM_writeLong(EEPROM_WEBDATA + i * 4, *(U_html[i].data));
	}
}


//webで設定したCCMの情報をEEPROMに保存する
//返り値：CCMid、エラーの時は-1
int HTTPGetFormDataEDITCCMPage(){
	int i;
	int startPos=0;
	int progPos=0;
	unsigned char tempDecimal;
	long ccmid,room,region,order,priority;
	//L=が見つからなかったらエラーを返す
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
		return -1;
	}
	startPos+=progPos;
	//小数を扱える形に変換する
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&ccmid,&tempDecimal,&progPos)){
		return -1;
	}
	startPos+=progPos;
	
	//if(UECSbuffer[startPos]!='&'){return -1;}//last '&' not found
	//小数を含んでいたらエラーを返す
	if(tempDecimal!=0){
		return -1;
	}
	
	//Room
	//正常なデータか確認する
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&room,&tempDecimal,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){
		return ccmid;
	}//last '&' not found
	if(tempDecimal!=0){
		return ccmid;
	}
	/*constrain 数値をある範囲に制限する:roomを0から127に制限する
	room < 0 の時は 0, room > 127の時は127*/
	room=constrain(room,0,127);		//roomの範囲の確認
	
	//Region
	//正常なデータか確認する
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&region,&tempDecimal,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){
		return ccmid;
	}//last '&' not found
	if(tempDecimal!=0){
		return ccmid;
	}
	//regionを正常な範囲にする
	region=constrain(region,0,127);
	
	//Order
	//正常なデータか確認する
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&order,&tempDecimal,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){
		return ccmid;
	}//last '&' not found
	if(tempDecimal!=0){
		return ccmid;
	}
	//orderを適正な範囲にする
	order=constrain(order,0,30000);
	
	//Priority
	//正常なデータか確認する
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&priority,&tempDecimal,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){
		return ccmid;
	}//last '&' not found
	if(tempDecimal!=0){
		return ccmid;
	}
	//priorityを正常な範囲にする
	priority=constrain(priority,0,30);
	//Web の Network Config 画面で設定したノードの基本属性
	U_ccmList[ccmid].baseAttribute[AT_ROOM]=room;
	U_ccmList[ccmid].baseAttribute[AT_REGI]=region;
	U_ccmList[ccmid].baseAttribute[AT_ORDE]=order;
	U_ccmList[ccmid].baseAttribute[AT_PRIO]=priority;
	
	UECS_EEPROM_SaveCCMAttribute(ccmid);		//CCM情報をEEPROMに保存する
	
	//---------------------------Type
	//typeの情報を取得していく(CCMの名前？)
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
		return ccmid;
	}
	startPos+=progPos;
	
	//copy type
	int count=0;
	//&が出てきたら終了
	for(i=0;i<MAX_CCMTYPESIZE;i++){
		if(UECSbuffer[startPos+i]=='&'){
			break;
		}
		//終端のnullが出てくるか、最後までいったらccmidを返す
		if(UECSbuffer[startPos+i]=='\0' || i==MAX_CCMTYPESIZE){
			return ccmid;
		}//���[���Ȃ�
		//数字又はアルファベット、ピリオドとハイフンが利用できる
		if( (UECSbuffer[startPos+i]>='0' && UECSbuffer[startPos+i]<='9')||
			(UECSbuffer[startPos+i]>='A' && UECSbuffer[startPos+i]<='Z')||
			(UECSbuffer[startPos+i]>='a' && UECSbuffer[startPos+i]<='z')||
			 UECSbuffer[startPos+i]=='.' || UECSbuffer[startPos+i]=='_' )
		{

		}else{
			UECSbuffer[startPos+i]='x';			//使用できない記号はxで表示する
		}
		//UECS文字列テンプに入れる
		UECStempStr20[i]=UECSbuffer[startPos+i];
		count++;
	}
	//終端にnullを入れる
	UECStempStr20[i]='\0';//set end code
	//3文字目から19文字目までをEEPROMに保存する
	if(count>=3 && count<=19){
		strcpy(U_ccmList[ccmid].typeStr,UECStempStr20);
		UECS_EEPROM_SaveCCMType(ccmid);
	}	
	return ccmid;		//ccmidを返す
}

//--------------------------------------------------------------------------
//LANの設定をEEPROMに書き込む
void HTTPGetFormDataLANSettingPage(){
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
	//データの妥当性を判断する
	for(i=0;i<16;i++){
		if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
			return ;
		}
		startPos+=progPos;
		if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&UECStempValue[i],&tempDecimal,&progPos)){
			return ;
		}
		startPos+=progPos;
		if(UECSbuffer[startPos]!='&'){
			return ;
		}//last '&' not found
		if(tempDecimal!=0){
			return ;
		}
		//check value and write
		//0~255の間に入っているか確認する
		if(UECStempValue[i]<0 || UECStempValue[i]>255){
			return ;
		}//IP address
	}
	//同じ値は無視する
	for(int i = 0; i < 16; i++){
		if(EEPROM.read(EEPROM_DATATOP + i) != (unsigned char)UECStempValue[i]){	//skip same value	
			EEPROM.write(EEPROM_DATATOP + i, (unsigned char)UECStempValue[i]);
			/*bit or演算？
			x |= y; // equivalent to x = x | y;
			何をしているかはよくわからない？　※　*/
			U_orgAttribute.status |= STATUS_NEEDRESET;		//リセットボタンを押してほしいとステータスに入れる			
		}
	}
       		
	//---------------------------NODE NAME
	//データの妥当性を判断
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
		return ;
	}
	startPos+=progPos;
	//copy node name
	//node nameの確認
	for(i=0;i<20;i++){		//かっこを取り除く
		if(UECSbuffer[startPos+i]=='<' || UECSbuffer[startPos+i]=='>'){		//eliminate tag
			UECSbuffer[startPos+i]='*';
		}	
		if(UECSbuffer[startPos+i]=='&'){		//&が出てきたら中断
			break;
		}
		if(UECSbuffer[startPos+i]=='\0' || i==19){	//nullがでてくるか最後までいったら終了
			return;
		}//�I�[�������̂Ŗ���
		//prevention of Cutting multibyte UTF-8 code
		//マルチバイトコードへの対応
		if(i>=16 && (unsigned char)UECSbuffer[startPos+i]>=0xC0){	//UTF-8 multibyte code header
			break;	
		}
		UECStempStr20[i]=UECSbuffer[startPos+i];		//node nameの取り出し
	}
	UECStempStr20[i]='\0';//set end code
	//EEPROMに保存されているnode nameと異なる時はEEPROMに書き込む
	for(int i = 0; i < 20; i++){
		U_nodename[i]=UECStempStr20[i];			
			if(EEPROM.read(EEPROM_NODENAME + i)!=U_nodename[i]){		//skip same value
				EEPROM.write(EEPROM_NODENAME + i, U_nodename[i]);
			}
	}
	return ;
}

//--------------------------------------------------------------------
//
void HTTPGetFormDataFillCCMAttributePage(){
	int i;
	int startPos=0;
	int progPos=0;
	unsigned char tempDecimal;
	long ccmid;
	//入っているデータの妥当性をチェック
	if(!UECSFindPGMChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){
		return;
	}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&ccmid,&tempDecimal,&progPos)){
		return;
	}
	startPos+=progPos;
	if(tempDecimal!=0){
		return;
	}
	ccmid-=100;		//ccmidが100以上の時に実行する
	if(ccmid<0 || ccmid>=U_MAX_CCM){
		return;
	}
	//room, region, order, priority情報をEEPROMに保存する
	for(i=0;i<U_MAX_CCM;i++){
		U_ccmList[i].baseAttribute[AT_ROOM]=U_ccmList[ccmid].baseAttribute[AT_ROOM];
		U_ccmList[i].baseAttribute[AT_REGI]=U_ccmList[ccmid].baseAttribute[AT_REGI];
		U_ccmList[i].baseAttribute[AT_ORDE]=U_ccmList[ccmid].baseAttribute[AT_ORDE];
		U_ccmList[i].baseAttribute[AT_PRIO]=U_ccmList[ccmid].baseAttribute[AT_PRIO];
		UECS_EEPROM_SaveCCMAttribute(i);
	}
}

//---------------------------------------------####################
//httpのメソッド（リクエスト）を確認
void HTTPcheckRequest(){
	//サーバーに接続しているクラインアントを取得	利用可能なクライアントがないときは偽
	UECSclient=UECSlogserver.available();	
  //Caution! This function can not receive only up to the top 299 bytes.
  //Dropped string will be ignored.
	//上位299バイトが読み込まれる
  
  if(UECSclient){		//クライアントがあるか確認 
		//Add null termination 
		UECSbuffer[UECSclient.read((uint8_t *)UECSbuffer,BUF_SIZE-1)]='\0';	//null終端の追加	
		//余計なものを除く
		HTTPFilterToBuffer();//Get first line before "&S=" and eliminate unnecessary code
		int progPos = 0;
		//受信したGETメソッドに応じたページの作成
		if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP0[0]),&progPos)){
			HTTPsendPageIndex();	//Indexページの作成
		}else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP1[0]),&progPos)){
			HTTPsendPageCCM();		//CCMの設定ページ
		}else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP2[0]),&progPos)){
			HTTPsendPageLANSetting();	//ネットワーク設定ページ
		}else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP1A[0]),&progPos)){	//include form data
			HTTPGetFormDataCCMPage();		//ccmの設定画面
			HTTPPrintRedirect(1);				//リダイレクト？※
		}else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP2A[0]),&progPos)){	//include form data		
			HTTPGetFormDataLANSettingPage();		//LANの設定をEEPROOMに書き込む
			HTTPsendPageLANSetting();						//LANの設定ページ
		}else if(UECSFindPGMChar(UECSbuffer,&(UECSaccess_NOSPC_GETP3A[0]),&progPos)){
			int ccmid=HTTPGetFormDataEDITCCMPage();		//ccmの値をEEPROMに書き込んで返り値としてCCMidを返す
			//Type Reset
			//例外を除去する・
			if(ccmid==999){
				for(int i=0;i<U_MAX_CCM;i++){
					strcpy_P(U_ccmList[i].typeStr, U_ccmList[i].type);	//typeをコピーする
					UECS_EEPROM_SaveCCMType(i);		//EEPROMにccmtypeを書き込む
				}
				HTTPPrintRedirect(3);	//リダイレクト
			//何を確認しているのかよくわからない？※
			}else if(ccmid-100>=0 && ccmid-100<U_MAX_CCM){	//Attribute Reset	
				HTTPGetFormDataFillCCMAttributePage();		//データをEEPROMに保存
				HTTPPrintRedirect(3);		//リダイレクト
			}
			//Err
			//ccmidが範囲外であればエラーページを返す
			else if(ccmid<0 || ccmid>=U_MAX_CCM){
				HTTPsendPageError();
			}else{	//それ以外であればssmeditページの作成
				//CCM Edit
				HTTPsendPageEDITCCM(ccmid);
			}
				
		}else {
			HTTPsendPageError();		//エラーページを返す
		}
  }
  UECSclient.stop();	//サーバーとの接続を切断
}



//----------------------------------
//ccmの情報の取り込みと更新確認
//第一引数：
//第二引数：
void UECSupdate16520portReceive( UECSTEMPCCM* _tempCCM, unsigned long _millis){
	//https://garretlab.web.fc2.com/arduino_reference/libraries/standard_libraries/Ethernet/EthernetUDP/parsePacket.html
  int packetSize = UECS_UDP16520.parsePacket();		//パケットのサイズを取得、バッファを読む前に呼ぶ必要がある
  int matchCCMID=-1;
  if(packetSize){ 		//パケットを受信した時
   	ClearMainBuffer();	//バッファのクリア
    _tempCCM->address = UECS_UDP16520.remoteIP();   //リモートコネクションのIPアドレスを取得
    UECSbuffer[UECS_UDP16520.read(UECSbuffer, BUF_SIZE-1)]='\0';	//終端にはnullをつける
    UDPFilterToBuffer();		//不要なアスキーコードを除く
		if(UECSparseRec( _tempCCM,&matchCCMID)){		//ccmの情報をオブジェクトに入れる
			UECScheckUpDate( _tempCCM, _millis,matchCCMID);		//UDPが更新されていないかの確認
		}
  }
}

//----------------------------------
//スキャンポート
void UECSupdate16529port( UECSTEMPCCM* _tempCCM){
	int packetSize = UECS_UDP16529.parsePacket();		//パケットのサイズを確認
	if(packetSize){  	   
		ClearMainBuffer();
		_tempCCM->address = UECS_UDP16529.remoteIP();  	//ipアドレスを取得 
		UECSbuffer[UECS_UDP16529.read(UECSbuffer, BUF_SIZE-1)]='\0';	//終端にnullをつける
		UDPFilterToBuffer();		//不要なテキストを除去
		if(UECSresNodeScan()){	//スキャンに返信する
			//リモートコネクションに対してUDPデータの書き込みを開始する
			UECS_UDP16529.beginPacket(_tempCCM->address, 16529);
			UECS_UDP16529.write(UECSbuffer);		//バッファの内容を書き込み
			if(UECS_UDP16529.endPacket()==0){		//送信に失敗したらイーサネットの初期化
				UECSresetEthernet(); //when udpsend failed,reset ethernet status
			}
		}     
	}
}

//----------------------------------
//サーチに対する返信
//引数：読み出した内容の保存場所
void UECSupdate16521port( UECSTEMPCCM* _tempCCM){	//ccmのサーチと送信要求
  int packetSize = UECS_UDP16521.parsePacket();		//パケットサイズの把握
	if(packetSize){   
		ClearMainBuffer();
		_tempCCM->address = UECS_UDP16521.remoteIP();  	//ipアドレスの取得 
		UECSbuffer[UECS_UDP16521.read(UECSbuffer, BUF_SIZE-1)]='\0';	//終端にnullを入れる
	  UDPFilterToBuffer();		//余計なテキストの除去
	  UECSresCCMSearchAndSend(_tempCCM);		//ccmのサーチに対して返信
	}
}


//----------------------------------
void UECSsetup(){
	UECSCheckProgramUpdate();		//多分プログラムのアップデートチェック？
	delay(300);
	pinMode(U_InitPin,INPUT_PULLUP);		//ipアドレスのリセットピンの準備
	//セーフモードかどうかの確認
	if(digitalRead(U_InitPin) == U_InitPin_Sense || UECS_EEPROM_readLong(EEPROM_IP)==-1){
		U_orgAttribute.status|=STATUS_SAFEMODE;
  }else{
	 	U_orgAttribute.status&=STATUS_SAFEMODE_MASK;//Release safemode
	}

  UECSinitOrgAttribute();	//接続情報の確認
  UECSinitCCMList();			//UECSccmの初期化
  UserInit();							//メインファイルで設定する（ユーザー任意の初期化）
  UECSstartEthernet();		//イーサネットサーバーを立ち上げてポートを開く
  
}

//---------------------------------------------
//プログラムがアップデートされていないか確認する？
void UECSCheckProgramUpdate(){
	//Program Upadate Check
	ClearMainBuffer();
	UDPAddPGMCharToBuffer(&(ProgramDate[0]));		//日付けの取得
	UDPAddPGMCharToBuffer(&(ProgramTime[0]));		//時間の取得
	//check and write datetime
	int i;
	for(i=0;i<(EEPROM_CCMTOP-EEPROM_PROGRAMDATETIME);i++){
		//日時をEEPROMに書き込む
		if(EEPROM.read(i+EEPROM_PROGRAMDATETIME)!=UECSbuffer[i]){
			//何をやっているかよくわからない
			U_orgAttribute.status|=STATUS_PROGRAMUPDATE;
			EEPROM.write(i+EEPROM_PROGRAMDATETIME,UECSbuffer[i]);
		}
		if(UECSbuffer[i]=='\0'){	//終端にnullをつける
			break;
		}
	}
}
//---------------------------------------------

//--------------------------------------------------------------
//EEPROMにccmTypeを書き込む
//引数：ccmid
void UECS_EEPROM_SaveCCMType(int ccmid){
#if defined(_ARDUINIO_MEGA_SETTING)
	if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory
#endif
	int i;
	//type��������
	for(i=0;i<=MAX_CCMTYPESIZE;i++){
			//保存されているccmidと異なる時はccmidをEEPROMに保存する
		if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i)!=U_ccmList[ccmid].typeStr[i]){
			EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i,U_ccmList[ccmid].typeStr[i]);
		}
		if(U_ccmList[ccmid].typeStr[i]=='\0'){
			break;
		}		//nullで終端を把握
	}
}
//--------------------------------------------------------------
//ccmの属性をEEPROMに保存する
//引数：ccmid
void UECS_EEPROM_SaveCCMAttribute(int ccmid){
#if defined(_ARDUINIO_MEGA_SETTING)
	if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory
#endif
	//EEPROMでの保存内容と異なる時はccmの属性を書き込む
	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ROOM])){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ROOM]));
	}
	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_REGI])){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_REGI]));
	}
	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE])){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]));
	}
	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]/256)){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]/256));
	}
	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_PRIO])){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_PRIO]));
	}
}

//--------------------------------------------------------------
//EEPROMからccmの設定情報を読み出す
//引数：ccmid
void UECS_EEPROM_LoadCCMSetting(int ccmid){
#if defined(_ARDUINIO_MEGA_SETTING)
	if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory
#endif
	int i;
	for(i=0;i<MAX_CCMTYPESIZE;i++){
		//設定内容を読み出す
		U_ccmList[ccmid].typeStr[i]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i);
		//終端の把握
		if(U_ccmList[ccmid].typeStr[i]==255){
			U_ccmList[ccmid].typeStr[i]='x';break;
		}
		if(U_ccmList[ccmid].typeStr[i]=='\0'){
			break;
		}
	}
	//終端把握のためにnullをつける
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
	//読み出した設定内容をccmに代入する
	U_ccmList[ccmid].baseAttribute[AT_ROOM]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM);
	U_ccmList[ccmid].baseAttribute[AT_REGI]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI);
	U_ccmList[ccmid].baseAttribute[AT_ORDE]=(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L)+EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H)*256);
	U_ccmList[ccmid].baseAttribute[AT_PRIO]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO);

	//Prepare the correct values for the Arduino where the data will be written for the first time.
	//始めてデータを書き込むArduinoには正しい値を用意する
	//始めて使用する際の初期値のことだと思われる
	if(U_ccmList[ccmid].baseAttribute[AT_ROOM]==0xff){
		U_ccmList[ccmid].baseAttribute[AT_ROOM]=1;
		U_ccmList[ccmid].baseAttribute[AT_REGI]=1;
		U_ccmList[ccmid].baseAttribute[AT_ORDE]=1;
		U_ccmList[ccmid].baseAttribute[AT_PRIO]=U_ccmList[ccmid].attribute[AT_PRIO];
		UECS_EEPROM_SaveCCMAttribute(ccmid);		//ccmの属性をEEPROMに保存する
	}
	//プライオリティの設定
	U_ccmList[ccmid].attribute[AT_PRIO] =U_ccmList[ccmid].baseAttribute[AT_PRIO];
}

//---------------------------------------------
//イーサネットサーバーを立ち上げてポートを開く
void UECSstartEthernet(){
	//ethernetライブラリとネットワークの初期化　
	//https://garretlab.web.fc2.com/arduino_reference/libraries/standard_libraries/Ethernet/EthernetClass/begin.html
  //セーフモードの時の設定
  if(U_orgAttribute.status&STATUS_SAFEMODE){
  	byte defip[]     = {192,168,1,7};
  	//byte defsubnet[] = {255,255,255,0};
  	byte defdummy[] = {0,0,0,0};
  	Ethernet.begin(U_orgAttribute.mac, defip, defdummy,defdummy,defdummy);
	}else{//セーフモードでないときはネットワークの設定情報を使用する    
		Ethernet.begin(U_orgAttribute.mac, U_orgAttribute.ip, U_orgAttribute.dns, U_orgAttribute.gateway, U_orgAttribute.subnet);
	}
	//サーバーを立ち上げてポートを開く　※
  UECSlogserver.begin();
  UECS_UDP16520.begin(16520);		//デフォルト
  UECS_UDP16529.begin(16529);		//スキャン
  UECS_UDP16521.begin(16521);		//サーチ
}

//---------------------------------------------------------
//イーサネットの初期化
void UECSresetEthernet(){
  	//UECS_UDP16520.stop();
  	//UECS_UDP16529.stop();
  	//SPI.end();
  	UECSstartEthernet();		//イーサネットサーバーを立ち上げてポートを開く
}
//------------------------------------------------------------------------
//Software reset command　ソフトウェアリセット
// call 0
//https://ehbtj.com/electronics/arduino-software-reset/
typedef int (*CALLADDR)(void);
void SoftReset(void){
	CALLADDR resetAddr=0;
	(resetAddr)();
}


//---------------------------------------------------------
//---------------------------------------------------------
//----------------------------------------------------------------------
//uecsのメイン関数
void UECSloop(){

	/*前回のループから1秒経過していなくてもデータの受信はできるがデータは捨てる？
		おそらくタイマー割り込みで制御する方が妥当だと思われる
	*/
  // network Check
  // 1. http request
  // 2. udp16520Receive
  // 3. udp16529Receive and Send
  // 4. udp16521Receive and Send
  // << USER MANAGEMENT >>
  // 5. udp16520Send
  HTTPcheckRequest();		//httpのリクエストを確認
  UECSupdate16520portReceive(&UECStempCCM, UECSnowmillis);	//ccmの情報の取り込みと更新確認？
  UECSupdate16529port(&UECStempCCM);		//スキャン(他のノードからのデータの取り込み？)
  UECSupdate16521port(&UECStempCCM);		//サーチに対する返信
  UserEveryLoop();  										//ループ毎に行う処理（ユーザー設定）

 	UECSnowmillis = millis();							//現在の時間の把握
 	if(UECSnowmillis==UECSlastmillis){return;}	//前のループから時間が過ぎていなければ中断
 
	//how long elapsed?
	unsigned long td=UECSnowmillis-UECSlastmillis;		//前回のループからの経過時間
	//check overflow 	tdがオーバーフローしたらリセットする
	if(UECSnowmillis<UECSlastmillis){
		td=(4294967295-UECSlastmillis)+UECSnowmillis;
	}

	//over 1msec
	UECSsyscounter1s+=td;							//1秒までカウント
	UECSlastmillis=UECSnowmillis;			//経過時間を把握するための処理
	UECSautomaticValidManager(td);		//受信パケットが有効時間内かどうか確認する


  if(UECSsyscounter1s < 999){return;}		//1秒経過してなかったらここで中断
	//over 1000msec
	UECSsyscounter1s = 0;		//カウンターのリセット→カウンターはグローバルにある
	UECSsyscounter60s++;		//1分間をカウントしていく
    
	if(UECSsyscounter60s >= 60){		//1分間経過した時の処理を行う
		UserEveryMinute();
		UECSsyscounter60s = 0;  
	}     

	UECSautomaticSendManager();		//ロードバランシング
	UserEverySecond();						//毎秒行う処理（ユーザーが任意に書くことができる）
    
	for(int i = 0; i < U_MAX_CCM; i++){				//ccmの数だけ繰り返す
		//条件がそろっていたらccmを送信する
		if(U_ccmList[i].sender && U_ccmList[i].flagStimeRfirst && U_ccmList[i].ccmLevel != NONE){
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
			 //htmlをEEPROMから読み込む
	     *(U_html[i].data) = UECS_EEPROM_readLong(EEPROM_WEBDATA + i * 4);
		 } 
}
//------------------------------------------------------
//UECSccmを初期化する
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
//おそらくこの関数は使用されていない　※
//
//引数：設定内容
//
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
	//PROGMEM領域にある文字列の連結
  strcat_P(U_ccmList[_num].typeStr,U_ccmList[_num].type);

if(U_orgAttribute.status&STATUS_PROGRAMUPDATE)		//新しいプログラムをロードするときは
	{
  	UECS_EEPROM_SaveCCMType(_num);		//ccmTypeを書き込む　引数はccmid
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
//EEPROMから読み出してバッファに加える
//引数：読み出す読み込み専用のデータ
void UDPAddPGMCharToBuffer(const char* _romword)
{
   for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++)
   {
		//読み出した値をバッファに加える
    UECSbuffer[wp]=pgm_read_byte(&_romword[i]);
		//nullが出たら終了
    if(UECSbuffer[wp]=='\0'){break;}

    wp++;
  }
  #if defined(_ARDUINIO_MEGA_SETTING)
      MemoryWatching();
  #endif

}
//-----------------------------------
//バッファに文字を追加していく
//引数：連結する文字
void UDPAddCharToBuffer(char* word){
	//バッファに文字を追加
	strcat(UECSbuffer,word);
	wp=strlen(UECSbuffer);		//バッファのサイズを把握
  #if defined(_ARDUINIO_MEGA_SETTING)
	 MemoryWatching();
  #endif

}
//-----------------------------------
//値をバッファに入れる
//引数：バッファに入れたい値
void UDPAddValueToBuffer(long value){
	sprintf(&UECSbuffer[wp], "%ld", value);
	wp=strlen(UECSbuffer);
	
  #if defined(_ARDUINIO_MEGA_SETTING)
	MemoryWatching();
  #endif
}
//-----------------------------------
//const charに保存した文字の配列をUECSバッファに保存
//引数：const charに保存した文字のアドレス
void HTTPAddPGMCharToBuffer(const char* _romword){
	//nullが入力されるか最大サイズまで繰り返す
	for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++){
    UECSbuffer[wp]=pgm_read_byte(&_romword[i]);		//const charの文字をバッファに保存
    if(UECSbuffer[wp]=='\0'){		//終端のnullが出てきたら終了
			break;
		}
    wp++;
    //auto send
    if(wp>BUF_HTTP_REFRESH_SIZE){		//バッファがあふれそうになったらクライアントに表示してクリア
			HTTPRefreshBuffer();
    }
  }
  #if defined(_ARDUINIO_MEGA_SETTING)
      MemoryWatching();
  #endif
}
//---------------------------------------------
//引数の文字列をUECSバッファに保存	
//引数：入力したい文字列
void HTTPAddCharToBuffer(char* word){
	//nullが入力されるか最大サイズまでバッファに入れていく
	for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++){
    UECSbuffer[wp]=word[i];
    if(UECSbuffer[wp]=='\0'){		//nullが入力されたら終了
			break;
		}
    wp++;
    //auto send
    if(wp>BUF_HTTP_REFRESH_SIZE){
			HTTPRefreshBuffer();		//バッファがあふれそうになったらクライアントに表示してクリア
		}
  }
  #if defined(_ARDUINIO_MEGA_SETTING)
		MemoryWatching();
  #endif
}
//---------------------------------------------
//引数で入ってきた数値をバッファに保存する
//引数：保存したい数値
void HTTPAddValueToBuffer(long value){
	//引数で入って来た数値を文字に変換
	sprintf(&UECSbuffer[wp], "%ld", value);
	wp=strlen(UECSbuffer);
	//バッファのサイズが指定した値より大きくなりそうな時はバッファの消去処理
	if(wp>BUF_HTTP_REFRESH_SIZE){
		HTTPRefreshBuffer();
	}
  #if defined(_ARDUINIO_MEGA_SETTING)
  	MemoryWatching();
  #endif
}

//clientに表示してバッファを消去する
void HTTPRefreshBuffer(void){		
	UECSbuffer[wp]='\0';		//nullがあったら
	UECSclient.print(UECSbuffer);		//クライアントに表示
	ClearMainBuffer();		//バッファのクリア
}

//UECSバッファがあるときはクライアントに表示する
void HTTPCloseBuffer(void){
	if(strlen(UECSbuffer)>0){
		UECSclient.print(UECSbuffer);
	}
}

//------------------------------------
//delete \r,\n and contorol code
//replace the continuous space character to 1 space
//Attention:first one character is ignored without delete.
//------------------------------------
//スペースや改行コード等不要なアスキーコードを除く
void UDPFilterToBuffer(void)
{

int s_size=strlen(UECSbuffer);
int write=0,read=0;
int i,j;
  for(i=1;i<s_size;i++)
  {
    if(UECSbuffer[i]=='\0'){break;}
    
    //find space
		//アスキーコードでスペースより小さいかスペースが2文字続いた場合
    if(UECSbuffer[i]<ASCIICODE_SPACE || (UECSbuffer[i]==ASCIICODE_SPACE && UECSbuffer[i-1]==ASCIICODE_SPACE))
      {
        write=i;
        //find end of space
        for(j=write;j<=s_size;j++)
          {
						//文字か改行コードがでてきたら終了
          if(UECSbuffer[j]>ASCIICODE_SPACE || UECSbuffer[j]=='\0')
            {
            read=j;
            break;		//場所をreadに確保して終了
            }
          }
        //copy str to fill space
				//先ほど終了したところから入れていく（不必要なところを除く）
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
//httpレスポンスから不要なところを削除
//------------------------------------
void HTTPFilterToBuffer(void)
{

//文字列バッファの長さを取得
int s_size=strlen(UECSbuffer);
//初期化と宣言
int write=0,read=0;
int i,j;

  //decode after %
	//バッファに入っている文字列の長さだけ繰り返す
  for(i=1;i<s_size;i++)
  {
		//スペースか、&S=がでてきたら終了
    if((unsigned char)UECSbuffer[i]<ASCIICODE_SPACE || (UECSbuffer[i-1]=='&' && UECSbuffer[i]=='S' && UECSbuffer[i+1]=='='))
		{UECSbuffer[i]='\0';break;}
    
		//%が出て来たら
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
//指定した文字列を探す
//第一引数：比較する場所
//第二引数：比較する文字列
//第三引数：同じ文字があるところを保存する場所のポインタ
//返り値：同じ文字列があるかないか
bool UECSFindPGMChar(char* targetBuffer,const char *_romword_startStr,int *lastPos)
{
//初期化
*lastPos=0;
int startPos=-1;
//文字列の大きさを把握
int _targetBuffersize=strlen(targetBuffer);
//引数のPROGMEMの長さを把握
int _startStrsize=strlen_P(_romword_startStr);
//保存するバッファサイズより入力するサイズが大きいときは中止
if(_targetBuffersize<_startStrsize){return false;}


int i,j;

//-------------start string check
//引数で指定したアドレスから読み出す
unsigned char startchr=pgm_read_byte(&_romword_startStr[0]);
//入力された文字列の長さから
for(i=0;i<_targetBuffersize-_startStrsize+1;i++)
	{
	//not hit
	//同じ文字がないときは続きを行う
	if(targetBuffer[i]!=startchr){continue;}
	
	//if hit 1 chr ,more check
	for(j=0;j<_startStrsize;j++)
			{
			//同じ文字ではなくなるまで繰り返す
			if(targetBuffer[i+j]!=pgm_read_byte(&_romword_startStr[j])){break;}//not hit!
			}
	//hit all chr
	//最後までいったら終了
	if(j==_startStrsize)
		{
		startPos=i;
		break;
		}
	
	}

if(startPos<0){return false;}
//処理の最後に残ったアドレスを保存する
*lastPos=startPos+_startStrsize;
return true;

}

//------------------------------------
//値を取得する
//第一引数：目的とする場所
//
//
//第三引数：最後の目印にする文字（"\"）
//
//返り値：
bool UECSGetValPGMStrAndChr(char* targetBuffer,const char *_romword_startStr, char end_asciiCode,short *shortValue,int *lastPos)
{
//ターゲットのバッファのサイズを把握
int _targetBuffersize=strlen(targetBuffer);
*lastPos=-1;
int startPos=-1;
int i;
//指定した文字列がないときはfalseを返す
if(!UECSFindPGMChar(targetBuffer,_romword_startStr,&startPos)){false;}
//数字以外の時はfalse
if(targetBuffer[startPos]<'0' || targetBuffer[startPos]>'9'){return false;}

//------------end code check
//目印(\)が見つからなかったら終了
for(i=startPos;i<_targetBuffersize;i++)
{
if(targetBuffer[i]=='\0'){return false;}//no end code found
if(targetBuffer[i]==end_asciiCode)
	{break;}
	
}

//ターゲットのバッファサイズを超えたらfalseを返す
if(i>=_targetBuffersize){return false;}//not found
*lastPos=i;		//最後に到達したアドレスを保存する

//*shortValue=UECSAtoI(&targetBuffer[startPos]);
long longVal;
unsigned char decimal;
int progPos;
//
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
//floatをlong(?)に変換する
//第一引数：読み出すアドレス
//第二引数：変換した値を入れておくアドレス
//第三引数：小数点の桁数
//第四引数：最後のアドレス
//返り値：変換ができたかどうか
bool UECSGetFixedFloatValue(char* targetBuffer,long *longVal,unsigned char *decimal,int *lastPos)
{
//初期化
*longVal=0;
*decimal=0;
//領域の確保
int i;
int validFlag=0;
bool floatFlag=false;
char signFlag=1;
unsigned long newValue=0;
unsigned long prvValue=0;

//おそらく16bitだということ
for(i=0;i<MAX_DIGIT;i++)
{
//小数点を見つける
if(targetBuffer[i]=='.')
	{
	//小数点が二重にあったら処理を中断してfalseを返す
	if(floatFlag){return false;}//Multiple symbols error
	floatFlag=true;			//小数のフラグ
	continue;						//スキップ
	}
else if(targetBuffer[i]=='-')			//マイナスがあるときは
	{
	//数字の後にマイナスがあるときは処理を中断してfalseを返す
	if(validFlag){return false;}//Symbol is after number
	//既にマイナスが代入されているときは間違って二回マイナスが入っている
	if(signFlag<0){return false;}//Multiple symbols error
	//マイナスがあるときはマイナスフラグを立てる
	signFlag=-1;
	continue;	//以下の処理をスキップ
	}
//0~9の間に入っている場合
else if(targetBuffer[i]>='0' && targetBuffer[i]<='9')
	{
	//validフラグを立てる
	validFlag=true;
	//小数の時は小数点の位置を確認（小数点何桁か把握する？）
	if(floatFlag){(*decimal)++;}
	
	//10倍しながら数字を1の位から詰めていく（次の値を入れるために10倍する）
	//10の位の重みづけを行っている
	newValue=newValue*10;
	newValue+=(targetBuffer[i]-'0');
	
	//オーバーフローしていないか確認する
	if(prvValue>newValue || newValue>2147483646){return false;}//over flow!

	prvValue=newValue;
	continue;
	}

	break;		//最後まで処理したらforを抜ける
}

*longVal=((long)newValue) * signFlag;		//符号を確認する
*lastPos=i;			//小数点の桁数？

return validFlag;		//変換ができたかどうか
}


//------------------------------------
#if defined(_ARDUINIO_MEGA_SETTING)
//
void MemoryWatching()
{
if(UECStempStr20[MAX_TYPE_CHAR-1]!=0 || UECSbuffer[BUF_SIZE-1]!=0)
	{U_orgAttribute.status|=STATUS_MEMORY_LEAK;}
}
#endif
//------------------------------------
//In Safemode this function is not working properly because Web value will be reset.
//セーフモードではwebの値がリセットされるため、この機能はうまく動作しない
//webインターフェースに設定された値の書き換え（EEPROMの書き換え）
//第一引数：書き換えたい変数のアドレス
//第二引数：書き込む値
//返り値：書き込み成功でtrue
bool ChangeWebValue(signed long* data,signed long value)
{
//web画面で表示できる項数だけ繰り返す
for(int i=0;i<U_HtmlLine;i++)
{
//書き換えたい変数のアドレスを把握する
if(U_html[i].data==data)
	{
	//変更する値を書き込む
	*(U_html[i].data)=value;
	//EEPROMに書き込んでいく
	UECS_EEPROM_writeLong(EEPROM_WEBDATA + i * 4, *(U_html[i].data));
	return true;
	}
}
//アドレスが想定通りでなければfalseを返す
return false;
}

/********************************/
/* 16521 Response   *************/
/********************************/
//CCMのサーチに対する応答
//引数：作成したUECSオブジェクトの保存場所
//返り値：処理に成功したかどうか
boolean UECSresCCMSearchAndSend(UECSTEMPCCM* _tempCCM){
	//CCM provider search response
	/*
	In periodic transmission, only CCMs with a proven track record of transmission will return a response.
	Among regularly sent CCMs, CCMs that have not been sent at the specified frequency will not return a response even if they are registered. 
	This is to prevent accidental reference to broken sensors.
	定期送信では送信実績のあるCCMのみ応答を返す
	*/
  
	//初期化
	int i;
	int progPos = 0;
	int startPos = 0;
	short room=0;
	short region=0;
	short order=0;
	
	//XMLのヘッダー確認、なければ終了してfalseを返す
	if(!UECSFindPGMChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){return false;}
	startPos+=progPos;		//保存されている場所を確認
	//UECSのバージョンを確認する
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos)){return false;}
	startPos+=progPos;
	//typeの確認の準備をする
	if(!UECSFindPGMChar(&UECSbuffer[startPos],&UECSccm_CCMSEARCH[0],&progPos)){return false;}
	startPos+=progPos;
	
	
	//copy ccm type string
	//ccmのtypeを把握する
	for(i=0;i<MAX_TYPE_CHAR;i++)
	{
	//一時保存領域に入れていく
	UECStempStr20[i]=UECSbuffer[startPos+i];
	//「"」かnullの時は　nullを入れて終了
	if(UECStempStr20[i]==ASCIICODE_DQUOT || UECStempStr20[i]=='\0')
		{UECStempStr20[i]='\0';break;}
	}
	//終端の確認のためにnullを入れる
	UECStempStr20[MAX_CCMTYPESIZE]='\0';
	startPos=startPos+i;		//アドレスの次に進める
	
	//Extract "room", "region", and "order". If not found, it is assumed to be zero.
	//room"、"region"、"order "を抽出する。見つからない場合は、0とする。
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_ROOM[0],'\"',&room,&progPos);
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_REGION[0],'\"',&region,&progPos);
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_ORDER[0],'\"',&order,&progPos);
	//Serial.print(room);Serial.print("-");
	//Serial.print(region);Serial.print("-");
	//Serial.print(order);Serial.print("\n");
	
	
for(int id=0;id<U_MAX_CCM;id++)
	{
	//ccmを受信した時の動作
	if(UECSCCMSimpleHitcheck(id,room,region,order))
		{
		//Serial.print(id);Serial.print("/");
		//Serial.println(U_ccmList[id].typeStr);
		//packet create

		//バッファの初期化
		ClearMainBuffer();
			//ヘッダーの情報とccmの情報をバッファに加えていく
    	UDPAddPGMCharToBuffer(&(UECSccm_XMLHEADER[0]));
    	UDPAddPGMCharToBuffer(&(UECSccm_UECSVER_E10[0]));
    	UDPAddPGMCharToBuffer(&(UECSccm_CCMSERVER[0]));
    	UDPAddCharToBuffer(UECStempStr20);		//XMLのために文字列を加える
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
		//セーフモードの時はipアドレスを192.168.1.7にする
	    if(U_orgAttribute.status & STATUS_SAFEMODE)
			{
			UDPAddPGMCharToBuffer(&(UECSdefaultIPAddress[0]));
			}
		else
			{
				//セーフモードでない時は取得したipアドレスを文字列に変換する
		    sprintf(iptxt, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2], U_orgAttribute.ip[3]);
		    UDPAddCharToBuffer(iptxt);	//ipアドレスをバッファに加える
		    }
    	UDPAddPGMCharToBuffer(&(UECSccm_CCMSERVERCLOSE[0]));		//XMLを閉じるタグ

    	
    	//----------------------------------------------send 
		//https://garretlab.web.fc2.com/arduino_reference/libraries/standard_libraries/Ethernet/EthernetUDP/beginPacket.html
		//リモートコネクションに対してUDPデータの書き込みを開始する
		//第一引数：リモートコネクションのホスト名
		//第二引数：リモートコネクションのポート
		//返り値：成功 true,  失敗 false
		UECS_UDP16521.beginPacket(_tempCCM->address, 16521);
		//リモート接続先にUDPデータを書き出す
		//引数：送信するメッセージ
        UECS_UDP16521.write(UECSbuffer);
				//リモートコネクションに対してUDPデータを書き込んだ後に呼ぶ。
				//成功でtrue
	        if(UECS_UDP16521.endPacket()==0)	//送信に失敗したらイーサネットの初期化
	         	{
	  			UECSresetEthernet(); //when udpsend failed,reset ethernet status
	         	}

		}
	
	}
		//無事に応答できたらtrueを返す
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