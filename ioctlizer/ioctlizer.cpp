// ioctlizer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

NTSTATUS TryOpenDevice(TCHAR* deviceName, PHANDLE deviceHandle, PBYTE accessGranted);
NTSTATUS OpenDeviceTryNamespace(TCHAR* deviceName, DWORD requiredAccess, PHANDLE deviceHandle);
NTSTATUS OpenDevice(TCHAR* deviceName, DWORD requiredAccess, BOOLEAN withNamespace, PHANDLE deviceHandle);
NTSTATUS ConvertAccessToString(BYTE access, TCHAR* accessStr);
void ConvertErrorCodeToErrorString(NTSTATUS code);


#define MALLOC_CHECK(var, typ, len) do { \
										if (((var) = (typ)malloc((len))) == NULL) { \
											return STATUS_INSUFFICIENT_RESOURCES; } \
									} while (0);

#define VERIFY_HANDLE(hnd) do { \
							if (!(hnd) || INVALID_HANDLE_VALUE == hnd) { \
								return STATUS_INVALID_HANDLE; }\
							} while (0);


NTSTATUS OpenDevice(TCHAR* deviceName, DWORD requiredAccess, BOOLEAN withNamespace, PHANDLE deviceHandle) {
	TCHAR *namesp = TEXT("\\Namespace"), *tmpDeviceName;
	SIZE_T size = (lstrlen(deviceName) * sizeof(TCHAR)) + (lstrlen(namesp) * sizeof(TCHAR)) + sizeof(TCHAR);
	NTSTATUS status = STATUS_SUCCESS;
	
	//tmpDeviceName = (TCHAR*)malloc(size);
	MALLOC_CHECK(tmpDeviceName, TCHAR*, size);
	// add check
	memset(tmpDeviceName, 0x00, size);
	if (!SUCCEEDED(StringCchCopy(tmpDeviceName, size / sizeof(TCHAR), deviceName))) {
		printf("[**] OpenDevice: Failed to copy device name into temp device name\n");
		status = STATUS_INSUFFICIENT_RESOURCES;
	}

	if (withNamespace) {
		if (!SUCCEEDED(StringCchCat(tmpDeviceName, size, namesp))) {
			printf("[**] OpenDevice: Failed to concat namespace into device name\n");
			status = STATUS_INSUFFICIENT_RESOURCES;
		}

		*deviceHandle = CreateFile(tmpDeviceName, requiredAccess, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	else {
		*deviceHandle = CreateFile(tmpDeviceName, requiredAccess, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	VERIFY_HANDLE(*deviceHandle);

	free(tmpDeviceName);
	return status;
}


NTSTATUS OpenDeviceTryNamespace(TCHAR* deviceName, DWORD requiredAccess, PHANDLE deviceHandle) {
	NTSTATUS status = STATUS_ACCESS_DENIED;
	HANDLE devHandle = NULL, tmpHandle = NULL;

	status = OpenDevice(deviceName, requiredAccess, FALSE, &tmpHandle);
	if (status >= 0 && tmpHandle != NULL && tmpHandle != INVALID_HANDLE_VALUE) { // succeeded
		devHandle = tmpHandle;
		status = STATUS_SUCCESS;
	}
	else {
		status = OpenDevice(deviceName, requiredAccess, TRUE, &tmpHandle);
		if (status >= 0 && tmpHandle != NULL && tmpHandle != INVALID_HANDLE_VALUE) { // succeeded
			devHandle = tmpHandle;
			status = STATUS_SUCCESS;
		}
	}

	*deviceHandle = devHandle;
	return status;
}



NTSTATUS TryOpenDevice(TCHAR* deviceName, PHANDLE deviceHandle, PBYTE accessGranted) {
	// accessGranted will be given a value according to the type of possible access rights:
	// 0 -> ANY
	// 1 -> R
	// 2 -> W
	// 3 -> RW
	// 4 -> ALL
	HANDLE hndDevice = NULL, tmpHndDevice = NULL;
	NTSTATUS status;
	*accessGranted = 5;

	// First, try opening for ANY_ACCESS:
	status = OpenDeviceTryNamespace(deviceName, FILE_ANY_ACCESS, &tmpHndDevice);
	if (status >= 0 && tmpHndDevice != NULL && tmpHndDevice != INVALID_HANDLE_VALUE) { // succeeded
		hndDevice = tmpHndDevice;
		*accessGranted = 0;
	}

	status = OpenDeviceTryNamespace(deviceName, GENERIC_READ, &tmpHndDevice);
	if (status >= 0 && tmpHndDevice != NULL && tmpHndDevice != INVALID_HANDLE_VALUE) { // succeeded
		hndDevice = tmpHndDevice;
		*accessGranted = 1;
	}

	status = OpenDeviceTryNamespace(deviceName, GENERIC_WRITE, &tmpHndDevice);
	if (status >= 0 && tmpHndDevice != NULL && tmpHndDevice != INVALID_HANDLE_VALUE) { // succeeded
		hndDevice = tmpHndDevice;
		*accessGranted = 2;
	}

	status = OpenDeviceTryNamespace(deviceName, GENERIC_READ | GENERIC_WRITE, &tmpHndDevice);
	if (status >= 0 && tmpHndDevice != NULL && tmpHndDevice != INVALID_HANDLE_VALUE) { // succeeded
		hndDevice = tmpHndDevice;
		*accessGranted = 3;
	}


	*deviceHandle = hndDevice;
	if (*accessGranted == 5) {
		return STATUS_ACCESS_DENIED;
	}

	return STATUS_SUCCESS;
}


NTSTATUS ConvertAccessToString(BYTE access, TCHAR* accessStr) {
	if (access == 0) {
		if (!lstrcpy(accessStr, TEXT("FILE_ANY_ACCESS"))) {
			return STATUS_INVALID_PARAMETER;
		}
	}
	else if (access == 1) {
		if (!lstrcpy(accessStr, TEXT("FILE_READ_ACCESS"))) {
			return STATUS_INVALID_PARAMETER;
		}
	}
	else if (access == 2) {
		if (!lstrcpy(accessStr, TEXT("FILE_WRITE_ACCESS"))) {
			return STATUS_INVALID_PARAMETER;
		}
	}
	else if (access == 3) {
		if (!lstrcpy(accessStr, TEXT("FILE_READ_ACCESS | FILE_WRITE_ACCESS"))) {
			return STATUS_INVALID_PARAMETER;
		}
	}
	else if (access == 4) {
		if (!lstrcpy(accessStr, TEXT("FILE_ALL_ACCESS"))) {
			return STATUS_INVALID_PARAMETER;
		}
	}

	return STATUS_SUCCESS;
}


void ConvertErrorCodeToErrorString(NTSTATUS code, TCHAR* errStr) {
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		errStr, 0x200, NULL);
	return;
}


int main(int argc, char* argv[])
{
	TCHAR *deviceName = NULL, *fullName = NULL, *namesp = TEXT("\\Namespace"), err[0x201] = { 0 };
	int nameSize = 0, fullSize = 0, namespSize = lstrlen(namesp), totalSize;
	NTSTATUS status = STATUS_SUCCESS;
	HANDLE hndDevice = INVALID_HANDLE_VALUE;
	BYTE accessGranted = NULL;

	for (int idx = 1; idx < argc; ++idx) {
		nameSize = MultiByteToWideChar(CP_UTF8, NULL, argv[idx], strlen(argv[idx]), NULL, 0);
		nameSize *= sizeof(TCHAR);
		totalSize = nameSize + namespSize*sizeof(TCHAR) + sizeof(TCHAR);
		
		deviceName = (TCHAR*)malloc(totalSize);
		if (!deviceName) {
			printf("Failed to allocate memory for device %s", argv[idx]);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		memset(deviceName, 0x00, totalSize);
		MultiByteToWideChar(CP_UTF8, NULL, argv[idx], strlen(argv[idx]), deviceName, nameSize / sizeof(TCHAR));
		
		status = TryOpenDevice(deviceName, &hndDevice, &accessGranted);
		if (status < 0) {
			ConvertErrorCodeToErrorString(status, err);
			printf("[--] FAILURE opening device %ws: %ws\n", deviceName, err);
			free(deviceName);
			continue;
		}
		
		TCHAR accessStr[100] = { 0 };
		if (ConvertAccessToString(accessGranted, accessStr) < 0) {
			printf("[++] SUCCESS opening device %ws with rights %x\n", deviceName, accessGranted);
		}
		else {
			printf("[++] SUCCESS opening device %ws with rights %ws\n", deviceName, accessStr);
		}

		free(deviceName);
	}

    return 0;
}

