// injector_d3d9_loader.cpp : Defines the exported functions for the DLL application.
//

#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#if 1
#include <MMSystem.h>
#include <dsound.h>
#undef DirectSoundCreate8
#else
typedef void* LPCGUID;
typedef void* LPUNKNOWN;
#endif

#include "com-proxy.h"

HRESULT (__stdcall *DirectSoundCreate8_orig)(LPCGUID, LPDIRECTSOUND8*, LPUNKNOWN);

LARGE_INTEGER perfFrequency;

void onetime_setup(void) {
	static bool setup_complete = false;

	if (setup_complete)
		return;

	char path[MAX_PATH];
	if (GetSystemDirectoryA(path, sizeof(path)) == 0) {
		return;
	}
	strcat_s(path, sizeof(path), "\\dsound.dll");
	HMODULE dsound = LoadLibraryA(path);

	DirectSoundCreate8_orig = (HRESULT(__stdcall *)(LPCGUID, LPDIRECTSOUND8*, LPUNKNOWN))GetProcAddress(dsound, "DirectSoundCreate8");

	QueryPerformanceFrequency(&perfFrequency);

	setup_complete = true;
}

typedef struct {
	LARGE_INTEGER start_time;
	DWORD start_position;
	DWORD bytesPerSec;
} buf_ctx;

HRESULT __stdcall GetCurrentPosition(IDirectSoundBuffer* self, LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor) {
	buf_ctx* ctx = com_proxy_downcast(self)->ctx;
	IDirectSoundBuffer* real = com_proxy_downcast(self)->real;

	HRESULT ret = S_OK;

	// some samples are looped and filled dynamically, we don't want to fake those
	// Easiest to only affect things when the caller doesn't provide a write cursor
	if (pdwCurrentPlayCursor && !pdwCurrentWriteCursor && ctx->start_time.QuadPart && ctx->bytesPerSec) {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);

		double delta = now.QuadPart - ctx->start_time.QuadPart;
		delta /= (double)perfFrequency.QuadPart;
		delta *= ctx->bytesPerSec;
		*pdwCurrentPlayCursor = (DWORD)delta + ctx->start_position;
	} else {
		ret = IDirectSoundBuffer_GetCurrentPosition(real, pdwCurrentPlayCursor, pdwCurrentWriteCursor);
	}

	return ret;
}

HRESULT __stdcall SetCurrentPosition(IDirectSoundBuffer* self, DWORD dwNewPosition) {
	buf_ctx* ctx = com_proxy_downcast(self)->ctx;
	IDirectSoundBuffer* real = com_proxy_downcast(self)->real;

	ctx->start_position = dwNewPosition;
	QueryPerformanceCounter(&ctx->start_time);

	HRESULT ret = IDirectSoundBuffer_SetCurrentPosition(real, dwNewPosition);
	return ret;
}

HRESULT __stdcall Play(IDirectSoundBuffer* self, DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) {
	buf_ctx* ctx = com_proxy_downcast(self)->ctx;
	IDirectSoundBuffer* real = com_proxy_downcast(self)->real;

	QueryPerformanceCounter(&ctx->start_time);

	HRESULT ret = IDirectSoundBuffer_Play(real, dwReserved1, dwPriority, dwFlags);
	return ret;
}

void free_ctx(void* ctx) {
	free(ctx);
}

HRESULT __stdcall CreateSoundBuffer(IDirectSound8 *self, LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER* ppDSBuffer, LPUNKNOWN pUnkOuter) {
	IDirectSound8* real = com_proxy_downcast(self)->real;

	buf_ctx* ctx = calloc(sizeof(buf_ctx), 1);
	if (pcDSBufferDesc && pcDSBufferDesc->lpwfxFormat) {
		ctx->bytesPerSec = pcDSBufferDesc->lpwfxFormat->nAvgBytesPerSec;
	}

	HRESULT ret = IDirectSound8_CreateSoundBuffer(real, pcDSBufferDesc, ppDSBuffer, pUnkOuter);

	IDirectSoundBufferVtbl* api_vtbl;
	struct com_proxy* api_proxy;

	com_proxy_wrap(&api_proxy, *ppDSBuffer, sizeof(*(*ppDSBuffer)->lpVtbl));

	api_vtbl = api_proxy->vptr;
	api_proxy->ctx = ctx;
	api_proxy->cleanup_ctx = free_ctx;

	api_vtbl->GetCurrentPosition = GetCurrentPosition;
	api_vtbl->SetCurrentPosition = SetCurrentPosition;
	api_vtbl->Play = Play;

	*ppDSBuffer = (LPDIRECTSOUNDBUFFER)api_proxy;

	return ret;
}

HRESULT __stdcall DirectSoundCreateMe(
	LPCGUID lpcGuidDevice,
	LPDIRECTSOUND8* ppDS8,
	LPUNKNOWN pUnkOuter
) {
	onetime_setup();

	IDirectSound8Vtbl* api_vtbl;
	struct com_proxy* api_proxy;

	HRESULT ret = DirectSoundCreate8_orig(lpcGuidDevice, ppDS8, pUnkOuter);
	if (ret != DS_OK) {
		return ret;
	}

	com_proxy_wrap(&api_proxy, *ppDS8, sizeof(*(*ppDS8)->lpVtbl));

	api_vtbl = api_proxy->vptr;

	api_vtbl->CreateSoundBuffer = CreateSoundBuffer;

	*ppDS8 = (LPDIRECTSOUND8)api_proxy;

	return DS_OK;
}
