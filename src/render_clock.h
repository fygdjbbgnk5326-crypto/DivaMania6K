#pragma once
#include <algorithm>
#include <cmath>
namespace diva6k {
// Bounded visual extrapolation between observed game-clock samples. Never used
// for hit detection, song progress, or judgement timestamps.
class RenderClock {
    bool known=false,advancing=false;
    double sample=0,sampleWall=0,lastWall=0,value=0,step=1./60;
public:
    void Clear(){known=false;advancing=false;}
    double Update(double gameSeconds,double now,bool playing) {
        if(!std::isfinite(gameSeconds)||!std::isfinite(now)){Clear();return 0;}
        if(!playing){Clear();return gameSeconds;}
        if(!known||gameSeconds<sample-.05||gameSeconds>sample+.25||now<lastWall||now-lastWall>.25) {
            known=true;advancing=false;sample=gameSeconds;value=gameSeconds;
            sampleWall=lastWall=now;step=1./60;return value;
        }
        if(gameSeconds>sample+.000001) {
            step=std::clamp(gameSeconds-sample,1./240,1./30);
            sample=gameSeconds;sampleWall=now;advancing=true;
        }
        lastWall=now;
        const double candidate=sample+(advancing?std::clamp(now-sampleWall,0.,step):0.);
        // A late sample must not pull falling notes backwards. If the game clock
        // stalls, freeze after at most one observed tick (capped at 33.3ms).
        value=std::clamp((std::max)(value,candidate),gameSeconds,gameSeconds+step);
        return value;
    }
};
}
