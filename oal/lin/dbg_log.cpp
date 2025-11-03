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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <iostream>
#include <fstream>
#include <array>
#include <filesystem>
#include <vector>
#include <debug.h>
#include <cstring>
#include <cerrno>

#define ERR_UUID -1
#define ERR_TMP_DIR -2
#define ERR_COPY_FILES -3
#define ERR_GEN_OUT -5
#define ERR_TAR -6
#define ERR_RM_TMP -7

namespace {

class TempDirectory
{
	std::filesystem::path dirPath;
	bool should_cleanup = true;
	bool created_successfully = false;

public:
	explicit TempDirectory(std::string_view uuid) : dirPath{std::string("/var/tmp/xpum-") + std::string(uuid)}
	{
		std::error_code ec;
		created_successfully = std::filesystem::create_directory(dirPath, ec);

		if (!created_successfully) {
			ERR("Failed to create directory %s: %s\n", dirPath.string().c_str(), ec.message().c_str());
		}
	}

	~TempDirectory()
	{
		if (should_cleanup && created_successfully) {
			std::error_code ec;
			std::filesystem::remove_all(dirPath, ec);
			if (ec) {
				ERR("Warning: Failed to cleanup %s: %s\n", dirPath.string().c_str(), ec.message().c_str());
			}
		}
	}

	TempDirectory(const TempDirectory &) = delete;
	TempDirectory &operator=(const TempDirectory &) = delete;
	TempDirectory(TempDirectory &&other) noexcept
		: dirPath(std::move(other.dirPath)), should_cleanup(other.should_cleanup),
		  created_successfully(other.created_successfully)
	{
		other.should_cleanup = false;
	}
	TempDirectory &operator=(TempDirectory &&other) noexcept
	{
		if (this != &other) {
			dirPath = std::move(other.dirPath);
			should_cleanup = other.should_cleanup;
			created_successfully = other.created_successfully;
			other.should_cleanup = false;
		}
		return *this;
	}

	const std::filesystem::path &path() const { return dirPath; }
	std::string uuid() const
	{
		return dirPath.filename().string().substr(5);
	} // Remove "xpum-", left with just the UUID
	bool is_valid() const { return created_successfully; }
	void keep() { should_cleanup = false; } // Don't cleanup on destruction

	explicit operator bool() const { return created_successfully; }
};
} // namespace

/**
 * @brief A class to encapsulate the result of executing a system command
 *
 * This class stores both the output and exit status of a system command
 * execution, providing a clean interface to access command results.
 * It's used by the execCommand function to return structured results.
 */
class SystemCommandResult
{
	std::string _output;
	int _exitStatus;

public:
	SystemCommandResult(std::string &cmd_output, int cmd_exit_status)
	{
		_output = cmd_output;
		_exitStatus = cmd_exit_status;
	}

	SystemCommandResult(const std::string &cmd_output, int cmd_exit_status)
	{
		_output = cmd_output;
		_exitStatus = cmd_exit_status;
	}

	const std::string &output() { return _output; }

	int exitStatus() { return _exitStatus; }
};

/**
 * @brief Execute a system command and capture its output
 *
 * This function executes a shell command using popen and captures both
 * the output and exit status. It provides a safer alternative to system()
 * by capturing output and providing proper error handling with RAII.
 *
 * @param command The shell command to execute
 * @return SystemCommandResult containing the command output and exit status
 *
 * @note Uses RAII with custom deleter for automatic pipe cleanup
 * @note Provides enhanced error handling for I/O operations
 * @note Uses std::fread for efficient buffer reading
 */
