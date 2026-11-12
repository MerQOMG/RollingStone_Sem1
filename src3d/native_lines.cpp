// OpenGL 1.1 glDrawArrays is supplied by Windows opengl32 (already used by raylib).
// Keep Windows/GDI declarations isolated from raylib's identically named APIs.
#include <Windows.h>
#include <GL/gl.h>
void drawNativeLineArray(int count){glDrawArrays(GL_LINES,0,count);}
