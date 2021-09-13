#include "hwinfo.h"
#include <array>
#include <deque>
#include <vector>
#include "stddef.h"

class SystemCommandResult {
  std::string _output;
  int _exitStatus;

public:
  SystemCommandResult(std::string& cmd_output, int cmd_exit_status) {
	  _output = cmd_output;
    _exitStatus = cmd_exit_status;
  }

  const std::string& output() {
	  return _output;
  }

  const int exitStatus() {
		return _exitStatus;
  }
};

static SystemCommandResult execCommand(const std::string &command) {
  int exitcode = 0;
  std::array<char, 1048576> buffer {};
  std::string result;

  FILE *pipe = popen(command.c_str(), "r");
  if (pipe != nullptr) {
	try {
	  std::size_t bytesread;
	  while ((bytesread = std::fread(buffer.data(), sizeof(buffer.at(0)), sizeof(buffer), pipe)) != 0) {
		result += std::string(buffer.data(), bytesread);
	  }
	} catch (...) {
      pclose(pipe);
    }
  }
  exitcode = WEXITSTATUS(pclose(pipe));
  return SystemCommandResult(result, exitcode);
}

static const std::string PCI_FILE_SYS("sys");
static const std::string PCI_FILE_DEVICES("devices");
static std::deque<std::string> getParentPciBridges(const std::string& origin_str) {
  std::deque<std::string> res;
  if (!origin_str.empty()) {
	const char* ostr = origin_str.c_str();
	const int len = origin_str.length();
	std::string nstr;
    for (int i=0; i<len; i++) {
			const char cc = ostr[i];
	    switch (cc) {
	    case '/':
		    if (!nstr.empty()) {
					if (nstr != PCI_FILE_SYS && nstr != PCI_FILE_DEVICES) {
						res.push_front(nstr);
					}
					nstr.clear();
		    }
        break;
	    default:
				nstr += cc;
		    break;
	    }
	  }
  }
  return res;
}

static const std::string SYSTEM_SLOT_NAME_MARKER("Designation:");
static const std::string SYSTEM_SLOT_BUS_ADDRESS_MARKER("Bus Address:");
static const std::string SYSTEM_SLOT_CURRENT_USAGE_MARKER("Current Usage:");
static const std::string SYSTEM_INFO_IGNORED_STARTER(" \t");
static const std::string SYSTEM_INFO_IGNORED_ENDER("\r\n");
static std::string getValueAtMarker(const std::string& sysInfo, const std::string& marker) {
  std::string res;
	std::string spaces;
	std::size_t mPos = sysInfo.find(marker);
	if (mPos != std::string::npos) {
	  const int len = sysInfo.length();
		int i=mPos+marker.length();
		while (i<len && SYSTEM_INFO_IGNORED_STARTER.find(sysInfo.at(i)) != std::string::npos) i++;
		char cc;
		while (i<len && SYSTEM_INFO_IGNORED_ENDER.find(cc = sysInfo.at(i)) == std::string::npos) {
			switch (cc) {
			case ' ':
			case '\t':
				spaces += cc;
				break;
			default:
				if (!spaces.empty()) {
					res += spaces;
					spaces.clear();
				}
				res += cc;
				break;
			}
			i ++;
		}
	}
	return res;
}

static const std::string SYSTEM_SLOT_IN_USE("In Use");
class DMISystemSlot {
	std::string _name;
  std::string _busAddress;
	std::string _currentUsage;
public:
	DMISystemSlot(const std::string& slotInfo) {
		_name = getValueAtMarker(slotInfo, SYSTEM_SLOT_NAME_MARKER);
		_busAddress = getValueAtMarker(slotInfo, SYSTEM_SLOT_BUS_ADDRESS_MARKER);
		_currentUsage = getValueAtMarker(slotInfo, SYSTEM_SLOT_CURRENT_USAGE_MARKER);
	}

	const std::string& name() {
		return _name;
	}

	const std::string& busAddress() {
		return _busAddress;
	}

	const std::string& currentUsage() {
		return _currentUsage;
	}

	bool inUse() {
		return _currentUsage == SYSTEM_SLOT_IN_USE;
	}
};

static const std::string SYSTEM_SLOT_MARKER("System Slot Information");
static std::vector<DMISystemSlot> getSystemSlotBlocks(const std::string& ssInfos) {
	std::vector<DMISystemSlot> res;
	std::size_t curPos = 0;
	std::size_t nextPos;
	while ((nextPos = ssInfos.find(SYSTEM_SLOT_MARKER, curPos)) != std::string::npos) {
		if (curPos > 0) {
			res.push_back(DMISystemSlot(ssInfos.substr(curPos,nextPos - curPos)));
		}
		curPos = nextPos + SYSTEM_SLOT_MARKER.length();
	}
	if (curPos > 0) {
		res.push_back(DMISystemSlot(ssInfos.substr(curPos)));
	}
	return res;
}

std::string HWInfo::getPciSlot(const std::string& bdf_regex) {
	std::string res;
	std::string cmd_find_device_link = "find /sys/devices -name \"*" + bdf_regex + "\"";
	SystemCommandResult sc_res = execCommand(cmd_find_device_link);
	SystemCommandResult ss_res = execCommand("dmidecode -t 9");

	if (sc_res.exitStatus() == 0 && ss_res.exitStatus() == 0) {
		std::deque<std::string> parentBridges = getParentPciBridges(sc_res.output());
		std::vector<DMISystemSlot> systemSlots = getSystemSlotBlocks(ss_res.output());
		for (auto& pBridge: parentBridges) {
			for (auto& sysSlot: systemSlots) {
				if (sysSlot.inUse() && sysSlot.busAddress() == pBridge) {
					res = sysSlot.name();
					break;
				}
				if (!res.empty()) break;
			}
		}
	}
	return res;
}