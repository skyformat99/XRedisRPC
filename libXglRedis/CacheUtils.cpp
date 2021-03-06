#include "RedisUtils.h"
#include "CacheUtils.h"


CCacheUtils::CCacheUtils(const std::string & strClientId)
{
	m_redisUtil = new CRedis_Utils(strClientId);
	if (g_ECGLogger == nullptr)
		g_ECGLogger = CMyLogger::getInstance();
	//if (g_pLogger == NULL)
	//	InitialLog();
}


std::shared_ptr<CCacheUtils> CCacheUtils::createInstance(const std::string &strClientId)
{
	return shared_ptr<CCacheUtils>(new CCacheUtils(strClientId));
}

CCacheUtils::~CCacheUtils()
{
	if (m_redisUtil != nullptr)
	{
		delete (CRedis_Utils *)m_redisUtil;
		m_redisUtil = nullptr;
	}
}

bool CCacheUtils::connect(const std::string & strIp, int iPort, bool bNeedSubs /*= false*/)
{
	return ((CRedis_Utils *)m_redisUtil)->connect(strIp, iPort, bNeedSubs);
}

void CCacheUtils::disconnect()
{
	((CRedis_Utils *)m_redisUtil)->disconnect();
}

int CCacheUtils::get(const std::string & strInKey, std::string & strOutResult)
{
	return ((CRedis_Utils *)m_redisUtil)->get(strInKey, strOutResult);
}

int CCacheUtils::set(const std::string & strInKey, const std::string & strInValue, std::string & strOutResult)
{
	return ((CRedis_Utils *)m_redisUtil)->set(strInKey, strInValue, strOutResult);
}

int CCacheUtils::push(const std::string & strInListName, const std::string & strInValue, std::string & strOutResult)
{
	return ((CRedis_Utils *)m_redisUtil)->push(strInListName, strInValue, strOutResult);
}

int CCacheUtils::pop(const std::string & strInListName, std::string & strOutResult)
{
	return ((CRedis_Utils *)m_redisUtil)->pop(strInListName, strOutResult);
}

int CCacheUtils::subs(const std::string & strInKey, subsCallback cb)
{
	return ((CRedis_Utils *)m_redisUtil)->subs(strInKey, cb);
}

bool CCacheUtils::unsubs(const std::string & strInKey)
{
	return ((CRedis_Utils *)m_redisUtil)->unsubs(strInKey);
}

int CCacheUtils::pull(const std::string & strInKey, pullCallback cb)
{
	return ((CRedis_Utils *)m_redisUtil)->pull(strInKey, cb);
}

bool CCacheUtils::unpull(const std::string & strInKey)
{
	return ((CRedis_Utils *)m_redisUtil)->unpull(strInKey);
}

int CCacheUtils::subsClientGetOp(const std::string & strInKey, clientOpCallBack cb)
{
	return ((CRedis_Utils *)m_redisUtil)->subsClientGetOp(strInKey, cb);
}

bool CCacheUtils::unsubClientGetOp(const std::string & strInKey)
{
	return ((CRedis_Utils *)m_redisUtil)->unsubClientGetOp(strInKey);
}

void CCacheUtils::stopSubClientGetOp()
{
	((CRedis_Utils *)m_redisUtil)->stopSubClientGetOp();
}

int CCacheUtils::notifyRlt(const std::string & strInKey, const std::string & strInValue)
{
	return ((CRedis_Utils *)m_redisUtil)->notifyRlt(strInKey, strInValue);
}

void CCacheUtils::log(const int iLogType, const std::string & strLog)
{
	switch (iLogType)
	{
	case LOG_INFO:
		_INFOLOG(strLog.c_str());
		break;
	case LOG_DEBUG:
		_DEBUGLOG(strLog.c_str());
		break;
	case LOG_WARN:
		_WARNLOG(strLog.c_str());
		break;
	case LOG_ERROR:
		_ERRORLOG(strLog.c_str());
		break;
	default:
		_DEBUGLOG(strLog.c_str());
		break;
	}
}

void CCacheUtils::log0(const int iLogType, const char * lpFormatText, ...)
{
	if (g_ECGLogger == nullptr)
		g_ECGLogger = CMyLogger::getInstance();
	va_list argList;
	va_start(argList, lpFormatText);

	switch (iLogType)
	{
	case LOG_INFO:
		MY_Log_Variable_Num(LOGL_INFOR, __FUNCTION__, lpFormatText, argList);
		break;
	case LOG_DEBUG:
		MY_Log_Variable_Num(LOGL_DEBUG, __FUNCTION__, lpFormatText, argList);
		break;
	case LOG_WARN:
		MY_Log_Variable_Num(LOGL_WARN, __FUNCTION__, lpFormatText, argList);
		break;
	case LOG_ERROR:
		MY_Log_Variable_Num(LOGL_ERROR, __FUNCTION__, lpFormatText, argList);
		break;
	default:
		MY_Log_Variable_Num(LOGL_DEBUG, __FUNCTION__, lpFormatText, argList);
		break;
	}
	va_end(argList);
}

