#include "stdafx.h"
#include "doctest\doctest.h"
#include "WildCardNameChecker.h"

TEST_CASE("WildCardNameChecker")
{
	SUBCASE("excluding names")
	{
		keeper::WildCardNameChecker checker;
		checker.IsFilteringEnabled = true;
		checker.AddExcludeMask(L"*.mp3");
		CHECK(checker.IsFitPattern(L"mydoc.xls"));
		CHECK(checker.IsFitPattern(L"mp3mp3mp3"));
		CHECK(!checker.IsFitPattern(L".mp3"));
		CHECK(checker.IsFitPattern(L"file.mp3333"));
		CHECK(!checker.IsFitPattern(L"song.mp3"));
		checker.AddExcludeMask(L"*.aac");
		CHECK(checker.IsFitPattern(L"mydoc.doc"));
		CHECK(!checker.IsFitPattern(L"song.aac"));
		CHECK(!checker.IsFitPattern(L"song.mp3"));
	}
	SUBCASE("exclude masks set")
	{
		keeper::WildCardNameChecker checker;
		checker.IsFilteringEnabled = true;
		checker.AddExcludeMask(L"*.mp3;*.aac;*.wav");
		CHECK(checker.IsFitPattern(L"mydoc.xls"));
		CHECK(checker.IsFitPattern(L"mydoc.doc"));
		CHECK(!checker.IsFitPattern(L"song.mp3"));
		CHECK(!checker.IsFitPattern(L"song.aac"));
		CHECK(!checker.IsFitPattern(L"song.wav"));
	}
	SUBCASE("including names")
	{
		keeper::WildCardNameChecker checker;
		checker.IsFilteringEnabled = true;
		checker.AddIncludeMask(L"*.mp3");
		CHECK(!checker.IsFitPattern(L"mydoc.xls"));
		CHECK(checker.IsFitPattern(L"song.mp3"));
		checker.AddIncludeMask(L"*.aac");
		CHECK(!checker.IsFitPattern(L"mydoc.doc"));
		CHECK(checker.IsFitPattern(L"song.aac"));
		CHECK(checker.IsFitPattern(L"song.mp3"));
	}
	SUBCASE("include masks set")
	{
		keeper::WildCardNameChecker checker;
		checker.IsFilteringEnabled = true;
		checker.AddIncludeMask(L"*.mp3;*.aac;*.wav");
		CHECK(!checker.IsFitPattern(L"mydoc.xls"));
		CHECK(!checker.IsFitPattern(L"mydoc.doc"));
		CHECK(checker.IsFitPattern(L"song.mp3"));
		CHECK(checker.IsFitPattern(L"song.aac"));
		CHECK(checker.IsFitPattern(L"song.wav"));
	}
}