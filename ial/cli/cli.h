#ifndef _CLI_H
#define _CLI_H

using namespace std;

enum DAEMONCAP
{
	DAEMONLESS,
	DAEMON,
	BOTH,
};

#define STRINGIFY(x) #x
#define CONCATENATE_WITH_DOT(a, b) "v" STRINGIFY(a) "." STRINGIFY(b)
#define CONCATENATE_WITH_DOTS(a, b, c, d) STRINGIFY(a) "." STRINGIFY(b) "." STRINGIFY(c) "." STRINGIFY(d)
#define GET_SHORT_VERSION() CONCATENATE_WITH_DOT(MAJOR, MINOR)
#define GET_FULL_VERSION() CONCATENATE_WITH_DOTS(MAJOR, MINOR, PATCH, BUILD_NUMBER)
#ifdef DAEMONMODE
DAEMONCAP curDaemonMode = DAEMON;
#else
DAEMONCAP curDaemonMode = DAEMONLESS;
#endif

enum HELP
{
	SHORT_HELP,
	FULL_HELP,
};

// Enum to represent OS types
enum class OSTYPE
{
	Windows,
	Linux,
	Both,
};

/* Structure to hold function and OS type */
struct function_entry
{
	function<cmds *()> create_func;
	DAEMONCAP daemon_cap;
	OSTYPE os_type;
};

#endif