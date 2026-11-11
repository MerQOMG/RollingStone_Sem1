#ifndef BATCH_STATS_H
#define BATCH_STATS_H

// ============================================================================
//  任务 1、2：批量蒙特卡洛模拟 + CSV 统计（凸多边形版本）
//  ---------------------------------------------------------------------------
//  用法（不打开图形窗口）：
//      rollingstone.exe --batch 200 2026
//      rollingstone.exe --batch 500 0      // 种子 0 表示用当前时间
//
//  每次模拟随机抽样（变异系数 CV = 0.1）：
//      x0   ~ U(90, 100) m              释放点（坡脚上方 30° 陡坡段）
//      v0   ~ N(10.0, 1.0) m/s         顺坡初速度（模拟已经加速的滚石）
//      r    ~ N(2.0, 0.20) m            外接圆半径
//      S    ~ N(3.0, 0.30) m^2          截面积，夹到 [1e-4 r^2, 0.95*maxArea(3,r)]
//      mu   ~ N(0.45, 0.045)            摩擦系数
//      e    ~ N(0.45, 0.045)            恢复系数
//      Cd   ~ N(1.0, 0.10)              空气阻力系数
//      c_rr ~ N(0.02, 0.002)            滚动阻力系数
//      边数 k 在 [3, 6] 内均匀随机，顶点由 generatePolygon 的定面积采样生成。
//
//  坡脚分析区：地面高程不超过坡脚高程+10 m（x 从 10 m 等高线到坡脚），1 m x 1 m 网格。
//      p_all     = 经过该格的模拟次数 / 总次数
//      p_entered = 经过该格的模拟次数 / 进入坡脚区的次数
//
//  输出（results/ 下每次独立目录，只有完整且成功写盘后才移除 .partial 后缀）：
//      batch_trials.csv, route_probability_grid.csv, route_profile_x.csv,
//      impact_height_at_toe.csv, trajectories_toe.csv, batch_summary.csv
//
//  说明：本头文件必须在 HillSlope / PolygonShape / RollingStone 定义之后、
//        main 之前包含；它复用主程序的物理模型和全局参数。
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <random>
#include <stdexcept>
#include <vector>
#include <cstdarg>
#include <climits>
#include <sstream>

static double batchClamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static double batchMean(double sum, int n) {
    return (n > 0) ? sum / (double)n : 0.0;
}

static double batchStd(double sum, double sum2, int n) {
    if (n <= 1) return 0.0;
    double mean = sum / (double)n;
    double var = (sum2 - (double)n * mean * mean) / (double)(n - 1);
    if (var < 0.0) var = 0.0;
    return sqrt(var);
}

static void batchCsvText(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < cap; ++i) {
        char c = src[i];
        if (c == ',' || c == '"' || c == '\n' || c == '\r') c = ' ';
        dst[j++] = c;
    }
    dst[j] = 0;
}


// 严格解析十进制整数，不接受负号、尾随字符、溢出或静默截断。
static bool batchParseUnsigned(const char* text, unsigned long long maximum, unsigned long long& value) {
    if(!text || !*text) return false;
    value=0;
    for(const char* c=text;*c;++c) {
        if(*c<'0' || *c>'9') return false;
        unsigned int digit=(unsigned int)(*c-'0');
        if(digit>maximum || value>(maximum-digit)/10) return false;
        value=value*10+digit;
    }
    return true;
}

