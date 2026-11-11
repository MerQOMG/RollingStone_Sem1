#include <Jolt/Jolt.h>
#include <Jolt/Geometry/ConvexHullBuilder.h>
#include "geometry.h"
#include "engine_geometry.h"
#include "random.h"
#include <chrono>
#include <cfloat>
#include <climits>
#include <map>
#include <set>
#include <sstream>

namespace rs3d {
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void checkpoint(const std::function<bool()>& cancelled){if(cancelled&&cancelled())throw std::runtime_error("geometry cancelled");}
Vec3 mean(const std::vector<Vec3>& v){Vec3 c;for(auto p:v)c+=p;return c/double(v.size());}
double extent(const std::vector<Vec3>& v,Vec3 c){double r=0;for(auto p:v)r=std::max(r,norm(p-c));return r;}
std::vector<Face> joltHull(const std::vector<Vec3>& v,double tau){
    JPH::ConvexHullBuilder::Positions points;for(auto p:v)points.emplace_back(float(p.x),float(p.y),float(p.z));
    JPH::ConvexHullBuilder builder(points);const char* error=nullptr;
    auto result=builder.Initialize(INT_MAX,float(tau),error);
    if(result!=JPH::ConvexHullBuilder::EResult::Success)throw std::runtime_error(error?error:"Jolt topology failure");
    require(builder.GetNumVerticesUsed()==int(v.size()),"Jolt topology discarded vertices");
    std::vector<Face> faces;
    for(auto f:builder.GetFaces()){
        if(f->mRemoved)continue;
        std::vector<int> ids;auto e=f->mFirstEdge;
        do{ids.push_back(e->mStartIdx);e=e->mNextEdge;}while(e!=f->mFirstEdge);
        for(size_t i=1;i+1<ids.size();++i)faces.push_back({ids[0],ids[i],ids[i+1]});
    }
    return faces;
}
// Incremental double convex hull fallback. Maintains an interior tetrahedral witness;
// only visible faces are removed and each horizon edge gets one new outward face.
std::vector<Face> doubleHull(const std::vector<Vec3>& v){
    int i0=0,i1=1,i2=-1,i3=-1;double best=0;
    for(int i=1;i<int(v.size());++i)if(norm2(v[i]-v[i0])>best){best=norm2(v[i]-v[i0]);i1=i;}
    best=0;for(int i=0;i<int(v.size());++i){double d=norm2(cross(v[i1]-v[i0],v[i]-v[i0]));if(d>best){best=d;i2=i;}}
    require(i2>=0&&best>1e-24,"collinear vertices");Vec3 normal=unit(cross(v[i1]-v[i0],v[i2]-v[i0]));
    best=0;for(int i=0;i<int(v.size());++i){double d=std::abs(dot(normal,v[i]-v[i0]));if(d>best){best=d;i3=i;}}
    require(i3>=0&&best>1e-12,"coplanar vertices");
    Vec3 witness=(v[i0]+v[i1]+v[i2]+v[i3])/4;
    auto orient=[&](Face f){if(dot(cross(v[f.b]-v[f.a],v[f.c]-v[f.a]),witness-v[f.a])>0)std::swap(f.b,f.c);return f;};
    std::vector<Face> faces{orient({i0,i1,i2}),orient({i0,i3,i1}),orient({i0,i2,i3}),orient({i1,i3,i2})};
    for(int i=0;i<int(v.size());++i){
        if(i==i0||i==i1||i==i2||i==i3)continue;
        std::map<std::pair<int,int>,int> horizon;std::vector<Face> keep;
        auto edge=[&](int a,int b){auto reverse=horizon.find({b,a});if(reverse!=horizon.end())horizon.erase(reverse);else horizon[{a,b}]=1;};
        for(auto f:faces){auto n=cross(v[f.b]-v[f.a],v[f.c]-v[f.a]);
            if(dot(n,v[i]-v[f.a])>1e-12*norm(n)){edge(f.a,f.b);edge(f.b,f.c);edge(f.c,f.a);}else keep.push_back(f);
        }
        for(auto e:horizon)keep.push_back(orient({e.first.first,e.first.second,i}));
        faces=std::move(keep);
    }
    return faces;
}
void topology(const std::vector<Vec3>& v,const std::vector<Face>& faces){
    require(faces.size()==2*v.size()-4,"triangle count does not match k");
    Vec3 center=mean(v);double scale=extent(v,center);require(scale>0,"zero hull extent");
    std::map<std::pair<int,int>,std::pair<int,int>> edges;std::set<int> used;
    for(auto f:faces){
        for(int i:{f.a,f.b,f.c}){require(i>=0&&i<int(v.size()),"invalid face index");used.insert(i);}
        Vec3 a=v[f.a],n=cross(v[f.b]-a,v[f.c]-a);double area=norm(n);
        require(area>1e-14*scale*scale,"degenerate triangle");n=n/area;
        require(dot(n,center-a)<0,"inward/zero volume face");
        for(auto p:v)require(dot(n,p-a)<=1e-10*scale,"double support-plane check failed");
        for(auto e:{std::pair<int,int>{f.a,f.b},{f.b,f.c},{f.c,f.a}}){int sign=e.first<e.second?1:-1;auto key=std::minmax(e.first,e.second);auto& info=edges[key];++info.first;info.second+=sign;}
    }
    require(used.size()==v.size(),"hull discarded input vertex");
    require(edges.size()==3*v.size()-6,"edge count does not match k");
    for(auto e:edges)require(e.second.first==2&&e.second.second==0,"non-manifold or inconsistent edge");
    require(int(v.size())-int(edges.size())+int(faces.size())==2,"Euler characteristic failure");
}
void integrate(Polyhedron& p){
    const Vec3 origin=mean(p.vertices);Vec3 first;Mat3 second;double volume=0;
    for(auto f:p.faces){
        Vec3 a=p.vertices[f.a]-origin,b=p.vertices[f.b]-origin,c=p.vertices[f.c]-origin;
        double dv=dot(a,cross(b,c))/6;require(dv>0,"nonpositive tetrahedron volume");
        Vec3 sum=a+b+c;volume+=dv;first+=sum*(dv/4);
        second=second+(outer(sum,sum)+outer(a,a)+outer(b,b)+outer(c,c))*(dv/20);
    }
    require(std::isfinite(volume)&&volume>0,"invalid solid volume");Vec3 relative=first/volume;
    p.volume=volume;p.center=origin+relative;second=second-outer(relative,relative)*volume;
    p.inertia=Mat3::identity()*(second(0,0)+second(1,1)+second(2,2))-second;
    p.innerRadius=1e300;
    for(auto f:p.faces){Vec3 a=p.vertices[f.a],n=unit(cross(p.vertices[f.b]-a,p.vertices[f.c]-a));p.innerRadius=std::min(p.innerRadius,-dot(n,p.center-a));}
    p.slenderness=extent(p.vertices,p.center)/p.innerRadius;
}
std::vector<Vec3> reference(int k){
    if(k==4){double a=1/std::sqrt(3.0);return {{a,a,a},{a,-a,-a},{-a,a,-a},{-a,-a,a}};}
    std::vector<Vec3> v{{0,0,1},{0,0,-1}};
    for(int i=0;i<k-2;++i){double a=2*pi*i/(k-2);v.push_back({std::cos(a),std::sin(a),0});}return v;
}
}

