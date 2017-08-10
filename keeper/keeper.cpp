#include "stdafx.h"
#include "TaskContext.h"
#include "CLIparser.h"
#include "TaskBackup.h"
#include "TaskDumpDB.h"
#include "TaskRestore.h"
#include "TaskPurge.h"

using namespace std;
using namespace ConsoleLogger;

keeper::TaskContext taskCtx;

int wmain(int argc, wchar_t *argv[])
{
	SetLogLevel(ConsoleLogger::LogLevel::info);

	LOG_INFO() << MESSAGE_ABOUT << endl;

	if (!ParseCLITask(taskCtx, argc, argv))
		exit(0);

	sodium_init();

#ifdef _DEBUG
	SetLogLevel(ConsoleLogger::LogLevel::trace);
	SetAttrLogLevelVisible(true);
	SetAttrTimestampVisible(true);
#endif

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
		return -1;
	}
	catch (runtime_error& e)
	{
		LOG_FATAL() << e.what() << endl;
		return -1;
	}
	catch (exception& e)
	{
		LOG_FATAL() << e.what() << endl;
		return -1;
	}
	catch (...)
	{
		LOG_FATAL() << "Error occured" << endl;
		return -1;
	}

	if (GetMaxLogLevelPrinted() > LogLevel::warning)
		return -1;
	else
		return 0;
}

