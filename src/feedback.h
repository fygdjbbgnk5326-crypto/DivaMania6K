#pragma once
#include <cstdint>
#include <cmath>
namespace diva6k {
// Verified PV+0x2D248 layout: combo, survival bonus, gap, challenge combo,
// max combo, five total judge counters, five correct-input judge counters.
struct Metrics {
    int32_t combo,totalSurvival;
    uint8_t gap[8];
    int32_t challengeCombo,maxCombo;
    int32_t all[5],correct[5];
};
static_assert(sizeof(Metrics)==64);
class Feedback {
    Metrics previous{};
    bool known=false;
    double previousTime=0;
public:
    uint32_t mask=0;
    double until=0;
    void Clear(){known=false;mask=0;until=0;}
    bool Update(const Metrics& m,double time,bool reset=false) {
        if(m.combo<0||m.maxCombo<0||m.combo>2000000||m.maxCombo>2000000){Clear();return false;}
        for(int i=0;i<5;++i)if(m.all[i]<0||m.correct[i]<0||m.correct[i]>m.all[i]||m.all[i]>2000000){Clear();return false;}
        bool backwards=known&&time<previousTime-.05;
        for(int i=0;i<5;++i)if(m.all[i]<previous.all[i]||m.correct[i]<previous.correct[i])reset=true;
        if(!known||reset||backwards){mask=0;until=0;}
        else {
            uint32_t changed=0;
            for(int i=0;i<5;++i){int da=m.all[i]-previous.all[i],dc=m.correct[i]-previous.correct[i];if(dc>0)changed|=1u<<i;if(da>dc)changed|=1u<<5;}
            if(changed){mask=changed;until=time+.65;}
            else if(time>=until)mask=0;
        }
        previous=m;previousTime=time;known=true;return true;
    }
};
}
