#include "game_adapter.h"
#include "resolved.h"
#include "hold_plan.h"
#include <array>
#include <cstring>
#include <limits>
namespace diva6k {
namespace {
// Verified in this local game's IDA database; all addresses are module-relative.
constexpr uintptr_t pvRva=0x12c2330,arcadeRva=0x12f0f50;
constexpr uintptr_t vectorOffset=181680,arcadeClockOffset=78480;
struct VectorHeader {uintptr_t begin,end,capacity;};
bool Read(uintptr_t address,void* destination,size_t size) {
    SIZE_T got=0;
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),destination,size,&got)&&got==size;
}
template<class T>bool Read(uintptr_t address,T& value){return Read(address,&value,sizeof(value));}
bool Getter(uintptr_t base,uintptr_t rva,uintptr_t expected) {
    uint8_t code[8]{};if(!Read(base+rva,code,8))return false;
    int32_t displacement=0;memcpy(&displacement,code+3,4);
    return code[0]==0x48&&code[1]==0x8d&&code[2]==0x05&&code[7]==0xc3&&base+rva+7+displacement==expected;
}
uint64_t Hash(const std::vector<Group>& groups) {
    uint64_t hash=1469598103934665603ULL;
    // Hash semantic data only; ignore padding and animation coordinates.
    for(const auto& g:groups){hash^=static_cast<uint64_t>(g.hitNs);hash*=1099511628211ULL;hash^=g.count;
        for(int i=0;i<g.count&&i<4;++i){hash^=static_cast<uint32_t>(g.targets[i].type);hash*=1099511628211ULL;hash^=g.targets[i].chainStart|(g.targets[i].chainEnd<<1)|(g.targets[i].chain<<2);}}
    return hash;
}
}
void GameAdapter::Initialize() {
    uintptr_t base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    pv=base+pvRva;ac=base+arcadeRva;
    state.compatible=Getter(base,0x266720,pv)&&Getter(base,0x27dda0,ac);
    state.event=state.compatible?"Game adapter compatible: PV/arcade getters verified":"Game adapter disabled: executable signatures do not match";
}
void GameAdapter::Clear() {
    state.active=false;state.paused=false;state.seconds=state.duration=0;state.ended=false;timeline.Clear();
    feedback.Clear();state.metricsValid=false;state.combo=state.maxCombo=0;state.judgement=0;state.judgementUntil=0;
    state.hold={};state.resolved.clear();
    if(wasActive){state.notes.clear();state.prefixEnds.clear();state.unknown=0;state.event="Song ended/unloaded; chart cache cleared";}
    wasActive=false;lastBegin=lastEnd=0;lastHash=0;nextChartCheck=0;lastPlanDuration=-1;
}
void GameAdapter::Update() {
    if(!state.compatible)return;
    std::array<uint8_t,4> flags{};uint8_t playing=0;float seconds=0;VectorHeader header{};
    if(!Read(pv,flags)||!Read(ac,playing)||!Read(ac+arcadeClockOffset,seconds)||!Read(pv+vectorOffset,header)) {Clear();return;}
    if(!flags[0]||!playing||!header.begin||header.end<=header.begin){Clear();return;}
    const uintptr_t bytes=header.end-header.begin;
    if(header.end>header.capacity||bytes%sizeof(Group)||bytes>sizeof(Group)*100000||header.capacity-header.begin>sizeof(Group)*200000||!std::isfinite(seconds)||seconds< -60||seconds>7200){Clear();return;}
    bool backwards=wasActive&&seconds<state.seconds-.05;
    bool entering=!wasActive;state.active=true;state.paused=flags[1]!=0||flags[2]!=0;state.seconds=seconds;
    if(backwards)std::fill(state.resolved.begin(),state.resolved.end(),uint8_t{0});
    ULONGLONG now=GetTickCount64();
    bool identityChanged=header.begin!=lastBegin||header.end!=lastEnd;
    // IDA: parser 0x140245C50 stores int64 nanoseconds at PV+0x2D330;
    // main update 0x140241DF0 branches to end/result handling at PV+0x2D17D.
    int64_t endNs=0;uint8_t endSignal=0;
    Read(pv+0x2d330,endNs);Read(pv+0x2d17d,endSignal);
    timeline.Update(seconds,endNs,endSignal!=0,state.paused,entering||identityChanged);
    state.seconds=timeline.seconds;state.duration=timeline.duration;state.ended=timeline.ended;
    HoldRaw hold{};state.hold=Read(ac+78752,hold)?DecodeHold(hold):HoldView{};
    Metrics metrics{};
    state.metricsValid=Read(pv+0x2d248,metrics)&&feedback.Update(metrics,state.seconds,entering||identityChanged);
    if(state.metricsValid){state.combo=metrics.combo;state.maxCombo=metrics.maxCombo;state.judgement=feedback.mask;state.judgementUntil=feedback.until;}
    else{feedback.Clear();state.combo=state.maxCombo=0;state.judgement=0;state.judgementUntil=0;}
    if(entering||identityChanged||now>=nextChartCheck) {
        nextChartCheck=now+1000;
        std::vector<Group> groups(bytes/sizeof(Group));
        if(!Read(header.begin,groups.data(),bytes)){Clear();return;}
        VectorHeader after{};
        if(!Read(pv+vectorOffset,after)||memcmp(&header,&after,sizeof(header))){Clear();return;}
        uint64_t hash=Hash(groups);
        if(entering||identityChanged||hash!=lastHash) {
            if(!entering&&!identityChanged){feedback.Clear();state.judgement=0;}
            if(!entering&&!identityChanged){timeline.Update(seconds,endNs,endSignal!=0,state.paused,true);state.seconds=timeline.seconds;state.duration=timeline.duration;state.ended=timeline.ended;}
            size_t unknown=0;
            if(!Convert(groups,state.notes,unknown)){Clear();return;}
            state.resolved.assign(state.notes.size(),0);
            PlanHolds(state.notes,state.duration);lastPlanDuration=state.duration;
            BuildPrefixEnds(state.notes,state.prefixEnds);state.unknown=unknown;
            char message[200];sprintf_s(message,"Real chart loaded: groups=%zu notes=%zu unknown=%zu clock=%.3f",groups.size(),state.notes.size(),unknown,state.seconds);state.event=message;
            lastHash=hash;lastBegin=header.begin;lastEnd=header.end;
        }
    }
    if(lastPlanDuration!=state.duration){PlanHolds(state.notes,state.duration);BuildPrefixEnds(state.notes,state.prefixEnds);lastPlanDuration=state.duration;}
    std::array<RuntimeTarget,64> targets{};
    if(Read(ac+40,targets))
        for(const auto& target:targets)ResolveTarget(target,state.seconds,state.notes,state.resolved);
    if(state.ended){state.hold={};state.judgement=0;}
    wasActive=true;
}
}
