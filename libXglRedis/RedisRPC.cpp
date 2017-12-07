#include "RedisRPC.h"

//INITIALIZE_EASYLOGGINGPP
bool CRedisRPC::keyProcessDone = false;

CRedisRPC::CRedisRPC(std::string ip, int port)
{
	this->ip = ip;
	this->port = port;
	if(this->requestID.length() <= 0)
	{
		this->requestID = generate_uuid();
		//el::Configurations conf("easylog.conf");
		//el::Loggers::reconfigureAllLoggers(conf);
	}

	//DEBUGLOG << "��ʼ����־��, redis ip = " << this->ip 
	//	<< ", port = " << this->port;
}


CRedisRPC::CRedisRPC()
{

}

CRedisRPC::~CRedisRPC()
{
}


bool CRedisRPC::connect(const char* ip, int port)
{
	//��ʱredis�ͻ��ˣ����ڶ��Ľ������
	timeval tv = { 3, 500 };
	context = (redisContext *)redisConnectWithTimeout(
		ip, port, tv);
	DEBUGLOG << "connect to redis server ip = " << this->ip << ", port = "
		<< this->port << ", isSubs = ";

	if ((NULL == context) || (context->err))
	{
		if (context)
			ERRORLOG << "connect error:" << context->errstr;
		else
			ERRORLOG << "connect error: can't allocate redis context.";
		return false;
	}
	return true;
}

bool CRedisRPC::isServiceModelAvailable(const char *key)
{
	//����Ƿ��п��÷��� m_HBChnl
	DEBUGLOG << "��ѯ�Ƿ��п��÷���ģ��...";
	bool bRlt = false;
	if (!connect(ip.c_str(), port))
	{
		return false;
	}
	std::string getHeartBeatCmd = R_GET + std::string(" ") + m_HBChnl[key];
	redisReply *reply = (redisReply *)redisCommand(context, getHeartBeatCmd.c_str());
	std::string heartbeat;
	if (reply->type == REDIS_REPLY_STRING)
	{
		heartbeat = reply->str;
		DEBUGLOG << "heartbeat = " << heartbeat << ", now = " << time(NULL);
		if (time(NULL) - atoi(heartbeat.c_str()) <= HEARTBEATTIMEOUT)
			bRlt = true;
	}
	else
		WARNLOG << "��ȡ��������ʧ��...";
	freeReplyObject(reply);
	return bRlt;
}

bool CRedisRPC::isKeySubs(const char *key)
{
	//���ü��Ƿ���Ҫ���д��� m_getKeys
	bool bRlt = false;
	if(m_HBChnl.size() > 0)
	{
		mapHBchnl::iterator it = m_HBChnl.begin();
		for (; it != m_HBChnl.end(); ++it)
		{
			if (keyMatch(std::string(key), it->first))
			{
				bRlt = true;
				break;
			}
		}
	}
	return bRlt;
}

void CRedisRPC::processKey(const char *key, int timeout)
{
	DEBUGLOG << "��ʼ����Զ�̷�����... key = " << key;
	if (!connect(ip.c_str(), port))
	{
		WARNLOG << "����Զ�̷���ʧ��(redis)... ip = " << ip << ", port = " << port;
		if(context)
		{
			redisFree(context);
			context = nullptr;
		}
		return;
	}
	//Զ�̵��ô���keyֵ
	//����ֵ�����Ӧ���������
	std::string requestURL = m_reqChnl[key] + requestID;
	std::string pushCmd = std::string(R_PUSH) + std::string(" ") + requestURL + std::string(" ") + std::string(key);
	if (!redisCommand(context, pushCmd.c_str()))
	{
		WARNLOG << "����Զ�̷���ʧ��... pushCMD = " << pushCmd;
		if (context)
		{
			redisFree(context);
			context = nullptr;
		}
		return;
	}
	CRedisRPC::keyProcessDone = false;
	//�ȴ�����������ʱ����
	redisReply *reply;
	std::string subcmd = std::string(R_SUBS) + std::string(" ") + std::string(R_KEYSPACE) + std::string(key);
	std::string unsubcmd = std::string(R_UNSUBS) + std::string(" ") + std::string(R_KEYSPACE) + std::string(key);
	if (!(reply = (redisReply *)redisCommand(context, subcmd.c_str())))
	{
		WARNLOG << "�ȴ�Զ�̷�������ʧ��..." << endl;
		if (context)
		{
			redisFree(context);
			context = nullptr;
		}
		return;
	}
	if (reply != nullptr)
		freeReplyObject(reply);

	std::string timer_msg = ip + "&_&" + int2str(port) + "&_&" + int2str(timeout) + "&_&" + key;
	std::thread thTimer(thTimeout, &timer_msg);

	DEBUGLOG << "�ȴ�Զ�̷�������...";
	redisGetReply(context, (void **)&reply);
	if (reply != nullptr)
		freeReplyObject(reply);
	CRedisRPC::keyProcessDone = true;
	thTimer.join();
	DEBUGLOG << "Զ�̴������...";
	reply = (redisReply *)redisCommand(context, unsubcmd.c_str());

	if (reply != nullptr)
		freeReplyObject(reply);
	if (context)
	{
		redisFree(context);
		context = nullptr;
	}
}

