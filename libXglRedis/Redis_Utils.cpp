#include "Redis_Utils.h"

CRedis_Utils::CRedis_Utils(std::string clientID)
{
	this->is_connected = false;
	this->client_id = clientID;
}

CRedis_Utils::~CRedis_Utils()
{
	close();
}

bool CRedis_Utils::connect(const char* ip, int port, bool isSubs)
{
	if(is_connected) 
	{
		//DEBUGLOG << "redis 服务已经连接成功，重复连接... ip = " << ip 
		//	<< ", port = " << port;
		return true;
	}
	this->ip = ip;
	this->port = port;
	this->needSubs = isSubs;
	redisRPC = CRedisRPC(ip, port);
	timeval t = { 1, 500 };

	//连接redis服务
	DEBUGLOG << "connect to redis server ip = " << this->ip << ", port = " 
		<< this->port << ", isSubs = " << this->needSubs;
	pRedisContext = (redisContext *)redisConnectWithTimeout(
		ip, port, t);
	if ((NULL == pRedisContext) || (pRedisContext->err))
	{
		if (pRedisContext)
			ERRORLOG << "connect error:" << pRedisContext->errstr;
		else
			ERRORLOG << "connect error: can't allocate redis context.";
		return false;
	}
	
	//订阅redis键空间通知
	if(needSubs)
	{
		DEBUGLOG << "subscribe redis keyspace notification!!!";
		thread thAsyncSubALL(CRedis_Utils::thAsyncSubsAll, this);
		thAsyncSubALL.detach();
	}
	this->is_connected = true;		//连接成功
	return true;
}

void CRedis_Utils::disconnect()
{
	//if(this->needSubs && !pRedisAsyncContext)
	//	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	//close();
	//delete this;
	this->is_connected = false;
}

void* CRedis_Utils::thAsyncSubsAll(void *arg)
{
	CRedis_Utils *self = (CRedis_Utils *)arg;
#ifdef _WIN32
	/* For Win32_IOCP the event loop must be created before the async connect */
	self->loop = aeCreateEventLoop(1024 * 10);
#else
	signal(SIGPIPE, SIG_IGN);
	self->base = event_base_new();
#endif
	self->pRedisAsyncContext = redisAsyncConnect(self->ip.c_str(), self->port);
	if (self->pRedisAsyncContext->err) {
		printf("Error: %s\n", self->pRedisAsyncContext->errstr);
		return NULL;
	}
#ifdef _WIN32
	redisAeAttach(self->loop, self->pRedisAsyncContext);
#else
	redisLibeventAttach(self->pRedisAsyncContext, self->base);
#endif

	redisAsyncSetConnectCallback(self->pRedisAsyncContext, CRedis_Utils::connectCallback);
	redisAsyncSetDisconnectCallback(self->pRedisAsyncContext, CRedis_Utils::disconnectCallback);
	std::string cmd = std::string(R_PSUBS) + std::string(" ") + std::string(R_KEYSPACE) + std::string("*");
	redisAsyncCommand(self->pRedisAsyncContext, subsAllCallback, self, cmd.c_str()/*"PSUBSCRIBE __keyspace@0__:*"*/);
	DEBUGLOG << "subscribe redis keyspace notification!!! cmd = " << cmd;

#ifdef _WIN32
	aeMain(self->loop);
#else
	event_base_dispatch(self->base);
#endif
	return NULL;
}

int CRedis_Utils::get(const char* _key, char* sRlt)
{
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip 
			<< ", port = " << port;
		std::string rlt = "redis is not connected...";
		memcpy(sRlt, rlt.c_str(), rlt.length());
		return -1;
	}
	std::string new_key = genNewKey(_key);
	std::string cmd = std::string(R_GET) + std::string(" ") + std::string(new_key);
	DEBUGLOG << "get cmd = " << cmd;

	if(!redisRPC.isServiceModelAvailable(new_key.c_str()) || !redisRPC.isKeySubs(new_key.c_str()))
	{
		//get失败(nil也表示失败) 该key值不需要处理或者没有可用服务
		//直接返回数据
		DEBUGLOG << "no available service or no service subs the key-->" << new_key;
		if (sendCmd(cmd.c_str(), sRlt))
			return strlen(sRlt);
		else if (strlen(sRlt))
			return -1;
		else
			return 0;
	} 
	if (strlen(sRlt) > 0)
		memset(sRlt, 0, strlen(sRlt));

	//远程调用处理数据
	DEBUGLOG << "process the key before return.   key-->" << new_key;
	redisRPC.processKey(new_key.c_str(), 5000);		
	DEBUGLOG << "process the key, job done.   key-->" << new_key;
	
	//返回数据
	if (sendCmd(cmd.c_str(), sRlt))
		return strlen(sRlt);
	else if (sizeof(sRlt))
		return -1;
	else
		return 0;
}