static SystemCommandResult execCommand(const std::string &command)
{
	std::array<char, 1048576> buffer = {};
	std::string result;
	// Custom deleter for pipe(), ensures it is always closed automatically on error
	struct PipeDeleter
	{
		int *exit_code;
		explicit PipeDeleter(int *ec) : exit_code(ec) {}
		void operator()(FILE *f) const noexcept
		{
			if (f && exit_code) {
				*exit_code = pclose(f);
			}
		}
	};
	using safePipe = std::unique_ptr<FILE, PipeDeleter>;
	int exitcode = -1;
	bool readSuccess = true;
	safePipe pipe(popen(command.c_str(), "r"), PipeDeleter(&exitcode));
	if (pipe != nullptr) {
		try {
			std::size_t bytesread;
			while ((bytesread = std::fread(buffer.data(), sizeof(buffer.at(0)), sizeof(buffer), pipe.get())) != 0) {
				result += std::string(buffer.data(), bytesread);
			}
			// std::fread returns 0 both when EOF is reached and when some I/O error occurred. Check for completeness
			if (std::ferror(pipe.get())) {
				return SystemCommandResult{"I/O error", -1}; // Not sure;
			} else if (!std::feof(pipe.get())) {
				return SystemCommandResult("Unexpected read termination", -1); // Not sure
			}
		} catch (...) {
			readSuccess = false;
		}
	}
	// RAII/custom deleter will close the pipe and set the exitcode
	pipe.reset();
	// If there was a read error, override the exitcode
	if (!readSuccess) {
		exitcode = -1;
	}
	// Only call WEXITSTATUS if exitcode is valid (not -1)
	if (exitcode == -1) {
		return SystemCommandResult(result, exitcode);
	} else {
		return SystemCommandResult(result, WEXITSTATUS(exitcode));
	}
}

/**
 * @brief Safely copies a file while handling special kernel debug files
 *
 * This function attempts to copy a file from source to destination with
 * enhanced error handling for special kernel debug files that may not be
 * readable through standard file operations. It performs readability checks
 * before attempting the copy operation.
 *
 * @param source The source file path to copy from
 * @param destination The destination file path to copy to
 * @return true if the file was successfully copied, false otherwise
 *
 * @note Special kernel debug files in /sys/kernel/debug may not be readable
 *       and will be safely skipped without crashing the application
 */
static bool safeCopyFile(const std::string &source, const std::string &destination)
{
	try {
		// First check if the file is readable by trying to open it
		std::ifstream sourceFile(source, std::ios::binary);
		if (!sourceFile.is_open()) {
			// File is not readable, skip it
			return false;
		}

		// Check if we can read from it without throwing an exception
		sourceFile.peek();
		if (sourceFile.bad()) {
			sourceFile.close();
			return false;
		}
		sourceFile.close();

		// File seems readable, try to copy it
		std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
		return true;
	} catch (const std::exception &ex) {
		// Log the error but don't terminate the program
		ERR("Cannot copy %s: %s\n", source.c_str(), ex.what());
		return false;
	}
}

/**
 * @brief Copies all regular files from a source directory to a destination directory
 *
 * This function iterates through all regular files in the source directory and
 * copies them to the destination directory using the safeCopyFile function.
 * It automatically creates the destination directory if it doesn't exist.
 *
 * @param sourceDir The source directory path to copy files from
 * @param destDir The destination directory path to copy files to
 *
 * @note This function only copies regular files, not subdirectories
 * @note Uses safeCopyFile to handle special kernel debug files gracefully
 */
// Helper function to copy all regular files from a source directory to destination directory
static void copyFilesFromDirectory(const std::string &sourceDir, const std::string &destDir)
{
	try {
		if (!std::filesystem::exists(sourceDir) || !std::filesystem::is_directory(sourceDir)) {
			return;
		}

		// Create destination directory if it doesn't exist
		std::filesystem::create_directories(destDir);

		for (const auto &entry : std::filesystem::directory_iterator(sourceDir)) {
			if (entry.is_regular_file()) {
				const auto destFile = std::filesystem::path(destDir) / entry.path().filename();
				DBG("Copying %s to %s\n", entry.path().string().c_str(), destFile.string().c_str());
				safeCopyFile(entry.path().string(), destFile);
			}
		}
	} catch (const std::filesystem::filesystem_error &ex) {
		ERR("Error accessing directory %s: %s\n", sourceDir.c_str(), ex.what());
	}
}

/**
 * @brief Copies files from all subdirectories within a base directory
 *
 * This function iterates through all subdirectories in the base directory
 * and copies regular files from each subdirectory to corresponding locations
 * in the destination directory structure. This is equivalent to the shell
 * script's copy_files_d2 function.
 *
 * @param baseDir The base directory to scan for subdirectories
 * @param destBaseDir The destination base directory where files will be copied
 *
 * @note The destination directory structure mirrors the source structure
 * @note Only processes immediate subdirectories, not nested subdirectories
 */
