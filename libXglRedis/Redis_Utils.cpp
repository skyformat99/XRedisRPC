#include "Redis_Utils.h"

CRedis_Utils::CRedis_Utils(std::string clientID)
{
	this->m_bIsConnected = false;
	if (clientID.length() > 0)
		this->m_lpStrClientId = clientID;
	else
		clientID = "default";
}

CRedis_Utils::~CRedis_Utils()
{
	close();
}

bool CRedis_Utils::connect(const char* ip, int port, bool isSubs)
{
	if(m_bIsConnected) 
	{
		DEBUGLOG << "redis 服务已经连接成功，重复连接... ip = " << ip 
			<< ", port = " << port;
		return true;
	}
	this->m_lpStrIp = ip;
	this->m_iPort = port;
	this->m_bNeedSubs = isSubs;
	m_redisRPC = CRedisRPC(ip, port);
	timeval t = { 1, 500 };

	//连接redis服务
	DEBUGLOG << "connect to redis server ip = " << this->m_lpStrIp << ", port = " 
		<< this->m_iPort << ", isSubs = " << this->m_bNeedSubs;
	m_pRedisContext = (redisContext *)redisConnectWithTimeout(
		ip, port, t);
	if ((NULL == m_pRedisContext) || (m_pRedisContext->err))
	{
		if (m_pRedisContext)
			ERRORLOG << "connect error:" << m_pRedisContext->errstr;
		else
			ERRORLOG << "connect error: can't allocate redis context.";
		return false;
	}
	
	//订阅redis键空间通知
	if(m_bNeedSubs)
	{
		DEBUGLOG << "subscribe redis keyspace notification!!!";
		thread thAsyncSubALL(CRedis_Utils::thAsyncSubsAll, this);
		thAsyncSubALL.detach();
	}
	this->m_bIsConnected = true;		//连接成功
	return true;
}

void CRedis_Utils::disconnect()
{
	//if(this->needSubs && !pRedisAsyncContext)
	//	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	//close();
	//delete this;
	this->m_bIsConnected = false;
	if (this->m_bNeedSubs)
	{
		this->m_bNeedSubs = false;
		if (m_mapSubsKeys.size() > 0)
			m_mapSubsKeys.clear();
		if (m_mapPullKeys.size() > 0)
			m_mapPullKeys.clear();
		m_redisRPC.clearChnl();
	}
}

void* CRedis_Utils::thAsyncSubsAll(void *arg)
{
	CRedis_Utils *self = (CRedis_Utils *)arg;
#ifdef _WIN32
	/* For Win32_IOCP the event loop must be created before the async connect */
	self->m_loop = aeCreateEventLoop(1024 * 10);
#else
	signal(SIGPIPE, SIG_IGN);
	self->m_base = event_base_new();
#endif
	self->m_pRedisAsyncContext = redisAsyncConnect(self->m_lpStrIp.c_str(), self->m_iPort);
	if (self->m_pRedisAsyncContext->err) {
		printf("Error: %s\n", self->m_pRedisAsyncContext->errstr);
		return NULL;
	}
#ifdef _WIN32
	redisAeAttach(self->m_loop, self->m_pRedisAsyncContext);
#else
	redisLibeventAttach(self->m_pRedisAsyncContext, self->m_base);
#endif

	redisAsyncSetConnectCallback(self->m_pRedisAsyncContext, CRedis_Utils::connectCallback);
	redisAsyncSetDisconnectCallback(self->m_pRedisAsyncContext, CRedis_Utils::disconnectCallback);
	std::string cmd = std::string(R_PSUBS) + std::string(" ") + std::string(R_KEYSPACE) + std::string("*");
	redisAsyncCommand(self->m_pRedisAsyncContext, subsAllCallback, self, cmd.c_str()/*"PSUBSCRIBE __keyspace@0__:*"*/);
	DEBUGLOG << "subscribe redis keyspace notification!!! cmd = " << cmd;

#ifdef _WIN32
	aeMain(self->m_loop);
#else
	event_base_dispatch(self->m_base);
#endif
	return NULL;
}