void CRedisRPC::subsClientGetOp(const char *keys, const char *reqChlName,
	const char *heartbeatChnName)
{
	DEBUGLOG << "Զ�̷���ע��... keys = " << keys << ", reqChlName = " << reqChlName
		<< ", heartbeatChnName = " << heartbeatChnName;
	mapReqchnl::iterator it = m_HBChnl.find(std::string(keys));
	if (it == m_HBChnl.end())
	{
//		m_getKeys.insert(std::pair<std::string, getCallback>(std::string(keys), cb));
		m_reqChnl.insert(std::pair<std::string, std::string>(std::string(keys), std::string(reqChlName)));
		m_HBChnl.insert(std::pair<std::string, std::string>(std::string(keys), std::string(heartbeatChnName)));
		DEBUGLOG << "Զ�̷���ע��ɹ� key = " << keys
			<< "reqChnl = " << reqChlName
			<< "hbChnl, = " << heartbeatChnName << endl;
	}
	else
		DEBUGLOG << "�ü��Ѿ����� key = " << keys << endl;
}

std::string CRedisRPC::unsubClientGetOp(const char *keys)
{
	mapReqchnl::iterator it = m_reqChnl.find(std::string(keys));
	std::string chnlName = "";

	if (it != m_reqChnl.end())
	{
		chnlName = m_reqChnl[keys];
		m_HBChnl.erase(keys);
		m_reqChnl.erase(keys);
	}
	DEBUGLOG << "erase key = " << keys << ", size = " << m_HBChnl.size() << ", " << m_reqChnl.size();
	return chnlName;
}

void* CRedisRPC::thTimeout(void *arg)
{
	std::string args = *(std::string *)arg;
	std::vector<std::string> sStr = split(args, "&_&");
	std::string ip = sStr[0];
	int port = atoi(sStr[1].c_str());
	int timeout = atoi(sStr[2].c_str());
	std::string key = sStr[3];

	int i = 0;
	DEBUGLOG << "�ȴ�Զ�̷�����...";
	while (i < 50)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(timeout / 50));
		i++;
		if (keyProcessDone)
			return nullptr;
	}
	DEBUGLOG << "Զ�̷�����ʱ...��ֹͣ�ȴ�";
	timeval tv = { 3, 500 };
	redisContext *context = (redisContext *)redisConnectWithTimeout(
		ip.c_str(), port, tv);
	if ((NULL == context) || (context->err))
	{
		if (context)
			cout << "connect error:" << context->errstr << endl;
		else
			cout << "connect error: can't allocate redis context." << endl;
		return false;
	}
	char sRlt[256];
	memset(sRlt, 0, 256);
	redisReply *reply;

	//����ѭ��
	std::string new_key = generate_uuid();
	string existNewKeyCmd = std::string(R_EXISTS) + std::string(" ") + new_key;
	string existKeyCmd = std::string(R_EXISTS) + std::string(" ") + key;
	string renameCmd = std::string(R_RENAME) + std::string(" ") + key + " " + new_key;
	string renameBackCmd = std::string(R_RENAME) + std::string(" ") + new_key + " " + key;
	string setCmd = std::string(R_SET) + std::string(" ") + key + std::string(" ") + key;
	string delCmd = std::string(R_DEL) + std::string(" ") + key;
	if ((reply = (redisReply *)(redisCommand(context, existKeyCmd.c_str()))) && reply->integer)	//���Ҫget��key�Ƿ����
	{
		freeReplyObject(reply);
		while ((reply = (redisReply *)(redisCommand(context, existNewKeyCmd.c_str()))) && reply->integer)
		{
			cout << "key existed... cmd = " << existNewKeyCmd << endl;
			new_key = generate_uuid();
			existNewKeyCmd = std::string(R_EXISTS) + std::string(" ") + new_key;
			freeReplyObject(reply);
		}
		//replyCheck(reply);
		//freeReplyObject(reply);
		//��������rename֪ͨ��ֹͣ�ȴ�
		redisCommand(context, "MULTI");				//����
		redisCommand(context, renameCmd.c_str());
		redisCommand(context, renameBackCmd.c_str());
		redisCommand(context, "EXEC");
	}
	else
	{
		//����������del֪ͨ��ֹͣ�ȴ�
		redisCommand(context, "MULTI");				//����
		redisCommand(context, setCmd.c_str());
		redisCommand(context, delCmd.c_str());
		redisCommand(context, "EXEC");
	}

	return nullptr;
}

void CRedisRPC::clearChnl()
{
	if (m_reqChnl.size() > 0)
		m_reqChnl.clear();
	if (m_HBChnl.size() > 0)
		m_HBChnl.clear();
}
