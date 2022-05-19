#include <sdkddkver.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
	}
	return TRUE;
}

#define GL_GLEXT_PROTOTYPES
#include "../include/glcorearb.h"
#define WGL_WGLEXT_PROTOTYPES
#include "../include/wglext.h"

#include <assert.h>

static PFNGLDEBUGMESSAGECALLBACKPROC g_imp_glDebugMessageCallback = NULL;
void APIENTRY glDebugMessageCallback(GLDEBUGPROCARB callback, const void *userParam)
{
	return g_imp_glDebugMessageCallback(callback, userParam);
}

static PFNGLDEBUGMESSAGECONTROLPROC g_imp_glDebugMessageControl = NULL;
void APIENTRY glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled)
{
	return g_imp_glDebugMessageControl(source, type, severity, count, ids, enabled);
}

static PFNGLGENBUFFERSPROC g_imp_glGenBuffers = NULL;
void APIENTRY glGenBuffers(GLsizei n, GLuint *buffers)
{
	return g_imp_glGenBuffers(n, buffers);
}

static PFNGLBINDBUFFERPROC g_imp_glBindBuffer = NULL;
void APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
	return g_imp_glBindBuffer(target, buffer);
}

static PFNGLBUFFERDATAPROC g_imp_glBufferData = NULL;
void APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	return g_imp_glBufferData(target, size, data, usage);
}

static PFNGLBUFFERSTORAGEPROC g_imp_glBufferStorage = NULL;
void APIENTRY glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
	return g_imp_glBufferStorage(target, size, data, flags);
}

static PFNGLBUFFERSUBDATAPROC g_imp_glBufferSubData = NULL;
void APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
	return g_imp_glBufferSubData(target, offset, size, data);
}

static PFNGLUNMAPBUFFERPROC g_imp_glUnmapBuffer = NULL;
GLboolean APIENTRY glUnmapBuffer(GLenum target)
{
	return g_imp_glUnmapBuffer(target);
}

static PFNGLDELETEBUFFERSPROC g_imp_glDeleteBuffers = NULL;
void APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	return g_imp_glDeleteBuffers(n, buffers);
}

static PFNGLACTIVETEXTUREPROC g_imp_glActiveTexture = NULL;
void APIENTRY glActiveTexture(GLenum texture)
{
	return g_imp_glActiveTexture(texture);
}

static PFNGLTEXSTORAGE2DPROC g_imp_glTexStorage2D = NULL;
void APIENTRY glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	return g_imp_glTexStorage2D(target, levels, internalformat, width, height);
}

static PFNGLTEXSTORAGE2DMULTISAMPLEPROC g_imp_glTexStorage2DMultisample = NULL;
void APIENTRY glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	return g_imp_glTexStorage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
}

static PFNGLTEXSUBIMAGE2DPROC g_imp_glTexSubImage2D = NULL;
void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
	return g_imp_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

static PFNGLTEXSUBIMAGE3DPROC g_imp_glTexSubImage3D = NULL;
void APIENTRY glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
	return g_imp_glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

static PFNGLGENSAMPLERSPROC g_imp_glGenSamplers = NULL;
void APIENTRY glGenSamplers(GLsizei count, GLuint *samplers)
{

	return g_imp_glGenSamplers(count, samplers);
}

static PFNGLSAMPLERPARAMETERIPROC g_imp_glSamplerParameteri = NULL;
void APIENTRY glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
	return g_imp_glSamplerParameteri(sampler, pname, param);
}

static PFNGLSAMPLERPARAMETERFVPROC g_imp_glSamplerParameterfv = NULL;
void APIENTRY glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param)
{
	return g_imp_glSamplerParameterfv(sampler, pname, param);
}

static PFNGLBINDSAMPLERPROC g_imp_glBindSampler = NULL;
void APIENTRY glBindSampler(GLuint unit, GLuint sampler)
{
	return g_imp_glBindSampler(unit, sampler);
}

static PFNGLGENFRAMEBUFFERSPROC g_imp_glGenFramebuffers = NULL;
void APIENTRY glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	return g_imp_glGenFramebuffers(n, framebuffers);
}

static PFNGLBINDFRAMEBUFFERPROC g_imp_glBindFramebuffer = NULL;
void APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	return g_imp_glBindFramebuffer(target, framebuffer);
}

