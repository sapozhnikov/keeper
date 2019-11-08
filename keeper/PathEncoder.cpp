#include "stdafx.h"
#include "PathEncoder.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include "basen.hpp"
#include "fpaq.h"

using namespace std;
using namespace keeper;

wstring PathEncoder::BinToString(byte* buf, size_t length)
{
	wstring outputString;
	outputString.reserve(size_t(length * 1.7)); //160% data grown for base32
	bn::encode_b32(buf, buf + length, back_inserter(outputString));
	return outputString;
}

size_t PathEncoder::StringToBin(const std::wstring & s, byte * buf)
{
	//decode_b32 don't work with unicode, so, convert it to OEM
	int length = ::WideCharToMultiByte(CP_OEMCP, 0, s.c_str(), s.size(), NULL, 0, NULL, NULL);
	if (length <= 0)
		return 0;
	std::vector<char> buffer(length);
	::WideCharToMultiByte(CP_OEMCP, 0, s.c_str(), s.size(), buffer.data(), length, NULL, NULL);
	std::string narrowedStr(buffer.begin(), buffer.end());

	assert(s.length() == narrowedStr.length());

	std::vector<byte> tmpBuf;
	tmpBuf.reserve(size_t(length / 1.5)); //160% data grown for base32
	bn::decode_b32(narrowedStr.c_str(), narrowedStr.c_str() + narrowedStr.size(), back_inserter(tmpBuf));
	memcpy(buf, tmpBuf.data(), tmpBuf.size());
	return tmpBuf.size();
}

//PathEncoder::PathEncoder(const byte * key, const byte * iv)
PathEncoder::PathEncoder(const TaskContext& ctx)
{
	//fixme: possibly OutOfRange error with very long filename
	compressBuffer_ = make_unique<byte[]>(compressBufferSize_);
	bocu1Buffer_ = make_unique<byte[]>(compressBufferSize_);
	charsBuffer_ = make_unique<wchar_t[]>(MAX_PATH_LENGTH * sizeof(wchar_t));

	memcpy(key_, ctx.NamesEncodeKey_, crypto_stream_chacha20_KEYBYTES);
	memcpy(nonce_, ctx.NamesEncodeNonce_, crypto_stream_chacha20_NONCEBYTES);
}

PathEncoder::~PathEncoder()
{
	sodium_memzero(key_, crypto_stream_chacha20_KEYBYTES);
	sodium_memzero(nonce_, crypto_stream_chacha20_NONCEBYTES);
}

std::wstring PathEncoder::GetCachedName(const std::wstring & name)
{
	for (auto it = NameCache.begin(); it != NameCache.end(); it++)
	{
		if (get<0>(*it) == name)
		{
			//move cache match to the top of cache
			NameCache.emplace_front(move(*it));
			NameCache.erase(it);
			return get<1>(NameCache.front());
		}
	}
	return std::wstring();
}

void PathEncoder::AddNameToCache(const std::wstring & cachedKey, const std::wstring & cachedName)
{
	NameCache.push_front(CacheDictPair(cachedKey, cachedName));
	if (NameCache.size() > NameCacheSize)
		NameCache.resize(NameCacheSize);
}

std::wstring PathEncoder::encode(const std::wstring & path)
{
	vector<wstring> fileNames, fileNamesEncoded;

	boost::split(fileNames, path, [](wchar_t ch) -> bool { return ch == L'\\'; });
	if (fileNames.size() > NameCacheSize)
		NameCacheSize = fileNames.size();
	std::wstring cachedName;
	for (const auto& fileName : fileNames)
	{
		cachedName = GetCachedName(fileName);
		if (cachedName.empty())
		{
			//compressing UCS-2 string
			//Unicode -> BOCU-1
			unsigned int lengthCompressed = BOCU1::writeString(fileName.c_str(), fileName.length(), bocu1Buffer_.get());
			//BOCU-1 -> fpaq compressing
			lengthCompressed = fpaq::fpaqCompress(bocu1Buffer_.get(), lengthCompressed, compressBuffer_.get());
			//encrypt
			crypto_stream_chacha20_xor_ic(compressBuffer_.get(), compressBuffer_.get(), lengthCompressed, nonce_, 0, key_);
			//convert to BASE32 string
			cachedName = BinToString(compressBuffer_.get(), lengthCompressed);
			AddNameToCache(fileName, cachedName);
		}
		fileNamesEncoded.push_back(cachedName);
	}
	
	return boost::join(fileNamesEncoded, L"\\");
}

std::wstring PathEncoder::decode(const std::wstring & path)
{
	vector<wstring> fileNamesEncoded, fileNamesDecoded;

	boost::split(fileNamesEncoded, path, [](wchar_t ch) -> bool { return ch == L'\\'; });
	if (fileNamesEncoded.size() > NameCacheSize)
		NameCacheSize = fileNamesEncoded.size();
	std::wstring cachedName;
	for (const auto& fileName : fileNamesEncoded)
	{
		cachedName = GetCachedName(fileName);
		if (cachedName.empty())
		{
			//BASE32 string to binary
			auto lengthBin = StringToBin(fileName, compressBuffer_.get());
			//decrypt
			crypto_stream_chacha20_xor_ic(compressBuffer_.get(), compressBuffer_.get(), lengthBin, nonce_, 0, key_);
			//decompress
			unsigned int lengthDecompressed = fpaq::fpaqDecompress(compressBuffer_.get(), lengthBin, bocu1Buffer_.get());
			lengthDecompressed = BOCU1::readString(bocu1Buffer_.get(), lengthDecompressed, charsBuffer_.get());
			//convert to string
			assert((lengthDecompressed & 1) == 0);
			cachedName = std::wstring(charsBuffer_.get(), lengthDecompressed);
			AddNameToCache(fileName, cachedName);
		}
		fileNamesDecoded.push_back(cachedName);
	}
	return boost::join(fileNamesDecoded, L"\\");
}
