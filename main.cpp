#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <sstream>
#include <openssl/md5.h>
#include <stdio.h>

#include "inc/json.h"
#include "WXBizMsgCrypt.h"
#include "http_proto.h"
#include <iostream>

#define MAX_KEY_LEN 128
using namespace std;
static std::string getSignFromTimeStampKey(unsigned int uiTimeStamp, std::string strKey)
{
	char comkey[MAX_KEY_LEN] = {0};
	snprintf(comkey, MAX_KEY_LEN, "%u%s", uiTimeStamp, strKey.c_str());

	MD5_CTX ctx;
	unsigned char md[16] = {0};
	MD5_Init(&ctx);
	MD5_Update(&ctx, comkey, strlen(comkey));
	MD5_Final(md, &ctx);

	char buf[33]={'\0'};
	char tmp[3]={'\0'};
	for(int i = 8; i < 16; i++ )
	{
		snprintf(tmp, 3, "%02x", md[i]);
		strncat(buf, tmp, 3);
	}

	return buf;
}


struct in_addr* GetIpByHost(const char* pszHost, char *pszIp, int iMaxLen)
{
	struct hostent * host_addr = gethostbyname(pszHost);
	if (host_addr == NULL)
	{
		return 0;
	}

	struct in_addr *in = (struct in_addr *) host_addr->h_addr;

	if (pszIp != NULL)
	{
		char *sIp = inet_ntoa(*in);
		strncpy(pszIp, sIp, iMaxLen);
	}
	return in;
}
static int isValidIPAddr(char *sIP)
{
	u_int32_t uiIpAddress;
	if (NULL == sIP)
		return 0;

	if (strlen(sIP) < 7 || strlen(sIP) > 15)
		return 0;

	uiIpAddress = inet_addr(sIP);
	if (uiIpAddress == INADDR_NONE || uiIpAddress == INADDR_ANY)
		return 0;
	return 1;
}


int main(int argc, char *argv[])
{
	// check param
	if(argc < 7)
	{
		fprintf(stderr,"Usage:%s fileMd5 host appid key port fileType(0,1:apk,2:pe)\n", argv[0]);
		exit(1);
	}


	// get param
	std::string ip;
	// ip check and get
	if (0 == isValidIPAddr(argv[2]))
	{
		char szDestIp[20] =	{ 0 };
		GetIpByHost(argv[2], szDestIp, sizeof(szDestIp));
		ip = szDestIp;
	}
	else
	{
		ip = argv[2];
	}

	printf("ip:%s\n", ip.c_str());
	std::string fileMd5 =  argv[1];
	int appid = atoi(argv[3]);
	int fileType = atoi(argv[6]);
	string version = "1.0";
	std::string strKey =  argv[4];
	// pack request
	std::string reqstr;
	Json::Value req;

	req["header"]["appid"] = Json::Value(appid);
	time_t tnow = time(NULL);
	unsigned int uinow = tnow; // todo
	req["header"]["timeStamp"] = Json::Value(uinow);
	req["header"]["version"] = Json::Value(version);
	req["header"]["ip"] = Json::Value(ip);
	std::string sign = getSignFromTimeStampKey(uinow, strKey);
	//printf("sign: %s\n", sign.c_str());
	req["header"]["sign"] = Json::Value(sign);


	Json::Value urllist;
	Json::Value urlattr;
	urlattr["id"] = Json::Value(0);
	urlattr["fileMd5"] = Json::Value(fileMd5);
	urlattr["fileType"] = Json::Value(fileType);
	urllist.append(urlattr);

	Json::FastWriter fast_writer;
	std::string reqinfo = fast_writer.write(urllist);
	//printf("reqinfo=:%s\n",reqinfo);
	cout<<"reqinfo=:"<<reqinfo<<endl;
	EncryptAndDecrypt::WXBizMsgCrypt crypt(strKey);
	std::string encrypt_reqinfo;
	crypt.EncryptMsg(reqinfo, encrypt_reqinfo);
	
	req["reqinfo"] = Json::Value(encrypt_reqinfo);

	reqstr = fast_writer.write(req);

	printf("request:\n%s\n", reqstr.c_str());


	// pack http, send request and recv response
	std::string strRespHeader;
	std::string strRespBody;
	//	    for(int i=0;i<20000;i++)
	{

		printf("request len: %d\n", reqstr.length());
		int ii = HttpPostRequest ( argv[2], atoi(argv[5]), "POST /", reqstr.c_str(), strRespHeader, strRespBody);
		if(ii !=0)
		{
			std::cout<<"tcp failed! continue..."<<std::endl;
			//continue;
		}


		printf("------------------------ response begin ---------------------\n");
		printf("response header:\n%s\n", strRespHeader.c_str());
		printf("response body:\n%s\n", strRespBody.c_str());

		Json::Reader reader;
		Json::Value value;
		if(!reader.parse(strRespBody.c_str(), value))
		{
			cout<<"reader.parse.error!"<<endl;
			// error
			return 0;
		}

		std::string echostr = value["echostr"].asString();
		std::string msg = value["msg"].asString();
		int status = value["status"].asUInt();
		printf("get each attr from response: \n");
		printf("status: %d\n", status);
		printf("msg: %s\n", msg.c_str());
		
		Json::Reader reader_arr;
		Json::Value value_arr;
		std::string tmp = value["rspBody"].asString();
		std::string decrypt;
		crypt.DecryptMsg(tmp,decrypt);
		cout<<"decrypt:"<<decrypt<<endl;
		if(!reader_arr.parse(decrypt.c_str(), value_arr))
		{
			cout<<"reader.parse.error!"<<endl;
			// error
			return 0;
		}
		// get attr
			for(int i = 0; i < value_arr["fileArray"].size(); i++)
			{
				int id = value_arr["fileArray"][i]["id"].asUInt();
				std::string fileName = value_arr["fileArray"][i]["fileName"].asString();
				int fileType = value_arr["fileArray"][i]["fileType"].asUInt();
				string fileMd5 = value_arr["fileArray"][i]["fileMd5"].asString();
				int safeType = value_arr["fileArray"][i]["safeType"].asUInt();
				std::string virusName = value_arr["fileArray"][i]["virusName"].asString();
				std::string virusDesc = value_arr["fileArray"][i]["viursDesc"].asString();
				printf("fileArray: %d,%s, %d, %s, %d, %s,%s\n", id,fileName.c_str(),fileType,fileMd5.c_str(),safeType,virusName.c_str(),virusDesc.c_str());
			}

		}
	return 0;

}
