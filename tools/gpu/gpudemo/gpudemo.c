/* gpudemo.c — Stage 5: live animated GPU demo on the LCD.
 * Bionic-linked: stock libEGL_MRVL + libGLESv2_MRVL render an animated
 * rotating cube into an 800x1280 pbuffer; each frame is read back,
 * converted to the panel's BGRA8888 and page-flipped through the three
 * fb0 scanout pages (virtual 800x3840). No stdio — raw write() output.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <math.h>

/* GCC -O2 fuses cosf/sinf pairs into sincosf(); the t4build armhf GCC
 * miscompiles that fusion (r2/&cos never loaded -> NULL deref in libm).
 * Keep the calls unfused through noinline wrappers. */
static float _cosf(float x) __attribute__((noinline));
static float _sinf(float x) __attribute__((noinline));
static float _cosf(float x) { return cosf(x); }
static float _sinf(float x) { return sinf(x); }

#define FB_W 800
#define FB_H 1280
#define FB_BYTES (FB_W * FB_H * 4)
#define FB_PAGES 3
#define FBIOPAN_DISPLAY _IOW('F', 6, uint32_t)
#define FBIOWAITFORVSYNC _IOW('F', 32, uint32_t)

typedef int32_t EGLint;
typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLSurface;

#define EGL_DEFAULT_DISPLAY ((void*)0)
#define EGL_ALPHA_SIZE 0x3021
#define EGL_BLUE_SIZE 0x3022
#define EGL_GREEN_SIZE 0x3023
#define EGL_RED_SIZE 0x3024
#define EGL_DEPTH_SIZE 0x3025
#define EGL_SURFACE_TYPE 0x3033
#define EGL_PBUFFER_BIT 0x0001
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_NONE 0x3038

typedef EGLDisplay (*fn_eglGetDisplay)(void* d);
typedef int32_t (*fn_eglInitialize)(EGLDisplay d, EGLint* maj, EGLint* min);
typedef int32_t (*fn_eglChooseConfig)(EGLDisplay d, const EGLint* attrs, EGLConfig* out, EGLint n, EGLint* got);
typedef EGLContext (*fn_eglCreateContext)(EGLDisplay d, EGLConfig c, EGLContext share, const EGLint* attrs);
typedef EGLSurface (*fn_eglCreatePbufferSurface)(EGLDisplay d, EGLConfig c, const EGLint* attrs);
typedef int32_t (*fn_eglMakeCurrent)(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext ctx);
typedef int32_t (*fn_eglSwapBuffers)(EGLDisplay d, EGLSurface s);
typedef int32_t (*fn_eglTerminate)(EGLDisplay d);
typedef int32_t (*fn_eglGetError)(void);

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
typedef int32_t (*fn_glGetUniformLocation)(uint32_t prog, const char* name);
typedef void (*fn_glVertexAttribPointer)(uint32_t idx, int32_t size, uint32_t type, uint32_t norm, int32_t stride, const void* ptr);
typedef void (*fn_glEnableVertexAttribArray)(uint32_t idx);
typedef void (*fn_glUniformMatrix4fv)(int32_t loc, int32_t count, uint32_t transpose, const float* v);
typedef void (*fn_glEnable)(uint32_t cap);
typedef void (*fn_glDepthFunc)(uint32_t fn);
typedef void (*fn_glViewport)(int32_t x, int32_t y, int32_t w, int32_t h);
typedef void (*fn_glDrawArrays)(uint32_t mode, int32_t first, int32_t count);
typedef void (*fn_glFinish)(void);
typedef uint32_t (*fn_glGetError)(void);
typedef void (*fn_glReadPixels)(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t fmt, uint32_t type, void* px);

#define GL_VERTEX_SHADER 0x8b31
#define GL_FRAGMENT_SHADER 0x8b30
#define GL_COMPILE_STATUS 0x8b81
#define GL_LINK_STATUS 0x8b82
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_FAN 0x0006
#define GL_DEPTH_TEST 0x0b71
#define GL_LEQUAL 0x0203
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401

static void out(const char* s)
{
    int l = 0;
    while (s[l]) l++;
    asm volatile(
        "mov r7, #4\n" "mov r0, #1\n" "mov r1, %0\n" "mov r2, %1\n" "svc 0\n"
        :: "r"(s), "r"(l) : "r7", "r0", "r1", "r2", "memory");
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
        "mov r7, #4\n" "mov r0, #1\n" "mov r1, %0\n" "mov r2, %1\n" "svc 0\n"
        :: "r"(b), "r"(i) : "r7", "r0", "r1", "r2", "memory");
}


