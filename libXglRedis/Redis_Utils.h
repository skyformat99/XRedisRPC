#pragma once

#include "RedisRPC.h"

//#ifdef _WIN32
//#include <WinSock2.h>
//#else
//#include <sys/time.h>
//#endif // _WIN32


typedef void(*subsCallback)(const char *lpStrKey, const char *lpStrValue);
typedef void(*pullCallback)(const char *lpStrKey, const char *lpStrValue);
typedef void(*clientOpCallBack)(const char *lpStrKey, const char *lpStrValue);

typedef std::map<std::string, subsCallback> mapSubsCB;		//subkey-->subfunc
typedef std::map<std::string, pullCallback> mapPullCB;		//pullkey-->subfunc
typedef std::map<std::string, clientOpCallBack> mapReqCB;	//getkey-->getfunc

class CRedis_Utils
{
public:
	CRedis_Utils(std::string lpStrClientID);
	~CRedis_Utils();

	//�ͻ��˻�����������
	/*
	isSubs˵����Ĭ��false
		false����ʹ��redis�ļ��ռ�֪ͨ���ܣ���subs��pull��subsClientGetOp�Ƚӿڲ�������
		true������redis�ļ��ռ�֪ͨ����
	*/
	bool connect(const char* lpStrIp, int iPort, bool bNeedSubs = false);		//����redis���񲢶���redis���ռ�֪ͨ
	void disconnect();

	//redis��������
	/**
		get()    pop()
		����ֵ˵����
			-1������ʧ�ܣ�ʧ��ԭ��ͨ��lpStrRlt�鿴
			 0��lpStrKey��Ӧ��ֵΪ��
			>0�������ɹ���lpStrKey��ֵͨ��lpStrRlt�鿴������ֵ�ĳ���

		set()    push()
		����ֵ˵����
			true�������ɹ������ͨ��lpStrRlt�鿴��һ��ΪOK
			false������ʧ�ܣ����ͨ��lpStrRlt�鿴
	  */ 
	int  get(const char* lpStrKey, char *lpStrRlt);		
	bool set(const char* lpStrKey, const char* lpStrValue, char *lpStrRlt);
	bool push(const char* lpStrListName, const char* lpStrValue, char *lpStrRlt);
	int  pop(const char* lpStrListName, char *lpStrRlt);

	//redis���Ĺ���
	void subs(const char *lpStrKey, subsCallback cb);	//subscribe channel
	void unsubs(const char *lpStrKey);					//unsubscribe channel
	void pull(const char *lpStrKey, pullCallback cb);	//pull list
	void unpull(const char *lpStrKey);					//unpull list, like unsubs

	//ҵ����ģ��
	void subsClientGetOpSimple(const char *lpStrKey, clientOpCallBack cb);
	void subsClientGetOp(const char *lpStrKey, const char *reqChlName,
		const char *heartbeatChnName, clientOpCallBack cb);				//ע������ͻ���get key����
	void unsubClientGetOp(const char *lpStrKey);						//ע�������ͻ���get key����
	void stopSubClientGetOp();											//ȡ�������ͻ���ȫ��get����

private:
	void close();
	//ʵ��ҵ��ģ������ݸ���
	std::string genNewKey(std::string lpStrKeyOldKey);					//��װ�û������key				
	std::string getOldKey(std::string lpStrKeyNewKey);					//��ȡ�ͻ���ԭʼkey
	bool sendCmd(const char *lpStrCmd, char *lpStrRlt);					//����redis����
	bool replyCheck(redisReply *rRedisReply, char *lpStrReply);		//����redisӦ����Ϣ

	static void* thAsyncSubsAll(void *arg);						//redis���ռ�֪ͨ

	static void connectCallback(const redisAsyncContext *c, int iStatus);	//redis�첽�ص�����
	static void disconnectCallback(const redisAsyncContext *c, int iStatus);
	static void subsAllCallback(redisAsyncContext *c, void *r, void *data);

	void callSubsCB(const char *lpStrKey, const char *lpsStrKeyOp);
	
	redisContext *m_pRedisContext;			//redisͬ��������
	redisAsyncContext *m_pRedisAsyncContext;	//redis�첽������
	
#ifdef _WIN32				//redis�첽�¼���
	aeEventLoop *m_loop;
#else
	struct event_base *m_base;
#endif

	mapSubsCB m_mapSubsKeys;		//������Ϣ  ��-->�ص�����
	mapPullCB m_mapPullKeys;		//pull��Ϣ  ��-->�ص�����
	mapReqCB  m_mapReqChnl;			//�������	������-->�ص�����

	mutex m_getLock;				//���߳�ͬ����
	mutex m_setLock;
	mutex m_pushLock;
	mutex m_popLock;
	mutex m_subsLock;
	mutex m_pullLock;
	mutex m_reqLock;

	std::string m_lpStrIp;				//redis ip
	int m_iPort;					//redis �˿�

	CRedisRPC m_redisRPC;			//redis RPC����ʵ��

	bool m_bNeedSubs;				//�Ƿ���Ҫredis���Ĺ���
	bool m_bIsConnected;			//����redis�ɹ���־
	std::string m_lpStrClientId;		//�ͻ��˱�־
};

