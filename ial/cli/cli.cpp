#include <config.h>
#include <discovery.h>
#include <list>

using namespace std;

int main(int argc, char *argv[])
{
	help_cmd msg;
	int lines = 0;
	list<cmds *> *cmd_list = new list<cmds *>;

	cmd_list->push_back(new config());
	cmd_list->push_back(new discovery());

	for(auto& it : *cmd_list) {
		it->help(&msg, &lines);
		it->run();
	}

	for(auto& it : *cmd_list) {
		delete it;
	}

	delete cmd_list;
	return 0;
}
