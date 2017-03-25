#pragma once
#include "TaskContext.h"

class TaskList final
{
public:
	TaskList(keeper::TaskContext& ctx);
	bool Run();

private:
	keeper::TaskContext& ctx_;
};