// Helper function to copy files from all subdirectories (equivalent to copy_files_d2)
static void copyFilesFromSubdirectories(const std::string &baseDir, const std::string &destBaseDir)
{
	try {
		if (!std::filesystem::exists(baseDir) || !std::filesystem::is_directory(baseDir)) {
			return;
		}

		for (const auto &entry : std::filesystem::directory_iterator(baseDir)) {
			if (entry.is_directory()) {
				std::string subDirName = entry.path().filename().string();
				std::string destSubDir = destBaseDir + baseDir + "/" + subDirName;
				copyFilesFromDirectory(entry.path().string(), destSubDir);
			}
		}
	} catch (const std::filesystem::filesystem_error &ex) {
		ERR("Error accessing base directory %s: %s\n", baseDir.c_str(), ex.what());
	}
}

/**
 * @brief Copies specific named directories from subdirectories within a base directory
 *
 * This function searches through all subdirectories within the base directory
 * and copies any subdirectory that matches the target directory name. This is
 * equivalent to the shell script's copy_a_dir function and is used for copying
 * specific directories like "xe_params".
 *
 * @param baseDir The base directory to search for subdirectories
 * @param targetDirName The specific directory name to look for and copy
 * @param destBaseDir The destination base directory where directories will be copied
 *
 * @note Uses recursive copy options to copy entire directory trees
 * @note Creates the destination directory structure as needed
 */
// Helper function to copy specific named directories (equivalent to copy_a_dir)
static void copySpecificDirectories(const std::string &baseDir, const std::string &targetDirName,
									const std::string &destBaseDir)
{
	try {
		if (!std::filesystem::exists(baseDir) || !std::filesystem::is_directory(baseDir)) {
			return;
		}

		for (const auto &entry : std::filesystem::directory_iterator(baseDir)) {
			if (entry.is_directory()) {
				std::string subDir = entry.path().string();

				// Look for the target directory within this subdirectory
				std::string targetPath = subDir + "/" + targetDirName;
				if (std::filesystem::exists(targetPath) && std::filesystem::is_directory(targetPath)) {
					std::string destDir = destBaseDir + baseDir + "/" + entry.path().filename().string();
					std::filesystem::create_directories(destDir);

					try {
						std::filesystem::copy(targetPath, destDir + "/" + targetDirName,
											  std::filesystem::copy_options::recursive |
												  std::filesystem::copy_options::overwrite_existing);
					} catch (const std::filesystem::filesystem_error &ex) {
						ERR("Failed to copy directory %s: %s\n", targetPath.c_str(), ex.what());
					}
				}
			}
		}
	} catch (const std::filesystem::filesystem_error &ex) {
		ERR("Error accessing base directory %s: %s\n", baseDir.c_str(), ex.what());
	}
}

/**
 * @brief Copies system files from /sys directories for debugging purposes
 *
 * This function copies various system files from /sys/class/drm and
 * /sys/kernel/debug/dri directories that are useful for GPU debugging.
 * It replaces the previous shell script approach with native C++ filesystem
 * operations for better performance and reliability.
 *
 * @param uuid The unique identifier used to create the destination directory path
 *
 * @note This function copies files from:
 *       - /sys/class/drm subdirectories
 *       - /sys/kernel/debug/dri subdirectories
 *       - Specific xe_params directories
 */
// Helper function to copy system files from /sys directories
static void copySysFiles(const std::string &uuid)
{
	// Copy system files using C++ filesystem operations instead of shell script
	std::string destDir = "/var/tmp/xpum-" + uuid;

	// Copy files from /sys/class/drm subdirectories
	copyFilesFromSubdirectories("/sys/class/drm", destDir);

	// Copy files from /sys/kernel/debug/dri subdirectories
	copyFilesFromSubdirectories("/sys/kernel/debug/dri", destDir);

	// Copy specific xe_params directories
	copySpecificDirectories("/sys/kernel/debug/dri", "xe_params", destDir);
}

/**
 * @brief Copies a file if it exists using the C++ filesystem API
 *
 * This function checks if a source file exists and copies it to the destination
 * if found. It uses the safeCopyFile function to handle special kernel debug
 * files that may not be readable through standard operations.
 *
 * @param source The source file path to copy from
 * @param destination The destination file path to copy to
 * @return true if the file existed and was successfully copied, false otherwise
 *
 * @note This function will not fail if the source file doesn't exist
 * @note Uses enhanced error handling for special system files
 */
