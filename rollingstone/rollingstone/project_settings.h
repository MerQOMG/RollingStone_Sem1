#ifndef ROCK_PROJECT_SETTINGS_H
#define ROCK_PROJECT_SETTINGS_H
#include <sstream>
#include <cctype>
#include <functional>
#include <atomic>
#include <thread>
#include <memory>
#include <commdlg.h>

// 实验输入的唯一数据源；界面、配置文件和批量模式共享，派生量不单独输入。
struct ProjectSettings {
    double shapeMode=0, sidesMin=3, sidesMax=6, radius=2, area=3, density=2600, thickness=1;
    double releaseX=0, releaseLift=0, initialSpeed=1.5, initialSpin=0, initialAngle=0;
    double friction=0.45, restitution=0.45, rolling=0.02, drag=1, airDensity=1.225;
    double terrainMode=0, topHeight=55.6, steepAngle=40, benchStart=25, benchLength=15, benchAngle=7, toeLength=20;
    double barrierEnabled=1, barrierAuto=1, barrierX=108, barrierHeight=8, barrierMass=2000;
    double barrierStiffness=500000, barrierDamping=30000, barrierFriction=0.3;
    double batchCount=500, seed=2026, cv=0.1, randomSpeed=1, randomSurface=1, randomGeometry=1;
    double batchRange=1, batchXMin=90, batchXMax=100, batchSpeed=10, batchBarrier=0;
    double analysisHeight=10, heightReference=0, collectorAuto=1, collectorX=110;
    double gridX=1, gridZ=1, batchMaxTime=30, trajectoryInterval=0.05;
    double gravity=9.81, timeStep=0.002, maxTime=180, playback=1, trailLimit=800, chartWindow=10, zoom=1;
    double restSpeed=0.015, restTime=2, settleWait=3;
    std::string terrainPoints="0 55.6;10 51.9;11.25 50.6;30 35.6;40 34.4;50 23.1;60 20.6;70 15.6;80 10;90 8.75;100 2.88;120 0";
};
struct ParameterField {
    const char* key; const wchar_t* label; int page;
    double ProjectSettings::* member; double lo,hi; bool integer; const wchar_t* choices;
};
const std::vector<ParameterField>& parameterFields() {
    static const std::vector<ParameterField> fields={
        {"shape_mode",L"形状模式",0,&ProjectSettings::shapeMode,0,1,true,L"随机凸多边形|近圆正多边形"},
        {"sides_min",L"最小边数",0,&ProjectSettings::sidesMin,3,30,true,nullptr},
        {"sides_max",L"最大边数",0,&ProjectSettings::sidesMax,3,30,true,nullptr},
        {"radius_m",L"外接圆半径 / m（随机块石模式）",0,&ProjectSettings::radius,0.05,10,false,nullptr},
        {"area_m2",L"截面积 / m²",0,&ProjectSettings::area,0.000001,314,false,nullptr},
        {"density_kg_m3",L"石材密度 / kg·m⁻³",0,&ProjectSettings::density,100,10000,false,nullptr},
        {"thickness_m",L"截面挤出厚度 / m",0,&ProjectSettings::thickness,0.01,20,false,nullptr},
        {"release_x_m",L"释放位置 x / m",0,&ProjectSettings::releaseX,0,120,false,nullptr},
        {"release_lift_m",L"贴地放置后的抬升量 / m",0,&ProjectSettings::releaseLift,0,30,false,nullptr},
        {"initial_speed_m_s",L"单次顺坡初速度 / m·s⁻¹",0,&ProjectSettings::initialSpeed,0,100,false,nullptr},
        {"initial_spin_rad_s",L"初始角速度 / rad·s⁻¹",0,&ProjectSettings::initialSpin,-100,100,false,nullptr},
        {"initial_angle_deg",L"初始姿态角 / °",0,&ProjectSettings::initialAngle,-180,180,false,nullptr},
        {"friction",L"地面摩擦系数 μ",1,&ProjectSettings::friction,0,1.5,false,nullptr},
        {"restitution",L"地面恢复系数 e",1,&ProjectSettings::restitution,0,0.95,false,nullptr},
        {"rolling_resistance",L"滚动阻力系数 c_rr",1,&ProjectSettings::rolling,0,0.3,false,nullptr},
        {"drag_coefficient",L"空气阻力系数 Cd",1,&ProjectSettings::drag,0,3,false,nullptr},
        {"air_density_kg_m3",L"空气密度 / kg·m⁻³",1,&ProjectSettings::airDensity,0,5,false,nullptr},
        {"terrain_mode",L"地形来源",2,&ProjectSettings::terrainMode,0,2,true,L"课程原始地形|六参数山坡|自定义控制点"},
        {"top_height_m",L"坡顶高度 H / m",2,&ProjectSettings::topHeight,5,60,false,nullptr},
        {"steep_angle_deg",L"陡坡角 / °",2,&ProjectSettings::steepAngle,15,70,false,nullptr},
        {"bench_start_m",L"缓台起点水平距离 / m",2,&ProjectSettings::benchStart,0,110,false,nullptr},
        {"bench_length_m",L"缓台水平长度 / m",2,&ProjectSettings::benchLength,0,100,false,nullptr},
        {"bench_angle_deg",L"缓台坡角 / °",2,&ProjectSettings::benchAngle,0,69,false,nullptr},
        {"toe_length_m",L"坡脚平台长度 / m",2,&ProjectSettings::toeLength,5,100,false,nullptr},
        {"barrier_enabled",L"单次模拟挡板",3,&ProjectSettings::barrierEnabled,0,1,true,L"关闭|启用"},
        {"barrier_auto",L"挡板安装位置",3,&ProjectSettings::barrierAuto,0,1,true,L"手动指定|随地形自动安装"},
        {"barrier_x_m",L"手动安装位置 x / m",3,&ProjectSettings::barrierX,0,120,false,nullptr},
        {"barrier_height_m",L"挡板高度 / m",3,&ProjectSettings::barrierHeight,0.5,30,false,nullptr},
        {"barrier_mass_kg",L"等效挡板质量 / kg",3,&ProjectSettings::barrierMass,1,10000000,false,nullptr},
        {"barrier_stiffness_N_m",L"弹簧刚度 k / N·m⁻¹",3,&ProjectSettings::barrierStiffness,0,100000000,false,nullptr},
        {"barrier_damping_N_s_m",L"阻尼 c / N·s·m⁻¹",3,&ProjectSettings::barrierDamping,0,10000000,false,nullptr},
        {"barrier_friction",L"石头与挡板的摩擦系数",3,&ProjectSettings::barrierFriction,0,1.5,false,nullptr},
        {"batch_count",L"模拟次数 N",4,&ProjectSettings::batchCount,1,20000,true,nullptr},
        {"seed",L"随机种子（固定数值可复现）",4,&ProjectSettings::seed,1,4294967295.0,true,nullptr},
        {"cv",L"变异系数 CV（课程要求 0.1）",4,&ProjectSettings::cv,0,0.5,false,nullptr},
        {"random_speed",L"初速度正态抽样",4,&ProjectSettings::randomSpeed,0,1,true,L"关闭：固定均值|启用"},
        {"random_surface",L"地表参数正态抽样",4,&ProjectSettings::randomSurface,0,1,true,L"关闭：固定均值|启用"},
        {"random_geometry",L"尺寸参数正态抽样",4,&ProjectSettings::randomGeometry,0,1,true,L"关闭：固定均值|启用"},
        {"batch_release_range",L"批量释放位置",4,&ProjectSettings::batchRange,0,1,true,L"使用单次释放位置|区间内均匀抽样"},
        {"batch_x_min_m",L"释放区间起点 / m",4,&ProjectSettings::batchXMin,0,120,false,nullptr},
        {"batch_x_max_m",L"释放区间终点 / m",4,&ProjectSettings::batchXMax,0,120,false,nullptr},
        {"batch_speed_mean_m_s",L"批量初速度均值 / m·s⁻¹",4,&ProjectSettings::batchSpeed,0,100,false,nullptr},
        {"batch_use_barrier",L"批量模拟挡板",4,&ProjectSettings::batchBarrier,0,1,true,L"关闭|启用当前挡板参数"},
        {"analysis_height_m",L"统计高度上限 / m（课程10m）",4,&ProjectSettings::analysisHeight,0.5,30,false,nullptr},
        {"height_reference",L"统计高度基准",4,&ProjectSettings::heightReference,0,1,true,L"绝对高程 z|离当地地面的高度"},
        {"collector_auto",L"收集器位置",4,&ProjectSettings::collectorAuto,0,1,true,L"手动指定|随地形自动定位"},
        {"collector_x_m",L"手动收集器断面 x / m",4,&ProjectSettings::collectorX,0,120,false,nullptr},
        {"grid_x_m",L"概率网格水平宽度 / m",4,&ProjectSettings::gridX,0.1,10,false,nullptr},
        {"grid_z_m",L"概率网格高度 / m",4,&ProjectSettings::gridZ,0.1,10,false,nullptr},
        {"batch_max_time_s",L"单次批量试验时限 / s",4,&ProjectSettings::batchMaxTime,1,180,false,nullptr},
        {"trajectory_interval_s",L"批量轨迹导出间隔 / s",4,&ProjectSettings::trajectoryInterval,0.002,1,false,nullptr},
        {"gravity_m_s2",L"重力加速度 / m·s⁻²",5,&ProjectSettings::gravity,0.1,30,false,nullptr},
        {"time_step_s",L"物理时间步长 / s",5,&ProjectSettings::timeStep,0.00025,0.01,false,nullptr},
        {"single_max_time_s",L"单次演示时限 / s",5,&ProjectSettings::maxTime,1,300,false,nullptr},
        {"playback",L"播放倍速（不放大物理步长）",5,&ProjectSettings::playback,1,8,true,nullptr},
        {"trail_limit",L"轨迹显示点数上限",5,&ProjectSettings::trailLimit,0,10000,true,nullptr},
        {"chart_window_s",L"速度曲线时间窗口 / s",5,&ProjectSettings::chartWindow,1,180,false,nullptr},
        {"view_zoom",L"视图缩放倍率",5,&ProjectSettings::zoom,0.5,2,false,nullptr},
        {"rest_speed_m_s",L"平动与转动等效静止阈值 / m·s⁻¹",5,&ProjectSettings::restSpeed,0.001,0.1,false,nullptr},
        {"rest_time_s",L"低速持续多久进入静止 / s",5,&ProjectSettings::restTime,0.2,10,false,nullptr},
        {"settle_wait_s",L"双方静止后的结束等待 / s",5,&ProjectSettings::settleWait,0.2,10,false,nullptr}
    }; return fields;
}
const wchar_t* parameterPageName(int page) {
    static const wchar_t* names[]={L"石头与释放",L"地表与空气",L"山坡地形",L"弹性挡板",L"批量与统计",L"运行与显示"};
    return names[page];
}
DynamicArray<Point> parseTerrainPoints(const std::string& text) {
    std::string normalized=text;
    for(size_t i=0;i<normalized.size();++i) if(normalized[i]==';' || normalized[i]=='\r') normalized[i]='\n';
    std::istringstream lines(normalized); std::string line; DynamicArray<Point> out;
    while(std::getline(lines,line)) {
        if(line.find_first_not_of(" \t")==std::string::npos) continue;
        std::istringstream row(line); double x,z; std::string extra;
        if(!(row>>x>>z) || (row>>extra) || !std::isfinite(x+z) || x<0 || x>120 || z<0 || z>60)
            throw std::runtime_error("控制点每行填写 x z；x=0~120，z=0~60，不能附加其他文字。");
        if(out.size() && x<=out[out.size()-1].x) throw std::runtime_error("地形控制点的 x 必须严格递增。");
        out.push_back(Point(x,z));
        if(out.size()>100) throw std::runtime_error("地形控制点最多100个。");
    }
    if(out.size()<2 || out[out.size()-1].x-out[0].x<5) throw std::runtime_error("至少两个控制点，水平范围至少5m。");
    return out;
}
HillSlope projectHill(const ProjectSettings& s) {
    if(s.terrainMode==0) return HillSlope();
    if(s.terrainMode==2) return HillSlope(parseTerrainPoints(s.terrainPoints));
    TerrainSettings t; t.topHeight=s.topHeight; t.steepDeg=s.steepAngle; t.benchStart=s.benchStart;
    t.benchLength=s.benchLength; t.benchDeg=s.benchAngle; t.toeLength=s.toeLength;
    return HillSlope(buildTerrain(t));
}
double projectEndX(const HillSlope& h) { const auto& p=h.getPoints(); return p[p.size()-1].x; }
double projectStartX(const HillSlope& h) { return h.getPoints()[0].x; }
double projectCollectorX(const ProjectSettings& s,const HillSlope& h) {
    if(!s.collectorAuto) return s.collectorX;
    double end=projectEndX(h), offset=min(10.0,0.1*(end-projectStartX(h)));
    double x=end-offset;
    while(!s.heightReference && h.getHeight(x)>=s.analysisHeight && offset>0.1) { offset*=0.5; x=end-offset; }
    return x;
}
BarrierSettings projectBarrier(const ProjectSettings& s,const HillSlope& h,bool batch=false) {
    BarrierSettings b; b.enabled=batch?s.batchBarrier!=0:s.barrierEnabled!=0;
    b.x=s.barrierAuto?max(projectStartX(h)+2,min(projectEndX(h)-2,projectEndX(h)-12)):s.barrierX;
    b.height=s.barrierHeight; b.mass=s.barrierMass; b.stiffness=s.barrierStiffness; b.damping=s.barrierDamping;
    return b;
}
void applyProjectGlobals(const ProjectSettings& s) {
    g_slideFriction=s.friction; g_restitution=s.restitution; g_rollResist=s.rolling; g_dragCoef=s.drag;
    g_rockDensity=s.density; g_rockThickness=s.thickness; g_airDensity=s.airDensity; g_gravity=s.gravity;
    g_barrierFriction=s.barrierFriction; g_restSpeed=s.restSpeed; g_restTime=s.restTime;
}
void validateProjectSettings(const ProjectSettings& s) {
    for(const auto& f:parameterFields()) {
        double v=s.*(f.member);
        if(!std::isfinite(v) || v<f.lo || v>f.hi || (f.integer && floor(v)!=v))
            throw std::runtime_error(std::string("参数范围或整数要求不满足：")+f.key);
    }
    if(s.sidesMin>s.sidesMax) throw std::runtime_error("最小边数不能大于最大边数。");
    if(s.shapeMode==1) {
        if(s.sidesMin<12) throw std::runtime_error("近圆模式建议12~30边，最小边数不能小于12。");
        double r=sqrt(s.area/maxPolygonArea((int)s.sidesMin,1));
        if(r<0.05 || r>10) throw std::runtime_error("面积对应的近圆半径不在0.05~10m内。");
    } else validateShape(ShapeSettings{(int)s.sidesMin,(int)s.sidesMax,s.radius,s.area});
    HillSlope hill=projectHill(s); double start=projectStartX(hill),end=projectEndX(hill);
    if(s.releaseX<start || s.releaseX>end-0.1) throw std::runtime_error("单次释放位置必须位于当前地形内部。");
    if(s.batchRange && (s.batchXMin>s.batchXMax || s.batchXMin<start || s.batchXMax>=end))
        throw std::runtime_error("批量释放区间不在当前地形内；改地形后请同步调整释放区间。");
    BarrierSettings b=projectBarrier(s,hill); b.enabled=true; ElasticBarrier::validate(b,hill);
    double collector=projectCollectorX(s,hill);
    if(collector<=start || collector>=end) throw std::runtime_error("收集器必须处于当前地形内部。");
    if(!s.heightReference && hill.getHeight(collector)>=s.analysisHeight)
        throw std::runtime_error("收集器处地面高程不低于统计上限；请更换断面或高度基准。");
    if(ceil((end-start)/s.gridX)*ceil(s.analysisHeight/s.gridZ)>200000)
        throw std::runtime_error("概率网格过密，请增大网格尺寸。");
    if(s.timeStep*sqrt(s.barrierStiffness/s.barrierMass)/0.05>256)
        throw std::runtime_error("挡板过硬或过轻；请减小时间步、减小刚度或增大质量。");
}
PolygonShape projectShape(const ProjectSettings& s,std::mt19937& rng,double radius=-1,double area=-1) {
    double a=area<0?s.area:area, r=radius<0?s.radius:radius;
    if(s.shapeMode==1) {
        int k=std::uniform_int_distribution<int>((int)s.sidesMin,(int)s.sidesMax)(rng);
        r=sqrt(a/maxPolygonArea(k,1)); return generatePolygon(k,r,a,rng);
    }
    return generatePolygon(ShapeSettings{(int)s.sidesMin,(int)s.sidesMax,r,a},rng);
}
RollingStone projectStone(const ProjectSettings& s,const HillSlope& hill,const PolygonShape& shape,double x,double speed) {
    RollingStone stone(x,hill.getHeight(x),shape);
    stone.setInitialPose(s.initialAngle*POLY_PI/180); stone.adjustToGround(hill);
    stone.raise(s.releaseLift); stone.startAlongSlope(hill,speed); stone.setInitialSpin(s.initialSpin);
    stone.setTrailMax(0); return stone;
}
void saveProjectSettings(const ProjectSettings& s,const std::wstring& path) {
    std::ofstream out(path); out.imbue(std::locale::classic()); out<<std::setprecision(17);
    if(!out) throw std::runtime_error("无法写入参数方案文件。");
    out<<"# RollingStone settings v2; numeric SI units; UTF-8\n";
    for(const auto& f:parameterFields()) out<<f.key<<'='<<s.*(f.member)<<'\n';
    std::string points=s.terrainPoints;
    for(char& c:points) if(c=='\n' || c=='\r') c=';';
    out<<"terrain_points="<<points<<'\n'; out.flush();
    if(!out) throw std::runtime_error("参数方案写入失败。");
}
ProjectSettings loadProjectSettings(const std::wstring& path) {
    std::ifstream in(path); if(!in) throw std::runtime_error("无法读取参数方案文件。");
    ProjectSettings s; std::string line;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#') continue; size_t pos=line.find('=');
        if(pos==std::string::npos) throw std::runtime_error("参数方案格式错误，需要 key=value。");
        std::string key=line.substr(0,pos),value=line.substr(pos+1);
        if(key=="terrain_points") { s.terrainPoints=value; continue; }
        bool found=false;
        for(const auto& f:parameterFields()) if(key==f.key) {
            size_t used=0; double v=std::stod(value,&used);
            while(used<value.size() && isspace((unsigned char)value[used])) ++used;
            if(used!=value.size()) throw std::runtime_error("参数方案数值后包含无效字符。");
            s.*(f.member)=v; found=true; break;
        }
        if(!found) throw std::runtime_error(std::string("未知参数：")+key);
    }
    validateProjectSettings(s); return s;
}
std::wstring projectWide(const std::string& text) {
    if(text.empty()) return L"";
    int count=MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,nullptr,0);
    std::vector<wchar_t> buffer(count); MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,buffer.data(),count);
    return buffer.data();
}
std::string projectUtf8(const std::wstring& text) {
    if(text.empty()) return ""; int count=WideCharToMultiByte(CP_UTF8,0,text.c_str(),-1,nullptr,0,nullptr,nullptr);
    std::vector<char> buffer(count); WideCharToMultiByte(CP_UTF8,0,text.c_str(),-1,buffer.data(),count,nullptr,nullptr);
    return buffer.data();
}
#endif
