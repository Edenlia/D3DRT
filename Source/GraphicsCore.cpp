#include "GraphicsCore.h"

namespace Graphics 
{
	ComPtr<ID3D12Device5> g_device = nullptr;
	ComPtr<ID3D12CommandAllocator> g_commandAllocator = nullptr;
	ComPtr<ID3D12CommandQueue> g_commandQueue = nullptr;
	ComPtr<ID3D12GraphicsCommandList4> g_commandList = nullptr;
}