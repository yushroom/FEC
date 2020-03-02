#ifndef RENDER_D3D12_HPP
#define RENDER_D3D12_HPP

#include <d3d12.h>

constexpr int NUM_FRAMES_IN_FLIGHT = 3;

struct FrameContext {
    ID3D12CommandAllocator* CommandAllocator;
    UINT64 FenceValue;
};

void FrameBegin();
void FrameEnd();

ID3D12Device* GetDevice();
ID3D12GraphicsCommandList* GetCurrentGraphicsCommandList();
ID3D12DescriptorHeap* GetRTVDescriptorHeap();
ID3D12DescriptorHeap* GetSRVDescriptorHeap();

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext* WaitForNextFrameResources();
void ResizeSwapChain(HWND hWnd, int width, int height);

#endif  // !RENDER_D3D12_HPP
