#pragma once
#include <cstdint>
#include <algorithm>
namespace diva6k {
// Monotonic display timer: a frozen song clock must not leave feedback on screen.
class JudgementPopup {
    double observedUntil=0,previousSongTime=0;
    uint32_t observedMask=0;
    uint64_t deadline=0;
public:
    uint32_t mask=0;
    void Update(uint32_t incoming,double until,double songTime,bool paused,bool valid,uint64_t now,uint32_t durationMs) {
        if(!valid||paused||until==0||songTime<previousSongTime-.05) {
            mask=0;deadline=0;observedUntil=until;observedMask=incoming;
        } else if(incoming&&(until!=observedUntil||incoming!=observedMask)) {
            mask=incoming;deadline=now+durationMs;observedUntil=until;observedMask=incoming;
        }
        previousSongTime=songTime;
        if(now>=deadline)mask=0;
    }
    float Alpha(uint64_t now)const {
        if(!mask||now>=deadline)return 0;
        return (std::min)(1.f,static_cast<float>(deadline-now)/150.f);
    }
};
}
