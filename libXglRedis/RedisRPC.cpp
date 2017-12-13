#include "RedisRPC.h"

//INITIALIZE_EASYLOGGINGPP
//bool CRedisRPC::m_bKeyProcessDone = false;
//std::mutex CRedisRPC::hbLock;
//std::thread CRedisRPC::thHeartBeat;

//CRedisRPC::CRedisRPC(std::string ip, int port)
//{
//	this->m_strIp = ip;
//	this->m_iPort = port;
//	if (this->m_strRequestID.length() <= 0)
//	{
//		this->m_strRequestID = generate_uuid();
//		//el::Configurations conf("easylog.conf");
//		//el::Loggers::reconfigureAllLoggers(conf);
//	}
//
//	//DEBUGLOG << "��ʼ����־��, redis ip = " << this->ip 
//	//	<< ", port = " << this->port;
//}


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
	m_redisContext = (redisContext *)redisConnectWithTimeout(
		ip, port, tv);
	DEBUGLOG("connect to redis server ip = " << this->m_strIp.c_str() << ", port = "
		<< this->m_iPort);

	if ((NULL == m_redisContext) || (m_redisContext->err))
	{
		if (m_redisContext)
			ERRORLOG("connect error:" << m_redisContext->errstr);
		else
			ERRORLOG("connect error: can't allocate redis context.");
		return false;
	}
	return true;
}

bool CRedisRPC::isServiceModelAvailable(const char *key)
{
	//����Ƿ��п��÷��� m_HBChnl
	DEBUGLOG("��ѯ�Ƿ��п��÷���ģ��...");
	bool bRlt = false;
	if (!connect(m_strIp.c_str(), m_iPort))
	{
		return false;
	}
	std::string heartBeat = key + std::string(HEARTSLOT);			//��������
	std::string getHeartBeatCmd = R_GET + std::string(" ") + heartBeat;
	redisReply *reply = (redisReply *)redisCommand(m_redisContext, getHeartBeatCmd.c_str());
	std::string heartbeat;
	if (reply->type == REDIS_REPLY_STRING)
	{
		heartbeat = reply->str;
		DEBUGLOG("heartbeat = " << heartbeat.c_str() << ", now = " << time(NULL));
		if (time(NULL) - atoi(heartbeat.c_str()) <= HEARTBEATTIMEOUT)
			bRlt = true;
	}
	else
		WARNLOG("��ȡ��������ʧ��...");
	freeReplyObject(reply);
	return bRlt;
}

