#include "stdafx.h"
#include "WildCardNameChecker.h"
#include <vector>
#include <boost/algorithm/string.hpp>

using namespace std;

keeper::WildCardNameChecker::WildCardNameChecker()
{
}

void keeper::WildCardNameChecker::AddMasksToSet(const wstring & masks, std::vector<wregex>& s)
{
	vector<wstring> masksVect;
	boost::split(masksVect, masks, std::bind2nd(std::equal_to<wchar_t>(), MASKS_SEPARATOR));
	
	for (auto const& mask : masksVect)
	{
		s.push_back(wregex(MaskToPattern(mask), regex_constants::icase));
	}
}

void keeper::WildCardNameChecker::AddIncludeMask(const std::wstring & masks)
{
	AddMasksToSet(masks, IncludePatterns_);
	IsFilteringEnabled = IsFilteringEnabled || !IncludePatterns_.empty();
}

void keeper::WildCardNameChecker::AddExcludeMask(const std::wstring & masks)
{
	AddMasksToSet(masks, ExcludePatterns_);
	IsFilteringEnabled = IsFilteringEnabled || !ExcludePatterns_.empty();
}

std::wstring keeper::WildCardNameChecker::MaskToPattern(const std::wstring & mask)
{
	std::wstring pattern = mask;
	boost::replace_all(pattern, "\\", "\\\\");
	boost::replace_all(pattern, "^", "\\^");
	boost::replace_all(pattern, ".", "\\.");
	boost::replace_all(pattern, "$", "\\$");
	boost::replace_all(pattern, "|", "\\|");
	boost::replace_all(pattern, "(", "\\(");
	boost::replace_all(pattern, ")", "\\)");
	boost::replace_all(pattern, "[", "\\[");
	boost::replace_all(pattern, "]", "\\]");
	boost::replace_all(pattern, "*", "\\*");
	boost::replace_all(pattern, "+", "\\+");
	boost::replace_all(pattern, "?", "\\?");
	boost::replace_all(pattern, "/", "\\/");

	// Convert chars '*?' back to their regex equivalents
	boost::replace_all(pattern, "\\?", ".");
	boost::replace_all(pattern, "\\*", ".*");

	return pattern;
}

bool keeper::WildCardNameChecker::IsFitPattern(const std::wstring & fileName)
{
	if (!IsFilteringEnabled)
		return true;

	bool includeMatch = false;
	for (auto const& expr : IncludePatterns_)
	{
		if (regex_match(fileName, expr))
		{
			includeMatch = true;
			break;
		}
	}
	if (!IncludePatterns_.empty() && (includeMatch == false))
		return false;

	for (auto const& expr : ExcludePatterns_)
	{
		if (regex_match(fileName, expr))
			return false;
	}
	
	return true;
}