#pragma once
#include "result_archive.h"
#include "session.h"
#include "ui/analysis_model.h"
#include "ui/app.h"
#include "ui/dialogs.h"
#include "ui/params.h"
#include "ui/replay.h"
#include "ui/viewport3d.h"
#include "ui/widgets.h"
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <set>
namespace rs3d::gui {
inline std::string sfmt(const char*f,...){char b[640];va_list a;va_start(a,f);std::vsnprintf(b,sizeof(b),f,a);va_end(a);return b;}
inline std::string friendly(const std::string&technical){auto h=explainError(technical);if(h.keys.empty()&&h.message==u8"配置未通过检查：")return u8"出错："+technical;return h.message;}
enum class Tab { scene,run,analysis };
struct Toast {std::string text;Color color;double until;};
struct Modal {bool open=false;std::string title,body,ok;std::function<void()> onOk;};
// Vertical layout cursor used by the scrolling panels.
struct Col {
    float x=0,y=0,w=0,origin=0;
    R row(float h,float gap=6){R r{x,y,w,h};y+=h+gap;return r;}
    void space(float h){y+=h;}
    float used()const{return y-origin;}
};
class App {
public:
    App(const SceneDraft&draft,const AppOptions&options);
    int run();
private:
    AppOptions opt_;
    SceneDraft editDraft_;
    std::unique_ptr<Session> session_;
    std::unique_ptr<Form> form_;
    Replay replay_;Viewport3D vp_;Ui ui_;
    SessionView sv_;
    Tab tab_=Tab::scene;
    bool reframe_=true,demo_=false,help_=false,quit_=false,shotPending_=false;
    float W_=0,H_=0,zoom_=1,baseScale_=1;
    R topR_,leftR_,viewR_,dockR_;
    std::vector<R> occluders_;
    ScrollState sceneScroll_,runScroll_,anaScroll_,trialScroll_,helpScroll_;
    std::map<std::string,bool> open_;
    std::vector<Toast> toasts_;
    Modal modal_;
    std::vector<std::function<void()>> menuActions_;
    bool timelineCapture_=false,formDirty_=true;std::set<std::string> rangeMode_;std::string outputText_;
    double now_=0,lastHeatAt_=0;
    // analysis tab state
    MapKind mapKind_=MapKind::route;Metric metric_=Metric::frequency;
    bool showTrails_=true,showHeat_=true,showRock_=true,showBand_=true;double trailAlpha_=.7;
    std::optional<std::size_t> hoverCell_;
    std::string gotoText_;
    std::shared_ptr<const AnalysisSnapshot> mapAnalysis_;MapData map_;
    std::uint64_t trajVersion_=~0ull;const void*trajOwner_=nullptr;
    int previewMode_=0;
    std::filesystem::path shotPath_;
    // tour
    std::ofstream log_;bool tabLogged_=false;bool dumpCtl_=false;int failedFrames_=0;int dumpFrame_=0;Vector2 lastMouse_{-1,-1};int lastBtn_=-1;std::filesystem::path tourDir_;int tourStep_=0,tourWait_=0;std::string shotName_;
    // --- core
    void bindSession(std::unique_ptr<Session> s);
    void refreshResolved();
    void frame(float dt);
    void layout();
    void shortcuts();
    void drawChrome();
    void drawDemo();
    void drawViewport();
    void drawTopBar();
    void drawFileMenu(R anchor);
    void drawDock();
    void drawModal();
    void drawHelp();
    void drawToasts();
    void drawRunPill(R view);
    void drawViewOverlay(R view);
    void drawHud(R view);
    void drawLegend(R view);
    // tabs
    void drawLeft();
    void drawSceneTab(R view);
    void drawRunTab(R view);
    void drawAnalysisTab(R view);
    void drawApplyBar(R bar);
    float drawField(Col&c,const FieldDef&d,bool enabled);
    void drawSpecimen(Col&c);
    void drawRunControls(Col&c,bool compact);
    void drawBatchSummary(Col&c);
    void drawMapPlot(R r);
    void drawTrialList(Col&c,float height);
    void drawStopLegend(Col&c);
    void drawTimeline(R dock);
    void drawSceneStrip(R dock);
    void drawRunStrip(R dock);
    // actions
    bool applyEdits();
    void startSingle();
    void startBatch();
    void pauseResume();
    void cancelRun();
    void newSpecimen();
    void openScene();
    void saveSceneFile();
    void openResults(std::filesystem::path dir);
    void pickResults();
    void backToEditing();
    void setTab(Tab t);
    void toast(const std::string&text,Color c=col::ok);
    void confirm(const std::string&title,const std::string&body,const std::string&ok,std::function<void()> onOk);
    void setDemo(bool on);
    void syncViewportContent();
    bool running()const{return sv_.state==SessionState::running||sv_.state==SessionState::paused||sv_.state==SessionState::cancelling||!sv_.workerFinished;}
    bool paused()const{return sv_.state==SessionState::paused;}
    void tourTick();
    void captureShot(const std::string&name);
    void flushShot();
    Snapshot shownSnapshot(std::shared_ptr<const PhysicsScene>&scene)const;
};
}
