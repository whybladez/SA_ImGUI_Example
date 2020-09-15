#include "Main.h"

mainLoop_t mainLoopHandler;
mainLoop_t mainLoopTrampoline;
windowProc_t windowProcTrampoline;
d3dPresent_t presentTrampoline;
resetDevice_t resetDeviceTrampoline;

CBaseHook *hookGameUpdate;
CBaseHook *hookWindowProc;
CBaseHook *hookD3DPresent;
CBaseHook *hookResetDevice;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

long __stdcall d3dPresent(IDirect3DDevice9 *d3dDevice, CONST RECT *pSourceRect, CONST RECT *pDestRect,
	HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion)
{
	static bool guiInitialized = false;
	if (!guiInitialized)
	{
		auto direct3DDevice = *reinterpret_cast<IDirect3DDevice9 **>(0xC97C28);
		ImGui::CreateContext();

		ImGuiIO *ImIO = &ImGui::GetIO();
		if (ImIO != nullptr)
		{
			ImIO->IniFilename = nullptr;
			ImIO->LogFilename = nullptr;
		}

		ImGui_ImplDX9_Init(direct3DDevice);
		ImGui_ImplWin32_Init(*reinterpret_cast<HWND *>(0xC97C1C));

	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ImGui::Begin("Hello");
	ImGui::End();

	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	return presentTrampoline(d3dDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

long __stdcall resetDevice(IDirect3DDevice9 *d3dDevice, D3DPRESENT_PARAMETERS *d3dPresentParameters)
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
	auto returnResult = resetDeviceTrampoline(d3dDevice, d3dPresentParameters);
	ImGui_ImplDX9_CreateDeviceObjects();

	return returnResult;
}

void gameUpdate()
{
	static bool hooksInitialized = false;
	if (!hooksInitialized)
	{
		IDirect3DDevice9 *d3dDevice = *reinterpret_cast<IDirect3DDevice9 **>(0xC97C28);
		if (d3dDevice != nullptr)
		{
			void **d3dDeviceVTBL = *reinterpret_cast<void ***>(d3dDevice);
			if (d3dDeviceVTBL != nullptr)
			{
				hookD3DPresent = new CBaseHook(d3dDeviceVTBL[17], d3dPresent, 5);
				hookResetDevice = new CBaseHook(d3dDeviceVTBL[16], resetDevice, 5);

				presentTrampoline = hookD3DPresent->getTrampoline<d3dPresent_t>();
				resetDeviceTrampoline = hookResetDevice->getTrampoline<resetDevice_t>();

				hooksInitialized = true;
			}
		}
	}

	mainLoopTrampoline();
}

long __fastcall windowProcedure(void *pThis, int, HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMessage, wParam, lParam))
		return true;

	return windowProcTrampoline(pThis, hWnd, uMessage, wParam, lParam);
}

void cPlugin::initPlugin()
{
	hookGameUpdate = new CBaseHook(0x561B10, gameUpdate, 6);
	hookWindowProc = new CBaseHook(0x747EB0, windowProcedure, 9);

	mainLoopTrampoline = hookGameUpdate->getTrampoline<mainLoop_t>();
	windowProcTrampoline = hookWindowProc->getTrampoline<windowProc_t>();
}

cPlugin * Plugin = new cPlugin();