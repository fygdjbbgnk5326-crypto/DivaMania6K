#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <vector>
#include <array>
#include "chart.h"
#include "feedback.h"
#include "hold.h"
#include "resolved.h"
using Microsoft::WRL::ComPtr;
using InitFn=void(*)();
using D3DFn=void(*)(IDXGISwapChain*,ID3D11Device*,ID3D11DeviceContext*);
using FrameFn=void(*)(IDXGISwapChain*);
int main(int argc,char**) {
    auto fail=[](const char* msg){fprintf(stderr,"FAIL: %s\n",msg);return 1;};
    HMODULE plugin=LoadLibraryW(L"build/DivaMania6K-test.dll");if(!plugin)return fail("LoadLibrary");
    auto init=reinterpret_cast<InitFn>(GetProcAddress(plugin,"Init"));
    auto setup=reinterpret_cast<D3DFn>(GetProcAddress(plugin,"D3DInit"));
    auto frame=reinterpret_cast<FrameFn>(GetProcAddress(plugin,"OnFrame"));
    if(!init||!setup||!frame)return fail("exports");init();
    auto setPointers=reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(GetProcAddress(plugin,"SetTestGamePointers"));
    if(!setPointers)return fail("test fixture export");
    std::vector<unsigned char> pv(190000),arcade(80000);
    std::vector<diva6k::Group> groups(8);
    for(int i=0;i<6;++i){groups[i].count=1;groups[i].hitNs=1800000000LL+i*100000000;groups[i].targets[0].type=i==0?12:i==5?13:i-1;}
    groups[6].count=1;groups[6].hitNs=2400000000LL;groups[6].targets[0].type=4;
    groups[7].count=1;groups[7].hitNs=1200000000LL;groups[7].targets[0].type=5;
    groups[0].count=2;groups[0].targets[1].type=13;
    groups[2].count=3;groups[2].targets[1].type=2;groups[2].targets[2].type=3;
    groups[4].count=4;for(int i=0;i<4;++i)groups[4].targets[i].type=i;
    uintptr_t pointers[]={reinterpret_cast<uintptr_t>(groups.data()),reinterpret_cast<uintptr_t>(groups.data()+groups.size()),reinterpret_cast<uintptr_t>(groups.data()+groups.size())};
    memcpy(pv.data()+181680,pointers,sizeof(pointers));pv[0]=1;arcade[0]=1;float seconds=1.5f;memcpy(arcade.data()+78480,&seconds,4);
    int64_t songEndNs=8000000000LL;memcpy(pv.data()+0x2d330,&songEndNs,sizeof(songEndNs));
    std::array<diva6k::RuntimeTarget,64> runtime{};runtime[0].group=7;runtime[0].type=5;runtime[0].hitNs=1200000000LL;runtime[0].judgement=0;
    memcpy(arcade.data()+40,runtime.data(),sizeof(runtime));
    setPointers(reinterpret_cast<uintptr_t>(pv.data()),reinterpret_cast<uintptr_t>(arcade.data()));
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"SixLaneSmoke";RegisterClassW(&wc);
    HWND hwnd=CreateWindowW(wc.lpszClassName,L"Hidden smoke test",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,wc.hInstance,nullptr);
    if(!hwnd)return fail("window");
    DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Width=1280;sd.BufferDesc.Height=720;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=1;sd.OutputWindow=hwnd;sd.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> ctx;ComPtr<IDXGISwapChain> swap;
    UINT flags=argc>1?D3D11_CREATE_DEVICE_SINGLETHREADED:0;
    if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&swap,&device,nullptr,&ctx)))return fail("WARP device");
    printf("Device flags: 0x%X\n",flags);
    setup(swap.Get(),device.Get(),ctx.Get());
    diva6k::Metrics feedback{};feedback.combo=42;feedback.maxCombo=42;feedback.all[0]=feedback.correct[0]=42;
    memcpy(pv.data()+0x2d248,&feedback,sizeof(feedback));
    diva6k::HoldRaw activeHold{};activeHold.state=1;activeHold.buttons=0x80;activeHold.timer=4.6f;
    memcpy(arcade.data()+78752,&activeHold,sizeof(activeHold));
    frame(swap.Get()); // Establish a real counter baseline before a hit.
    ++feedback.combo;++feedback.maxCombo;++feedback.all[0];++feedback.correct[0];
    memcpy(pv.data()+0x2d248,&feedback,sizeof(feedback));
    auto setKeys=reinterpret_cast<void(*)(int)>(GetProcAddress(plugin,"SetTestKeyMask"));
    if(!setKeys)return fail("test key fixture export");setKeys((1<<1)|(1<<4));frame(swap.Get());Sleep(65);
    for(int pass=0;pass<2;++pass) {
        UINT w=pass?800:1280,h=pass?600:720;
        if(pass&&FAILED(swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0)))return fail("ResizeBuffers: leaked backbuffer reference");
        ComPtr<ID3D11Texture2D> back,staging;ComPtr<ID3D11RenderTargetView> target;
        if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&back))))return fail("backbuffer");
        if(FAILED(device->CreateRenderTargetView(back.Get(),nullptr,&target)))return fail("RTV");
        float bg[]={.08f,.12f,.18f,1};ctx->ClearRenderTargetView(target.Get(),bg);
        D3D11_VIEWPORT vp{3,5,123,234,0,1};ctx->RSSetViewports(1,&vp);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        frame(swap.Get());
        D3D11_PRIMITIVE_TOPOLOGY topology{};ctx->IAGetPrimitiveTopology(&topology);UINT count=1;D3D11_VIEWPORT actual{};ctx->RSGetViewports(&count,&actual);
        if(topology!=D3D11_PRIMITIVE_TOPOLOGY_LINELIST||actual.Width!=123||actual.TopLeftX!=3)return fail("game pipeline state not restored");
        D3D11_TEXTURE2D_DESC td{};back->GetDesc(&td);td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;td.MiscFlags=0;
        if(FAILED(device->CreateTexture2D(&td,nullptr,&staging)))return fail("staging");ctx->CopyResource(staging.Get(),back.Get());
        D3D11_MAPPED_SUBRESOURCE data{};if(FAILED(ctx->Map(staging.Get(),0,D3D11_MAP_READ,0,&data)))return fail("readback");
        if(!pass){
            // This fixture's HOLD ends at the following same-lane chord. Its
            // white tail cap must remain visible above that later colored note.
            auto cap=static_cast<unsigned char*>(data.pData)+347*data.RowPitch+720*4;
            if(cap[0]<180||cap[1]<180||cap[2]<180)return fail("HOLD tail hidden by later note");
        }
        size_t colored=0;std::vector<unsigned char> pixels(static_cast<size_t>(w)*h*4);
        for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){auto p=static_cast<unsigned char*>(data.pData)+y*data.RowPitch+x*4;auto out=&pixels[(static_cast<size_t>(h-1-y)*w+x)*4];out[0]=p[2];out[1]=p[1];out[2]=p[0];out[3]=255;if(p[0]>120||p[1]>120||p[2]>120)++colored;}
        ctx->Unmap(staging.Get(),0);if(colored<1000)return fail("overlay missing");
        if(!pass){BITMAPFILEHEADER fh{};BITMAPINFOHEADER ih{};fh.bfType=0x4d42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+static_cast<DWORD>(pixels.size());ih.biSize=sizeof(ih);ih.biWidth=w;ih.biHeight=h;ih.biPlanes=1;ih.biBitCount=32;FILE* f=nullptr;fopen_s(&f,"build/preview.bmp","wb");if(!f)return fail("preview write");fwrite(&fh,sizeof(fh),1,f);fwrite(&ih,sizeof(ih),1,f);fwrite(pixels.data(),pixels.size(),1,f);fclose(f);}
        printf("PASS: %ux%u overlay (%zu bright pixels), game state restored\n",w,h,colored);
    }
    DestroyWindow(hwnd);puts("PASS: DLL exports, WARP rendering, readback and resize");return 0;
}
