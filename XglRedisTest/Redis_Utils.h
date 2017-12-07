#pragma once

#include "RedisRPC.h"

//#ifdef _WIN32
//#include <WinSock2.h>
//#else
//#include <sys/time.h>
//#endif // _WIN32


typedef void(*subsCallback)(const char *key, const char *value);
typedef void(*pullCallback)(const char *key, const char *value);
typedef void(*clientOpCallBack)(const char *key, const char *value);

typedef std::map<std::string, subsCallback> mapSubsCB;		//subkey-->subfunc
typedef std::map<std::string, pullCallback> mapPullCB;		//pullkey-->subfunc
typedef std::map<std::string, clientOpCallBack> mapReqCB;	//getkey-->getfunc

class CRedis_Utils
{
public:
	CRedis_Utils(std::string clientID);
	~CRedis_Utils();

	//�ͻ��˻�����������
	/*
	isSubs˵����Ĭ��false
		false����ʹ��redis�ļ��ռ�֪ͨ���ܣ���subs��pull��subsClientGetOp�Ƚӿڲ�������
		true������redis�ļ��ռ�֪ͨ����
	*/
	bool connect(const char* ip, int port, bool isSubs = false);		//����redis���񲢶���redis���ռ�֪ͨ
	void disconnect();

	//redis��������
	int  get(const char* _key, char *sRlt);		//-1ʧ��  0 key�޶�Ӧ��ֵ
	bool set(const char* _key, const char* _value, char *sRlt);
	bool push(const char* list_name, const char* _value, char *sRlt);
	int  pop(const char* list_name, char *sRlt);

	//redis���Ĺ���
	void subs(const char *key, subsCallback cb);	//subscribe channel
	void unsubs(const char *key);					//unsubscribe channel
	void pull(const char *key, pullCallback cb);	//pull list
	void unpull(const char *key);					//unpull list, like unsubs

	//ҵ����ģ��
	void subsClientGetOp(const char *keys, const char *reqChlName,
		const char *heartbeatChnName, clientOpCallBack cb);			//ע������ͻ���get key����
	void unsubClientGetOp(const char *keys);						//ע�������ͻ���get key����
	void stopSubClientGetOp();										//ȡ�������ͻ���ȫ��get����

private:
	void close();
	//ʵ��ҵ��ģ������ݸ���
	std::string genNewKey(std::string old_key);					//��װ�û������key				
	std::string getOldKey(std::string new_key);					//��ȡ�ͻ���ԭʼkey
	bool sendCmd(const char *cmd, char *sRlt);					//����redis����
	bool replyCheck(redisReply *pRedisReply, char *sReply);		//����redisӦ����Ϣ

	static void* thAsyncSubsAll(void *arg);						//redis���ռ�֪ͨ

	static void connectCallback(const redisAsyncContext *c, int status);	//redis�첽�ص�����
	static void disconnectCallback(const redisAsyncContext *c, int status);
	static void subsAllCallback(redisAsyncContext *c, void *r, void *data);

	void callSubsCB(const char *key, const char *key_op);
	
	redisContext *pRedisContext;			//redisͬ��������
	redisAsyncContext *pRedisAsyncContext;	//redis�첽������
	
#ifdef _WIN32				//redis�첽�¼���
	aeEventLoop *loop;
#else
	struct event_base *base;
#endif

	mapSubsCB m_subsKeys;		//������Ϣ  ��-->�ص�����
	mapPullCB m_pullKeys;		//pull��Ϣ  ��-->�ص�����
	mapReqCB  m_reqChnl;		//�������	������-->�ص�����

	mutex get_lock;				//���߳�ͬ����
	mutex set_lock;
	mutex push_lock;
	mutex pop_lock;
	mutex subs_lock;
	mutex pull_lock;
	mutex req_lock;

	std::string ip;				//redis ip
	int port;					//redis �˿�

	CRedisRPC redisRPC;			//redis RPC����ʵ��

	bool needSubs;				//�Ƿ���Ҫredis���Ĺ���
	bool is_connected;			//����redis�ɹ���־
	std::string client_id;		//�ͻ��˱�־
};

