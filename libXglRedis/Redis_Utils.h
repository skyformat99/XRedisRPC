#pragma once

#include "RedisRPC.h"

//#ifdef _WIN32
//#include <WinSock2.h>
//#else
//#include <sys/time.h>
//#endif // _WIN32


typedef void(*subsCallback)(const std::string & strKey, const std::string & strValue);
typedef void(*pullCallback)(const std::string & strKey, const std::string & strValue);
typedef void(*clientOpCallBack)(const std::string & strKey, const std::string & strValue);

typedef std::map<std::string, subsCallback> mapSubsCB;		//subkey-->subfunc
typedef std::map<std::string, pullCallback> mapPullCB;		//pullkey-->subfunc
typedef std::map<std::string, clientOpCallBack> mapReqCB;	//getkey-->getfunc

class CRedis_Utils
{
public:
	CRedis_Utils(std::string strClientID);
	~CRedis_Utils();

	//�ͻ��˻�����������
	/*
	isSubs˵����Ĭ��false
		false����ʹ��redis�ļ��ռ�֪ͨ���ܣ���subs��pull��subsClientGetOp�Ƚӿڲ�������
		true������redis�ļ��ռ�֪ͨ����
	*/
	bool connect(const std::string & strIp, int iPort, bool bNeedSubs = false);		//����redis���񲢶���redis���ռ�֪ͨ
	void disconnect();

	//redis��������
	/**
		get()    pop()
		����ֵ˵����
			-1������ʧ�ܣ�ʧ��ԭ��ͨ��lpStrRlt�鿴
			 0��lpStrKey��Ӧ��ֵΪ��
			>0�������ɹ���lpStrKey��ֵͨ��lpStrRlt�鿴������lpStrRlt�ĳ���

		set()    push()
		����ֵ˵����
			true�������ɹ������ͨ��lpStrRlt�鿴��һ��ΪOK
			false������ʧ�ܣ����ͨ��lpStrRlt�鿴
	  */ 
	int  get(const std::string & strInKey, std::string & strOutResult);
	bool set(const std::string & strInKey, const std::string & strInValue, std::string & strOutResult);
	bool push(const std::string & strInListName, const std::string & strInValue, std::string & strOutResult);
	int  pop(const std::string & strInListName, std::string & strOutResult);

	//redis���Ĺ���
	void subs(const std::string & strInKey, subsCallback cb);	//subscribe channel
	void unsubs(const std::string & strInKey);					//unsubscribe channel
	void pull(const std::string & strInKey, pullCallback cb);	//pull list
	void unpull(const std::string & strInKey);					//unpull list, like unsubs

	//ҵ����ģ��
	void subsClientGetOp(const std::string & strInKey, clientOpCallBack cb);
	void unsubClientGetOp(const std::string & strInKey);						//ע�������ͻ���get key����
	void stopSubClientGetOp();											//ȡ�������ͻ���ȫ��get����

private:
	void close();
	bool _connect(const std::string & strIp, int iPort);
	//ʵ��ҵ��ģ������ݸ���
	std::string genNewKey(const std::string & lpStrOldKey);					//��װ�û������key				
	std::string getOldKey(const std::string & lpStrNewKey);					//��ȡ�ͻ���ԭʼkey
	bool sendCmd(const std::string & strInCmd, std::string & strOutResult);		//����redis����
	bool replyCheck(redisReply *rRedisReply, std::string & strOutResult);		//����redisӦ����Ϣ

	static void* thAsyncSubsAll(void *arg);							//redis���ռ�֪ͨ

	static void connectCallback(const redisAsyncContext *c, int iStatus);	//redis�첽�ص�����
	static void disconnectCallback(const redisAsyncContext *c, int iStatus);
	static void subsAllCallback(redisAsyncContext *c, void *r, void *data);

	void callSubsCB(const std::string & strInKey, const std::string & strInOp);
	
	CRedis_Utils(const CRedis_Utils & c);

	redisContext *m_pRedisContext;			//redisͬ��������
	//std::shared_ptr<redisContext *> m_spRedisContext;
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
	mutex m_aeStopLock;

	std::thread thAsyncKeyNotify;

	std::string m_strIp;				//redis ip
	int m_iPort;						//redis �˿�

	CRedisRPC m_redisRPC;				//redis RPC����ʵ��

	bool m_bNeedSubs;					//�Ƿ���Ҫredis���Ĺ���
	bool m_bIsConnected;				//����redis�ɹ���־
	std::string m_strClientId;			//�ͻ��˱�־
};

