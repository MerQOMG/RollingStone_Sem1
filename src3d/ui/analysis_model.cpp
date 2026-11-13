#include "ui/analysis_model.h"
#include <algorithm>
#include <cstdio>
namespace rs3d::gui {
std::string formatPercent(double f){char b[32];if(f>=.9995||f==0)std::snprintf(b,sizeof(b),"%.0f%%",f*100);else if(f>=.1)std::snprintf(b,sizeof(b),"%.1f%%",f*100);else std::snprintf(b,sizeof(b),"%.2f%%",f*100);return b;}
std::string formatCount(std::uint64_t v){return std::to_string(v);}
std::string stopReasonZh(const std::string&s){
    if(s=="resting")return u8"静止";if(s=="downstream")return u8"越过下游边界";if(s=="upstream")return u8"退至上游边界";if(s=="lateral")return u8"侧向出界";
    if(s=="timeout")return u8"超时";if(s=="geometry_failure")return u8"几何失败";if(s=="numeric_failure")return u8"数值失败";if(s=="cancelled")return u8"已取消";
    if(s=="running")return u8"运行中";if(s=="not_started")return u8"未开始";return s;
}
std::string stopReasonZh(StopReason r){return stopReasonZh(std::string(stopReasonName(r)));}
std::string contactStatusZh(FirstContactStatus s){
    switch(s){case FirstContactStatus::notStarted:return u8"未开始";case FirstContactStatus::observing:return u8"观察中";case FirstContactStatus::hit:return u8"已触板";case FirstContactStatus::noHit:return u8"未触板";
    case FirstContactStatus::unresolved:return u8"未确认";case FirstContactStatus::disabled:return u8"未启用挡板";case FirstContactStatus::notRecorded:return u8"未记录";}return "?";
}
std::string batchStatusZh(BatchStatus s){switch(s){case BatchStatus::complete:return u8"完整";case BatchStatus::incompleteFailure:return u8"不完整（有失败或超时）";case BatchStatus::cancelled:return u8"已取消";case BatchStatus::ioError:return u8"写入错误";}return "?";}
std::string sessionStateZh(const SessionView&v){
    if(v.state==SessionState::cancelling)return u8"正在取消";
    if(v.state==SessionState::paused)return u8"计算已暂停";
    if(v.state==SessionState::running){
        if(v.phase==SessionPhase::generating)return u8"正在生成石块";
        if(v.phase==SessionPhase::preview)return u8"实时预览中";
        if(v.phase==SessionPhase::background)return u8"后台批量计算中";
        return u8"运行中";
    }
    if(v.state==SessionState::completed)return u8"已完成";
    if(v.state==SessionState::failed){
        if(v.result){if(v.result->status==BatchStatus::cancelled)return u8"已取消";if(v.result->status==BatchStatus::ioError)return u8"写入错误";if(v.result->status==BatchStatus::incompleteFailure)return u8"完成（结果不完整）";}
        return u8"未能完整完成";
    }
    return u8"就绪";
}
MapData buildMap(const RunConfig&config,const AnalysisSnapshot&a,MapKind kind,Metric metric,bool readError){
    MapData m;m.kind=kind;m.metric=metric;m.n=config.source.count;
    switch(kind){
    case MapKind::route:if(a.route){m.grid=a.route->grid();m.hits=&a.route->hits();m.known=a.route->completed();m.applicable=a.routeRecorded;}break;
    case MapKind::contact:if(a.contact){m.grid=a.contact->grid();m.hits=&a.contact->hits();m.known=a.contact->known();m.hitsTotal=a.contact->hitTrials();m.misses=a.contact->missTrials();m.applicable=a.contactRecorded&&a.contact->enabled();}break;
    case MapKind::toeXY:case MapKind::toeXZ:{
        const auto&g=config.grid;int axis=kind==MapKind::toeXY?1:2;
        m.grid=Grid3D({{g.lower.x,g.lower[axis],0},{g.upper.x,g.upper[axis],1},{g.cell.x,g.cell[axis],1}});
        m.hits=kind==MapKind::toeXY?&a.toeXY:&a.toeXZ;m.known=a.normalTrials;m.applicable=!a.ioError;break;}
    }
    m.conditional=kind==MapKind::contact&&metric==Metric::conditional;
    bool trustworthy=!a.ioError&&!readError;
    m.usable=m.applicable&&m.hits&&m.hits->size()==m.grid.count()&&(trustworthy||metric==Metric::count)&&(!m.conditional||(a.filesComplete&&m.known==m.n&&m.hitsTotal>0));
    if(metric==Metric::count&&m.hits&&!m.hits->empty())m.denominator=double(std::max<std::uint64_t>(1,*std::max_element(m.hits->begin(),m.hits->end())));
    else if(m.conditional)m.denominator=double(std::max<std::uint64_t>(1,m.hitsTotal));
    else m.denominator=double(std::max<std::uint64_t>(1,m.n));
    if(!trustworthy){m.level=Level::error;m.status=u8"读取或写入出错：概率已失效";m.detail=u8"次数仅供参考，不能当作概率使用。";}
    else if(!m.applicable){
        m.level=Level::warn;
        if(kind==MapKind::contact)m.status=config.barrier.enabled?u8"未记录首次触点":u8"未启用挡板";else m.status=u8"不适用或未记录";
        m.detail=kind==MapKind::contact&&!config.barrier.enabled?u8"在“场景 → 挡板”中启用挡板后重新批量计算。":u8"这份结果没有保存该项统计。";
    }
    else if(m.conditional){
        if(m.known==m.n&&m.hitsTotal){m.level=Level::ok;m.status=u8"完整：只统计已触板试验内的分布";m.detail=u8"分母是触板试验数 B，不是总试验数 N。";}
        else{m.level=Level::warn;m.status=u8"条件概率暂不适用";m.detail=u8"仍有未确认或未完成的试验，或没有任何触板。";}
    }
    else if(a.filesComplete&&m.known==m.n){m.level=Level::ok;m.status=u8"完整的经验频率";m.detail=u8"样本有限，不保证收敛；分辨率为 1/N。";}
    else{m.level=Level::warn;m.status=u8"累计下界（尚不完整）";m.detail=u8"上界按未完成试验全部命中计算，二者不是置信区间。";}
    if(metric==Metric::count)m.legend=u8"次数 0 – "+std::to_string(std::uint64_t(m.denominator));
    else if(m.conditional)m.legend=u8"条件概率 H / B：0 – 100%";
    else m.legend=u8"频率 H / N：0 – 100%";
    return m;
}
std::string cellReadout(const MapData&m,const AnalysisSnapshot&a,std::size_t cell){
    if(!m.hits||cell>=m.hits->size()||cell>=m.grid.count())return {};
    auto idx=m.grid.indices(cell);auto&gx=m.grid.lines(0);auto&gy=m.grid.lines(1);
    char head[160];std::snprintf(head,sizeof(head),m.kind==MapKind::toeXZ?"x %.1f–%.1f m, z %.1f–%.1f m":(m.kind==MapKind::contact?u8"宽向 %.2f–%.2f m, 高向 %.2f–%.2f m":"x %.1f–%.1f m, y %.1f–%.1f m"),gx[idx[0]],gx[idx[0]+1],gy[idx[1]],gy[idx[1]+1]);
    auto h=(*m.hits)[cell];std::string out=head;out+=u8" · 命中 "+std::to_string(h)+u8" 次";
    if(m.metric==Metric::count||!m.usable)return out;
    char buf[160];
    if(m.conditional){if(m.hitsTotal)std::snprintf(buf,sizeof(buf),u8" · H/B = %s",formatPercent(double(h)/m.hitsTotal).c_str());else buf[0]=0;return out+buf;}
    double lo=m.n?double(h)/m.n:0,hi=m.n?double(h+m.n-m.known)/m.n:1;
    if(m.known==m.n)std::snprintf(buf,sizeof(buf),u8" · H/N = %s",formatPercent(lo).c_str());
    else std::snprintf(buf,sizeof(buf),u8" · 下界 %s，上界 %s",formatPercent(lo).c_str(),formatPercent(hi).c_str());
    (void)a;return out+buf;
}
ErrorHint explainError(const std::string&t){
    auto has=[&](const char*s){return t.find(s)!=std::string::npos;};
    if(has("release x/y range outside terrain"))return {u8"释放位置的 x/y 范围超出了地形。请缩小释放区间，或增大地形长度/宽度。",{"initial.x","initial.y"}};
    if(has("stable_face fixture"))return {u8"“稳定面”姿态只能用于水平平地：请选择“平地”，并把平面斜率、横向坡度、沟槽幅度都设为 0。",{"initial.orientation","terrain.type"}};
    if(has("invalid range:")){auto p=t.find("invalid range:");auto name=t.substr(p+14);while(!name.empty()&&name[0]==' ')name.erase(0,1);return {u8"区间的最小值不能大于最大值（"+name+u8"）。",{"initial."+name}};}
    if(has("negative material coefficient"))return {u8"摩擦、阻力、滚阻系数不能为负。",{"initial.mu","initial.Cd","initial.c_rr"}};
    if(has("restitution above one"))return {u8"恢复系数不能大于 1。",{"initial.e"}};
    if(has("initial velocity component exceeds solver range"))return {u8"初速度或初始自旋超出求解器范围（绝对值需小于 5000）。",{"initial.vx","initial.vy","initial.vz","initial.omega_x","initial.omega_y","initial.omega_z"}};
    if(has("production k must be at least 10"))return {u8"顶点数 k 至少为 10。",{"geometry.k_min"}};
    if(has("run count outside"))return {u8"批次数 N 必须在 1 到 1000000 之间。",{"run.count"}};
    if(has("invalid density/air/gap/time limit"))return {u8"密度需大于 0，空气密度与释放间隙不能为负，单次时间上限需在 (0, 60] 秒内。",{"material.density","material.air_density","initial.gap","run.max_time"}};
    if(has("invalid barrier dimensions/material"))return {u8"挡板尺寸必须为正，摩擦系数不能为负，恢复系数需在 0–1 之间。",{"barrier.width","barrier.height","barrier.thickness","barrier.mu","barrier.e"}};
    if(has("invalid analysis cell size"))return {u8"分析格宽必须大于 0。",{"analysis.route_cell","analysis.contact_cell"}};
    if(has("GUI batch requires N >= 2"))return {u8"批量计算至少需要 2 次试验；只想算一次请用“单次试验”。",{"run.count"}};
    auto colon=t.find(": ");
    if(colon!=std::string::npos&&t.find(' ')>colon&&t.substr(0,colon).find('.')!=std::string::npos){
        auto key=t.substr(0,colon);auto rest=t.substr(colon+2);
        if(rest.find("invalid finite number")!=std::string::npos)return {u8"这里需要一个有限的数字。",{key}};
        if(rest.find("invalid unsigned integer")!=std::string::npos||rest.find("integer exceeds")!=std::string::npos)return {u8"这里需要一个非负整数（且不超过资源上限）。",{key}};
        return {u8"数值不被接受（"+rest+u8"）。",{key}};
    }
    return {u8"配置未通过检查：",{}};
}
}
