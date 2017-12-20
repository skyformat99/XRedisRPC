#pragma once

//#ifdef _WIN32
//#include <process.h>
//#define sleep(x) Sleep(x)
//#else
//#include <unistd.h>
//#define sleep(x) usleep((x)*1000)
//#endif

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <signal.h> 
#ifdef _WIN32
#include <Win32_Interop/win32fixes.h>
#include "CommonTools.h"
extern "C"
{
#include <hiredis/adapters/ae.h>
}
#else
#include "common_tool.h"
#include <hiredis/adapters/libevent.h>
#endif

#define _INFOLOG(inf) LOG4CPLUS_INFO(g_ECGLogger->logger, "[" << __FUNCTION__ << "] " << inf)
#define _TRACELOG(inf) LOG4CPLUS_TRACE(g_ECGLogger->logger, "[" << __FUNCTION__ << "] " << inf)
#define _DEBUGLOG(inf) LOG4CPLUS_DEBUG(g_ECGLogger->logger, "[" << __FUNCTION__ << "] " << inf)
#define _WARNLOG(inf) LOG4CPLUS_WARN(g_ECGLogger->logger, "[" << __FUNCTION__ << "] " << inf)
#define _ERRORLOG(inf) LOG4CPLUS_ERROR(g_ECGLogger->logger, "[" << __FUNCTION__ << "] " << inf)
#define _FATALLOG(inf) LOG4CPLUS_FATAL(g_ECGLogger->logger, "[" << __FUNCTION__ << "] " << inf)

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
#define HEARTSLOT		"_heart_beart_slot"
#define REQPROCESSING	"_processing?"
#define REQSLOT			"_request_slot"

#define HEARTBEATTIMEOUT	10		//10s������heartbeat
#define HEARTBEATINTERVAL	3
#define GET_WAITTIMEOUT 10000


#define REDIS_TIMEOUT			100		//ҵ��ģ�鴦��ʱ
#define REDIS_NOSERVICE			101		//��ҵ����ģ��
#define REDIS_SENDREQFAIL		102		//��������ʧ��
#define REDIS_CONNFAIL			103		//redis��������ʧ��
#define REDIS_CMD_ERR			105		//��������ʧ��
#define REDIS_KEY_NULL			104		//�����keyֵΪ��
#define REDIS_VALUE_NULL		105		//�����valueֵΪ��
#define REDIS_KEY_EXISTED		106		//key�ѱ�����
#define REDIS_SUBS_OFF			107		//δ����redis���ռ�֪ͨ����

typedef std::map<std::string, std::string>		mapReqchnl;	//getkey-->requestChnl
typedef std::map<std::string, std::string>		mapHBchnl;	//getkey-->HeartBeatChnl

class CRedisRPC
{
public:
	CRedisRPC();
	//CRedisRPC(std::string lpStrIp, int iPort);
	~CRedisRPC();

	bool connect(const char* lpStrIp, int iPort);		//����redis����
	bool isServiceModelAvailable(const char *lpStrKey);					//����Ƿ��п��÷���
	bool isKeySubs(const char *lpStrKey);			//���key�Ƿ���Ҫ����

	int processKey(const char *lpStrKey);			//����key, timeout--ms
	//ҵ����ģ��
	void subsClientGetOp(const char *lpStrKey, const char *reqChlName,
		const char *heartbeatChnName);		//ע��key�Լ��ص�����
	std::string unsubClientGetOp(const char *keys);
	void clearChnl();
	void setClientId(const std::string & strClientId);
	void setRedisAddr(const std::string & strIp, const int iPort);
private:
	bool sendReq(redisContext *c, std::string strReqCmd, std::string strProcesssCmd);
	static void* thTimeout(void *arg);
	static void* thSetHeartBeat(void *arg);
	std::mutex hbLock;
	std::thread thHeartBeat;
	mapReqchnl m_mapReqChnl;
	mapHBchnl  m_mapHBChnl;

	std::string m_strIp;			//redis ip
	int m_iPort;					//redis �˿�
	std::string m_strRequestID;
	redisContext *m_redisContext;
	std::string m_strClientId;
	std::string m_strCurrentProcessKey;

	bool m_bKeyProcessDone;
};

