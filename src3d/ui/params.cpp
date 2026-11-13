#include "ui/params.h"
#include "ui/analysis_model.h"
#include <charconv>
#include <cmath>
#include <sstream>
namespace rs3d::gui {
namespace {
constexpr double inf=std::numeric_limits<double>::infinity();
FieldDef F(const char*section,const char*group,const char*key,const char*label,const char*unit,Kind kind,const char*help,const char*when="",double lo=-inf,double hi=inf,bool loOpen=false){
    FieldDef d;d.section=section;d.group=group;d.key=key;d.label=label;d.unit=unit;d.kind=kind;d.help=help;d.when=when;d.lo=lo;d.hi=hi;d.loOpen=loOpen;return d;
}
FieldDef C(const char*section,const char*group,const char*key,const char*label,const char*help,std::vector<std::string> labels,std::vector<std::string> tokens,const char*when=""){
    FieldDef d=F(section,group,key,label,"",Kind::choice,help,when);d.labels=std::move(labels);d.tokens=std::move(tokens);return d;
}
std::vector<FieldDef> build(){
    std::vector<FieldDef> v;
    // ---- terrain
    v.push_back(C("terrain","","terrain.type",u8"地形类型",u8"参数化山坡由陡坡、平台和坡脚组成；平地用于对照与基准测试。",{u8"参数化山坡",u8"平地"},{"parameterized","flat"}));
    v.push_back(F("terrain",u8"山坡剖面","terrain.height",u8"坡顶高度","m",Kind::number,u8"剖面最高点相对坡脚的高度。","terrain.type=parameterized",0,inf,true));
    v.push_back(F("terrain",u8"山坡剖面","terrain.steep_degrees",u8"陡坡角度",u8"°",Kind::number,u8"坡顶到平台之间的陡坡与水平面的夹角。","terrain.type=parameterized",0,89.9));
    v.push_back(F("terrain",u8"山坡剖面","terrain.bench_start",u8"平台起点 x","m",Kind::number,u8"平台开始处沿下坡方向的位置。","terrain.type=parameterized"));
    v.push_back(F("terrain",u8"山坡剖面","terrain.bench_length",u8"平台长度","m",Kind::number,u8"中部平台沿下坡方向的长度。","terrain.type=parameterized",0,inf));
    v.push_back(F("terrain",u8"山坡剖面","terrain.bench_degrees",u8"平台坡度",u8"°",Kind::number,u8"平台与水平面的夹角。","terrain.type=parameterized"));
    v.push_back(F("terrain",u8"山坡剖面","terrain.toe_length",u8"坡脚延伸长度","m",Kind::number,u8"坡脚之后继续向下游延伸的平地长度。","terrain.type=parameterized",0,inf));
    v.push_back(F("terrain",u8"平地","terrain.flat_length",u8"平面长度","m",Kind::number,u8"平地沿下坡方向的长度。","terrain.type=flat",0,inf,true));
    v.push_back(F("terrain",u8"平地","terrain.flat_height",u8"平面高度","m",Kind::number,u8"平地的基准高度。","terrain.type=flat"));
    v.push_back(F("terrain",u8"平地","terrain.flat_slope_x",u8"沿 x 斜率","",Kind::number,u8"高度对 x 的变化率（0 为水平）。","terrain.type=flat"));
    v.push_back(F("terrain",u8"通用","terrain.width",u8"地形宽度","m",Kind::number,u8"沿 y（横向）的总宽度。","",0,inf,true));
    v.push_back(F("terrain",u8"通用","terrain.cross_slope_degrees",u8"横向坡度",u8"°",Kind::number,u8"沿 y 方向的倾斜角。"));
    v.push_back(F("terrain",u8"通用","terrain.groove_amplitude",u8"沟槽幅度","m",Kind::number,u8"沿下坡方向的沟槽起伏幅度，0 表示没有沟槽。","",0,inf));
    v.push_back(F("terrain",u8"通用","terrain.cell_size",u8"网格边长","m",Kind::number,u8"地形三角网格的单元边长；越小越精细、越慢。","",0,inf,true));
    v.push_back(F("terrain",u8"通用","terrain.flip_diagonals",u8"翻转网格对角线","",Kind::flag,u8"改变每个方格的对角线方向，用于网格敏感性对照。"));
    // ---- rock and release
    v.push_back(F("rock",u8"石块形状","geometry.k_min",u8"最少顶点数 k","",Kind::count,u8"随机凸多面体的最少顶点数（10–100）。","",10,100));
    v.push_back(F("rock",u8"石块形状","geometry.k_max",u8"最多顶点数 k","",Kind::count,u8"随机凸多面体的最多顶点数（10–100）。","",10,100));
    v.push_back(F("rock",u8"石块形状","geometry.r",u8"外包半径 r","m",Kind::number,u8"质心到最远顶点的距离。","",0,inf,true));
    v.push_back(F("rock",u8"石块形状","geometry.V",u8"体积 V",u8"m³",Kind::number,u8"石块实体体积。","",0,inf,true));
    v.push_back(F("rock",u8"石块形状","material.density",u8"密度",u8"kg/m³",Kind::number,u8"石块材料密度。","",0,inf,true));
    v.push_back(C("rock",u8"释放","initial.orientation",u8"初始姿态",u8"“随机姿态”每次试验独立采样；“稳定面”仅用于水平平地的基准测试。",{u8"随机姿态",u8"稳定面"},{"sampled","stable_face"}));
    v.push_back(F("rock",u8"释放","initial.gap",u8"释放间隙","m",Kind::number,u8"石块最低点距地面的初始高度。","",0,inf));
    v.push_back(F("rock",u8"释放位置","initial.x",u8"释放位置 x","m",Kind::range,u8"沿下坡方向的释放位置；填入最小值与最大值则每次均匀随机。"));
    v.push_back(F("rock",u8"释放位置","initial.y",u8"释放位置 y","m",Kind::range,u8"横向释放位置。"));
    v.push_back(F("rock",u8"初速度","initial.vx",u8"速度 vx","m/s",Kind::range,u8"沿下坡方向的初速度。","",-4999,4999));
    v.push_back(F("rock",u8"初速度","initial.vy",u8"速度 vy","m/s",Kind::range,u8"横向初速度。","",-4999,4999));
    v.push_back(F("rock",u8"初速度","initial.vz",u8"速度 vz","m/s",Kind::range,u8"竖向初速度（向上为正）。","",-4999,4999));
    v.push_back(F("rock",u8"初始自旋","initial.omega_x",u8"自旋 ωx","rad/s",Kind::range,u8"绕 x 轴的初始角速度。","",-4999,4999));
    v.push_back(F("rock",u8"初始自旋","initial.omega_y",u8"自旋 ωy","rad/s",Kind::range,u8"绕 y 轴的初始角速度。","",-4999,4999));
    v.push_back(F("rock",u8"初始自旋","initial.omega_z",u8"自旋 ωz","rad/s",Kind::range,u8"绕 z 轴的初始角速度。","",-4999,4999));
    // ---- contact
    v.push_back(F("contact",u8"地面接触","initial.mu",u8"摩擦系数 μ","",Kind::range,u8"石块与地面的库仑摩擦系数。","",0,inf));
    v.push_back(F("contact",u8"地面接触","initial.e",u8"恢复系数 e","",Kind::range,u8"碰撞恢复系数，范围 0–1。","",0,1));
    v.push_back(F("contact",u8"阻力","initial.Cd",u8"空气阻力系数 Cd","",Kind::range,u8"空气阻力系数。","",0,inf));
    v.push_back(F("contact",u8"阻力","initial.c_rr",u8"滚动阻力系数 c_rr","",Kind::range,u8"接触滚动阻力系数。","",0,inf));
    v.push_back(F("contact",u8"阻力","material.air_density",u8"空气密度",u8"kg/m³",Kind::number,u8"空气密度，0 表示忽略空气阻力。","",0,inf));
    // ---- barrier
    v.push_back(F("barrier","","barrier.enabled",u8"启用挡板","",Kind::flag,u8"在坡脚附近放置有限宽度的挡板，并记录首次触板。"));
    v.push_back(C("barrier","","barrier.mode",u8"挡板模式",u8"刚性：固定不动；弹性：挡板通过弹簧和阻尼支承，会真实地位移和回弹。",{u8"刚性",u8"弹性"},{"rigid","elastic"},"barrier.enabled=true"));
    v.push_back(F("barrier",u8"位置与朝向","barrier.center_x",u8"中心 x","m",Kind::autoNumber,u8"沿下坡方向的位置；“自动”放在坡脚前 10 m。","barrier.enabled=true"));
    v.push_back(F("barrier",u8"位置与朝向","barrier.center_y",u8"中心 y","m",Kind::number,u8"横向位置。","barrier.enabled=true"));
    v.push_back(F("barrier",u8"位置与朝向","barrier.center_z",u8"中心 z","m",Kind::autoNumber,u8"高度；“自动”让挡板底面落在地面上。","barrier.enabled=true"));
    v.push_back(F("barrier",u8"位置与朝向","barrier.yaw_degrees",u8"绕 z 方位角",u8"°",Kind::number,u8"挡板绕竖直轴的旋转。","barrier.enabled=true"));
    v.push_back(F("barrier",u8"尺寸","barrier.width",u8"宽度","m",Kind::number,u8"沿横向的有限宽度。","barrier.enabled=true",0,inf,true));
    v.push_back(F("barrier",u8"尺寸","barrier.height",u8"高度","m",Kind::number,u8"挡板高度。","barrier.enabled=true",0,inf,true));
    v.push_back(F("barrier",u8"尺寸","barrier.thickness",u8"厚度","m",Kind::number,u8"挡板厚度。","barrier.enabled=true",0,inf,true));
    v.push_back(F("barrier",u8"接触","barrier.mu",u8"摩擦系数 μ","",Kind::number,u8"挡板摩擦系数。","barrier.enabled=true",0,inf));
    v.push_back(F("barrier",u8"接触","barrier.e",u8"恢复系数 e","",Kind::number,u8"刚性接触恢复系数（0–1）。","barrier.enabled=true",0,1));
    v.push_back(F("barrier",u8"弹性支承","barrier.mass",u8"等效质量 M","kg",Kind::number,u8"挡板及其支承的等效质量。","barrier.mode=elastic",0,inf,true));
    v.push_back(F("barrier",u8"弹性支承","barrier.stiffness",u8"弹簧刚度 k","N/m",Kind::number,u8"支承弹簧刚度。","barrier.mode=elastic",0,inf));
    v.push_back(F("barrier",u8"弹性支承","barrier.damping",u8"支承阻尼 c",u8"N·s/m",Kind::number,u8"支承阻尼系数。","barrier.mode=elastic",0,inf));
    v.push_back(F("barrier",u8"弹性支承","barrier.initial_displacement",u8"初始位移 q","m",Kind::number,u8"挡板在释放前的初始位移。","barrier.mode=elastic"));
    v.push_back(F("barrier",u8"弹性支承","barrier.initial_velocity",u8"初始速度","m/s",Kind::number,u8"挡板的初始速度。","barrier.mode=elastic"));
    // ---- obstacles
    v.push_back(F("obstacles","","obstacles.enabled",u8"启用固定障碍","",Kind::flag,u8"在地形上放置固定的随机碎石盒体；同一批试验使用同一布局。"));
    v.push_back(F("obstacles",u8"数量与随机性","obstacles.count",u8"数量","",Kind::count,u8"障碍数量（0–500）。","obstacles.enabled=true",0,500));
    v.push_back(F("obstacles",u8"数量与随机性","obstacles.seed",u8"布局种子","",Kind::count,u8"障碍布局的独立随机种子。","obstacles.enabled=true"));
    v.push_back(C("obstacles",u8"尺寸","obstacles.size_mode",u8"尺寸单位",u8"相对：尺寸按石块外包半径 r 的倍数；绝对：单位为米。",{u8"相对半径 r",u8"绝对尺寸 m"},{"relative_to_r","absolute_m"},"obstacles.enabled=true"));
    v.push_back(F("obstacles",u8"尺寸","obstacles.length",u8"长度区间","",Kind::range,u8"盒体长度的随机区间。","obstacles.enabled=true",0,inf,true));
    v.push_back(F("obstacles",u8"尺寸","obstacles.width",u8"宽度区间","",Kind::range,u8"盒体宽度的随机区间。","obstacles.enabled=true",0,inf,true));
    v.push_back(F("obstacles",u8"尺寸","obstacles.exposed_height",u8"露出高度区间","",Kind::range,u8"盒体高出地面的随机区间。","obstacles.enabled=true",0,inf,true));
    v.push_back(F("obstacles",u8"尺寸","obstacles.max_total_height",u8"总高度上限","",Kind::number,u8"限制障碍总体高度。","obstacles.enabled=true",0,inf,true));
    v.push_back(C("obstacles",u8"布置","obstacles.placement",u8"布置范围",u8"全地形：覆盖整个地形；旧走廊：沿旧版释放走廊布置。",{u8"全地形",u8"旧走廊"},{"terrain","legacy_corridor"},"obstacles.enabled=true"));
    v.push_back(F("obstacles",u8"布置","obstacles.automatic_region",u8"自动范围","",Kind::flag,u8"由地形与释放区间自动确定布置范围。","obstacles.enabled=true"));
    v.push_back(F("obstacles",u8"布置","obstacles.x",u8"自定义范围 x","m",Kind::range,u8"自定义布置范围（沿下坡）。","obstacles.enabled=true;obstacles.automatic_region=false"));
    v.push_back(F("obstacles",u8"布置","obstacles.y",u8"自定义范围 y","m",Kind::range,u8"自定义布置范围（横向）。","obstacles.enabled=true;obstacles.automatic_region=false"));
    v.push_back(F("obstacles",u8"布置","obstacles.min_spacing",u8"足迹最小间距","m",Kind::number,u8"相邻障碍足迹之间的最小间距。","obstacles.enabled=true",0,inf));
    v.push_back(F("obstacles",u8"布置","obstacles.embed_depth",u8"底面埋深","m",Kind::number,u8"盒体埋入地面的深度。","obstacles.enabled=true",0,inf));
    v.push_back(F("obstacles",u8"接触","obstacles.mu",u8"摩擦系数 μ","",Kind::number,u8"障碍的摩擦系数。","obstacles.enabled=true",0,inf));
    v.push_back(F("obstacles",u8"接触","obstacles.e",u8"恢复系数 e","",Kind::number,u8"障碍的恢复系数（0–1）。","obstacles.enabled=true",0,1));
    // ---- statistics
    v.push_back(C("stats",u8"统计区域","statistics.height_reference",u8"高度基准",u8"统计盒高度的零点：坡脚中心高度或坡脚最低点高度。",{u8"坡脚中心",u8"坡脚最低点"},{"toe_center","toe_min"}));
    v.push_back(F("stats",u8"统计区域","statistics.automatic_box",u8"自动统计盒","",Kind::flag,u8"取坡脚前最多 40 m、全宽、基准以上 10 m 的区域。"));
    for(const char*axis:{"x","y","z"}){
        std::string a=axis;
        v.push_back(F("stats",u8"统计盒（手动）",(std::string("statistics.lower_")+a).c_str(),(u8"下界 "+a).c_str(),"m",Kind::number,u8"统计盒下界。","statistics.automatic_box=false"));
        v.push_back(F("stats",u8"统计盒（手动）",(std::string("statistics.upper_")+a).c_str(),(u8"上界 "+a).c_str(),"m",Kind::number,u8"统计盒上界。","statistics.automatic_box=false"));
        v.push_back(F("stats",u8"统计盒（手动）",(std::string("statistics.cell_")+a).c_str(),(u8"格宽 "+a).c_str(),"m",Kind::number,u8"体素格宽。","statistics.automatic_box=false",0,inf,true));
    }
    v.push_back(F("stats",u8"统计区域","statistics.collector_x",u8"收集截面 x","m",Kind::autoNumber,u8"统计越过该截面的石块；“自动”取坡脚前 10 m。"));
    v.push_back(F("stats",u8"分析格宽","analysis.route_cell",u8"全路径热图格宽","m",Kind::number,u8"全路径频率图的格宽。","",0,inf,true));
    v.push_back(F("stats",u8"分析格宽","analysis.contact_cell",u8"首次触板图格宽","m",Kind::number,u8"挡板表面首次触点图的格宽。","",0,inf,true));
    // ---- output and history
    v.push_back(F("output","","output.trajectory_stride",u8"轨迹 CSV 步间隔","步",Kind::count,u8"轨迹文件每隔多少步写一行，0 关闭轨迹行；统计始终逐步计算。","",0,1000000));
    v.push_back(F("output","","history.enabled",u8"记录逐帧历史","",Kind::flag,u8"保存每次试验的运动历史，用于回放；关闭后只能看统计。"));
    v.push_back(F("output","","history.interval",u8"历史时间间隔","s",Kind::autoNumber,u8"“自动”按试验数与时长自动选择。","history.enabled=true"));
    v.push_back(F("output","","history.cache_mib",u8"回放缓存上限","MiB",Kind::count,u8"回放读取时的内存缓存上限。","history.enabled=true"));
    v.push_back(F("output","","history.display_vertices",u8"全部轨迹显示顶点预算","",Kind::count,u8"全部轨迹同时显示时的顶点总预算。","history.enabled=true"));
    // ---- run (shown in the run panel)
    v.push_back(F("run","","run.count",u8"批次数 N","",Kind::count,u8"蒙特卡洛试验次数。批量计算至少需要 2 次。","",1,1000000));
    v.push_back(F("run","","run.seed",u8"批次种子","",Kind::count,u8"决定整批试验的随机采样；相同种子得到相同结果。"));
    v.push_back(F("run","","preview.seed",u8"预览种子","",Kind::count,u8"决定“预览石块”和单次试验；不会被正式批次占用。"));
    v.push_back(F("run","","run.max_time",u8"单次时间上限","s",Kind::number,u8"每次试验的最长模拟时间（不超过 60 s）；超时的试验使批次结果不完整。","",0,60,true));
    return v;
}
bool parseDouble(const std::string&s,double&v){
    if(s.empty())return false;char*end=nullptr;std::string t=s;v=std::strtod(t.c_str(),&end);return end&&*end==0&&std::isfinite(v);
}
bool parseUnsigned(const std::string&s){if(s.empty()||s.size()>20)return false;for(char c:s)if(c<'0'||c>'9')return false;return true;}
}
const std::vector<FieldDef>&fieldDefs(){static const auto defs=build();return defs;}
const std::vector<SectionDef>&sectionDefs(){
    static const std::vector<SectionDef> s{
        {"terrain",u8"地形",u8"坡面剖面、宽度与网格"},
        {"rock",u8"石块与释放",u8"形状、释放位置、初速度与自旋"},
        {"contact",u8"接触与阻力",u8"摩擦、恢复系数与空气阻力"},
        {"barrier",u8"挡板",u8"刚性或弹性挡板，记录首次触板"},
        {"obstacles",u8"固定障碍",u8"同批共用的随机碎石盒体"},
        {"stats",u8"统计与分析",u8"统计盒、收集截面与热图格宽"},
        {"output",u8"输出与记录",u8"轨迹文件、回放历史与保存位置"}};
    return s;
}
const FieldDef*findDef(const std::string&key){for(auto&d:fieldDefs())if(d.key==key)return &d;return nullptr;}
Val Form::split(const FieldDef&d,const std::string&text){
    Val v;if(d.kind==Kind::range){auto comma=text.find(',');if(comma==text.npos)v.a=v.b=text;else{v.a=text.substr(0,comma);v.b=text.substr(comma+1);}}else v.a=text;return v;
}
std::string Form::compose(const FieldDef&d,const Val&v){
    if(d.kind==Kind::range)return v.a==v.b?v.a:v.a+","+v.b;
    return v.a;
}
Form::Form(const SceneDraft&d):editor_(d){load(d);}
void Form::load(const SceneDraft&d){
    editor_.sync(d);now_.clear();applied_.clear();errors_.clear();formError_.clear();applyError_.clear();
    for(auto&f:editor_.fields){auto*def=findDef(f.key);if(!def)continue;now_[f.key]=split(*def,f.text.data());}
    applied_=now_;outputRoot=appliedOutputRoot=d.outputRoot;
}
void Form::rebase(const SceneDraft&d){
    SceneEditor fresh(d);auto keepNow=now_;
    for(auto&f:fresh.fields){auto*def=findDef(f.key);if(!def)continue;auto v=split(*def,f.text.data());bool wasPending=pending(f.key);applied_[f.key]=v;if(!wasPending)now_[f.key]=v;}
    (void)keepNow;appliedOutputRoot=d.outputRoot;if(outputRoot==appliedOutputRoot)outputRoot=d.outputRoot;
}
void Form::setResolved(const std::string&key,double v){char b[48];std::snprintf(b,sizeof(b),"%.6g",v);resolved_[key]=b;}
bool Form::pending(const std::string&key)const{auto it=now_.find(key);auto ia=applied_.find(key);if(it==now_.end()||ia==applied_.end())return false;return it->second.a!=ia->second.a||it->second.b!=ia->second.b;}
int Form::pendingCount(const std::string&section)const{int n=0;for(auto&d:fieldDefs())if(d.section==section&&pending(d.key))++n;if(section=="output"&&pendingOutput())++n;return n;}
int Form::pendingTotal()const{int n=0;for(auto&d:fieldDefs())if(pending(d.key))++n;if(pendingOutput())++n;return n;}
bool Form::is(const std::string&key,const std::string&token)const{auto it=now_.find(key);return it!=now_.end()&&it->second.a==token;}
bool Form::visible(const FieldDef&d)const{
    if(d.when.empty())return true;std::stringstream ss(d.when);std::string cond;
    while(std::getline(ss,cond,';')){
        bool neg=!cond.empty()&&cond[0]=='!';if(neg)cond.erase(0,1);auto eq=cond.find('=');if(eq==cond.npos)continue;
        auto key=cond.substr(0,eq),vals=cond.substr(eq+1);std::stringstream vs(vals);std::string one;bool match=false;while(std::getline(vs,one,'|'))if(is(key,one))match=true;
        if(match==neg)return false;
    }
    return true;
}
std::string Form::localCheck(const FieldDef&d,const Val&v)const{
    auto checkNumber=[&](const std::string&s,bool allowAuto)->std::string{
        if(allowAuto&&s=="auto")return {};
        double x;if(!parseDouble(s,x))return u8"请输入数字（可带小数点或科学计数法）。";
        if(x<d.lo||(d.loOpen&&x==d.lo)){char b[96];std::snprintf(b,sizeof(b),d.loOpen?u8"必须大于 %g。":u8"不能小于 %g。",d.lo);return b;}
        if(x>d.hi){char b[96];std::snprintf(b,sizeof(b),u8"不能大于 %g。",d.hi);return b;}
        return {};
    };
    switch(d.kind){
    case Kind::number:return checkNumber(v.a,false);
    case Kind::autoNumber:return checkNumber(v.a,true);
    case Kind::integer:case Kind::count:{
        if(!parseUnsigned(v.a))return u8"请输入非负整数。";
        double x=std::strtod(v.a.c_str(),nullptr);if(x<d.lo){char b[96];std::snprintf(b,sizeof(b),u8"不能小于 %g。",d.lo);return b;}if(x>d.hi){char b[96];std::snprintf(b,sizeof(b),u8"不能大于 %g。",d.hi);return b;}return {};}
    case Kind::range:{
        auto ea=checkNumber(v.a,false);if(!ea.empty())return u8"最小值："+ea;auto eb=checkNumber(v.b,false);if(!eb.empty())return u8"最大值："+eb;
        double a=std::strtod(v.a.c_str(),nullptr),b=std::strtod(v.b.c_str(),nullptr);if(a>b)return u8"最小值不能大于最大值。";return {};}
    default:return {};
    }
}
void Form::validate(){
    errors_.clear();formError_.clear();
    for(auto&d:fieldDefs()){if(!visible(d))continue;auto it=now_.find(d.key);if(it==now_.end())continue;auto e=localCheck(d,it->second);if(!e.empty())errors_[d.key]=e;}
    if(!errors_.empty())return;
    try{for(auto&d:fieldDefs()){auto it=now_.find(d.key);if(it!=now_.end())editor_.set(d.key,compose(d,it->second));}editor_.parse();}
    catch(const std::exception&e){
        auto hint=explainError(e.what());bool known=!hint.keys.empty()||hint.message!=u8"配置未通过检查：";bool attributed=false;
        for(auto&k:hint.keys){auto*d=findDef(k);if(d&&visible(*d)){errors_[k]=hint.message;attributed=true;}}
        if(attributed)formError_.clear();else formError_=known?hint.message:std::string(u8"配置未通过检查：")+e.what();
        if(attributed&&errors_.empty())formError_=hint.message;
    }
}
void Form::revert(){now_=applied_;outputRoot=appliedOutputRoot;errors_.clear();formError_.clear();applyError_.clear();}
void Form::revertSection(const std::string&section){for(auto&d:fieldDefs())if(d.section==section)now_[d.key]=applied_[d.key];if(section=="output")outputRoot=appliedOutputRoot;}
SceneDraft Form::draft(){
    for(auto&d:fieldDefs()){auto it=now_.find(d.key);if(it!=now_.end())editor_.set(d.key,compose(d,it->second));}
    auto draft=editor_.parse();draft.outputRoot=outputRoot;return draft;
}
}
