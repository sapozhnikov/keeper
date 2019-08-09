#include "stdafx.h"
#include "TaskRestore.h"
#include <boost\date_time\posix_time\posix_time.hpp>
#include <boost\scope_exit.hpp>
#include "GlobalUtils.h"
#include "FileIO.h"

using namespace ConsoleLogger;
using namespace boost::posix_time;

TaskRestore::TaskRestore(keeper::TaskContext & ctx) :
	ctx_(ctx)
{
}

TaskRestore::~TaskRestore()
{
}

bool IsDirectory(const Dbt& rec)
{
	if ((static_cast<DbFileEvent*>(rec.get_data()))->FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return true;
	else
		return false;
}

void RestoreFileTimestamps(const std::wstring& path, const WIN32_FILE_ATTRIBUTE_DATA& attribData)
{
	HANDLE hFile = CreateFile(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != NULL)
	{
		SetFileTime(hFile, &attribData.ftCreationTime, &attribData.ftLastAccessTime, &attribData.ftLastWriteTime);
		CloseHandle(hFile);
	}
	else
		LOG_ERROR() << "Can't change file's timestamps: " << path << std::endl;
}

void TaskRestore::RestoreFromMirrorFolder(const Dbt& key, const DbFileEvent& data)
{
	std::wstring relativePath = keeper::DbtToWstring(key);
	
	if (ctx_.NamesChecker.IsFilteringEnabled)
	{
		if (!ctx_.NamesChecker.IsFitPattern(relativePath))
			return;
	}

	std::wstring destinationFullPath = ctx_.GetDestinationDirectory() + relativePath;
	bool isRestoreSucceed;
	bool isDirectory = (data.FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	if (isDirectory)
	{
		if (boost::filesystem::exists(destinationFullPath))
			isRestoreSucceed = true;
		else
			isRestoreSucceed = keeper::FileIO::CreateDir(destinationFullPath);
	}
	else
	{
		auto transformedPath_ = transformer->GetTransformedName(relativePath, isDirectory);
		isRestoreSucceed = transformer->RestoreFile(ctx_.GetSourceDirectory() + MIRROR_SUB_DIR + transformedPath_, destinationFullPath);
	}
	if (isRestoreSucceed)
	{
		SetFileAttributes(destinationFullPath.c_str(), data.FileAttributes.dwFileAttributes);
		RestoreFileTimestamps(destinationFullPath, data.FileAttributes);
	}
}

void TaskRestore::RestoreFromEventFolder(const Dbt & key, const DbFileEvent& data)
{
	boost::posix_time::ptime storeOldTimestamp = keeper::ConvertMillisecToPtime(data.EventTimeStamp);
	bool isDirectory = (data.FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	std::wstring relativePath = keeper::DbtToWstring(key);

	if (ctx_.NamesChecker.IsFilteringEnabled)
	{
		if (!ctx_.NamesChecker.IsFitPattern(relativePath))
			return;
	}

	auto transformedPath_ = transformer->GetTransformedName(relativePath, isDirectory);
	std::wstring storeOldFullPath = ctx_.GetSourceDirectory() + keeper::PTimeToWstringSafeSymbols(storeOldTimestamp) + L'\\' + transformedPath_;
	std::wstring destinationFullPath = ctx_.GetDestinationDirectory() + relativePath;
	bool restoreResult;

	if (data.FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (boost::filesystem::exists(destinationFullPath))
			restoreResult = true;
		else
			restoreResult = keeper::FileIO::CreateDir(destinationFullPath);
	}
	else
	{
		restoreResult = transformer->RestoreFile(storeOldFullPath, destinationFullPath);
	}
	if (restoreResult)
	{
		SetFileAttributes(destinationFullPath.c_str(), data.FileAttributes.dwFileAttributes);
		RestoreFileTimestamps(destinationFullPath, data.FileAttributes);
	}
}

inline FileEventType GetEventType(const Dbt& dbt)
{
	return static_cast<FileEventType>(static_cast<DbFileEvent*>(dbt.get_data())->FileEvent);
}

bool TaskRestore::Run()
{
	if (!ctx_.OpenDatabase())
	{
		LOG_FATAL() << "Can't open database" << std::endl;
		return false;
	}
	transformer = std::make_unique<keeper::FilesTransformer>(ctx_);

	const ptime& timestamp = ctx_.GetRestoreTimeStamp();
	int64_t userTimestamp64 = (timestamp != not_a_date_time) ? keeper::ConvertPtimeToMillisec(timestamp) : 0;

	Dbc* mainCursor = nullptr;
	ctx_.GetMainDB().cursor(nullptr, &mainCursor, 0);
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (mainCursor != nullptr)
			mainCursor->close();
	};

	Dbt keyMain, dataMain/*, dataMainPrev*/;
	DbFileEvent fileEvent = { 0 }, fileEventPrev = { 0 };

	Dbt dataNext;
	int64_t eventTimestamp;
	int result;
	
	while (true)
	{
		result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_NODUP);
		if (result == DB_NOTFOUND) //end of DB
			break;

		//got the file name, seek for bounded events
		bool isFirstEvent = true;
		bool isFileRestored = false;
		while (true)
		{
			eventTimestamp = static_cast<DbFileEvent*>(dataMain.get_data())->EventTimeStamp;
			fileEvent = *static_cast<DbFileEvent*>(dataMain.get_data());

			if (timestamp != not_a_date_time)
			{
				//timestamp match exactly
				if (eventTimestamp == userTimestamp64)
				{
					if (GetEventType(dataMain) == FileEventType::Deleted)
						break;
					result = mainCursor->get(&keyMain, &dataNext, DB_NEXT_DUP);
					//is it the last event?
					if (result == DB_NOTFOUND)
						RestoreFromMirrorFolder(keyMain, fileEvent);
					else
						RestoreFromEventFolder(keyMain, fileEvent);
					
					isFileRestored = true;
					break;
				}

				//got event after timestamp
				if (eventTimestamp > userTimestamp64)
				{
					isFileRestored = true; //in any case
					if (isFirstEvent)
					{
						//first event occured AFTER timestamp

						//if first event !Added, then restore from the next event, cause Added event was purged
						if (GetEventType(dataMain) != FileEventType::Added)
						{
							//read next event
							result = mainCursor->get(&keyMain, &dataNext, DB_NEXT_DUP);
							if (result == DB_NOTFOUND)
								RestoreFromMirrorFolder(keyMain, fileEvent);
							else
								RestoreFromEventFolder(keyMain, fileEvent);

							break;
						}
						else
							break; //there was no such file at the given time
					}
					else
					{
						//previous event as the actual
						if (static_cast<FileEventType>(fileEventPrev.FileEvent) == FileEventType::Deleted) //no such file at this time
							break;
						
						RestoreFromEventFolder(keyMain, fileEvent);
					}

					break;
				}
			}//if (timestamp != not_a_date_time)
			else
			{
				//seek till the end
			}
			//proceed to the next event
			fileEventPrev = fileEvent;
			isFirstEvent = false;

			result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_DUP);
			if (result != 0)
				break;
		}////////////////////////////////////////////

		if (isFileRestored)
			continue;

		if (result == DB_NOTFOUND)
		{
			//got the last event
			//if (timestamp == not_a_date_time)
			{
				//ReplayLastEvent(keyMain, dataMain);
				if (static_cast<FileEventType>(fileEventPrev.FileEvent) != FileEventType::Deleted)
					RestoreFromMirrorFolder(keyMain, fileEventPrev);
			}
		}
	}//while

	return true;
}