// Helper function to copy a file using C++ filesystem API
static bool copyFileIfExists(const std::string &source, const std::string &destination)
{
	try {
		if (std::filesystem::exists(source)) {
			return safeCopyFile(source, destination);
		}
	} catch (const std::filesystem::filesystem_error &ex) {
		// Log error if needed, but continue with other files
		ERR("Failed to copy %s: %s\n", source.c_str(), ex.what());
	}
	return false;
}

/**
 * @brief Generates a UUID by reading from the kernel's random UUID generator
 *
 * This function reads a UUID from /proc/sys/kernel/random/uuid and stores
 * it in the provided string reference. The UUID is used to create unique
 * temporary directory names for log collection.
 *
 * @param uuid Reference to a string where the generated UUID will be stored
 * @return 0 on success, -1 on failure (file couldn't be opened or UUID is empty)
 *
 * @note The function removes any trailing newline characters from the UUID
 * @note Uses the kernel's built-in UUID generation mechanism
 */
static int getUUID(std::string &uuid)
{
	std::ifstream uuidFile("/proc/sys/kernel/random/uuid");
	if (!uuidFile.is_open()) {
		return -1; // Failed to open file
	}

	std::getline(uuidFile, uuid);
	uuidFile.close();

	// Remove trailing newline if present
	if (!uuid.empty() && uuid.back() == '\n') {
		uuid.pop_back();
	}

	return uuid.empty() ? -1 : 0; // Return 0 on success, -1 on failure
}

/**
 * @brief Copies various system files and logs for debugging purposes
 *
 * This function collects a comprehensive set of system files and logs that
 * are useful for debugging GPU and system issues. It copies files from /proc,
 * /etc, /var/log, and /sys directories to a temporary directory structure.
 *
 * The function copies:
 * - Process information files from /proc
 * - System configuration files from /etc
 * - Kernel log files from /var/log
 * - GPU-related system files from /sys directories
 *
 * @param uuid The UUID string used to identify the temporary directory
 * @return 0 on success, non-zero error code on failure
 *
 * @note Creates a "/proc" subdirectory within the temporary directory
 * @note Uses the safeCopyFile function to handle special kernel debug files
 * @note Continues operation even if some files cannot be accessed
 */
static int copyFiles(const std::string &uuid)
{
	std::string dirName = "/var/tmp/xpum-" + uuid + "/proc";

	// Create directory using std::filesystem instead of mkdir
	std::error_code ec;
	if (!std::filesystem::create_directories(dirName, ec)) {
		ERR("Failed to create directory %s: %s\n", dirName.c_str(), ec.message().c_str());
		return -1;
	}

	// Copy proc files using filesystem API instead of shell commands
	copyFileIfExists("/proc/cpuinfo", dirName + "/cpuinfo");
	copyFileIfExists("/proc/interrupts", dirName + "/interrupts");
	copyFileIfExists("/proc/meminfo", dirName + "/meminfo");
	copyFileIfExists("/proc/modules", dirName + "/modules");
	copyFileIfExists("/proc/version", dirName + "/version");
	copyFileIfExists("/proc/pci", dirName + "/pci");
	copyFileIfExists("/proc/iomem", dirName + "/iomem");
	copyFileIfExists("/proc/mtrr", dirName + "/mtrr");
	copyFileIfExists("/proc/cmdline", dirName + "/cmdline");

	dirName = "/var/tmp/xpum-" + uuid;
	copyFileIfExists("/etc/os-release", dirName + "/os-release");
	copyFileIfExists("/var/log/syslog", dirName + "/syslog");

	// Copy kernel log files using filesystem API instead of shell commands
	try {
		for (const auto &entry : std::filesystem::directory_iterator("/var/log")) {
			if (entry.is_regular_file()) {
				std::string filename = entry.path().filename().string();
				// Check if filename starts with "kern" and ends with ".log"
				if (filename.starts_with("kern") && filename.ends_with(".log")) {
					std::string source = entry.path().string();
					std::string destination = dirName + "/" + filename;
					copyFileIfExists(source, destination);
				}
			}
		}
	} catch (const std::filesystem::filesystem_error &ex) {
		// Continue if directory doesn't exist or can't be accessed
		ERR("Failed to scan /var/log for kern*.log files: %s\n", ex.what());
	}

	// Copy system files from /sys directories
	copySysFiles(uuid);

	return 0; // Return 0 on success, as individual file copy failures are not fatal
}

