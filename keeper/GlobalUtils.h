#pragma once
//#include <boost\date_time\posix_time\posix_time.hpp>

namespace keeper
{
	int64_t ConvertPtimeToMillisec(boost::posix_time::ptime time);
	boost::posix_time::ptime ConvertMillisecToPtime(int64_t millisec);
	std::string WstringToUTF8(const std::wstring& str);
	std::wstring StringToWstring(const std::string&& str);

	//manual implenemtation of "to_iso_wstring" to avoid different locales
	std::wstring PTimeToWstringSafeSymbols(boost::posix_time::ptime time);
	std::wstring PTimeToWstring(boost::posix_time::ptime time);
	std::wstring SystemTimeToWstring(const SYSTEMTIME& utc);
	boost::posix_time::time_duration StringToDuration(const std::string& str);

	std::wstring DbtToWstring(const Dbt& dbt);
	Dbt WstringToDbt(const std::wstring& str);

	void CheckDbResult(int result);

	std::string PasswordToKey(const std::string& password);
}