#include "Redis_Utils.h"

CRedis_Utils::CRedis_Utils(std::string strClientID)
{
	this->m_bIsConnected = false;
	if (strClientID.length() > 0)
		this->m_strClientId = strClientID;
	else
		this->m_strClientId = "default";
}

CRedis_Utils::~CRedis_Utils()
{
	close();
}

bool CRedis_Utils::connect(const std::string & strIp, int port, bool isSubs)
{
	if(m_bIsConnected) 
	{
		DEBUGLOG("redis �����Ѿ����ӳɹ����ظ�����... ip = " << strIp.c_str() 
			<< ", port = " << port);
		return true;
	}
	this->m_strIp = strIp;
	this->m_iPort = port;
	this->m_bNeedSubs = isSubs;
	m_redisRPC = CRedisRPC(strIp, port);

	//����redis����
	DEBUGLOG("connect to redis server ip = " << this->m_strIp.c_str() << ", port = "
		<< this->m_iPort << ", isSubs = " << this->m_bNeedSubs);
	if (!_connect(strIp, port))
		return false;
	
	//����redis���ռ�֪ͨ
	if(m_bNeedSubs)
	{
		DEBUGLOG("subscribe redis keyspace notification!!!");
		thAsyncKeyNotify = std::thread(CRedis_Utils::thAsyncSubsAll, this);
	}
	this->m_bIsConnected = true;		//���ӳɹ�
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
		m_subsLock.lock(); 
		if (m_mapSubsKeys.size() > 0)
			m_mapSubsKeys.clear();
		m_subsLock.unlock();
		m_pullLock.lock();
		if (m_mapPullKeys.size() > 0)
			m_mapPullKeys.clear();
		m_redisRPC.clearChnl();
		m_pullLock.unlock();
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
	self->m_pRedisAsyncContext = redisAsyncConnect(self->m_strIp.c_str(), self->m_iPort);
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
	DEBUGLOG("subscribe redis keyspace notification!!! cmd = " << cmd.c_str());

#ifdef _WIN32
	aeMain(self->m_loop);
#else
	event_base_dispatch(self->m_base);
#endif
	DEBUGLOG("event dispatch done!!! --> " << self->m_strClientId.c_str());
	return nullptr;
}

int CRedis_Utils::get(const std::string & strInKey, std::string & strOutResult)
{
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str() 
				<< ", port = " << m_iPort);
		if(!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			strOutResult = "redis is not connected...";
			return -1;
		}
	}
	m_getLock.lock();
	bool bRlt = false;
	std::string new_key = genNewKey(strInKey.c_str());
	std::string cmd = std::string(R_GET) + std::string(" ") + std::string(new_key);
	DEBUGLOG("get cmd = " << cmd.c_str());
	if(!m_redisRPC.isServiceModelAvailable(new_key.c_str()) /*|| !m_redisRPC.isKeySubs(new_key.c_str())*/)
	{
		//getʧ��(nilҲ��ʾʧ��) ��keyֵ����Ҫ�������û�п��÷���
		//ֱ�ӷ�������
		DEBUGLOG("no available service-->" << new_key.c_str());
		bRlt = sendCmd(cmd, strOutResult);
		m_getLock.unlock();
		if (bRlt)
			return strOutResult.length();
		else
			return -1;
	} 

	//Զ�̵��ô�������
	DEBUGLOG("process the key before return.   key-->" << new_key.c_str());
	m_redisRPC.processKey(new_key.c_str(), 5000, m_getLock);		
	DEBUGLOG("process the key, job done.   key-->" << new_key.c_str());
	
	//��������
	m_getLock.lock();
	bRlt = sendCmd(cmd, strOutResult);
	m_getLock.unlock();
	if (bRlt)
		return strOutResult.length();
	else
		return -1;
}

bool CRedis_Utils::set(const std::string & strInKey, const std::string & strInValue, std::string & strOutResult)
{
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			strOutResult = "redis is not connected...";
			return false;
		}
	}
	m_setLock.lock();
	bool bRlt = false;
	std::string new_key = genNewKey(strInKey);
	std::string cmd = std::string(R_SET) + std::string(" ") 
		+ new_key + std::string(" ") + strInValue;

	DEBUGLOG("set cmd = " << cmd.c_str());
	bRlt = sendCmd(cmd, strOutResult);
	m_setLock.unlock();
	if (bRlt)
		return true;
	else
		return false;
}

