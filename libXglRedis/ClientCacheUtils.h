#pragma once

#include <map>
#include <string>
#include <functional>


//��־����
#define LOG_INFO		0
#define LOG_TRACE		1
#define LOG_DEBUG		2
#define LOG_WARN		3
#define LOG_ERROR		4
#define	LOG_FATAL		5

//״̬��
#define REDIS_TIMEOUT			100		//ҵ��ģ�鴦��ʱ
#define REDIS_NOSERVICE			101		//��ҵ����ģ��
#define REDIS_SENDREQFAIL		102		//��������ʧ��
#define REDIS_CONNFAIL			103		//redis��������ʧ��
#define REDIS_CMD_ERR			105		//��������ʧ��
#define REDIS_KEY_NULL			104		//�����keyֵΪ��
#define REDIS_VALUE_NULL		105		//�����valueֵΪ��
#define REDIS_KEY_EXISTED		106		//key�ѱ�����
#define REDIS_SUBS_OFF			107		//δ����redis���ռ�֪ͨ����
#define REDIS_REQ_SYNC_FAIL		108		//����ͬ��ʧ��
#define	REDIS_KEY_NOT_EXIST		109		//get��pop��key������

/************************************************************************/
/*  ������صĻص�������
subs-->subsCallback
pull-->pullCallback
subsClientGetOp-->clientOpCallBack

����˵����
strKey�����ĵļ�
strValue������Ӧ��ֵ
*/
/************************************************************************/
typedef std::function<void(const std::string &, const std::string &)> subsCallback;
typedef std::function<void(const std::string &, const std::string &)> pullCallback;

typedef std::map<std::string, subsCallback> mapSubsCB;		//subkey-->subfunc
typedef std::map<std::string, pullCallback> mapPullCB;		//pullkey-->subfunc

class CClientCacheUtils
{
public:
	CClientCacheUtils();
	~CClientCacheUtils();

	//�ͻ��˻�����������
	/*
	isSubs˵����Ĭ��false
	false����ʹ��redis�ļ��ռ�֪ͨ���ܣ���subs��pull��subsClientGetOp�Ƚӿڲ������ã�ֻ�ܽ���redis��������
	true������redis�ļ��ռ�֪ͨ����
	*/
	bool connect(const std::string & strIp, int iPort, bool bNeedSubs = false);		//����redis���񲢶���redis���ռ�֪ͨ
	void disconnect();

	//redis��������
	/**
	����ֵ˵����
	0�������ɹ�
	>0������ʧ�ܣ�����״̬��
	�������ͨ��strOutResult��ȡ

	ʹ��˵�����˽ӿ������strClientId�ֶΣ������ǿ��������л�ģ����������ʹ�õ�ʱ��Ӧ���̵߳��ô˽ӿڡ�
		��������strClientId�ǿ��ַ�������ô���ֶλ���һ��Ĭ��ֵ��__default__
	*/
	int get(const std::string & strClientId, const std::string & strInKey, std::string & strOutResult);
	int set(const std::string & strClientId, const std::string & strInKey, const std::string & strInValue, std::string & strOutResult);
	int push(const std::string & strClientId, const std::string & strInListName, const std::string & strInValue, std::string & strOutResult);
	int pop(const std::string & strClientId, const std::string & strInListName, std::string & strOutResult);

	//redis���Ĺ���
	/*
	subs()	pull()
	����ֵ˵����
	0�������ɹ�
	>0������ʧ�ܣ�����״̬��
	
	subs��pull�Ƚϣ�
	1. subs���ĵ����ַ����ı仯��pull���ĵ���list�ı仯��
	2. ��set��������ʱ��subs�ص����յ���set��key��value��
	del��������ʱ��subs�ص����յ���ɾ����key����ʱvalueΪ���ַ�����
	3. ��push��������ʱ��pull�ص����յ���push��listName��value��pop��������ʱ��pull�ص������յ���Ϣ��
	del��������ʱ��pull�ص����յ���ɾ����listName����ʱvalueΪ���ַ�����

	unsubs()	unpull()
	����ֵ˵����
	true�������ɹ�
	false������ʧ��
	ʹ��˵�����˽ӿ������strClientId�ֶΣ������ǿ��������л�ģ����������ʹ�õ�ʱ��Ӧ���̵߳��ô˽ӿڡ�
		��������strClientId�ǿ��ַ�������ô���ֶλ���һ��Ĭ��ֵ��__default__
	*/
	int subs(const std::string & strClientId, const std::string & strInKey, subsCallback cb);	//subscribe channel
	bool unsubs(const std::string & strClientId, const std::string & strInKey);					//unsubscribe channel
	int pull(const std::string & strClientId, const std::string & strInKey, pullCallback cb);	//pull list
	bool unpull(const std::string & strClientId, const std::string & strInKey);					//unpull list, like unsubs

	static void log(const int iLogType, const std::string & strLog);
protected:
	void * m_redisUtil;
};

