#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace rs3d {
constexpr double pi=3.1415926535897932384626433832795;
// Metres, seconds, kilograms; x downslope, y across slope, z up.
struct Vec3 {
    double x=0,y=0,z=0;
    double& operator[](int i){return i==0?x:(i==1?y:z);}
    double operator[](int i)const{return i==0?x:(i==1?y:z);}
};
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator-(Vec3 a){return {-a.x,-a.y,-a.z};}
inline Vec3 operator*(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
inline Vec3 operator*(double s,Vec3 a){return a*s;}
inline Vec3 operator/(Vec3 a,double s){return a*(1/s);}
inline Vec3& operator+=(Vec3& a,Vec3 b){a=a+b;return a;}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double norm2(Vec3 a){return dot(a,a);}
inline double norm(Vec3 a){return std::sqrt(norm2(a));}
inline Vec3 unit(Vec3 a){double n=norm(a);if(!(n>0)||!std::isfinite(n))throw std::runtime_error("invalid vector normalization");return a/n;}
inline bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
struct Mat3 {
    std::array<double,9> a{};
    double& operator()(int i,int j){return a[3*i+j];}
    double operator()(int i,int j)const{return a[3*i+j];}
    static Mat3 identity(){Mat3 m;m(0,0)=m(1,1)=m(2,2)=1;return m;}
};
inline Mat3 operator+(Mat3 a,const Mat3&b){for(int i=0;i<9;++i)a.a[i]+=b.a[i];return a;}
inline Mat3 operator-(Mat3 a,const Mat3&b){for(int i=0;i<9;++i)a.a[i]-=b.a[i];return a;}
inline Mat3 operator*(Mat3 a,double s){for(double&v:a.a)v*=s;return a;}
inline Mat3 outer(Vec3 a,Vec3 b){Mat3 m;for(int i=0;i<3;++i)for(int j=0;j<3;++j)m(i,j)=a[i]*b[j];return m;}
inline Mat3 transpose(Mat3 a){Mat3 b;for(int i=0;i<3;++i)for(int j=0;j<3;++j)b(i,j)=a(j,i);return b;}
inline Mat3 operator*(Mat3 a,Mat3 b){Mat3 c;for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)c(i,j)+=a(i,k)*b(k,j);return c;}
inline Vec3 operator*(Mat3 a,Vec3 b){return {dot({a(0,0),a(0,1),a(0,2)},b),dot({a(1,0),a(1,1),a(1,2)},b),dot({a(2,0),a(2,1),a(2,2)},b)};}
inline double frobenius(Mat3 a){double v=0;for(double x:a.a)v+=x*x;return std::sqrt(v);}
inline double determinant(Mat3 a){return a(0,0)*(a(1,1)*a(2,2)-a(1,2)*a(2,1))-a(0,1)*(a(1,0)*a(2,2)-a(1,2)*a(2,0))+a(0,2)*(a(1,0)*a(2,1)-a(1,1)*a(2,0));}
inline Mat3 inverse(Mat3 a){double d=determinant(a);if(!std::isfinite(d)||d==0)throw std::runtime_error("singular matrix");Mat3 b;for(int i=0;i<3;++i)for(int j=0;j<3;++j)b(j,i)=(a((i+1)%3,(j+1)%3)*a((i+2)%3,(j+2)%3)-a((i+1)%3,(j+2)%3)*a((i+2)%3,(j+1)%3))/d;return b;}
struct Quaternion {
    double w=1,x=0,y=0,z=0;
    Mat3 matrix()const{
        double s=std::sqrt(w*w+x*x+y*y+z*z);if(!std::isfinite(s)||s==0)throw std::runtime_error("invalid quaternion");
        double a=w/s,b=x/s,c=y/s,d=z/s;Mat3 r;
        r.a={1-2*(c*c+d*d),2*(b*c-a*d),2*(b*d+a*c),2*(b*c+a*d),1-2*(b*b+d*d),2*(c*d-a*b),2*(b*d-a*c),2*(c*d+a*b),1-2*(b*b+c*c)};return r;
    }
    static Quaternion fromAxisAngle(Vec3 axis,double angle){axis=unit(axis);double s=std::sin(angle/2);return {std::cos(angle/2),axis.x*s,axis.y*s,axis.z*s};}
};
inline std::array<double,3> eigenvalues(Mat3 a){
    for(int n=0;n<40;++n){
        int p=0,q=1;for(int i=0;i<3;++i)for(int j=i+1;j<3;++j)if(std::abs(a(i,j))>std::abs(a(p,q))){p=i;q=j;}
        if(std::abs(a(p,q))<1e-15*frobenius(a))break;
        double theta=0.5*std::atan2(2*a(p,q),a(q,q)-a(p,p)),c=std::cos(theta),s=std::sin(theta);
        Mat3 r=Mat3::identity();r(p,p)=r(q,q)=c;r(p,q)=s;r(q,p)=-s;a=transpose(r)*a*r;
    }
    std::array<double,3> out{a(0,0),a(1,1),a(2,2)};std::sort(out.begin(),out.end());return out;
}
inline Mat3 axisRotation(Vec3 axis,double angle){axis=unit(axis);double c=std::cos(angle),s=std::sin(angle);Mat3 r=Mat3::identity()*c+outer(axis,axis)*(1-c);r(0,1)-=axis.z*s;r(0,2)+=axis.y*s;r(1,0)+=axis.z*s;r(1,2)-=axis.x*s;r(2,0)-=axis.y*s;r(2,1)+=axis.x*s;return r;}
}
