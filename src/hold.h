#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
namespace diva6k {
// IDA: arcade+78752; update 0x140273120 caps timer-lastAdded at 5 seconds.
struct HoldRaw {
    uint8_t prefix[12];
    int32_t state;
    uint32_t buttons,alternate;
    float timer;
    int32_t score;
    float duration,step;
    int32_t scoreUnit;
    uint32_t flags;
    float lastAdded;
    int32_t lastScore,count;
    uint32_t tail;
};
static_assert(sizeof(HoldRaw)==64);
struct HoldView {uint32_t lanes=0;double remaining=0;};
inline HoldView DecodeHold(const HoldRaw& raw) {
    HoldView view;
    if(raw.state!=1||!std::isfinite(raw.timer)||!std::isfinite(raw.lastAdded)||raw.timer<raw.lastAdded||raw.timer>7200||raw.lastAdded<0)return view;
    constexpr uint32_t buttons[]={0x40,0x80,0x100,0x200};
    constexpr int lanes[]={2,4,3,1};
    for(int i=0;i<4;++i)if(raw.buttons&buttons[i])view.lanes|=1u<<lanes[i];
    view.remaining=(std::clamp)(5.0-static_cast<double>(raw.timer-raw.lastAdded),0.0,5.0);
    if(view.remaining<=0)view.lanes=0;
    return view;
}
}