static PFNGLFRAMEBUFFERTEXTURE2DPROC g_imp_glFramebufferTexture2D = NULL;
void APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	return g_imp_glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

static PFNGLDRAWBUFFERSPROC g_imp_glDrawBuffers = NULL;
void APIENTRY glDrawBuffers(GLsizei n, const GLenum *bufs)
{
	return g_imp_glDrawBuffers(n, bufs);
}

static PFNGLCHECKFRAMEBUFFERSTATUSPROC g_imp_glCheckFramebufferStatus = NULL;
GLenum APIENTRY glCheckFramebufferStatus(GLenum target)
{
	return g_imp_glCheckFramebufferStatus(target);
}

static PFNGLCREATESHADERPROC g_imp_glCreateShader = NULL;
GLuint APIENTRY glCreateShader(GLenum type)
{
	return g_imp_glCreateShader(type);
}

static PFNGLSHADERBINARYPROC g_imp_glShaderBinary = NULL;
void APIENTRY glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length)
{
	return g_imp_glShaderBinary(count, shaders, binaryformat, binary, length);
}

static PFNGLSPECIALIZESHADERPROC g_imp_glSpecializeShader = NULL;
void APIENTRY glSpecializeShader(GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue)
{
	return g_imp_glSpecializeShader(shader, pEntryPoint, numSpecializationConstants, pConstantIndex, pConstantValue);
}

static PFNGLSHADERSOURCEPROC g_imp_glShaderSource = NULL;
void APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)
{
	return g_imp_glShaderSource(shader, count, string, length);
}
static PFNGLCOMPILESHADERPROC g_imp_glCompileShader = NULL;
void APIENTRY glCompileShader(GLuint shader)
{
	return g_imp_glCompileShader(shader);
}
static PFNGLGETSHADERIVPROC g_imp_glGetShaderiv = NULL;
void APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
	return g_imp_glGetShaderiv(shader, pname, params);
}

static PFNGLGETSHADERINFOLOGPROC g_imp_glGetShaderInfoLog = NULL;
void APIENTRY glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	return g_imp_glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

static PFNGLCREATEPROGRAMPROC g_imp_glCreateProgram = NULL;
GLuint APIENTRY glCreateProgram(void)
{
	return g_imp_glCreateProgram();
}

static PFNGLATTACHSHADERPROC g_imp_glAttachShader = NULL;
void APIENTRY glAttachShader(GLuint program, GLuint shader)
{
	return g_imp_glAttachShader(program, shader);
}

static PFNGLLINKPROGRAMPROC g_imp_glLinkProgram = NULL;
void APIENTRY glLinkProgram(GLuint program)
{
	return g_imp_glLinkProgram(program);
}

static PFNGLGETPROGRAMIVPROC g_imp_glGetProgramiv = NULL;
void APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
	return g_imp_glGetProgramiv(program, pname, params);
}

static PFNGLGETPROGRAMINFOLOGPROC g_imp_glGetProgramInfoLog = NULL;
void APIENTRY glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	return g_imp_glGetProgramInfoLog(program, bufSize, length, infoLog);
}

static PFNGLUSEPROGRAMPROC g_imp_glUseProgram = NULL;
void APIENTRY glUseProgram(GLuint program)
{
	return g_imp_glUseProgram(program);
}

static PFNGLDELETEPROGRAMPROC g_imp_glDeleteProgram = NULL;
void APIENTRY glDeleteProgram (GLuint program)
{
	g_imp_glDeleteProgram(program);
}

static PFNGLBINDBUFFERBASEPROC g_imp_glBindBufferBase = NULL;
void APIENTRY glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	return g_imp_glBindBufferBase(target, index, buffer);
}

static PFNGLGENVERTEXARRAYSPROC g_imp_glGenVertexArrays = NULL;
void APIENTRY glGenVertexArrays(GLsizei n, GLuint *arrays)
{
	return g_imp_glGenVertexArrays(n, arrays);
}

static PFNGLBINDVERTEXARRAYPROC g_imp_glBindVertexArray = NULL;
void APIENTRY glBindVertexArray(GLuint array)
{
	return g_imp_glBindVertexArray(array);
}

static PFNGLVERTEXATTRIBFORMATPROC g_imp_glVertexAttribFormat = NULL;
void APIENTRY glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
	g_imp_glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
}

