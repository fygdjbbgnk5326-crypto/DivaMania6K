#pragma once
#include "chart.h"
#include <array>
namespace diva6k {
// DIVA supplies HOLD starts, not release notes. Plan a continuous six-key route:
// add distinct holds, stop at MAX, a required re-press, or the song end.
inline void PlanHolds(std::vector<Note>& notes,double songEnd=0) {
    for(auto& n:notes)if(n.hold)n.end=n.time;
    std::array<size_t,4> active{};size_t count=0;
    uint32_t lanes=0;double deadline=0;
    auto Close=[&](double end){
        if(songEnd>0)end=(std::min)(end,songEnd);
        for(size_t k=0;k<count;++k){auto& n=notes[active[k]];n.end=(std::max)(n.time,end);}
        count=0;lanes=0;
    };
    for(size_t i=0;i<notes.size();) {
        size_t stop=i+1;while(stop<notes.size()&&std::abs(notes[stop].time-notes[i].time)<.000001)++stop;
        const double time=notes[i].time;
        if(count&&time>=deadline-.000001)Close(deadline);
        uint32_t presses=0;
        for(size_t j=i;j<stop;++j)if(notes[j].lane>=1&&notes[j].lane<=4&&!notes[j].chain)presses|=1u<<notes[j].lane;
        if(count&&(presses&lanes))Close(time);
        for(size_t j=i;j<stop;++j)if(notes[j].hold&&notes[j].lane>=1&&notes[j].lane<=4&&count<active.size()) {
            if(!(lanes&(1u<<notes[j].lane))){active[count++]=j;lanes|=1u<<notes[j].lane;deadline=time+5;}
        }
        i=stop;
    }
    Close(deadline);
}
inline bool ShowHold(const Note& note,bool resolved,uint32_t heldLanes,double gameTime,double visualTime) {
    if(!note.hold||note.end<=visualTime)return false;
    if(!resolved)return note.time>=gameTime-.10;
    return (heldLanes&(1u<<note.lane))!=0;
}
}
