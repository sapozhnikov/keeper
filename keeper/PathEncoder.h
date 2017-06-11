#pragma once
#include <memory>
#include <list>
#include "TaskContext.h"

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
	std::wstring BinToString(byte* buf, unsigned int length);
	unsigned int StringToBin(const std::wstring& s, byte* buf);

	const size_t compressBufferSize_ = 32767 * sizeof(wchar_t) * 2; //k=2 just to make sure
	std::unique_ptr<byte[]> compressBuffer_;
	std::unique_ptr<wchar_t[]> charsBuffer_;

	byte key_[crypto_stream_chacha20_KEYBYTES];
	byte nonce_[crypto_stream_chacha20_NONCEBYTES];

	//encoded names cache
	const size_t NameCacheSize = 16;
	//ATTENTION: same cache for encoding and decoding
	std::list<CacheDictPair> NameCache;
	std::wstring GetCachedName(const std::wstring& name);
	void AddNameToCache(const std::wstring& cachedKey, const std::wstring& cachedName);
};