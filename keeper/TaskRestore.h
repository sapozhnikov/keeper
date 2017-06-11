#pragma once
#include "TaskContext.h"
#include "FilesTransformer.h"

class TaskRestore final
{
public:
	TaskRestore(keeper::TaskContext& ctx);
	~TaskRestore();
	bool Run();

private:
	void RestoreFromEventFolder(const Dbt & key, const DbFileEvent& data);
	void RestoreFromMirrorFolder(const Dbt & key, const DbFileEvent& data);
	keeper::TaskContext& ctx_;
	std::unique_ptr<keeper::FilesTransformer> transformer;
};