class BatchIoError : public std::runtime_error {
public: explicit BatchIoError(const char* message) : std::runtime_error(message) {}
};
static void batchPrint(FILE* file,const char* format,...) {
    va_list args; va_start(args,format);
    int result=std::vfprintf(file,format,args); va_end(args);
    if(result<0 || std::ferror(file)) throw BatchIoError("CSV write failed; output remains incomplete.");
}
class BatchOutputFiles {
    std::vector<FILE*> files;
    std::string partialPath,completePath;
public:
    BatchOutputFiles() {
        if(!CreateDirectoryA("results",nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
            throw BatchIoError("Cannot create results directory.");
        SYSTEMTIME now; GetLocalTime(&now);
        bool created=false;
        for(unsigned int attempt=0;attempt<1000;++attempt) {
            char name[200];
            std::snprintf(name,sizeof(name),"results/batch_%04u%02u%02u_%02u%02u%02u_%03u_%lu_%u",
                now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,
                (unsigned long)GetCurrentProcessId(),attempt);
            completePath=name; partialPath=completePath+".partial";
            if(GetFileAttributesA(completePath.c_str())!=INVALID_FILE_ATTRIBUTES) continue;
            if(CreateDirectoryA(partialPath.c_str(),nullptr)) { created=true; break; }
            if(GetLastError()!=ERROR_ALREADY_EXISTS) break;
        }
        if(!created) throw BatchIoError("Cannot create a unique batch output directory.");
        std::printf("Output directory (incomplete until published): %s\n",partialPath.c_str());
    }
    ~BatchOutputFiles() { for(size_t i=0;i<files.size();++i) if(files[i]) std::fclose(files[i]); }
    FILE* open(const char* name) {
        FILE* file=std::fopen((partialPath+"/"+name).c_str(),"w");
        if(!file) throw BatchIoError("Cannot create a CSV file; existing runs were not overwritten.");
        files.push_back(file); return file;
    }
    std::string finish(bool complete) {
        bool ok=true;
        for(size_t i=0;i<files.size();++i) {
            FILE* file=files[i]; files[i]=nullptr;
            if(std::fflush(file)!=0 || std::ferror(file)) ok=false;
            if(std::fclose(file)!=0) ok=false;
        }
        if(!ok) throw BatchIoError("CSV flush/close failed; output remains incomplete.");
        if(complete && !MoveFileA(partialPath.c_str(),completePath.c_str()))
            throw BatchIoError("Cannot publish output directory; output remains incomplete.");
        return complete?completePath:partialPath;
    }
};
static std::string batchProbability(int hits,int denominator,bool complete) {
    if(!complete || denominator<=0) return "";
    std::ostringstream text; text.imbue(std::locale::classic());
    text<<std::setprecision(17)<<(double)hits/denominator;
    return text.str();
}
// 半开网格 [min,max)，最外侧上边界归最后一格；不把内部顶点瞬间触碰算成额外经过。
// 分段中点覆盖每一段实际经过的格子，包含跨越统计带的线段及初始静止点。
template<typename Visit>
static void batchVisitSegment(double x0,double z0,double x1,double z1,
    double xmin,double xmax,double zmin,double zmax,double cellX,double cellZ,Visit visit) {
    const int cols=(int)ceil((xmax-xmin)/cellX-1e-9);
    const int rows=(int)ceil((zmax-zmin)/cellZ-1e-9);
    auto point=[&](double f,double entry) {
        double x=x0+f*(x1-x0),z=z0+f*(z1-z0);
        if(x<xmin || x>xmax || z<zmin || z>zmax) return;
        int col=std::min(cols-1,(int)floor((x-xmin)/cellX));
        int row=std::min(rows-1,(int)floor((z-zmin)/cellZ));
        if(col>=0 && row>=0) visit(row*cols+col,entry);
    };
    if(x0==x1 && z0==z1) { point(0,0); return; }
    std::vector<double> cuts{0,1};
    if(x1!=x0) for(int i=0;i<=cols;++i) {
        double f=(std::min(xmax,xmin+i*cellX)-x0)/(x1-x0);
        if(f>0 && f<1) cuts.push_back(f);
    }
    if(z1!=z0) for(int i=0;i<=rows;++i) {
        double f=(std::min(zmax,zmin+i*cellZ)-z0)/(z1-z0);
        if(f>0 && f<1) cuts.push_back(f);
    }
    std::sort(cuts.begin(),cuts.end());
    for(size_t i=1;i<cuts.size();++i) if(cuts[i]>cuts[i-1])
        point((cuts[i]+cuts[i-1])/2,cuts[i-1]);
}
static double batchToeStart(const HillSlope& hill,double ceiling) {
    const auto& pts=hill.getPoints();
    for(size_t i=0;i<pts.size();++i) {
        if(pts[i].z<=ceiling) return pts[i].x;
        if(i+1<pts.size() && pts[i+1].z<=ceiling)
            return pts[i].x+(pts[i+1].x-pts[i].x)*(pts[i].z-ceiling)/(pts[i].z-pts[i+1].z);
    }
    return hill.getEndX();
}
struct BatchTracePoint { double t,x,z,speed; };

static int runBatchSimulationImpl(int nTrials, unsigned int seed) {
    if(nTrials<2 || nTrials>20000) throw std::invalid_argument("Trial count must be 2..20000.");
    PhysicsParameterGuard restorePhysics;
    if (seed == 0) seed = (unsigned int)std::time(nullptr);

    const double TOE_HEIGHT = 10.0;
    const double CELL_X = 1.0;
    const double CELL_Z = 1.0;
    const double MAX_BATCH_TIME = 30.0;
    const double TRAJ_SAMPLE_DT = 0.05;

    HillSlope hill;
    const double endX = (double)hill.getEndX();
    const double collectorX = endX - 10.0;   // 坡脚平台上的虚拟收集器 x=110 m

    const double toeZ=hill.getHeight(hill.getValleyX());
    const double toeStartX=batchToeStart(hill,toeZ+TOE_HEIGHT);
    double toeWidth = endX - toeStartX;
    if (toeWidth < CELL_X) toeWidth = CELL_X;
    int nCols = (int)ceil(toeWidth / CELL_X - 1e-9);
    int nRows = (int)ceil(TOE_HEIGHT / CELL_Z - 1e-9);
    if (nCols < 1) nCols = 1;
    if (nRows < 1) nRows = 1;
    int nCells = nCols * nRows;

    std::vector<int> cellHits(nCells, 0), cellStamp(nCells, -1);
    std::vector<int> colHits(nCols, 0), colStamp(nCols, -1);
    std::vector<int> impactHits(nRows, 0);

    BatchOutputFiles outputs;
    FILE* fpTrials=outputs.open("batch_trials.csv");
    FILE* fpGrid=outputs.open("route_probability_grid.csv");
    FILE* fpProfile=outputs.open("route_profile_x.csv");
    FILE* fpImpact=outputs.open("impact_height_at_toe.csv");
    FILE* fpTraj=outputs.open("trajectories_toe.csv");
    FILE* fpSummary=outputs.open("batch_summary.csv");

    batchPrint(fpTrials,
        "trial,x0,ground_z0,v0,radius,area,sides,mu,e,cd,crr,"
        "final_x,final_z,final_speed,max_speed,max_height,sim_time,"
        "entered_toe,reached_collector,collector_time,collector_z,"
        "collected_in_10m,reached_end,stopped,timed_out,failed,error,"
        "path_length,toe_entry_time\n");
    batchPrint(fpGrid,
        "x_min,x_max,z_min,z_max,center_x,center_z,ground_z,"
        "hit_trials,p_all,p_entered,run_status,total_trials\n");
    batchPrint(fpProfile,
        "x_min,x_max,center_x,ground_z,hit_trials,p_all,p_entered,run_status,total_trials\n");
    batchPrint(fpImpact,
        "z_min,z_max,center_z,hit_trials,p_all,p_collected_0_10m,run_status,total_trials\n");
    batchPrint(fpTraj, "trial,time,x,z,speed\n");

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> ux(90.0, 100.0);
    std::normal_distribution<double> nV0(10.0, 1.0);
    std::normal_distribution<double> nR(2.0, 0.20);
    std::normal_distribution<double> nArea(3.0, 0.30);
    std::normal_distribution<double> nMu(0.45, 0.045);
    std::normal_distribution<double> nE(0.45, 0.045);
    std::normal_distribution<double> nCd(1.0, 0.10);
    std::normal_distribution<double> nCrr(0.02, 0.002);

    int nEntered = 0, nCollector = 0, nCollected10 = 0;
    int nReachedEnd = 0, nStopped = 0, nTimeout = 0, nFailed = 0, nSuccess = 0;
    double sumFX = 0, sumFX2 = 0, sumFZ = 0, sumFZ2 = 0;
    double sumMaxV = 0, sumMaxV2 = 0, sumPath = 0, sumPath2 = 0;
    double sumTime = 0, sumTime2 = 0;

    std::printf("Running %d Monte-Carlo trials (seed=%u)...\n", nTrials, seed);
    std::printf("Toe region: x in [%.2f, %.2f] m, z in [%.2f, %.2f] m\n",
                toeStartX, endX, toeZ, toeZ+TOE_HEIGHT);

    for (int trial = 0; trial < nTrials; ++trial) {
        double x0 = ux(rng);
        double groundZ0 = hill.getHeight(x0);
        double v0 = batchClamp(nV0(rng), 0.0, 20.0);
        double radius = batchClamp(nR(rng), 0.5, 3.0);
        double mu = batchClamp(nMu(rng), 0.05, 1.0);
        double e = batchClamp(nE(rng), 0.0, 0.9);
        double cd = batchClamp(nCd(rng), 0.0, 2.0);
        double crr = batchClamp(nCrr(rng), 0.0, 0.3);

        const int kMin = 3, kMax = 6;
        double maxArea = maxPolygonArea(kMin, radius);
        double areaLow = std::max(1.01e-4 * radius * radius, 0.01);
        double areaHigh = 0.95 * maxArea;
        if (areaHigh < areaLow) areaHigh = areaLow;
        double area = batchClamp(nArea(rng), areaLow, areaHigh);

        g_slideFriction = mu;
        g_restitution = e;
        g_dragCoef = cd;
        g_rollResist = crr;

        bool failed = false;
        char errorMsg[160] = "";
        int sides = 0;
        double finalX = x0, finalZ = groundZ0, finalSpeed = v0;
        double maxSpeed = v0, maxHeight = groundZ0;
        double t = 0.0, pathLength = 0.0, toeEntryTime = -1.0;
        double collectorTime = -1.0, collectorZ = -1.0;
        bool enteredToe = false, reachedCollector = false, collectedIn10m = false;
        bool reachedEnd = false, stopped = false, timedOut = false;
        int impactBin = -1;
        std::vector<BatchTracePoint> trialTrace;
        auto markCell=[&](int idx,double entryTime) {
            if(!enteredToe) { enteredToe=true; toeEntryTime=entryTime; ++nEntered; }
            int col=idx%nCols;
            if(cellStamp[idx]!=trial) { cellStamp[idx]=trial; ++cellHits[idx]; }
            if(colStamp[col]!=trial) { colStamp[col]=trial; ++colHits[col]; }
        };

        try {
            ShapeSettings ss{ kMin, kMax, radius, area };
            PolygonShape shape = generatePolygon(ss, rng);
            sides = (int)shape.local.size();
            area = shape.area;

            RollingStone stone(x0, groundZ0, shape);
            stone.adjustToGround(hill);
            stone.startAlongSlope(hill, v0);
            stone.setTrailMax(0);

            finalX = stone.getX();
            finalZ = stone.getZ();
            finalSpeed = stone.getSpeed();
            maxSpeed = finalSpeed;
            maxHeight = finalZ;

            double prevX = stone.getX();
            double prevZ = stone.getZ();
            double nextTrajTime = 0.0;
            batchVisitSegment(prevX,prevZ,prevX,prevZ,toeStartX,endX,toeZ,toeZ+TOE_HEIGHT,CELL_X,CELL_Z,
                [&](int idx,double){markCell(idx,0);});
            if(enteredToe) { trialTrace.push_back(BatchTracePoint{0,prevX,prevZ,finalSpeed}); nextTrajTime=TRAJ_SAMPLE_DT; }

            while (t < MAX_BATCH_TIME && stone.isMoving()) {
                const double h=std::min(DT,MAX_BATCH_TIME-t);
                stone.update(hill, h);
                t += h;

                double x = stone.getX();
                double z = stone.getZ();
                double sp = stone.getSpeed();
                double dxs = x - prevX;
                double dzs = z - prevZ;
                pathLength += sqrt(dxs * dxs + dzs * dzs);
                if (sp > maxSpeed) maxSpeed = sp;
                if (z > maxHeight) maxHeight = z;

                if (!reachedCollector && prevX < collectorX && x >= collectorX) {
                    double denom = x - prevX;
                    double frac = (denom > 1e-12) ? (collectorX - prevX) / denom : 1.0;
                    if (frac < 0.0) frac = 0.0;
                    if (frac > 1.0) frac = 1.0;
                    collectorZ = prevZ + frac * (z - prevZ);
                    collectorTime = (t - h) + frac * h;
                    reachedCollector = true;
                    ++nCollector;
                    if (collectorZ >= toeZ && collectorZ <= toeZ+TOE_HEIGHT) {
                        collectedIn10m = true;
                        ++nCollected10;
                        impactBin = (int)((collectorZ-toeZ) / CELL_Z);
                        if (impactBin < 0) impactBin = 0;
                        if (impactBin >= nRows) impactBin = nRows - 1;
                        ++impactHits[impactBin];
                    }
                }

                batchVisitSegment(prevX,prevZ,x,z,toeStartX,endX,toeZ,toeZ+TOE_HEIGHT,CELL_X,CELL_Z,
                    [&](int idx,double entry){ markCell(idx,t-h+entry*h); });
                bool inToe=x>=toeStartX && x<=endX && z>=toeZ && z<=toeZ+TOE_HEIGHT;
                if(inToe && t>=nextTrajTime) {
                    trialTrace.push_back(BatchTracePoint{t,x,z,sp});
                    nextTrajTime=t+TRAJ_SAMPLE_DT;
                }
                finalX=x; finalZ=z; finalSpeed=sp;

                prevX = x;
                prevZ = z;

                if (x >= endX) { reachedEnd = true; break; }
            }

            finalX = stone.getX();
            finalZ = stone.getZ();
            finalSpeed = stone.getSpeed();
            stopped = (!stone.isMoving()) && (!reachedEnd);
            timedOut = !reachedEnd && !stopped && t >= MAX_BATCH_TIME - 1e-9;
        }
        catch (const std::exception& ex) {
            failed = true;
            ++nFailed;
            batchCsvText(errorMsg, sizeof(errorMsg), ex.what());
        }

        if(timedOut) ++nTimeout;
        if (failed || timedOut) {
            // 失败的尝试不计入路线概率；撤销它在网格/收集器上的计数。
            for (int i = 0; i < nCells; ++i) {
                if (cellStamp[i] == trial) { --cellHits[i]; cellStamp[i] = -1; }
            }
            for (int c = 0; c < nCols; ++c) {
                if (colStamp[c] == trial) { --colHits[c]; colStamp[c] = -1; }
            }
            if (enteredToe) { --nEntered; enteredToe = false; toeEntryTime = -1.0; }
            if (reachedCollector) { --nCollector; reachedCollector = false; }
            if (collectedIn10m) {
                --nCollected10;
                if (impactBin >= 0 && impactBin < nRows && impactHits[impactBin] > 0) {
                    --impactHits[impactBin];
                }
                collectedIn10m = false;
            }
            reachedEnd = stopped = false;
            collectorTime=collectorZ=-1;
        } else {
            ++nSuccess;
            if (reachedEnd) ++nReachedEnd;
            if (stopped) ++nStopped;
            for(size_t i=0;i<trialTrace.size();++i) {
                const BatchTracePoint& row=trialTrace[i];
                batchPrint(fpTraj,"%d,%.9g,%.12g,%.12g,%.12g\n",trial,row.t,row.x,row.z,row.speed);
            }

            sumFX += finalX; sumFX2 += finalX * finalX;
            sumFZ += finalZ; sumFZ2 += finalZ * finalZ;
            sumMaxV += maxSpeed; sumMaxV2 += maxSpeed * maxSpeed;
            sumPath += pathLength; sumPath2 += pathLength * pathLength;
            sumTime += t; sumTime2 += t * t;
        }

        char errorField[160];
        batchCsvText(errorField, sizeof(errorField), failed ? errorMsg : (timedOut ? "time_limit" : "ok"));

        batchPrint(fpTrials,
            "%d,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%.4f,%.4f,%.4f,%.4f,"
            "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
            "%d,%d,%.4f,%.4f,%d,%d,%d,%d,%d,%s,%.4f,%.4f\n",
            trial, x0, groundZ0, v0, radius, area, sides, mu, e, cd, crr,
            finalX, finalZ, finalSpeed, maxSpeed, maxHeight, t,
            enteredToe ? 1 : 0, reachedCollector ? 1 : 0,
            collectorTime, collectorZ,
            collectedIn10m ? 1 : 0, reachedEnd ? 1 : 0,
            stopped ? 1 : 0, timedOut ? 1 : 0,
            failed ? 1 : 0, errorField, pathLength, toeEntryTime);

        if ((trial + 1) % 10 == 0 || trial + 1 == nTrials) {
            std::printf("  trial %d / %d\r", trial + 1, nTrials);
            std::fflush(stdout);
        }
    }
    std::printf("\n");

    const bool complete=nSuccess==nTrials;
    const char* status=complete?"complete":"incomplete";
    for (int row = 0; row < nRows; ++row) {
        for (int col = 0; col < nCols; ++col) {
            int idx = row * nCols + col;
            double xmin = toeStartX + col * CELL_X;
            double xmax = xmin + CELL_X;
            double zmin = toeZ+row * CELL_Z;
            double zmax = zmin + CELL_Z;
            if (xmax > endX) xmax = endX;
            if (zmax > toeZ+TOE_HEIGHT) zmax = toeZ+TOE_HEIGHT;
            double cx = 0.5 * (xmin + xmax);
            double cz = 0.5 * (zmin + zmax);
            double groundZ = hill.getHeight(cx);
            batchPrint(fpGrid,
                "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%s,%s,%s,%d\n",
                xmin,xmax,zmin,zmax,cx,cz,groundZ,cellHits[idx],
                batchProbability(cellHits[idx],nTrials,complete).c_str(),
                batchProbability(cellHits[idx],nEntered,complete).c_str(),status,nTrials);
        }
    }

    for (int col = 0; col < nCols; ++col) {
        double xmin = toeStartX + col * CELL_X;
        double xmax = xmin + CELL_X;
        if (xmax > endX) xmax = endX;
        double cx = 0.5 * (xmin + xmax);
        double groundZ = hill.getHeight(cx);
        batchPrint(fpProfile,"%.9g,%.9g,%.9g,%.9g,%d,%s,%s,%s,%d\n",
            xmin,xmax,cx,groundZ,colHits[col],batchProbability(colHits[col],nTrials,complete).c_str(),
            batchProbability(colHits[col],nEntered,complete).c_str(),status,nTrials);
    }

    for (int bin = 0; bin < nRows; ++bin) {
        double zmin = toeZ+bin * CELL_Z;
        double zmax = zmin + CELL_Z;
        if (zmax > toeZ+TOE_HEIGHT) zmax = toeZ+TOE_HEIGHT;
        double cz = 0.5 * (zmin + zmax);
        batchPrint(fpImpact,"%.9g,%.9g,%.9g,%d,%s,%s,%s,%d\n",zmin,zmax,cz,impactHits[bin],
            batchProbability(impactHits[bin],nTrials,complete).c_str(),
            batchProbability(impactHits[bin],nCollected10,complete).c_str(),status,nTrials);
    }

    batchPrint(fpSummary,
        "run_status,n_trials,seed,toe_start_x,toe_height,"
        "n_entered_toe,p_entered_toe,"
        "n_reached_collector,p_reached_collector,"
        "n_collected_0_10m,p_collected_0_10m,"
        "n_reached_end,p_reached_end,"
        "n_stopped,p_stopped,n_timed_out,p_timed_out,"
        "n_failed,p_failed,n_success,"
        "mean_final_x,std_final_x,mean_final_z,std_final_z,"
        "mean_max_speed,std_max_speed,"
        "mean_path_length,std_path_length,"
        "mean_sim_time,std_sim_time,toe_base_z,dt_s,time_limit_s,barrier_enabled\n");
    batchPrint(fpSummary,
        "%s,%d,%u,%.3f,%.3f,%d,%s,%d,%s,%d,%s,%d,%s,%d,%s,%d,%.6f,%d,%.6f,%d,"
        "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.12g,%.12g,%.12g,0\n",
        status, nTrials, seed, toeStartX, TOE_HEIGHT,
        nEntered, batchProbability(nEntered,nTrials,complete).c_str(),
        nCollector, batchProbability(nCollector,nTrials,complete).c_str(),
        nCollected10, batchProbability(nCollected10,nTrials,complete).c_str(),
        nReachedEnd, batchProbability(nReachedEnd,nTrials,complete).c_str(),
        nStopped, batchProbability(nStopped,nTrials,complete).c_str(),
        nTimeout, (double)nTimeout / (double)nTrials,
        nFailed, (double)nFailed / (double)nTrials,
        nSuccess,
        batchMean(sumFX, nSuccess), batchStd(sumFX, sumFX2, nSuccess),
        batchMean(sumFZ, nSuccess), batchStd(sumFZ, sumFZ2, nSuccess),
        batchMean(sumMaxV, nSuccess), batchStd(sumMaxV, sumMaxV2, nSuccess),
        batchMean(sumPath, nSuccess), batchStd(sumPath, sumPath2, nSuccess),
        batchMean(sumTime, nSuccess), batchStd(sumTime, sumTime2, nSuccess),toeZ,DT,MAX_BATCH_TIME);

    const std::string outputPath=outputs.finish(complete);
    std::printf(complete?"Batch simulation finished.\n":"Batch INCOMPLETE: failed or timed-out trials; probabilities are blank.\n");
    std::printf("  trials              : %d\n", nTrials);
    std::printf("  seed                : %u\n", seed);
    std::printf("  toe region          : x in [%.2f, %.2f] m, z in [%.2f, %.2f] m\n",
                toeStartX, endX, toeZ, toeZ+TOE_HEIGHT);
    std::printf("  entered toe         : %d (%.1f%%)\n",
                nEntered, 100.0 * nEntered / nTrials);
    std::printf("  reached collector   : %d (%.1f%%)\n",
                nCollector, 100.0 * nCollector / nTrials);
    std::printf("  collected in 0-10 m : %d (%.1f%%)\n",
                nCollected10, 100.0 * nCollected10 / nTrials);
    std::printf("  reached end         : %d (%.1f%%)\n",
                nReachedEnd, 100.0 * nReachedEnd / nTrials);
    std::printf("  stopped on slope    : %d (%.1f%%)\n",
                nStopped, 100.0 * nStopped / nTrials);
    std::printf("  timeout             : %d (%.1f%%)\n",
                nTimeout, 100.0 * nTimeout / nTrials);
    std::printf("  failed              : %d (%.1f%%)\n",
                nFailed, 100.0 * nFailed / nTrials);
    std::printf("  CSV directory       : %s\n",outputPath.c_str());
    return complete?0:2;
}


static int runBatchSimulation(int nTrials,unsigned int seed) {
    try { return runBatchSimulationImpl(nTrials,seed); }
    catch(const std::exception& e) { std::fprintf(stderr,"Batch FAILED: %s\n",e.what()); return 1; }
}
static int batchSelfTest() {
    try {
        unsigned long long value=0;
        requireTest(batchParseUnsigned("20000",20000,value) && value==20000,"parse valid count");
        requireTest(!batchParseUnsigned("abc",20000,value) && !batchParseUnsigned("-1",20000,value) &&
            !batchParseUnsigned("20001",20000,value) && !batchParseUnsigned("2x",20000,value),"reject invalid counts");
        requireTest(!batchParseUnsigned("4294967296",UINT_MAX,value),"reject overflowing seed");
        std::vector<int> hits(20,0);
        auto mark=[&](int idx,double){hits[idx]=1;};
        batchVisitSegment(-1,0.5,11,0.5,0,10,0,2,1,1,mark);
        requireTest(std::count(hits.begin(),hits.end(),1)==10,"cross all cells including clipped endpoints");
        batchVisitSegment(11,0.5,-1,0.5,0,10,0,2,1,1,mark);
        requireTest(std::count(hits.begin(),hits.end(),1)==10,"repeat traversal does not double-count");
        std::fill(hits.begin(),hits.end(),0);
        batchVisitSegment(0.99,1.02,1.02,0.99,0,10,0,2,1,1,mark);
        requireTest(hits[10] && hits[11] && hits[1],"diagonal corner sliver is counted");
        std::fill(hits.begin(),hits.end(),0);
        batchVisitSegment(0.5,0.5,1.5,1.5,0,10,0,2,1,1,mark);
        requireTest(hits[0] && hits[11] && std::count(hits.begin(),hits.end(),1)==2,"exact corner adds no touch-only cells");
        std::fill(hits.begin(),hits.end(),0);
        batchVisitSegment(1,0.5,1,1.5,0,10,0,2,1,1,mark);
        requireTest(hits[1] && hits[11] && std::count(hits.begin(),hits.end(),1)==2,"internal boundary belongs to upper cell");
        std::fill(hits.begin(),hits.end(),0);
        batchVisitSegment(10,2,10,2,0,10,0,2,1,1,mark);
        requireTest(hits[19],"upper outer boundary belongs to last cell");
        std::fill(hits.begin(),hits.end(),0);
        batchVisitSegment(1,3,2,3,0,10,0,2,1,1,mark);
        requireTest(std::count(hits.begin(),hits.end(),1)==0,"outside height band");
        requireTest(batchProbability(1,2,true)=="0.5" && batchProbability(0,2,false).empty() &&
            batchProbability(0,0,true).empty(),"invalid/incomplete probabilities are not zero");
        TerrainSettings settings; HillSlope custom(buildTerrain(settings));
        const auto& pts=custom.getPoints();
        requireTest(custom.getEndX()==pts[pts.size()-1].x,"fractional terrain endpoint retained");
        DynamicArray<Point> translated; translated.push_back(Point(0,25)); translated.push_back(Point(10.5,5));
        HillSlope shifted(translated);
        requireTest(fabs(batchToeStart(shifted,15)-5.25)<1e-12,"exact contour relative to toe elevation");
        std::printf("PASS batch: strict input, segment traversal, unique hits, grid boundaries, incomplete probabilities and fractional terrain.\n");
        return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL batch: %s\n",e.what()); return 1; }
}

#endif // BATCH_STATS_H
