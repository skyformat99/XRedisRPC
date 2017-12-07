// XglRedisTest.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "Redis_Utils.h"
#pragma comment(lib, "libXglRedis.lib")

void test_get(CRedis_Utils& redis);
void test_set(CRedis_Utils& redis);
void test_push(CRedis_Utils& redis);
void test_pop(CRedis_Utils& redis);

void subs_test();
void pull_test();
void subCBA(const char *key, const char *value);
void subCBB(const char *key, const char *value);
void subCBC(const char *key, const char *value);
void pullCBA(const char *key, const char *value);
void pullCBB(const char *key, const char *value);
void pullCBC(const char *key, const char *value);

void subGetOp();
void getCBA(const char *key, const char *value);
void getCBB(const char *key, const char *value);

#define THREADNUM 10
void abnormalTest(CRedis_Utils& redis);


static void* multiThread(void *args);

int main(int argc, char const *argv[])
{
	CRedis_Utils redis("A");
	redis.connect("192.168.31.217", 6379);
	//������������
	//test_set(redis);
	//test_get(redis);
	//test_push(redis);
	//test_pop(redis);

	//���ķ�������
	//subs_test();
	//pull_test();
	//subGetOp();

	//abnormalTest(redis);

	//���̲߳���
	vector<thread> thGroup;
	for (int i = 0; i < THREADNUM; ++i)
		thGroup.push_back(thread(multiThread, &redis));
	for (int i = 0; i < THREADNUM; ++i)
		thGroup[i].join();
	getchar();
	return 0;
}

void test_set(CRedis_Utils& redis)
{
	char *msg = (char *)malloc(REDIS_BUF_SIZE);
	for (int i = 0; i < 10; ++i)
	{
		std::string key_ = "hello" + int2str(i);
		std::string value = "world" + int2str(i);
		memset(msg, 0, REDIS_BUF_SIZE);
		if (redis.set(key_.c_str(), value.c_str(), msg))	
			DEBUGLOG << "set op succ!!! msg = " << msg << endl;
		else
			ERRORLOG << "set op fail!!! err = " << msg << endl;
	}
	free(msg);
}
void test_get(CRedis_Utils& redis)
{
	char *msg = (char *)malloc(REDIS_BUF_SIZE);
	for (int i = 0; i < 1; ++i)
	{
		std::string key_ = "helloasd" + int2str(i);
		memset(msg, 0, REDIS_BUF_SIZE);
		int size = redis.get(key_.c_str(), msg);
		if (size > 0)
			DEBUGLOG << "set op succ!!! msg = " << msg << endl;
		else if (size == 0)
			DEBUGLOG << "no data... key = " << key_ << endl;
		else
			ERRORLOG << "get op fail!!! err = " << msg << endl;
	}
	free(msg);
}
void test_push(CRedis_Utils& redis)
{
	char *msg = (char *)malloc(REDIS_BUF_SIZE);
	for (int i = 0; i < 10; ++i)
	{
		std::string key_ = "hello";
		std::string value = "world" + int2str(i);
		memset(msg, 0, REDIS_BUF_SIZE);
		if (redis.push(key_.c_str(), value.c_str(), msg))
			DEBUGLOG << "push op succ!!! size = " << msg << endl;
		else
			ERRORLOG << "push op fail!!! err = " << msg << endl;
	}
	free(msg);
}
void test_pop(CRedis_Utils& redis)
{
	char *msg = (char *)malloc(REDIS_BUF_SIZE);
	for (int i = 0; i < 11; ++i)
	{
		std::string key_ = "hello";
		memset(msg, 0, REDIS_BUF_SIZE);
		int size = redis.pop(key_.c_str(), msg);
		if (size > 0)
			DEBUGLOG << "pop op succ!!! size = " << msg << endl;
		else if (size == 0)
			DEBUGLOG << "no data... key = " << key_ << endl;
		else
			ERRORLOG << "pop op fail!!! err = " << msg << endl;
	}
	free(msg);
}

