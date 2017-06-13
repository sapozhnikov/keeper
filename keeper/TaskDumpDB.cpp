#include "stdafx.h"
#include "TaskDumpDB.h"
#include "GlobalUtils.h"
#include <boost\scope_exit.hpp>

using namespace keeper;
using namespace ConsoleLogger;

TaskDumpDB::TaskDumpDB(keeper::TaskContext & ctx) :
	ctx_(ctx)
{
}

TaskDumpDB::~TaskDumpDB()
{
}

bool TaskDumpDB::Run()
{
	if (!ctx_.OpenDatabase())
	{
		LOG_FATAL() << "Can't open database" << std::endl;
		return false;
	}

	Dbc* mainCursor = nullptr;
	Dbt keyMain, dataMain;
	ctx_.GetMainDB().cursor(NULL, &mainCursor, 0);

	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (mainCursor != nullptr)
			mainCursor->close();

		ctx_.CloseDatabase();
	};

	int result;
	std::wostringstream s;

	while (true)
	{
		result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_NODUP);
		if (result)
			break;

		s << std::endl << DbtToWstring(keyMain);
		CONSOLE() << s.str() << std::endl;
		s.str(std::wstring());
		s.clear();

		while (true)
		{
			if (dataMain.get_size() != sizeof(DbFileEvent))
				throw std::exception("Corrupted DB data");

			//events
			DbFileEvent const* pEvent = static_cast<DbFileEvent const*>(dataMain.get_data());

			s << PTimeToWstring(ConvertMillisecToPtime(pEvent->EventTimeStamp)) << L" : ";
			switch (static_cast<FileEventType>(pEvent->FileEvent))
			{
			case FileEventType::Deleted:
				s << "DELETED";
				break;
			case FileEventType::Added:
				s << "ADDED";
				break;
			case FileEventType::Changed:
				s << "CHANGED";
				break;
			default:
				s << "UNKNOWN";
				break;
			}

			//size
			ULONGLONG fileSize = static_cast<ULONGLONG>(pEvent->FileAttributes.nFileSizeHigh) * (static_cast<ULONGLONG>(MAXDWORD) + 1) + pEvent->FileAttributes.nFileSizeLow;
			s << L"\nSize: ";
			s << std::dec << fileSize;

			//file attributes
			auto fileAttr = pEvent->FileAttributes.dwFileAttributes;
			s << L" Attributes: ";
			if (fileAttr & FILE_ATTRIBUTE_READONLY)
				s << L"READONLY ";
			if (fileAttr & FILE_ATTRIBUTE_HIDDEN)
				s << L"HIDDEN ";
			if (fileAttr & FILE_ATTRIBUTE_SYSTEM)
				s << L"SYSTEM ";
			if (fileAttr & FILE_ATTRIBUTE_DIRECTORY)
				s << L"DIRECTORY ";
			if (fileAttr & FILE_ATTRIBUTE_ARCHIVE)
				s << L"ARCHIVE ";
			if (fileAttr & FILE_ATTRIBUTE_DEVICE)
				s << L"DEVICE ";
			if (fileAttr & FILE_ATTRIBUTE_NORMAL)
				s << L"NORMAL ";
			if (fileAttr & FILE_ATTRIBUTE_TEMPORARY)
				s << L"TEMPORARY ";
			if (fileAttr & FILE_ATTRIBUTE_SPARSE_FILE)
				s << L"SPARSE_FILE ";
			if (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT)
				s << L"REPARSE_POINT ";
			if (fileAttr & FILE_ATTRIBUTE_COMPRESSED)
				s << L"COMPRESSED ";
			if (fileAttr & FILE_ATTRIBUTE_OFFLINE)
				s << L"OFFLINE ";
			if (fileAttr & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
				s << L"NOT_CONTENT_INDEXED ";
			if (fileAttr & FILE_ATTRIBUTE_ENCRYPTED)
				s << L"ENCRYPTED ";
#ifndef _USING_V110_SDK71_
			if (fileAttr & FILE_ATTRIBUTE_INTEGRITY_STREAM)
				s << L"INTEGRITY_STREAM ";
			if (fileAttr & FILE_ATTRIBUTE_NO_SCRUB_DATA)
				s << L"NO_SCRUB_DATA ";
			if (fileAttr & FILE_ATTRIBUTE_EA)
				s << L"EA ";
#endif
			if (fileAttr & FILE_ATTRIBUTE_VIRTUAL)
				s << L"VIRTUAL";
			CONSOLE() << s.str() << std::endl;
			s.str(std::wstring());
			s.clear();

			//dates
			SYSTEMTIME systemTime;
			FILETIME fileTime;

			fileTime = pEvent->FileAttributes.ftCreationTime;
			FileTimeToLocalFileTime(&fileTime, &fileTime);
			FileTimeToSystemTime(&fileTime, &systemTime);
			s << L"Created:" << SystemTimeToWstring(systemTime);

			fileTime = pEvent->FileAttributes.ftLastAccessTime;
			FileTimeToLocalFileTime(&fileTime, &fileTime);
			FileTimeToSystemTime(&fileTime, &systemTime);
			s << L" Last access:" << SystemTimeToWstring(systemTime);

			fileTime = pEvent->FileAttributes.ftLastWriteTime;
			FileTimeToLocalFileTime(&fileTime, &fileTime);
			FileTimeToSystemTime(&fileTime, &systemTime);
			s << L" Write time:" << SystemTimeToWstring(systemTime);
			CONSOLE() << s.str() << std::endl;
			s.str(std::wstring());
			s.clear();

			result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_DUP);
			if (result)
				break;
		}
	}
	
	return true;
}
