#pragma once

#include "EndpointIdentity.h"

#include <string>
#include <vector>

class EndpointIdentityProvider
{
public:
	static std::vector<EndpointIdentity> enumerate();
	static bool findByEndpointGuid(const std::vector<EndpointIdentity>& catalog, const std::wstring& endpointGuid, EndpointIdentity& identity);
};
