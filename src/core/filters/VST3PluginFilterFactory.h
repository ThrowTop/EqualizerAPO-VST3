#pragma once

#include "IFilterFactory.h"

class VST3PluginFilterFactory : public IFilterFactory
{
public:
	std::vector<IFilter*> createFilter(const std::wstring& configPath,
		std::wstring& command, std::wstring& parameters) override;
};
