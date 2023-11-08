#pragma once

#include "DXAPI/stdafx.h"

using Microsoft::WRL::ComPtr;

namespace Graphics {
	extern ComPtr<ID3D12Device5> g_device;
	extern ComPtr<ID3D12CommandAllocator> g_commandAllocator;
	extern ComPtr<ID3D12CommandQueue> g_commandQueue;
	extern ComPtr<ID3D12GraphicsCommandList4> g_commandList;
}

