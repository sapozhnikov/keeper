#pragma once
#include "TaskContext.h"

class TaskPurge final
{
public:
	TaskPurge(keeper::TaskContext& ctx);
	bool Run();

private:
	keeper::TaskContext& ctx_;
};