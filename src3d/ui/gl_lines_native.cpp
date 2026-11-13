// Windows/OpenGL declarations are isolated here because they collide with raylib's names.
#include <Windows.h>
#include <GL/gl.h>
namespace rs3d::gui {
void nativeDrawLines(int count){glDrawArrays(GL_LINES,0,count);}
}
