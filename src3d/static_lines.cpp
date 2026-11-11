#include "static_lines.h"
#include <raymath.h>
#include <rlgl.h>
#include <stdexcept>
void drawNativeLineArray(int count);
void StaticLines::clear(){if(vbo_)rlUnloadVertexBuffer(vbo_);if(vao_)rlUnloadVertexArray(vao_);if(shader_.id)UnloadShader(shader_);vao_=vbo_=0;shader_={};count_=0;}
void StaticLines::load(const std::vector<Vector3>&points,int perLine){
    clear();if(perLine<2||points.empty()||points.size()%perLine)throw std::runtime_error("invalid static polyline layout");
    std::vector<Vector3>segments;segments.reserve(points.size()*2);
    for(size_t begin=0;begin<points.size();begin+=perLine)for(int i=1;i<perLine;++i){segments.push_back(points[begin+i-1]);segments.push_back(points[begin+i]);}
    count_=int(segments.size());
    shader_=LoadShaderFromMemory("#version 330\nlayout(location=0) in vec3 vertexPosition;uniform mat4 mvp;void main(){gl_Position=mvp*vec4(vertexPosition,1.0);}",
        "#version 330\nout vec4 finalColor;void main(){finalColor=vec4(235.0/255.0,190.0/255.0,95.0/255.0,1.0);}");
    if(!shader_.id||shader_.id==rlGetShaderIdDefault())throw std::runtime_error("static line shader compilation failed");
    mvp_=GetShaderLocation(shader_,"mvp");vao_=rlLoadVertexArray();rlEnableVertexArray(vao_);
    vbo_=rlLoadVertexBuffer(segments.data(),int(segments.size()*sizeof(Vector3)),false);
    rlSetVertexAttribute(0,3,RL_FLOAT,false,int(sizeof(Vector3)),0);rlEnableVertexAttribute(0);rlDisableVertexArray();rlDisableVertexBuffer();
    if(!vao_||!vbo_||mvp_<0)throw std::runtime_error("static line GPU allocation failed");
}
void StaticLines::draw()const{
    // Keep ordering/depth identical to DrawLine3D. No simplification or point loss.
    rlDrawRenderBatchActive();rlEnableShader(shader_.id);
    rlSetUniformMatrix(mvp_,MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));
    rlEnableVertexArray(vao_);drawNativeLineArray(count_);rlDisableVertexArray();rlDisableShader();
}

