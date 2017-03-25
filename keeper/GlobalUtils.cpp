#include "stdafx.h"
#include <codecvt>
#include <regex>
#include "GlobalUtils.h"
#include "ConsoleLogger.h"

//#include <Windows.h> //include it AFTER berkley stuff

using namespace boost::posix_time;
using namespace ConsoleLogger;

namespace keeper
{
	static const ptime myEpoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));

	//int64_t ConvertPtimeToMicrosec(boost::posix_time::ptime time)
	//{
	//	time_duration myTimeFromEpoch = time - myEpoch;
	//	int64_t myTimeAsInt = myTimeFromEpoch.total_microseconds();

	//	return myTimeAsInt;
	//}

	int64_t ConvertPtimeToMillisec(boost::posix_time::ptime time)
	{
		time_duration myTimeFromEpoch = time - myEpoch;
		int64_t myTimeAsInt = myTimeFromEpoch.total_milliseconds();

		return myTimeAsInt;
	}

	//boost::posix_time::ptime ConvertMicrosecToPtime(int64_t microsec)
	//{
	//	ptime result = myEpoch + boost::posix_time::microseconds(microsec);

	//	return result;
	//}

	boost::posix_time::ptime ConvertMillisecToPtime(int64_t millisec)
	{
		ptime result = myEpoch + boost::posix_time::milliseconds(millisec);

		return result;
	}

	std::string WstringToUTF8(const std::wstring& str)
	{
		using namespace std;
		wstring_convert<codecvt_utf8<wchar_t>> utf8_conv;
		string resultUTF8(utf8_conv.to_bytes(str));

		return resultUTF8;
	}

	std::wstring StringToWstring(const std::string && str)
	{
		std::wstring wresult(str.begin(), str.end());
		return wresult;
	}

	std::wstring PTimeToWstringSafeSymbols(boost::posix_time::ptime time)
	{
		boost::gregorian::date dt = time.date();
		auto timePart = time.time_of_day();
		std::wostringstream stm;
		const auto w2 = std::setw(2);
		stm << std::setfill(L'0') << std::setw(4) << dt.year() << L'-' << w2 << static_cast<int>(dt.month()) << L'-' << w2 << dt.day() << L'T'
			<< w2 << timePart.hours() << L'-' << w2 << timePart.minutes() << L'-' << w2 << timePart.seconds() << L'.' << std::setw(3) << int(timePart.fractional_seconds()/1000);

		return stm.str();
	}

	std::wstring PTimeToWstring(boost::posix_time::ptime time)
	{
		boost::gregorian::date dt = time.date();
		auto timePart = time.time_of_day();
		std::wostringstream stm;
		const auto w2 = std::setw(2);
		stm << std::setfill(L'0') << std::setw(4) << dt.year() << L'-' << w2 << static_cast<int>(dt.month()) << L'-' << w2 << dt.day() << L'T'
			<< w2 << timePart.hours() << L':' << w2 << timePart.minutes() << L':' << w2 << timePart.seconds() << L'.' << std::setw(3) << int(timePart.fractional_seconds() / 1000);

		return stm.str();
	}

	std::wstring SystemTimeToWstring(const SYSTEMTIME & utc)
	{
		std::wostringstream stm;
		const auto w2 = std::setw(2);
		stm << std::setfill(L'0') << std::setw(4) << utc.wYear << L'-' << w2 << utc.wMonth << L'-' << w2 << utc.wDay << L'T'
			<< w2 << utc.wHour << L':' << w2 << utc.wMinute << L':' << w2 << utc.wSecond << L'.' << std::setw(3) << utc.wMilliseconds;

		return stm.str();
	}

	long match_duration(const std::string& input, const std::regex& re)
	{
		std::smatch match;
		std::regex_search(input, match, re);
		if (match.empty())
			return 0; // Pattern don't match

		double durArgs[6] = { 0,0,0,0,0,0 }; // years, months, days, hours, minutes, seconds

		std::string matchStr;
		for (size_t i = 0; (i < match.size()) && (i < 6); i++)
		{
			if (match[i + 1].matched)
			{
				matchStr = match[i + 1];
				matchStr.pop_back(); // remove last character.
				durArgs[i] = std::stod(matchStr);
			}
		}

		long duration = long(
			60 * 60 * 24 * 365.25 * durArgs[0] +  // years  
			60 * 60 * 24 * 31 * durArgs[1] +  // months
			60 * 60 * 24 * durArgs[2] +  // days
			60 * 60 * durArgs[3] +  // hours
			60 * durArgs[4] +  // minutes
			durArgs[5]);   // seconds

		return duration;
	}

	boost::posix_time::time_duration StringToDuration(const std::string & str)
	{
		time_duration result = not_a_date_time;

		if (str.empty())
			return result;

		long secs;
		std::regex rshort("^((?!T).)*$");
		if (std::regex_match(str, rshort)) // no T (Time) exist
		{
			std::regex r("P([[:d:]]+Y)?([[:d:]]+M)?([[:d:]]+D)?");
			secs = match_duration(str, r);
		}
		else
		{
			std::regex r("P([[:d:]]+Y)?([[:d:]]+M)?([[:d:]]+D)?T([[:d:]]+H)?([[:d:]]+M)?([[:d:]]+S|[[:d:]]+\\.[[:d:]]+S)?");
			secs = match_duration(str, r);
		}

		if (secs <= 0)
			return result;
		else
			result = seconds(secs);

		return result;
	}

	std::wstring DbtToWstring(const Dbt & dbt)
	{
		return std::wstring(static_cast<wchar_t*>(dbt.get_data()), dbt.get_size() / sizeof(wchar_t));
	}

	Dbt WstringToDbt(const std::wstring & str) //is it safe to return???
	{
		Dbt dbt((void*)str.c_str(), sizeof(wchar_t)*str.length()); //dirty hack
		return dbt;
	}

	void keeper::CheckDbResult(int result)
	{
		if ((result == 0) || (result == DB_NOTFOUND))
			return;
		LOG_FATAL() << "DB error: " << result << std::endl;
		throw std::exception();
	}
}