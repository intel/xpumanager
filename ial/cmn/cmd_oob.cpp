#include "cmd_oob.h"
#include "debug.h"
#include <assert.h>

void cmdOOB::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Out-of-Band (OOB) Management Commands"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s oob [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "%s oob telemetry", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob telemetry -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob telemetry -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob telemetry -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw fw_type", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw -t AMC -f [imageFilePath]", progName.c_str()));

	printHelp(helpList, helpType);
	helpList.clear();
}

int cmdOOB::run(arg_struct *args)
{
	TRACING();
	// Implement the command logic here
	return 0;
}