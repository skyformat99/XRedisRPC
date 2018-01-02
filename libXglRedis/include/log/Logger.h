#ifndef _LOGGER__
#define _LOGGER__
#include <mutex>
#include <fstream>
#include <stdarg.h>
#include <string>



enum LogLevel
{
	LOGL_INFOR = 1,    // ��ͨ��Ϣ������̫��
	LOGL_DEBUG = 2 ,   // ������Ϣ�����԰���ϸ�����������
	LOGL_WARN  = 4 ,   // ������Ϣ������ȷ�������Ƿ���ȷ��������������������Ӳ��ϡ�
	LOGL_ERROR = 8,    // ������ȷ�ϳ���Ż���롣������μ�ⲻ�ԣ������ڴ治�㣬����ϵͳӦ�þ߱��ķ����޷����õȵȡ�
};

#define LogLevel_ALL (0xFFFFFFFF)


class CLogger
{
public:

	CLogger( );


	// �����ļ���������ļ���Ϊ�գ����������Ļ��Ĭ������������Ļ��
	bool SetFileName( const char * lpFileName );

	
	~CLogger( );

	// �޸�ʽ����ֱ�����һ����־��Ч�ʽϸߡ�
	void Log0( LogLevel lvl , const char * lpModuleName , const char * lpText );
	void Log0( LogLevel lvl , const char * lpModuleName , const std::string & strText );
	// �и�ʽ�������Ч�ʽϵ͡�
	void Log( LogLevel lvl , const char * lpModuleName , const char * lpFormatText , ... );
	//   (�Կɱ�����Ĵ���)�и�ʽ�������Ч�ʽϵ͡�
	void Log( LogLevel lvl , const char * lpModuleName , const char * lpFormatText , va_list & vaList );

	// �ر���־���
	void LogOff( );
	// ������־���
	void LogOn( );
	// ��־�Ƿ���
	bool IsLogOn( ) const;

	// ��ȡ��ǰ��־����
	unsigned int GetCurrentLogLevel( )const;
	// ������־����
	void SetCurrentLogLevel( unsigned int uiLogLevel );


	
protected:	



	std::unique_lock<std::mutex> _LockAsUnique( );
	// �Ƿ����������־��Ϣ
	bool _LogLvlAvailable( LogLevel lvl );
	// ��ӡ��ǰʱ�䣬�Լ��߳�ID
	void _PrintfTimer( LogLevel lvl , const char * lpModuleName , std::ostream & outputStream );

	char m_buf [ 1536 ];
	int m_iSize;

	// ��־���𿪹�
	unsigned int m_uiLogLevel;
	// �Ƿ���־
	bool m_bShowLog;
	// ������
	std::mutex m_mtx;

	// �����־�ļ�
	std::ofstream * m_ospOutput;

	// ��־������
	static CLogger * m_pInstance;
};


extern CLogger * g_pLogger;


//////////////////////////////////////////////////////////////////////////



// ��ʼ����־�������޸���־������
// ���������lpFileName �� NULL-> �������Ļ���� "" ���ַ���������ԭ��������䣨�����ǰû��Initial����Ĭ���������Ļ�����������Ч�ַ���->��־��������ַ����������ļ���
// lvl ���Ըı䵱ǰ��־ϵͳ����־������־������һ�����򡱵Ĺ�ϵ��ֻ�е���Ӧ����������Ӧ��־�Żᱻ�����
void InitialLog( const char * lpFileName = "", unsigned int lvl = ( unsigned int )LogLevel_ALL );

// ɾ�����ر���־
void DeleteAndCloseLog( );

// ��ʱ�򿪡��ر���־
void LogOnOff( bool bOn );

// �ж���־��ǰ�Ƿ���
bool IsLogOn( );

// ȫ����־��������
// �޸�ʽ����ֱ�����һ����־��Ч�ʽϸߡ�
void  MY_Log0( LogLevel lvl , const char * lpModuleName , const char * lpText );
void  MY_Log0( LogLevel lvl , const char * lpModuleName , const std::string &  strText );

// �и�ʽ�������Ч�ʽϵ͡�
void MY_Log( LogLevel lvl , const char *lpModuleName , const char * lpFormatText , ... );
	
// �������ṩ�ɱ������װ�ĺ�����
void MY_Log_Variable_Num( LogLevel lvl , const char *lpModuleName , const char * lpFormatText , va_list & vaList );

#endif