bool CRedis_Utils::push(const std::string & strInListName, const std::string & strInValue, std::string & strOutResult)
{
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			strOutResult = "redis is not connected...";
			return false;
		}
	}
	m_pushLock.lock();
	std::string new_list_name = genNewKey(strInListName);
	std::string cmd = std::string(R_PUSH) + std::string(" ")
		+ new_list_name + std::string(" ") + strInValue;
	DEBUGLOG("push cmd = " << cmd.c_str());
	bool bRlt = sendCmd(cmd, strOutResult);
	m_pushLock.unlock();
	if (bRlt)
		return true;
	else
		return false;
}

int CRedis_Utils::pop(const std::string & strInListName, std::string & strOutResult)
{
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			strOutResult = "redis is not connected...";
			return -1;
		}
	}
	m_popLock.lock();
	std::string new_list_name = genNewKey(strInListName);
	std::string cmd = std::string(R_POP) + std::string(" ") + new_list_name;
	DEBUGLOG("pop cmd = " << cmd.c_str());
	bool bRlt = sendCmd(cmd, strOutResult);
	m_popLock.unlock();
	if (bRlt)
		return strOutResult.length();
	else 
		return -1;
}

void CRedis_Utils::subs(const std::string & strInKey, subsCallback cb)
{
	if (strInKey.length() == 0 || cb == nullptr)
	{
		WARNLOG("key or cb is null... subs failed");
		return;
	}

	std::string new_key = genNewKey(strInKey);
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			return;
		}
	}
	if(m_bNeedSubs)
	{
		m_subsLock.lock();
		mapSubsCB::iterator it = m_mapSubsKeys.find(new_key);
		if (it == m_mapSubsKeys.end())
		{
			m_mapSubsKeys.insert(std::pair<std::string, subsCallback>(new_key, cb));
			DEBUGLOG("CRedis_Utils::subs ���ĳɹ� key = " << new_key.c_str()
				<< ", size = " << m_mapSubsKeys.size());
		}
		else
			DEBUGLOG("CRedis_Utils::subs �ü��Ѿ����� key = " << new_key.c_str());
		m_subsLock.unlock();
	}
	else
		DEBUGLOG("redis subs function is shutdown!!! needSubs = " << m_bNeedSubs);
}

void CRedis_Utils::unsubs(const std::string & strInKey)
{
	if (strInKey.length() == 0)
	{
		WARNLOG("key or cb is null... subs failed");
		return;
	}
	std::string new_key = genNewKey(strInKey);
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			return;
		}
	}
	m_subsLock.lock();
	mapSubsCB::iterator it = m_mapSubsKeys.find(new_key);
	if (it != m_mapSubsKeys.end())
	{
		m_mapSubsKeys.erase(new_key);
		DEBUGLOG("CRedis_Utils::unsubs ȡ�����ĳɹ� key = " << new_key.c_str()
			<< ", size = " << m_mapSubsKeys.size());
	}
	else
		DEBUGLOG("CRedis_Utils::unsubs �ü���δ������ key = " << new_key.c_str());
	m_subsLock.unlock();
}

void CRedis_Utils::pull(const std::string & strInKey, pullCallback cb)
{
	if (strInKey.length() == 0 || cb == nullptr)
	{
		WARNLOG("key or cb is null... subs failed");
		return;
	}

	std::string new_key = genNewKey(strInKey);
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			return;
		}
	}
	if(m_bNeedSubs)
	{
		m_pullLock.lock();
		mapPullCB::iterator it = m_mapPullKeys.find(new_key);
		if (it == m_mapPullKeys.end())
		{
			m_mapPullKeys.insert(std::pair<std::string, pullCallback>(new_key, cb));
			DEBUGLOG("CRedis_Utils::pull ���ĳɹ� key = " << new_key.c_str()
				<< ", size = " << m_mapPullKeys.size());
		}
		else
			DEBUGLOG("CRedis_Utils::pull �ü��Ѿ����� key = " << new_key.c_str());
		m_pullLock.unlock();
	}
	else
		DEBUGLOG("redis subs function is shutdown!!! needSubs = " << m_bNeedSubs);
}

void CRedis_Utils::unpull(const std::string & strInKey)
{
	if (strInKey.length() == 0)
	{
		WARNLOG("key is null... unpull failed");
		return;
	}
	std::string new_key = genNewKey(strInKey);
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			return;
		}
	}
	m_pullLock.lock();
	mapPullCB::iterator it = m_mapPullKeys.find(new_key);
	if (it != m_mapPullKeys.end())
	{
		m_mapPullKeys.erase(new_key);
		DEBUGLOG("CRedis_Utils::pull ȡ�����ĳɹ� key = " << new_key.c_str()
			<< ", size = " << m_mapSubsKeys.size());
	}
	else
		DEBUGLOG("CRedis_Utils::pull �ü��Ѿ����� key = " << new_key.c_str());
	m_pullLock.unlock();
}

