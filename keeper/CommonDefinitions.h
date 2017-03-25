#pragma once

static wchar_t* MIRROR_SUB_DIR = L"mirror\\";
static wchar_t* MAIN_DB_FILE = L"events.db";
//static wchar_t* DELTA_DB_FILE = L"deletes.db";

const byte configKey[] = { 0xC0,0x5B,0x18,0xE2,0xEE,0x31,0xF6,0xF8,0x13,0xD6,0xFE,0x8C,0xEC,0xF7,0x4D,0x9C };
const DWORD ConfigStructVersionRequired = 1;

#pragma pack(push, 1)
struct DbConfig
{
	DWORD ConfigStructVersion;
	byte IsCompressed;
	//byte IsEncoded : 1;
};
#pragma pack(pop)

template <typename T> int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}

enum class FileEventType
{
	Deleted,
	Added,
	Changed
};

// TODO: change to serialization
#pragma pack(push, 1)
struct DbFileEvent
{
	int64_t EventTimeStamp;
	byte FileEvent;
	WIN32_FILE_ATTRIBUTE_DATA FileAttributes;
};
#pragma pack(pop)