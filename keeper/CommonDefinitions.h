#pragma once

const wchar_t * const MIRROR_SUB_DIR = L"mirror\\";
const wchar_t * const MAIN_DB_FILE = L"buro.database";
const char * const EVENTS_DB_TABLE = "events";
const char * const CONFIG_DB_TABLE = "config";
const char * const SECRETS_DB_TABLE = "secrets";
const wchar_t * const NAME_SUFFIX_COMPRESSED = L".bz2";
const wchar_t * const NAME_SUFFIX_ENCRYPTED = L".encrypted";
//you can add here anything you want
const wchar_t* const ENCODED_NAME_CHARS = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!~.$()";

//const DWORD ConfigStructVersionRequired = 1;

//#pragma pack(push, 1)
//struct DbConfig
//{
//	DWORD ConfigStructVersion;
//	byte IsCompressed;
//	//byte IsEncoded : 1;
//};
//#pragma pack(pop)

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