int CRedis_Utils::get(const char* _key, char* sRlt)
{
	if (sRlt == nullptr)
	{
		ERRORLOG << "char* sRlt pointed to null";
		return -1;
	}

	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp 
			<< ", port = " << m_iPort;
		if(!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			std::string rlt = "redis is not connected...";
			memcpy(sRlt, rlt.c_str(), rlt.length());
			return -1;
		}
	}
	m_getLock.lock();
	bool bRlt = false;
	std::string new_key = genNewKey(_key);
	std::string cmd = std::string(R_GET) + std::string(" ") + std::string(new_key);
	DEBUGLOG << "get cmd = " << cmd;
	if(!m_redisRPC.isServiceModelAvailable(new_key.c_str()) /*|| !m_redisRPC.isKeySubs(new_key.c_str())*/)
	{
		//get失败(nil也表示失败) 该key值不需要处理或者没有可用服务
		//直接返回数据
		DEBUGLOG << "no available service-->" << new_key;
		bRlt = sendCmd(cmd.c_str(), sRlt);
		m_getLock.unlock();
		if (bRlt)
			return strlen(sRlt);
		else if (strlen(sRlt))
			return -1;
	} 
	if (strlen(sRlt) > 0)
		memset(sRlt, 0, strlen(sRlt));
	//get_lock.unlock();

	//远程调用处理数据
	DEBUGLOG << "process the key before return.   key-->" << new_key;
	m_redisRPC.processKey(new_key.c_str(), 5000, m_getLock);		
	DEBUGLOG << "process the key, job done.   key-->" << new_key;
	
	//返回数据
	m_getLock.lock();
	bRlt = sendCmd(cmd.c_str(), sRlt);
	m_getLock.unlock();
	if (bRlt)		
		return strlen(sRlt);
	else if (sizeof(sRlt))
		return -1;
}

bool CRedis_Utils::set(const char* _key, const char* _value, char *sRlt)
{
	if (sRlt == nullptr)
	{
		ERRORLOG << "char* sRlt pointed to null";
		return false;
	}
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			std::string rlt = "redis is not connected...";
			memcpy(sRlt, rlt.c_str(), rlt.length());
			return false;
		}
	}
	m_setLock.lock();
	bool bRlt = false;
	std::string new_key = genNewKey(_key);
	std::string cmd = std::string(R_SET) + std::string(" ") 
		+ new_key + std::string(" ") + _value;

	DEBUGLOG << "set cmd = " << cmd;
	bRlt = sendCmd(cmd.c_str(), sRlt);
	m_setLock.unlock();
	if (bRlt)
		return true;
	else
		return false;
}

bool CRedis_Utils::push(const char* list_name, const char* _value, char *sRlt)
{
	if (sRlt == nullptr)
	{
		ERRORLOG << "char* sRlt pointed to null";
		return false;
	}
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			std::string rlt = "redis is not connected...";
			memcpy(sRlt, rlt.c_str(), rlt.length());
			return false;
		}
	}
	m_pushLock.lock();
	std::string new_list_name = genNewKey(list_name);
	std::string cmd = std::string(R_PUSH) + std::string(" ")
		+ new_list_name + std::string(" ") + _value;
	DEBUGLOG << "push cmd = " << cmd;
	bool bRlt = sendCmd(cmd.c_str(), sRlt);
	m_pushLock.unlock();
	if (bRlt)
		return true;
	else
		return false;
}

int CRedis_Utils::pop(const char* list_name, char *sRlt)
{
	if (sRlt == nullptr)
	{
		ERRORLOG << "char* sRlt pointed to null";
		return -1;
	}
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			std::string rlt = "redis is not connected...";
			memcpy(sRlt, rlt.c_str(), rlt.length());
			return -1;
		}
	}
	m_popLock.lock();
	std::string new_list_name = genNewKey(list_name);
	std::string cmd = std::string(R_POP) + std::string(" ") + new_list_name;
	DEBUGLOG << "pop cmd = " << cmd; 
	bool bRlt = sendCmd(cmd.c_str(), sRlt);
	m_popLock.unlock();
	if (bRlt)
		return strlen(sRlt);
	else if (strlen(sRlt))
		return -1;
}