/**
 * @brief Generates driver information for debugging purposes
 *
 * This function collects GPU driver-related information including kernel version,
 * xe module information, and DRI device listings. The information is written
 * to a "driver-info" file in the temporary directory.
 *
 * Information collected includes:
 * - Kernel version extracted from /proc/version
 * - xe graphics driver module path and status
 * - List of DRI (Direct Rendering Infrastructure) devices
 *
 * @param uuid The UUID string used to identify the temporary directory
 * @return 0 on success, non-zero on failure
 *
 * @note Uses filesystem APIs instead of shell commands for better reliability
 * @note Handles cases where the xe module may not be loaded
 * @note Output file: "/var/tmp/xpum-<uuid>/driver-info"
 */
namespace {
// Extract kernel version parsing into separate function
std::string getKernelVersion()
{
	struct utsname info;
	if (uname(&info) == 0) {
		return std::string{info.release};
	}
	return "Unknown kernel version";
}

// Couple all parts of the kernel module we're interested in reporting
struct ModuleInfo
{
	bool is_loaded = false;
	std::string path;
	std::string status;
};

ModuleInfo checkXeModule(const std::string &kernel_version)
{
	ModuleInfo info;

	// Check if module is loaded
	std::ifstream file("/proc/modules");
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			if (line.find("xe ") == 0) { // starts_with equivalent
				info.is_loaded = true;
				info.status = "xe module is loaded\n";
				break;
			}
		}
	}

	// Try to find module file paths
	const std::vector<std::string> module_extensions = {".ko", ".ko.xz"};
	const std::string base_path = "/lib/modules/" + kernel_version + "/kernel/drivers/gpu/drm/xe/xe";

	for (const auto &ext : module_extensions) {
		const std::string module_path = base_path + ext;
		if (std::filesystem::exists(module_path)) {
			info.path = "Module path: " + module_path + "\n";
			break;
		}
	}

	if (info.status.empty() && info.path.empty()) {
		info.status = "xe module not found or not loaded";
	}

	return info;
}

std::string getDriListing()
{
	const std::string dri_path = "/dev/dri";

	try {
		if (!std::filesystem::exists(dri_path) || !std::filesystem::is_directory(dri_path)) {
			return "/dev/dri directory not found\n";
		}

		std::string result;
		for (const auto &entry : std::filesystem::directory_iterator(dri_path)) {
			result += entry.path().filename().string() + "\n";
		}
		return result;

	} catch (const std::filesystem::filesystem_error &ex) {
		return "Error accessing /dev/dri: " + std::string(ex.what()) + "\n";
	}
}
} // namespace

// Helper function to generate driver information
static int generateDriverInfo(const std::string &uuid)
{
	const auto output_file = std::filesystem::path("/var/tmp") / ("xpum-" + uuid) / "driver-info";

	std::ofstream output(output_file);
	if (!output) {
		return -1; // Failed to open output file
	}

	// Get kernel version
	const auto kernel_version = getKernelVersion();
	output << "Kernel Version:\n" << kernel_version << "\n";

	// Get module information
	const auto module_info = checkXeModule(kernel_version);
	output << "modinfo -n xe\n" << module_info.status << module_info.path;

	// Get DRI listing
	const auto dri_listing = getDriListing();
	output << "\nls /dev/dri\n" << dri_listing;

	return 0;
}

/**
 * @brief Generates kernel message output for debugging purposes
 *
 * This function collects kernel messages (dmesg output) by reading from
 * available log files rather than executing the dmesg command. It tries
 * multiple sources to ensure kernel messages are captured.
 *
 * Sources checked (in order):
 * - /var/log/dmesg (if available)
 * - /var/log/kern.log (fallback)
 *
 * @param uuid The UUID string used to identify the temporary directory
 * @return 0 on success, non-zero on failure
 *
 * @note Avoids shell command execution for better security and reliability
 * @note Provides fallback options if primary log sources are unavailable
 * @note Output file: "/var/tmp/xpum-<uuid>/dmesg-output"
 */