double hullTolerance(const std::vector<Vec3>& points){Vec3 m;for(auto p:points)for(int i=0;i<3;++i)m[i]=std::max(m[i],std::abs(p[i]));return std::max(1e-6,3.0*FLT_EPSILON*(m.x+m.y+m.z));}
double referenceVolume(int k,double r){require(k>=4&&k<=100,"k outside kernel range [4,100]");return (k==4?8/(9*std::sqrt(3.0)):(k-2)*std::sin(2*pi/(k-2))/3)*r*r*r;}
void validateGeometrySettings(const GeometrySettings&s){
    require(s.kMin>=10&&s.kMax<=100&&s.kMin<=s.kMax,"production k must satisfy 10 <= k_min <= k_max <= 100");
    require(std::isfinite(s.radius)&&s.radius>=0.2&&s.radius<=10,"radius outside [0.2,10] m");
    // Decimal boundary inputs and equivalent multiplication orders can differ by a few
    // representable doubles. This rounding guard does not relax geometric tolerances.
    constexpr double rounding=8*DBL_EPSILON;
    require(std::isfinite(s.volume)&&s.volume/(s.radius*s.radius*s.radius)>=0.02*(1-rounding),"volume below first-release V/r^3 >= 0.02 support limit");
    double cap=1e300;for(int k=s.kMin;k<=s.kMax;++k)cap=std::min(cap,referenceVolume(k,s.radius));
    require(s.volume<=0.90*cap*(1+rounding),"volume above constructible first-release support limit (not a mathematical maximum)");
}
Polyhedron convexPolyhedron(const std::vector<Vec3>& vertices){
    require(vertices.size()>=4&&vertices.size()<=100,"kernel requires 4..100 vertices");
    for(auto v:vertices)require(finite(v),"non-finite vertex");
    Vec3 origin=mean(vertices);double scale=extent(vertices,origin);require(std::isfinite(scale)&&scale>0,"zero/invalid extent");
    std::vector<Vec3> normalized;for(auto p:vertices)normalized.push_back((p-origin)/scale);
    for(size_t i=0;i<normalized.size();++i)for(size_t j=0;j<i;++j)require(norm(normalized[i]-normalized[j])>1e-11,"duplicate/near-duplicate vertices");
    Polyhedron p;p.vertices=vertices;p.tau=hullTolerance(normalized);
    try{p.faces=joltHull(normalized,p.tau);topology(normalized,p.faces);}
    catch(const std::runtime_error&){p.faces=doubleHull(normalized);p.doubleFallback=true;topology(normalized,p.faces);}
    integrate(p);validatePolyhedron(p);return p;
}
Polyhedron polyhedronFromMesh(const std::vector<Vec3>&vertices,const std::vector<Face>&faces){
    require(vertices.size()>=4&&vertices.size()<=100,"replay vertex count outside [4,100]");for(auto v:vertices)require(finite(v),"non-finite replay vertex");
    Polyhedron p;p.vertices=vertices;p.faces=faces;topology(vertices,faces);integrate(p);
    double radius=0;for(auto v:vertices)radius=std::max(radius,norm(v));std::vector<Vec3>normalized;for(auto v:vertices)normalized.push_back((v-p.center)/radius);p.tau=hullTolerance(normalized);validatePolyhedron(p);return p;
}
void validatePolyhedron(const Polyhedron&p){
    topology(p.vertices,p.faces);
    require(std::isfinite(p.volume)&&p.volume>0&&finite(p.center),"invalid mass properties");
    require(p.innerRadius>0&&std::isfinite(p.slenderness),"COM outside hull");
    require(frobenius(p.inertia-transpose(p.inertia))<=1e-12*frobenius(p.inertia),"asymmetric inertia");
    auto e=eigenvalues(p.inertia);require(e[0]>0&&std::isfinite(e[2]),"inertia is not positive definite");
}
GeneratedShape generatePolyhedron(const GeometrySettings&s,std::uint64_t seed,const std::function<bool()>& cancelled,int rejectFirst){
    validateGeometrySettings(s);auto begin=std::chrono::steady_clock::now();Random krng(mix(seed^kDomain));
    GeneratedShape out;auto& d=out.diagnostics;d.k=krng.integer(s.kMin,s.kMax);d.trialSeed=seed;
    auto checkCancellation=[&](){if(cancelled&&cancelled()){d.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();throw GenerationFailure("geometry cancelled",d,true);}return false;};
    const double target=s.volume/(s.radius*s.radius*s.radius);
    for(int attempt=0;attempt<64;++attempt){
        checkpoint(checkCancellation);d.attempts=attempt+1;d.attemptSeed=mix((seed^shapeDomain)+std::uint64_t(attempt));Random rng(d.attemptSeed);
        try{
            if(attempt<rejectFirst)throw std::runtime_error("injected attempt rejection");
            auto base=reference(d.k);std::vector<Vec3> direction;for(auto q:base){auto v=rng.direction();direction.push_back(v-q*dot(q,v));}
            std::vector<Vec3> ref;double amplitude=0.8/(d.k-2);
            bool above=false;
            for(int shrink=0;shrink<24;++shrink){checkpoint(checkCancellation);ref.clear();for(size_t i=0;i<base.size();++i)ref.push_back(unit(base[i]+direction[i]*amplitude));
                if(convexPolyhedron(ref).volume>target){above=true;break;}amplitude*=0.5;}
            require(above,"reference perturbation did not bracket target");
            Vec3 n;bool axis=false;
            for(int i=0;i<128;++i){checkpoint(checkCancellation);n=rng.direction();axis=true;for(auto q:ref)if(1+dot(q,n)<=0.01){axis=false;break;}if(axis)break;}
            require(axis,"128 projection axes exhausted");
            Vec3 e1=unit(cross(n,std::abs(n.z)<0.8?Vec3{0,0,1}:Vec3{1,0,0})),e2=cross(n,e1);
            std::vector<Vec3> uv;for(auto q:ref){double den=1+dot(q,n);uv.push_back({dot(q,e1)/den,dot(q,e2)/den,0});}
            auto deform=[&](double t){std::vector<Vec3> v;for(auto u:uv){double sq=t*t*(u.x*u.x+u.y*u.y);v.push_back((e1*(2*t*u.x)+e2*(2*t*u.y)+n*(1-sq))/(1+sq));}return v;};
            double lo=0,hi=1;Polyhedron solved;bool converged=false;
            for(int iteration=0;iteration<80;++iteration){checkpoint(checkCancellation);double t=(lo+hi)/2;solved=convexPolyhedron(deform(t));d.rootIterations=iteration+1;
                if(std::abs(solved.volume-target)<=std::max(1e-13,1e-9*target)){converged=true;break;}
                if(solved.volume>target)hi=t;else lo=t;
            }
            require(converged,"volume bracket root exceeded 80 iterations");
            auto rotation=axisRotation(rng.direction(),2*pi*rng.uniform());std::vector<Vec3> final;
            for(auto q:solved.vertices)final.push_back((rotation*q)*s.radius);
            out.geometry=convexPolyhedron(final);auto& p=out.geometry;
            for(auto q:p.vertices)require(std::abs(norm(q)-s.radius)<=1e-8*s.radius,"sphere radius constraint violated");
            require(std::abs(p.volume-s.volume)<=std::max(1e-12*s.radius*s.radius*s.radius,1e-6*s.volume),"canonical volume tolerance exceeded");
            std::vector<Vec3> enginePoints;for(auto q:p.vertices)enginePoints.push_back((q-p.center)/s.radius);p.tau=hullTolerance(enginePoints);
            require(p.innerRadius>=32*p.tau*s.radius,"inner radius below 32*tau*r quality threshold");
            createCollisionShape(p,s.radius,2600,d);
            d.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();return out;
        }catch(const GenerationFailure&){throw;}catch(const std::runtime_error&e){checkpoint(checkCancellation);d.rejections.push_back(e.what());}
    }
    std::ostringstream msg;msg<<"geometry exhausted 64 attempts: trial_seed="<<seed<<" k="<<d.k<<" r="<<s.radius<<" V="<<s.volume;for(auto& reason:d.rejections)msg<<"; "<<reason;
    d.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();throw GenerationFailure(msg.str(),d);
}
}
