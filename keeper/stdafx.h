// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here
#include <db_cxx.h>
#include <Windows.h> //include it AFTER berkley stuff
#include <string>
#include "CommonDefinitions.h"
#include "ConsoleLogger.h"
#include <boost\date_time\posix_time\posix_time.hpp>

#define SODIUM_STATIC 1
#define SODIUM_EXPORT
#include <sodium.h>
