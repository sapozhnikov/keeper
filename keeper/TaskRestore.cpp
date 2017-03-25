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

void TaskRestore::RestoreFromMirrorFolder(const Dbt & key, const Dbt& data)
{
	std::wstring relativePath = keeper::DbtToWstring(key);
	std::wstring destinationFullPath = ctx_.GetDestinationDirectory() + relativePath;
	bool restoreResult;

	if (IsDirectory(data))
	{
		if (boost::filesystem::exists(destinationFullPath))
			restoreResult = true;
		else
			restoreResult = keeper::FileIO::CreateDir(destinationFullPath);
	}
	else
	{
		restoreResult = keeper::FileIO::CopySingleFile(ctx_.GetSourceDirectory() + MIRROR_SUB_DIR + relativePath, destinationFullPath);
	}
	if (restoreResult)
		SetFileAttributes(destinationFullPath.c_str(), static_cast<DbFileEvent*>(data.get_data())->FileAttributes.dwFileAttributes);
}

void TaskRestore::RestoreFromMirrorFolder(const Dbt & key, const DbFileEvent& data)
{
	std::wstring relativePath = keeper::DbtToWstring(key);
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
		restoreResult = keeper::FileIO::CopySingleFile(ctx_.GetSourceDirectory() + MIRROR_SUB_DIR + relativePath, destinationFullPath);
	}
	if (restoreResult)
		SetFileAttributes(destinationFullPath.c_str(), data.FileAttributes.dwFileAttributes);
}

void TaskRestore::RestoreFromEventFolder(const Dbt & key, const Dbt & data)
{
	DbFileEvent* pEvent = static_cast<DbFileEvent*>(data.get_data());
	boost::posix_time::ptime storeOldTimestamp = keeper::ConvertMillisecToPtime(pEvent->EventTimeStamp);

	std::wstring relativePath = keeper::DbtToWstring(key);
	std::wstring storeOldFullPath = ctx_.GetSourceDirectory() + keeper::PTimeToWstringSafeSymbols(storeOldTimestamp) + L'\\' + relativePath;
	std::wstring destinationFullPath = ctx_.GetDestinationDirectory() + relativePath;
	bool restoreResult;

	if (IsDirectory(data))
	{
		if (boost::filesystem::exists(destinationFullPath))
			restoreResult = true;
		else
			restoreResult = keeper::FileIO::CreateDir(destinationFullPath);
	}
	else
	{
		restoreResult = keeper::FileIO::CopySingleFile(storeOldFullPath, destinationFullPath);
	}
	if (restoreResult)
		SetFileAttributes(destinationFullPath.c_str(), static_cast<DbFileEvent*>(data.get_data())->FileAttributes.dwFileAttributes);
}

void TaskRestore::RestoreFromEventFolder(const Dbt & key, const DbFileEvent& data)
{
	boost::posix_time::ptime storeOldTimestamp = keeper::ConvertMillisecToPtime(data.EventTimeStamp);

	std::wstring relativePath = keeper::DbtToWstring(key);
	std::wstring storeOldFullPath = ctx_.GetSourceDirectory() + keeper::PTimeToWstringSafeSymbols(storeOldTimestamp) + L'\\' + relativePath;
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
		restoreResult = keeper::FileIO::CopySingleFile(storeOldFullPath, destinationFullPath);
	}
	if (restoreResult)
		SetFileAttributes(destinationFullPath.c_str(), data.FileAttributes.dwFileAttributes);
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

	const ptime& timestamp = ctx_.GetRestoreTimeStamp();
	int64_t userTimestamp64 = (timestamp != not_a_date_time) ? keeper::ConvertPtimeToMillisec(timestamp) : 0;

	Dbc* mainCursor = nullptr;
	ctx_.GetMainDB().cursor(NULL, &mainCursor, 0);
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (mainCursor != nullptr)
			mainCursor->close();

		ctx_.CloseDatabase();
	};

	Dbt keyMain, dataMain, dataMainPrev;
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
						//RestoreFromMirrorFolder(keyMain, dataMain);
						RestoreFromMirrorFolder(keyMain, fileEvent);
					else
						//RestoreFromEventFolder(keyMain, dataMain);
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
								//RestoreFromMirrorFolder(keyMain, dataMain);
								RestoreFromMirrorFolder(keyMain, fileEvent);
							else
								//RestoreFromEventFolder(keyMain, dataMain);
								RestoreFromEventFolder(keyMain, fileEvent);

							break;
						}
						else
							break; //there was no such file at the given time
					}
					else
					{
						//previous event as the actual
						//if (GetEventType(dataMainPrev) == FileEventType::Deleted) //no such file at this time
						if (static_cast<FileEventType>(fileEventPrev.FileEvent) == FileEventType::Deleted) //no such file at this time
							break;
						
						//RestoreFromEventFolder(keyMain, dataMain);
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
			dataMainPrev = dataMain;
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
				if (GetEventType(dataMainPrev) != FileEventType::Deleted)
					RestoreFromMirrorFolder(keyMain, dataMainPrev);
			}
		}
	}//while

	return true;
}
