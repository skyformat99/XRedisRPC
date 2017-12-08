#include "RedisRPC.h"

//INITIALIZE_EASYLOGGINGPP
bool CRedisRPC::m_bKeyProcessDone = false;

CRedisRPC::CRedisRPC(std::string ip, int port)
{
	this->m_lpStrIp = ip;
	this->m_iPort = port;
	if(this->m_lpStrRequestID.length() <= 0)
	{
		this->m_lpStrRequestID = generate_uuid();
		//el::Configurations conf("easylog.conf");
		//el::Loggers::reconfigureAllLoggers(conf);
	}

	//DEBUGLOG << "初始化日志库, redis ip = " << this->ip 
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
	//临时redis客户端，用于订阅结果数据
	timeval tv = { 3, 500 };
	m_redisContext = (redisContext *)redisConnectWithTimeout(
		ip, port, tv);
	DEBUGLOG << "connect to redis server ip = " << this->m_lpStrIp << ", port = "
		<< this->m_iPort << ", isSubs = ";

	if ((NULL == m_redisContext) || (m_redisContext->err))
	{
		if (m_redisContext)
			ERRORLOG << "connect error:" << m_redisContext->errstr;
		else
			ERRORLOG << "connect error: can't allocate redis context.";
		return false;
	}
	return true;
}

bool CRedisRPC::isServiceModelAvailable(const char *key)
{
	//检查是否有可用服务 m_HBChnl
	DEBUGLOG << "查询是否有可用服务模块...";
	bool bRlt = false;
	if (!connect(m_lpStrIp.c_str(), m_iPort))
	{
		return false;
	}
	std::string getHeartBeatCmd = R_GET + std::string(" ") + m_mapHBChnl[key];
	redisReply *reply = (redisReply *)redisCommand(m_redisContext, getHeartBeatCmd.c_str());
	std::string heartbeat;
	if (reply->type == REDIS_REPLY_STRING)
	{
		heartbeat = reply->str;
		DEBUGLOG << "heartbeat = " << heartbeat << ", now = " << time(NULL);
		if (time(NULL) - atoi(heartbeat.c_str()) <= HEARTBEATTIMEOUT)
			bRlt = true;
	}
	else
		WARNLOG << "获取心跳数据失败...";
	freeReplyObject(reply);
	return bRlt;
}

