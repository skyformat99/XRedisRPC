#include "Redis_Utils.h"

CRedis_Utils::CRedis_Utils(std::string clientID)
{
	this->is_connected = false;
	if (clientID.length() > 0)
		this->client_id = clientID;
	else
		clientID = "default";
}

CRedis_Utils::~CRedis_Utils()
{
	close();
}

bool CRedis_Utils::connect(const char* ip, int port, bool isSubs)
{
	if(is_connected) 
	{
		DEBUGLOG << "redis �����Ѿ����ӳɹ����ظ�����... ip = " << ip 
			<< ", port = " << port;
		return true;
	}
	this->ip = ip;
	this->port = port;
	this->needSubs = isSubs;
	redisRPC = CRedisRPC(ip, port);
	timeval t = { 1, 500 };

	//����redis����
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
	
	//����redis���ռ�֪ͨ
	if(needSubs)
	{
		DEBUGLOG << "subscribe redis keyspace notification!!!";
		thread thAsyncSubALL(CRedis_Utils::thAsyncSubsAll, this);
		thAsyncSubALL.detach();
	}
	this->is_connected = true;		//���ӳɹ�
	return true;
}

void CRedis_Utils::disconnect()
{
	//if(this->needSubs && !pRedisAsyncContext)
	//	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	//close();
	//delete this;
	this->is_connected = false;
	if (this->needSubs)
	{
		this->needSubs = false;
		if (m_subsKeys.size() > 0)
			m_subsKeys.clear();
		if (m_pullKeys.size() > 0)
			m_pullKeys.clear();
		redisRPC.clearChnl();
	}
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
		DEBUGLOG << "redis ������δ����... ip = " << ip 
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
		//getʧ��(nilҲ��ʾʧ��) ��keyֵ����Ҫ�������û�п��÷���
		//ֱ�ӷ�������
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

	//Զ�̵��ô�������
	DEBUGLOG << "process the key before return.   key-->" << new_key;
	redisRPC.processKey(new_key.c_str(), 5000);		
	DEBUGLOG << "process the key, job done.   key-->" << new_key;
	
	//��������
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
		DEBUGLOG << "redis ������δ����... ip = " << ip
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
		DEBUGLOG << "redis ������δ����... ip = " << ip
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
		DEBUGLOG << "redis ������δ����... ip = " << ip
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
		DEBUGLOG << "redis ������δ����... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	if(needSubs)
	{
		subs_lock.lock();
		mapSubsCB::iterator it = m_subsKeys.find(std::string(new_key));
		if (it == m_subsKeys.end())
		{
			m_subsKeys.insert(std::pair<std::string, subsCallback>(std::string(new_key), cb));
			
			DEBUGLOG << "CRedis_Utils::subs ���ĳɹ� key = " << new_key
				<< ", size = " << m_subsKeys.size();
		}
		else
			DEBUGLOG << "CRedis_Utils::subs �ü��Ѿ����� key = " << new_key;
		subs_lock.unlock();
	}
	else
		DEBUGLOG << "redis subs function is shutdown!!! needSubs = " << needSubs;
}

