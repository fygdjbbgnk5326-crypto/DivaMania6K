#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace diva6k {
class SongTimeline {
    bool known=false;
    double lastRaw=0;
public:
    double seconds=0,duration=0;
    bool ended=false;
    void Clear(){known=false;lastRaw=0;seconds=duration=0;ended=false;}
    void Update(double raw,int64_t endNs,bool endSignal,bool paused,bool reset=false) {
        if(!std::isfinite(raw))return;
        if(reset||(known&&!endSignal&&raw<lastRaw-.05))Clear();
        if(endNs>0&&endNs<=7200000000000LL)duration=endNs*1e-9;
        const double current=duration>0?std::clamp(raw,0.,duration):std::max(0.,raw);
        if(!known){seconds=current;known=true;}
        if(!ended) {
            if(endSignal) {
                // At a normal ending use the exact end time; an early failure
                // keeps the last gameplay timestamp rather than the result clock.
                if(duration>0&&raw>=duration)seconds=duration;
                ended=true;
            } else if(!paused) {
                seconds=current;
                if(duration>0&&raw>=duration)ended=true;
            }
        }
        lastRaw=raw;
    }
};
}