void CRedis_Utils::subsClientGetOp(const std::string & strInKey, clientOpCallBack cb)
{
	if (strInKey.length() == 0 || cb == nullptr)
	{
		WARNLOG("key or cb is null... subs failed");
		return;
	}

	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			return;
		}
	}
	if (m_bNeedSubs)
	{
		std::string new_key = genNewKey(strInKey);				//��
		std::string heartBeat = new_key + HEARTLIST;			//��������
		std::string reqChnl = new_key + REQLIST;				//�������
		std::string subsReqChnl = reqChnl + std::string("*");	//ÿ���ͻ��˶�Ӧһ���������

		mapReqCB::iterator it = m_mapReqChnl.find(subsReqChnl);
		m_reqLock.lock();
		if (it == m_mapReqChnl.end())
		{
			m_mapReqChnl.insert(std::pair<std::string, pullCallback>(subsReqChnl, cb));
			DEBUGLOG("subsClientGetOp ���ĳɹ� key = " << subsReqChnl.c_str()
				<< ", size = " << m_mapReqChnl.size());
		}
		else
			DEBUGLOG("subsClientGetOp �ü��Ѿ����� key = " << subsReqChnl.c_str());
		m_reqLock.unlock();
		m_redisRPC.subsClientGetOp(new_key.c_str(), reqChnl.c_str(), heartBeat.c_str());
	}
}

void CRedis_Utils::unsubClientGetOp(const std::string & strInKey)
{
	if (strInKey.length() == 0)
	{
		WARNLOG("key is null... unsubClientGetOp failed");
		return;
	}
	if (!m_bIsConnected)
	{
		DEBUGLOG("redis ������δ����...������������... ip = " << m_strIp.c_str()
			<< ", port = " << m_iPort);
		if (!_connect(m_strIp.c_str(), m_iPort))
		{
			WARNLOG("redis������������ʧ��... ");
			return;
		}
	}
	if (m_mapReqChnl.size() > 0)
	{
		std::string new_key = genNewKey(strInKey);
		m_reqLock.lock();
		std::string reqChnl = m_redisRPC.unsubClientGetOp(new_key.c_str()) + "*";
		if (((reqChnl.length() - 1) > 0) && (m_mapReqChnl.find(reqChnl) != m_mapReqChnl.end()))
		{
			//DEBUGLOG << "before unsubGetOp chnlName = " << reqChnl << ", size = " << m_reqChnl.size();
			m_mapReqChnl.erase(reqChnl);
			DEBUGLOG("unsubGetOp chnlName = " << reqChnl.c_str() << ", size = " << m_mapReqChnl.size());
		}
		m_reqLock.unlock();
	}
	
}

void CRedis_Utils::stopSubClientGetOp() 
{
	m_reqLock.lock();
	m_redisRPC.clearChnl();
	if (m_mapReqChnl.size() > 0)
		m_mapReqChnl.clear();
	m_reqLock.unlock();
}

void CRedis_Utils::close()
{
	DEBUGLOG("close redis connection ip = " << this->m_strIp.c_str() << ", port = "
		<< this->m_iPort << ", clientID-->" << m_strClientId.c_str());
	std::this_thread::sleep_for(std::chrono::milliseconds(500));	//��ֹ�����˳����죬�����쳣���첽�ص�������������...
	if (m_bNeedSubs)
	{
		//stopSubClientGetOp();
		if (m_pRedisAsyncContext) { 
			//m_aeStopLock.lock(); 
			redisAsyncDisconnect(m_pRedisAsyncContext);
			//redisAsyncFree(m_pRedisAsyncContext);
			//m_pRedisAsyncContext = nullptr;

#ifdef _WIN32
		
		aeStop(m_loop);
		m_loop = nullptr;
		DEBUGLOG("stop event dispatch --> " << m_strClientId.c_str());
#else
		//event_base_loopbreak(base);
		event_base_free(m_base);
		m_base = nullptr;
#endif
		if(thAsyncKeyNotify.joinable())
			thAsyncKeyNotify.join();
		//m_aeStopLock.unlock();
		}
	}
	if (m_pRedisContext != nullptr)
	{
		redisFree(m_pRedisContext);
		m_pRedisContext = nullptr;
	}
}


