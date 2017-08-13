#pragma once
#include <boost\date_time\posix_time\posix_time.hpp>
#include "WildCardNameChecker.h"

namespace keeper
{
	enum class Task
	{
		Undetected, //used in CLI parser
		Backup,
		Restore,
		Purge,
		DumpDB
	};

	struct TaskContext
	{
	public:
		TaskContext();
		~TaskContext();

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
		bool OpenDatabase(bool LoadConfig = true);
		bool CreateDatabase();
		bool CompressDatabase();
		void CloseDatabase();
		Db& GetMainDB();

		DWORD CompressionLevel = 0;
		std::string DbPassword;
		byte FileEncodeKey_[crypto_stream_chacha20_KEYBYTES];
		bool isEncryptedFileNames = false;
		byte NamesEncodeKey_[crypto_stream_chacha20_KEYBYTES];
		byte NamesEncodeNonce_[crypto_stream_chacha20_NONCEBYTES];

		//file names wildcards support
		WildCardNameChecker NamesChecker;

	private:
		bool SetPurgeTimeStampFromDate(std::string timestamp);
		bool SetPurgeTimeStampFromDuration(std::string timestamp);

		bool SetConfigValueDWord(const std::string& name, DWORD val);
		bool GetConfigValueDWord(const std::string & name, DWORD& val);
		bool SetConfigValueBinaryArr(const std::string& name, byte* val, unsigned int len);
		bool GetConfigValueBinaryArr(const std::string& name, byte* val, unsigned int len);
		void DisplayTaskConfig() const;

		std::wstring sourceDirectory_;
		std::wstring destinationDirectory_;
		Db eventsDb_, configDb_, secretsDb_;
		boost::posix_time::ptime restoreTimeStamp_ = boost::posix_time::not_a_date_time;
		boost::posix_time::ptime purgeTimeStamp_ = boost::posix_time::not_a_date_time;
		std::wstring GenerateDbPath();
		std::string DbKey; //key derived from password
	};
}