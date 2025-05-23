/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "os.h"
#include <sys/mman.h>
#include <math.h>
#include <debug.h>
#include <grp.h>
#include <pwd.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <fcntl.h>

/**
 * @brief Creates a new thread.
 *
 * @param thread Function pointer to the thread function.
 * @param args Arguments to pass to the thread function.
 * @return A pointer to the thread_id object representing the created thread.
 */
thread_id *create_thread(funcptr thread, void *args)
{
	pthread_t thread_hdl;
	pthread_create(&thread_hdl, NULL, thread, args);

	DBG("%s: thread handle is %ld\n", __func__, thread_hdl);
	thread_id *new_thread_id = new thread_id(thread_hdl);
	return new_thread_id;
}

/**
 * @brief Waits for the specified thread to complete.
 *
 * @param tid Pointer to the thread_id object representing the thread to wait for.
 */
void wait_for_thread(thread_id *tid)
{
	if (tid->ret_thread_uid())
	{
		DBG("%s: thread handle is %ld\n", __func__, tid->ret_thread_uid());
		pthread_join(tid->ret_thread_uid(), NULL);
	}
}

bool privilegeCheck()
{
	uid_t uid = getuid();
	if (uid == 0)
	{
		return true;
	}
	struct passwd *pw = getpwuid(uid);
	if (pw == NULL)
	{
		ERR("getpwuid error\n");
		return false;
	}
	int ngroups = 0;
	getgrouplist(pw->pw_name, pw->pw_gid, NULL, &ngroups);
	if (ngroups == 0)
	{
		return false;
	}
	gid_t groups[ngroups];
	getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
	string xpum_grp("xpum");
	bool has_privilege = false;
	for (int i = 0; i < ngroups; i++)
	{
		struct group *gr = getgrgid(groups[i]);
		if (gr == NULL)
		{
			ERR("getgrgid error\n");
			return false;
		}
		string grp_name(gr->gr_name);
		if (grp_name == xpum_grp)
		{
			has_privilege = true;
		}
	}

	return has_privilege;
}

string getProcessName(uint32_t processId)
{
	string processName = "";
	ifstream pinfo;
	char path[255];
	sprintf(path, "/proc/%d/cmdline", processId);
	pinfo.open(path);
	if (pinfo.is_open())
	{
		getline(pinfo, processName);
		pinfo.close();
	}
	return processName;
}

vector<string> findI2CDevices()
{
	vector<string> devices;
	string devicesPath = "/sys/bus/i2c/devices";

	// Check if the directory exists
	if (!filesystem::exists(devicesPath))
	{
		ERR("I2C devices directory does not exist: %s\n", devicesPath.c_str());
		return devices;
	}

	// Iterate through all entries in the directory
	for (const auto &entry : filesystem::directory_iterator(devicesPath))
	{
		if (entry.is_directory())
		{
			string dirname = entry.path().filename().string();
			devices.push_back(entry.path().string());
		}
	}

	return devices;
}

string getI2CDeviceName(const string &devicePath)
{
	string name;
	ifstream nameFile(devicePath + "/name");
	if (nameFile.is_open())
	{
		getline(nameFile, name);
		nameFile.close();
	}
	return name;
}

long long openI2C(const string &deviceName)
{
	vector<string> devices = findI2CDevices();
	for (const auto &device : devices)
	{
		string name = getI2CDeviceName(device);
		if (name == deviceName)
		{
			// Found the device with the specified name, the first character of the path
			// Example: "/sys/bus/i2c/devices/7-0040" -> "7"
			// This needs to be added to i2c- so that we can open the device
			// Example: "/dev/i2c-7"
			string i2cDevice = "/dev/i2c-" + device.substr(device.find_last_of('/') + 1, 1);
			// Open the I2C device
			return (long long)open(i2cDevice.c_str(), O_RDWR | O_NONBLOCK);
		}
	}
	return -1;
}

int closeI2C(long long fd)
{
	return close((int)fd);
}