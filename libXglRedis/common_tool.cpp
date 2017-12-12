#include "common_tool.h"

CMyLogger *g_ECGLogger;// = CMyLogger::getInstance();

std::string time2str ( time_t time )
{
	tm* local;
	char time_str[ 128 ];

	local = localtime ( &time );
	memset ( time_str , 0 , 128 );
	strftime ( time_str , 64 , "%Y-%m-%d %H:%M:%S" , local );
	return time_str;
}

template<class T>
std::string num2str ( const T num )
{
	stringstream ss;
	std::string str;
	ss << num;
	ss >> str;
	return str;
}

std::string int2str ( const int &int_temp )
{
	std::string str;
	stringstream st;
	st << int_temp;
	st >> str;
	return str;
}

//string long2str ( const unsigned __int64 &double_temp )
//{
//	stringstream ss;
//	string str;
//	ss << double_temp;
//	ss >> str;
//	return str;
//}

std::string double2str ( const double &double_temp )
{
	stringstream ss;
	std::string str;
	ss << double_temp;
	ss >> str;
	return str;
}

std::vector<std::string> split(std::string str, std::string pattern)
{
	std::string::size_type pos;
	std::vector<std::string> result;
	str += pattern;				//��չ�ַ����Է������
	int size = str.size();

	for (int i = 0; i < size; i++)
	{
		pos = str.find(pattern, i);
		if (pos < size)
		{
			std::string s = str.substr(i, pos - i);
			result.push_back(s);
			i = pos + pattern.size() - 1;
		}
	}
	return result;
}

// �ļ���ƥ��
// ���ַ����ﱣ�����ļ���, ��������ƥ��ģ��, ƥ��ɹ����� true, ���򷵻� false
// ƥ��ʱע��������ת����Сд, ��ƥ�亯�������Զ���Сдת��
bool keyMatch(const std::string srcStr, const std::string & strPattern)
{
	if (srcStr.empty())
	{
		return false;
	}
	if (strPattern.empty())
	{
		return true;
	}
	//�ļ�����ͨ���ƥ��
	const char * ft = strPattern.c_str();
	const char * f = srcStr.c_str();
	const char * p1, *p2;

	size_t lt = (size_t)strPattern.length();
	size_t lf = (size_t)srcStr.length();

	//����ƥ��Ҫ�õ�����Ҫ��¼�û���û������ * ��
	//���� * �˱���ֵΪ 1
	//û�����˱���ֵΪ 0
	int iMeetXing = 0;

	//����ƥ��Ҫ�õ���Ҫ��¼�û��������ٸ�? ��
	int iQuestionNumber = 0;

	// ��ģ��ƥ���� * ��ʱ, ��һ����������, �� A.1.DOC, �� *.DOC ƥ��, .1.DOC �� .DOC ƥ�䲻�ɹ�, ���Ǻ���� .DOC Ӧ��ƥ��ɹ���, ������������־
	bool bTempReset = false;


	if (lf == 0)
	{
		return false;
	}

	p1 = ft; //ģ���ַ�
	p2 = f; //�ļ����ַ�

	while (lf > 0 && lt > 0)
	{
		if (*p1 != *p2)
		{ //���߲���
			if (*p1 != '*'  &&  *p1 != '?')
			{
				if (iMeetXing > 0)
				{
					if (!bTempReset)
					{
						--p1;
						++lt;
						bTempReset = true;
					}
					else
					{
						++p2;
						--lf;
					}
					continue;
				}
				return false;
			}

			iMeetXing = 0;
			iQuestionNumber = 0;

			while (*p1 == '*' || *p1 == '?')
			{
				--lt;
				if (*p1 == '*')
				{
					iMeetXing = 1;
				}
				else
				{
					++iQuestionNumber;
				}
				++p1;

				if (lt == 0)
				{ //˵��ģ����漸���ַ�ȫ����ͨ���
					if (iQuestionNumber != 0 && iQuestionNumber != (int)lf)
					{
						return false;
					}
					return true;
				}
			}
			//��������*�ź�?�ŵ����

			//������������*�ţ�ֱ��ƥ���ļ�����һ���ַ�
			if (iMeetXing > 0)
			{
				//����*�ŵ�ͬʱ��Ҳ�������� ? �ţ�ÿ����һ�� ? �ţ�Ҫ���ļ����ַ�����һ��
				if ((int)lf <= iQuestionNumber)
				{
					return false;
				}
				lf -= iQuestionNumber;
				p2 += iQuestionNumber;

				do
				{
					if (*p2 == *p1)
					{
						++p2;
						++p1;

						--lt;
						--lf;
						bTempReset = false;
						break;
					}
					++p2;
					--lf;
				} while (lf > 0);
				continue;
			}
			else
			{ //û������ * �ţ� ���������� ? ��
				if ((int)lf <= iQuestionNumber)
				{
					return false;
				}
				lf -= iQuestionNumber;
				p2 += iQuestionNumber;
				continue;
			}
		}//if( *p1 != *p2 )

		 //�������ʾ���
		bTempReset = false;
		++p1; //�Ƚ���һ���ַ�
		++p2;

		--lt;
		--lf;
	}

	if (lt == 0 && lf == 0)
	{
		return true;
	}
	else if (lt > 0)
	{ //���ģ�廹��ʣ�࣬��ʣ������ݱ���ȫ���� * ��
		while (lt > 0)
		{
			if (*p1 != '*')
			{
				return false;
			}
			--lt;
			++p1;
		}
		return true;

	}
	return false;
}

std::string generate_uuid()
{
	char buf[UUID4_LEN];
	uuid4_generate(buf);
	
	string uuidStr = std::string(buf);
	uuidStr.erase(remove(uuidStr.begin(), uuidStr.end(), '-'), uuidStr.end());
	return uuidStr;
}
