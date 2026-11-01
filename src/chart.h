#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace diva6k {
struct Target {
    int32_t type;
    float targetX,targetY,startX,startY,amplitude;
    int32_t frequency;
    uint8_t chainStart,chainEnd,chainLeft,chainRight,chain;
    uint8_t padding[3];
};
struct Group {
    int32_t count;
    Target targets[4];
    int32_t field94;
    int64_t spawnNs,hitNs;
    uint8_t chain;
    uint8_t padding[7];
};
static_assert(sizeof(Target)==36);
static_assert(sizeof(Group)==176);
static_assert(offsetof(Group,hitNs)==160);
struct Note {
    double time=0,end=0;
    int lane=0,type=0,group=0;
    bool hold=false,chain=false,doubled=false,multi=false;
};
inline int Lane(int type) {
    constexpr int face[]={2,4,3,1}; // Triangle(up/F), Circle(right/K), Cross(down/J), Square(left/D).
    if(type>=0&&type<=7)return face[type%4];
    if(type>=18&&type<=21)return face[type-18];
    if(type==12||type==15||type==23)return 0;
    if(type==13||type==16||type==24)return 5;
    return -1; // Unknown/console-mod targets must never be silently remapped.
}
inline bool Convert(const std::vector<Group>& groups,std::vector<Note>& output,size_t& unknown) {
    std::vector<Note> notes;notes.reserve(groups.size()*2);unknown=0;
    int openChain[2]={-1,-1};
    for(size_t i=0;i<groups.size();++i) {
        const auto& g=groups[i];
        if(g.count<0||g.count>4||g.hitNs<0||g.hitNs>7200000000000LL)return false;
        const double time=g.hitNs*1e-9;
        for(int t=0;t<g.count;++t) {
            const auto& target=g.targets[t];int lane=Lane(target.type);
            if(lane<0){++unknown;continue;}
            bool chain=target.chain||target.type==15||target.type==16;
            int side=lane==0?0:1;
            if(chain&&(lane==0||lane==5)) {
                if(target.chainStart||openChain[side]<0){notes.push_back({time,time,lane,target.type,static_cast<int>(i),false,true,false,g.count>1});openChain[side]=static_cast<int>(notes.size()-1);}
                notes[openChain[side]].end=time;
                if(target.chainEnd)openChain[side]=-1;
            } else {
                // Same-direction double slides remain represented, with a double marker.
                bool duplicate=false;
                for(auto it=notes.rbegin();it!=notes.rend()&&it->group==static_cast<int>(i);++it)if(it->lane==lane){it->doubled=true;duplicate=true;break;}
                if(!duplicate)notes.push_back({time,time,lane,target.type,static_cast<int>(i),target.type>=4&&target.type<=7,false,false,g.count>1});
            }
        }
    }
    std::stable_sort(notes.begin(),notes.end(),[](const Note&a,const Note&b){return a.time<b.time;});output.swap(notes);return true;
}
// Index visible intervals by end time too, so long chains that began earlier remain visible.
inline void BuildPrefixEnds(const std::vector<Note>& notes,std::vector<double>& ends) {
    ends.clear();ends.reserve(notes.size());double largest=-1;
    for(const auto& n:notes){largest=(std::max)(largest,n.end);ends.push_back(largest);}
}
}
