#include "editor.h"
#include <sstream>
#include <cstring>
#include <map>
#include <charconv>
namespace rs3d {
namespace {
std::map<std::string,std::string> values(const SceneDraft&draft){std::map<std::string,std::string>result;std::istringstream in(sceneText(draft));std::string line,section;while(std::getline(in,line)){if(line.empty())continue;if(line[0]=='['){section=line.substr(1,line.size()-2);continue;}auto eq=line.find('=');if(eq!=line.npos)result[section.empty()?line.substr(0,eq):section+'.'+line.substr(0,eq)]=line.substr(eq+1);}return result;}
}
SceneEditor::SceneEditor(const SceneDraft&draft){
    auto add=[&](int group,const char*key,const char*label){fields.push_back({key,label,group,{}});};
    add(0,"geometry.k_min",u8"最少顶点 k (10–100)");add(0,"geometry.k_max",u8"最多顶点 k (10–100)");add(0,"geometry.r",u8"外包半径 r (m)");add(0,"geometry.V",u8"实体体积 V (m^3)");add(0,"run.count",u8"批次数 N (至少 2)");add(0,"run.seed",u8"批次种子 seed");add(0,"preview.seed",u8"独立预览种子");add(0,"material.density",u8"密度 (kg/m^3)");add(0,"initial.gap",u8"释放间隙 (m)");add(0,"initial.orientation",u8"姿态 sampled / stable_face");
    add(1,"initial.x",u8"释放 x (m)");add(1,"initial.y",u8"释放 y (m)");add(1,"initial.vx",u8"初速度 vx (m/s)");add(1,"initial.vy",u8"初速度 vy (m/s)");add(1,"initial.vz",u8"初速度 vz (m/s)");add(1,"initial.omega_x",u8"初始自旋 x (rad/s)");add(1,"initial.omega_y",u8"初始自旋 y (rad/s)");add(1,"initial.omega_z",u8"初始自旋 z (rad/s)");add(1,"initial.mu",u8"地面摩擦系数");add(1,"initial.e",u8"地面恢复系数 (0–1)");add(1,"initial.Cd",u8"空气阻力 Cd");add(1,"initial.c_rr",u8"接触滚阻 c_rr");add(1,"material.air_density",u8"空气密度 (kg/m^3)");
    add(2,"terrain.type",u8"地形 parameterized / flat");add(2,"terrain.height",u8"坡顶高度 (m)");add(2,"terrain.steep_degrees",u8"陡坡角度 (deg)");add(2,"terrain.bench_start",u8"平台起点 x (m)");add(2,"terrain.bench_length",u8"平台长度 (m)");add(2,"terrain.bench_degrees",u8"平台坡度 (deg)");add(2,"terrain.toe_length",u8"坡脚延伸长度 (m)");add(2,"terrain.width",u8"地形宽度 (m)");add(2,"terrain.cross_slope_degrees",u8"横向坡度 (deg)");add(2,"terrain.groove_amplitude",u8"沟槽幅度 (m)");add(2,"terrain.cell_size",u8"地形网格边长 (m)");add(2,"terrain.flat_length",u8"平面长度 (m)");add(2,"terrain.flat_height",u8"平面高度 (m)");add(2,"terrain.flat_slope_x",u8"平面 x 斜率");add(2,"terrain.flip_diagonals",u8"翻转对角线 true/false");
    add(3,"barrier.enabled",u8"启用挡板 true/false");add(3,"barrier.mode",u8"挡板模式 rigid / elastic");add(3,"barrier.mass",u8"挡板等效质量 M (kg)");add(3,"barrier.stiffness",u8"弹簧刚度 k (N/m)");add(3,"barrier.damping",u8"支承阻尼 c (N*s/m)");add(3,"barrier.center_x",u8"挡板中心 x (m / auto)");add(3,"barrier.center_y",u8"挡板中心 y (m)");add(3,"barrier.center_z",u8"挡板中心 z (m / auto)");add(3,"barrier.width",u8"有限宽度 (m)");add(3,"barrier.height",u8"挡板高度 (m)");add(3,"barrier.thickness",u8"挡板厚度 (m)");add(3,"barrier.yaw_degrees",u8"绕 Z 方位角 (deg)");add(3,"barrier.mu",u8"挡板摩擦系数");add(3,"barrier.e",u8"刚性接触恢复系数 (0–1)");
    add(3,"barrier.initial_displacement",u8"初始挡板位移 q (m)");add(3,"barrier.initial_velocity",u8"初始挡板速度 (m/s)");
    add(4,"statistics.height_reference",u8"高度基准 toe_center/toe_min");add(4,"statistics.automatic_box",u8"自动统计盒 true/false");
    for(int i=0;i<3;++i){std::string a(1,"xyz"[i]);add(4,("statistics.lower_"+a).c_str(),(u8"统计下界 "+a+" (m)").c_str());add(4,("statistics.upper_"+a).c_str(),(u8"统计上界 "+a+" (m)").c_str());add(4,("statistics.cell_"+a).c_str(),(u8"体素宽度 "+a+" (m)").c_str());}
    add(4,"statistics.collector_x",u8"收集截面 x (m / auto)");add(4,"run.max_time",u8"单次模拟上限 (s, <=60)");add(4,"output.trajectory_stride",u8"轨迹输出步间隔 (0=关闭)");
    add(4,"history.enabled",u8"分块历史 true/false");add(4,"history.interval",u8"历史间隔 (s / auto)");add(4,"history.cache_mib",u8"历史缓存上限 (MiB)");add(4,"history.display_vertices",u8"全部轨迹显示顶点预算");add(4,"analysis.route_cell",u8"全路径格宽 (m)");add(4,"analysis.contact_cell",u8"首次触板格宽高 (m)");
    add(5,"obstacles.enabled",u8"启用障碍 true/false");add(5,"obstacles.count",u8"障碍数量 (0–500)");add(5,"obstacles.seed",u8"障碍独立种子");add(5,"obstacles.size_mode",u8"relative_to_r / absolute_m");
    add(5,"obstacles.length",u8"长度区间 (r倍数 / m)");add(5,"obstacles.width",u8"宽度区间 (r倍数 / m)");add(5,"obstacles.exposed_height",u8"地面以上高度 (r倍数 / m)");add(5,"obstacles.max_total_height",u8"总高上限 (r倍数 / m)");
    add(5,"obstacles.automatic_region",u8"自动走廊 true/false");add(5,"obstacles.x",u8"自定义区域 x (m)");add(5,"obstacles.y",u8"自定义区域 y (m)");add(5,"obstacles.min_spacing",u8"足迹最小间距 (m)");add(5,"obstacles.embed_depth",u8"底面埋深 (m)");add(5,"obstacles.mu",u8"障碍接触摩擦系数");add(5,"obstacles.e",u8"障碍恢复系数 (0–1)");sync(draft);
}
void SceneEditor::set(const std::string&key,const std::string&value){for(auto&field:fields)if(field.key==key){if(value.size()>=field.text.size())throw std::runtime_error("editor value too long");field.text.fill(0);std::copy(value.begin(),value.end(),field.text.begin());return;}throw std::runtime_error("unknown editor field");}
void SceneEditor::sync(const SceneDraft&draft){auto all=values(draft);for(auto&field:fields)set(field.key,all.at(field.key));// Shortest round-tripping decimal for the editor, while CSV/config retain their
    // canonical 17-digit representation. Seeds/integers must never pass through double.
    for(auto&field:fields){std::string value=field.text.data(),shorter;size_t start=0;bool valid=true;
        while(start<value.size()){auto end=value.find(',',start);if(end==value.npos)end=value.size();auto part=value.substr(start,end-start);
            if(part.find_first_of(".eE")!=part.npos){double number=0;auto parsed=std::from_chars(part.data(),part.data()+part.size(),number);char buffer[64];auto written=std::to_chars(buffer,buffer+sizeof(buffer),number);
                if(parsed.ec!=std::errc{}||parsed.ptr!=part.data()+part.size()||written.ec!=std::errc{}){valid=false;break;}part.assign(buffer,written.ptr);}
            if(!shorter.empty())shorter+=',';shorter+=part;start=end+1;
        }if(valid)set(field.key,shorter);
    }base_=draft;}
SceneDraft SceneEditor::parse()const{
    auto all=values(base_);for(auto&field:fields)all[field.key]=field.text.data();std::ostringstream out;out<<"schema="<<all.at("schema")<<'\n';std::string section;
    for(auto&entry:all){auto dot=entry.first.find('.');if(dot==entry.first.npos)continue;auto next=entry.first.substr(0,dot);if(next!=section){section=next;out<<'['<<section<<"]\n";}out<<entry.first.substr(dot+1)<<'='<<entry.second<<'\n';}return parseScene(out.str());
}
std::string SceneEditor::glyphs()const{std::string result;for(auto&field:fields)result+=field.label;return result;}
}
