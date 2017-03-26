#include "stdafx.h"
#include "ConsoleLogger.h"
#include "GlobalUtils.h"

namespace ConsoleLogger
{
	void SetLogLevel(LogLevel lvl)
	{
		LogConfig::GetInstance().CurrentLogLevel = lvl;
	}

	void ConsoleLogger::SetAttrLogLevelVisible(bool b)
	{
		LogConfig::GetInstance().AddLogLevelName = b;
	}
	
	void ConsoleLogger::SetAttrTimestampVisible(bool b)
	{
		LogConfig::GetInstance().AddTimestamp = b;
	}

	LogLevel ConsoleLogger::GetMaxLogLevelPrinted()
	{
		return LogConfig::GetInstance().MaxLogLevelPrinted;
	}

	LogTrace& LOG_TRACE()
	{
		LogTrace& inst = LogTrace::GetInstance();
		inst.PrintLogEntryAttributes();
		return inst;
	}

	LogDebug& LOG_DEBUG()
	{
		LogDebug& inst = LogDebug::GetInstance();
		inst.PrintLogEntryAttributes();
		return inst;
	}

	LogVerbose& LOG_VERBOSE()
	{
		LogVerbose& inst = LogVerbose::GetInstance();
		inst.PrintLogEntryAttributes();
		return inst;
	}

	LogInfo& LOG_INFO()
	{
		LogInfo& inst = LogInfo::GetInstance();
		inst.PrintLogEntryAttributes();
		return inst;
	}

	LogWarning& LOG_WARNING()
	{
		LogWarning& inst = LogWarning::GetInstance();
		inst.PrintLogEntryAttributes();
		return inst;
	}

	LogError& LOG_ERROR()
	{
		LogError& inst = LogError::GetInstance();
		inst.PrintLogEntryAttributes();
		return inst;
	}

	LogFatal& LOG_FATAL()
	{
		LogFatal& inst = LogFatal::GetInstance();
		inst.PrintLogEntryAttributes();
		return inst;
	}

	LogMandatory& CONSOLE()
	{
		//LogMandatory::GetInstance().PrintLogEntryAttributes();
		return LogMandatory::GetInstance();
	}
}