/* unit cube: 6 faces, 6 verts each (2 triangles), per-face color */
static float cubePos[36 * 3] = {
    -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
};
static float cubeCol[36 * 4] = {
    1,0,0,1,  1,0,0,1,  1,0,0,1,  1,0,0,1,  1,0,0,1,  1,0,0,1,
    0,1,0,1,  0,1,0,1,  0,1,0,1,  0,1,0,1,  0,1,0,1,  0,1,0,1,
    0,0,1,1,  0,0,1,1,  0,0,1,1,  0,0,1,1,  0,0,1,1,  0,0,1,1,
    1,1,0,1,  1,1,0,1,  1,1,0,1,  1,1,0,1,  1,1,0,1,  1,1,0,1,
    1,0,1,1,  1,0,1,1,  1,0,1,1,  1,0,1,1,  1,0,1,1,  1,0,1,1,
    0,1,1,1,  0,1,1,1,  0,1,1,1,  0,1,1,1,  0,1,1,1,  0,1,1,1,
};

/* fullscreen background quad at far depth (clear is broken in this blob) */
static float bgPos[4 * 2] = { -1,-1, 3,-1, 3,3, -1,3 };
static float bgCol[6 * 4] = {
    0.05f,0.05f,0.08f,1,  0.05f,0.05f,0.08f,1,  0.05f,0.05f,0.08f,1,
    0.05f,0.05f,0.08f,1,  0.05f,0.05f,0.08f,1,  0.05f,0.05f,0.08f,1,
};

static void mat4Mul(float* r, const float* a, const float* b)
{
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a[k * 4 + rr] * b[c * 4 + k];
            r[c * 4 + rr] = s;
        }
}