void CRedis_Utils::subs(const char *key, subsCallback cb)
{
	if (strlen(key) == 0 || key == nullptr)
	{
		WARNLOG << "key is null... subs failed";
		return;
	}
	if (cb == nullptr)
	{
		WARNLOG << "subsCallback is null... subs failed";
		return;
	}
	std::string new_key = genNewKey(key);
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			//std::string rlt = "redis is not connected...";
			//memcpy(sRlt, rlt.c_str(), rlt.length());
			return;
		}
	}
	if(m_bNeedSubs)
	{
		m_subsLock.lock();
		mapSubsCB::iterator it = m_mapSubsKeys.find(std::string(new_key));
		if (it == m_mapSubsKeys.end())
		{
			m_mapSubsKeys.insert(std::pair<std::string, subsCallback>(std::string(new_key), cb));
			
			DEBUGLOG << "CRedis_Utils::subs 订阅成功 key = " << new_key
				<< ", size = " << m_mapSubsKeys.size();
		}
		else
			DEBUGLOG << "CRedis_Utils::subs 该键已经订阅 key = " << new_key;
		m_subsLock.unlock();
	}
	else
		DEBUGLOG << "redis subs function is shutdown!!! needSubs = " << m_bNeedSubs;
}

void CRedis_Utils::unsubs(const char *key)
{
	if (strlen(key) == 0 || key == nullptr)
	{
		WARNLOG << "key is null... unsubs failed";
		return;
	}
	std::string new_key = genNewKey(key);
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			//std::string rlt = "redis is not connected...";
			//memcpy(sRlt, rlt.c_str(), rlt.length());
			return;
		}
	}
	m_subsLock.lock();
	mapSubsCB::iterator it = m_mapSubsKeys.find(std::string(new_key));
	if (it != m_mapSubsKeys.end())
	{
		m_mapSubsKeys.erase(new_key);
		DEBUGLOG << "CRedis_Utils::unsubs 取消订阅成功 key = " << new_key 
			<< ", size = " << m_mapSubsKeys.size();
	}
	else
		DEBUGLOG << "CRedis_Utils::unsubs 该键尚未被订阅 key = " << new_key;
	m_subsLock.unlock();
}

void CRedis_Utils::pull(const char *key, pullCallback cb)
{
	if (strlen(key) == 0 || key == nullptr)
	{
		WARNLOG << "key is null... pull failed";
		return;
	}
	if (cb == nullptr)
	{
		WARNLOG << "subsCallback is null... subs failed";
		return;
	}

	std::string new_key = genNewKey(key);
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			//std::string rlt = "redis is not connected...";
			//memcpy(sRlt, rlt.c_str(), rlt.length());
			return;
		}
	}
	if(m_bNeedSubs)
	{
		m_pullLock.lock();
		mapPullCB::iterator it = m_mapPullKeys.find(std::string(new_key));
		if (it == m_mapPullKeys.end())
		{
			m_mapPullKeys.insert(std::pair<std::string, pullCallback>(std::string(new_key), cb));
			DEBUGLOG << "CRedis_Utils::pull 订阅成功 key = " << new_key
				<< ", size = " << m_mapPullKeys.size();
		}
		else
			DEBUGLOG << "CRedis_Utils::pull 该键已经订阅 key = " << new_key;
		m_pullLock.unlock();
	}
	else
		DEBUGLOG << "redis subs function is shutdown!!! needSubs = " << m_bNeedSubs;
}

void CRedis_Utils::unpull(const char *key)
{
	if (strlen(key) == 0 || key == nullptr)
	{
		WARNLOG << "key is null... unpull failed";
		return;
	}
	std::string new_key = genNewKey(key);
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			//std::string rlt = "redis is not connected...";
			//memcpy(sRlt, rlt.c_str(), rlt.length());
			return;
		}
	}
	m_pullLock.lock();
	mapPullCB::iterator it = m_mapPullKeys.find(std::string(new_key));
	if (it != m_mapPullKeys.end())
	{
		m_mapPullKeys.erase(new_key);
		DEBUGLOG << "CRedis_Utils::pull 取消订阅成功 key = " << new_key 
			<< ", size = " << m_mapSubsKeys.size();
	}
	else
		DEBUGLOG << "CRedis_Utils::pull 该键已经订阅 key = " << new_key;
	m_pullLock.unlock();
}

