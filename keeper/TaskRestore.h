#pragma once
#include "TaskContext.h"

class TaskRestore final
{
public:
	TaskRestore(keeper::TaskContext& config);
	~TaskRestore();
	bool Run();

private:
	keeper::TaskContext& ctx_;
	
	//void RestoreFromMirrorFolder(const Dbt& key, const Dbt& data);
	//void RestoreFromEventFolder(const Dbt& key, const Dbt& data);
	void RestoreFromEventFolder(const Dbt & key, const DbFileEvent& data);
	void RestoreFromMirrorFolder(const Dbt & key, const DbFileEvent& data);
};