int main(void)
{
    out("== gpudemo: animated cube on fb0 ==\n");
    void* e = dlopen("/system/lib/egl/libEGL_MRVL.so", RTLD_NOW);
    void* g = dlopen("/system/lib/egl/libGLESv2_MRVL.so", RTLD_NOW);
    if (!e || !g) { out("dlopen FAIL\n"); return 1; }

    fn_eglGetDisplay getDisplay = (fn_eglGetDisplay)dlsym(e, "eglGetDisplay");
    fn_eglInitialize init = (fn_eglInitialize)dlsym(e, "eglInitialize");
    fn_eglChooseConfig chooseConfig = (fn_eglChooseConfig)dlsym(e, "eglChooseConfig");
    fn_eglCreateContext createContext = (fn_eglCreateContext)dlsym(e, "eglCreateContext");
    fn_eglCreatePbufferSurface createPbuffer = (fn_eglCreatePbufferSurface)dlsym(e, "eglCreatePbufferSurface");
    fn_eglMakeCurrent makeCurrent = (fn_eglMakeCurrent)dlsym(e, "eglMakeCurrent");
    fn_eglSwapBuffers swapBuffers = (fn_eglSwapBuffers)dlsym(e, "eglSwapBuffers");
    fn_eglTerminate terminate = (fn_eglTerminate)dlsym(e, "eglTerminate");
    fn_eglGetError getError = (fn_eglGetError)dlsym(e, "eglGetError");

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
    fn_glGetUniformLocation getUniformLocation = (fn_glGetUniformLocation)dlsym(g, "glGetUniformLocation");
    fn_glVertexAttribPointer vertexAttribPointer = (fn_glVertexAttribPointer)dlsym(g, "glVertexAttribPointer");
    fn_glEnableVertexAttribArray enableVertexAttribArray = (fn_glEnableVertexAttribArray)dlsym(g, "glEnableVertexAttribArray");
    fn_glUniformMatrix4fv uniformMatrix4fv = (fn_glUniformMatrix4fv)dlsym(g, "glUniformMatrix4fv");
    fn_glEnable glEnable = (fn_glEnable)dlsym(g, "glEnable");
    fn_glDepthFunc glDepthFunc = (fn_glDepthFunc)dlsym(g, "glDepthFunc");
    fn_glViewport glViewport = (fn_glViewport)dlsym(g, "glViewport");
    fn_glDrawArrays drawArrays = (fn_glDrawArrays)dlsym(g, "glDrawArrays");
    fn_glFinish glFinish = (fn_glFinish)dlsym(g, "glFinish");
    fn_glGetError glGetError = (fn_glGetError)dlsym(g, "glGetError");
    fn_glReadPixels glReadPixels = (fn_glReadPixels)dlsym(g, "glReadPixels");
    if (!getDisplay || !init || !chooseConfig || !createContext || !createPbuffer || !makeCurrent ||
        !terminate || !getError || !createShader || !shaderSource || !compileShader || !createProgram ||
        !attachShader || !linkProgram || !useProgram || !getShaderiv || !getProgramiv ||
        !getAttribLocation || !getUniformLocation || !vertexAttribPointer || !enableVertexAttribArray ||
        !uniformMatrix4fv || !glEnable || !glDepthFunc || !glViewport || !drawArrays || !glFinish ||
        !glGetError || !glReadPixels) {
        out("dlsym FAIL\n");
        return 2;
    }
    out("dlsym OK\n");

    EGLDisplay dpy = getDisplay(EGL_DEFAULT_DISPLAY);
    EGLint maj, min;
    if (init(dpy, &maj, &min) != 1) { out("eglInitialize FAIL\n"); return 3; }

    static const EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint got = 0;
    if (chooseConfig(dpy, cfgAttrs, &cfg, 1, &got) != 1 || got < 1) { out("eglChooseConfig FAIL\n"); return 4; }

    static const EGLint ctxAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext ctx = createContext(dpy, cfg, 0, ctxAttrs);
    static const EGLint surfAttrs[] = { EGL_WIDTH, FB_W, EGL_HEIGHT, FB_H, EGL_NONE };
    EGLSurface surf = createPbuffer(dpy, cfg, surfAttrs);
    if (!ctx || !surf) { out("context/surface FAIL\n"); return 5; }
    if (makeCurrent(dpy, surf, surf, ctx) != 1) { out("makeCurrent FAIL\n"); return 6; }
    out("EGL ready\n");

    static const char* vs =
        "attribute vec3 pos; attribute vec4 col;"
        "varying vec4 vc;"
        "void main(){ vc = col; gl_Position = vec4(pos, 1.0); }";
    static const char* fs =
        "precision mediump float; varying vec4 vc; void main(){ gl_FragColor = vc; }";
    int32_t ok = 0;
    uint32_t vsh = createShader(GL_VERTEX_SHADER);
    shaderSource(vsh, 1, &vs, 0);
    compileShader(vsh);
    getShaderiv(vsh, GL_COMPILE_STATUS, &ok);
    uint32_t fsh = createShader(GL_FRAGMENT_SHADER);
    shaderSource(fsh, 1, &fs, 0);
    compileShader(fsh);
    getShaderiv(fsh, GL_COMPILE_STATUS, &ok);
    uint32_t prog = createProgram();
    attachShader(prog, vsh);
    attachShader(prog, fsh);
    linkProgram(prog);
    getProgramiv(prog, GL_LINK_STATUS, &ok);
    dec32("shader link", (uint32_t)ok);
    if (!ok) { out("program FAIL\n"); return 7; }
    useProgram(prog);
    int32_t posLoc = getAttribLocation(prog, "pos");
    int32_t colLoc = getAttribLocation(prog, "col");
    dec32("pos loc", (uint32_t)posLoc);
    dec32("col loc", (uint32_t)colLoc);
    if (posLoc < 0 || colLoc < 0) { out("program setup FAIL\n"); return 7; }

    glViewport(0, 0, FB_W, FB_H);

    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) { out("open fb0 FAIL\n"); return 8; }
    void* fbmap = mmap(0, FB_BYTES * FB_PAGES, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmap == (void*)-1) { out("mmap fb0 FAIL\n"); return 9; }

    static uint8_t px[FB_BYTES];
    uint32_t frame = 0;
    int panErr = 0;
    int vsyncErr = 0;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    while (frame < 600) {
        float ang = frame * 0.02f;
        float c = _cosf(ang), s = _sinf(ang);
        float c2 = _cosf(ang * 0.6f), s2 = _sinf(ang * 0.6f);
        float rotY[16], rotX[16], mv[16];
        memset(rotY, 0, sizeof(rotY));
        rotY[0] = c; rotY[2] = -s; rotY[8] = s; rotY[10] = c; rotY[15] = 1.0f;
        memset(rotX, 0, sizeof(rotX));
        rotX[0] = 1.0f; rotX[5] = c2; rotX[6] = -s2; rotX[9] = s2; rotX[10] = c2; rotX[15] = 1.0f;
        mat4Mul(mv, rotX, rotY);

        /* blob uniforms are broken (locations collide with attributes):
           rotate the cube on the CPU instead */
        static float rotCube[36 * 3];
        for (int i = 0; i < 36 * 3; i += 3) {
            float x = cubePos[i], y = cubePos[i + 1], z = cubePos[i + 2];
            float cx = mv[0] * x + mv[4] * y + mv[8] * z;
            float cy = mv[1] * x + mv[5] * y + mv[9] * z;
            float cz = mv[2] * x + mv[6] * y + mv[10] * z;
            rotCube[i] = cx * 0.22f;
            rotCube[i + 1] = cy * 0.22f;
            rotCube[i + 2] = cz * 0.22f;
        }

        /* blob quirk: only the FIRST+LAST drawArrays of a frame batch
           render (each rendered draw contributes only its last 6 verts);
           glFinish / tiny glReadPixels / eglSwapBuffers / glViewport all
           fail to flush mid-frame.  Budget: 2 draws of 6 verts per frame.
           So: no GL background; draw the two front-most cube faces and
           convert the cleared black background to the bg color on CPU. */
        float zAvg[6];
        for (int f = 0; f < 6; f++) {
            zAvg[f] = rotCube[18 * f + 2] + rotCube[18 * f + 5] +
                      rotCube[18 * f + 8] + rotCube[18 * f + 11] +
                      rotCube[18 * f + 14] + rotCube[18 * f + 17];
        }
        int f0 = 0;
        for (int f = 0; f < 6; f++) {
            if (zAvg[f] > zAvg[f0]) f0 = f;
        }
        /* blob dedups consecutive drawArrays with the same
           (mode, first, count): identical draws alternate skip/draw.
           Make every draw distinct: bg = TRIANGLE_FAN (mode 6, 4 verts),
           each face = (GL_TRIANGLES, first=6*f, count=6). */
        vertexAttribPointer((uint32_t)posLoc, 2, GL_FLOAT, 0, 0, bgPos);
        vertexAttribPointer((uint32_t)colLoc, 4, GL_FLOAT, 0, 0, bgCol);
        enableVertexAttribArray((uint32_t)posLoc);
        enableVertexAttribArray((uint32_t)colLoc);
        drawArrays(GL_TRIANGLE_FAN, 0, 4);
        vertexAttribPointer((uint32_t)posLoc, 3, GL_FLOAT, 0, 0, rotCube);
        vertexAttribPointer((uint32_t)colLoc, 4, GL_FLOAT, 0, 0, cubeCol);
        for (int f = 0; f < 6; f++)
            drawArrays(GL_TRIANGLES, 6 * f, 6);
        glFinish();

        uint32_t err = glGetError();
        if (err != 0) {
            out("GL error ");
            dec32("", err);
            break;
        }

        glReadPixels(0, 0, FB_W, FB_H, GL_RGBA, GL_UNSIGNED_BYTE, px);
        for (int i = 0; i < FB_W * FB_H; i++) {
            uint8_t t = px[4 * i];
            px[4 * i] = px[4 * i + 2];
            px[4 * i + 2] = t;
        }

        /* no GL bg draw (2-draw budget): convert the cleared black
           background to the bg color here */
        for (int i = 0; i < FB_W * FB_H; i++) {
            if (px[4 * i] == 0 && px[4 * i + 1] == 0 && px[4 * i + 2] == 0) {
                px[4 * i] = 13; px[4 * i + 1] = 13; px[4 * i + 2] = 20;
            }
        }

        uint32_t page = frame % FB_PAGES;
        memcpy((uint8_t*)fbmap + page * FB_BYTES, px, FB_BYTES);
        uint32_t yoff = page * FB_H;
        if (panErr == 0 && ioctl(fbfd, FBIOPAN_DISPLAY, &yoff) != 0) {
            panErr = 1;
            out("FBIOPAN_DISPLAY unsupported\n");
        }
        if (vsyncErr == 0 && (frame & 1)) {
            uint32_t z = 0;
            if (ioctl(fbfd, FBIOWAITFORVSYNC, &z) != 0) { vsyncErr = 1; out("FBIOWAITFORVSYNC unsupported\n"); }
        }

        frame++;
        if ((frame % 60) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
            uint64_t ms = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
            out("frames=");
            dec32("", frame);
            out(" avgms=");
            dec32("", (uint32_t)(ms / frame));
            uint32_t ci = 4 * (FB_W * (FB_H / 2) + FB_W / 2);
            out(" center bg=");
            dec32("", (uint32_t)px[ci]);
            dec32("", (uint32_t)px[ci + 1]);
            dec32("", (uint32_t)px[ci + 2]);
            out("\n");
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    uint64_t ms = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
    out("== gpudemo done: ");
    dec32("frames", frame);
    out(" ");
    dec32("ms", (uint32_t)ms);
    out(" ");
    dec32("avgms", (uint32_t)(ms / (frame ? frame : 1)));
    out(" ==\n");
    munmap(fbmap, FB_BYTES * FB_PAGES);
    close(fbfd);
    terminate(dpy);
    return 0;
}
