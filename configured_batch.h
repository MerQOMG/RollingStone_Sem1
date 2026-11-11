#ifndef ROCK_CONFIGURED_BATCH_H
#define ROCK_CONFIGURED_BATCH_H
#include <shellapi.h>

std::string uniqueProjectRun(const char* kind) {
    if(!CreateDirectoryA("results",nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) throw std::runtime_error("无法创建 results 文件夹。");
    SYSTEMTIME now; GetLocalTime(&now); static unsigned counter=0; char path[180];
    std::snprintf(path,sizeof(path),"results/%s_%04u%02u%02u_%02u%02u%02u_%03u_%lu_%u",kind,
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,(unsigned long)GetTickCount(),++counter);
    if(!CreateDirectoryA(path,nullptr)) throw std::runtime_error("无法创建本次试验的独立输出文件夹。");
    return path;
}
struct ConfiguredBatchReport { int completed=0,failed=0,entered=0,collected=0,contacted=0,blocked=0,passed=0,reachedCollector=0,timedOut=0,stopped=0,reachedEnd=0; bool cancelled=false; };
double projectHeight(const ProjectSettings& s,const HillSlope& hill,double x,double z) {
    return s.heightReference?z-hill.getHeight(x):z;
}
ConfiguredBatchReport runConfiguredBatch(const ProjectSettings& settings,const std::string& directory,
    const std::function<bool(int,int)>& progress=std::function<bool(int,int)>()) {
    validateProjectSettings(settings); PhysicsParameterGuard guard; applyProjectGlobals(settings);
    HillSlope hill=projectHill(settings); double start=projectStartX(hill),end=projectEndX(hill);
    double collector=projectCollectorX(settings,hill),toeStart=start;
    if(!settings.heightReference) {
        toeStart=end;
        for(double x=start;x<=end;x+=0.02) if(hill.getHeight(x)<=settings.analysisHeight) { toeStart=x; break; }
        toeStart=min(toeStart,end-0.001);
    }
    int nx=max(1,(int)ceil((end-toeStart)/settings.gridX)),nz=max(1,(int)ceil(settings.analysisHeight/settings.gridZ));
    std::vector<int> hits(nx*nz,0),profile(nx,0),impacts(nz,0);
    auto open=[&](const char* name) { std::ofstream out(directory+"/"+name); out.imbue(std::locale::classic());
        out<<std::setprecision(15); if(!out) throw std::runtime_error("无法写入批量 CSV。"); return out; };
    std::ofstream trials=open("batch_trials.csv"),grid=open("route_probability_grid.csv"),height=open("impact_height_at_toe.csv"),
        traj=open("trajectories_toe.csv"),summary=open("batch_summary.csv"),prof=open("route_profile_x.csv"),shapes=open("shapes.csv");
    saveProjectSettings(settings,projectWide(directory+"/settings.ini"));
    trials<<"trial,x0,v0,radius_m,area_m2,sides,mu,e,cd,crr,final_x_m,final_z_m,final_speed_m_s,entered_toe,reached_collector,collector_height_m,collected_in_range,contacted,passed,blocked,max_abs_deflection_m,peak_support_force_N,damper_loss_J,numerical_loss_J,contact_loss_J,time_s,failed,clipped,max_speed_m_s,max_height_m,max_kinetic_J,final_kinetic_J,path_length_m,timed_out,stopped,reached_end,error\n";
    traj<<"trial,time_s,x_m,z_m,height_m,speed_m_s,barrier_q_m,barrier_v_m_s,support_force_N\n";
    shapes<<"trial,vertex,local_x_m,local_z_m,mass_kg,inertia_kg_m2\n";
    std::mt19937 rng((unsigned int)settings.seed); ConfiguredBatchReport report;
    std::uniform_real_distribution<double> release(settings.batchXMin,settings.batchXMax);
    for(int trial=0;trial<(int)settings.batchCount;++trial) {
        if(progress && !progress(trial,(int)settings.batchCount)) { report.cancelled=true; break; }
        bool clipped=false;
        auto sample=[&](double mean,bool random,double lo,double hi) {
            double raw=mean;
            if(random && settings.cv>0 && mean!=0) raw=std::normal_distribution<double>(mean,fabs(mean)*settings.cv)(rng);
            double value=max(lo,min(hi,raw)); if(value!=raw) clipped=true; return value;
        };
        double x0=settings.batchRange?release(rng):settings.releaseX;
        double v0=sample(settings.batchSpeed,settings.randomSpeed!=0,0,100);
        double radius=settings.shapeMode?settings.radius:sample(settings.radius,settings.randomGeometry!=0,0.05,10);
        double area=sample(settings.area,settings.randomGeometry!=0,0.000001,314);
        if(!settings.shapeMode) {
            double cap=maxPolygonArea((int)settings.sidesMin,radius),low=1.01e-4*radius*radius;
            double adjusted=max(low,min(cap,area)); if(adjusted!=area) clipped=true; area=adjusted;
        } else {
            double lo=maxPolygonArea((int)settings.sidesMax,0.05),hi=maxPolygonArea((int)settings.sidesMin,10);
            double adjusted=max(lo,min(hi,area)); if(adjusted!=area) clipped=true; area=adjusted;
        }
        double mu=sample(settings.friction,settings.randomSurface!=0,0,1.5);
        double e=sample(settings.restitution,settings.randomSurface!=0,0,0.95);
        double cd=sample(settings.drag,settings.randomSurface!=0,0,3);
        double crr=sample(settings.rolling,settings.randomSurface!=0,0,0.3);
        g_slideFriction=mu; g_restitution=e; g_dragCoef=cd; g_rollResist=crr;
        std::vector<unsigned char> visited(nx*nz,0),visitedColumns(nx,0);
        bool entered=false,reached=false,collected=false,failed=false,wasCancelled=false;
        double collectorHeight=-1,t=0,nextSample=0,stable=0,finalX=x0,finalZ=hill.getHeight(x0),finalSpeed=v0;
        int sides=0; std::string error="ok";
        double maxSpeed=v0,maxHeight=finalZ,maxKinetic=0,finalKinetic=0,pathLength=0;
        bool timedOut=false,stopped=false,reachedEnd=false;
        ElasticBarrier barrier; barrier.reset(hill,projectBarrier(settings,hill,true)); bool blocked=false;
        try {
            PolygonShape shape=projectShape(settings,rng,radius,area); radius=shape.radius; area=shape.area; sides=(int)shape.local.size();
            for(size_t v=0;v<shape.local.size();++v) shapes<<trial<<','<<v<<','<<shape.local[v].x<<','<<shape.local[v].z<<','<<shape.mass<<','<<shape.inertia<<'\n';
            RollingStone stone=projectStone(settings,hill,shape,x0,v0); double previousX=stone.getX(),previousZ=stone.getZ();
            maxHeight=stone.getZ();maxKinetic=stone.getEnergy();
            auto mark=[&](double x,double z) {
                double h=projectHeight(settings,hill,x,z);
                if(x<toeStart || x>end || h<0 || h>settings.analysisHeight) return;
                entered=true; int c=min(nx-1,(int)((x-toeStart)/settings.gridX)); int r=min(nz-1,(int)(h/settings.gridZ));
                visited[r*nx+c]=1; visitedColumns[c]=1;
            };
            mark(previousX,previousZ);
            int maxSteps=(int)ceil(settings.batchMaxTime/settings.timeStep);
            for(int step=0;step<maxSteps;++step) {
                if(step%250==0 && progress && !progress(trial,(int)settings.batchCount)) { wasCancelled=true; break; }
                double dt=min(settings.timeStep,settings.batchMaxTime-t);
                stone.update(hill,dt,&barrier); t+=dt;
                double x=stone.getX(),z=stone.getZ();
                maxSpeed=max(maxSpeed,stone.getSpeed());maxHeight=max(maxHeight,z);maxKinetic=max(maxKinetic,stone.getEnergy());
                pathLength+=sqrt((x-previousX)*(x-previousX)+(z-previousZ)*(z-previousZ));
                if(!reached && previousX<collector && x>=collector) {
                    double f=(collector-previousX)/(x-previousX),zz=previousZ+f*(z-previousZ);
                    collectorHeight=projectHeight(settings,hill,collector,zz); reached=true;
                    collected=collectorHeight>=0 && collectorHeight<=settings.analysisHeight;
                }
                // 对相邻质心位置之间的线段细分，降低大步长时跳过小网格的风险。
                int pieces=max(1,(int)ceil(2*max(fabs(x-previousX)/settings.gridX,fabs(z-previousZ)/settings.gridZ)));
                for(int j=1;j<=pieces;++j) { double f=(double)j/pieces; mark(previousX+f*(x-previousX),previousZ+f*(z-previousZ)); }
                if(t+1e-10>=nextSample) {
                    traj<<trial<<','<<t<<','<<x<<','<<z<<','<<projectHeight(settings,hill,x,z)<<','<<stone.getSpeed()<<','
                        <<barrier.displacement()<<','<<barrier.speed()<<','<<barrier.supportForce()<<'\n';
                    nextSample=t+settings.trajectoryInterval;
                }
                previousX=x; previousZ=z;
                stable=(!stone.isMoving() && barrier.settled())?stable+dt:0;
                if(stable>=settings.settleWait) break;
                double boundary=max(end-0.5,barrier.enabled()?barrier.faceX()+shape.extent+1:end-0.5);
                if(x>=boundary) { reachedEnd=true;break; }
            }
            finalX=stone.getX(); finalZ=stone.getZ(); finalSpeed=stone.getSpeed();
            finalKinetic=stone.getEnergy();stopped=stable>=settings.settleWait;
            timedOut=t>=settings.batchMaxTime-1e-9 && !stopped && !reachedEnd;
            blocked=barrier.enabled() && barrier.hasContact() && !barrier.hasPassed() && stable>=settings.settleWait;
        } catch(const std::exception& ex) { failed=true; error=ex.what(); }
        if(wasCancelled) { report.cancelled=true; break; }
        ++report.completed;
        if(failed) ++report.failed;
        else {
            if(entered) ++report.entered; if(collected) ++report.collected;if(reached) ++report.reachedCollector;
            if(timedOut) ++report.timedOut;if(stopped) ++report.stopped;if(reachedEnd) ++report.reachedEnd;
            if(barrier.hasContact()) ++report.contacted; if(barrier.hasPassed()) ++report.passed; if(blocked) ++report.blocked;
            for(size_t i=0;i<hits.size();++i) hits[i]+=visited[i];
            for(int i=0;i<nx;++i) profile[i]+=visitedColumns[i];
            if(collected) ++impacts[min(nz-1,(int)(collectorHeight/settings.gridZ))];
        }
        for(char& c:error) if(c==',' || c=='\n' || c=='\r' || c=='"') c=' ';
        trials<<trial<<','<<x0<<','<<v0<<','<<radius<<','<<area<<','<<sides<<','<<mu<<','<<e<<','<<cd<<','<<crr<<','
            <<finalX<<','<<finalZ<<','<<finalSpeed<<','<<entered<<','<<reached<<','<<collectorHeight<<','<<collected<<','
            <<barrier.hasContact()<<','<<barrier.hasPassed()<<','<<blocked<<','<<barrier.maxDisplacement()<<','<<barrier.peakSupportForce()<<','
            <<barrier.dampingEnergy()<<','<<barrier.numericalEnergy()<<','<<barrier.impactEnergy()<<','<<t<<','<<failed<<','<<clipped<<','
            <<maxSpeed<<','<<maxHeight<<','<<maxKinetic<<','<<finalKinetic<<','<<pathLength<<','<<timedOut<<','<<stopped<<','<<reachedEnd<<','<<error<<'\n';
    }
    auto probability=[&](int count,int denominator) { return denominator?(double)count/denominator:0.0; };
    grid<<"x_min,x_max,z_min,z_max,center_x,center_z,ground_z,hit_trials,p_all,p_entered\n";
    for(int row=0;row<nz;++row) for(int col=0;col<nx;++col) {
        double x1=toeStart+col*settings.gridX,x2=min(end,x1+settings.gridX),z1=row*settings.gridZ,z2=min(settings.analysisHeight,z1+settings.gridZ);
        int count=hits[row*nx+col];
        grid<<x1<<','<<x2<<','<<z1<<','<<z2<<','<<(x1+x2)/2<<','<<(z1+z2)/2<<','<<hill.getHeight((x1+x2)/2)<<','
            <<count<<','<<probability(count,report.completed)<<','<<probability(count,report.entered)<<'\n';
    }
    prof<<"x_min,x_max,hit_trials,p_all,p_entered\n";
    for(int col=0;col<nx;++col) prof<<toeStart+col*settings.gridX<<','<<min(end,toeStart+(col+1)*settings.gridX)<<','<<profile[col]<<','
        <<probability(profile[col],report.completed)<<','<<probability(profile[col],report.entered)<<'\n';
    height<<"z_min,z_max,hit_trials,p_all,p_collected\n";
    for(int row=0;row<nz;++row) height<<row*settings.gridZ<<','<<min(settings.analysisHeight,(row+1)*settings.gridZ)<<','<<impacts[row]<<','
        <<probability(impacts[row],report.completed)<<','<<probability(impacts[row],report.collected)<<'\n';
    summary<<"n_requested,n_completed,n_failed,n_entered,n_collected,p_collected,n_contacted,n_blocked,n_passed,cancelled,seed,collector_x_m,analysis_height_m,height_reference,dt_s,n_reached_collector,n_timed_out,n_stopped,n_reached_end\n"
        <<settings.batchCount<<','<<report.completed<<','<<report.failed<<','<<report.entered<<','<<report.collected<<','
        <<probability(report.collected,report.completed)<<','<<report.contacted<<','<<report.blocked<<','<<report.passed<<','<<report.cancelled<<','
        <<settings.seed<<','<<collector<<','<<settings.analysisHeight<<','<<settings.heightReference<<','<<settings.timeStep<<','
        <<report.reachedCollector<<','<<report.timedOut<<','<<report.stopped<<','<<report.reachedEnd<<'\n';
    trials.flush();grid.flush();height.flush();traj.flush();summary.flush();prof.flush();shapes.flush();
    if(!trials || !grid || !height || !traj || !summary || !prof || !shapes) throw std::runtime_error("批量文件写入失败，请检查磁盘空间。");
    if(progress) progress(report.completed,(int)settings.batchCount);
    return report;
}

int projectSettingsSelfTest() {
    try {
        ProjectSettings s; validateProjectSettings(s); PhysicsParameterGuard guard;
        s.shapeMode=1; s.sidesMin=s.sidesMax=30; s.releaseX=2; s.density=3000; s.thickness=2;
        applyProjectGlobals(s); std::mt19937 rng(2026); PolygonShape p=projectShape(s,rng);
        requireTest(fabs(p.mass-18000)<1e-7,"density/thickness propagate to mass");
        requireTest(fabs(p.area-3)<1e-10,"round mode preserves specified area");
        double minimum=1e9,maximum=0;
        for(const auto& v:p.local) { double r=sqrt(dot2(v,v)); minimum=min(minimum,r); maximum=max(maximum,r); }
        requireTest(maximum-minimum<1e-10 && p.local.size()==30,"round vertices lie evenly on circle");
        HillSlope hill=projectHill(s); RollingStone stone=projectStone(s,hill,p,2,6);
        requireTest(fabs(stone.getSpeed()-6)<1e-10,"initial speed setting applied");
        ProjectSettings invalid=s; invalid.sidesMin=3; bool rejected=false;
        try { validateProjectSettings(invalid); } catch(const std::exception&) { rejected=true; }
        requireTest(rejected,"incompatible round shape rejected");
        s.terrainMode=1; hill=projectHill(s);
        requireTest(projectCollectorX(s,hill)<projectEndX(hill),"collector follows current terrain");
        s.terrainMode=2; s.terrainPoints="0 20;20 10;40 0;60 0"; s.batchXMin=20;s.batchXMax=30;
        validateProjectSettings(s); hill=projectHill(s);
        requireTest(fabs(hill.getHeight(10)-15)<1e-12,"custom points parsed and interpolated");
        std::printf("PASS settings: field ranges, round geometry, density/thickness/mass, release speed, current terrain collector, custom control points.\n");
        return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL settings: %s\n",e.what()); return 1; }
}
#endif