static PFNGLENABLEVERTEXATTRIBARRAYPROC g_imp_glEnableVertexAttribArray = NULL;
void APIENTRY glEnableVertexAttribArray(GLuint index)
{
	return g_imp_glEnableVertexAttribArray(index);
}

static PFNGLVERTEXATTRIBBINDINGPROC g_imp_glVertexAttribBinding = NULL;
void APIENTRY glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
	return g_imp_glVertexAttribBinding(attribindex, bindingindex);
}

static PFNGLBINDVERTEXBUFFERPROC g_imp_glBindVertexBuffer = NULL;
void APIENTRY glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
	return g_imp_glBindVertexBuffer(bindingindex, buffer, offset, stride);
}

static PFNGLPRIMITIVERESTARTINDEXPROC g_imp_glPrimitiveRestartIndex = NULL;
void APIENTRY glPrimitiveRestartIndex(GLuint index)
{
	return g_imp_glPrimitiveRestartIndex(index);
}

static PFNGLVIEWPORTARRAYVPROC g_imp_glViewportArrayv = NULL;
void APIENTRY glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v)
{
	return g_imp_glViewportArrayv(first, count, v);
}

static PFNGLCLIPCONTROLPROC g_imp_glClipControl = NULL;
void APIENTRY glClipControl(GLenum origin, GLenum depth)
{
	return g_imp_glClipControl(origin, depth);
}

static PFNGLPOLYGONOFFSETCLAMPPROC g_imp_glPolygonOffsetClamp = NULL;
void APIENTRY glPolygonOffsetClamp(GLfloat factor, GLfloat units, GLfloat clamp)
{
	return g_imp_glPolygonOffsetClamp(factor, units, clamp);
}

static PFNGLSTENCILFUNCSEPARATEPROC g_imp_glStencilFuncSeparate = NULL;
void APIENTRY glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
	return g_imp_glStencilFuncSeparate(face, func, ref, mask);
}

static PFNGLSTENCILOPSEPARATEPROC g_imp_glStencilOpSeparate = NULL;
void APIENTRY glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
	return g_imp_glStencilOpSeparate(face, sfail, dpfail, dppass);
}

static PFNGLCOLORMASKIPROC g_imp_glColorMaski = NULL;
void APIENTRY glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
	return g_imp_glColorMaski(index, r, g, b, a);
}

static PFNGLCLEARBUFFERFVPROC g_imp_glClearBufferfv = NULL;
void APIENTRY glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	return g_imp_glClearBufferfv(buffer, drawbuffer, value);
}

static PFNGLBLITFRAMEBUFFERPROC g_imp_glBlitFramebuffer = NULL;
void APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
	return g_imp_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

static PFNWGLSWAPINTERVALEXTPROC g_imp_wglSwapIntervalEXT = NULL;
BOOL WINAPI wglSwapIntervalEXT(int interval)
{
	return g_imp_wglSwapIntervalEXT(interval);
}

static PFNWGLCHOOSEPIXELFORMATARBPROC g_imp_wglChoosePixelFormatARB = NULL;
BOOL WINAPI wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
	return g_imp_wglChoosePixelFormatARB(hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats);
}

static PFNWGLCREATECONTEXTATTRIBSARBPROC g_imp_wglCreateContextAttribsARB = NULL;
HGLRC WINAPI wglCreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int *attribList)
{
	return g_imp_wglCreateContextAttribsARB(hDC, hShareContext, attribList);
}

static PFNWGLMAKECONTEXTCURRENTARBPROC g_imp_wglMakeContextCurrentARB = NULL;
BOOL WINAPI wglMakeContextCurrentARB(HDC hDrawDC, HDC hReadDC, HGLRC hglrc)
{
#ifndef NDEBUG
	// RenderDoc不支持wglMakeContextCurrentARB
	assert(hDrawDC == hReadDC);
	return wglMakeCurrent(hDrawDC, hglrc);
#else
	return g_imp_wglMakeContextCurrentARB(hDrawDC, hReadDC, hglrc);
#endif
}

