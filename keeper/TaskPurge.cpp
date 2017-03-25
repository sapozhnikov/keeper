#include "stdafx.h"
#include "TaskPurge.h"
#include "GlobalUtils.h"
#include <boost\scope_exit.hpp>
#include <set>
#include <vector>
//#include <boost\filesystem.hpp>
#include "FileIO.h"

using namespace keeper;
using namespace ConsoleLogger;
using namespace boost::posix_time;

TaskPurge::TaskPurge(keeper::TaskContext & ctx) :
	ctx_(ctx)
{
}

inline FileEventType GetEventType(const Dbt& dbt)
{
	return static_cast<FileEventType>(static_cast<DbFileEvent*>(dbt.get_data())->FileEvent);
}

bool TaskPurge::Run()
{
	auto purgeTimeStamp = ctx_.GetPurgeTimeStamp();
	if (purgeTimeStamp == not_a_date_time)
	{
		LOG_FATAL() << "Purge timestamp not defined" << std::endl;
		return false;
	}

	if (!ctx_.OpenDatabase())
	{
		LOG_FATAL() << "Can't open database" << std::endl;
		return false;
	}

	LOG_INFO() << "Purging files older than " << PTimeToWstring(purgeTimeStamp) << std::endl;

	Dbc* mainCursor;
	Dbt keyMain, dataMain;
	ctx_.GetMainDB().cursor(NULL, &mainCursor, 0);
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (mainCursor != nullptr)
			mainCursor->close();

		ctx_.CloseDatabase();
	};

	int64_t timestamp64 = keeper::ConvertPtimeToMillisec(purgeTimeStamp);
	std::set<int64_t> deletedDirsTimestamps64;
	std::wstring relativePath;
	int result;
	DbFileEvent* pEvent;
	while (true)
	{
		result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_NODUP);
		if (result == DB_NOTFOUND) //end of DB
			break;

		while (true)
		{
			if (dataMain.get_size() != sizeof(DbFileEvent))
				throw std::exception("Corrupted DB data");

			pEvent = static_cast<DbFileEvent*>(dataMain.get_data());
			auto eventTimestamp64 = pEvent->EventTimeStamp;
			if (eventTimestamp64 < timestamp64)
			{
				//check if next record present
				result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_DUP);
				if (result == DB_NOTFOUND)
				{
					//it was the last event
					if (static_cast<FileEventType>(pEvent->FileEvent) == FileEventType::Deleted)
					{
						mainCursor->del(0);
						deletedDirsTimestamps64.insert(eventTimestamp64);
					}
					break;
				}
				
				//step back and delete
				result = mainCursor->get(&keyMain, &dataMain, DB_PREV_DUP);
				//delete DB record
				mainCursor->del(0);

				deletedDirsTimestamps64.insert(eventTimestamp64);
			}

			result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_DUP);
			if (result == DB_NOTFOUND)
				break;
		}
	}

	if (!deletedDirsTimestamps64.empty())
	{
		LOG_INFO() << "Purging backups at:" << std::endl;
		for (auto& ts : deletedDirsTimestamps64)
		{
			std::wstring storeOldDir = ctx_.GetSourceDirectory() + PTimeToWstringSafeSymbols(ConvertMillisecToPtime(ts));
			if (boost::filesystem::exists(storeOldDir))
			{
				LOG_INFO() << PTimeToWstring(ConvertMillisecToPtime(ts)) << std::endl;
				keeper::FileIO::RemoveDir(storeOldDir);
			}
		}

		//compress DB
		LOG_VERBOSE() << "Compressing database" << std::endl;
		ctx_.CompressDatabase();
	}
	
	LOG_VERBOSE() << "Deleting orphaned files (without DB record)" << std::endl;
	std::wstring mirrorDir = ctx_.GetSourceDirectory() + MIRROR_SUB_DIR;
	auto skipPathCharsCount = mirrorDir.length();
	boost::filesystem::recursive_directory_iterator dirIterator(mirrorDir);
	std::vector<std::wstring> filesToDelete;

	while (true)
	{
		if (dirIterator == boost::filesystem::recursive_directory_iterator())
			break;
		const std::wstring& sourceFullPath = dirIterator->path().wstring();
		relativePath = sourceFullPath.substr(skipPathCharsCount, skipPathCharsCount - sourceFullPath.length());

		//get the last event
		Dbt key = keeper::WstringToDbt(relativePath);
		Dbt data;

		result = mainCursor->get(&key, &data, DB_SET);
		if (result == 0)
		{
			while (true)
			{
				result = mainCursor->get(&key, &data, DB_NEXT_DUP);
				if (result == DB_NOTFOUND)
					break;
				//TODO: result check
			}
			pEvent = static_cast<DbFileEvent*>(data.get_data());
			if (static_cast<FileEventType>(pEvent->FileEvent) == FileEventType::Deleted)
				filesToDelete.push_back(dirIterator->path().wstring());
		}
		else
			if (result == DB_NOTFOUND)
			{
				filesToDelete.push_back(dirIterator->path().wstring());
			}
		dirIterator++;
	}

	for (const auto& filename : filesToDelete)
	{
		if (boost::filesystem::exists(filename))
			keeper::FileIO::RemoveDir(filename);
	}

	return true;
}
