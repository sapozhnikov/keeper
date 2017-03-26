#include "stdafx.h"
#include "TaskContext.h"
#include "CLIparser.h"
#include "TaskBackup.h"
#include "TaskDumpDB.h"
#include "TaskRestore.h"
#include "TaskList.h"
#include "TaskPurge.h"

using namespace std;
using namespace ConsoleLogger;

keeper::TaskContext taskCtx;

int wmain(int argc, wchar_t *argv[])
{
	SetLogLevel(ConsoleLogger::LogLevel::info);

	if (!ParseCLITask(taskCtx, argc, argv))
		exit(0);

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

		case keeper::Task::List:
		{
			TaskList taskList(taskCtx);
			taskList.Run();
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
	}
	catch (exception& e)
	{
		LOG_FATAL() << e.what();
	}

	if (GetMaxLogLevelPrinted() > LogLevel::warning)
		return -1;
	else
		return 0;
}