bool CRedisRPC::isKeySubs(const char *key)
{
	//���ü��Ƿ���Ҫ���д��� m_getKeys
	bool bRlt = false;
	if (m_mapHBChnl.size() > 0)
	{
		mapHBchnl::iterator it = m_mapHBChnl.begin();
		for (; it != m_mapHBChnl.end(); ++it)
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

int CRedisRPC::processKey(const char *key)
{
	DEBUGLOG("��ʼ����Զ�̷�����... key = " << key);
	if (!connect(m_strIp.c_str(), m_iPort))
	{
		WARNLOG("����Զ�̷���ʧ��(redis)... ip = " << m_strIp.c_str() << ", port = " << m_iPort);
		if (m_redisContext)
		{
			redisFree(m_redisContext);
			m_redisContext = nullptr;
		}
		//processKeyLock.unlock();
		return REDIS_CONNFAIL;		//����redisʧ��
	}
	//Զ�̵��ô���keyֵ
	//����ֵ�����Ӧ���������
	//processKeyLock.lock();
	std::string reqChnl = key + std::string(REQSLOT);
	std::string requestURL = reqChnl;// + m_strRequestID;
									 //std::string pushCmd = std::string(R_PUSH) + std::string(" ") + requestURL + std::string(" ") + std::string(key);
	std::string processURL = reqChnl + std::string(REQPROCESSING);
	std::string setReqCmd = std::string(R_SET) + std::string(" ") + requestURL + std::string(" ") + std::string(key);
	std::string setProcessCMd = std::string(R_SET) + std::string(" ") + processURL + std::string(" ") + std::string("0");
	if (!redisCommand(m_redisContext, setReqCmd.c_str()))
	{
		WARNLOG("����Զ�̷���ʧ��... setReqCmd = " << setReqCmd.c_str());
		if (m_redisContext)
		{
			redisFree(m_redisContext);
			m_redisContext = nullptr;
		}
		//processKeyLock.unlock();
		return REDIS_SENDREQFAIL;			//��������ʧ��
	}

	int iRlt = REDIS_TIMEOUT;		//0��ʾ�ɹ�
	m_bKeyProcessDone = false;
	//�ȴ�����������ʱ����
	redisReply *reply;
	std::string subcmd = std::string(R_SUBS) + std::string(" ") + std::string(R_KEYSPACE) + std::string(key);
	std::string unsubcmd = std::string(R_UNSUBS) + std::string(" ") + std::string(R_KEYSPACE) + std::string(key);
	if (!(reply = (redisReply *)redisCommand(m_redisContext, subcmd.c_str())))
	{
		WARNLOG("�ȴ�Զ�̷�������ʧ��...");
		if (m_redisContext)
		{
			redisFree(m_redisContext);
			m_redisContext = nullptr;
		}
		//processKeyLock.unlock();
		return REDIS_SENDREQFAIL;
	}
	if (reply != nullptr)
		freeReplyObject(reply);

	//std::string timer_msg = m_strIp + "&_&" + int2str(m_iPort) + "&_&" + int2str(timeout) + "&_&" + key;
	m_strCurrentProcessKey = key;
	std::thread thTimer(thTimeout, this);

	DEBUGLOG("�ȴ�Զ�̷�������...");
	//processKeyLock.unlock();
	redisGetReply(m_redisContext, (void **)&reply);
	if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3 && strcmp(reply->element[2]->str, "set") == 0)
		iRlt = 0;

	if (reply != nullptr)
		freeReplyObject(reply);
	m_bKeyProcessDone = true;
	thTimer.join();
	m_strCurrentProcessKey = "";
	DEBUGLOG("Զ�̴������... iRlt = " << iRlt);
	//reply = (redisReply *)redisCommand(m_redisContext, unsubcmd.c_str());

	//if (reply != nullptr)
	//	freeReplyObject(reply);
	if (m_redisContext)
	{
		redisFree(m_redisContext);
		m_redisContext = nullptr;
	}
	return iRlt;
}

void CRedisRPC::subsClientGetOp(const char *keys, const char *reqChlName,
	const char *heartbeatChnName)
{
	DEBUGLOG("Զ�̷���ע��... keys = " << keys << ", reqChlName = " << reqChlName
		<< ", heartbeatChnName = " << heartbeatChnName);
	mapReqchnl::iterator it = m_mapHBChnl.find(std::string(keys));
	if (it == m_mapHBChnl.end())
	{

		//		m_getKeys.insert(std::pair<std::string, getCallback>(std::string(keys), cb));
		m_mapReqChnl.insert(std::pair<std::string, std::string>(std::string(keys), std::string(reqChlName)));
		m_mapHBChnl.insert(std::pair<std::string, std::string>(std::string(keys), std::string(heartbeatChnName)));
		DEBUGLOG("Զ�̷���ע��ɹ� key = " << keys
			<< "reqChnl = " << reqChlName
			<< "hbChnl, = " << heartbeatChnName);
		if (m_mapHBChnl.size() == 1)	//�����������������̣߳�m_mapReqChnlΪ��ʱ�߳�ֹͣ
		{
			DEBUGLOG("�����������������߳�...");
			//std::thread thHeartBeat(thSetHeartBeat, this);
			//thHeartBeat.detach();
			if (!thHeartBeat.joinable())
				thHeartBeat = std::thread(thSetHeartBeat, this);
		}
	}
	else
		DEBUGLOG("�ü��Ѿ����� key = " << keys);
}

std::string CRedisRPC::unsubClientGetOp(const char *keys)
{
	std::string chnlName = "";
	hbLock.lock();
	mapReqchnl::iterator it = m_mapReqChnl.find(std::string(keys));
	if (it != m_mapReqChnl.end())
	{
		chnlName = m_mapReqChnl[keys];
		m_mapHBChnl.erase(keys);
		m_mapReqChnl.erase(keys);
	}
	hbLock.unlock();
	DEBUGLOG("erase key = " << keys << ", size = " << m_mapHBChnl.size() << ", " << m_mapReqChnl.size());
	if (m_mapHBChnl.size() <= 0)
	{
		DEBUGLOG("wait for thHeartBeat quit... size = " << m_mapHBChnl.size());
		thHeartBeat.join();
		DEBUGLOG("thHeartBeat quit... ");
	}
	return chnlName;
}


bool CRedisRPC::sendReq(redisContext *context, std::string strReqCmd, std::string strProcesssCmd)
{
	if (!redisCommand(context, "MULTI") ||
		!redisCommand(context, strReqCmd.c_str()) ||
		!redisCommand(context, strProcesssCmd.c_str()) ||
		!redisCommand(context, "EXEC"))
		return false;
	return true;
}

void* CRedisRPC::thTimeout(void *arg)
{
	CRedisRPC *self = (CRedisRPC *)arg;
	//std::string args = *(std::string *)arg;
	//std::vector<std::string> sStr = split(args, "&_&");
	//std::string ip = sStr[0];
	//int port = atoi(sStr[1].c_str());
	//int timeout = atoi(sStr[2].c_str());
	//std::string key = sStr[3];
	std::string key = self->m_strCurrentProcessKey;

	int i = 0;
	DEBUGLOG("�ȴ�Զ�̷�����...");
	while (i < 50)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(GET_WAITTIMEOUT / 50));
		i++;
		if (self->m_bKeyProcessDone)
			return nullptr;
	}
	DEBUGLOG("Զ�̷�����ʱ...ֹͣ�ȴ�");
	timeval tv = { 3, 500 };
	redisContext *context = (redisContext *)redisConnectWithTimeout(
		self->m_strIp.c_str(), self->m_iPort, tv);
	if ((NULL == context) || (context->err))
	{
		if (context)
			WARNLOG("connect error:" << context->errstr);
		else
			WARNLOG("connect error: can't allocate redis context.");
		return nullptr;
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
	std::string setProcessingCmd = R_SET + std::string(" ") + key + std::string(REQSLOT) + std::string(REQPROCESSING)
		+ std::string(" ") + std::string("0");
	redisCommand(context, setProcessingCmd.c_str());
	if ((reply = (redisReply *)(redisCommand(context, existKeyCmd.c_str()))) && reply->integer)	//���Ҫget��key�Ƿ����
	{
		freeReplyObject(reply);
		while ((reply = (redisReply *)(redisCommand(context, existNewKeyCmd.c_str()))) && reply->integer)
		{
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

void* CRedisRPC::thSetHeartBeat(void *arg)
{
	CRedisRPC *self = (CRedisRPC *)arg;

	//��������
	while (1)
	{
		self->hbLock.lock();
		if (self->m_mapHBChnl.size() <= 0)
		{
			DEBUGLOG("�������������߳��˳�...");
			self->hbLock.unlock();
			return nullptr;
		}
		DEBUGLOG("m_mapHBChnl size = " << self->m_mapHBChnl.size());
		mapHBchnl::iterator it_hb = self->m_mapHBChnl.begin();
		for (; it_hb != self->m_mapHBChnl.end(); ++it_hb)
		{
			std::string setHbCmd = std::string(R_SET) + " " + it_hb->second + " " + int2str(time(NULL));
			if (!self->connect(self->m_strIp.c_str(), self->m_iPort))
			{
				WARNLOG("����Զ�̷���ʧ��(redis)... ip = " << self->m_strIp.c_str() << ", port = " << self->m_iPort);
				if (self->m_redisContext)
				{
					redisFree(self->m_redisContext);
					self->m_redisContext = nullptr;
				}
			}
			if (self->m_redisContext)
				redisCommand(self->m_redisContext, setHbCmd.c_str());
		}
		self->hbLock.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(GET_WAITTIMEOUT / 2));
	}

	return nullptr;
}

void CRedisRPC::clearChnl()
{
	if (m_mapReqChnl.size() > 0)
		m_mapReqChnl.clear();
	hbLock.lock();
	bool needWait = false;
	if (m_mapHBChnl.size() > 0)
	{
		needWait = true;
		m_mapHBChnl.clear();
	}
	hbLock.unlock();
	if (needWait)
	{
		DEBUGLOG("wait for thHeartBeat quit... size = " << m_mapHBChnl.size());
		thHeartBeat.join();
		DEBUGLOG("thHeartBeat quit... ");
	}
}

void CRedisRPC::setClientId(const std::string & strClientId)
{
	this->m_strClientId = strClientId;
}

void CRedisRPC::setRedisAddr(const std::string & strIp, const int iPort)
{
	this->m_strIp = strIp;
	this->m_iPort = iPort;
}
