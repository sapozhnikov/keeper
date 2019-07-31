#pragma once
#include <boost\filesystem.hpp>

namespace keeper
{
namespace FileIO
{
	bool CopySingleFile(const boost::filesystem::path& source, const boost::filesystem::path& destination, bool failIfExists = false);
	bool MoveSingleFile(const boost::filesystem::path& source, const boost::filesystem::path& destination);
	bool RemoveDir(const boost::filesystem::path& dir);
	bool CreateDir(const boost::filesystem::path& dir);
	bool CopyDir(const boost::filesystem::path& source, const boost::filesystem::path& destination);
}
}