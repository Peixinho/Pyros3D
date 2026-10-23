//============================================================================
// Name        : WindowsDisplay.cpp
// Author      : Duarte Peixinho
// Description : What Windows says of the display, for HDR output: the
//               monitor's peak (DXGI) and the user's SDR-content brightness
//               (Display settings' slider) - neither of which Vulkan tells.
//============================================================================

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_6.h>
#include <vector>

// The monitor the foreground window is on (else the primary): its peak in nits, and SDR white in
// nits (Windows' "SDR content brightness"). False when that monitor is not in HDR mode.
static bool QueryDisplayHDR(float* peakNits, float* sdrWhiteNits);

// (asked every frame - by the tonemap, by the present pass: the answer is kept a second, DXGI's walk
// of every adapter and output is not a per-frame thing)
extern "C" bool PyrosWindowsDisplayHDR(float* peakNits, float* sdrWhiteNits)
{
	static ULONGLONG askedAt = 0;
	static bool hdr = false;
	static float peak = 0.f, white = 0.f;
	const ULONGLONG now = GetTickCount64();
	if (askedAt == 0 || now - askedAt > 1000) { askedAt = now; peak = white = 0.f; hdr = QueryDisplayHDR(&peak, &white); }
	if (peakNits) *peakNits = peak;
	if (sdrWhiteNits) *sdrWhiteNits = white;
	return hdr;
}

static bool QueryDisplayHDR(float* peakNits, float* sdrWhiteNits)
{
	HMONITOR monitor = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
	bool found = false;
	IDXGIFactory1* factory = NULL;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)) || factory == NULL) return false;
	IDXGIAdapter1* adapter = NULL;
	for (UINT a = 0; !found && factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; a++)
	{
		IDXGIOutput* output = NULL;
		for (UINT o = 0; !found && adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; o++)
		{
			IDXGIOutput6* output6 = NULL;
			if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), (void**)&output6)) && output6 != NULL)
			{
				DXGI_OUTPUT_DESC1 d = {};
				if (SUCCEEDED(output6->GetDesc1(&d)) && d.Monitor == monitor && d.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
				{
					if (peakNits) *peakNits = d.MaxLuminance;
					found = true;
					// SDR white: the display path's SDR white level (in 1/1000ths of 80 nits)
					if (sdrWhiteNits)
					{
						*sdrWhiteNits = 0.f;
						MONITORINFOEXW mi = {}; mi.cbSize = sizeof(mi);
						GetMonitorInfoW(monitor, &mi);
						UINT32 nPaths = 0, nModes = 0;
						if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &nPaths, &nModes) == ERROR_SUCCESS)
						{
							std::vector<DISPLAYCONFIG_PATH_INFO> paths(nPaths);
							std::vector<DISPLAYCONFIG_MODE_INFO> modes(nModes);
							if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &nPaths, paths.data(), &nModes, modes.data(), NULL) == ERROR_SUCCESS)
								for (UINT32 p = 0; p < nPaths; p++)
								{
									DISPLAYCONFIG_SOURCE_DEVICE_NAME src = {};
									src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME; src.header.size = sizeof(src);
									src.header.adapterId = paths[p].sourceInfo.adapterId; src.header.id = paths[p].sourceInfo.id;
									if (DisplayConfigGetDeviceInfo(&src.header) != ERROR_SUCCESS || wcscmp(src.viewGdiDeviceName, mi.szDevice) != 0) continue;
									DISPLAYCONFIG_SDR_WHITE_LEVEL white = {};
									white.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL; white.header.size = sizeof(white);
									white.header.adapterId = paths[p].targetInfo.adapterId; white.header.id = paths[p].targetInfo.id;
									if (DisplayConfigGetDeviceInfo(&white.header) == ERROR_SUCCESS) *sdrWhiteNits = (float)white.SDRWhiteLevel / 1000.f * 80.f;
									break;
								}
						}
					}
				}
				output6->Release();
			}
			output->Release();
		}
		adapter->Release();
	}
	factory->Release();
	return found;
}
#endif
