// Dll1.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Dll1.h"

HANDLE completionPort;

typedef struct AssyncCTX{
	HANDLE file;
	CHAR buffer;
	OVERLAPPED ov;
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

typedef struct aio_dev {
	HANDLE handle;
	OVERLAPPED overlapped;
	AsyncCallback callback;
	PAssyncCTX context;
} AIO_DEV, *PAIO_DEV;



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
		uintptr_t r = _beginthreadex(NULL, 0, workerThreadFunc, ((PAssyncCTX)completionPort)->file, 0, NULL);
		assert(r != 0);
	}
}

PAIO_DEV FileAsync(PCSTR filename, BOOL readWrite) {
	PAIO_DEV aio = (PAIO_DEV)malloc(sizeof(AIO_DEV));
	if(readWrite)
		aio->handle = CreateFile((LPCTSTR)filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	else
		aio->handle = CreateFile((LPCTSTR)filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	ZeroMemory(&aio->overlapped, sizeof(OVERLAPPED));
	return aio;
}

BOOL AssociateDeviceWithCompletionPort(HANDLE hComplPort, HANDLE hDevice, DWORD CompletionKey) {
	HANDLE h = CreateIoCompletionPort(hDevice, hComplPort, CompletionKey, 0);
	return h == hComplPort;
}

BOOL AsyncInit() {
	((PAssyncCTX)completionPort)->file = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, GetNumberOfThreads());
	if (((PAssyncCTX)completionPort)->file = NULL) return FALSE;
	CreateThreadPool();
	return TRUE;
}

BOOL funcCallback(LPVOID ctx, DWORD status, UINT64 transferedBytes) {

}

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx) {

	PAIO_DEV dev = FileAsync(srcFile, FALSE);
	PAIO_DEV devDest = FileAsync(dstFile, TRUE);

	LARGE_INTEGER pos;
	LPOVERLAPPED ovr = &dev->overlapped;
	pos.HighPart = ovr->OffsetHigh;
	pos.LowPart = ovr->Offset;
	pos.QuadPart += (DWORD)(dev->handle);
	ovr->OffsetHigh = pos.HighPart;
	ovr->Offset = pos.LowPart;

	dev->overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	PCHAR bBuffer = (PCHAR)malloc(BUFFER_SIZE);
	AssyncCTX context = { devDest->handle, *bBuffer, dev->overlapped };

	//BOOL ret = ReadAsyncIO(dev, *bBuffer, BUFFER_SIZE, &readCallback, &context);

	WaitForSingleObject(dev->overlapped.hEvent, INFINITE);
	return TRUE;
}

VOID AsyncTerminate() {
	CloseHandle(completionPort);
}