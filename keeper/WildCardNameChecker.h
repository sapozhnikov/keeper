#pragma once
#include <regex>

namespace keeper
{
	class WildCardNameChecker
	{
	public:
		WildCardNameChecker();
		void AddIncludeMask(const std::wstring& mask);
		void AddExcludeMask(const std::wstring& mask);
		bool IsFitPattern(const std::wstring& fileName);

		bool IsFilteringEnabled = false;
	
	private:
		void AddMasksToSet(const std::wstring & masks, std::vector<std::wregex>& v);
		std::vector<std::wregex> IncludePatterns_;
		std::vector<std::wregex> ExcludePatterns_;
		std::wstring MaskToPattern(const std::wstring& mask);
	};
}