void CRedis_Utils::subsClientGetOp(const char *lpStrKey, clientOpCallBack cb)
{
	if (strlen(lpStrKey) == 0 || lpStrKey == nullptr)
	{
		WARNLOG << "key is null... subs failed!!!";
		return;
	}
	if (cb == nullptr)
	{
		WARNLOG << "subsCallback is null... subs failed";
		return;
	}
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			return;
		}
	}
	if (m_bNeedSubs)
	{
		std::string new_key = genNewKey(lpStrKey);				//键
		std::string heartBeat = new_key + HEARTLIST;			//心跳信令
		std::string reqChnl = new_key + REQLIST;				//请求队列
		std::string subsReqChnl = reqChnl + std::string("*");	//每个客户端对应一个请求队列

		mapReqCB::iterator it = m_mapReqChnl.find(std::string(subsReqChnl));
		m_reqLock.lock();
		if (it == m_mapReqChnl.end())
		{
			m_mapReqChnl.insert(std::pair<std::string, pullCallback>(std::string(subsReqChnl), cb));
			DEBUGLOG << "subsClientGetOp 订阅成功 key = " << subsReqChnl
				<< ", size = " << m_mapReqChnl.size();
		}
		else
			DEBUGLOG << "subsClientGetOp 该键已经订阅 key = " << subsReqChnl;
		m_reqLock.unlock();
		m_redisRPC.subsClientGetOp(new_key.c_str(), reqChnl.c_str(), heartBeat.c_str());
	}
}


void CRedis_Utils::unsubClientGetOp(const char *keys)
{
	if (strlen(keys) == 0 || keys == nullptr)
	{
		WARNLOG << "key is null... unsubClientGetOp failed";
		return;
	}
	if (!m_bIsConnected)
	{
		DEBUGLOG << "redis 服务尚未连接...尝试重新连接... ip = " << m_lpStrIp
			<< ", port = " << m_iPort;
		if (!connect(m_lpStrIp.c_str(), m_iPort, m_bNeedSubs))
		{
			WARNLOG << "redis服务重新连接失败... ";
			//std::string rlt = "redis is not connected...";
			//memcpy(sRlt, rlt.c_str(), rlt.length());
			return;
		}
	}
	if (m_mapReqChnl.size() > 0)
	{
		std::string new_key = genNewKey(keys);
		std::string reqChnl = m_redisRPC.unsubClientGetOp(new_key.c_str()) + "*";
		m_reqLock.lock();
		if (((reqChnl.length() - 1) > 0) && (m_mapReqChnl.find(reqChnl) != m_mapReqChnl.end()))
		{
			//DEBUGLOG << "before unsubGetOp chnlName = " << reqChnl << ", size = " << m_reqChnl.size();
			m_mapReqChnl.erase(reqChnl);
			DEBUGLOG << "unsubGetOp chnlName = " << reqChnl << ", size = " << m_mapReqChnl.size();
		}
		m_reqLock.unlock();
	}
	
}

void CRedis_Utils::stopSubClientGetOp() 
{
	m_redisRPC.clearChnl();
	m_reqLock.lock();
	if (m_mapReqChnl.size() > 0)
		m_mapReqChnl.clear();
	m_reqLock.unlock();
}

void CRedis_Utils::close()
{
	DEBUGLOG << "close redis connection ip = " << this->m_lpStrIp << ", port = "
		<< this->m_iPort;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));	//防止程序退出过快，导致异常。异步回调函数仍在运行...
	if (m_pRedisContext != nullptr)
	{
		redisFree(m_pRedisContext);
		m_pRedisContext = nullptr;
	}
	if (m_bNeedSubs)
	{
		stopSubClientGetOp();
		if (m_pRedisAsyncContext) {
			redisAsyncFree(m_pRedisAsyncContext);
			m_pRedisAsyncContext = nullptr;
			//redisAsyncDisconnect(pRedisAsyncContext);

#ifdef _WIN32
			aeStop(m_loop);
			m_loop = nullptr;
#else
			//event_base_loopbreak(base);
			event_base_free(m_base);
			m_base = nullptr;
#endif
		}
	}
}


std::string CRedis_Utils::genNewKey(std::string old_key)
{
	return m_lpStrClientId + old_key;
}

std::string CRedis_Utils::getOldKey(std::string new_key)
{
	if (new_key.length() > m_lpStrClientId.length())
		return new_key.substr(m_lpStrClientId.length());
	else
		return new_key;
}

