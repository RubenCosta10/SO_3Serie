// Dll1.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Dll1.h"


typedef struct AssyncCTX{
	HANDLE IoCTX;
}AssyncCTX, *PAssyncCTX;

// Estrutura do enunciado
typedef VOID(*AsyncCallback)(LPVOID userCtx, DWORD status, UINT64 transferedBytes);

DWORD GetNumberOfThreads() {
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors*2;
}

typedef struct {
	OVERLAPPED ov;
	PULONG func;
	PULONG arg;
} OPER, *POPER;

typedef struct callback_context {
	HANDLE file;
	CHAR buffer;
	OVERLAPPED ov;
} CB_CTX, *PCB_CTX;

typedef struct aio_dev {
	HANDLE handle;
	OVERLAPPED overlapped;
	AsyncCallback callback;
	PCB_CTX context;
} AIO_DEV, *PAIO_DEV;

PAssyncCTX completionPort;

uintptr_t WINAPI workerThreadFunc(LPVOID arg) {
	HANDLE completionPort = (HANDLE)arg;
	DWORD transferedBytes;
	PAIO_DEV dev;
	LPOVERLAPPED overlaped;

	for (;;) {
		if (GetQueuedCompletionStatus(completionPort, &transferedBytes, (PULONG_PTR)&dev, &overlaped, INFINITE) && GetLastError() != ERROR_HANDLE_EOF) {
			if (transferedBytes > 0) {
				LARGE_INTEGER pos;
				LPOVERLAPPED ovr = &dev->overlapped;
				pos.HighPart = ovr->OffsetHigh;
				pos.LowPart = ovr->Offset;
				pos.QuadPart += transferedBytes;
				ovr->OffsetHigh = pos.HighPart;
				ovr->Offset = pos.LowPart;
			}
			dev->callback(dev->context, 0, transferedBytes);
		}
	}
	return 0;
}

#define MAX_THREADS 512 
VOID CreateThreadPool() {
	for (DWORD i = 0; i < MAX_THREADS; i++) {
		uintptr_t r = _beginthreadex(NULL, 0, workerThreadFunc, (LPVOID)completionPort->IoCTX, 0, NULL);
		assert(r != 0);
	}
}

PAIO_DEV OpenFileAsync(PCSTR filename) {
	PAIO_DEV aio = (PAIO_DEV)malloc(sizeof(AIO_DEV));
	aio->handle = CreateFile((LPCTSTR)filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	ZeroMemory(&aio->overlapped, sizeof(OVERLAPPED));
	return aio;
}

PAIO_DEV CreateFileAsync(PCSTR filename) {
	PAIO_DEV aio = (PAIO_DEV)malloc(sizeof(AIO_DEV));
	aio->handle = CreateFile((LPCTSTR)filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	ZeroMemory(&aio->overlapped, sizeof(OVERLAPPED));
	return aio;
}


BOOL AsyncInit() {
	completionPort->IoCTX = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, GetNumberOfThreads());
	if (completionPort->IoCTX = NULL) return FALSE;
	CreateThreadPool();
	return TRUE;
}

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx) {
	PAIO_DEV dev = OpenFileAsync(srcFile);
	PAIO_DEV devDest = CreateFileAsync(dstFile);

	return TRUE;
}

VOID AsyncTerminate() {
	CloseHandle(completionPort);
}