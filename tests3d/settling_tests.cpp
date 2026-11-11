#include "config.h"
#include "trial.h"
#include "jolt_runtime.h"
#include <iostream>
using namespace rs3d;
int main(){try{JoltRuntime runtime;SceneDraft s;s.count=1;s.masterSeed=20261113;s.terrain.type=TerrainType::flat;s.terrain.width=20;s.releaseGap=0;s.properties[restitutionValue]={0,0};s.maxTime=10;auto config=freezeConfig(s);auto input=sampleTrialInput(*config,1);auto shape=generatePolyhedron(s.geometry,input.seed);Trial trial(makePhysicsScene(*config,input,shape.geometry),{10});
    while(trial.snapshot().reason==StopReason::running){auto state=trial.step();auto d=trial.diagnostics();if(d.updates%250==0)std::cout<<state.time<<" v="<<norm(state.velocity)<<" omega="<<norm(state.angularVelocity)<<" K="<<state.kineticEnergy<<" P="<<state.potentialEnergy<<" RR="<<state.rollingEnergyRemoved<<" contact="<<d.rollingContactSteps<<" applied="<<d.rollingAppliedSteps<<" rejected="<<d.rollingRejectedSteps<<'\n';}
    std::cout<<stopReasonName(trial.snapshot().reason)<<'\n';return trial.snapshot().reason==StopReason::resting?0:1;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
