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

	//byte buffer[1024];

	keeper::TaskContext& ctx_;
	boost::filesystem::recursive_directory_iterator dirIterator_;
	boost::filesystem::path initialDirectory_;
	unsigned int skipPathCharsCount_ = 0;
	std::wstring mirrorPath_;

	std::wstring storeOldPath_;
	bool isStoreOldFolderCreated = false;
	void CheckAndMakeStoreOldFolder();

	void DbAddEvent(const std::wstring& fullPath, const std::wstring& relativePath, FileEventType ev);
	//bool DbGetFileAttributes(const std::wstring& relativePath, DbFileProperties& props);

	boost::posix_time::ptime currentTimeStamp_;
	int64_t timestamp64_;

	FileState GetFileState(const std::wstring& relativePath);
	Dbc* mainCursor_ = nullptr;
};

