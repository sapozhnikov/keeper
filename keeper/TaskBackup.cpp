#include "stdafx.h"
#include <algorithm>
#include "GlobalUtils.h"
#include "TaskBackup.h"
#include <boost\scope_exit.hpp>
#include "ConsoleLogger.h"
#include "FileIO.h"

using namespace std;
using namespace boost::filesystem;
using namespace ConsoleLogger;

TaskBackup::TaskBackup(keeper::TaskContext& ctx) :
	ctx_(ctx),
	dirIterator_(ctx_.GetSourceDirectory()),
	initialDirectory_(ctx_.GetSourceDirectory()),
	skipPathCharsCount_(ctx_.GetSourceDirectory().length())
{
	currentTimeStamp_ = boost::posix_time::microsec_clock::local_time();
	timestamp64_ = keeper::ConvertPtimeToMillisec(currentTimeStamp_);
}

TaskBackup::~TaskBackup()
{
}

void TaskBackup::Run()
{
	boost::system::error_code errorCode;
	//bool isFirstBackup = false;
	
	bool isFirstBackup = false;
	if (!ctx_.OpenDatabase())
	{
		LOG_INFO() << "Creating new repository" << std::endl;
		isFirstBackup = true;
		ctx_.CreateDatabase();
	}

	mirrorPath_ = ctx_.GetDestinationDirectory() + MIRROR_SUB_DIR;
	if (!boost::filesystem::exists(mirrorPath_))
	{
		//boost::filesystem::create_directory(mirrorPath_);
		if (!keeper::FileIO::CreateDir(mirrorPath_))
		{
			LOG_FATAL() << "Can't continue" << std::endl;
			return;
		}
	}

	storeOldPath_ = ctx_.GetDestinationDirectory() + keeper::PTimeToWstringSafeSymbols(currentTimeStamp_) + L"\\";

	ctx_.GetMainDB().cursor(NULL, &mainCursor_, 0);
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (mainCursor_ != nullptr)
			mainCursor_->close();

		ctx_.CloseDatabase();
	};


	//start copy process
	wstring relativePath/*, sourceFullPath*/;
	//wstring sourceDir = config_.GetSourceDirectory();
	while (true)
	{
		if (dirIterator_ == recursive_directory_iterator())
			break;
		const wstring& sourceFullPath = dirIterator_->path().wstring();
		relativePath = sourceFullPath.substr(skipPathCharsCount_, skipPathCharsCount_ - sourceFullPath.length());

		if (is_directory(dirIterator_->path()))
		{
			//directory handling
			if (isFirstBackup)
			{
				//copy_directory(sourceFullPath, mirrorPath_ + relativePath, errorCode); //check!
				if (keeper::FileIO::CopyDir(sourceFullPath, mirrorPath_ + relativePath))
					DbAddEvent(sourceFullPath, relativePath, FileEventType::Added);
			}
			else
			{
				//do not move existing folder to storage

				//check if exists
				if (!boost::filesystem::exists(mirrorPath_ + relativePath, errorCode)) //or DB check?
				{
					//copy to mirror
					//copy_directory(sourceFullPath, mirrorPath_ + relativePath, errorCode); //check!
					if (keeper::FileIO::CopyDir(sourceFullPath, mirrorPath_ + relativePath))
						DbAddEvent(sourceFullPath, relativePath, FileEventType::Changed);
				}
			}
		}
		else
		{
			//file handling
			if (isFirstBackup)
			{
				//just copy the file
				if (keeper::FileIO::CopySingleFile(sourceFullPath, mirrorPath_ + relativePath)) //fail if exists?
					DbAddEvent(sourceFullPath, relativePath, FileEventType::Added);
			}
			else
			{
				//compare file attrs with DB record
				TaskBackup::FileState fileState = GetFileState(relativePath);
				switch (fileState)
				{
				case FileState::New:
					//just copy the file
					if (keeper::FileIO::CopySingleFile(sourceFullPath, mirrorPath_ + relativePath))
						DbAddEvent(sourceFullPath, relativePath, FileEventType::Added);
					break;

				case FileState::Same:
					//do nothing
					break;

				case FileState::Changed:
					//move to storage
					CheckAndMakeStoreOldFolder();
					if (keeper::FileIO::MoveSingleFile(mirrorPath_ + relativePath, storeOldPath_ + relativePath))
					{
						//copy new version
						if (keeper::FileIO::CopySingleFile(sourceFullPath, mirrorPath_ + relativePath))
							DbAddEvent(sourceFullPath, relativePath, FileEventType::Changed);
					}
					break;
				}
			}
		}
		dirIterator_++;
	}

	//search for deleted files
	if (!isFirstBackup)
	{
		ctx_.GetMainDB().cursor(NULL, &mainCursor_, 0);
		Dbt keyMain, dataMain;
		int result;
		vector<std::wstring> filesToDelete;

		result = mainCursor_->get(&keyMain, &dataMain, DB_FIRST);
		keeper::CheckDbResult(result);

		while (true)
		{
			if (result == DB_NOTFOUND) //end of DB
				break;

			if (result == 0)
			{
				//move to the last record with the same key
				while (true)
				{
					result = mainCursor_->get(&keyMain, &dataMain, DB_NEXT_DUP);
					keeper::CheckDbResult(result);
					if (result == DB_NOTFOUND)
						break;
				}
				DbFileEvent* pEvent = static_cast<DbFileEvent*>(dataMain.get_data());
				
				if ((static_cast<FileEventType>(pEvent->FileEvent) != FileEventType::Deleted) &&
					(pEvent->EventTimeStamp != timestamp64_))
				{
					relativePath = keeper::DbtToWstring(keyMain);
					//check if file exists
					if (!boost::filesystem::exists(ctx_.GetSourceDirectory() + relativePath))
					{
						//if add DB record now, the cursor will skip some records, so keep names for later
						filesToDelete.push_back(relativePath);
					}
				}
			}
			result = mainCursor_->get(&keyMain, &dataMain, DB_NEXT_NODUP);
			keeper::CheckDbResult(result);
		}//while

		//the easyest way to move directories with subdirs and files in right order is sort them
		sort(filesToDelete.begin(), filesToDelete.end());

		for (const auto& fName : filesToDelete)
		{
			//move from mirror folder to delta folder
			auto deletedFile = mirrorPath_ + fName;
			if (boost::filesystem::exists(deletedFile))
			{
				CheckAndMakeStoreOldFolder();
				keeper::FileIO::MoveSingleFile(deletedFile, storeOldPath_ + fName);
			}

			//write event to DB
			DbAddEvent(std::wstring(), fName, FileEventType::Deleted);
		}
	} //if (!isFirstBackup)
}

