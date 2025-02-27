#include <config.h>
#include <discovery.h>
#include <list>
#include <debug.h>

using namespace std;

int main(int argc, char *argv[])
{
	TRACING();
	help_cmd msg;
	int lines = 0;
	/* Create a list of commands */
	list<cmds *> *cmd_list = new list<cmds *>;

	/* Add each class in this list */
	cmd_list->push_back(new config());
	cmd_list->push_back(new discovery());

	/* Run each command */
	for(auto& it : *cmd_list) {
		it->help(&msg, &lines);
		it->run();
	}

	/* Delete all commands from the class */
	for(auto& it : *cmd_list) {
		delete it;
	}

	/* Delete the list itself */
	delete cmd_list;
	return 0;
}
