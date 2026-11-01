#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <vector>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include "game_adapter.h"
#include "popup.h"
#include "render_clock.h"
#include "hold_plan.h"
using Microsoft::WRL::ComPtr;
namespace {
struct Vertex { float x,y,r,g,b,a; };
struct Color { float r,g,b,a; };
struct HoldTailMarker {float x,tail,head;int lane;double remaining;};
std::filesystem::path root;
std::mutex guard;
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> renderContext;
ComPtr<ID3D11DeviceContext1> context1;
ComPtr<ID3DDeviceContextState> overlayState;
ComPtr<ID3D11VertexShader> vs;
ComPtr<ID3D11PixelShader> ps;
ComPtr<ID3D11InputLayout> layout;
ComPtr<ID3D11Buffer> buffer;
ComPtr<ID3D11BlendState> blend;
ComPtr<ID3D11RasterizerState> raster;
ComPtr<ID3D11DepthStencilState> depth;
std::vector<Vertex> vertices;
std::vector<HoldTailMarker> holdTails;
constexpr size_t capacity=65536;
bool ready=false, visible=true, loggedFrame=false;
bool oldToggle=false, oldPause=false;
ULONGLONG nextAttempt=0;
diva6k::GameAdapter game;
std::string lastEvent;
double fallSeconds=1.2,offsetSeconds=0;
diva6k::RenderClock renderClock;
double renderSeconds=0;
bool smoothAnimation=true;
bool hideInMenus=true;
diva6k::JudgementPopup judgementPopup;
UINT judgementDurationMs=500;
float feedbackPosition=.45f;
float positionY=-60.f;
bool clickEffects=true;
bool multiPressOutline=true;
float clickBeamHeight=90.f;
float clickFadeMs=360.f;
float laneLineOpacity=.30f;
float laneLineWidth=1.f;
float laneWidth=90.f;
float backgroundOpacity=.95f;
float coloredOpacity=1.f;
float arrowBackgroundOpacity=.20f;
constexpr char laneKeys[]={'S','D','F','J','K','L'};
constexpr int alternateKeys[]={VK_LCONTROL,VK_LEFT,VK_UP,VK_DOWN,VK_RIGHT,VK_RCONTROL};
std::array<bool,6> keyDown{};
std::array<ULONGLONG,6> keyFlashes{};
#ifdef DIVAMANIA_TEST
int testKeyMask=-1;
#endif
float width=1920,height=1080;
const Color white{.85f,.95f,1,1};
const std::array<Color,6> colors={Color{1,.65f,.08f,1},Color{1,.35f,.8f,1},Color{.12f,.95f,.65f,1},Color{.12f,.7f,1,1},Color{1,.2f,.45f,1},Color{1,.65f,.08f,1}};
void Log(const char* message) {
    FILE* f=nullptr;
    if (_wfopen_s(&f,(root/L"DivaMania6K.log").c_str(),L"a")==0 && f) {
        fprintf(f,"[%llu] %s\n",GetTickCount64(),message); fclose(f);
    }
}
bool Check(HRESULT hr,const char* step) {
    if(SUCCEEDED(hr)) return true;
    char line[200]; sprintf_s(line,"%s failed: 0x%08lX",step,static_cast<unsigned long>(hr)); Log(line); return false;
}
double PerformanceSeconds() {
    static const double frequency=[](){LARGE_INTEGER f{};QueryPerformanceFrequency(&f);return static_cast<double>(f.QuadPart);}();
    LARGE_INTEGER counter{};QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart)/frequency;
}
void LoadSettings() {
    const auto file=(root/L"Player.ini").wstring();
    fallSeconds=std::clamp(GetPrivateProfileIntW(L"View",L"FallTimeMs",1200,file.c_str()),200u,5000u)/1000.;
    wchar_t offset[32]{};GetPrivateProfileStringW(L"View",L"OffsetMs",L"0",offset,32,file.c_str());
    offsetSeconds=std::clamp(_wtoi(offset),-1000,1000)/1000.;
    hideInMenus=GetPrivateProfileIntW(L"View",L"HideInMenus",1,file.c_str())!=0;
    judgementDurationMs=std::clamp(GetPrivateProfileIntW(L"View",L"JudgementDurationMs",500,file.c_str()),100u,2000u);
    feedbackPosition=std::clamp(GetPrivateProfileIntW(L"View",L"FeedbackPositionPercent",45,file.c_str()),10u,85u)/100.f;
    wchar_t position[32]{};GetPrivateProfileStringW(L"View",L"PositionY",L"-60",position,32,file.c_str());
    positionY=static_cast<float>(std::clamp(_wtoi(position),-70,150));
    clickEffects=GetPrivateProfileIntW(L"View",L"ClickEffects",1,file.c_str())!=0;
    multiPressOutline=GetPrivateProfileIntW(L"View",L"MultiPressOutline",1,file.c_str())!=0;
    clickBeamHeight=static_cast<float>(std::clamp(GetPrivateProfileIntW(L"View",L"ClickBeamHeight",90,file.c_str()),20u,240u));
    clickFadeMs=static_cast<float>(std::clamp(GetPrivateProfileIntW(L"View",L"ClickFadeMs",360,file.c_str()),100u,1000u));
    laneLineOpacity=std::min(GetPrivateProfileIntW(L"View",L"LaneLineOpacity",30,file.c_str()),100u)/100.f;
    laneLineWidth=static_cast<float>(std::clamp(GetPrivateProfileIntW(L"View",L"LaneLineWidth",1,file.c_str()),1u,3u));
    laneWidth=static_cast<float>(std::clamp(GetPrivateProfileIntW(L"View",L"LaneWidth",90,file.c_str()),48u,140u));
    backgroundOpacity=std::min(GetPrivateProfileIntW(L"View",L"BackgroundOpacity",95,file.c_str()),100u)/100.f;
    coloredOpacity=std::min(GetPrivateProfileIntW(L"View",L"ColoredOpacity",100,file.c_str()),100u)/100.f;
    arrowBackgroundOpacity=std::min(GetPrivateProfileIntW(L"View",L"ArrowBackgroundOpacity",20,file.c_str()),100u)/100.f;
    smoothAnimation=GetPrivateProfileIntW(L"View",L"SmoothAnimation",1,file.c_str())!=0;
    renderClock.Clear();
}
void Rect(float x,float y,float w,float h,Color c) {
    if(w<=0 || h<=0) return;
    auto V=[&](float px,float py){return Vertex{px/width*2-1,1-py/height*2,c.r,c.g,c.b,c.a};};
    Vertex a=V(x,y),b=V(x+w,y),d=V(x,y+h),e=V(x+w,y+h);
    vertices.insert(vertices.end(),{a,b,d,b,e,d});
}
// Tiny built-in glyphs keep the test DLL independent of fonts and textures.
std::array<unsigned char,7> Glyph(char c) {
    switch(c) {
    case '6':return {14,16,16,30,17,17,14}; case 'K':return {17,18,20,24,20,18,17};
    case 'T':return {31,4,4,4,4,4,4}; case 'E':return {31,16,16,30,16,16,31};
    case 'S':return {15,16,16,14,1,1,30}; case 'D':return {30,17,17,17,17,17,30};
    case 'F':return {31,16,16,30,16,16,16}; case 'J':return {7,2,2,2,18,18,12};
    case 'L':return {16,16,16,16,16,16,31}; case 'H':return {17,17,17,31,17,17,17};
    case 'I':return {14,4,4,4,4,4,14}; case 'O':return {14,17,17,17,17,17,14};
    case 'W':return {17,17,17,21,21,21,10}; case 'P':return {30,17,17,30,16,16,16};
    case 'A':return {14,17,17,31,17,17,17}; case 'U':return {17,17,17,17,17,17,14};
    case '8':return {14,17,17,14,17,17,14}; case '9':return {14,17,17,15,1,1,14};
    case '<':return {2,4,8,16,8,4,2}; case '>':return {8,4,2,1,2,4,8};
    case 'V':return {17,17,17,17,17,10,4};case 'N':return {17,25,25,21,19,19,17};
    case 'G':return {14,17,16,23,17,17,14};case 'R':return {30,17,17,30,20,18,17};
    case 'C':return {14,17,16,16,16,17,14};case 'M':return {17,27,21,21,17,17,17};
    case 'B':return {30,17,17,30,17,17,30};case 'Y':return {17,17,10,4,4,4,4};
    case '0':return {14,17,19,21,25,17,14};case '1':return {4,12,4,4,4,4,14};
    case '2':return {14,17,1,2,4,8,31};case '3':return {30,1,1,14,1,1,30};
    case '4':return {2,6,10,18,31,2,2};case '5':return {31,16,16,30,1,1,30};
    case '7':return {31,1,2,4,8,8,8};case '.':return {0,0,0,0,0,12,12};
    case '-':return {0,0,0,31,0,0,0};case ':':return {0,12,12,0,12,12,0};
    case '/':return {1,2,2,4,8,8,16}; default:return {};
    }
}
void Text(float x,float y,const char* s,float size,Color color) {
    for(;*s;++s,x+=size*6) {auto rows=Glyph(*s); for(int r=0;r<7;++r)for(int c=0;c<5;++c)if(rows[r]&(1<<(4-c)))Rect(x+c*size,y+r*size,size,size,color);}
}
void ArrowShape(float cx,float cy,float size,int lane,Color color) {
    constexpr float pi=3.14159265f;
    const float angles[]={-pi*.75f,-pi*.5f,0,pi,pi*.5f,pi*.75f};
    const float c=std::cos(angles[lane]),s=std::sin(angles[lane]);
    auto V=[&](float x,float y){float px=cx+size*(c*x-s*y),py=cy+size*(s*x+c*y);return Vertex{px/width*2-1,1-py/height*2,color.r,color.g,color.b,color.a};};
    vertices.insert(vertices.end(),{V(-1,0),V(0,-1),V(1,0),V(-.36f,0),V(.36f,0),V(-.36f,.85f),V(.36f,0),V(.36f,.85f),V(-.36f,.85f)});
}
void Icon(float x,float y,float size,int lane) {
    ArrowShape(x,y,size+2,lane,{.015f,.025f,.04f,1});
    ArrowShape(x,y,size,lane,colors[lane]);
}
void UpdateKeys(bool focused,bool playing) {
#ifdef DIVAMANIA_TEST
    if(testKeyMask>=0)focused=true;
#endif
    ULONGLONG now=GetTickCount64();
    for(int i=0;i<6;++i) {
        bool down=focused&&playing&&((GetAsyncKeyState(laneKeys[i])|GetAsyncKeyState(alternateKeys[i]))&0x8000);
#ifdef DIVAMANIA_TEST
        if(testKeyMask>=0)down=playing&&(testKeyMask&(1<<i));
#endif
        if(!playing||!focused)keyFlashes[i]=0;
        if(down&&!keyDown[i])keyFlashes[i]=now;
        keyDown[i]=down;
    }
}
void ClickGlow(float left,float lane,float judge,float scale) {
    if(!clickEffects)return;
    const ULONGLONG now=GetTickCount64();
    for(int i=0;i<6;++i) {
        float age=keyFlashes[i]?static_cast<float>(now-keyFlashes[i])/clickFadeMs:1.f;
        float burst=std::max(0.f,1-age),power=std::max(burst,keyDown[i]?.28f:0.f);
        if(power<=0)continue;
        float x=left+i*lane,cx=x+lane/2,h=clickBeamHeight*(.78f+.22f*burst)*scale;
        // Layered alpha gradients approximate bloom without textures or postprocessing.
        auto Beam=[&](float bx,float bw,Color color){
            auto V=[&](float px,float py,float alpha){return Vertex{px/width*2-1,1-py/height*2,color.r,color.g,color.b,alpha};};
            auto a=V(bx,judge-h,0),b=V(bx+bw,judge-h,0),c=V(bx,judge,color.a*power),d=V(bx+bw,judge,color.a*power);
            vertices.insert(vertices.end(),{a,b,c,b,d,c});
        };
        Beam(x+2*scale,lane-4*scale,{.02f,.48f,1,.45f});
        Beam(x+lane*.19f,lane*.62f,{.08f,.72f,1,.6f});
        Beam(cx-7*scale,14*scale,{.6f,.95f,1,.75f});
        Rect(x+scale,judge-8*scale,lane-2*scale,19*scale,{.03f,.6f,1,.38f*power});
        Rect(x+3*scale,judge-3*scale,lane-6*scale,7*scale,{.55f,.95f,1,.9f*power});
        Rect(cx-12*scale,judge-2*scale,24*scale,4*scale,{1,1,1,power});
        const float particleAge=keyFlashes[i]?static_cast<float>(now-keyFlashes[i])/260.f:1.f;
        const float particleBurst=std::max(0.f,1-particleAge);
        if(particleBurst>0)for(int p=0;p<9;++p) {
            float a=3.14159265f*(.08f+.84f*p/8.f),distance=(12+74*particleAge)*scale;
            float px=cx+std::cos(a)*distance*.42f,py=judge-std::sin(a)*distance;
            float size=(p%3==0?3:2)*scale;
            Rect(px-size*2,py-size*2,size*4,size*4,{.02f,.6f,1,.17f*particleBurst});
            Rect(px-size/2,py-2*size,size,4*size,{.5f,.95f,1,particleBurst});
            Rect(px-2*size,py-size/2,4*size,size,{.8f,1,1,particleBurst});
        }
    }
}
void DrawHoldTail(const HoldTailMarker& marker,float lane,float top,float judge,float scale) {
    if(marker.head<top||marker.tail>judge)return;
    const float w=lane-18*scale,x=marker.x+4*scale;
    if(marker.tail<top) {
        if(marker.head<top+56*scale)return;
        // An offscreen tail is not a release cap at the top of the window.
        // A countdown explicitly shows that the actual endpoint is still ahead.
        char label[32];sprintf_s(label,"%.1fS",std::max(0.,marker.remaining));
        const float font=std::min(1.7f*scale,(w-4*scale)/(static_cast<float>(strlen(label))*6));
        const float y=top+37*scale;
        Rect(x,y-3*scale,w,19*scale,{.015f,.025f,.04f,.94f});
        Text(x+(w-(static_cast<float>(strlen(label))*6-1)*font)/2,y,label,font,white);
        return;
    }
    const float h=16*scale,y=std::clamp(marker.tail-h/2,top,judge-h);
    Rect(x-2*scale,y-2*scale,w+4*scale,h+4*scale,{.01f,.02f,.03f,1});
    Rect(x,y,w,h,white);
    Color inset=colors[marker.lane];inset.r*=.22f;inset.g*=.22f;inset.b*=.22f;
    Rect(x+2*scale,y+2*scale,w-4*scale,h-4*scale,inset);
    const float font=std::min(1.5f*scale,(w-8*scale)/17);
    Text(x+(w-17*font)/2,y+(h-7*font)/2,"END",font,white);
}
void Geometry(bool focused) {
    (void)focused;
    const float scale=std::min(width/1920.f,height/1080.f);
    const float lane=laneWidth*scale, total=lane*6, left=(width-total)/2, top=(150+positionY)*scale;
    const float judge=height-(180-positionY)*scale, bottom=height-(70-positionY)*scale;
    // One background layer keeps the configured opacity exact inside the lanes.
    Rect(left-10*scale,top-72*scale,total+20*scale,bottom-top+82*scale,{.025f,.035f,.06f,backgroundOpacity});
    const auto& live=game.State();
    Text(left+10*scale,top-57*scale,live.active?(live.ended?"6K ENDED":live.paused?"6K LIVE PAUSED":"6K LIVE"):"6K WAIT SONG",3*scale,white);
    const int elapsed=static_cast<int>(std::max(0.,live.seconds));
    const int duration=static_cast<int>(std::max(0.,live.duration));
    char status[100];
    if(live.duration>0)sprintf_s(status,"TIME %d:%02d / %d:%02d  NOTES %zu",elapsed/60,elapsed%60,duration/60,duration%60,live.notes.size());
    else sprintf_s(status,"TIME %d:%02d / --:--  NOTES %zu",elapsed/60,elapsed%60,live.notes.size());
    const float statusFont=std::min(1.6f*scale,(total-20*scale)/(static_cast<float>(strlen(status))*6));
    Text(left+10*scale,top-29*scale,status,statusFont,white);
    const float progress=live.duration>0?static_cast<float>(std::clamp(live.seconds/live.duration,0.,1.)):0;
    Rect(left,top-10*scale,total,6*scale,{.65f,.8f,.9f,.22f});
    Rect(left,top-10*scale,total*progress,6*scale,live.ended?Color{.12f,.95f,.65f,1}:Color{.08f,.8f,1,1});
    Text(left+10*scale,bottom-17*scale,"F8 HIDE/SHOW  F9 RELOAD",1.6f*scale,white);
    for(int i=0;i<6;++i) {
        const float x=left+i*lane;
        Color base=colors[i]; base.a=arrowBackgroundOpacity;
        Rect(x+3*scale,judge+8*scale,lane-6*scale,76*scale,base);
        Color edge=colors[i];edge.a=coloredOpacity;
        Rect(x+3*scale,judge+8*scale,lane-6*scale,2*scale,edge);
        Icon(x+lane/2,judge+29*scale,13*scale,i);
        Rect(x+lane/2-16*scale,judge+51*scale,32*scale,25*scale,{.02f,.03f,.05f,.8f});
        char label[]={laneKeys[i],0};Text(x+lane/2-6.25f*scale,judge+55*scale,label,2.5f*scale,white);
        Icon(x+lane/2,top+22*scale,9*scale,i);
    }
    // Draw shared borders once, after all lane backgrounds. Pixel alignment keeps
    // thin separators consistent at smaller resolutions; stop above the footer.
    if(laneLineOpacity>0) {
        const float thickness=std::max(1.f,std::round(laneLineWidth*scale));
        const float lineTop=std::round(top),lineBottom=std::round(judge+84*scale);
        for(int i=0;i<=6;++i) {
            const float x=std::round(left+i*lane-thickness/2);
            const float alpha=std::min(1.f,laneLineOpacity*((i==0||i==6)?1.4f:1.f));
            Rect(x,lineTop,thickness,lineBottom-lineTop,{.82f,.89f,.95f,alpha});
        }
    }
    const double visualTime=renderSeconds-offsetSeconds;
    holdTails.clear();
    size_t begin=static_cast<size_t>(std::lower_bound(live.prefixEnds.begin(),live.prefixEnds.end(),visualTime-.10)-live.prefixEnds.begin());
    auto Y=[&](double t){return judge-static_cast<float>((t-visualTime)/fallSeconds)*(judge-top);};
    for(size_t index=begin;!live.ended&&index<live.notes.size();++index) {
        const auto& note=live.notes[index];
        if(note.time>visualTime+fallSeconds)break;
        const bool resolved=index<live.resolved.size()&&live.resolved[index];
        if(resolved&&!note.hold)continue;
        if(note.end<visualTime-.10)continue;
        const int i=note.lane;float x=left+i*lane+5*scale,y=Y(note.time);
        if(note.hold) {
            if(!diva6k::ShowHold(note,resolved,live.hold.lanes,live.seconds,visualTime))continue;
            const float tail=Y(note.end);
            const float head=resolved?judge:std::min(judge,y);
            const float bodyTop=std::max(top,tail),bodyBottom=std::min(judge,head-13*scale);
            Color body=colors[i];body.a=coloredOpacity;
            Rect(x+8*scale,bodyTop,lane-26*scale,bodyBottom-bodyTop,body);
            holdTails.push_back({x,tail,head,i,note.end-visualTime});
            if(resolved){
                Rect(x,judge-13*scale,lane-10*scale,26*scale,colors[i]);
                Text(x+3*scale,judge-5*scale,"H",1.5f*scale,white);
                continue;
            }
        }
        if(note.chain&&note.end>note.time) {
            float bodyTop=std::max(top,Y(note.end)),bodyBottom=std::min(judge,y)-13*scale;
            Color c=colors[i];c.a=coloredOpacity;Rect(x+12*scale,bodyTop,lane-34*scale,bodyBottom-bodyTop,c);
            y=std::min(y,judge); // Keep the head at the receptor while a chain is held.
        }
        if(y>=top&&y<=judge+14*scale) {
            // Falling heads are opaque color bars without direction glyphs.
            Rect(x,y-13*scale,lane-10*scale,26*scale,colors[i]);
            if(note.hold)Text(x+3*scale,y-5*scale,"H",1.5f*scale,white);
            if(note.doubled)Text(x+lane-23*scale,y-5*scale,"2",1.5f*scale,white);
            if(note.multi&&multiPressOutline) {
                const float w=lane-10*scale,h=32*scale,t=2.5f*scale,ny=y-16*scale;
                const Color orange{1,.57f,.06f,1};
                const Color glow{1,.45f,.02f,.16f};
                Rect(x-2*scale,ny-2*scale,w+4*scale,2*scale,glow);
                Rect(x-2*scale,ny+h,w+4*scale,2*scale,glow);
                Rect(x-2*scale,ny,2*scale,h,glow);Rect(x+w,ny,2*scale,h,glow);
                Rect(x,ny,w,t,orange);Rect(x,ny+h-t,w,t,orange);
                Rect(x,ny,t,h,orange);Rect(x+w-t,ny,t,h,orange);
            }
        }
        if(vertices.size()>capacity-1000)break;
    }
    // Tail caps must be above later notes too, especially at a same-key re-press.
    for(const auto& marker:holdTails)DrawHoldTail(marker,lane,top,judge,scale);
    Rect(left,judge,total,3*scale,white);
    ClickGlow(left,lane,judge,scale);
    // Draw feedback last so it remains inside the grid, above the lane geometry.
    if(live.metricsValid) {
        const float cx=left+total/2, feedbackY=top+(judge-top)*feedbackPosition-62*scale;
        auto Centered=[&](const char* label,float y,float font,Color color){
            const float x=cx-(static_cast<float>(strlen(label))*6-1)*font/2;
            Text(x+2*scale,y+2*scale,label,font,{0,0,0,color.a*.9f});Text(x,y,label,font,color);
        };
        char combo[32];sprintf_s(combo,"%d",live.combo);
        Centered("COMBO",feedbackY+42*scale,1.8f*scale,{.6f,.72f,.85f,.85f});
        Centered(combo,feedbackY+62*scale,(live.combo>=100000?4.f:5.f)*scale,live.combo?white:Color{.45f,.5f,.6f,1});
        float alpha=judgementPopup.Alpha(GetTickCount64());
        if(alpha>0) {
            const char* names[]={"COOL","FINE","SAFE","SAD","WORST","WRONG"};
            const Color tones[]={Color{1,.85f,.25f,1},Color{.25f,.9f,1,1},Color{.6f,1,.4f,1},Color{1,.6f,.25f,1},Color{1,.25f,.3f,1},Color{1,.25f,.65f,1}};
            std::string label;Color tone=white;
            for(int i=0;i<6;++i)if(judgementPopup.mask&(1u<<i)){if(!label.empty())label+='/';label+=names[i];tone=tones[i];}
            tone.a=alpha;
            float font=std::min(3.4f*scale,(total-32*scale)/(static_cast<float>(label.size())*6));
            Centered(label.c_str(),feedbackY,font,tone);
        }
    }
}
bool Setup(ID3D11Device* d,ID3D11DeviceContext* ctx) {
    ready=false; device=d;
    renderContext.Reset();vs.Reset();ps.Reset();layout.Reset();buffer.Reset();blend.Reset();raster.Reset();depth.Reset();
    // DIVA may create a SINGLETHREADED device, which forbids deferred contexts.
    // Use the immediate context with an isolated D3D11.1 state instead.
    ComPtr<ID3D11Device1> device1;
    context1.Reset();overlayState.Reset();
    if(!Check(d->QueryInterface(IID_PPV_ARGS(&device1)),"Query Device1"))return false;
    if(!Check(ctx->QueryInterface(IID_PPV_ARGS(&context1)),"Query Context1"))return false;
    D3D_FEATURE_LEVEL level=d->GetFeatureLevel();
    UINT flags=(d->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)?D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
    if(!Check(device1->CreateDeviceContextState(flags,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),nullptr,&overlayState),"Create context state"))return false;
    renderContext=ctx;
    char deviceMessage[120];sprintf_s(deviceMessage,"Immediate renderer: device flags=0x%X",d->GetCreationFlags());Log(deviceMessage);
    const char* shader="struct V {float2 p:POSITION;float4 c:COLOR;};struct O {float4 p:SV_POSITION;float4 c:COLOR;};O VS(V v){O o;o.p=float4(v.p,0,1);o.c=v.c;return o;}float4 PS(O v):SV_TARGET{return v.c;}";
    ComPtr<ID3DBlob> vb,pb,errors;
    if(!Check(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"VS","vs_4_0",0,0,&vb,&errors),"Compile VS"))return false;
    if(!Check(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"PS","ps_4_0",0,0,&pb,&errors),"Compile PS"))return false;
    if(!Check(d->CreateVertexShader(vb->GetBufferPointer(),vb->GetBufferSize(),nullptr,&vs),"Create VS"))return false;
    if(!Check(d->CreatePixelShader(pb->GetBufferPointer(),pb->GetBufferSize(),nullptr,&ps),"Create PS"))return false;
    D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(!Check(d->CreateInputLayout(elements,2,vb->GetBufferPointer(),vb->GetBufferSize(),&layout),"Create layout"))return false;
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=static_cast<UINT>(capacity*sizeof(Vertex));bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(!Check(d->CreateBuffer(&bd,nullptr,&buffer),"Create buffer"))return false;
    D3D11_BLEND_DESC bl{};auto& rt=bl.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(!Check(d->CreateBlendState(&bl,&blend),"Create blend"))return false;
    D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
    if(!Check(d->CreateRasterizerState(&rs,&raster),"Create raster"))return false;
    D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=FALSE;ds.DepthFunc=D3D11_COMPARISON_ALWAYS;
    if(!Check(d->CreateDepthStencilState(&ds,&depth),"Create depth"))return false;
    vertices.reserve(capacity);ready=true;Log("D3D ready");return true;
}
void Frame(IDXGISwapChain* swap) {
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(swap->GetDesc(&desc)))return;
    bool focused=GetForegroundWindow()==desc.OutputWindow;
    bool toggle=focused && (GetAsyncKeyState(VK_F8)&0x8000), reload=focused && (GetAsyncKeyState(VK_F9)&0x8000);
    if(toggle&&!oldToggle)visible=!visible;if(reload&&!oldPause)LoadSettings();oldToggle=toggle;oldPause=reload;
    game.Update();
    const auto& live=game.State();
    if(live.event!=lastEvent)renderClock.Clear();
    renderSeconds=renderClock.Update(live.seconds,PerformanceSeconds(),smoothAnimation&&live.active&&!live.paused&&!live.ended);
    if(live.duration>0)renderSeconds=std::min(renderSeconds,live.duration);
    UpdateKeys(focused,live.active&&!live.paused&&!live.ended);
    judgementPopup.Update(live.judgement,live.judgementUntil,live.seconds,live.paused,live.active&&live.metricsValid&&!live.ended,GetTickCount64(),judgementDurationMs);
    if(game.State().event!=lastEvent){lastEvent=game.State().event;if(!lastEvent.empty())Log(lastEvent.c_str());}
    if(!visible||(!game.State().active&&hideInMenus))return;
    ComPtr<ID3D11Texture2D> back;ComPtr<ID3D11RenderTargetView> target;
    if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&back))))return;
    D3D11_TEXTURE2D_DESC td{};back->GetDesc(&td);width=static_cast<float>(td.Width);height=static_cast<float>(td.Height);if(!td.Width||!td.Height)return;
    if(FAILED(device->CreateRenderTargetView(back.Get(),nullptr,&target)))return;
    vertices.clear();Geometry(focused);if(vertices.size()>capacity)return;
    D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(renderContext->Map(buffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(Vertex));renderContext->Unmap(buffer.Get(),0);
    struct StateScope {
        ID3D11DeviceContext1* ctx;
        ComPtr<ID3DDeviceContextState> previous;
        StateScope(ID3D11DeviceContext1* c,ID3DDeviceContextState* state):ctx(c){ctx->SwapDeviceContextState(state,&previous);}
        ~StateScope(){ctx->ClearState();ctx->SwapDeviceContextState(previous.Get(),nullptr);}
    } stateScope(context1.Get(),overlayState.Get());
    D3D11_VIEWPORT viewport{0,0,width,height,0,1};renderContext->RSSetViewports(1,&viewport);renderContext->RSSetState(raster.Get());
    ID3D11RenderTargetView* targets[]={target.Get()};renderContext->OMSetRenderTargets(1,targets,nullptr);renderContext->OMSetDepthStencilState(depth.Get(),0);renderContext->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
    UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* bufs[]={buffer.Get()};renderContext->IASetVertexBuffers(0,1,bufs,&stride,&offset);renderContext->IASetInputLayout(layout.Get());renderContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    renderContext->VSSetShader(vs.Get(),nullptr,0);renderContext->PSSetShader(ps.Get(),nullptr,0);renderContext->GSSetShader(nullptr,nullptr,0);renderContext->HSSetShader(nullptr,nullptr,0);renderContext->DSSetShader(nullptr,nullptr,0);
    renderContext->Draw(static_cast<UINT>(vertices.size()),0);
    if(!loggedFrame){Log("First overlay frame submitted (immediate context)");loggedFrame=true;}
}
}
extern "C" __declspec(dllexport) void Init() {
    HMODULE self=nullptr;wchar_t path[32768]{};
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&Init),&self)&&GetModuleFileNameW(self,path,32768))root=std::filesystem::path(path).parent_path();
    LoadSettings();game.Initialize();
    Log("Init v1.0: visible HOLD end caps and offscreen tail countdowns; display only");
    Log(game.State().event.c_str());
}
#ifdef DIVAMANIA_TEST
extern "C" __declspec(dllexport) void SetTestGamePointers(uintptr_t pv,uintptr_t arcade) {
    std::lock_guard<std::mutex> lock(guard);game.TestPointers(pv,arcade);
}
extern "C" __declspec(dllexport) void SetTestKeyMask(int mask) {
    std::lock_guard<std::mutex> lock(guard);testKeyMask=mask;
}
#endif
extern "C" __declspec(dllexport) void D3DInit(IDXGISwapChain*,ID3D11Device* d,ID3D11DeviceContext* ctx) {
    std::lock_guard<std::mutex> lock(guard);try{Setup(d,ctx);}catch(...){ready=false;Log("D3DInit exception");}
}
extern "C" __declspec(dllexport) void OnFrame(IDXGISwapChain* swap) {
    std::lock_guard<std::mutex> lock(guard);if(!swap)return;
    try {
        if(!ready) {ULONGLONG now=GetTickCount64();if(now<nextAttempt)return;nextAttempt=now+5000;ComPtr<ID3D11Device> d;ComPtr<ID3D11DeviceContext> ctx;if(FAILED(swap->GetDevice(IID_PPV_ARGS(&d))))return;d->GetImmediateContext(&ctx);if(!Setup(d.Get(),ctx.Get()))return;}
        Frame(swap);
    } catch(...){ready=false;Log("OnFrame exception; rendering disabled until reinitialization");}
}