bool CRedisRPC::isKeySubs(const char *key)
{
	//检查该键是否需要进行处理 m_getKeys
	bool bRlt = false;
	if(m_mapHBChnl.size() > 0)
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

void CRedisRPC::processKey(const char *key, int timeout, mutex &processKeyLock)
{
	DEBUGLOG << "开始请求远程服务处理... key = " << key;
	if (!connect(m_lpStrIp.c_str(), m_iPort))
	{
		WARNLOG << "连接远程服务失败(redis)... ip = " << m_lpStrIp << ", port = " << m_iPort;
		if(m_redisContext)
		{
			redisFree(m_redisContext);
			m_redisContext = nullptr;
		}
		processKeyLock.unlock(); 
		return;
	}
	//远程调用处理key值
	//将键值塞入对应的请求队列
	//processKeyLock.lock();
	std::string requestURL = m_mapReqChnl[key] + m_lpStrRequestID;
	std::string pushCmd = std::string(R_PUSH) + std::string(" ") + requestURL + std::string(" ") + std::string(key);
	if (!redisCommand(m_redisContext, pushCmd.c_str()))
	{
		WARNLOG << "请求远程服务失败... pushCMD = " << pushCmd;
		if (m_redisContext)
		{
			redisFree(m_redisContext);
			m_redisContext = nullptr;
		}
		processKeyLock.unlock();
		return;
	}

	CRedisRPC::m_bKeyProcessDone = false;
	//等待处理结果，超时返回
	redisReply *reply;
	std::string subcmd = std::string(R_SUBS) + std::string(" ") + std::string(R_KEYSPACE) + std::string(key);
	std::string unsubcmd = std::string(R_UNSUBS) + std::string(" ") + std::string(R_KEYSPACE) + std::string(key);
	if (!(reply = (redisReply *)redisCommand(m_redisContext, subcmd.c_str())))
	{
		WARNLOG << "等待远程服务处理结果失败..." << endl;
		if (m_redisContext)
		{
			redisFree(m_redisContext);
			m_redisContext = nullptr;
		}
		processKeyLock.unlock();
		return;
	}
	if (reply != nullptr)
		freeReplyObject(reply);

	std::string timer_msg = m_lpStrIp + "&_&" + int2str(m_iPort) + "&_&" + int2str(timeout) + "&_&" + key;
	std::thread thTimer(thTimeout, &timer_msg);

	DEBUGLOG << "等待远程服务处理结果...";
	processKeyLock.unlock();
	redisGetReply(m_redisContext, (void **)&reply);
	if (reply != nullptr)
		freeReplyObject(reply);
	CRedisRPC::m_bKeyProcessDone = true;
	thTimer.join();
	DEBUGLOG << "远程处理完成...";
	//reply = (redisReply *)redisCommand(context, unsubcmd.c_str());

	//if (reply != nullptr)
	//	freeReplyObject(reply);
	if (m_redisContext)
	{
		redisFree(m_redisContext);
		m_redisContext = nullptr;
	}
}

void CRedisRPC::subsClientGetOp(const char *keys, const char *reqChlName,
	const char *heartbeatChnName)
{
	DEBUGLOG << "远程服务注册... keys = " << keys << ", reqChlName = " << reqChlName
		<< ", heartbeatChnName = " << heartbeatChnName;
	mapReqchnl::iterator it = m_mapHBChnl.find(std::string(keys));
	if (it == m_mapHBChnl.end())
	{

//		m_getKeys.insert(std::pair<std::string, getCallback>(std::string(keys), cb));
		m_mapReqChnl.insert(std::pair<std::string, std::string>(std::string(keys), std::string(reqChlName)));
		m_mapHBChnl.insert(std::pair<std::string, std::string>(std::string(keys), std::string(heartbeatChnName)));
		DEBUGLOG << "远程服务注册成功 key = " << keys
			<< "reqChnl = " << reqChlName
			<< "hbChnl, = " << heartbeatChnName << endl;
		if (m_mapHBChnl.size() == 1)	//启动设置心跳信令线程，m_mapReqChnl为空时线程停止
		{
			DEBUGLOG << "启动心跳信令设置线程...";
			std::thread thHeartBeat(thSetHeartBeat, this);
			thHeartBeat.detach();
		}
	}
	else
		DEBUGLOG << "该键已经订阅 key = " << keys << endl;
}

std::string CRedisRPC::unsubClientGetOp(const char *keys)
{
	mapReqchnl::iterator it = m_mapReqChnl.find(std::string(keys));
	std::string chnlName = "";
	if (it != m_mapReqChnl.end())
	{
		chnlName = m_mapReqChnl[keys];
		m_mapHBChnl.erase(keys);
		m_mapReqChnl.erase(keys);
	}
	DEBUGLOG << "erase key = " << keys << ", size = " << m_mapHBChnl.size() << ", " << m_mapReqChnl.size();
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
	DEBUGLOG << "等待远程服务处理...";
	while (i < 50)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(timeout / 50));
		i++;
		if (m_bKeyProcessDone)
			return nullptr;
	}
	DEBUGLOG << "远程服务处理超时...，停止等待";
	timeval tv = { 3, 500 };
	redisContext *context = (redisContext *)redisConnectWithTimeout(
		ip.c_str(), port, tv);
	if ((NULL == context) || (context->err))
	{
		if (context)
			cout << "connect error:" << context->errstr << endl;
		else
			cout << "connect error: can't allocate redis context." << endl;
		return nullptr;
	}
	char sRlt[256];
	memset(sRlt, 0, 256);
	redisReply *reply;

	//结束循环
	std::string new_key = generate_uuid();
	string existNewKeyCmd = std::string(R_EXISTS) + std::string(" ") + new_key;
	string existKeyCmd = std::string(R_EXISTS) + std::string(" ") + key;
	string renameCmd = std::string(R_RENAME) + std::string(" ") + key + " " + new_key;
	string renameBackCmd = std::string(R_RENAME) + std::string(" ") + new_key + " " + key;
	string setCmd = std::string(R_SET) + std::string(" ") + key + std::string(" ") + key;
	string delCmd = std::string(R_DEL) + std::string(" ") + key;
	if ((reply = (redisReply *)(redisCommand(context, existKeyCmd.c_str()))) && reply->integer)	//检查要get的key是否存在
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
		//存在则用rename通知，停止等待
		redisCommand(context, "MULTI");				//事务
		redisCommand(context, renameCmd.c_str());
		redisCommand(context, renameBackCmd.c_str());
		redisCommand(context, "EXEC");
	}
	else
	{
		//不存在则用del通知，停止等待
		redisCommand(context, "MULTI");				//事务
		redisCommand(context, setCmd.c_str());
		redisCommand(context, delCmd.c_str());
		redisCommand(context, "EXEC");
	}

	return nullptr;
}

void* CRedisRPC::thSetHeartBeat(void *arg)
{
	CRedisRPC *self = (CRedisRPC *)arg;

	//设置信令
	while(1)
	{
		if (self->m_mapHBChnl.size() <= 0)
		{
			DEBUGLOG << "心跳信令设置线程退出...";
			return nullptr;
		}

		mapHBchnl::iterator it_hb = self->m_mapHBChnl.begin();
		for (; it_hb != self->m_mapHBChnl.end(); ++it_hb)
		{
			std::string setHbCmd = std::string(R_SET) + " " + it_hb->second + " " + int2str(time(NULL));
			if (!self->connect(self->m_lpStrIp.c_str(), self->m_iPort))
			{
				WARNLOG << "连接远程服务失败(redis)... ip = " << self->m_lpStrIp << ", port = " << self->m_iPort;
				if (self->m_redisContext)
				{
					redisFree(self->m_redisContext);
					self->m_redisContext = nullptr;
				}
			}
			if(self->m_redisContext)
				redisCommand(self->m_redisContext, setHbCmd.c_str());
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(4000));
	}

	return nullptr;
}

void CRedisRPC::clearChnl()
{
	if (m_mapReqChnl.size() > 0)
		m_mapReqChnl.clear();
	if (m_mapHBChnl.size() > 0)
		m_mapHBChnl.clear();
}
