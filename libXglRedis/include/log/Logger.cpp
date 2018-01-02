#include "Logger.h"
#include <string.h>
#include <chrono>
#include <thread>
#include <iostream>


CLogger::CLogger( )
{
	m_iSize = sizeof( this->m_buf );

	m_uiLogLevel = LogLevel_ALL;
	m_bShowLog = true;
	m_ospOutput = NULL;
}


CLogger::~CLogger( )
{
	if ( NULL != m_ospOutput )
	{
		delete m_ospOutput;
		m_ospOutput = NULL;
	}
}


// �����ļ���������ļ���Ϊ�գ����������Ļ��Ĭ������������Ļ��
bool CLogger::SetFileName( const char * lpFileName )
{
	auto locker = this->_LockAsUnique( );


	if ( NULL != m_ospOutput )
	{
		delete m_ospOutput;
		m_ospOutput = NULL;
	}


	if ( NULL != lpFileName && 0 != lpFileName [ 0 ] )
	{
		// ������ļ�
		try
		{
			m_ospOutput = new std::ofstream( lpFileName , std::ios::binary | std::ios::app | std::ios::out );
		}
		catch ( ... )
		{
			m_ospOutput = NULL;
		}
		if ( NULL == m_ospOutput )
		{
			// ���ļ�ʧ��
			return false;
		}
		if ( m_ospOutput && !m_ospOutput->is_open( ) )
		{
			delete m_ospOutput;
			m_ospOutput = NULL;
			// ���ļ�ʧ��
			return false;
		}
	}
	// �����ǽ������������Ļ
	return true;
}


void CLogger::Log0( LogLevel lvl , const char * lpModuleName , const char * lpText )
{
	if ( !_LogLvlAvailable( lvl ) )
		return;

	auto locker = this->_LockAsUnique( );

	std::ostream & outStream = m_ospOutput ? *m_ospOutput : std::cout;

	_PrintfTimer( lvl , lpModuleName , outStream );
	int len = 0;
	if ( lpText && lpText [ 0 ] )
	{
		outStream << lpText;
		len = ( int ) strlen( lpText );
	}
	if ( len == 0 || lpText [ len - 1 ] != '\n' )
	{
		outStream << '\n' ;
	}
}

void CLogger::Log0( LogLevel lvl , const char * lpModuleName , const std::string & strText )
{
	if ( !_LogLvlAvailable( lvl ) )
		return;

	auto locker = this->_LockAsUnique( );

	std::ostream & outStream = m_ospOutput ? *m_ospOutput : std::cout;

	_PrintfTimer( lvl , lpModuleName , outStream );
	if ( !strText.empty( ) )
	{
		outStream << strText;
	}
	if ( strText.empty( ) || strText [ strText.size( ) - 1 ] != '\n' )
	{
		outStream << '\n';
	}
}

void CLogger::Log( LogLevel lvl , const char * lpModuleName , const char * lpFormatText , ... )
{
	if ( !_LogLvlAvailable( lvl ) )
		return;

	auto locker = this->_LockAsUnique( );

	std::ostream & outStream = m_ospOutput ? *m_ospOutput : std::cout;

	_PrintfTimer( lvl , lpModuleName , outStream );

	if ( lpFormatText && lpFormatText [ 0 ] )
	{
		int len = 0;

		va_list argList;
		va_start( argList , lpFormatText );
		len = vsnprintf( m_buf , m_iSize , lpFormatText , argList );
		va_end( argList );

		outStream << m_buf;		
		if ( len == 0 || lpFormatText [ len - 1 ] != '\n' )
		{
			outStream << '\n';
		}
	}	
	else
	{
		outStream << '\n';
	}
}

//   (�Կɱ�����Ĵ���)�и�ʽ�������Ч�ʽϵ͡�
void CLogger::Log( LogLevel lvl , const char * lpModuleName , const char * lpFormatText , va_list & vaList )
{
	if ( !_LogLvlAvailable( lvl ) )
		return;

	auto locker = this->_LockAsUnique( );

	std::ostream & outStream = m_ospOutput ? *m_ospOutput : std::cout;

	_PrintfTimer( lvl , lpModuleName , outStream );

	if ( lpFormatText && lpFormatText [ 0 ] )
	{
		int len = 0;

		len = vsnprintf( m_buf , m_iSize , lpFormatText , vaList );

		outStream << m_buf;
		if ( len == 0 || lpFormatText [ len - 1 ] != '\n' )
		{
			outStream << '\n';
		}
	}
	else
	{
		outStream << '\n';
	}
}


// �ر���־���
void CLogger::LogOff( )
{
	m_bShowLog = false;
}
// ������־���
void CLogger::LogOn( )
{
	m_bShowLog = true;
}
// ��־�Ƿ���
bool CLogger::IsLogOn( ) const
{
	return m_bShowLog;
}
// ��ȡ��ǰ��־����
unsigned int CLogger::GetCurrentLogLevel( )const
{
	return m_uiLogLevel;
}
// ������־����
void CLogger::SetCurrentLogLevel( unsigned int uiLogLevel )
{
	m_uiLogLevel = uiLogLevel;
}



