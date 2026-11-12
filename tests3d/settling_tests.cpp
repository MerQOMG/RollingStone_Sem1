#include "config.h"
#include "trial.h"
#include "jolt_runtime.h"
#include "legacy_settling_shape.h"
#include <iostream>
using namespace rs3d;
int main(){try{JoltRuntime runtime;SceneDraft s;s.count=1;s.masterSeed=20261113;s.terrain.type=TerrainType::flat;s.terrain.width=20;s.releaseGap=0;s.properties[restitutionValue]={0,0};s.maxTime=10;auto config=freezeConfig(s);auto input=sampleTrialInput(*config,1);auto shape=convexPolyhedron(legacySettlingVertices());auto legacyScene=makePhysicsScene(*config,input,shape);legacyScene.dynamics.manifoldReduction=true;Trial trial(legacyScene,{10});
    while(trial.snapshot().reason==StopReason::running){auto state=trial.step();auto d=trial.diagnostics();if(d.updates%250==0)std::cout<<state.time<<" v="<<norm(state.velocity)<<" omega="<<norm(state.angularVelocity)<<" K="<<state.kineticEnergy<<" P="<<state.potentialEnergy<<" RR="<<state.rollingEnergyRemoved<<" contact="<<d.rollingContactSteps<<" applied="<<d.rollingAppliedSteps<<" rejected="<<d.rollingRejectedSteps<<'\n';}
    std::cout<<stopReasonName(trial.snapshot().reason)<<'\n';if(trial.snapshot().reason!=StopReason::resting)return 1;
    s.stableFaceRelease=true;s.masterSeed=20261111;s.count=1000;config=freezeConfig(s);
    for(std::uint64_t id:{325,850}){auto specimen=sampleTrialInput(*config,id);auto rock=generatePolyhedron(s.geometry,specimen.seed);
        auto first=makePhysicsScene(*config,specimen,rock.geometry),second=first;
        auto flipped=s.terrain;flipped.flipDiagonals=true;second.terrain=std::make_shared<TerrainMesh>(buildTerrain(flipped));
        Trial one(first,{10}),two(second,{10});double maximum=0;
        while(one.snapshot().reason==StopReason::running||two.snapshot().reason==StopReason::running){
            if(one.snapshot().reason==StopReason::running)one.step();if(two.snapshot().reason==StopReason::running)two.step();
            maximum=std::max(maximum,norm(one.snapshot().center-two.snapshot().center));}
        std::cout<<"v3 stable face id="<<id<<" reasons="<<stopReasonName(one.snapshot().reason)<<','<<stopReasonName(two.snapshot().reason)
            <<" times="<<one.snapshot().time<<','<<two.snapshot().time<<" flipped_diagonal_max_path_difference_m="<<maximum<<'\n';
        if(one.snapshot().reason!=StopReason::resting||two.snapshot().reason!=StopReason::resting)throw std::runtime_error("v3 stable support face rocks beyond 10 seconds");
        if(maximum>.01)throw std::runtime_error("v3 support face depends on terrain triangulation");}
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
