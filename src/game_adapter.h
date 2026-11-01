#pragma once
#include "chart.h"
#include "feedback.h"
#include "hold.h"
#include "timeline.h"
#include <windows.h>
#include <string>
namespace diva6k {
struct Snapshot {
    bool active=false,paused=false,compatible=false;
    double seconds=0;
    double duration=0;
    bool ended=false;
    bool metricsValid=false;
    int combo=0,maxCombo=0;
    uint32_t judgement=0;
    double judgementUntil=0;
    HoldView hold;
    size_t unknown=0;
    std::vector<Note> notes;
    std::vector<double> prefixEnds;
    std::vector<uint8_t> resolved;
    std::string event;
};
class GameAdapter {
public:
    void Initialize();
    void Update();
    const Snapshot& State()const{return state;}
#ifdef DIVAMANIA_TEST
    void TestPointers(uintptr_t game,uintptr_t arcade){pv=game;ac=arcade;state.compatible=true;}
#endif
private:
    Snapshot state;
    uintptr_t pv=0,ac=0,lastBegin=0,lastEnd=0;
    uint64_t lastHash=0;
    ULONGLONG nextChartCheck=0;
    bool wasActive=false;
    Feedback feedback;
    SongTimeline timeline;
    double lastPlanDuration=-1;
    void Clear();
};
}
