#pragma once

// Simple console logger by Dmitriy Sapozhnikov
//
// Usage example:
//
// using namespace ConsoleLogger;
// SetLogLevel(LogLevel::info);
// LOG_WARNING() << "some warning text";
//
// Error and Fatal messages goes to STDERR, others goes to STDOUT

#include <ostream>
#include <iostream>
#include <vector>

namespace ConsoleLogger
{
	//Levels of logging
	typedef enum
	{
		trace,
		debug,
		verbose,
		info, //default
		warning,
		error,
		fatal,
		mandatory
	} LogLevel;

	static char* LogLevelName[] = {
		"TRACE",
		"DEBUG",
		"VERBOSE",
		"INFO",
		"WARNING",
		"ERROR",
		"FATAL",
		"MANDATORY"
	};

	//Singleton class which stores logger config
	class LogConfig
	{
	public:
		LogLevel CurrentLogLevel;
		LogLevel MaxLogLevelPrinted = LogLevel::trace; // keep it for program return code
		bool AddTimestamp = false;
		bool AddLogLevelName = false;

		//singleton stuff
	public:
		static LogConfig& GetInstance()
		{
			static LogConfig s;
			return s;
		}
	private:
		LogConfig() : CurrentLogLevel(LogLevel::info)
		{}
		LogConfig(LogConfig const&) = delete;
		LogConfig& operator= (LogConfig const&) = delete;
	};


	void SetLogLevel(LogLevel lvl);
	void SetAttrLogLevelVisible(bool b);
	void SetAttrTimestampVisible(bool b);
	LogLevel GetMaxLogLevelPrinted();

	//Base class for other loggers
	template<std::ostream& OSTREAM, LogLevel LOGLEVEL>
	class BaseLogger final
	{
	public:
		template <class T>
		BaseLogger& operator << (const T& arg)
		{
			if (IsLogLevelEnough())
				OSTREAM << arg;
			return *this;
		}

		template <>
		BaseLogger& operator << (const std::wstring& arg)
		{
			if (IsLogLevelEnough())
				OSTREAM << convertWStringToString(arg).c_str();
			return *this;
		}

		BaseLogger& operator << (const wchar_t* p)
		{
			if (IsLogLevelEnough())
				OSTREAM << convertPWCharToString(p).c_str();
			return *this;
		}
		
		//specification for stream manipulators
		BaseLogger& operator<<(std::ostream& (*os)(std::ostream&))
		{
			if (IsLogLevelEnough())
				OSTREAM << os;
			return *this;
		}

		void PrintLogEntryAttributes()
		{
			if (!IsLogLevelEnough())
				return;

			LogConfig& config = LogConfig::GetInstance();

			if (config.AddTimestamp)
				(*this) << '[' << keeper::PTimeToWstring(boost::posix_time::microsec_clock::local_time()) << ']';
			if (config.AddLogLevelName)
				(*this) << '[' << LogLevelName[LOGLEVEL] << ']';
		}

	private:
		bool IsLogLevelEnough()
		{
#pragma warning(suppress: 4127)
			if (LOGLEVEL == LogLevel::mandatory)
				return true;
			else
			{
				LogConfig& inst = LogConfig::GetInstance();
				if (inst.MaxLogLevelPrinted < LOGLEVEL)
					inst.MaxLogLevelPrinted = LOGLEVEL;
				return inst.CurrentLogLevel <= LOGLEVEL;
			}
		}

		std::string convertWStringToString(const std::wstring& source)
		{
			if (source.empty())
				return std::string();

			int length = ::WideCharToMultiByte(CP_OEMCP, 0, source.data(), (int)source.length(), NULL, 0, NULL, NULL);
			if (length <= 0)
				return std::string();

			std::vector<char> buffer(length);
			::WideCharToMultiByte(CP_OEMCP, 0, source.data(), (int)source.length(), &buffer[0], length, NULL, NULL);

			return std::string(buffer.begin(), buffer.end());
		}

		std::string convertPWCharToString(const wchar_t* p)
		{
			if (p == nullptr)
				return std::string();
			size_t sourceLength = std::wcslen(p);
			if (sourceLength == 0)
				return std::string();

			int length = ::WideCharToMultiByte(CP_OEMCP, 0, p, sourceLength, NULL, 0, NULL, NULL);
			if (length <= 0)
				return std::string();

			std::vector<char> buffer(length);
			::WideCharToMultiByte(CP_OEMCP, 0, p, sourceLength, &buffer[0], length, NULL, NULL);

			return std::string(buffer.begin(), buffer.end());
		}

		//singleton stuff
	public:
		static BaseLogger& GetInstance()
		{
			static BaseLogger log;
			return log;
		}
	private:
		BaseLogger() {}
		BaseLogger(BaseLogger const&) = delete;
		BaseLogger& operator= (BaseLogger const&) = delete;
	};

	typedef BaseLogger<std::cout, LogLevel::trace> LogTrace;
	typedef BaseLogger<std::cout, LogLevel::debug> LogDebug;
	typedef BaseLogger<std::cout, LogLevel::verbose> LogVerbose;
	typedef BaseLogger<std::cout, LogLevel::info> LogInfo;
	typedef BaseLogger<std::cout, LogLevel::warning> LogWarning;
	typedef BaseLogger<std::cerr, LogLevel::error> LogError;
	typedef BaseLogger<std::cerr, LogLevel::fatal> LogFatal;
	typedef BaseLogger<std::cout, LogLevel::mandatory> LogMandatory;

	LogTrace& LOG_TRACE();
	LogDebug& LOG_DEBUG();
	LogVerbose& LOG_VERBOSE();
	LogInfo& LOG_INFO();
	LogWarning& LOG_WARNING();
	LogError& LOG_ERROR();
	LogFatal& LOG_FATAL();
	LogMandatory& CONSOLE();
}