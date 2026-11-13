// Graphics-free tests of the new front end's models: parameter form fidelity and validation,
// analysis-map semantics (frequency, bounds, conditional) and error translation.
#include "ui/analysis_model.h"
#include "ui/params.h"
#include "config.h"
#include "jolt_runtime.h"
#include "route_heatmap.h"
#include "first_barrier_contact.h"
#include <cstdlib>
#include <memory>
#include <iostream>
namespace {
int failures=0;
#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAILED line "<<__LINE__<<": "<<#cond<<'\n';++failures;}}while(0)
using namespace rs3d;using namespace rs3d::gui;
void formRoundTrip(const SceneDraft&d,const char*name){
    Form form(d);form.validate();
    if(!form.valid()){std::cerr<<name<<": form invalid: "<<form.formError()<<'\n';++failures;return;}
    CHECK(form.pendingTotal()==0);
    auto round=form.draft();
    // The form must be a lossless view of the scene text transport.
    CHECK(sceneText(round,false)==sceneText(d,false));
}
void formValidation(){
    SceneDraft d;Form form(d);
    form.value("terrain.height").a="-5";form.validate();CHECK(!form.valid());CHECK(!form.error("terrain.height").empty());CHECK(form.pending("terrain.height"));CHECK(form.pendingTotal()==1);
    form.value("terrain.height").a="abc";form.validate();CHECK(!form.valid());
    form.revert();form.validate();CHECK(form.valid());CHECK(form.pendingTotal()==0);
    form.value("run.count").a="250";form.validate();CHECK(form.valid());CHECK(form.draft().count==250);CHECK(form.pendingCount("run")==1);
    form.value("run.count").a="0";form.validate();CHECK(!form.valid());
    form.revert();
    // Hidden fields never block: with the barrier disabled its dimensions are not validated.
    form.value("barrier.width").a="-1";form.validate();CHECK(form.valid()||!form.visible(*findDef("barrier.width")));
    form.value("barrier.width").a="10";
    // Range ordering is reported next to the field.
    form.value("initial.mu").a="0.9";form.value("initial.mu").b="0.1";form.validate();CHECK(!form.valid());CHECK(!form.error("initial.mu").empty());
    form.revert();form.validate();CHECK(form.valid());
    // Conditional visibility.
    CHECK(!form.visible(*findDef("barrier.mass")));form.value("barrier.enabled").a="true";form.value("barrier.mode").a="elastic";CHECK(form.visible(*findDef("barrier.mass")));
    form.validate();CHECK(form.valid());auto e=form.draft();CHECK(e.barrier.enabled&&e.barrier.elastic);
    // Rebase keeps unapplied edits but adopts new applied values.
    SceneDraft other=d;other.previewSeed=99;Form f2(d);f2.value("run.count").a="321";f2.rebase(other);CHECK(f2.value("run.count").a=="321");CHECK(f2.appliedValue("preview.seed").a=="99");CHECK(f2.value("preview.seed").a=="99");
}
void errors(){
    auto h=explainError("run count outside 1..1000000");CHECK(!h.keys.empty()&&h.keys[0]=="run.count");
    h=explainError("terrain.height: invalid finite number");CHECK(h.keys.size()==1&&h.keys[0]=="terrain.height");
    h=explainError("something unknown");CHECK(h.keys.empty());
    CHECK(stopReasonZh(std::string("resting"))==u8"静止");CHECK(stopReasonZh(std::string("timeout"))==u8"超时");
    CHECK(formatPercent(0)=="0%");CHECK(formatPercent(.25)=="25.0%");CHECK(formatPercent(1)=="100%");
}
void maps(){
    SceneDraft d;d.count=10;d.barrier.enabled=true;auto cfg=freezeConfig(d);
    auto route=std::make_shared<RouteAccumulator>(*cfg->terrain,d.count,d.routeCell);
    AnalysisSnapshot a;a.route=route;
    auto m=buildMap(*cfg,a,MapKind::route,Metric::frequency,false);
    CHECK(m.applicable);CHECK(m.level==Level::warn);CHECK(m.denominator==double(d.count)); // nothing finished: lower bound, never "complete"
    auto hits=std::vector<std::uint64_t>(route->hits().size(),0);hits[0]=4;route->restore(hits,d.count);a.filesComplete=true;
    m=buildMap(*cfg,a,MapKind::route,Metric::frequency,false);CHECK(m.level==Level::ok);CHECK(m.usable);
    m=buildMap(*cfg,a,MapKind::route,Metric::count,false);CHECK(m.denominator==4.0);
    m=buildMap(*cfg,a,MapKind::route,Metric::frequency,true);CHECK(m.level==Level::error&&!m.usable); // read error invalidates probabilities
    m=buildMap(*cfg,a,MapKind::contact,Metric::conditional,false);CHECK(!m.usable); // no contact accumulator
}
}
int main(int argc,char**argv){
    try{
        JoltRuntime runtime;
        formRoundTrip(SceneDraft{},"default");
        if(argc>1)formRoundTrip(loadScene(argv[1]),"config");
        formValidation();errors();maps();
    }catch(const std::exception&e){std::cerr<<"exception: "<<e.what()<<'\n';return 2;}
    if(failures){std::cerr<<failures<<" failure(s)\n";return 1;}
    std::cout<<"ui model tests passed\n";return 0;
}
