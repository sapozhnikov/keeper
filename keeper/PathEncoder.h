#pragma once
#include <memory>
#include <list>
#include "TaskContext.h"
#include "bocu1.h"

typedef std::tuple<std::wstring, std::wstring> CacheDictPair;

class PathEncoder
{
public:
	PathEncoder() = delete;
	PathEncoder(const PathEncoder&) = delete;
	//PathEncoder(const byte* key, const byte* iv);
	PathEncoder(const keeper::TaskContext& ctx);
	~PathEncoder();
	std::wstring encode(const std::wstring& path);
	std::wstring decode(const std::wstring& path);
private:
	std::wstring BinToString(byte* buf, size_t length);
	size_t StringToBin(const std::wstring& s, byte* buf);

	static constexpr size_t compressBufferSize_ = MAX_PATH_LENGTH * sizeof(wchar_t) * 2; //k=2 just to make sure
	std::unique_ptr<byte[]> compressBuffer_;
	std::unique_ptr<byte[]> bocu1Buffer_;
	std::unique_ptr<wchar_t[]> charsBuffer_;

	byte key_[crypto_stream_chacha20_KEYBYTES];
	byte nonce_[crypto_stream_chacha20_NONCEBYTES];

	//encoded names cache
	size_t NameCacheSize = 8;
	//ATTENTION: same cache for encoding and decoding
	std::list<CacheDictPair> NameCache;
	std::wstring GetCachedName(const std::wstring& name);
	void AddNameToCache(const std::wstring& cachedKey, const std::wstring& cachedName);
};