#include "stdafx.h"
#include "FilesTransformer.h"
#include "CommonDefinitions.h"
#include "FileIO.h"
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

using namespace ConsoleLogger;

namespace keeper
{
	FilesTransformer::FilesTransformer(const TaskContext & ctx) :
		//pathEncoder(ctx.NamesEncodeKey_, ctx.NamesEncodeNonce_),
		pathEncoder(ctx),
		ctx_(ctx)
	{
		isFileCompressed = (ctx.CompressionLevel != 0);
		isFileEncrypted = !ctx.DbPassword.empty();
		isFileNamesEncrypted = ctx.isEncryptedFileNames;
		memcpy(FileEncryptKey, ctx.FileEncodeKey_, crypto_stream_chacha20_KEYBYTES);
	}
	
	FilesTransformer::~FilesTransformer()
	{
		sodium_memzero(FileEncryptKey, crypto_stream_chacha20_KEYBYTES);
	}

	std::wstring FilesTransformer::GetTransformedName(const std::wstring & name, bool isDirectory)
	{
		if (isFileNamesEncrypted)
			return pathEncoder.encode(name);
		if (!isDirectory)
		{
			if (isFileEncrypted)
				return name + NAME_SUFFIX_ENCRYPTED;
			if (isFileCompressed)
				return name + NAME_SUFFIX_COMPRESSED;
		}
		return name;
	}
	
	std::wstring FilesTransformer::GetOriginalName(const std::wstring & name, bool isDirectory)
	{
		auto const static LenSuffixEncrypted = wcslen(NAME_SUFFIX_ENCRYPTED);
		auto const static LenSuffixCompressed = wcslen(NAME_SUFFIX_COMPRESSED);

		if (isFileNamesEncrypted)
			return pathEncoder.decode(name);

		if (!isDirectory)
		{
			if (isFileEncrypted)
			{
				if ((name.length() < LenSuffixEncrypted) ||
					(name.substr(name.length() - LenSuffixEncrypted, name.length()) != NAME_SUFFIX_ENCRYPTED))
					throw std::exception("Bad file name");
				return name.substr(0, name.length() - LenSuffixEncrypted);
			}
			else
				if (isFileCompressed)
				{
					if ((name.length() < LenSuffixCompressed) ||
						(name.substr(name.length() - LenSuffixCompressed, name.length()) != NAME_SUFFIX_COMPRESSED))
						throw std::exception("Bad file name");
					return name.substr(0, name.length() - LenSuffixCompressed);
				}
		}

		return name;
	}
	
	bool FilesTransformer::TransformFile(const std::wstring & pathFrom, const std::wstring & pathTo)
	{
		if (!isFileCompressed && !isFileEncrypted)
		{
			return FileIO::CopySingleFile(pathFrom, pathTo);
		}
		else
		{
			LOG_VERBOSE() << "Storing file " << pathFrom << " to " << pathTo << std::endl;

			std::ifstream inFile(pathFrom, std::ios::binary | std::ios::in);
			if (!inFile.is_open())
				return false;

			boost::filesystem::create_directories(boost::filesystem::path(pathTo).parent_path());
			std::ofstream outFile(pathTo, std::ios::binary | std::ios::out | std::ios::trunc);
			if (isFileEncrypted)
			{
				//random nonce for each file
				randombytes_buf(FileEncryptNonce, crypto_stream_chacha20_NONCEBYTES);
				outFile.write(reinterpret_cast<const char*>(FileEncryptNonce), crypto_stream_chacha20_NONCEBYTES);
			}
			boost::iostreams::filtering_streambuf<boost::iostreams::output> outStream;
			bzip2_params bz2Param;
			if (isFileCompressed)
			{
				bz2Param.block_size = ctx_.CompressionLevel;
				outStream.push(boost::iostreams::bzip2_compressor(bz2Param));
			}
			if (isFileEncrypted)
				outStream.push(ChaCha20filter(FileEncryptKey, FileEncryptNonce));
			outStream.push(outFile);

			boost::iostreams::copy(inFile, outStream);
			
			return true;
		}
	}
	
	bool FilesTransformer::RestoreFile(const std::wstring & pathFrom, const std::wstring & pathTo)
	{
		if (!isFileCompressed && !isFileEncrypted)
		{
			return FileIO::CopySingleFile(pathFrom, pathTo);
		}
		else
		{
			LOG_VERBOSE() << "Restoring file from " << pathFrom << " to " << pathTo << std::endl;

			std::ifstream inFile(pathFrom, std::ios::binary | std::ios::in);
			if (!inFile.is_open())
				return false;

			if (isFileEncrypted)
			{
				inFile.read(reinterpret_cast<char*>(FileEncryptNonce), crypto_stream_chacha20_NONCEBYTES);
				if (inFile.gcount() < crypto_stream_chacha20_NONCEBYTES)
					return false;
			}
			boost::iostreams::filtering_streambuf<boost::iostreams::output> outStream;

			boost::filesystem::create_directories(boost::filesystem::path(pathTo).parent_path());
			std::ofstream outFile(pathTo, std::ios::binary | std::ios::out | std::ios::trunc);
			if (isFileEncrypted)
				outStream.push(ChaCha20filter(FileEncryptKey, FileEncryptNonce));
			if (isFileCompressed)
				outStream.push(boost::iostreams::bzip2_decompressor());
			outStream.push(outFile);

			boost::iostreams::copy(inFile, outStream);
			return true;
		}
	}
}