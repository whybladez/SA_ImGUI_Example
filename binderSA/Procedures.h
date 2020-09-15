typedef void(__cdecl *mainLoop_t)();
typedef long(__thiscall *windowProc_t)(void *, HWND, UINT, WPARAM, LPARAM);
typedef long(__stdcall *d3dPresent_t)(IDirect3DDevice9 *, CONST RECT *, CONST RECT *, HWND, CONST RGNDATA *);
typedef long(__stdcall *resetDevice_t)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);

class cPlugin {
public:
	void initPlugin();
};

extern cPlugin * Plugin;