// Helper function to generate dmesg output
static int generateDmesgOutput(const std::string &uuid)
{
	std::string fileName = "/var/tmp/xpum-" + uuid + "/dmesg-output";
	std::ofstream os(fileName);

	// Get dmesg output without executing dmesg command
	std::string dmesgOutput;
	bool dmesgRead = false;

	const std::vector<std::string> logPaths = {"/var/log/dmesg", "/var/log/kern.log"};
	for (const auto &path : logPaths) {
		std::ifstream file(path);
		if (file.is_open()) {
			std::string line;
			while (std::getline(file, line)) {
				dmesgOutput += line + "\n";
			}
			dmesgRead = true;
			break;
		}
	}
	if (!dmesgRead) {
		dmesgOutput = "Unable to read kernel messages from log files";
	}

	os << "Kernel Messages:\n" + dmesgOutput;
	os.close();
	return 0;
}

/**
 * @brief Generates package information for debugging purposes
 *
 * This function collects information about installed Intel-related packages
 * by querying the system's package manager. It supports both Debian-based
 * (dpkg) and Red Hat-based (rpm) package management systems.
 *
 * Package managers supported:
 * - dpkg (Debian/Ubuntu systems) - searches for Intel packages
 * - rpm (Red Hat/SUSE systems) - searches for Intel packages
 *
 * @param uuid The UUID string used to identify the temporary directory
 * @return 0 on success, non-zero on failure
 *
 * @note Automatically detects the available package manager
 * @note Continues operation even if no Intel packages are found
 * @note Output file: "/var/tmp/xpum-<uuid>/package-info"
 */
// Helper function to generate package information
static int generatePackageInfo(const std::string &uuid)
{
	std::string fileName = "/var/tmp/xpum-" + uuid + "/package-info";
	std::ofstream os(fileName);

	std::string packageInfo;
	bool foundPackages = false;

	// Check for dpkg (Debian/Ubuntu)
	if (std::filesystem::exists("/usr/bin/dpkg")) {
		try {
			std::string cmd = "dpkg -l | grep -i intel";
			SystemCommandResult scr = execCommand(cmd.c_str());
			packageInfo += "=== Debian/Ubuntu packages (dpkg) ===\n";
			packageInfo += scr.output() + "\n";
			foundPackages = true;
		} catch (const std::exception &ex) {
			packageInfo += "Error querying dpkg: " + std::string(ex.what()) + "\n";
		}
	}

	// Check for rpm (RedHat/SUSE)
	if (std::filesystem::exists("/usr/bin/rpm")) {
		try {
			std::string cmd = "rpm -qa | grep -i intel";
			SystemCommandResult scr = execCommand(cmd.c_str());
			packageInfo += "=== RPM packages ===\n";
			packageInfo += scr.output() + "\n";
			foundPackages = true;
		} catch (const std::exception &ex) {
			packageInfo += "Error querying rpm: " + std::string(ex.what()) + "\n";
		}
	}

	if (!foundPackages) {
		packageInfo = "No supported package manager found or no Intel packages detected\n";
	}

	os << packageInfo;
	os.close();
	return 0;
}

/**
 * @brief Generates comprehensive system information for debugging purposes
 *
 * This function collects detailed system hardware and software information
 * that is useful for debugging GPU and system issues. It combines both
 * filesystem-based data collection and command execution for comprehensive
 * system profiling.
 *
 * Information collected includes:
 * - PCI device information (from /proc/pci or lspci command)
 * - DMI/SMBIOS system information (dmidecode)
 * - USB device information (from /proc or lsusb command)
 * - XPU discovery information (xpu-smi)
 * - OpenCL platform information (clinfo)
 * - Video acceleration information (vainfo)
 *
 * @param uuid The UUID string used to identify the temporary directory
 * @return 0 on success, non-zero on failure
 *
 * @note Uses filesystem APIs where possible, falls back to commands when needed
 * @note Continues operation even if some information sources are unavailable
 * @note Output file: "/var/tmp/xpum-<uuid>/system-info"
 */
// Helper function to generate system information
static int generateSystemInfo(const std::string &uuid)
{
	std::string fileName = "/var/tmp/xpum-" + uuid + "/system-info";
	std::ofstream os(fileName);

	// clang-format off
	constexpr std::array cmds{
		"lspci -v -xxx",
		"dmidecode 2>&1",
		"lsusb 2>&1",
		"xpu-smi discovery 2>&1",
		"clinfo 2>&1",
		"vainfo 2>&1",
	};
	// clang-format on

	for (const auto &cmd : cmds) {
		SystemCommandResult scr = execCommand(cmd);
		os << cmd << "\n" << scr.output();
		if (&cmd != &cmds.back()) {
			os << "\n";
		}
	}

	os.close();
	return 0;
}

