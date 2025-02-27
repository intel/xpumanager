#include <iostream>
#include <config.h>

using namespace std;

int main(int argc, char *argv[])
{
	help_cmd msg;
	int lines = 0;
	cmds *new_cmd = new config();

	new_cmd->help(&msg, &lines);
	new_cmd->run();

	delete new_cmd;
	return 0;
}
