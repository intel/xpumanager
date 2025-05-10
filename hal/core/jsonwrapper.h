#ifndef _JSONWRAPPER_H
#define _JSONWRAPPER_H

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class jsonWrapper
{

private:
	json newJsonObject;

public:
	jsonWrapper() {}
	~jsonWrapper() {}
	void addObj(const char *id, const char *val)
	{
		newJsonObject[id] = val;
	}
};

#endif