bool CRedis_Utils::sendCmd(const char *cmd, char *sRlt)
{
	bool bRlt = false;
	redisReply *pRedisReply = (redisReply*)redisCommand(m_pRedisContext, cmd);  //执行INFO命令
	
	//错误处理!!!
	if (!pRedisReply)
	{
		WARNLOG << "redis cmd = " << cmd << ", err = " << m_pRedisContext->errstr
			<< ", try to reconnect...";
		m_bIsConnected = false;
		redisFree(m_pRedisContext);
		connect(m_lpStrIp.c_str(), m_iPort);
		pRedisReply = (redisReply*)redisCommand(m_pRedisContext, cmd);
		if (pRedisReply)
		{
			if (replyCheck(pRedisReply, sRlt))
				bRlt = true;
			freeReplyObject(pRedisReply);
			return bRlt;
		}
		m_bIsConnected = false;
		ERRORLOG << "Error send cmd[" << m_pRedisContext->err << ":" << m_pRedisContext->errstr << "]";
		memcpy(sRlt, m_pRedisContext->errstr, strlen(m_pRedisContext->errstr));
		return false;
	}
	if (replyCheck(pRedisReply, sRlt))
		bRlt = true;
	freeReplyObject(pRedisReply);
//	DEBUGLOG << "redis cmd = " << cmd << ", rlt = " << bRlt;
	return bRlt;
}

bool CRedis_Utils::replyCheck(redisReply *pRedisReply, char *sReply)
{
	bool bRlt = true;

	switch (pRedisReply->type) {
	case REDIS_REPLY_STATUS:		//表示状态，内容通过str字段查看，字符串长度是len字段
		DEBUGLOG << "type:REDIS_REPLY_STATUS, reply->len:" 
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str;
		memcpy(sReply, pRedisReply->str, pRedisReply->len);
		break;
	case REDIS_REPLY_ERROR:			//表示出错，查看出错信息，如上的str,len字段
		bRlt = false;
		WARNLOG << "type:REDIS_REPLY_ERROR, reply->len:"
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str;
		memcpy(sReply, pRedisReply->str, pRedisReply->len);
		break;
	case REDIS_REPLY_INTEGER:		//返回整数，从integer字段获取值
		DEBUGLOG << "type:REDIS_REPLY_INTEGER, reply->integer:" << pRedisReply->integer;
		memcpy(sReply, int2str(pRedisReply->integer).c_str(), int2str(pRedisReply->integer).length());
		break;
	case REDIS_REPLY_NIL:			//没有数据返回
		//bRlt = false;
		DEBUGLOG << "type:REDIS_REPLY_NIL, no data";
		break;
	case REDIS_REPLY_STRING:		//返回字符串，查看str,len字段
		DEBUGLOG << "type:REDIS_REPLY_STRING, reply->len:"
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str; 
		memcpy(sReply, pRedisReply->str, pRedisReply->len);
		break;
	case REDIS_REPLY_ARRAY:			//返回一个数组，查看elements的值（数组个数），通过element[index]的方式访问数组元素，每个数组元素是一个redisReply对象的指针
		DEBUGLOG << "------------------------------------------------------------" << endl;
		DEBUGLOG << "type:REDIS_REPLY_ARRAY, reply->elements:" << pRedisReply->elements << endl;
		for (int i = 0; i < pRedisReply->elements; i++) {
			//printf("%d: %s\n", i, pRedisReply->element[i]->str);
			char sRlt[256];
			memset(sRlt, 0, 256);
			replyCheck(pRedisReply->element[i], sRlt);
			memcpy(sReply + strlen(sReply), sRlt, strlen(sRlt));
			memcpy(sReply + strlen(sReply), "\n", 1);
		}
		DEBUGLOG << "------------------------------------------------------------" << endl;
		break;
	default:
		WARNLOG << "unkonwn type : " << pRedisReply->type;

		bRlt = false;
		break;
	}
	
	return bRlt;
}


void CRedis_Utils::connectCallback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK) {
		ERRORLOG << "Error: " << c->errstr;
		return;
	}
	DEBUGLOG << "Async Connected...";
}

