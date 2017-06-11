#include "stdafx.h"
#include "TaskContext.h"
#include "GlobalUtils.h"
#include "ConsoleLogger.h"
#include <boost\filesystem.hpp>

using namespace ConsoleLogger;

namespace keeper
{
	static char* PARAM_COMPRESSION = "COMPRESSION RATIO";
	static char* PARAM_FILES_ENCODE_KEY = "FILES ENCODE KEY";
	//static char* PARAM_FILES_ENCODE_NONCE = "FILES ENCODE NONCE";
	static char* PARAM_NAMES_ENCODE = "NAMES ENCODE";
	static char* PARAM_NAMES_ENCODE_KEY = "NAMES ENCODE KEY";
	static char* PARAM_NAMES_ENCODE_NONCE = "NAMES ENCODE NONCE";

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

	keeper::TaskContext::TaskContext() :
		eventsDb_(nullptr, 0),
		configDb_(nullptr, 0),
		secretsDb_(nullptr, 0)
	{
	}
	
	keeper::TaskContext::~TaskContext()
	{
		sodium_memzero(FileEncodeKey_, crypto_stream_chacha20_KEYBYTES);
		sodium_memzero(NamesEncodeKey_, crypto_stream_chacha20_KEYBYTES);
		sodium_memzero(NamesEncodeNonce_, crypto_stream_chacha20_NONCEBYTES);
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

	bool TaskContext::SetRestoreTimeStamp(std::string timestamp)
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

	bool TaskContext::SetPurgeTimeStampFromDate(std::string timestamp)
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

	bool TaskContext::SetPurgeTimeStampFromDuration(std::string timestamp)
	{
		auto duration = StringToDuration(timestamp);
		if (duration == boost::posix_time::not_a_date_time)
			return false;
		auto currentTimeStamp = boost::posix_time::microsec_clock::local_time();
		purgeTimeStamp_ = currentTimeStamp - duration;

		return true;
	}

	bool TaskContext::SetPurgeTimestampFromArg(std::string timestamp)
	{
		if (timestamp[0] == L'P')
			return SetPurgeTimeStampFromDuration(timestamp);
		else
			return SetPurgeTimeStampFromDate(timestamp);
	}

	const boost::posix_time::ptime & TaskContext::GetPurgeTimeStamp() const
	{
		return purgeTimeStamp_;
	}

	const boost::posix_time::ptime & TaskContext::GetRestoreTimeStamp() const
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
		//case keeper::Task::List:
		case keeper::Task::DumpDB:
			return GetSourceDirectory() + MAIN_DB_FILE;
		default:
			return std::wstring();
		}
	}

	void keeper::TaskContext::DisplayTaskConfig() const
	{
		LOG_VERBOSE() << "Compression Level = " << CompressionLevel << std::endl;
		LOG_VERBOSE() << "Encrypted = " << (!DbPassword.empty() ? "true" : "false") << std::endl;
		LOG_VERBOSE() << "File names encrypted = " << (isEncodeFileNames_ ? "true" : "false") << std::endl;
	}

	bool TaskContext::OpenDatabase(bool LoadConfig)
	{
		using namespace std;
		wstring dbFullPath = GenerateDbPath();

		if (!boost::filesystem::exists(dbFullPath))
		{
			//no DB exists
			return false;
		}
		string dbNameUTF8 = keeper::WstringToUTF8(dbFullPath);

		eventsDb_.set_flags(DB_DUPSORT);
		eventsDb_.set_dup_compare(DbEventsCompare);

		try
		{
			if (!DbPassword.empty())
			{
				if (DbKey.empty())
					DbKey = PasswordToKey(DbPassword);

				eventsDb_.set_encrypt(DbKey.c_str(), DB_ENCRYPT_AES);
				configDb_.set_encrypt(DbKey.c_str(), DB_ENCRYPT_AES);
				secretsDb_.set_encrypt(DbKey.c_str(), DB_ENCRYPT_AES);
			}

			//open existing
			eventsDb_.open(nullptr,
				dbNameUTF8.c_str(),
				EVENTS_DB_TABLE,
				DB_HASH,
				0,
				0);

			configDb_.open(nullptr,
				dbNameUTF8.c_str(),
				CONFIG_DB_TABLE,
				DB_HASH,
				0,
				0);

			secretsDb_.open(nullptr,
				dbNameUTF8.c_str(),
				SECRETS_DB_TABLE,
				DB_HASH,
				0,
				0);
		}
		catch (DbException)
		{
			LOG_FATAL() << "Database open failed" << endl;
			throw;
		}

		if (LoadConfig)
		{
			if (!GetConfigValueDWord(PARAM_COMPRESSION, CompressionLevel) ||
				!GetConfigValueBinaryArr(PARAM_FILES_ENCODE_KEY, FileEncodeKey_, crypto_stream_chacha20_KEYBYTES))
				throw;
			
			DWORD tmpVal = 0;
			GetConfigValueDWord(PARAM_NAMES_ENCODE, tmpVal);
			isEncodeFileNames_ = (tmpVal != 0);

			if (isEncodeFileNames_)
			{
				if (!GetConfigValueBinaryArr(PARAM_NAMES_ENCODE_KEY, NamesEncodeKey_, crypto_stream_chacha20_KEYBYTES) ||
					!GetConfigValueBinaryArr(PARAM_NAMES_ENCODE_NONCE, NamesEncodeNonce_, crypto_stream_chacha20_NONCEBYTES))
					throw;
			}

			DisplayTaskConfig();
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

		try
		{
			//workaround for Berkeley DB bug - can't create more than 2 DB's in same file at once
			Db tmpDb(nullptr, 0);
			tmpDb.set_flags(DB_DUPSORT);
			tmpDb.set_dup_compare(DbEventsCompare);

			if (!DbPassword.empty() && DbKey.empty())
				DbKey = PasswordToKey(DbPassword);

			if (!DbPassword.empty())
				tmpDb.set_encrypt(DbKey.c_str(), DB_ENCRYPT_AES);

			tmpDb.open(nullptr,
				dbNameUTF8.c_str(),
				EVENTS_DB_TABLE,
				DB_HASH,
				DB_CREATE /*| DB_EXCL*/,
				0);
			tmpDb.close(0);

			Db tmpDb2(nullptr, 0);
			if (!DbPassword.empty())
				tmpDb2.set_encrypt(DbKey.c_str(), DB_ENCRYPT_AES);
			tmpDb2.open(nullptr,
				dbNameUTF8.c_str(),
				CONFIG_DB_TABLE,
				DB_HASH,
				DB_CREATE /*| DB_EXCL*/,
				0);
			tmpDb2.close(0);

			Db tmpDb3(nullptr, 0);
			if (!DbPassword.empty())
				tmpDb3.set_encrypt(DbKey.c_str(), DB_ENCRYPT_AES);
			tmpDb3.open(nullptr,
				dbNameUTF8.c_str(),
				SECRETS_DB_TABLE,
				DB_HASH,
				DB_CREATE /*| DB_EXCL*/,
				0);
			tmpDb3.close(0);
		}
		catch (DbException)
		{
			LOG_FATAL() << "Database create failed" << endl;
			//eventsDb_.err(e.get_errno(), "Database create failed");
			throw;
		}

		if (OpenDatabase(false))
		{
			//save config to DB
			if (CompressionLevel > 9)
				CompressionLevel = 0;
			SetConfigValueDWord(PARAM_COMPRESSION, CompressionLevel);

			//generate key for files encryption
			if (!DbPassword.empty())
			{
				randombytes_buf(FileEncodeKey_, crypto_stream_chacha20_KEYBYTES);
				SetConfigValueBinaryArr(PARAM_FILES_ENCODE_KEY, FileEncodeKey_, crypto_stream_chacha20_KEYBYTES);
			}

			if (isEncodeFileNames_)
			{
				randombytes_buf(NamesEncodeKey_, crypto_stream_chacha20_KEYBYTES);
				randombytes_buf(NamesEncodeNonce_, crypto_stream_chacha20_NONCEBYTES);
				SetConfigValueBinaryArr(PARAM_NAMES_ENCODE_KEY, NamesEncodeKey_, crypto_stream_chacha20_KEYBYTES);
				SetConfigValueBinaryArr(PARAM_NAMES_ENCODE_NONCE, NamesEncodeNonce_, crypto_stream_chacha20_NONCEBYTES);
				SetConfigValueDWord(PARAM_NAMES_ENCODE, 1);
			}
			else
				SetConfigValueDWord(PARAM_NAMES_ENCODE, 0);

			DisplayTaskConfig();

			return true;
		}
		else
			return false;
	}

	bool TaskContext::CompressDatabase()
	{
		DB_COMPACT cData = { 0 };
		cData.compact_fillpercent = 80;
		eventsDb_.compact(nullptr, nullptr, nullptr, &cData, DB_FREELIST_ONLY, nullptr);
		secretsDb_.compact(nullptr, nullptr, nullptr, &cData, DB_FREELIST_ONLY, nullptr);
		return true;
	}

	void TaskContext::CloseDatabase()
	{
		eventsDb_.close(0);
		configDb_.close(0);
		secretsDb_.close(0);
	}

	Db & keeper::TaskContext::GetMainDB()
	{
		return eventsDb_;
	}

	bool TaskContext::SetConfigValueDWord(const std::string & name, DWORD val)
	{
		Dbt key = Dbt((void*)name.c_str(), name.length());
		Dbt data = Dbt((void*)&val, sizeof(DWORD));
		configDb_.put(nullptr, &key, &data, DB_OVERWRITE_DUP);

		return true;
	}

	bool TaskContext::GetConfigValueDWord(const std::string & name, DWORD& val)
	{
		Dbt key((void*)name.c_str(), name.length());
		Dbt data;

		int result = configDb_.get(nullptr, &key, &data, 0);
		if (result != 0)
		{
			LOG_ERROR() << "Can't read value from DB: " << name << std::endl;
			return false;
		}
		if (data.get_size() != sizeof(DWORD))
			return false;
		val = *(DWORD*)data.get_data();
		return true;
	}

	bool TaskContext::SetConfigValueBinaryArr(const std::string & name, byte * val, unsigned int len)
	{
		Dbt key = Dbt((void*)name.c_str(), name.length());
		Dbt data = Dbt(val, len);
		configDb_.put(nullptr, &key, &data, DB_OVERWRITE_DUP);

		return true;
	}
	bool TaskContext::GetConfigValueBinaryArr(const std::string & name, byte * val, unsigned int len)
	{
		Dbt key((void*)name.c_str(), name.length());
		Dbt data;

		int result = configDb_.get(nullptr, &key, &data, 0);
		if (result != 0)
		{
			LOG_ERROR() << "Can't read value from DB: " << name << std::endl;
			return false;
		}
		if (data.get_size() != len)
			return false;
		memcpy(val, data.get_data(), len);

		return true;
	}
}