void CRedis_Utils::unsubs(const char *key)
{
	std::string new_key = genNewKey(key);
	if (!is_connected)
	{
		DEBUGLOG << "redis ������δ����... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	subs_lock.lock();
	mapSubsCB::iterator it = m_subsKeys.find(std::string(new_key));
	if (it != m_subsKeys.end())
	{
		m_subsKeys.erase(new_key);
		DEBUGLOG << "CRedis_Utils::unsubs ȡ�����ĳɹ� key = " << new_key 
			<< ", size = " << m_subsKeys.size();
	}
	else
		DEBUGLOG << "CRedis_Utils::unsubs �ü���δ������ key = " << new_key;
	subs_lock.unlock();
}

void CRedis_Utils::pull(const char *key, pullCallback cb)
{
	std::string new_key = genNewKey(key);
	if (!is_connected)
	{
		DEBUGLOG << "redis ������δ����... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	if(needSubs)
	{
		pull_lock.lock();
		mapPullCB::iterator it = m_pullKeys.find(std::string(new_key));
		if (it == m_pullKeys.end())
		{
			m_pullKeys.insert(std::pair<std::string, pullCallback>(std::string(new_key), cb));
			DEBUGLOG << "CRedis_Utils::pull ���ĳɹ� key = " << new_key
				<< ", size = " << m_pullKeys.size();
		}
		else
			DEBUGLOG << "CRedis_Utils::pull �ü��Ѿ����� key = " << new_key;
		pull_lock.unlock();
	}
	else
		DEBUGLOG << "redis subs function is shutdown!!! needSubs = " << needSubs;
}

void CRedis_Utils::unpull(const char *key)
{
	std::string new_key = genNewKey(key);
	if (!is_connected)
	{
		DEBUGLOG << "redis ������δ����... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	pull_lock.lock();
	mapPullCB::iterator it = m_pullKeys.find(std::string(new_key));
	if (it != m_pullKeys.end())
	{
		m_pullKeys.erase(new_key);
		DEBUGLOG << "CRedis_Utils::pull ȡ�����ĳɹ� key = " << new_key 
			<< ", size = " << m_subsKeys.size();
	}
	else
		DEBUGLOG << "CRedis_Utils::pull �ü��Ѿ����� key = " << new_key;
	pull_lock.unlock();
}

void CRedis_Utils::subsClientGetOp(const char *key, const char *reqChlName,
	const char *heartbeatChnName, clientOpCallBack cb)
{
	if (!is_connected)
	{
		DEBUGLOG << "redis ������δ����... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return ;
	}
	if(needSubs)
	{
		std::string new_key = genNewKey(key);
		std::string new_reqChnl = genNewKey(reqChlName);
		std::string reqChls = new_reqChnl + std::string("*");		//ÿ���ͻ��˶�Ӧһ���������
		std::string new_hblist = genNewKey(heartbeatChnName);
		mapReqCB::iterator it = m_reqChnl.find(std::string(reqChls));
		req_lock.lock();
		if (it == m_reqChnl.end())
		{
			m_reqChnl.insert(std::pair<std::string, pullCallback>(std::string(reqChls), cb));
			DEBUGLOG << "subsClientGetOp ���ĳɹ� key = " << reqChls
				<< ", size = " << m_reqChnl.size();
		}
		else
			DEBUGLOG << "subsClientGetOp �ü��Ѿ����� key = " << reqChls;
		req_lock.unlock();
		redisRPC.subsClientGetOp(new_key.c_str(), new_reqChnl.c_str(), new_hblist.c_str());
	}
	else
		DEBUGLOG << "redis subs function is shutdown!!! needSubs = " << needSubs;
}

void CRedis_Utils::unsubClientGetOp(const char *keys)
{
	if (!is_connected)
	{
		DEBUGLOG << "redis ������δ����... ip = " << ip
			<< ", port = " << port;
		//std::string rlt = "redis is not connected...";
		//memcpy(sRlt, rlt.c_str(), rlt.length());
		return;
	}
	if (m_reqChnl.size() > 0)
	{
		std::string new_key = genNewKey(keys);
		std::string reqChnl = redisRPC.unsubClientGetOp(new_key.c_str()) + "*";
		req_lock.lock();
		if (((reqChnl.length() - 1) > 0) && (m_reqChnl.find(reqChnl) != m_reqChnl.end()))
		{
			//DEBUGLOG << "before unsubGetOp chnlName = " << reqChnl << ", size = " << m_reqChnl.size();
			m_reqChnl.erase(reqChnl);
			DEBUGLOG << "unsubGetOp chnlName = " << reqChnl << ", size = " << m_reqChnl.size();
		}
		req_lock.unlock();
	}
	
}

void CRedis_Utils::stopSubClientGetOp() 
{
	redisRPC.clearChnl();
	req_lock.lock();
	if (m_reqChnl.size() > 0)
		m_reqChnl.clear();
	req_lock.unlock();
}

void CRedis_Utils::close()
{
	DEBUGLOG << "close redis connection ip = " << this->ip << ", port = "
		<< this->port;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));	//��ֹ�����˳����죬�����쳣���첽�ص�������������...
	if (pRedisContext != nullptr)
		redisFree(pRedisContext);
	if (needSubs)
	{
		if (pRedisAsyncContext) {
			redisAsyncFree(pRedisAsyncContext);
			//redisAsyncDisconnect(pRedisAsyncContext);

#ifdef _WIN32
			aeStop(loop);
#else
			//event_base_loopbreak(base);
			event_base_free(base);
#endif
		}
	}
}


std::string CRedis_Utils::genNewKey(std::string old_key)
{
	return client_id + old_key;
}

std::string CRedis_Utils::getOldKey(std::string new_key)
{
	if (new_key.length() > client_id.length())
		return new_key.substr(client_id.length());
	else
		return new_key;
}

bool CRedis_Utils::sendCmd(const char *cmd, char *sRlt)
{
	bool bRlt = false;
	redisReply *pRedisReply = (redisReply*)redisCommand(pRedisContext, cmd);  //ִ��INFO����
	
	//������!!!
	if (!pRedisReply)
	{
		WARNLOG << "redis cmd = " << cmd << ", err = " << pRedisContext->errstr
			<< ", try to reconnect...";
		is_connected = false;
		redisFree(pRedisContext);
		connect(ip.c_str(), port);
		pRedisReply = (redisReply*)redisCommand(pRedisContext, cmd);
		if (pRedisReply)
		{
			if (replyCheck(pRedisReply, sRlt))
				bRlt = true;
			freeReplyObject(pRedisReply);
			return bRlt;
		}
		is_connected = false;
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
	case REDIS_REPLY_STATUS:		//��ʾ״̬������ͨ��str�ֶβ鿴���ַ���������len�ֶ�
		bRlt = false;
		DEBUGLOG << "type:REDIS_REPLY_STATUS, reply->len:" 
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str;
		memcpy(sReply, pRedisReply->str, pRedisReply->len);
		break;
	case REDIS_REPLY_ERROR:			//��ʾ�����鿴������Ϣ�����ϵ�str,len�ֶ�
		bRlt = false;
		WARNLOG << "type:REDIS_REPLY_ERROR, reply->len:"
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str;
		memcpy(sReply, pRedisReply->str, pRedisReply->len);
		break;
	case REDIS_REPLY_INTEGER:		//������������integer�ֶλ�ȡֵ
		DEBUGLOG << "type:REDIS_REPLY_INTEGER, reply->integer:" << pRedisReply->integer;
		memcpy(sReply, int2str(pRedisReply->integer).c_str(), int2str(pRedisReply->integer).length());
		break;
	case REDIS_REPLY_NIL:			//û�����ݷ���
		bRlt = false;
		DEBUGLOG << "type:REDIS_REPLY_NIL, no data";
		break;
	case REDIS_REPLY_STRING:		//�����ַ������鿴str,len�ֶ�
		DEBUGLOG << "type:REDIS_REPLY_STRING, reply->len:"
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str; 
		memcpy(sReply, pRedisReply->str, pRedisReply->len);
		break;
	case REDIS_REPLY_ARRAY:			//����һ�����飬�鿴elements��ֵ�������������ͨ��element[index]�ķ�ʽ��������Ԫ�أ�ÿ������Ԫ����һ��redisReply�����ָ��
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
		DEBUGLOG << "Received[ " << self->ip.c_str() << "] channel"
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

	//�����ƥ�������ĵļ�����ִ�ж��
	subs_lock.lock(); 
	if (m_subsKeys.size() > 0)			//����
	{
		mapSubsCB::iterator it_subs = m_subsKeys.begin();
		for(; it_subs != m_subsKeys.end(); ++it_subs)
		{
			if (keyMatch(std::string(key), it_subs->first))
			{
				//������ֵ�����ظ��ϲ㣬value����Ϊ0��ʾ����del����get rpopʧ��
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
	subs_lock.unlock();
	pull_lock.lock();
	if (m_pullKeys.size() > 0)		//��ȡ
	{
		mapPullCB::iterator it_pull = m_pullKeys.begin();
		for (; it_pull != m_pullKeys.end(); ++it_pull)
		{
			//������ֵ�����ظ��ϲ㣬value����Ϊ0��ʾ����del����get rpopʧ��
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
	pull_lock.unlock();
	req_lock.lock();
	if (this->m_reqChnl.size() > 0)	//�������
	{ 
		size_t s = m_reqChnl.size(); 
		mapReqCB::iterator it_req = m_reqChnl.begin();
		for (; it_req != m_reqChnl.end(); ++it_req)
		{
			if (keyMatch(std::string(key), it_req->first))
			{
				if (strcmp(key_op, "lpush") == 0)
				{
					std::string get_key = "";
					std::string get_value = "";
					if (sendCmd(popCmd.c_str(), value))		//��ȡ��������е�ֵ
					{
						get_key = value;
						memset(value, 0, REDIS_BUF_SIZE);
						getCmd.clear();
						getCmd = R_GET + std::string(" ") + get_key;
						if (sendCmd(getCmd.c_str(), value))		//��ȡ��Ӧ��ֵ
							get_value = value;
						else
							WARNLOG << "get value failed... getCmd = " << getCmd
								<< ", errstr = " << value;
					}
					else
						WARNLOG << "��ȡ���������Ϣʧ��... popCmd = " << getCmd
							<< ", errstr = " << value;
					it_req->second(getOldKey(get_key).c_str(), get_value.c_str());
				}
			}
		}
	}
	req_lock.unlock();
	free(value);
}

