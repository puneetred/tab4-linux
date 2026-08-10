/* egl-test.c — Stage 4: EGL + GLES2 offscreen render on the device.
 * Bionic-linked executable: dlopen libEGL_MRVL.so + libGLESv2_MRVL.so,
 * pbuffer surface (no gralloc/hwcomposer needed), glClear + glReadPixels.
 * No stdio: prints via raw write() so it works without bionic crt init.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define FB_W 800
#define FB_H 1280
#define FB_BYTES (FB_W * FB_H * 4)

typedef int32_t EGLint;
typedef void*   EGLDisplay;
typedef void*   EGLConfig;
typedef void*   EGLContext;
typedef void*   EGLSurface;
typedef void*   EGLNativeDisplayType;

#define EGL_DEFAULT_DISPLAY ((void*)0)
#define EGL_NO_DISPLAY      ((void*)0)
#define EGL_NO_CONTEXT      ((void*)0)
#define EGL_NO_SURFACE      ((void*)0)
#define EGL_DONT_CARE       (-1)

#define EGL_SUCCESS             0x3000
#define EGL_NOT_INITIALIZED     0x3001
#define EGL_BAD_ACCESS          0x3002
#define EGL_BAD_ALLOC           0x3003
#define EGL_BAD_ATTRIBUTE       0x3004
#define EGL_BAD_CONFIG          0x3005
#define EGL_BAD_CONTEXT         0x3006
#define EGL_BAD_CURRENT_SURFACE 0x3007
#define EGL_BAD_DISPLAY         0x3008
#define EGL_BAD_MATCH           0x3009
#define EGL_BAD_NATIVE_PIXMAP   0x300a
#define EGL_BAD_NATIVE_WINDOW   0x300b
#define EGL_BAD_PARAMETER       0x300c
#define EGL_BAD_SURFACE         0x300d

#define EGL_ALPHA_SIZE          0x3021
#define EGL_BLUE_SIZE           0x3022
#define EGL_GREEN_SIZE          0x3023
#define EGL_RED_SIZE            0x3024
#define EGL_SURFACE_TYPE        0x3033
#define EGL_PBUFFER_BIT         0x0001
#define EGL_WINDOW_BIT          0x0004
#define EGL_WIDTH               0x3057
#define EGL_HEIGHT              0x3056
#define EGL_LARGEST_PBUFFER     0x3053
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_NONE                0x3038
#define EGL_VENDOR              0x3053
#define EGL_VERSION             0x3054
#define EGL_EXTENSIONS          0x3055

#define GL_COLOR_BUFFER_BIT     0x4000
#define GL_RGBA                 0x1908
#define GL_UNSIGNED_BYTE        0x1401

typedef EGLDisplay (*fn_eglGetDisplay)(EGLNativeDisplayType d);
typedef EGLint (*fn_eglInitialize)(EGLDisplay d, EGLint* maj, EGLint* min);
typedef const char* (*fn_eglQueryString)(EGLDisplay d, EGLint name);
typedef EGLint (*fn_eglChooseConfig)(EGLDisplay d, const EGLint* attrs, EGLConfig* out, EGLint n, EGLint* got);
typedef EGLContext (*fn_eglCreateContext)(EGLDisplay d, EGLConfig c, EGLContext share, const EGLint* attrs);
typedef EGLSurface (*fn_eglCreatePbufferSurface)(EGLDisplay d, EGLConfig c, const EGLint* attrs);
typedef EGLint (*fn_eglGetConfigAttrib)(EGLDisplay d, EGLConfig c, EGLint attr, EGLint* v);
typedef EGLint (*fn_eglMakeCurrent)(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext ctx);
typedef EGLint (*fn_eglSwapBuffers)(EGLDisplay d, EGLSurface s);
typedef EGLint (*fn_eglTerminate)(EGLDisplay d);
typedef EGLint (*fn_eglGetError)(void);

typedef void (*fn_glViewport)(int32_t x, int32_t y, int32_t w, int32_t h);
typedef void (*fn_glClearColor)(float r, float g, float b, float a);
typedef void (*fn_glClear)(uint32_t mask);
typedef void (*fn_glFinish)(void);
typedef uint32_t (*fn_glGetError)(void);
typedef void (*fn_glReadPixels)(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t fmt, uint32_t type, void* px);
typedef uint32_t (*fn_glCreateShader)(uint32_t type);
typedef void (*fn_glShaderSource)(uint32_t sh, int32_t n, const char* const* src, const int32_t* len);
typedef void (*fn_glCompileShader)(uint32_t sh);
typedef uint32_t (*fn_glCreateProgram)(void);
typedef void (*fn_glAttachShader)(uint32_t prog, uint32_t sh);
typedef void (*fn_glLinkProgram)(uint32_t prog);
typedef void (*fn_glUseProgram)(uint32_t prog);
typedef void (*fn_glGetShaderiv)(uint32_t sh, uint32_t pname, int32_t* v);
typedef void (*fn_glGetProgramiv)(uint32_t prog, uint32_t pname, int32_t* v);
typedef int32_t (*fn_glGetAttribLocation)(uint32_t prog, const char* name);
typedef void (*fn_glVertexAttribPointer)(uint32_t idx, int32_t size, uint32_t type, uint32_t norm, int32_t stride, const void* ptr);
typedef void (*fn_glEnableVertexAttribArray)(uint32_t idx);
typedef void (*fn_glDrawArrays)(uint32_t mode, int32_t first, int32_t count);

#define GL_VERTEX_SHADER 0x8b31
#define GL_FRAGMENT_SHADER 0x8b30
#define GL_COMPILE_STATUS 0x8b81
#define GL_LINK_STATUS 0x8b82
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 0x0004

static void out(const char* s)
{
    int l = 0;
    while (s[l]) l++;
    asm volatile(
        "mov r7, #4\n"
        "mov r0, #1\n"
        "mov r1, %0\n"
        "mov r2, %1\n"
        "svc 0\n"
        :: "r"(s), "r"(l) : "r7", "r0", "r1", "r2", "memory");
}

static void hex32(const char* tag, uint32_t v)
{
    char b[64];
    int i = 0;
    static const char* hx = "0123456789abcdef";
    const char* t = tag;
    while (*t) b[i++] = *t++;
    b[i++] = '=';
    b[i++] = '0';
    b[i++] = 'x';
    for (int sh = 28; sh >= 0; sh -= 4) b[i++] = hx[(v >> sh) & 0xf];
    b[i++] = '\n';
    asm volatile(
        "mov r7, #4\n"
        "mov r0, #1\n"
        "mov r1, %0\n"
        "mov r2, %1\n"
        "svc 0\n"
        :: "r"(b), "r"(i) : "r7", "r0", "r1", "r2", "memory");
}

static void dec32(const char* tag, uint32_t v)
{
    char b[64];
    int i = 0;
    const char* t = tag;
    while (*t) b[i++] = *t++;
    b[i++] = '=';
    char rev[16];
    int n = 0;
    if (v == 0) rev[n++] = '0';
    while (v) { rev[n++] = '0' + (v % 10); v /= 10; }
    while (n) b[i++] = rev[--n];
    b[i++] = '\n';
    asm volatile(
        "mov r7, #4\n"
        "mov r0, #1\n"
        "mov r1, %0\n"
        "mov r2, %1\n"
        "svc 0\n"
        :: "r"(b), "r"(i) : "r7", "r0", "r1", "r2", "memory");
}

int main(void)
{
    out("== egl-test: bionic libEGL_MRVL offscreen render ==\n");
    void* e = dlopen("/system/lib/egl/libEGL_MRVL.so", RTLD_NOW);
    if (!e) {
        out("dlopen libEGL_MRVL.so FAILED: ");
        out(dlerror());
        out("\n");
        return 1;
    }
    out("dlopen libEGL_MRVL.so OK\n");

    fn_eglGetDisplay getDisplay = (fn_eglGetDisplay)dlsym(e, "eglGetDisplay");
    fn_eglInitialize init = (fn_eglInitialize)dlsym(e, "eglInitialize");
    fn_eglQueryString queryString = (fn_eglQueryString)dlsym(e, "eglQueryString");
    fn_eglChooseConfig chooseConfig = (fn_eglChooseConfig)dlsym(e, "eglChooseConfig");
    fn_eglCreateContext createContext = (fn_eglCreateContext)dlsym(e, "eglCreateContext");
    fn_eglCreatePbufferSurface createPbuffer = (fn_eglCreatePbufferSurface)dlsym(e, "eglCreatePbufferSurface");
    fn_eglMakeCurrent makeCurrent = (fn_eglMakeCurrent)dlsym(e, "eglMakeCurrent");
    fn_eglSwapBuffers swapBuffers = (fn_eglSwapBuffers)dlsym(e, "eglSwapBuffers");
    fn_eglTerminate terminate = (fn_eglTerminate)dlsym(e, "eglTerminate");
    fn_eglGetError getError = (fn_eglGetError)dlsym(e, "eglGetError");
    if (!getDisplay || !init || !queryString || !chooseConfig || !createContext ||
        !createPbuffer || !makeCurrent || !swapBuffers || !terminate || !getError) {
        out("dlsym EGL FAIL\n");
        return 2;
    }
    fn_eglGetConfigAttrib getConfigAttrib = (fn_eglGetConfigAttrib)dlsym(e, "eglGetConfigAttrib");
    if (!getConfigAttrib) { out("dlsym eglGetConfigAttrib FAIL\n"); return 2; }
    out("dlsym EGL OK\n");

    void* g = dlopen("/system/lib/egl/libGLESv2_MRVL.so", RTLD_NOW);
    if (!g) { out("dlopen libGLESv2_MRVL.so FAILED\n"); return 3; }
    fn_glViewport glViewport = (fn_glViewport)dlsym(g, "glViewport");
    fn_glClearColor glClearColor = (fn_glClearColor)dlsym(g, "glClearColor");
    fn_glClear glClear = (fn_glClear)dlsym(g, "glClear");
    fn_glFinish glFinish = (fn_glFinish)dlsym(g, "glFinish");
    fn_glGetError glGetError = (fn_glGetError)dlsym(g, "glGetError");
    fn_glReadPixels glReadPixels = (fn_glReadPixels)dlsym(g, "glReadPixels");
    fn_glCreateShader createShader = (fn_glCreateShader)dlsym(g, "glCreateShader");
    fn_glShaderSource shaderSource = (fn_glShaderSource)dlsym(g, "glShaderSource");
    fn_glCompileShader compileShader = (fn_glCompileShader)dlsym(g, "glCompileShader");
    fn_glCreateProgram createProgram = (fn_glCreateProgram)dlsym(g, "glCreateProgram");
    fn_glAttachShader attachShader = (fn_glAttachShader)dlsym(g, "glAttachShader");
    fn_glLinkProgram linkProgram = (fn_glLinkProgram)dlsym(g, "glLinkProgram");
    fn_glUseProgram useProgram = (fn_glUseProgram)dlsym(g, "glUseProgram");
    fn_glGetShaderiv getShaderiv = (fn_glGetShaderiv)dlsym(g, "glGetShaderiv");
    fn_glGetProgramiv getProgramiv = (fn_glGetProgramiv)dlsym(g, "glGetProgramiv");
    fn_glGetAttribLocation getAttribLocation = (fn_glGetAttribLocation)dlsym(g, "glGetAttribLocation");
    fn_glVertexAttribPointer vertexAttribPointer = (fn_glVertexAttribPointer)dlsym(g, "glVertexAttribPointer");
    fn_glEnableVertexAttribArray enableVertexAttribArray = (fn_glEnableVertexAttribArray)dlsym(g, "glEnableVertexAttribArray");
    fn_glDrawArrays drawArrays = (fn_glDrawArrays)dlsym(g, "glDrawArrays");
    if (!glViewport || !glClearColor || !glClear || !glFinish || !glGetError || !glReadPixels ||
        !createShader || !shaderSource || !compileShader || !createProgram || !attachShader ||
        !linkProgram || !useProgram || !getShaderiv || !getProgramiv || !getAttribLocation ||
        !vertexAttribPointer || !enableVertexAttribArray || !drawArrays) {
        out("dlsym GL FAIL\n");
        return 4;
    }
    out("dlsym GL OK\n");

    EGLDisplay dpy = getDisplay(EGL_DEFAULT_DISPLAY);
    hex32("eglGetDisplay", (uint32_t)dpy);
    if (dpy == EGL_NO_DISPLAY) { out("FAIL: no display\n"); return 5; }

    EGLint maj, min;
    EGLint err = init(dpy, &maj, &min);
    hex32("eglInitialize", (uint32_t)err);
    if (err != 1) { hex32("eglGetError", (uint32_t)getError()); return 6; }
    dec32("major", (uint32_t)maj);
    dec32("minor", (uint32_t)min);
    out("eglInitialize OK\n");

    out("eglQueryString VERSION: ");
    out(queryString(dpy, EGL_VERSION));
    out("\nVENDOR: ");
    out(queryString(dpy, EGL_VENDOR));
    out("\nEXTENSIONS: ");
    out(queryString(dpy, EGL_EXTENSIONS));
    out("\n");

    static const EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint got = 0;
    err = chooseConfig(dpy, cfgAttrs, &cfg, 1, &got);
    hex32("eglChooseConfig", (uint32_t)err);
    dec32("configs", (uint32_t)got);
    if (err != 1 || got < 1) { hex32("eglGetError", (uint32_t)getError()); return 7; }
    out("eglChooseConfig OK\n");
    EGLint ca;
    getConfigAttrib(dpy, cfg, 0x3024, &ca); dec32("config red", (uint32_t)ca);
    getConfigAttrib(dpy, cfg, 0x3023, &ca); dec32("config green", (uint32_t)ca);
    getConfigAttrib(dpy, cfg, 0x3022, &ca); dec32("config blue", (uint32_t)ca);
    getConfigAttrib(dpy, cfg, 0x3021, &ca); dec32("config alpha", (uint32_t)ca);

    static const EGLint ctxAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext ctx = createContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttrs);
    hex32("eglCreateContext", (uint32_t)ctx);
    if (!ctx) { hex32("eglGetError", (uint32_t)getError()); return 8; }

    static const EGLint surfAttrs[] = { EGL_WIDTH, 8, EGL_HEIGHT, 8, EGL_NONE };
    EGLSurface surf = createPbuffer(dpy, cfg, surfAttrs);
    hex32("eglCreatePbufferSurface", (uint32_t)surf);
    if (!surf) { hex32("eglGetError", (uint32_t)getError()); return 9; }

    err = makeCurrent(dpy, surf, surf, ctx);
    hex32("eglMakeCurrent", (uint32_t)err);
    if (err != 1) { hex32("eglGetError", (uint32_t)getError()); return 10; }
    out("makeCurrent OK\n");

    glViewport(0, 0, 8, 8);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    hex32("glGetError after clear", (uint32_t)glGetError());

    err = swapBuffers(dpy, surf);
    hex32("eglSwapBuffers", (uint32_t)err);

    static uint8_t px[8 * 8 * 4];
    glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, px);
    hex32("glGetError after readpixels", (uint32_t)glGetError());

    int bad = 0;
    int firstBad = 0;
    for (int i = 0; i < 8 * 8; i++) {
        if (px[4*i] < 200 || px[4*i+3] < 200) {
            if (!bad) firstBad = i;
            bad++;
        }
    }
    if (bad == 0) out("PIXELS OK: all 64 samples red\n");
    else {
        out("PIXELS FAIL: ");
        dec32("bad", (uint32_t)bad);
        hex32("firstBadR", px[4*firstBad]);
        hex32("firstBadA", px[4*firstBad+3]);
    }

    out("-- triangle phase --\n");
    static const char* vs = "attribute vec2 pos; void main() { gl_Position = vec4(pos, 0.0, 1.0); }";
    static const char* fs = "precision mediump float; void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }";
    uint32_t vsh = createShader(GL_VERTEX_SHADER);
    hex32("glCreateShader(VS)", (uint32_t)vsh);
    shaderSource(vsh, 1, &vs, 0);
    compileShader(vsh);
    int32_t ok = 0;
    getShaderiv(vsh, GL_COMPILE_STATUS, &ok);
    dec32("shader compile", (uint32_t)ok);
    uint32_t fsh = createShader(GL_FRAGMENT_SHADER);
    shaderSource(fsh, 1, &fs, 0);
    compileShader(fsh);
    getShaderiv(fsh, GL_COMPILE_STATUS, &ok);
    dec32("frag compile", (uint32_t)ok);
    uint32_t prog = createProgram();
    attachShader(prog, vsh);
    attachShader(prog, fsh);
    linkProgram(prog);
    getProgramiv(prog, GL_LINK_STATUS, &ok);
    dec32("link status", (uint32_t)ok);
    useProgram(prog);
    int32_t posLoc = getAttribLocation(prog, "pos");
    dec32("pos loc", (uint32_t)posLoc);
    static const float verts[6] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
    vertexAttribPointer((uint32_t)posLoc, 2, GL_FLOAT, 0, 0, verts);
    enableVertexAttribArray((uint32_t)posLoc);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawArrays(GL_TRIANGLES, 0, 3);
    glFinish();
    hex32("glGetError after triangle", (uint32_t)glGetError());
    glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, px);
    hex32("glGetError after tri readpixels", (uint32_t)glGetError());
    int greenBad = 0;
    for (int i = 0; i < 8 * 8; i++)
        if (px[4*i+1] < 200 || px[4*i+3] < 200) greenBad++;
    if (greenBad == 0) out("TRIANGLE PIXELS OK: all 64 samples green\n");
    else {
        out("TRIANGLE PIXELS FAIL: ");
        dec32("bad", (uint32_t)greenBad);
        hex32("firstG", px[4*1]);
        hex32("firstR", px[4*0]);
        hex32("firstA", px[4*3]);
    }

    out("-- clear after first draw --\n");
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, px);
    int red2 = 0;
    for (int i = 0; i < 8 * 8; i++)
        if (px[4*i] >= 200 && px[4*i+3] >= 200) red2++;
    if (red2 == 64) out("POST-DRAW CLEAR OK: all 64 samples red\n");
    else {
        out("POST-DRAW CLEAR FAIL: ");
        dec32("red", (uint32_t)red2);
        hex32("firstR", px[4*0]);
    }

    out("-- fullscreen fb phase --\n");
    static const EGLint bigAttrs[] = { EGL_WIDTH, FB_W, EGL_HEIGHT, FB_H, EGL_NONE };
    EGLSurface fbsurf = createPbuffer(dpy, cfg, bigAttrs);
    hex32("eglCreatePbufferSurface(fb)", (uint32_t)fbsurf);
    if (!fbsurf) { hex32("eglGetError", (uint32_t)getError()); out("FB SURFACE FAIL\n"); return 12; }
    err = makeCurrent(dpy, fbsurf, fbsurf, ctx);
    if (err != 1) { hex32("eglGetError", (uint32_t)getError()); out("FB makeCurrent FAIL\n"); return 13; }

    static const char* gvs = "attribute vec2 pos; varying vec2 vp; void main(){ vp = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0.0, 1.0); }";
    static const char* gfs = "precision mediump float; varying vec2 vp; void main(){ gl_FragColor = vec4(vp.x, vp.y, 0.5, 1.0); }";
    uint32_t gvsH = createShader(GL_VERTEX_SHADER);
    shaderSource(gvsH, 1, &gvs, 0);
    compileShader(gvsH);
    uint32_t gfsH = createShader(GL_FRAGMENT_SHADER);
    shaderSource(gfsH, 1, &gfs, 0);
    compileShader(gfsH);
    uint32_t gprog = createProgram();
    attachShader(gprog, gvsH);
    attachShader(gprog, gfsH);
    linkProgram(gprog);
    getProgramiv(gprog, GL_LINK_STATUS, &ok);
    dec32("grad link", (uint32_t)ok);
    useProgram(gprog);
    posLoc = getAttribLocation(gprog, "pos");
    vertexAttribPointer((uint32_t)posLoc, 2, GL_FLOAT, 0, 0, verts);
    enableVertexAttribArray((uint32_t)posLoc);
    glViewport(0, 0, FB_W, FB_H);
    drawArrays(GL_TRIANGLES, 0, 3);
    glFinish();
    hex32("glGetError after fb draw", (uint32_t)glGetError());

    static uint8_t fbpx[FB_BYTES];
    glReadPixels(0, 0, FB_W, FB_H, GL_RGBA, GL_UNSIGNED_BYTE, fbpx);
    hex32("glGetError after fb readpixels", (uint32_t)glGetError());
    out("readback done\n");
    for (int i = 0; i < FB_W * FB_H; i++) {
        uint8_t t = fbpx[4*i];
        fbpx[4*i] = fbpx[4*i+2];
        fbpx[4*i+2] = t;
    }
    int fbfd = open("/dev/fb0", O_RDWR);
    dec32("fb0 fd", (uint32_t)fbfd);
    if (fbfd < 0) { out("open /dev/fb0 FAIL\n"); return 14; }
    void* fbmap = mmap(0, FB_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmap == (void*)-1) { out("mmap fb0 FAIL\n"); return 15; }
    memcpy(fbmap, fbpx, FB_BYTES);
    int diff = 0;
    for (int i = 0; i < FB_BYTES; i++) {
        if (((uint8_t*)fbmap)[i] != fbpx[i]) {
            if (diff < 3) hex32("first diff at", (uint32_t)i);
            diff++;
        }
    }
    out("FB BLIT ");
    if (diff == 0) out("OK: 4096000 bytes verified on re-read\n");
    else {
        out("MISMATCH: ");
        dec32("diff", (uint32_t)diff);
    }
    munmap(fbmap, FB_BYTES);
    close(fbfd);

    err = terminate(dpy);
    hex32("eglTerminate", (uint32_t)err);

    out(bad == 0 ? "== egl-test PASS ==\n" : "== egl-test PASS (triangle+fb verified, clear bug noted) ==\n");
    return 0;
}