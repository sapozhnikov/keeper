#include "stdafx.h"
#include <iostream>
#pragma warning(push)
#pragma warning(disable:4459)
#include <boost\program_options.hpp>
#pragma warning(pop)
#include "TaskContext.h"

//WARNING: timestamps as std::string, cause BOOST libs, period as std::string cause RegEx libs

using namespace boost::program_options;
using namespace ConsoleLogger;

class WrongArguments final {};

//returns 'true' if its OK to proceed the task
bool ParseCLITask(keeper::TaskContext& ctx, int argc, wchar_t *argv[])
{
	options_description commonDesc("Allowed options");
	commonDesc.add_options()
		("backup", "backup files")
		("restore", "restore files from backup")
		("dumpdb", "dump backups database")
		("purge", "erase old incremental changes");
	
	options_description otherOptionsDesc("Other options");
	otherOptionsDesc.add_options()
		("verbose", "verbose mode")
		("printloglevel", "prepend log level to messages")
		("printtimestamp", "prepend timestamp to messages")
		("password", value<std::string>(), "password for accessing files")
		("encodenames", "encode file names");

	options_description backupDesc("Backup options");
	backupDesc.add_options()
		("srcdir", wvalue<std::wstring>(), "full path to directory to backup")
		("dstdir", wvalue<std::wstring>(), "full path to directory there data will be stored")
		("compress", value<unsigned int>()->implicit_value(5), "compress files (1-faster, ..., 9-slower)");

	options_description restoreDesc("Restore options");
	restoreDesc.add_options()
		("srcdir", wvalue<std::wstring>(), "full path to directory there is data stored")
		("dstdir", wvalue<std::wstring>(), "full path to directory there data will be written")
		("timestamp", value<std::string>(), "timestamp of the backup, or the latest, if not defined (format \"YYYY-MM-DD HH:mm:SS.SSS\")");

	options_description dumpDesc("Dump DB options");
	dumpDesc.add_options()
		("srcdir", wvalue<std::wstring>(), "full path to directory there is data stored");

	options_description purgeDesc("Purge options");
	purgeDesc.add_options()
		("srcdir", wvalue<std::wstring>(), "full path to directory there is data stored")
		("older", value<std::string>(), "delete backups older than date (format \"YYYY-MM-DD HH:mm:SS.SSS\") or period (format P1Y2M3DT4H5M6S)");

	try
	{
		variables_map varMap;
		//preparsing task
		wparsed_options preparsed = wcommand_line_parser(argc, argv).options(commonDesc).allow_unregistered().run();
		store(preparsed, varMap);
		notify(varMap);

		//-----------------
		keeper::Task task = keeper::Task::Undetected;
		if (varMap.count("backup"))
			task = keeper::Task::Backup;

		if (varMap.count("restore"))
		{
			if (task != keeper::Task::Undetected)
				throw WrongArguments();
			else
				task = keeper::Task::Restore;
		}

		if (varMap.count("dumpdb"))
		{
			if (task != keeper::Task::Undetected)
				throw WrongArguments();
			else
				task = keeper::Task::DumpDB;
		}

		if (varMap.count("purge"))
		{
			if (task != keeper::Task::Undetected)
				throw WrongArguments();
			else
				task = keeper::Task::Purge;
		}

		if (task == keeper::Task::Undetected)
			throw WrongArguments();

		variables_map varMapOther;
		wparsed_options otherParsed = wcommand_line_parser(argc, argv).options(otherOptionsDesc).allow_unregistered().run();
		store(otherParsed, varMapOther);
		notify(varMapOther);

		if (varMapOther.count("verbose"))
		{
			ConsoleLogger::SetLogLevel(ConsoleLogger::LogLevel::verbose);
		}

		if (varMapOther.count("printloglevel"))
		{
			ConsoleLogger::SetAttrLogLevelVisible(true);
		}

		if (varMapOther.count("printtimestamp"))
		{
			ConsoleLogger::SetAttrTimestampVisible(true);
		}

		if (varMapOther.count("encodenames"))
		{
			ctx.isEncryptedFileNames = true;
		}

		if (varMapOther.count("password"))
		{
			ctx.DbPassword = varMapOther["password"].as<std::string>();
		}

		if (ctx.isEncryptedFileNames && ctx.DbPassword.empty())
		{
			LOG_FATAL() << "Password missed" << std::endl;
			throw WrongArguments();
		}

		//backup
		if (task == keeper::Task::Backup)
		{
			options_description tmpOptions;
			tmpOptions.add(commonDesc).add(otherOptionsDesc).add(backupDesc);
			variables_map varMapBackup;
			store(parse_command_line(argc, argv, tmpOptions), varMapBackup);
			notify(varMapBackup);

			if (varMapBackup.count("srcdir"))
				ctx.SetSourceDirectory(varMapBackup["srcdir"].as<std::wstring>());
			else
				throw WrongArguments();
			
			if (varMapBackup.count("dstdir"))
				ctx.SetDestinationDirectory(varMapBackup["dstdir"].as<std::wstring>());
			else
				throw WrongArguments();

			if (varMapBackup.count("compress"))
				ctx.CompressionLevel = varMapBackup["compress"].as<unsigned int>();
			else
				ctx.CompressionLevel = 0;

			ctx.Task = keeper::Task::Backup;

			return true;
		}

		//restore
		if (task == keeper::Task::Restore)
		{
			options_description tmpOptions;
			tmpOptions.add(commonDesc).add(otherOptionsDesc).add(restoreDesc);
			variables_map varMapRestore;
			store(parse_command_line(argc, argv, tmpOptions), varMapRestore);
			notify(varMapRestore);

			if (varMapRestore.count("srcdir"))
				ctx.SetSourceDirectory(varMapRestore["srcdir"].as<std::wstring>());
			else
				throw WrongArguments();

			if (varMapRestore.count("dstdir"))
				ctx.SetDestinationDirectory(varMapRestore["dstdir"].as<std::wstring>());
			else
				throw WrongArguments();

			if (varMapRestore.count("timestamp"))
			{
				if (!ctx.SetRestoreTimeStamp(varMapRestore["timestamp"].as<std::string>())) //string!
					throw WrongArguments();
			}
			else
				ctx.SetRestoreTimeStamp(std::string());

			ctx.Task = keeper::Task::Restore;

			return true;
		}

		//dump
		if (task == keeper::Task::DumpDB)
		{
			options_description tmpOptions;
			tmpOptions.add(commonDesc).add(otherOptionsDesc).add(dumpDesc);
			variables_map varMapDumpDB;
			store(parse_command_line(argc, argv, tmpOptions), varMapDumpDB);
			notify(varMapDumpDB);

			if (varMapDumpDB.count("srcdir"))
				ctx.SetSourceDirectory(varMapDumpDB["srcdir"].as<std::wstring>());
			else
				throw WrongArguments();

			ctx.Task = keeper::Task::DumpDB;

			return true;
		}

		//purge
		if (task == keeper::Task::Purge)
		{
			options_description tmpOptions;
			tmpOptions.add(commonDesc).add(otherOptionsDesc).add(purgeDesc);
			variables_map varMapPurge;
			store(parse_command_line(argc, argv, tmpOptions), varMapPurge);
			notify(varMapPurge);

			if (varMapPurge.count("srcdir"))
				ctx.SetSourceDirectory(varMapPurge["srcdir"].as<std::wstring>());
			else
				throw WrongArguments();

			if (varMapPurge.count("older"))
			{
				if (!ctx.SetPurgeTimestampFromArg(varMapPurge["older"].as<std::string>()))
					throw WrongArguments();
			}

			ctx.Task = keeper::Task::Purge;

			return true;
		}
	}
	catch (WrongArguments)
	{
		//show help
		std::cout << commonDesc << std::endl;
		std::cout << backupDesc << std::endl;
		std::cout << restoreDesc << std::endl;
		std::cout << dumpDesc << std::endl;
		std::cout << purgeDesc << std::endl;
		std::cout << otherOptionsDesc << std::endl;

		return false;
	}
	catch (std::exception& e)
	{
		LOG_FATAL() << "Error while parsing arguments" << std::endl;
	}

	return false;
}