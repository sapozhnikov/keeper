#pragma once
#ifdef _WIN64
const char * const MESSAGE_ABOUT = "BURO - backup utility v1.1/W64 (c) 2017 Dmitriy Sapozhnikov";
#else
const char * const MESSAGE_ABOUT = "BURO - backup utility v1.1/W32 (c) 2017 Dmitriy Sapozhnikov";
#endif
const wchar_t * const MIRROR_SUB_DIR = L"mirror\\";
const wchar_t * const ENV_SUB_DIR = L"env\\";
const wchar_t * const MAIN_DB_FILE = L"buro.db";
const char * const EVENTS_DB_TABLE = "events";
const char * const CONFIG_DB_TABLE = "config";
const wchar_t * const NAME_SUFFIX_COMPRESSED = L".bz2";
const wchar_t * const NAME_SUFFIX_ENCRYPTED = L".encrypted";
const wchar_t MASKS_SEPARATOR = L';';
const unsigned int LOG_FILE_SIZE = 128 * 1024 * 1024; //DB log max file size
const unsigned int LOG_BUF_SIZE = 1024 * 1024; //DB log buffer
const unsigned int ENV_CACHE_SIZE = 16 * 1024 * 1024; //DB memory pool
const unsigned int MAX_PATH_LENGTH = 32767;

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