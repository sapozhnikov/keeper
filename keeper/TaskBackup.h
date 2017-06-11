#pragma once
#include <boost\filesystem.hpp>
#include "TaskContext.h"
//#include <db_cxx.h>

class TaskBackup final
{
public:
	TaskBackup(keeper::TaskContext& ctx);
	~TaskBackup();

	void Run();

private:
	enum class FileState
	{
		Error,
		New,
		Same,
		Changed,
		Deleted
	};

	keeper::TaskContext& ctx_;
	boost::filesystem::recursive_directory_iterator dirIterator_;
	unsigned int skipPathCharsCount_ = 0;
	std::wstring mirrorPath_;
	std::wstring relativePath_;
	std::wstring transformedPath_;

	std::wstring storeOldPath_;

	void DbAddEvent(const std::wstring& fullPath, const std::wstring& relativePath, FileEventType ev);

	boost::posix_time::ptime currentTimeStamp_;
	int64_t timestamp64_;

	FileState GetFileState();
	Dbc* mainCursor_ = nullptr;
};

