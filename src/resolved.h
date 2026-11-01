#pragma once
#include "chart.h"
namespace diva6k {
// Verified spawn 0x14026E8E0, judge 0x14026BF60 and erase 0x150C6B2E0.
struct RuntimeTarget {
    uint8_t prefix[20]{};
    int32_t type=0;
    uint8_t beforeTime[48]{};
    int64_t hitNs=0;
    uint8_t beforeJudge[1060]{};
    int32_t judgement=21;
    uint8_t beforeGroup[72]{};
    int32_t group=0;
    uint8_t tail[4]{};
};
static_assert(sizeof(RuntimeTarget)==1224);
static_assert(offsetof(RuntimeTarget,hitNs)==72);
static_assert(offsetof(RuntimeTarget,judgement)==1140);
static_assert(offsetof(RuntimeTarget,group)==1216);
inline void ResolveTarget(const RuntimeTarget& target,double seconds,
    const std::vector<Note>& notes,std::vector<uint8_t>& resolved) {
    if(target.judgement<0||target.judgement>=21||target.group<0||
        target.hitNs<0||target.hitNs>7200000000000LL||resolved.size()!=notes.size())return;
    const double time=target.hitNs*1e-9;
    if(std::abs(time-seconds)>.5)return;
    const int lane=Lane(target.type);
    if(lane<0)return;
    auto it=std::lower_bound(notes.begin(),notes.end(),time-.000001,
        [](const Note& n,double t){return n.time<t;});
    for(;it!=notes.end()&&it->time<=time+.000001;++it)
        if(it->group==target.group&&it->lane==lane&&!it->chain)
            resolved[static_cast<size_t>(it-notes.begin())]=1;
}
}
