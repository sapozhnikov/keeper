#include "stdafx.h"
#include "TaskContext.h"
#include "GlobalUtils.h"
#include <boost\filesystem.hpp>

namespace keeper
{
	void NormalizePath(std::wstring& path)
	{
		//make path extended length by default
		//TODO: skip it for Win10 and newer
		if ((path[0] != L'\\') ||
			(path[1] != L'\\') ||
			(path[2] != L'?') ||
			(path[3] != L'\\'))
			path = L"\\\\?\\" + path;

		if (path[path.length() - 1] != L'\\')
			path.append(L"\\");
	}

	keeper::TaskContext::TaskContext() : db_(nullptr, 0)
	{
	}

	bool TaskContext::SetSourceDirectory(std::wstring SourceDirectory)
	{
		sourceDirectory_ = SourceDirectory;
		NormalizePath(sourceDirectory_);

		return true;
	}

	bool TaskContext::SetDestinationDirectory(std::wstring DestinationDirectory)
	{
		destinationDirectory_ = DestinationDirectory;
		NormalizePath(destinationDirectory_);

		return true;
	}

	const std::wstring& TaskContext::GetSourceDirectory() const
	{
		return sourceDirectory_;
	}

	const std::wstring& TaskContext::GetDestinationDirectory() const
	{
		return destinationDirectory_;
	}

	bool keeper::TaskContext::SetRestoreTimeStamp(std::string timestamp)
	{
		if (timestamp == std::string())
		{
			restoreTimeStamp_ = boost::posix_time::not_a_date_time;
			return true;
		}

		try
		{
			restoreTimeStamp_ = boost::posix_time::time_from_string(timestamp);
		}
		catch (...)
		{
			restoreTimeStamp_ = boost::posix_time::not_a_date_time;
			return false;
		}

		return true;
	}

	bool keeper::TaskContext::SetPurgeTimeStampFromDate(std::string timestamp)
	{
		if (timestamp == std::string())
		{
			purgeTimeStamp_ = boost::posix_time::not_a_date_time;
			return true;
		}

		try
		{
			purgeTimeStamp_ = boost::posix_time::time_from_string(timestamp);
		}
		catch (...)
		{
			purgeTimeStamp_ = boost::posix_time::not_a_date_time;
			return false;
		}

		return true;
	}

	bool keeper::TaskContext::SetPurgeTimeStampFromDuration(std::string timestamp)
	{
		auto duration = StringToDuration(timestamp);
		if (duration == boost::posix_time::not_a_date_time)
			return false;
		auto currentTimeStamp = boost::posix_time::microsec_clock::local_time();
		purgeTimeStamp_ = currentTimeStamp - duration;

		return true;
	}

	bool keeper::TaskContext::SetPurgeTimestampFromArg(std::string timestamp)
	{
		if (timestamp[0] == L'P')
			return SetPurgeTimeStampFromDuration(timestamp);
		else
			return SetPurgeTimeStampFromDate(timestamp);
	}

	const boost::posix_time::ptime & keeper::TaskContext::GetPurgeTimeStamp() const
	{
		return purgeTimeStamp_;
	}

	const boost::posix_time::ptime & keeper::TaskContext::GetRestoreTimeStamp() const
	{
		return restoreTimeStamp_;
	}

#pragma warning(suppress: 4100)
	int DbEventsCompare(DB *db_, const DBT *data1, const DBT *data2, size_t *locp)
	{
		if ((data1->size != sizeof(DbFileEvent)) || (data2->size != sizeof(DbFileEvent)))
			throw std::exception("Corrupted database");

		if (static_cast<DbFileEvent*>(data1->data)->EventTimeStamp >= static_cast<DbFileEvent*>(data2->data)->EventTimeStamp)
			return 1;
		else
			return -1;
	}

	//generate DB path from args according task type
	std::wstring TaskContext::GenerateDbPath()
	{
		switch (Task)
		{
		case keeper::Task::Backup:
			return GetDestinationDirectory() + MAIN_DB_FILE;
		case keeper::Task::Purge:
		case keeper::Task::Restore:
		case keeper::Task::List:
		case keeper::Task::DumpDB:
			return GetSourceDirectory() + MAIN_DB_FILE;
		default:
			return std::wstring();
		}
	}

	bool TaskContext::OpenDatabase()
	{
		using namespace std;
		wstring dbFullPath = GenerateDbPath();

		if (!boost::filesystem::exists(dbFullPath))
		{
			//no DB exists
			return false;
		}
		string dbNameUTF8 = keeper::WstringToUTF8(dbFullPath);

		db_.set_flags(DB_DUPSORT);
		db_.set_dup_compare(DbEventsCompare);

		try
		{
			//open existing
			db_.open(nullptr,
				dbNameUTF8.c_str(),
				nullptr,
				DB_HASH,
				0,
				0);
		}
		catch (DbException &e)
		{
			db_.err(e.get_errno(), "Database open failed");
			throw;
		}

		return true;
	}

	bool TaskContext::CreateDatabase()
	{
		using namespace std;

		wstring dbFullPath = GenerateDbPath();
		if (boost::filesystem::exists(dbFullPath))
		{
			//DB already exists
			return false;
		}

		boost::filesystem::create_directories(boost::filesystem::path(dbFullPath).parent_path());

		string dbNameUTF8 = keeper::WstringToUTF8(dbFullPath);

		db_.set_flags(DB_DUPSORT);
		db_.set_dup_compare(DbEventsCompare);

		try
		{
			//create new DB
			db_.open(nullptr,
				dbNameUTF8.c_str(),
				nullptr,
				DB_HASH,
				DB_CREATE /*| DB_EXCL*/,
				0);
		}
		catch (DbException &e)
		{
			db_.err(e.get_errno(), "Database create failed");
			throw;
		}

		return true;
	}

	bool keeper::TaskContext::CompressDatabase()
	{
		DB_COMPACT cData = { 0 };
		cData.compact_fillpercent = 80;
		db_.compact(nullptr, nullptr, nullptr, &cData, DB_FREELIST_ONLY, nullptr);
		return true;
	}

	void keeper::TaskContext::CloseDatabase()
	{
		db_.close(0);
	}

	Db & keeper::TaskContext::GetMainDB()
	{
		return db_;
	}
}