bool CRedis_Utils::_connect(const std::string & strIp, int port)
{
	timeval t = { 1, 500 };
	m_pRedisContext = (redisContext *)redisConnectWithTimeout(
		strIp.c_str(), port, t);
	if ((NULL == m_pRedisContext) || (m_pRedisContext->err))
	{
		if (m_pRedisContext)
			ERRORLOG("connect error:" << m_pRedisContext->errstr);
		else
			ERRORLOG("connect error: can't allocate redis context.");
		return false;
	}
	m_bIsConnected = true;
	return true;
}

std::string CRedis_Utils::genNewKey(const std::string & old_key)
{
	return m_strClientId + old_key;
}

std::string CRedis_Utils::getOldKey(const std::string & new_key)
{
	if (new_key.length() > m_strClientId.length())
		return new_key.substr(m_strClientId.length());
	else
		return new_key;
}

bool CRedis_Utils::sendCmd(const std::string & strInCmd, std::string & strOutResult)
{
	bool bRlt = false;
	//m_aeStopLock.lock();
	redisReply *pRedisReply = (redisReply*)redisCommand(m_pRedisContext, strInCmd.c_str());  //ִ��INFO����
	//m_aeStopLock.unlock();
	//������!!!
	if (!pRedisReply)
	{
		WARNLOG("redis cmd = " << strInCmd.c_str() << ", err = " << m_pRedisContext->errstr
			<< ", try to reconnect...");
		m_bIsConnected = false;
		_connect(m_strIp, m_iPort);
		pRedisReply = (redisReply*)redisCommand(m_pRedisContext, strInCmd.c_str());
		if (pRedisReply)
		{
			if (replyCheck(pRedisReply, strOutResult))
				bRlt = true;
			freeReplyObject(pRedisReply);
			//m_bIsConnected = true;
			return bRlt;
		}
		//m_bIsConnected = false;
		ERRORLOG("Error send cmd[" << m_pRedisContext->err << ":" << m_pRedisContext->errstr << "]");
		strOutResult = m_pRedisContext->errstr;
		return false;
	}
	if (replyCheck(pRedisReply, strOutResult))
		bRlt = true;
	freeReplyObject(pRedisReply);
	DEBUGLOG("redis cmd = " << strInCmd.c_str() << ", rlt = " << bRlt);
	return bRlt;
}

bool CRedis_Utils::replyCheck(redisReply *pRedisReply, std::string & strOutResult)
{
	bool bRlt = true;

	switch (pRedisReply->type) {
	case REDIS_REPLY_STATUS:		//��ʾ״̬������ͨ��str�ֶβ鿴���ַ���������len�ֶ�
		DEBUGLOG("type:REDIS_REPLY_STATUS, reply->len:" 
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str);
		strOutResult = pRedisReply->str;
		break;
	case REDIS_REPLY_ERROR:			//��ʾ�����鿴������Ϣ�����ϵ�str,len�ֶ�
		bRlt = false;
		WARNLOG("type:REDIS_REPLY_ERROR, reply->len:"
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str);
		strOutResult = pRedisReply->str;
		break;
	case REDIS_REPLY_INTEGER:		//������������integer�ֶλ�ȡֵ
		DEBUGLOG("type:REDIS_REPLY_INTEGER, reply->integer:" << pRedisReply->integer);
		strOutResult = int2str(pRedisReply->integer);
		break;
	case REDIS_REPLY_NIL:			//û�����ݷ���
		//bRlt = false;
		DEBUGLOG("type:REDIS_REPLY_NIL, no data");
		break;
	case REDIS_REPLY_STRING:		//�����ַ������鿴str,len�ֶ�
		DEBUGLOG("type:REDIS_REPLY_STRING, reply->len:"
			<< pRedisReply->len << ", reply->str:" << pRedisReply->str); 
		strOutResult = pRedisReply->str;
		break;
	case REDIS_REPLY_ARRAY:			//����һ�����飬�鿴elements��ֵ�������������ͨ��element[index]�ķ�ʽ��������Ԫ�أ�ÿ������Ԫ����һ��redisReply�����ָ��
		DEBUGLOG("------------------------------------------------------------");
		DEBUGLOG("type:REDIS_REPLY_ARRAY, reply->elements:" << pRedisReply->elements);
		for (int i = 0; i < pRedisReply->elements; i++) {
			//printf("%d: %s\n", i, pRedisReply->element[i]->str);
			std::string strRlt;
			replyCheck(pRedisReply->element[i], strRlt);
			strOutResult.append(strRlt);
		}
		DEBUGLOG("------------------------------------------------------------");
		break;
	default:
		WARNLOG("unkonwn type : " << pRedisReply->type);
		strOutResult = "unkonwn type : " + int2str(pRedisReply->type);
		bRlt = false;
		break;
	}
	
	return bRlt;
}


