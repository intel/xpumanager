/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <fs_lock.h>
#include "utility/scope_exit.h"
#include <debug.h> // NOLINT(misc-include-cleaner) — provides the DBG()/ERR() macros
#include <windows.h>
#include <sddl.h>
#include <cstdint>

namespace {

// The lock is a named semaphore inside a private namespace whose boundary requires
// the Administrators SID.  Unlike a file in C:/ProgramData or an object in the
// Global\ namespace, a non-elevated user cannot create or open this namespace,
// so it cannot pre-create or hold the lock to block an administrator update.
constexpr const wchar_t *NAMESPACE_ALIAS = L"xpum";
constexpr const wchar_t *LOCK_NAME = L"xpum\\firmware_update";

// Protected DACL: full access for Administrators and SYSTEM only.
constexpr const wchar_t *LOCK_SDDL = L"D:P(A;;GA;;;BA)(A;;GA;;;SY)";

/**
 * @brief Create the private namespace, or open it if another xpu-smi
 * instance already has.  Both fail for tokens without an enabled
 * Administrators SID.
 *
 * @param sa Security attributes applied when creating the namespace
 * @return HANDLE to the private namespace, or nullptr on failure
 */
HANDLE openNamespace(SECURITY_ATTRIBUTES *sa)
{
	HANDLE boundary = CreateBoundaryDescriptorW(NAMESPACE_ALIAS, 0);
	if (boundary == nullptr) {
		return nullptr;
	}
	BYTE sidBuf[SECURITY_MAX_SID_SIZE];
	DWORD sidSize = sizeof(sidBuf);
	HANDLE ns = nullptr;
	if (CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, sidBuf, &sidSize) &&
		AddSIDToBoundaryDescriptor(&boundary, sidBuf)) {
		ns = CreatePrivateNamespaceW(sa, boundary, NAMESPACE_ALIAS);
		if (ns == nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
			ns = OpenPrivateNamespaceW(boundary, NAMESPACE_ALIAS);
		}
	}
	// Preserve the failing call's error for the caller's GetLastError().
	const DWORD err = GetLastError();
	DeleteBoundaryDescriptor(boundary);
	SetLastError(err);
	return ns;
}

/**
 * @brief Check whether the process token is elevated (UAC "Run as administrator").
 *
 * @return true if the token is elevated, false otherwise or on query failure
 */
bool isElevated()
{
	HANDLE token = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
		return false;
	}
	TOKEN_ELEVATION elevation = {};
	DWORD size = 0;
	const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) != 0;
	CloseHandle(token);
	return ok && elevation.TokenIsElevated != 0;
}

} // namespace

/**
 * @brief On Windows, we use a named semaphore (max count 1) in an
 * administrator-only private namespace to create an exclusive lock.  Unlike a
 * mutex it has no thread affinity, so release() may run on any thread.  The OS
 * closes the handle if the process dies, so a crashed update never leaves a
 * stale lock behind.
 */
void FSLock::acquire()
{
	if (acquired) {
		DBG("firmware update lock already held; ignoring acquire()\n");
		return;
	}
	if (!isElevated()) {
		ERR("firmware update requires administrator privileges\n");
		return;
	}
	PSECURITY_DESCRIPTOR sd = nullptr;
	if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(LOCK_SDDL, SDDL_REVISION_1, &sd, nullptr)) {
		DBG("cannot build lock security descriptor: {}\n", GetLastError());
		return;
	}
	ScopeExit freeSd{[sd] { LocalFree(sd); }};
	SECURITY_ATTRIBUTES sa = {sizeof(sa), sd, FALSE};
	// The guards below close whatever is still set on scope exit; on success
	// the handles are cleared once ownership has passed to this FSLock.
	HANDLE ns = openNamespace(&sa);
	ScopeExit closeNs{[&ns] {
		if (ns != nullptr) {
			ClosePrivateNamespace(ns, 0);
		}
	}};
	if (ns == nullptr) {
		DBG("cannot open firmware update lock namespace (administrator required): {}\n", GetLastError());
		return;
	}
	HANDLE sem = CreateSemaphoreW(&sa, 1, 1, LOCK_NAME);
	// Declared after closeNs so the semaphore is closed before the namespace.
	ScopeExit closeSem{[&sem] {
		if (sem != nullptr) {
			CloseHandle(sem);
		}
	}};
	if (sem == nullptr) {
		DBG("cannot create firmware update lock: {}\n", GetLastError());
		return;
	}
	// Non-blocking: a timeout means another update holds the lock.
	if (WaitForSingleObject(sem, 0) != WAIT_OBJECT_0) {
		return;
	}
	acquired = true;
	// Store HANDLEs as uintptr_t; there is no portable alternative to
	// round-tripping an opaque OS handle through an integer.
	// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
	handle = reinterpret_cast<uintptr_t>(sem);
	nsHandle = reinterpret_cast<uintptr_t>(ns);
	// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
	sem = nullptr;
	ns = nullptr;
}

/**
 * @brief Release the exclusive lock.  The namespace handle is kept open
 * until after the semaphore is released: closing it earlier would let another
 * process recreate an empty namespace and a second, unrelated semaphore.
 */
void FSLock::release()
{
	if (acquired) {
		// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
		HANDLE sem = reinterpret_cast<HANDLE>(handle);
		HANDLE ns = reinterpret_cast<HANDLE>(nsHandle);
		// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
		ReleaseSemaphore(sem, 1, nullptr);
		CloseHandle(sem);
		ClosePrivateNamespace(ns, 0);
	}
	handle = 0;
	nsHandle = 0;
	acquired = false;
}
