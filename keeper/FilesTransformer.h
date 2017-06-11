#pragma once
#include "TaskContext.h"
#include "PathEncoder.h"
#include "ChaChaFilter.h"

namespace keeper
{
	//class implements encrypting & decrypting of files & filenames
	class FilesTransformer
	{
	public:
		FilesTransformer() = delete;
		explicit FilesTransformer(const TaskContext& ctx);
		~FilesTransformer();
		std::wstring GetTransformedName(const std::wstring& name, bool isDirectory);
		std::wstring GetOriginalName(const std::wstring& name, bool isDirectory);
		bool TransformFile(const std::wstring& pathFrom, const std::wstring& pathTo/*, bool isMoveFile = false*/);
		bool RestoreFile(const std::wstring& pathFrom, const std::wstring& pathTo);
	private:
		const TaskContext& ctx_;
		bool isFileCompressed;
		bool isFileEncrypted;
		bool isFileNamesEncrypted;
		PathEncoder pathEncoder;
		byte FileEncryptKey[crypto_stream_chacha20_KEYBYTES];
		byte FileEncryptNonce[crypto_stream_chacha20_NONCEBYTES];
	};
}