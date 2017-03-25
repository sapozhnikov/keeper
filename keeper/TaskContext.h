#pragma once
#include <boost\date_time\posix_time\posix_time.hpp>

namespace keeper
{
	enum class Task
	{
		Undetected, //used in CLI parser
		Backup,
		Restore,
		List,
		Purge,
		DumpDB
	};

	struct TaskContext
	{
	public:
		TaskContext();

		keeper::Task Task;
		//avoiding BOOST's bug with extended length filenames
		bool SetSourceDirectory(std::wstring InitialDirectory);
		const std::wstring& GetSourceDirectory() const;
		bool SetDestinationDirectory(std::wstring InitialDirectory);
		const std::wstring& GetDestinationDirectory() const;
		bool SetRestoreTimeStamp(std::string timestamp);
		const boost::posix_time::ptime& GetRestoreTimeStamp() const;
		const boost::posix_time::ptime& GetPurgeTimeStamp() const;
		bool SetPurgeTimestampFromArg(std::string timestamp);
		bool OpenDatabase();
		bool CreateDatabase();
		bool CompressDatabase();
		void CloseDatabase();
		Db& GetMainDB();

	private:
		bool SetPurgeTimeStampFromDate(std::string timestamp);
		bool SetPurgeTimeStampFromDuration(std::string timestamp);

		std::wstring sourceDirectory_;
		std::wstring destinationDirectory_;
		Db db_/*, dbDelta_*/;
		boost::posix_time::ptime restoreTimeStamp_ = boost::posix_time::not_a_date_time;
		boost::posix_time::ptime purgeTimeStamp_ = boost::posix_time::not_a_date_time;
		std::wstring GenerateDbPath();
	};
}