/**
 * @brief Generates diagnostic command output files for debugging
 *
 * This function orchestrates the generation of four different types of
 * diagnostic information by calling specialized helper functions. Each
 * helper function generates a specific category of debugging information
 * and writes it to a separate file.
 *
 * Generated files:
 * - driver-info: GPU driver and kernel information
 * - dmesg-output: Kernel messages and logs
 * - package-info: Installed Intel package information
 * - system-info: Comprehensive hardware and software information
 *
 * @param uuid The UUID string used to identify the temporary directory
 * @return Sum of all helper function return codes (0 for complete success)
 *
 * @note All files are created in "/var/tmp/xpum-<uuid>/" directory
 * @note Uses modern C++ approaches instead of shell script execution
 * @note Designed for modular and maintainable diagnostic data collection
 */
static int genCmdOut(const std::string &uuid)
{
	/*
	 * This function writes information to 4 different log files at
	 * /var/tmp/<uuid>/driver-info,dmesg-output,package-info,system-info
	 */

	// Generate each type of diagnostic information using helper functions
	int result = 0;

	result += generateDriverInfo(uuid);
	result += generateDmesgOutput(uuid);
	result += generatePackageInfo(uuid);
	result += generateSystemInfo(uuid);

	return result;
}

/**
 * @brief Creates a compressed archive of collected diagnostic logs
 *
 * This function creates a gzipped tar archive containing all the diagnostic
 * files and directories that were collected in the temporary directory.
 * The archive is created at the specified file path for easy distribution
 * and analysis.
 *
 * @param uuid The UUID string identifying the temporary directory to archive
 * @param fileName The output path where the compressed archive will be created
 * @return Exit status from the tar command (0 for success, non-zero for failure)
 *
 * @note Uses tar with gzip compression (-czf flags)
 * @note Archive is created from /var/tmp/ directory to maintain relative paths
 * @note The archived directory name will be "xpum-<uuid>"
 */
static int tarBall(const std::string &uuid, const std::string fileName)
{
	std::string cmd = "tar -C /var/tmp/ -czf " + fileName + " xpum-" + uuid;
	SystemCommandResult scr = execCommand(cmd.c_str());
	return scr.exitStatus();
}

/**
 * @brief Main function to collect Linux system logs and create a diagnostic archive
 *
 * This is the primary entry point for the Linux log collection functionality.
 * It orchestrates the entire process of collecting diagnostic information,
 * creating a compressed archive, and cleaning up temporary files.
 *
 * The process follows these steps:
 * 1. Generate a unique UUID for the temporary directory
 * 2. Create a temporary directory for log collection
 * 3. Copy system files and logs to the temporary directory
 * 4. Generate diagnostic command outputs
 * 5. Create a compressed tar archive of all collected data
 * 6. Clean up the temporary directory
 *
 * @param fileName The output path where the compressed log archive will be created
 * @return Error code indicating success (0) or the specific step that failed
 *
 * @retval 0 Success - all operations completed successfully
 * @retval ERR_UUID (-1) Failed to generate UUID
 * @retval ERR_TMP_DIR (-2) Failed to create temporary directory
 * @retval ERR_COPY_FILES (-3) Failed to copy system files
 * @retval ERR_GEN_OUT (-5) Failed to generate diagnostic outputs
 * @retval ERR_TAR (-6) Failed to create compressed archive
 * @retval ERR_RM_TMP (-7) Failed to remove temporary directory
 *
 * @note Uses a goto-based error handling pattern for cleanup
 * @note All operations are designed to be safe and handle missing files gracefully
 * @note The resulting archive contains comprehensive system diagnostic information
 */
int getLinLogs(const std::string &fileName)
{
	std::string uuid;
	if (getUUID(uuid) != 0 || uuid.empty() == true) {
		return ERR_UUID;
	}
	TempDirectory tmpDir{uuid};
	if (!tmpDir.is_valid()) {
		return ERR_TMP_DIR;
	}
	if (copyFiles(uuid) != 0) {
		return ERR_COPY_FILES;
	}
	if (genCmdOut(uuid) != 0) {
		return ERR_GEN_OUT;
	}
	if (tarBall(uuid, fileName) != 0) {
		return ERR_TAR;
	}
	return 0;
}