bool CRedis_Utils::set(const char* _key, const char* _value, char *sRlt)
{
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip
			<< ", port = " << port;
		std::string rlt = "redis is not connected...";
		memcpy(sRlt, rlt.c_str(), rlt.length());
		return false;
	}
	std::string new_key = genNewKey(_key);
	std::string cmd = std::string(R_SET) + std::string(" ") 
		+ new_key + std::string(" ") + _value;

	DEBUGLOG << "set cmd = " << cmd;
	if (sendCmd(cmd.c_str(), sRlt))
		return true;
	else
		return false;
}

bool CRedis_Utils::push(const char* list_name, const char* _value, char *sRlt)
{
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip
			<< ", port = " << port;
		std::string rlt = "redis is not connected...";
		memcpy(sRlt, rlt.c_str(), rlt.length());
		return false;
	}
	std::string new_list_name = genNewKey(list_name);
	std::string cmd = std::string(R_PUSH) + std::string(" ")
		+ new_list_name + std::string(" ") + _value;
	DEBUGLOG << "push cmd = " << cmd;
	if (sendCmd(cmd.c_str(), sRlt))
		return true;
	else
		return false;
}

int CRedis_Utils::pop(const char* list_name, char *sRlt)
{
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip
			<< ", port = " << port;
		std::string rlt = "redis is not connected...";
		memcpy(sRlt, rlt.c_str(), rlt.length());
		return -1;
	}
	std::string new_list_name = genNewKey(list_name);
	std::string cmd = std::string(R_POP) + std::string(" ") + new_list_name;
	DEBUGLOG << "pop cmd = " << cmd; 
	if (sendCmd(cmd.c_str(), sRlt))
		return strlen(sRlt);
	else if (strlen(sRlt))
		return -1;
	else
		return 0;
}

void CRedis_Utils::subs(const char *key, subsCallback cb)
{
	std::string new_key = genNewKey(key);
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	if(needSubs)
	{
		mapSubsCB::iterator it = m_subsKeys.find(std::string(new_key));
		if (it == m_subsKeys.end())
		{
			m_subsKeys.insert(std::pair<std::string, subsCallback>(std::string(new_key), cb));
			
			DEBUGLOG << "CRedis_Utils::subs 订阅成功 key = " << new_key
				<< ", size = " << m_subsKeys.size();
		}
		else
			DEBUGLOG << "CRedis_Utils::subs 该键已经订阅 key = " << new_key;
	}
	else
		DEBUGLOG << "redis subs function is shutdown!!! needSubs = " << needSubs;
}

void CRedis_Utils::unsubs(const char *key)
{
	std::string new_key = genNewKey(key);
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	mapSubsCB::iterator it = m_subsKeys.find(std::string(new_key));
	if (it != m_subsKeys.end())
	{
		m_subsKeys.erase(new_key);
		DEBUGLOG << "CRedis_Utils::unsubs 取消订阅成功 key = " << new_key 
			<< ", size = " << m_subsKeys.size();
	}
	else
		DEBUGLOG << "CRedis_Utils::unsubs 该键尚未被订阅 key = " << new_key;
}

void CRedis_Utils::pull(const char *key, pullCallback cb)
{
	std::string new_key = genNewKey(key);
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	if(needSubs)
	{
		mapPullCB::iterator it = m_pullKeys.find(std::string(new_key));
		if (it == m_pullKeys.end())
		{
			m_pullKeys.insert(std::pair<std::string, pullCallback>(std::string(new_key), cb));
			DEBUGLOG << "CRedis_Utils::pull 订阅成功 key = " << new_key
				<< ", size = " << m_pullKeys.size();
		}
		else
			DEBUGLOG << "CRedis_Utils::pull 该键已经订阅 key = " << new_key;
	}
	else
		DEBUGLOG << "redis subs function is shutdown!!! needSubs = " << needSubs;
}

void CRedis_Utils::unpull(const char *key)
{
	std::string new_key = genNewKey(key);
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	mapPullCB::iterator it = m_pullKeys.find(std::string(new_key));
	if (it != m_pullKeys.end())
	{
		m_pullKeys.erase(new_key);
		DEBUGLOG << "CRedis_Utils::pull 取消订阅成功 key = " << new_key 
			<< ", size = " << m_subsKeys.size();
	}
	else
		DEBUGLOG << "CRedis_Utils::pull 该键已经订阅 key = " << new_key;
}

