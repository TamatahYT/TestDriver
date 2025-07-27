#include <stdio.h>
#include <Windows.h>
#include "..\TestDriver\Header.h"
#include <Psapi.h>

void Err(const char* customMessage) {
	DWORD errorCode = GetLastError();
	LPSTR messageBuffer = NULL;

	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&messageBuffer,
		0,
		NULL
	);

	printf("%s (%u): %s\n", customMessage, errorCode, messageBuffer);
	LocalFree(messageBuffer);
}
void DumpProcessModule(HANDLE hProcess) {
	HMODULE hModules[4096];
	DWORD out;
	if (!EnumProcessModulesEx(hProcess, hModules, sizeof(hModules), &out, LIST_MODULES_ALL)) {
		Err("Error in enumprocessmodules");
		return;
	}
	DWORD count = out / sizeof(HMODULE);
	printf("%u modules\n", count);
	for (int i = 0; i < count; i++) {
		printf("HModule: 0x%p", hModules[i]);
		WCHAR name[MAX_PATH];
		if (GetModuleBaseName(hProcess, hModules[i], name, _countof(name))) {
			printf(" %ws", name);
		}
		printf("\n");
	}

}
int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("Usage: Client.exe <PID> <TID> <PRIORITY>");
		return 1;
	}
	int PID = atoi(argv[1]);
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, PID);
	if (hProcess) {
		DumpProcessModule(hProcess);
		CloseHandle(hProcess);
		return 0;
	}
	Err("Error OpenProcess in User-Land");

	HANDLE hDevice = CreateFile(L"\\\\.\\DriverTest", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		Err("Error opening handle");
		return 1;
	}
	USERINPUT input;
	input.procID = PID;
	USEROUTPUT output;
	DWORD bytes;

	BOOL ok = DeviceIoControl(hDevice, IOCTL_OPEN_PROCESS, &input, sizeof(input), &output, sizeof(output), &bytes, nullptr);
	if (!ok) {
		Err("Error DeviceIoControl Open Process");
		return 1;
	}
	DumpProcessModule(output.procHandle);
	CloseHandle(output.procHandle);

	ThreadData TD;
	TD.ThreadID = atoi(argv[2]);
	TD.priority = atoi(argv[3]);

	ok = DeviceIoControl(hDevice, IOCTL_BOOSTER_THREAD, &TD, sizeof(TD), nullptr, 0, &bytes, nullptr);
	if (!ok) {
		Err("Error DeviceIoControl Thread Booster");
		return 1;
	}

	CloseHandle(UlongToHandle(TD.ThreadID));

	CloseHandle(hDevice);
	return 0;
}
