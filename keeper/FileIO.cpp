#include "stdafx.h"
#include "FileIO.h"
#include "ConsoleLogger.h"

using namespace ConsoleLogger;
using namespace boost::filesystem;

namespace keeper
{
namespace FileIO
{
	std::string StrAnsiToOEM(const std::string& str)
	{
		std::unique_ptr<char[]> buff = std::make_unique<char[]>(str.length() + 1);
		CharToOemBuffA(str.c_str(), buff.get(), DWORD(str.length() + 1));
		std::string result(buff.get());
		return result;
	}

	std::string GetLastErrorAsString()
	{
		//Get the error message, if any.
		DWORD errorMessageID = ::GetLastError();
		if (errorMessageID == 0)
			return std::string(); //No error message has been recorded

		LPSTR messageBuffer = nullptr;
		size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

		std::string message(messageBuffer, size);

		//Free the buffer.
		LocalFree(messageBuffer);

		return StrAnsiToOEM(message);
	}

	bool CopySingleFile(const boost::filesystem::path& source, const boost::filesystem::path& destination, bool failIfExists)
	{
		boost::system::error_code errorCode;

		LOG_VERBOSE() << "Copying file from " << source.wstring() << " to " << destination.wstring() << std::endl;

		boost::filesystem::create_directories(boost::filesystem::path(destination).parent_path());
		if (failIfExists)
			boost::filesystem::copy_file(source, destination, boost::filesystem::copy_option::fail_if_exists, errorCode);
		else
			boost::filesystem::copy_file(source, destination, errorCode);

		if (!errorCode)
			return true;
		else
		{
			LOG_ERROR() << "Can't copy file from " << source.wstring() << " to " << destination.wstring() << " : " << StrAnsiToOEM(errorCode.message()) << std::endl;
			return false;
		}
	}

	//remove R/O attribute from file or directory with subdirs and files
	bool RemoveReadOnlyAttribute(const boost::filesystem::path& fileName)
	{
		boost::system::error_code errorCode;

		permissions(fileName, status(fileName).permissions() | owner_write | group_write | others_write, errorCode);
		if (errorCode)
		{
			LOG_ERROR() << "Can't change file attributes: " << fileName.wstring() << " : " << StrAnsiToOEM(errorCode.message()) << std::endl;
			return false;
		}

		if (is_directory(fileName))
		{
			recursive_directory_iterator dirIterator(fileName);
			while (true)
			{
				if (dirIterator == recursive_directory_iterator())
					break;
				permissions(dirIterator->path(), status(dirIterator->path()).permissions() | owner_write | group_write | others_write, errorCode);
				if (errorCode)
				{
					LOG_ERROR() << "Can't change file attributes: " << fileName.wstring() << " : " << StrAnsiToOEM(errorCode.message()) << std::endl;
					return false;
				}

				dirIterator++;
			}
		}
		return true;
	}

	bool MoveSingleFile(const boost::filesystem::path& source, const boost::filesystem::path& destination)
	{
		boost::system::error_code errorCode;

		LOG_VERBOSE() << "Moving file from " << source.wstring() << " to " << destination.wstring() << std::endl;

		boost::filesystem::create_directories(boost::filesystem::path(destination).parent_path(), errorCode);
		if (errorCode)
		{
			LOG_ERROR() << "Can't create directories for " << destination.wstring() << " : " << StrAnsiToOEM(errorCode.message()) << std::endl;
			//return false;
		}

		boost::filesystem::rename(source, destination, errorCode);
		if (errorCode)
		{
			LOG_ERROR() << "Can't move file from " << source.wstring() << " to " << destination.wstring() << " : " << StrAnsiToOEM(errorCode.message()) << std::endl;
			return false;
		}
		return true;
	}

	bool RemoveDir(const boost::filesystem::path& dir)
	{
		boost::system::error_code errorCode;

		LOG_VERBOSE() << "Deleting directory " << dir.wstring() << std::endl;

		RemoveReadOnlyAttribute(dir);
		boost::filesystem::remove_all(dir, errorCode);
		if (errorCode)
		{
			LOG_ERROR() << "Can't delete directory " << dir.wstring() << " : " << StrAnsiToOEM(errorCode.message()) << std::endl;
			return false;
		}

		return true;
	}

	bool CreateDir(const boost::filesystem::path& dir)
	{
		boost::system::error_code errorCode;

		LOG_VERBOSE() << "Creating directory " << dir.wstring() << std::endl;

		//boost::filesystem::create_directories(dir.parent_path()); //!
		boost::filesystem::create_directories(dir, errorCode);
		if (!errorCode)
			return true;
		else
		{
			LOG_ERROR() << "Can't create directory " << dir.wstring() << " : " << StrAnsiToOEM(errorCode.message()) << std::endl;
			return false;
		}
	}

	bool CopyDir(const boost::filesystem::path& source, const boost::filesystem::path& destination)
	{
		boost::system::error_code errorCode;

		LOG_VERBOSE() << "Copying directory from " << source.wstring() << " to " << destination.wstring() << std::endl;

		boost::filesystem::copy_directory(source, destination, errorCode);
		if (!errorCode)
			return true;
		else
		{
			LOG_ERROR() << "Can't copy directory from " << source.wstring() << " to " << destination.wstring() << " : " << StrAnsiToOEM(errorCode.message()) << std::endl;
			return false;
		}
	}
}
}