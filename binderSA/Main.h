#include <Windows.h>
#include <cstdint>
#include <map>
#include <iostream>

// DirectX
#include <d3d9.h>
#pragma comment (lib, "d3d9.lib")

// GUI
#include "ImGui\ImGui.h"
#include "ImGui\ImGui_ImplDX9.h"
#include "ImGui\ImGui_ImplWin32.h"

// Hooks
#include "Hooks\CBaseHook.h"

// Etc
#include "Procedures.h"