//lazy Folder creating
void TaskBackup::CheckAndMakeStoreOldFolder()
{
	if (!isStoreOldFolderCreated)
	{
		keeper::FileIO::CreateDir(storeOldPath_);
		isStoreOldFolderCreated = true;
	}
}

void TaskBackup::DbAddEvent(const std::wstring& fullPath, const std::wstring& relativePath, FileEventType ev)
{
	//relative path as the key
	Dbt key = keeper::WstringToDbt(relativePath);
																					   
	//properties as the value
	DbFileEvent fileDbEvent;

	if (ev != FileEventType::Deleted)
	{
		if (!GetFileAttributesEx(fullPath.c_str(), GetFileExInfoStandard, &fileDbEvent.FileAttributes))
			throw std::exception("Can't get file attributes");
	}
	else
	{
		fileDbEvent.FileAttributes = { 0 };
	}

	fileDbEvent.EventTimeStamp = timestamp64_;
	fileDbEvent.FileEvent = static_cast<byte>(ev);

	Dbt data((void*)&fileDbEvent, sizeof(DbFileEvent));

	ctx_.GetMainDB().put(nullptr, &key, &data, 0);
}

TaskBackup::FileState TaskBackup::GetFileState(const std::wstring& relativePath)
{
	//wstring fileToCheck = mirrorPath_ + relativePath;
	
	//check if exists in mirror folder, ignore DB, files are more important
	if (!boost::filesystem::exists(mirrorPath_ + relativePath))
		return TaskBackup::FileState::New;
	
	//compare attributes from source file with DB record

	//get the last event
	Dbt key = keeper::WstringToDbt(relativePath);
	Dbt data;

	int result = mainCursor_->get(&key, &data, DB_SET);
	keeper::CheckDbResult(result);
	if (result == DB_NOTFOUND)
		return TaskBackup::FileState::New; //something wrong

	while (true)
	{
		result = mainCursor_->get(&key, &data, DB_NEXT_DUP);
		keeper::CheckDbResult(result);
		if (result == DB_NOTFOUND)
			break;
	}
	DbFileEvent* pEvent = static_cast<DbFileEvent*>(data.get_data()); //TODO: data size check

	if (static_cast<FileEventType>(pEvent->FileEvent) == FileEventType::Deleted)
		return TaskBackup::FileState::New;

	if (pEvent->EventTimeStamp == timestamp64_)
		return TaskBackup::FileState::Same; //something wrong

	const wstring& sourceFullPath = dirIterator_->path().wstring();

	WIN32_FILE_ATTRIBUTE_DATA sourceAttributes;

	if (!GetFileAttributesEx(sourceFullPath.c_str(), GetFileExInfoStandard, &sourceAttributes))
		throw std::exception("Can't get file attributes");

	if ((sourceAttributes.nFileSizeHigh == pEvent->FileAttributes.nFileSizeHigh) &&
		(sourceAttributes.nFileSizeLow == pEvent->FileAttributes.nFileSizeLow) &&
		(sourceAttributes.ftLastWriteTime.dwHighDateTime == pEvent->FileAttributes.ftLastWriteTime.dwHighDateTime) &&
		(sourceAttributes.ftLastWriteTime.dwLowDateTime == pEvent->FileAttributes.ftLastWriteTime.dwLowDateTime))
		return TaskBackup::FileState::Same;
	else
		return TaskBackup::FileState::Changed;
}
