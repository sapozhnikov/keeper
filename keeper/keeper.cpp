#include "stdafx.h"
#include "TaskContext.h"
#include "CLIparser.h"
#include "TaskBackup.h"
#include "TaskDumpDB.h"
#include "TaskRestore.h"
#include "TaskPurge.h"
#include "GlobalUtils.h"

using namespace std;
using namespace ConsoleLogger;
using namespace keeper;

keeper::TaskContext taskCtx;

//for the emergency DB flush
DbEnv* BuroEnv = nullptr;
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);

int wmain(int argc, wchar_t *argv[])
{
	SetLogLevel(ConsoleLogger::LogLevel::info);

	LOG_INFO() << MESSAGE_ABOUT << endl;
	LOG_INFO() << "This software comes with absolutely no warranty" << endl;

	if (!ParseCLITask(taskCtx, argc, argv))
		exit(-1);

	sodium_init();

#ifdef _DEBUG
	SetLogLevel(ConsoleLogger::LogLevel::trace);
	SetAttrLogLevelVisible(true);
	SetAttrTimestampVisible(true);
#endif
	auto normalExit = []()
	{
		LOG_INFO() << "Finished without errors" << endl;
		exit(0);
	};
	auto abnormalExit = []()
	{
		LOG_INFO() << "Finished with errors" << endl;
		exit(-1);
	};

	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
		LOG_WARNING() << "Could not set control handler" << std::endl;

	try
	{
		switch (taskCtx.Task)
		{
		case keeper::Task::Backup:
		{
			TaskBackup taskBackup(taskCtx);
			taskBackup.Run();
		}
		break;

		case keeper::Task::DumpDB:
		{
			TaskDumpDB taskDump(taskCtx);
			taskDump.Run();
		}
		break;

		case keeper::Task::Restore:
		{
			TaskRestore taskRestore(taskCtx);
			taskRestore.Run();
		}
		break;

		case keeper::Task::Purge:
		{
			TaskPurge taskPurge(taskCtx);
			taskPurge.Run();
		}
		break;

		default:
			break;
		}
	}
	catch (DbException& e)
	{
		LOG_FATAL() << "DB error: " << e.what() << endl;
		abnormalExit();
	}
	catch (runtime_error& e)
	{
		LOG_FATAL() << StrAnsiToOEM(e.what()) << endl;
		abnormalExit();
	}
	catch (exception& e)
	{
		LOG_FATAL() << StrAnsiToOEM(e.what()) << endl;
		abnormalExit();
	}
	catch (...)
	{
		LOG_FATAL() << "Error occured" << endl;
		abnormalExit();
	}

	if (GetMaxLogLevelPrinted() > LogLevel::warning)
		abnormalExit();
	else
		normalExit();
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	using namespace std;
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		//praise the Void
		if (BuroEnv)
			BuroEnv->txn_checkpoint(0, 0, 0);
		return true;
	default:
		return FALSE;
	}
}
