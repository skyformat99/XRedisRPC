#pragma once

#include "common_tool.h"
//#ifdef _WIN32
//#include <process.h>
//#define sleep(x) Sleep(x)
//#else
//#include <unistd.h>
//#define sleep(x) usleep((x)*1000)
//#endif

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#ifdef _WIN32
#include <Win32_Interop/win32fixes.h>
extern "C"
{
#include <hiredis/adapters/ae.h>
}
#else
#include <hiredis/adapters/libevent.h>
#endif
#include "include/easylog/easylogging++.h"

#define INFOLOG LOG(INFO)<<__FUNCTION__<<"***"
#define TRACELOG LOG(TRACE)<<__FUNCTION__<<"***"
#define WARNLOG LOG(WARNING)<<__FUNCTION__<<"***"
#define DEBUGLOG LOG(DEBUG)<<__FUNCTION__<<"***"
#define ERRORLOG LOG(ERROR)<<__FUNCTION__<<"***"

#define REDIS_BUF_SIZE 1024 * 2

//redis����
#define R_GET		"GET"
#define R_SET		"SET"
#define R_PUSH		"LPUSH"
#define R_POP		"RPOP"
#define R_UNSUBS	"UNSUBSCRIBE"
#define R_SUBS		"SUBSCRIBE"
#define R_PSUBS		"PSUBSCRIBE"
#define R_RENAME	"RENAME"
#define R_EXISTS	"EXISTS"
#define R_DEL		"DEL"
#define R_KEYSPACE	"__keyspace@0__:"

//RPC list
#define HEARTLIST	"heart_beart_redisRPC_list"
#define REQLIST		"request_redisRPC_list"
#define REPLIST		"respond_redisRPC_list"

#define HEARTBEATTIMEOUT 10		//10s������heartbeat

typedef void(*getCallback)(const char *key);

typedef std::map<std::string, getCallback>  mapgetCB;	//getkey-->getfunc
typedef std::map<std::string, std::string>  mapReqchnl;	//getkey-->requestChnl
typedef std::map<std::string, std::string>  mapHBchnl;	//getkey-->HeartBeatChnl

class CRedisRPC
{
public:
	CRedisRPC();
	CRedisRPC(std::string ip, int port);
	~CRedisRPC();

	bool connect(const char* ip, int port);		//����redis����
	bool isServiceModelAvailable(const char *key);					//����Ƿ��п��÷���
	bool isKeySubs(const char *key);			//���key�Ƿ���Ҫ����

	void processKey(const char *key, int timeout);			//����key, timeout--ms

	//ҵ����ģ��
	void subsClientGetOp(const char *keys, const char *reqChlName,
		const char *heartbeatChnName);		//ע��key�Լ��ص�����
private:
	static void* thTimeout(void *arg);

	mapgetCB   m_getKeys;		
	mapReqchnl m_reqChnl;
	mapHBchnl  m_HBChnl;

	std::string ip;				//redis ip
	int port;					//redis �˿�
	std::string requestID;
	redisContext *context;

	static bool keyProcessDone;
};