void CRedis_Utils::connectCallback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK) {
		ERRORLOG("Error: " << c->errstr);
		return;
	}
	DEBUGLOG("Async Connected...");
}

void CRedis_Utils::disconnectCallback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK) {
		ERRORLOG("Error: " << c->errstr);
		return;
	}
	DEBUGLOG("Async DisConnected...");
}

void CRedis_Utils::subsAllCallback(redisAsyncContext *c, void *r, void *data)
{
	CRedis_Utils *self = (CRedis_Utils *)data;
	redisReply *reply = (redisReply *)r;
	if (reply == NULL) return;
	if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
		DEBUGLOG("Received[%s " << self->m_strIp.c_str() << "] channel" 
			<< reply->element[1]->str << ": " << reply->element[2]->integer);
	}
	else if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 4) {
		std::string key = split(reply->element[2]->str, "__:")[1];
		DEBUGLOG("Received[ " << self->m_strIp.c_str() << "] channel"
			<< reply->element[1]->str << " -- " << key.c_str()
			<< " : " << reply->element[3]->str);
		self->callSubsCB(key, reply->element[3]->str);
	}
}

void CRedis_Utils::callSubsCB(const std::string & strInKey, const std::string & strInKeyOp)
{
	std::string getCmd = R_GET + std::string(" ") + strInKey;
	std::string popCmd = R_POP + std::string(" ") + strInKey;
	std::string sRlt;
	std::string old_key = getOldKey(strInKey);

	//�����ƥ�������ĵļ�����ִ�ж��
	m_subsLock.lock(); 
	if (m_mapSubsKeys.size() > 0)			//����
	{
		mapSubsCB::iterator it_subs = m_mapSubsKeys.begin();
		for(; it_subs != m_mapSubsKeys.end(); ++it_subs)
		{
			if (keyMatch(strInKey, it_subs->first))
			{
				//������ֵ�����ظ��ϲ㣬value����Ϊ0��ʾ����del����get rpopʧ��
				if (strcmp(strInKeyOp.c_str(), "set") == 0)
				{
					if (sendCmd(getCmd, sRlt))
						it_subs->second(old_key, sRlt);
				}
				else if (strcmp(strInKeyOp.c_str(), "del") == 0)
					it_subs->second(old_key, "");
			}
		}
	}
	m_subsLock.unlock();
	m_pullLock.lock();
	if (m_mapPullKeys.size() > 0)		//��ȡ
	{
		mapPullCB::iterator it_pull = m_mapPullKeys.begin();
		for (; it_pull != m_mapPullKeys.end(); ++it_pull)
		{
			//������ֵ�����ظ��ϲ㣬value����Ϊ0��ʾ����del����get rpopʧ��
			if (keyMatch(std::string(strInKey), it_pull->first))
			{
				if (strcmp(strInKeyOp.c_str(), "lpush") == 0)
				{
					if (sendCmd(popCmd, sRlt))
						it_pull->second(old_key, sRlt);
				}
			}
			else if (strcmp(strInKeyOp.c_str(), "del") == 0)
				it_pull->second(old_key, "");
		}
	}
	m_pullLock.unlock();
	m_reqLock.lock();
	if (this->m_mapReqChnl.size() > 0)	//�������
	{ 
		size_t s = m_mapReqChnl.size(); 
		mapReqCB::iterator it_req = m_mapReqChnl.begin();
		for (; it_req != m_mapReqChnl.end(); ++it_req)
		{
			if (keyMatch(std::string(strInKey), it_req->first))
			{
				if (strcmp(strInKeyOp.c_str(), "lpush") == 0)
				{
					std::string get_key = "";
					std::string get_value = "";
					if (sendCmd(popCmd, sRlt))		//��ȡ��������е�ֵ
					{
						get_key = sRlt;
						getCmd.clear();
						getCmd = R_GET + std::string(" ") + get_key;
						if (sendCmd(getCmd.c_str(), sRlt))		//��ȡ��Ӧ��ֵ
							get_value = sRlt;
						else
							WARNLOG("get value failed... getCmd = " << getCmd.c_str()
								<< ", errstr = " << sRlt.c_str());
					}
					else
						WARNLOG("��ȡ���������Ϣʧ��... popCmd = " << getCmd.c_str()
							<< ", errstr = " << sRlt.c_str());
					it_req->second(getOldKey(get_key), get_value);
				}
			}
		}
	}
	m_reqLock.unlock();
}

