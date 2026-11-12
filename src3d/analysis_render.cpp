#include "analysis_render.h"
#include <rlgl.h>
#include <cstring>
#include "hash.h"
void drawNativeLineArray(int count);
namespace rs3d::ui {
Color heatColor(double value,unsigned char alpha){value=std::clamp(value,0.,1.);return {static_cast<unsigned char>(35+220*value),static_cast<unsigned char>(90+90*(1-std::abs(2*value-1))),static_cast<unsigned char>(220-185*value),alpha};}
void AnalysisRenderer::clearLine(Line&line){if(line.vbo)rlUnloadVertexBuffer(line.vbo);if(line.vao)rlUnloadVertexArray(line.vao);line={};}
AnalysisRenderer::~AnalysisRenderer(){clear();if(shader_.id)UnloadShader(shader_);}
void AnalysisRenderer::clear(){for(auto&item:lines_)clearLine(item.second);lines_.clear();if(surface_.meshCount)UnloadModel(surface_);surface_={};surfaceCells_.clear();surfaceCounts_.clear();surfaceKey_.clear();surfaceDenominator_=-1;}
void AnalysisRenderer::loadLine(Line&line,std::shared_ptr<const DisplayTrajectory>source){
    clearLine(line);line.source=std::move(source);const auto&points=line.source->points;if(points.size()<2)return;std::vector<Vector3>segments;segments.reserve(points.size()*2);bool diagnostic=line.source->status!=StopReason::running&&!normalCompletion(line.source->status);
    for(size_t i=1;i<points.size();++i){if(diagnostic&&i%2==0)continue;segments.push_back(vector(points[i-1].center));segments.push_back(vector(points[i].center));}if(segments.empty())return;
    if(!shader_.id){shader_=LoadShaderFromMemory("#version 330\nlayout(location=0) in vec3 vertexPosition;uniform mat4 mvp;void main(){gl_Position=mvp*vec4(vertexPosition,1.0);}","#version 330\nuniform vec4 lineColor;out vec4 finalColor;void main(){finalColor=lineColor;}");if(!shader_.id||shader_.id==rlGetShaderIdDefault())throw std::runtime_error("trajectory shader allocation failed");mvp_=GetShaderLocation(shader_,"mvp");color_=GetShaderLocation(shader_,"lineColor");}
    line.count=int(segments.size());line.vao=rlLoadVertexArray();rlEnableVertexArray(line.vao);line.vbo=rlLoadVertexBuffer(segments.data(),int(segments.size()*sizeof(Vector3)),false);rlSetVertexAttribute(0,3,RL_FLOAT,false,sizeof(Vector3),0);rlEnableVertexAttribute(0);rlDisableVertexArray();rlDisableVertexBuffer();if(!line.vao||!line.vbo)throw std::runtime_error("trajectory GPU allocation failed");
}
void AnalysisRenderer::updateTrajectories(const std::vector<std::shared_ptr<const DisplayTrajectory>>&trajectories){
    for(size_t i=0;i<trajectories.size();++i){const auto&t=trajectories[i];if(!t||t->points.empty())continue;auto&line=lines_[t->trialId];if(line.source!=t)loadLine(line,t);}
}
void AnalysisRenderer::drawTrajectories(std::uint64_t selected,float alpha)const{
    if(!shader_.id)return;rlDrawRenderBatchActive();rlDisableDepthMask();rlEnableShader(shader_.id);rlSetUniformMatrix(mvp_,MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));
    for(const auto&item:lines_){const auto&line=item.second;if(!line.count)continue;bool diagnostic=line.source->status!=StopReason::running&&!normalCompletion(line.source->status);Color c=item.first==selected?Color{75,245,255,255}:diagnostic?Color{255,100,105,255}:Color{246,191,83,255};float color[]{c.r/255.f,c.g/255.f,c.b/255.f,item.first==selected?1.f:alpha};rlSetUniform(color_,color,RL_SHADER_UNIFORM_VEC4,1);rlEnableVertexArray(line.vao);drawNativeLineArray(line.count);}rlDisableVertexArray();rlDisableShader();rlEnableDepthMask();
}
void AnalysisRenderer::updateSurface(const RunConfig&config,const Grid3D&grid,const std::vector<std::uint64_t>&counts,double denominator,const std::string&layer){
    if(counts.size()!=size_t(grid.size(0))*grid.size(1)||!(denominator>0))return;ContentHash identity;identity.text(config.terrain->contentHash);identity.text(layer);for(int axis=0;axis<2;++axis){identity.number(grid.settings().lower[axis]);identity.number(grid.settings().upper[axis]);identity.number(grid.settings().cell[axis]);}std::string key=identity.hex();
    if(key!=surfaceKey_){if(surface_.meshCount)UnloadModel(surface_);surface_={};surfaceCells_.clear();std::vector<Vector3>vertices;
        for(int x=0;x<grid.size(0);++x)for(int y=0;y<grid.size(1);++y){auto cell=size_t(x)*grid.size(1)+y;auto triangles=terrainCellTriangles(*config.terrain,grid.lines(0)[x],grid.lines(0)[x+1],grid.lines(1)[y],grid.lines(1)[y+1]);for(auto p:triangles){p.z+=.01;vertices.push_back(vector(p));surfaceCells_.push_back(cell);}}
        if(!vertices.empty()){Mesh mesh{};mesh.vertexCount=int(vertices.size());mesh.triangleCount=mesh.vertexCount/3;mesh.vertices=static_cast<float*>(MemAlloc(unsigned(vertices.size()*sizeof(Vector3))));mesh.colors=static_cast<unsigned char*>(MemAlloc(unsigned(vertices.size()*4)));std::memcpy(mesh.vertices,vertices.data(),vertices.size()*sizeof(Vector3));std::memset(mesh.colors,0,vertices.size()*4);UploadMesh(&mesh,true);surface_=LoadModelFromMesh(mesh);}surfaceKey_=key;surfaceCounts_.clear();surfaceDenominator_=-1;
    }
    if(surface_.meshCount&&(surfaceCounts_!=counts||surfaceDenominator_!=denominator)){auto&mesh=surface_.meshes[0];for(size_t i=0;i<surfaceCells_.size();++i){auto c=heatColor(counts[surfaceCells_[i]]/denominator,counts[surfaceCells_[i]]?155:65);mesh.colors[4*i]=c.r;mesh.colors[4*i+1]=c.g;mesh.colors[4*i+2]=c.b;mesh.colors[4*i+3]=c.a;}UpdateMeshBuffer(mesh,3,mesh.colors,mesh.vertexCount*4,0);surfaceCounts_=counts;surfaceDenominator_=denominator;}
}
void AnalysisRenderer::drawSurface()const{if(!surface_.meshCount)return;rlDisableBackfaceCulling();rlDisableDepthMask();DrawModel(surface_,{0,0,0},1,WHITE);rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();}
void AnalysisRenderer::drawSelectedProgress(const DisplayTrajectory&t,double time)const{for(size_t i=1;i<t.points.size();++i){auto a=t.points[i-1],b=t.points[i];if(a.time>=time)break;auto end=b.center;if(b.time>time)end=a.center+(b.center-a.center)*((time-a.time)/(b.time-a.time));DrawLine3D(vector(a.center),vector(end),WHITE);if(b.time>=time)break;}}
}
