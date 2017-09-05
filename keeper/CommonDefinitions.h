#pragma once
#ifdef _WIN64
const char * const MESSAGE_ABOUT = "BURO - backup utility v1.0/W64 (c) 2017 Dmitriy Sapozhnikov";
#else
const char * const MESSAGE_ABOUT = "BURO - backup utility v1.0/W32 (c) 2017 Dmitriy Sapozhnikov";
#endif
const wchar_t * const MIRROR_SUB_DIR = L"mirror\\";
const wchar_t * const MAIN_DB_FILE = L"buro.db";
const char * const EVENTS_DB_TABLE = "events";
const char * const CONFIG_DB_TABLE = "config";
const wchar_t * const NAME_SUFFIX_COMPRESSED = L".bz2";
const wchar_t * const NAME_SUFFIX_ENCRYPTED = L".encrypted";
//you can add here anything you want
const wchar_t* const ENCODED_NAME_CHARS = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!~.$()";
const wchar_t MASKS_SEPARATOR = L';';

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