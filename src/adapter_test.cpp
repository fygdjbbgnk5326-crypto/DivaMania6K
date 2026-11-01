#include "game_adapter.h"
#include "popup.h"
#include "resolved.h"
#include "render_clock.h"
#include "hold_plan.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
using namespace diva6k;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()try {
    auto H=[](double t,int lane){Note n;n.time=n.end=t;n.lane=lane;n.hold=true;return n;};
    auto N=[](double t,int lane){Note n;n.time=n.end=t;n.lane=lane;return n;};
    std::vector<Note> planned{H(1,2)};PlanHolds(planned);
    Require(planned[0].end==6,"single HOLD tail planned before hit at five-second MAX");
    planned={H(1,2),H(3,1)};PlanHolds(planned);
    Require(planned[0].end==8&&planned[1].end==8,"additional distinct HOLD extends all shared tails before play");
    PlanHolds(planned);Require(planned[0].end==8,"HOLD planning is idempotent");
    planned={H(1,2),H(3,1),N(4,2)};PlanHolds(planned);
    Require(planned[0].end==4&&planned[1].end==4,"same-key re-press ends shared six-key HOLD plan");
    planned={H(1,2),N(2,0),N(3,4),H(7,1)};PlanHolds(planned);
    Require(planned[0].end==6&&planned[3].end==12,"slides and unrelated taps preserve HOLD; MAX splits sessions");
    planned={H(1,2),H(1,1),H(2,3),H(3,4)};PlanHolds(planned);
    for(const auto& n:planned)Require(n.end==8,"simultaneous and four-key additions share planned MAX endpoint");
    planned={H(1,2),H(2,2)};PlanHolds(planned);
    Require(planned[0].end==2&&planned[1].end==7,"repeated same HOLD lane starts a new session");
    planned={H(1,2),H(3,1)};PlanHolds(planned,4);
    Require(planned[0].end==4&&planned[1].end==4,"song end limits planned HOLD tails");
    Require(ShowHold(planned[0],false,0,.5,.5),"full HOLD body visible before input");
    Require(ShowHold(planned[0],true,1u<<2,1.1,1.1)&&planned[0].end==4,"hit preserves precomputed tail");
    Require(!ShowHold(planned[0],true,0,1.2,1.2),"release or MAX removes planned body using real state");
    Require(!ShowHold(planned[0],false,0,2,2),"missed HOLD cannot keep a fake active body");
    Require(!ShowHold(planned[0],true,1u<<2,4,4),"planned tail expires at endpoint");
    RenderClock animation;
    Require(animation.Update(1,0,true)==1,"animation starts at game clock");
    Require(animation.Update(1,.01,true)==1,"pre-song static clock is not animated");
    animation.Update(1+1./60,1./60,true);
    double inBetween=animation.Update(1+1./60,.025,true);
    Require(inBetween>1+1./60&&inBetween<1+2./60,"falling notes advance between game ticks");
    double stall=animation.Update(1+1./60,.2,true);
    Require(stall<=1+2./60+.000001,"stalled clock extrapolates at most one tick");
    Require(animation.Update(1+1./60,.21,false)==1+1./60,"pause/end/disabled mode returns exact game position");
    Require(animation.Update(.1,.22,true)==.1,"restart snaps to new song time");
    Require(animation.Update(20,.23,true)==20,"forward seek snaps without animation lag");
    Require(animation.Update(20,.8,true)==20,"long render stall discards old interpolation");
    for(int fps:{60,120,144,240}) {
        RenderClock simulated;double last=-1,rawError=0,smoothError=0,previousRaw=0;
        for(int frame=0;frame<fps*2;++frame) {
            double now=static_cast<double>(frame)/fps;
            double raw=std::floor(now*60+1e-8)/60;
            double display=simulated.Update(raw,now,true);
            Require(display>=last&&display>=raw&&display<=raw+1./30+.000001,"smooth clock remains monotonic and bounded");
            if(frame>fps/2){rawError+=std::pow(raw-previousRaw-1./fps,2);smoothError+=std::pow(display-last-1./fps,2);}
            previousRaw=raw;last=display;
        }
        if(fps>60)Require(smoothError<rawError*.8,"higher refresh rates reduce repeated-frame movement error");
        printf("PASS: simulated 60Hz clock at %d render FPS, motion error %.6f -> %.6f\n",fps,rawError,smoothError);
    }
    SongTimeline song;
    song.Update(1,10000000000LL,false,false);Require(song.seconds==1&&song.duration==10&&!song.ended,"song uses actual duration in nanoseconds");
    song.Update(2,10000000000LL,false,true);Require(song.seconds==1,"pause freezes progress");
    song.Update(3,10000000000LL,false,false);Require(song.seconds==3,"resume follows game clock");
    song.Update(30,10000000000LL,true,false);Require(song.seconds==10&&song.ended,"normal end clamps to exact duration");
    song.Update(35,10000000000LL,true,false);Require(song.seconds==10,"result clock cannot advance completed timer");
    song.Update(0,10000000000LL,false,false);Require(!song.ended&&song.seconds==0,"retry restarts completed timeline");
    song.Update(2,10000000000LL,false,false);song.Update(2.1,10000000000LL,true,false);
    Require(song.ended&&song.seconds==2,"early failure preserves last gameplay time");
    song.Update(99,10000000000LL,false,false);Require(song.seconds==2,"ended state latches even after end signal drops");
    song.Update(8,20000000000LL,false,false,true);Require(song.duration==20&&song.seconds==8&&!song.ended,"new chart resets old end state");
    song.Clear();song.Update(5,0,false,false);Require(song.duration==0&&song.seconds==5,"missing duration is not replaced with last-note estimate");
    song.Update(6,0,true,false);Require(song.ended&&song.seconds==5,"end flag freezes timer even with unknown duration");
    song.Clear();song.Update(11,10000000000LL,false,false);Require(song.ended&&song.seconds==10,"duration bounds late result clock even without end flag");
    HoldRaw hold{};hold.state=1;hold.buttons=0x40|0x200;hold.timer=3;hold.lastAdded=1;
    auto holdView=DecodeHold(hold);Require(holdView.lanes==((1u<<2)|(1u<<1))&&holdView.remaining==3,"real HOLD buttons and remaining bonus time");
    hold.lastAdded=2;Require(DecodeHold(hold).remaining==4,"additional HOLD extends shared reward timer");
    hold.state=2;Require(DecodeHold(hold).lanes==0,"MAX HOLD clears long bars");
    hold.state=0;Require(DecodeHold(hold).lanes==0,"released HOLD clears long bars");
    JudgementPopup popup;
    popup.Update(1,1.65,1,false,true,1000,500);Require(popup.mask==1&&popup.Alpha(1000)==1,"popup begins fully visible");
    Require(popup.Alpha(1400)>0&&popup.Alpha(1400)<1,"popup fades during final 150ms");
    popup.Update(1,1.65,1,false,true,1501,500);Require(popup.mask==0,"frozen song clock cannot keep popup alive");
    popup.Update(1,1.65,1,false,true,1600,500);Require(popup.mask==0,"unchanged judgement cannot retrigger");
    popup.Update(1,1.75,1.1,false,true,1700,500);Require(popup.mask==1,"next same-category hit retriggers");
    popup.Update(1,1.75,1.1,true,true,1750,500);Require(popup.mask==0,"pause clears popup");
    popup.Update(1,1.75,1.1,false,true,1800,500);Require(popup.mask==0,"resume cannot replay paused popup");
    popup.Update(2,2.65,2,false,true,2000,2000);popup.Update(0,2.65,2.7,false,true,2800,2000);Require(popup.mask==2,"configured duration independent of game-counter retention");
    popup.Update(0,0,0,false,true,2900,500);Require(popup.mask==0,"restart clears popup");
    Require(Lane(0)==2&&Lane(1)==4&&Lane(2)==3&&Lane(3)==1,"screenshot mapping: D left, F up, J down, K right");
    Require(Lane(4)==2&&Lane(7)==1&&Lane(12)==0&&Lane(13)==5&&Lane(15)==0&&Lane(16)==5,"hold/slide mapping");
    Require(Lane(8)==-1&&Lane(25)==-1,"unknown types rejected");
    Feedback feedback;Metrics metrics{};
    Require(feedback.Update(metrics,0)&&feedback.mask==0,"initial snapshot has no fake judgement");
    metrics.combo=1;metrics.maxCombo=1;metrics.all[0]=metrics.correct[0]=1;
    Require(feedback.Update(metrics,1)&&feedback.mask==1,"COOL from correct counter delta");
    Require(feedback.Update(metrics,1)&&feedback.mask==1,"paused game time does not expire feedback");
    Require(feedback.Update(metrics,1.7)&&feedback.mask==0,"judgement expires by game time");
    metrics.combo=0;++metrics.all[1];
    Require(feedback.Update(metrics,2)&&feedback.mask==(1u<<5),"wrong-key FINE detected as WRONG");
    ++metrics.all[4];++metrics.correct[4];
    Require(feedback.Update(metrics,3)&&feedback.mask==(1u<<4),"miss detected as WORST");
    ++metrics.all[0];++metrics.correct[0];++metrics.all[1];++metrics.correct[1];
    Require(feedback.Update(metrics,4)&&feedback.mask==3,"same-frame mixed judgements preserved");
    Require(feedback.Update(metrics,.1)&&feedback.mask==0,"backwards seek clears judgement");
    metrics={};Require(feedback.Update(metrics,0)&&feedback.mask==0,"retry resets counter baseline");
    metrics.correct[0]=1;Require(!feedback.Update(metrics,0),"inconsistent counters rejected");
    std::vector<Group> groups(4);std::vector<Note> notes;size_t unknown=0;
    for(int i=0;i<4;++i){groups[i].count=1;groups[i].hitNs=(i+1)*1000000000LL;}
    groups[0].count=4;for(int i=0;i<4;++i)groups[0].targets[i].type=i;
    groups[1].targets[0].type=15;groups[1].targets[0].chainStart=1;
    groups[2].targets[0].type=15;groups[2].targets[0].chainEnd=1;
    groups[3].count=2;groups[3].targets[0].type=groups[3].targets[1].type=13;
    Require(Convert(groups,notes,unknown)&&notes.size()==6&&unknown==0,"quad/chains/double conversion");
    Require(notes[4].chain&&notes[4].time==2&&notes[4].end==3&&notes[5].doubled,"chain interval/double preserved");
    for(int i=0;i<4;++i)Require(notes[i].multi,"all four simultaneous notes have orange-outline metadata");
    Require(!notes[4].multi&&notes[5].multi,"single chain excluded; same-direction double slide included");
    for(int count=2;count<=3;++count){Group multiGroup{};multiGroup.count=count;multiGroup.hitNs=1000000000;for(int i=0;i<count;++i)multiGroup.targets[i].type=i;std::vector<Note> converted;Require(Convert({multiGroup},converted,unknown)&&converted.size()==static_cast<size_t>(count),"multi group conversion");for(const auto& n:converted)Require(n.multi,"double/triple notes outlined");}
    std::vector<double> ends;BuildPrefixEnds(notes,ends);Require(std::lower_bound(ends.begin(),ends.end(),2.5)-ends.begin()==4,"active long chain visible");
    std::vector<uint8_t> resolved(notes.size());
    RuntimeTarget hit{};hit.type=0;hit.hitNs=1000000000LL;hit.group=0;
    ResolveTarget(hit,1,notes,resolved);Require(resolved[0]==0,"unjudged target must stay visible");
    hit.judgement=0;ResolveTarget(hit,.9,notes,resolved);
    Require(resolved[0]==1&&resolved[1]==0&&resolved[2]==0&&resolved[3]==0,"early real hit clears only matching lane of chord");
    hit.type=1;hit.judgement=4;ResolveTarget(hit,1,notes,resolved);Require(resolved[1]==1,"game-resolved wrong key clears processed target");
    hit.type=2;hit.group=99;ResolveTarget(hit,1,notes,resolved);Require(resolved[2]==0,"wrong group cannot erase same-time note");
    hit.group=0;hit.hitNs=1010000000LL;ResolveTarget(hit,1,notes,resolved);Require(resolved[2]==0,"time mismatch cannot erase adjacent note");
    hit.hitNs=1000000000LL;ResolveTarget(hit,3,notes,resolved);Require(resolved[2]==0,"stale runtime slot ignored");
    hit.type=15;hit.group=1;hit.hitNs=2000000000LL;ResolveTarget(hit,2,notes,resolved);Require(resolved[4]==0,"chain start hit preserves long body");
    std::vector<Note> holdNotes{{1,1,2,4,0,true}};std::vector<uint8_t> holdResolved(1);
    hit.type=4;hit.group=0;hit.hitNs=1000000000LL;ResolveTarget(hit,1,holdNotes,holdResolved);Require(holdResolved[0]==1,"HOLD entry head clears on real hit");
    groups[0].count=5;Require(!Convert(groups,notes,unknown),"malformed group rejected");groups[0].count=4;
    std::vector<uint8_t> pv(190000),arcade(80000);
    int64_t songEndNs=4000000000LL;memcpy(pv.data()+0x2d330,&songEndNs,sizeof(songEndNs));
    std::array<RuntimeTarget,64> runtime{};memcpy(arcade.data()+40,runtime.data(),sizeof(runtime));
    auto header=[&](){uintptr_t p[]={reinterpret_cast<uintptr_t>(groups.data()),reinterpret_cast<uintptr_t>(groups.data()+groups.size()),reinterpret_cast<uintptr_t>(groups.data()+groups.size())};memcpy(pv.data()+181680,p,sizeof(p));};header();
    pv[0]=1;arcade[0]=1;float time=2.5f;memcpy(arcade.data()+78480,&time,4);
    GameAdapter adapter;adapter.TestPointers(reinterpret_cast<uintptr_t>(pv.data()),reinterpret_cast<uintptr_t>(arcade.data()));adapter.Update();
    Require(adapter.State().active&&adapter.State().notes.size()==6&&adapter.State().seconds==2.5,"live memory snapshot");
    time=1;memcpy(arcade.data()+78480,&time,4);runtime[0].type=0;runtime[0].hitNs=1000000000LL;runtime[0].group=0;runtime[0].judgement=0;
    memcpy(arcade.data()+40,runtime.data(),sizeof(runtime));adapter.Update();
    Require(adapter.State().resolved[0]==1&&adapter.State().resolved[1]==0,"adapter reads per-target judgement independent of keyboard");
    runtime[0]={};memcpy(arcade.data()+40,runtime.data(),sizeof(runtime));adapter.Update();Require(adapter.State().resolved[0]==1,"slot reuse cannot resurrect hit head");
    time=2.5f;memcpy(arcade.data()+78480,&time,4);adapter.Update();
    pv[1]=1;adapter.Update();adapter.Update();Require(adapter.State().paused&&adapter.State().seconds==2.5,"paused clock stable");
    pv[1]=0;pv[0x2d17d]=1;adapter.Update();
    time=30;memcpy(arcade.data()+78480,&time,4);adapter.Update();
    Require(adapter.State().ended&&adapter.State().seconds==2.5&&adapter.State().duration==4,"results screen keeps final timer and real total duration");
    pv[0x2d17d]=0;
    time=.25f;memcpy(arcade.data()+78480,&time,4);pv[1]=0;adapter.Update();Require(adapter.State().seconds==.25,"restart/backward seek reads game clock directly");
    Require(!adapter.State().ended,"retry leaves results state");
    Require(adapter.State().resolved[0]==0,"retry clears consumed-note state");
    pv[0]=0;adapter.Update();Require(!adapter.State().active&&adapter.State().notes.empty(),"unload clears cache");
    Require(adapter.State().resolved.empty(),"unload clears consumed-note state");
    pv[0]=1;header();adapter.Update();Require(adapter.State().notes.size()==6,"reenter same allocation reloads chart");
    uintptr_t invalid[]={1,177,177};memcpy(pv.data()+181680,invalid,sizeof(invalid));adapter.Update();Require(!adapter.State().active,"invalid pointer fails closed");
    std::ifstream real("build/real-chart.bin",std::ios::binary|std::ios::ate);
    if(real){size_t bytes=static_cast<size_t>(real.tellg());Require(bytes%sizeof(Group)==0,"real capture size");std::vector<Group> captured(bytes/sizeof(Group));real.seekg(0);real.read(reinterpret_cast<char*>(captured.data()),bytes);Require(Convert(captured,notes,unknown),"real chart conversion");printf("PASS: real game capture: %zu groups -> %zu notes; unknown=%zu\n",captured.size(),notes.size(),unknown);}
    puts("PASS: mapping, resolved heads, HOLD preservation, song progress, paused/end timers, retry/seek, unloading and invalid memory");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
