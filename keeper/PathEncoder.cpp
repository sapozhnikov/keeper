#include "stdafx.h"
#include "PathEncoder.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include "utf16_compress.h"

#define _SCL_SECURE_NO_WARNINGS

using namespace std;
using namespace keeper;

//bool SplitPath(const std::wstring & path, vector<wstring>& result)
//{
//	result.clear();
//	//turn off SDL checks for this
//	boost::split(result, path, boost::is_any_of(L"\\"));
//	return true;
//}

//int CompressString(const std::wstring & inStr, byte* result)
//{
//	auto lengthCompressed = utf16_compress((u16*)inStr.c_str(), inStr.length(), result);
//	return lengthCompressed;
//}

wstring PathEncoder::BinToString(byte* buf, unsigned int length)
{
	using namespace boost::multiprecision;

	vector<unsigned char> binaryVector(buf, buf + length);
	//for (auto b : binData)
	//	binaryVector.push_back(b);

	cpp_int binaryInt;
	import_bits(binaryInt, binaryVector.begin(), binaryVector.end());

	cpp_int quotient, remainder, outputCharsCount;
	const auto charsSetCount = wcslen(ENCODED_NAME_CHARS);
	outputCharsCount = charsSetCount;
	wstring outputString;
	while (true)
	{
		if (binaryInt < outputCharsCount)
		{
			outputString += ENCODED_NAME_CHARS[binaryInt.convert_to<unsigned char>()];
			break;
		}
		divide_qr(binaryInt, outputCharsCount, quotient, remainder);
		binaryInt = quotient;
		outputString += ENCODED_NAME_CHARS[remainder.convert_to<unsigned char>()];
	}
	return outputString;
}

unsigned int PathEncoder::StringToBin(const std::wstring & s, byte * buf)
{
	using namespace boost::multiprecision;

	cpp_int binaryInt = 0;
	unsigned int position;
	const auto inStrLen = s.length();
	const auto charsSetCount = wcslen(ENCODED_NAME_CHARS);
	for (unsigned int i = 0; i < inStrLen; i++)
	{
		for (position = 0; position < charsSetCount; position++)
		{
			if (s[inStrLen - i - 1] == ENCODED_NAME_CHARS[position])
			{
				binaryInt = binaryInt * charsSetCount + position;
				break;
			}
		}
		if (position == charsSetCount)
			throw exception("Unexpected character in the encrypted file name");
	}
	
	vector<byte> binaryVector;
	export_bits(binaryInt, back_inserter(binaryVector), 8);
	//std::copy(binaryVector.begin(), binaryVector.end(), buf);
	std::copy(binaryVector.begin(), binaryVector.end(), stdext::checked_array_iterator<byte*>(buf, compressBufferSize_));

	return binaryVector.size();
}

//PathEncoder::PathEncoder(const byte * key, const byte * iv)
PathEncoder::PathEncoder(const TaskContext& ctx)
{
	//fixme: possibly OutOfRange error with very long filename
	compressBuffer_ = make_unique<byte[]>(compressBufferSize_);
	charsBuffer_ = make_unique<wchar_t[]>(32767 * sizeof(wchar_t));

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
	//SplitPath(path, fileNames);
	//boost::split(fileNames, path, boost::is_any_of(L"\\"));
	boost::split(fileNames, path, std::bind2nd(std::equal_to<wchar_t>(), L'\\'));
	std::wstring cachedName;
	for (const auto& fileName : fileNames)
	{
		cachedName = GetCachedName(fileName);
		if (cachedName.empty())
		{
			//compress UCS-2 string
			//int lengthCompressed = CompressString(fileName, compressBuffer_.get());
			auto lengthCompressed = utf16_compress((u16*)fileName.c_str(), fileName.length(), compressBuffer_.get());
			//encrypt
			crypto_stream_chacha20_xor_ic(compressBuffer_.get(), compressBuffer_.get(), lengthCompressed, nonce_, 0, key_);
			//convert to string
			cachedName = BinToString(compressBuffer_.get(), lengthCompressed);
			AddNameToCache(fileName, cachedName);
		}
		fileNamesEncoded.push_back(cachedName);
	}
	
	//std::wostringstream s;
	//for (const auto& fileName : fileNamesEncoded)
	//{
	//	if (&fileName != &fileNamesEncoded[0])
	//		s << L'\\';
	//	s << fileName;
	//}
	//return s.str();
	return boost::join(fileNamesEncoded, L"\\");
}

std::wstring PathEncoder::decode(const std::wstring & path)
{
	vector<wstring> fileNamesEncoded, fileNamesDecoded;
	//boost::split(fileNamesEncoded, path, boost::is_any_of(L"\\"));
	boost::split(fileNamesEncoded, path, std::bind2nd(std::equal_to<wchar_t>(), L'\\'));
	std::wstring cachedName;
	for (const auto& fileName : fileNamesEncoded)
	{
		cachedName = GetCachedName(fileName);
		if (cachedName.empty())
		{
			//string to binary
			auto lengthBin = StringToBin(fileName, compressBuffer_.get());
			//decrypt
			crypto_stream_chacha20_xor_ic(compressBuffer_.get(), compressBuffer_.get(), lengthBin, nonce_, 0, key_);
			//decompress
			auto lengthDecompressed = utf16_decompress((u8 *)compressBuffer_.get(), lengthBin, (u16 *)charsBuffer_.get());
			//convert to string
			cachedName = std::wstring(charsBuffer_.get(), lengthDecompressed / 2);
			AddNameToCache(fileName, cachedName);
		}
		fileNamesDecoded.push_back(cachedName);
	}
	return boost::join(fileNamesDecoded, L"\\");
}


