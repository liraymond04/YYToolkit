#include "Present.hpp"
#include "../../Features/AUMI_API/Exports.hpp"
#include "../../Features/Menu/Menu.hpp"
#include "../../Utils/Error.hpp"

static void SetupDescriptor(DXGI_SWAP_CHAIN_DESC* pDesc)
{
	YYTKTrace(__FUNCTION__ "()", __LINE__);

	RValue Result;

	if (auto Status = AUMI_CallBuiltinFunction("window_handle", &Result, 0, 0, 0, 0))
		Utils::Error::Error(1, "Failed to get the window handle.\nError Code: %s", Utils::Error::YYTKStatus_ToString(Status).data());

	if (!Result.Pointer)
		Utils::Error::Error(1, "Failed to get the window handle.");

	pDesc->BufferCount = 1;
	pDesc->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	pDesc->BufferDesc = { 0, 0, { 60, 1 }, DXGI_FORMAT_R8G8B8A8_UNORM };
	pDesc->OutputWindow = (HWND)Result.Pointer;

	if (auto Status = AUMI_CallBuiltinFunction("window_get_fullscreen", &Result, 0, 0, 0, 0))
		Utils::Error::Error(1, "Unspecified error while calling window_get_fullscreen.\nError Code: %i", Status);

	pDesc->Windowed = !(bool)Result.Value;

	pDesc->Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	pDesc->SampleDesc = { 1, 0 };
	pDesc->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
}

namespace Hooks::Present
{
	HRESULT __stdcall Function(IDXGISwapChain* _this, unsigned int Sync, unsigned int Flags)
	{
		if (!pDevice)
		{
			RValue Result;
			if (auto Status = AUMI_CallBuiltinFunction("window_device", &Result, 0, 0, 0, 0))
			{
				Utils::Error::Error(1, "Failed to get the window device.\nError Code: %s", Utils::Error::YYTKStatus_ToString(Status).data());
			}

			pDevice = (decltype(pDevice))Result.Pointer;
			pDevice->GetImmediateContext(&pContext);
		}
		
		Tool::Menu::Initialize(_this, pDevice, pContext, &pView);

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		Tool::Menu::Run();

		ImGui::Render();

		pContext->OMSetRenderTargets(1, &pView, 0);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		ImGui::EndFrame();

		return pfnOriginal(_this, Sync, Flags);
	}

	void* GetTargetAddress()
	{
		YYTKTrace(__FUNCTION__ "()", __LINE__);

		using Fn = PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN;

		HMODULE Module = GetModuleHandleA("d3d11.dll");

		if (!Module)
			Utils::Error::Error(1, "Cannot obtain the D3D11.dll module.");

		Fn pFn = (Fn)(GetProcAddress(Module, "D3D11CreateDeviceAndSwapChain"));

		if (!pFn)
			Utils::Error::Error(1, "Cannot obtain the CreateDevice function pointer.");

		DXGI_SWAP_CHAIN_DESC Descriptor = { 0 };
		IDXGISwapChain* pSwap = nullptr;

		SetupDescriptor(&Descriptor);

		auto Result = pFn(0, D3D_DRIVER_TYPE_NULL, 0, 0, 0, 0, D3D11_SDK_VERSION, &Descriptor, &pSwap, 0, 0, 0);

		if (FAILED(Result))
			Utils::Error::Error(1, "Failed to create a dummy swapchain!");

		void** ppVMT = *(void***)(pSwap);
		void* pPresent = ppVMT[8];

		//Throw these away, they're useless now.
		pSwap->Release();

		return pPresent;
	}
}