void CLogger::_PrintfTimer( LogLevel lvl , const char * lpModuleName , std::ostream & outputStream )
{
	auto curID = std::this_thread::get_id( );

	
	auto nowTime = std::chrono::system_clock::now( );
	auto t = std::chrono::system_clock::to_time_t( nowTime );
	auto loTime = localtime( &t );

	
	snprintf( m_buf , m_iSize , "[%04d-%02d-%02d %02d:%02d:%02d]" ,
		loTime->tm_year + 1900 ,
		loTime->tm_mon + 1,
		loTime->tm_mday ,

		loTime->tm_hour ,
		loTime->tm_min ,
		loTime->tm_sec
	);

	outputStream << m_buf;

	outputStream << "[TID:" << curID << "]";

	switch ( lvl )
	{
	case LOGL_INFOR:
		outputStream << "[Lv:Inform] " ;
		break;
	case LOGL_DEBUG:
		outputStream << "[Lv:Debug] ";
		break;
	case LOGL_WARN:
		outputStream << "[Lv:Warning] ";
		break;
	case LOGL_ERROR:
		outputStream << "[Lv:Error] ";
		break;
	default:
		outputStream << "[Lv:unknow] ";
		break;
	}

	outputStream << '[';
	if ( lpModuleName && lpModuleName [ 0 ] )
	{
		outputStream << lpModuleName;
	}
	outputStream << "] ";
}


std::unique_lock<std::mutex> CLogger::_LockAsUnique( )
{
	std::unique_lock<std::mutex> locker( this->m_mtx );
	return locker;
}


// �Ƿ����������־��Ϣ
bool CLogger::_LogLvlAvailable( LogLevel lvl )
{
	if ( !m_bShowLog )
		return false;

	if ( (( unsigned int ) lvl & m_uiLogLevel) == 0 )
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////

CLogger * g_pLogger = NULL;


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 




// ��ʼ����־
void InitialLog( const char * lpFileName , unsigned int lvl )
{
	if ( NULL != g_pLogger )
	{
		// ҪôΪNULL��Ҫô�ַ��������ݡ�
		if ( NULL == lpFileName || 0 != lpFileName [ 0 ] )
			g_pLogger->SetFileName( lpFileName );

		g_pLogger->SetCurrentLogLevel( lvl );

		return;
	}
		

	g_pLogger = new CLogger;
	g_pLogger->SetFileName( lpFileName );
	g_pLogger->SetCurrentLogLevel( lvl );

	g_pLogger->Log0( LOGL_INFOR , "MAIN", "" );
	g_pLogger->Log0( LOGL_INFOR , "MAIN" , "" );
	g_pLogger->Log0( LOGL_INFOR , "MAIN" , "           START" );
	g_pLogger->Log0( LOGL_INFOR , "MAIN" , "++-------------------------++" );
}


// ɾ����־
void DeleteAndCloseLog( )
{
	if ( NULL != g_pLogger )
	{
		g_pLogger->Log0( LOGL_INFOR , "MAIN" , "" );
		g_pLogger->Log0( LOGL_INFOR , "MAIN" , "==-------------------------==" );
		g_pLogger->Log0( LOGL_INFOR , "MAIN" , "           END." );
		g_pLogger->Log0( LOGL_INFOR , "MAIN" , "" );


		delete g_pLogger;
		g_pLogger = NULL;
	}
}

// ��ʱ�򿪡��ر���־
void LogOnOff( bool bOn )
{
	if ( NULL == g_pLogger )
		return;

	if ( bOn )
		g_pLogger->LogOn( );
	else
		g_pLogger->LogOff( );
}

// �ж���־��ǰ�Ƿ���
bool IsLogOn( )
{
	if ( NULL == g_pLogger )
		return false;

	return g_pLogger->IsLogOn( );
}

void MY_Log0( LogLevel lvl , const char * lpModuleName , const char * lpText )
{
	if ( NULL == g_pLogger )
		return;
	g_pLogger->Log0( lvl , lpModuleName , lpText );
}

void  MY_Log0( LogLevel lvl , const char * lpModuleName , const std::string &  strText )
{
	if ( NULL == g_pLogger )
		return;
	g_pLogger->Log0( lvl , lpModuleName , strText );
}

void MY_Log( LogLevel lvl , const char *lpModuleName , const char * lpFormatText , ... )
{
	if ( NULL == g_pLogger )
		return;

	va_list argList;
	va_start( argList , lpFormatText );

	g_pLogger->Log( lvl , lpModuleName , lpFormatText , argList );

	va_end( argList );
}

// �������ṩ�ɱ������װ�ĺ�����
void MY_Log_Variable_Num( LogLevel lvl , const char *lpModuleName , const char * lpFormatText , va_list & vaList )
{
	if ( NULL == g_pLogger )
		return;
	
	g_pLogger->Log( lvl , lpModuleName , lpFormatText , vaList );
}
