#include "stdafx.h"
#include "doctest\doctest.h"
#include "PathEncoder.h"
#include "fpaq.h"

std::wstring EncryptStr(const std::wstring& s, byte* key, byte* nonce)
{
	keeper::TaskContext ctx;
	ctx.Task = keeper::Task::Backup;

	std::copy(key, key + crypto_stream_chacha20_KEYBYTES, ctx.NamesEncodeKey_);
	std::copy(nonce, nonce + crypto_stream_chacha20_NONCEBYTES, ctx.NamesEncodeNonce_);

	PathEncoder encoder(ctx);
	return encoder.encode(s);
}

std::wstring DecryptStr(const std::wstring& s, byte* key, byte* nonce)
{
	keeper::TaskContext ctx;
	ctx.Task = keeper::Task::Backup;

	std::copy(key, key + crypto_stream_chacha20_KEYBYTES, ctx.NamesEncodeKey_);
	std::copy(nonce, nonce + crypto_stream_chacha20_NONCEBYTES, ctx.NamesEncodeNonce_);

	PathEncoder encoder(ctx);
	return encoder.decode(s);
}

TEST_CASE("PathsEncoder")
{
	byte sampleKey[crypto_stream_chacha20_KEYBYTES] =
		//{ 0x49,0x30,0x44,0xA6,0x0A,0x33,0xBF,0xD1,0xD4,0xBA,0x35,0x7D,0x42,0x48,0xE2,0xA9,0xD7,0x7F,0xC8,0xA7,0x00,0xED,0x6B,0x14,0x9B,0x55,0x89,0x97,0x7D,0xDE,0x70,0xD4 };
		{ 0x0b,0xd7,0x6e,0xe7,0x0b,0x89,0xa6,0xe3,0x42,0x6f,0x8d,0x99,0x7c,0xbc,0xcd,0x54,0x52,0xcc,0xd7,0xdf,0x89,0x50,0xf2,0xdf,0x68,0xb7,0x55,0xcb,0xd2,0xc0,0x0d,0xfa };
	byte sampleNonce[crypto_stream_chacha20_NONCEBYTES] =
		//{ 0xD8,0x5A,0x96,0xB8,0xBF,0xFF,0x87,0x7A };
		{ 0x4d,0x61,0xda,0x8d,0x0d,0xe2,0xea,0xe0 };
	std::wstring originalPath = L"ordinary file name";
	std::wstring encryptedPath = EncryptStr(originalPath, sampleKey, sampleNonce);
	std::wstring decryptedPath = DecryptStr(encryptedPath, sampleKey, sampleNonce);

	CHECK(originalPath == decryptedPath);

	for (int i = 0; i < 100; i++)
	{
		randombytes_buf(sampleKey, crypto_stream_chacha20_KEYBYTES);
		randombytes_buf(sampleNonce, crypto_stream_chacha20_NONCEBYTES);

		encryptedPath = EncryptStr(originalPath, sampleKey, sampleNonce);
		decryptedPath = DecryptStr(encryptedPath, sampleKey, sampleNonce);

		CHECK(originalPath == decryptedPath);
	}
}

TEST_CASE("ArithmeticCoder")
{
	byte originalData[] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 6, 6, 6, 6, 6, 6, 6, 6, 6};
	constexpr auto originalDataSize = sizeof(originalData);
	byte compressedData[originalDataSize * 2];
	byte decompressedData[originalDataSize * 2];

	auto iterBegin = std::begin(originalData);
	auto iterEnd = std::end(originalData);
	auto iterOut = std::begin(compressedData);

	//compressing
	fpaq::Encoder<decltype(iterBegin), decltype(iterOut)> encoder(fpaq::Mode::COMPRESS, iterBegin, iterEnd, iterOut);
	while (iterBegin != iterEnd)
	{
		encoder.encode(1);
		byte c = *iterBegin++;
		for (int i = 7; i >= 0; --i)
			encoder.encode((c >> i) & 1);
	}
	encoder.encode(0);
	encoder.flush();

	auto compessedSize = iterOut - std::begin(compressedData);
	iterBegin = std::begin(compressedData);
	iterEnd = iterBegin + compessedSize;
	iterOut = std::begin(decompressedData);

	//decompressing
	fpaq::Encoder<decltype(iterBegin), decltype(iterOut)> encoder2(fpaq::Mode::DECOMPRESS, iterBegin, iterEnd, iterOut);

	while (encoder2.decode())
	{
		int c = 1;
		while (c < 256)
			c += c + encoder2.decode();
		*iterOut++ = c - 256;
	}
	auto decompessedSize = iterOut - std::begin(decompressedData);
	CHECK(originalDataSize == decompessedSize);
	for (int i = 0; i < originalDataSize; i++)
		CHECK(originalData[i] == decompressedData[i]);
}

TEST_CASE("BOCU1")
{
	std::wstring str1 = L"Особенностью и ценностью продуктовой компании, основной ее миссией и задачей, является удовлетворенность клиентов, их вовлеченность, и лояльность к бренду.";
	byte compressBuffer[MAX_PATH_LENGTH * 2];
	wchar_t decompressBuffer[MAX_PATH_LENGTH * 2];
	std::fill(std::begin(compressBuffer), std::end(compressBuffer), 0);
	std::fill(std::begin(decompressBuffer), std::end(decompressBuffer), 0);

	unsigned int bocuSize, decodedSize;

	bocuSize = BOCU1::writeString(str1.c_str(), str1.length(), compressBuffer);
	decodedSize = BOCU1::readString(compressBuffer, bocuSize, decompressBuffer);
	std::wstring decodedStr(decompressBuffer, decompressBuffer + decodedSize);
	CHECK(str1 == decodedStr);
}