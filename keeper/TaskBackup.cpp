#include "stdafx.h"
#include <algorithm>
#include "GlobalUtils.h"
#include "TaskBackup.h"
#include <boost\scope_exit.hpp>
#include "ConsoleLogger.h"
#include "FileIO.h"
#include "FilesTransformer.h"

using namespace std;
using namespace boost::filesystem;
using namespace ConsoleLogger;

TaskBackup::TaskBackup(keeper::TaskContext& ctx) :
	ctx_(ctx),
	dirIterator_(ctx_.GetSourceDirectory()),
	//initialDirectory_(ctx_.GetSourceDirectory()),
	skipPathCharsCount_((unsigned int)ctx_.GetSourceDirectory().length())
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
			LOG_FATAL() << "Can't create directory " << mirrorPath_ << std::endl;
			return;
		}
	}

	storeOldPath_ = ctx_.GetDestinationDirectory() + keeper::PTimeToWstringSafeSymbols(currentTimeStamp_) + L"\\";

	ctx_.GetMainDB().cursor(nullptr, &mainCursor_, 0);
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (mainCursor_ != nullptr)
			mainCursor_->close();

		ctx_.CloseDatabase();
	};

	keeper::FilesTransformer transformer(ctx_);
	keeper::WildCardNameChecker& namesChecker = ctx_.NamesChecker;
	bool NamesFilteringEnabled = namesChecker.IsFilteringEnabled;

	//start copy process
	boost::system::error_code errCode;
	while (true)
	{
		if (dirIterator_ == recursive_directory_iterator())
			break;
		const wstring& sourceFullPath = dirIterator_->path().wstring();
		relativePath_ = sourceFullPath.substr(skipPathCharsCount_, skipPathCharsCount_ - sourceFullPath.length());

		bool isDirectory = is_directory(dirIterator_->path(), errCode);
		if (errCode.value() != boost::system::errc::success)
		{
			LOG_ERROR() << "Can't get info about " << sourceFullPath << std::endl;
			++dirIterator_;
			continue;
		}

		if (NamesFilteringEnabled)
		{
			if (!namesChecker.IsFitPattern(relativePath_))
			{
				if (isDirectory)
					dirIterator_.no_push();
				++dirIterator_;
				continue;
			}
		}

		//skip symlinks
		bool isSymlink = is_symlink(dirIterator_->path(), errCode);
		if (errCode.value() != boost::system::errc::success)
		{
			LOG_ERROR() << "Can't get info about " << sourceFullPath << std::endl;
			++dirIterator_;
			continue;
		}

		if (isSymlink)
		{
			LOG_INFO() << "Skipping symlink \"" << dirIterator_->path().wstring() << "\"" << endl;
			if (isDirectory)
				dirIterator_.no_push();
			++dirIterator_;
			continue;
		}

		transformedPath_ = transformer.GetTransformedName(relativePath_, isDirectory);

		if (isDirectory)
		{
			//directory handling
			if (isFirstBackup)
			{
				if (keeper::FileIO::CopyDir(sourceFullPath, mirrorPath_ + transformedPath_))
					DbAddEvent(sourceFullPath, relativePath_, FileEventType::Added);
			}
			else
			{
				//do not move existing folder to storage

				//check if exists
				if (!boost::filesystem::exists(mirrorPath_ + transformedPath_, errorCode)) //or DB check?
				{
					//copy to mirror
					if (keeper::FileIO::CopyDir(sourceFullPath, mirrorPath_ + transformedPath_))
						DbAddEvent(sourceFullPath, relativePath_, FileEventType::Changed);
				}
			}
		}
		else
		{
			//file handling
			if (isFirstBackup)
			{
				//just copy the file
				if (transformer.TransformFile(sourceFullPath, mirrorPath_ + transformedPath_))
					DbAddEvent(sourceFullPath, relativePath_, FileEventType::Added);
			}
			else
			{
				//compare file attrs with DB record
				TaskBackup::FileState fileState = GetFileState();
				switch (fileState)
				{
				case FileState::New:
					//just copy the file
					if (transformer.TransformFile(sourceFullPath, mirrorPath_ + transformedPath_))
						DbAddEvent(sourceFullPath, relativePath_, FileEventType::Added);
					break;

				case FileState::Same:
					//do nothing
					break;

				case FileState::Changed:
					//move to storage
					if (keeper::FileIO::MoveSingleFile(mirrorPath_ + transformedPath_, storeOldPath_ + transformedPath_))
					{
						//copy new version
						if (transformer.TransformFile(sourceFullPath, mirrorPath_ + transformedPath_))
							DbAddEvent(sourceFullPath, relativePath_, FileEventType::Changed);
					}
					break;
				default:
					break;
				}
			}
		}
		dirIterator_.increment(errCode);
		if (errCode) //failed to enter directory
		{
			if (isDirectory)
			{
				LOG_ERROR() << "No access to " << dirIterator_->path().wstring() << std::endl;
				dirIterator_.no_push();
				++dirIterator_;
			}
			else
				throw std::runtime_error(errCode.message());
		}
	}

	//search for deleted files
	if (!isFirstBackup)
	{
		ctx_.GetMainDB().cursor(nullptr, &mainCursor_, 0);
		Dbt keyMain, dataMain;
		//<is Directory, relative path>
		vector<tuple<bool, std::wstring>> filesToMove;

		int result = mainCursor_->get(&keyMain, &dataMain, DB_FIRST);
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
					relativePath_ = keeper::DbtToWstring(keyMain);
					//check if file exists
					if (!boost::filesystem::exists(ctx_.GetSourceDirectory() + relativePath_))
					{
						bool isDirectory = (pEvent->FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
						//if add DB record now, the cursor will skip some records, so keep names for later
						filesToMove.push_back(tuple<bool, std::wstring>(isDirectory, relativePath_));
					}
				}
			}
			result = mainCursor_->get(&keyMain, &dataMain, DB_NEXT_NODUP);
			keeper::CheckDbResult(result);
		}//while

		//the easyest way to move directories with subdirs and files in right order is sort them
		sort(filesToMove.begin(), filesToMove.end(),
			[](const auto& t1, const auto& t2){
			const wstring& path1 = std::get<wstring>(t1);
			const wstring& path2 = std::get<wstring>(t2);
			if (path1.length() == path2.length())
				return std::get<wstring>(t1) < std::get<wstring>(t2);
			else
				return path1.length() < path2.length();
			});

		for (const auto& fileInfo : filesToMove)
		{
			//move from mirror folder to delta folder
			transformedPath_ = transformer.GetTransformedName(std::get<wstring>(fileInfo), std::get<bool>(fileInfo));
			auto deletedFile = mirrorPath_ + transformedPath_;
			if (boost::filesystem::exists(deletedFile))
			{
				keeper::FileIO::MoveSingleFile(deletedFile, storeOldPath_ + transformedPath_);
			}

			//write event to DB
			DbAddEvent(std::wstring(), std::get<wstring>(fileInfo), FileEventType::Deleted);
		}
	} //if (!isFirstBackup)
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

TaskBackup::FileState TaskBackup::GetFileState()
{
	//check if exists in mirror folder, ignore DB, files are more important
	if (!boost::filesystem::exists(mirrorPath_ + transformedPath_))
		return TaskBackup::FileState::New;
	
	//compare attributes from source file with DB record

	//get the last event
	Dbt key = keeper::WstringToDbt(relativePath_);
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
