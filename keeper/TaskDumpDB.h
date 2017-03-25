#pragma once
#include "TaskContext.h"

class TaskDumpDB final
{
public:
	TaskDumpDB(keeper::TaskContext& ctx);
	~TaskDumpDB();

	bool Run();
private:
	keeper::TaskContext& ctx_;
};

