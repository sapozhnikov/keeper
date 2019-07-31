#include "stdafx.h"
#include <codecvt>
#include <regex>
#include "GlobalUtils.h"
#include "ConsoleLogger.h"

using namespace boost::posix_time;
using namespace ConsoleLogger;

namespace keeper
{
	static const ptime myEpoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));

	int64_t ConvertPtimeToMillisec(boost::posix_time::ptime time)
	{
		time_duration myTimeFromEpoch = time - myEpoch;
		int64_t myTimeAsInt = myTimeFromEpoch.total_milliseconds();

		return myTimeAsInt;
	}

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

	Dbt WstringToDbt(const std::wstring & str) //is it safe to pass???
	{
		Dbt dbt((void*)str.c_str(), int(sizeof(wchar_t)*str.length())); //dirty hack
		dbt.set_flags(DB_DBT_READONLY);
		return dbt;
	}

	void keeper::CheckDbResult(int result)
	{
		if ((result == 0) || (result == DB_NOTFOUND))
			return;
		LOG_FATAL() << "DB error: " << result << std::endl;
		throw std::exception();
	}

	std::string keeper::PasswordToKey(const std::string & password)
	{
		byte key[crypto_box_SEEDBYTES]; //32
		byte salt[crypto_pwhash_SALTBYTES] = { 0xBE,0x28,0x74,0x25,0x7E,0xDA,0x9F,0x3C,0x1C,0x19,0xBE,0x38,0x0A,0x3A,0x2C,0x7B }; //16
		
		if (crypto_pwhash(key, crypto_box_SEEDBYTES, password.c_str(), password.length(), salt,
			crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
			crypto_pwhash_ALG_DEFAULT) != 0)
		{
			LOG_FATAL() << "Can't derive key from password" << std::endl;
			throw;
		}
		std::ostringstream s;
		//s.fill('0');
		s << std::setfill('0') << std::hex;
		//s.width(2);
		for (int i = 0; i < crypto_box_SEEDBYTES; i++)
			s << std::setw(2) << int(key[i]);

		return s.str();
	}

	std::string keeper::StrAnsiToOEM(const std::string& str)
	{
		std::unique_ptr<char[]> buff = std::make_unique<char[]>(str.length() + 1);
		CharToOemBuffA(str.c_str(), buff.get(), DWORD(str.length() + 1));
		std::string result(buff.get());
		return result;
	}
}