extern "C" void APIENTRY init_libgl(void)
{
	g_imp_glDebugMessageCallback = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKPROC>(wglGetProcAddress("glDebugMessageCallback"));
	if (g_imp_glDebugMessageCallback == NULL)
	{
		g_imp_glDebugMessageCallback = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKARBPROC>(wglGetProcAddress("glDebugMessageCallbackARB"));
	}

	g_imp_glDebugMessageControl = reinterpret_cast<PFNGLDEBUGMESSAGECONTROLPROC>(wglGetProcAddress("glDebugMessageControl"));
	if (g_imp_glDebugMessageControl == NULL)
	{
		g_imp_glDebugMessageControl = reinterpret_cast<PFNGLDEBUGMESSAGECONTROLARBPROC>(wglGetProcAddress("glDebugMessageControlARB"));
	}

	g_imp_glGenBuffers = reinterpret_cast<PFNGLGENBUFFERSPROC>(wglGetProcAddress("glGenBuffers"));
	g_imp_glBindBuffer = reinterpret_cast<PFNGLBINDBUFFERPROC>(wglGetProcAddress("glBindBuffer"));
	g_imp_glBufferData = reinterpret_cast<PFNGLBUFFERDATAPROC>(wglGetProcAddress("glBufferData"));
	g_imp_glBufferStorage = reinterpret_cast<PFNGLBUFFERSTORAGEPROC>(wglGetProcAddress("glBufferStorage"));
	g_imp_glBufferSubData = reinterpret_cast<PFNGLBUFFERSUBDATAPROC>(wglGetProcAddress("glBufferSubData"));
	g_imp_glUnmapBuffer = reinterpret_cast<PFNGLUNMAPBUFFERPROC>(wglGetProcAddress("glUnmapBuffer"));
	g_imp_glDeleteBuffers = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(wglGetProcAddress("glDeleteBuffers"));

	g_imp_glActiveTexture = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(wglGetProcAddress("glActiveTexture"));
	g_imp_glTexStorage2D = reinterpret_cast<PFNGLTEXSTORAGE2DPROC>(wglGetProcAddress("glTexStorage2D"));
	g_imp_glTexStorage2DMultisample = reinterpret_cast<PFNGLTEXSTORAGE2DMULTISAMPLEPROC>(wglGetProcAddress("glTexStorage2DMultisample"));
	g_imp_glTexSubImage2D = reinterpret_cast<PFNGLTEXSUBIMAGE2DPROC>(wglGetProcAddress("glTexSubImage2D"));
	g_imp_glTexSubImage3D = reinterpret_cast<PFNGLTEXSUBIMAGE3DPROC>(wglGetProcAddress("glTexSubImage3D"));

	g_imp_glGenSamplers = reinterpret_cast<PFNGLGENSAMPLERSPROC>(wglGetProcAddress("glGenSamplers"));
	g_imp_glSamplerParameteri = reinterpret_cast<PFNGLSAMPLERPARAMETERIPROC>(wglGetProcAddress("glSamplerParameteri"));
	g_imp_glSamplerParameterfv = reinterpret_cast<PFNGLSAMPLERPARAMETERFVPROC>(wglGetProcAddress("glSamplerParameterfv"));
	g_imp_glBindSampler = reinterpret_cast<PFNGLBINDSAMPLERPROC>(wglGetProcAddress("glBindSampler"));

	g_imp_glGenFramebuffers = reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(wglGetProcAddress("glGenFramebuffers"));
	g_imp_glBindFramebuffer = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(wglGetProcAddress("glBindFramebuffer"));
	g_imp_glFramebufferTexture2D = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DPROC>(wglGetProcAddress("glFramebufferTexture2D"));
	g_imp_glDrawBuffers = reinterpret_cast<PFNGLDRAWBUFFERSPROC>(wglGetProcAddress("glDrawBuffers"));
	g_imp_glCheckFramebufferStatus = reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(wglGetProcAddress("glCheckFramebufferStatus"));

	g_imp_glCreateShader = reinterpret_cast<PFNGLCREATESHADERPROC>(wglGetProcAddress("glCreateShader"));
	g_imp_glShaderBinary = reinterpret_cast<PFNGLSHADERBINARYPROC>(wglGetProcAddress("glShaderBinary"));

	g_imp_glSpecializeShader = reinterpret_cast<PFNGLSPECIALIZESHADERPROC>(wglGetProcAddress("glSpecializeShader"));

	g_imp_glShaderSource = reinterpret_cast<PFNGLSHADERSOURCEPROC>(wglGetProcAddress("glShaderSource"));
	g_imp_glCompileShader = reinterpret_cast<PFNGLCOMPILESHADERPROC>(wglGetProcAddress("glCompileShader"));
	g_imp_glGetShaderiv = reinterpret_cast<PFNGLGETSHADERIVPROC>(wglGetProcAddress("glGetShaderiv"));
	g_imp_glGetShaderInfoLog = reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(wglGetProcAddress("glGetShaderInfoLog"));

	g_imp_glCreateProgram = reinterpret_cast<PFNGLCREATEPROGRAMPROC>(wglGetProcAddress("glCreateProgram"));
	g_imp_glAttachShader = reinterpret_cast<PFNGLATTACHSHADERPROC>(wglGetProcAddress("glAttachShader"));
	g_imp_glLinkProgram = reinterpret_cast<PFNGLLINKPROGRAMPROC>(wglGetProcAddress("glLinkProgram"));
	g_imp_glGetProgramiv = reinterpret_cast<PFNGLGETPROGRAMIVPROC>(wglGetProcAddress("glGetProgramiv"));
	g_imp_glGetProgramInfoLog = reinterpret_cast<PFNGLGETPROGRAMINFOLOGPROC>(wglGetProcAddress("glGetProgramInfoLog"));
	g_imp_glUseProgram = reinterpret_cast<PFNGLUSEPROGRAMPROC>(wglGetProcAddress("glUseProgram"));
	g_imp_glDeleteProgram = reinterpret_cast<PFNGLDELETEPROGRAMPROC>(wglGetProcAddress("glDeleteProgram"));

	g_imp_glBindBufferBase = reinterpret_cast<PFNGLBINDBUFFERBASEPROC>(wglGetProcAddress("glBindBufferBase"));

	g_imp_glGenVertexArrays = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(wglGetProcAddress("glGenVertexArrays"));
	g_imp_glBindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(wglGetProcAddress("glBindVertexArray"));
	g_imp_glVertexAttribFormat = reinterpret_cast<PFNGLVERTEXATTRIBFORMATPROC>(wglGetProcAddress("glVertexAttribFormat"));
	g_imp_glEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(wglGetProcAddress("glEnableVertexAttribArray"));
	g_imp_glVertexAttribBinding = reinterpret_cast<PFNGLVERTEXATTRIBBINDINGPROC>(wglGetProcAddress("glVertexAttribBinding"));
	g_imp_glBindVertexBuffer = reinterpret_cast<PFNGLBINDVERTEXBUFFERPROC>(wglGetProcAddress("glBindVertexBuffer"));

	g_imp_glPrimitiveRestartIndex = reinterpret_cast<PFNGLPRIMITIVERESTARTINDEXPROC>(wglGetProcAddress("glPrimitiveRestartIndex"));

	g_imp_glClipControl = reinterpret_cast<PFNGLCLIPCONTROLPROC>(wglGetProcAddress("glClipControl"));
	g_imp_glViewportArrayv = reinterpret_cast<PFNGLVIEWPORTARRAYVPROC>(wglGetProcAddress("glViewportArrayv"));

	g_imp_glPolygonOffsetClamp = reinterpret_cast<PFNGLPOLYGONOFFSETCLAMPPROC>(wglGetProcAddress("glPolygonOffsetClamp"));

	g_imp_glStencilFuncSeparate = reinterpret_cast<PFNGLSTENCILFUNCSEPARATEPROC>(wglGetProcAddress("glStencilFuncSeparate"));
	g_imp_glStencilOpSeparate = reinterpret_cast<PFNGLSTENCILOPSEPARATEPROC>(wglGetProcAddress("glStencilOpSeparate"));
	g_imp_glColorMaski = reinterpret_cast<PFNGLCOLORMASKIPROC>(wglGetProcAddress("glColorMaski"));

	g_imp_glClearBufferfv = reinterpret_cast<PFNGLCLEARBUFFERFVPROC>(wglGetProcAddress("glClearBufferfv"));

	g_imp_glBlitFramebuffer = reinterpret_cast<PFNGLBLITFRAMEBUFFERPROC>(wglGetProcAddress("glBlitFramebuffer"));

	g_imp_wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));

	g_imp_wglMakeContextCurrentARB = reinterpret_cast<PFNWGLMAKECONTEXTCURRENTEXTPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));

	g_imp_wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));

	g_imp_wglMakeContextCurrentARB = reinterpret_cast<PFNWGLMAKECONTEXTCURRENTARBPROC>(wglGetProcAddress("wglMakeContextCurrentARB"));

	return;
}