void CRedis_Utils::subsClientGetOp(const char *key, const char *reqChlName,
	const char *heartbeatChnName)
{
	std::string new_key = genNewKey(key);
	if (!is_connected)
	{
		DEBUGLOG << "redis 服务尚未连接... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	if(needSubs)
		redisRPC.subsClientGetOp(new_key.c_str(), reqChlName, heartbeatChnName);
	else
		DEBUGLOG << "CRedis_Utils::pull 该键已经订阅 key = " << new_key;
}

void CRedis_Utils::close()
{
	DEBUGLOG << "close redis connection ip = " << this->ip << ", port = "
		<< this->port;
	if (pRedisContext != nullptr)
		redisFree(pRedisContext);
	if(pRedisAsyncContext){
		redisAsyncFree(pRedisAsyncContext);
		//redisAsyncDisconnect(pRedisAsyncContext);

#ifdef _WIN32
		aeStop(loop);
#else
		event_base_loopbreak(base);
#endif
	}
}


std::string CRedis_Utils::genNewKey(std::string old_key)
{
	return client_id + old_key;
}

std::string CRedis_Utils::getOldKey(std::string new_key)
{
	return new_key.substr(client_id.length());
}

bool CRedis_Utils::sendCmd(const char *cmd, char *sRlt)
{
	bool bRlt = false;
	redisReply *pRedisReply = (redisReply*)redisCommand(pRedisContext, cmd);  //执行INFO命令
	
	//错误处理!!!
	if (!pRedisReply)
	{
		//WARNLOG << "redis cmd = " << cmd << ", err = " << pRedisContext->errstr;
		//redisFree(pRedisContext);
		//connect(ip.c_str(), port);
		//pRedisReply = (redisReply*)redisCommand(pRedisContext, cmd);
		//if (pRedisReply)
		//{
		//	if (replyCheck(pRedisReply, sRlt))
		//		bRlt = true;
		//	freeReplyObject(pRedisReply);
		//	return bRlt;
		//}
		ERRORLOG << "Error send cmd[" << pRedisContext->err << ":" << pRedisContext->errstr << "]";
		memcpy(sRlt, pRedisContext->errstr, strlen(pRedisContext->errstr));
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
		bRlt = false;
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
		bRlt = false;
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
	}
	DEBUGLOG << "Async DisConnected...";
}

void CRedis_Utils::subsAllCallback(redisAsyncContext *c, void *r, void *data)
{
	CRedis_Utils *self = (CRedis_Utils *)data;
	redisReply *reply = (redisReply *)r;
	if (reply == NULL) return;
	if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
		DEBUGLOG << "Received[%s " << self->ip.c_str() << "] channel" 
			<< reply->element[1]->str << ": " << reply->element[2]->integer;
	}
	else if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 4) {
		std::string key = split(reply->element[2]->str, "__:")[1];
		DEBUGLOG << "Received[%s " << self->ip.c_str() << "] channel"
			<< reply->element[1]->str << " -- " << key
			<< " : " << reply->element[3]->str;
		self->callSubsCB(key.c_str(), reply->element[3]->str);
	}
}

void CRedis_Utils::callSubsCB(const char *key, const char *key_op)
{
	std::string redisCmd;
	char *value = (char *)malloc(REDIS_BUF_SIZE);
	memset(value, 0, REDIS_BUF_SIZE);
	std::string old_key = getOldKey(key);

	//如果能匹配多个订阅的键，会执行多次
	if (m_subsKeys.size() > 0)
	{
		mapSubsCB::iterator it_subs = m_subsKeys.begin();
		for(; it_subs != m_subsKeys.end(); ++it_subs)
		{
			if (keyMatch(std::string(key), it_subs->first))
			{
				//将键和值都返回给上层，value长度为0表示键被del或者get rpop失败
				if (strcmp(key_op, "set") == 0)
				{
					redisCmd = R_GET + std::string(" ") + key;
					if (!sendCmd(redisCmd.c_str(), value))
						memset(value, 0, REDIS_BUF_SIZE);
					it_subs->second(old_key.c_str(), value);
				}
			}
		}
	}
	if (m_pullKeys.size() > 0)
	{
		mapPullCB::iterator it_pull = m_pullKeys.begin();
		for (; it_pull != m_pullKeys.end(); ++it_pull)
		{
			//将键和值都返回给上层，value长度为0表示键被del或者get rpop失败
			if (keyMatch(std::string(key), it_pull->first))
			{
				if (strcmp(key_op, "lpush") == 0)
				{
					redisCmd = R_POP + std::string(" ") + key;
					if (!sendCmd(redisCmd.c_str(), value))
						memset(value, 0, REDIS_BUF_SIZE);
					it_pull->second(old_key.c_str(), value);
				}
			}
		}
	}
	free(value);
}