void subs_test()
{
	CRedis_Utils redisA("A");
	CRedis_Utils redisB("B");
	CRedis_Utils redisC("C");
	redisA.connect("192.168.31.217", 6379, true);
	redisB.connect("192.168.31.217", 6379, true);
	redisC.connect("192.168.31.217", 6379, true);

	redisA.subs("hello*", subCBA);
	//redisB.subs("hello*", subCBB);
	//redisC.subs("hello*", subCBC);

	//ֻ�пͻ���A�յ��ص�
	redisA.set("helloA", "worldA", nullptr);
	redisA.unsubs("hello*");		//ȡ����ȡlhello*
	redisA.set("helloBCD", "worldABCD", nullptr);	
	//ֻ���յ��ַ������͵Ļص�
	redisA.push("hellol", "1234", nullptr);

	//ֻ�пͻ���B�յ��ص�
	redisB.set("helloA", "worldA", nullptr);
	redisB.set("helloBCD", "worldABCD", nullptr);

	//ֻ�пͻ���C�յ��ص�
	redisC.set("helloA", "worldA", nullptr);
	redisC.set("helloBCD", "worldABCD", nullptr);

	getchar();
}
void pull_test()
{
	CRedis_Utils redisA("A");
	CRedis_Utils redisB("B");
	CRedis_Utils redisC("C");
	redisA.connect("192.168.31.217", 6379, true);
	redisB.connect("192.168.31.217", 6379, true);
	redisC.connect("192.168.31.217", 6379, true);

	redisA.pull("lhello*", subCBA);
	redisB.pull("lhello*", subCBB);
	redisC.pull("lhello*", subCBC);
	
	//ֻ�пͻ���A�յ��ص�
	redisA.push("lhello123", "1", nullptr);
	redisA.unpull("lhello*");		//ȡ����ȡlhello*
	redisA.push("lhello456", "2", nullptr);
	//ֻ���յ�list���͵Ļص�
	redisA.set("lhello000", "stringtype", nullptr);
	
	//ֻ�пͻ���B�յ��ص�
	redisB.push("lhello789", "3", nullptr);
	redisB.push("lhello012", "4", nullptr);

	//ֻ�пͻ���C�յ��ص�
	redisC.push("lhello345", "5", nullptr);
	redisC.push("lhello678", "6", nullptr);
	getchar();
}
void subCBA(const char *key, const char *value)
{
	DEBUGLOG << "clientA got subs msg!!!";
	if (strlen(value) == 0)
		DEBUGLOG << "key = " << key << ", was deleted..." << endl;
	else
		DEBUGLOG << "got subcb msg, key = " << key << ", value = " << value << endl;
}
void subCBB(const char *key, const char *value)
{
	DEBUGLOG << "clientB got subs msg!!!";
	if (strlen(value) == 0)
		DEBUGLOG << "key = " << key << ", was deleted..." << endl;
	else
		DEBUGLOG << "got subcb msg, key = " << key << ", value = " << value << endl;
}
void subCBC(const char *key, const char *value)
{
	DEBUGLOG << "clientC got subs msg!!!";
	if (strlen(value) == 0)
		DEBUGLOG << "key = " << key << ", was deleted..." << endl;
	else
		DEBUGLOG << "got subcb msg, key = " << key << ", value = " << value << endl;
}
void pullCBA(const char *key, const char *value)
{
	DEBUGLOG << "clientA got pull msg!!!";
	if (strlen(value) == 0)
		DEBUGLOG << "key = " << key << ", was deleted..." << endl;
	else
		DEBUGLOG << "got pullcb msg, key = " << key << ", value = " << value << endl;
}
void pullCBB(const char *key, const char *value)
{
	DEBUGLOG << "clientB got pull msg!!!";
	if (strlen(value) == 0)
		DEBUGLOG << "key = " << key << ", was deleted..." << endl;
	else
		DEBUGLOG << "got pullcb msg, key = " << key << ", value = " << value << endl;
}
void pullCBC(const char *key, const char *value)
{
	DEBUGLOG << "clientC got pull msg!!!";
	if (strlen(value) == 0)
		DEBUGLOG << "key = " << key << ", was deleted..." << endl;
	else
		DEBUGLOG << "got pullcb msg, key = " << key << ", value = " << value << endl;
}

void subGetOp()
{
	CRedis_Utils redisA("A");
	CRedis_Utils redisB("B");
	redisA.connect("192.168.31.217", 6379, true);
	redisB.connect("192.168.31.217", 6379, true);
	char msg[256];
	memset(msg, 0, 256);

	redisA.subsClientGetOp("gethelloOp", "gethelloOpreq",
		"gethelloOphb", getCBA);	//ע��
	redisA.set("gethelloOphb", int2str(time(NULL)).c_str(), 
		msg);	//��ʱ��������

	memset(msg, 0, 256);
	redisA.get("gethelloOp", msg);
	//redisB.get("gethelloOp", msg);
	DEBUGLOG << "get result = " << msg;

	getchar();
}
void getCBA(const char *key, const char *value)
{
	DEBUGLOG << "clientA got get msg!!!";
	CRedis_Utils redis("A");
	redis.connect("192.168.31.217", 6379);
	char msg[256];
	memset(msg, 0, 256);
	//���ݴ����������֮�����set�ӿڸ�������
	//֪ͨ�ͻ��˴������
	if (strlen(value) == 0)
		redis.set(key, "nil", msg);
	else
	{
		std::string value_ = value + std::string("getCBA");
		
		redis.set(key, value_.c_str(), msg);
	}
}

void abnormalTest(CRedis_Utils& redis)
{
	redis.get("", nullptr);
	redis.set("", "", nullptr);
	redis.push("", "", nullptr);
	redis.pop("",  nullptr);

	redis.subs("", nullptr);
	redis.unsubs("");
	redis.pull("", nullptr);
	redis.unpull("");

	redis.subsClientGetOp("", "", "", nullptr);
	redis.unsubClientGetOp("");
}

void* multiThread(void *args)
{
	CRedis_Utils *redis = (CRedis_Utils *)args;
	stringstream ss;
	std::string str;
	ss << std::this_thread::get_id();
	ss >> str;
	std::string key = "hellolist" + str;
	char *msg = (char *)malloc(REDIS_BUF_SIZE);
	for (int i = 0; i < 10; ++i)
	{
		memset(msg, 0, REDIS_BUF_SIZE);
		if (redis->pop(key.c_str(), msg) >= 0)
			DEBUGLOG << "set op succ!!! msg = " << msg << endl;
		else
			ERRORLOG << "set op fail!!! err = " << msg << endl;
	}
	free(msg);
	return nullptr;
}
