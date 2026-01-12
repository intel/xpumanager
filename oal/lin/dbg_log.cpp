/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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
#include "lin.h"

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
	bool shouldCleanup = true;
	bool createdSuccessfully = false;

public:
	explicit TempDirectory(std::string_view uuid) : dirPath{std::string("/var/tmp/xpum-") + std::string(uuid)}
	{
		std::error_code ec;
		createdSuccessfully = std::filesystem::create_directory(dirPath, ec);

		if (!createdSuccessfully) {
			ERR("Failed to create directory %s: %s\n", dirPath.string().c_str(), ec.message().c_str());
		}
	}

	~TempDirectory()
	{
		if (shouldCleanup && createdSuccessfully) {
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
		: dirPath(std::move(other.dirPath)), shouldCleanup(other.shouldCleanup),
		  createdSuccessfully(other.createdSuccessfully)
	{
		other.shouldCleanup = false;
	}
	TempDirectory &operator=(TempDirectory &&other) noexcept
	{
		if (this != &other) {
			dirPath = std::move(other.dirPath);
			shouldCleanup = other.shouldCleanup;
			createdSuccessfully = other.createdSuccessfully;
			other.shouldCleanup = false;
		}
		return *this;
	}

	const std::filesystem::path &path() const { return dirPath; }
	std::string uuid() const
	{
		return dirPath.filename().string().substr(5);
	} // Remove "xpum-", left with just the UUID
	bool isValid() const { return createdSuccessfully; }
	void keep() { shouldCleanup = false; } // Don't cleanup on destruction

	explicit operator bool() const { return createdSuccessfully; }
};
} // namespace

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
	bool isLoaded = false;
	std::string path;
	std::string status;
};

ModuleInfo checkXeModule(const std::string &kernelVersion)
{
	ModuleInfo info;

	// Check if module is loaded
	std::ifstream file("/proc/modules");
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			if (line.find("xe ") == 0) { // starts_with equivalent
				info.isLoaded = true;
				info.status = "xe module is loaded\n";
				break;
			}
		}
	}

	// Try to find module file paths
	const std::vector<std::string> moduleExtensions = {".ko", ".ko.xz"};
	const std::string basePath = "/lib/modules/" + kernelVersion + "/kernel/drivers/gpu/drm/xe/xe";

	for (const auto &ext : moduleExtensions) {
		const std::string modulePath = basePath + ext;
		if (std::filesystem::exists(modulePath)) {
			info.path = "Module path: " + modulePath + "\n";
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
	const std::string driPath = "/dev/dri";

	try {
		if (!std::filesystem::exists(driPath) || !std::filesystem::is_directory(driPath)) {
			return "/dev/dri directory not found\n";
		}

		std::string result;
		for (const auto &entry : std::filesystem::directory_iterator(driPath)) {
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
	const auto outputFile = std::filesystem::path("/var/tmp") / ("xpum-" + uuid) / "driver-info";

	std::ofstream output(outputFile);
	if (!output) {
		return -1; // Failed to open output file
	}

	// Get kernel version
	const auto kernelVersion = getKernelVersion();
	output << "Kernel Version:\n" << kernelVersion << "\n";

	// Get module information
	const auto moduleInfo = checkXeModule(kernelVersion);
	output << "modinfo -n xe\n" << moduleInfo.status << moduleInfo.path;

	// Get DRI listing
	const auto driListing = getDriListing();
	output << "\nls /dev/dri\n" << driListing;

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
	if (!tmpDir.isValid()) {
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