void CRedis_Utils::disconnectCallback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK) {
		ERRORLOG << "Error: " << c->errstr;
		return;
	}
	DEBUGLOG << "Async DisConnected...";
}

void CRedis_Utils::subsAllCallback(redisAsyncContext *c, void *r, void *data)
{
	CRedis_Utils *self = (CRedis_Utils *)data;
	redisReply *reply = (redisReply *)r;
	if (reply == NULL) return;
	if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
		DEBUGLOG << "Received[%s " << self->m_lpStrIp.c_str() << "] channel" 
			<< reply->element[1]->str << ": " << reply->element[2]->integer;
	}
	else if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 4) {
		std::string key = split(reply->element[2]->str, "__:")[1];
		DEBUGLOG << "Received[ " << self->m_lpStrIp.c_str() << "] channel"
			<< reply->element[1]->str << " -- " << key
			<< " : " << reply->element[3]->str;
		self->callSubsCB(key.c_str(), reply->element[3]->str);
	}
}

void CRedis_Utils::callSubsCB(const char *key, const char *key_op)
{
	std::string getCmd = R_GET + std::string(" ") + key;
	std::string popCmd = R_POP + std::string(" ") + key;
	char *value = (char *)malloc(REDIS_BUF_SIZE);
	memset(value, 0, REDIS_BUF_SIZE);
	std::string old_key = getOldKey(key);

	//如果能匹配多个订阅的键，会执行多次
	m_subsLock.lock(); 
	if (m_mapSubsKeys.size() > 0)			//订阅
	{
		mapSubsCB::iterator it_subs = m_mapSubsKeys.begin();
		for(; it_subs != m_mapSubsKeys.end(); ++it_subs)
		{
			if (keyMatch(std::string(key), it_subs->first))
			{
				//将键和值都返回给上层，value长度为0表示键被del或者get rpop失败
				if (strcmp(key_op, "set") == 0)
				{
					if (!sendCmd(getCmd.c_str(), value))
						memset(value, 0, REDIS_BUF_SIZE);
					it_subs->second(old_key.c_str(), value);
				}
				else if (strcmp(key_op, "del") == 0)
					it_subs->second(old_key.c_str(), "");
			}
		}
	}
	m_subsLock.unlock();
	m_pullLock.lock();
	if (m_mapPullKeys.size() > 0)		//拉取
	{
		mapPullCB::iterator it_pull = m_mapPullKeys.begin();
		for (; it_pull != m_mapPullKeys.end(); ++it_pull)
		{
			//将键和值都返回给上层，value长度为0表示键被del或者get rpop失败
			if (keyMatch(std::string(key), it_pull->first))
			{
				if (strcmp(key_op, "lpush") == 0)
				{
					if (!sendCmd(popCmd.c_str(), value))
						memset(value, 0, REDIS_BUF_SIZE);
					it_pull->second(old_key.c_str(), value);
				}
			}
			else if (strcmp(key_op, "del") == 0)
				it_pull->second(old_key.c_str(), "");
		}
	}
	m_pullLock.unlock();
	m_reqLock.lock();
	if (this->m_mapReqChnl.size() > 0)	//请求队列
	{ 
		size_t s = m_mapReqChnl.size(); 
		mapReqCB::iterator it_req = m_mapReqChnl.begin();
		for (; it_req != m_mapReqChnl.end(); ++it_req)
		{
			if (keyMatch(std::string(key), it_req->first))
			{
				if (strcmp(key_op, "lpush") == 0)
				{
					std::string get_key = "";
					std::string get_value = "";
					if (sendCmd(popCmd.c_str(), value))		//获取请求队列中的值
					{
						get_key = value;
						memset(value, 0, REDIS_BUF_SIZE);
						getCmd.clear();
						getCmd = R_GET + std::string(" ") + get_key;
						if (sendCmd(getCmd.c_str(), value))		//获取对应的值
							get_value = value;
						else
							WARNLOG << "get value failed... getCmd = " << getCmd
								<< ", errstr = " << value;
					}
					else
						WARNLOG << "获取请求队列信息失败... popCmd = " << getCmd
							<< ", errstr = " << value;
					it_req->second(getOldKey(get_key).c_str(), get_value.c_str());
				}
			}
		}
	}
	m_reqLock.unlock();
	free(value);
}

