#include "stdafx.h"
#include "TaskList.h"
#include "GlobalUtils.h"
#include <set>
#include <boost\scope_exit.hpp>

using namespace ConsoleLogger;

TaskList::TaskList(keeper::TaskContext& ctx) :
	ctx_(ctx)
{
}

bool TaskList::Run()
{
	if (!ctx_.OpenDatabase())
	{
		CONSOLE() << "Nothing to list" << std::endl;
		return false;
	}

	std::set<int64_t> timestamps64;

	Dbc* mainCursor;
	Dbt keyMain, dataMain;
	ctx_.GetMainDB().cursor(NULL, &mainCursor, 0);
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (mainCursor != nullptr)
			mainCursor->close();
		
		ctx_.CloseDatabase();
	};

	int result;
	DbFileEvent* pEvent;
	while (true)
	{
		result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_NODUP);
		if (result == DB_NOTFOUND) //end of DB
			break;
		
		while (true)
		{
			if (dataMain.get_size() != sizeof(DbFileEvent))
				throw std::exception("Corrupted DB data");

			pEvent = static_cast<DbFileEvent*>(dataMain.get_data());
			timestamps64.insert(pEvent->EventTimeStamp);

			result = mainCursor->get(&keyMain, &dataMain, DB_NEXT_DUP);
			if (result)
				break;
		}
	}

	for (auto& timestamp64 : timestamps64)
	{
		auto posixTime = keeper::ConvertMillisecToPtime(timestamp64);
		CONSOLE() << keeper::PTimeToWstring(posixTime) << std::endl;
	}
	return true;
}
