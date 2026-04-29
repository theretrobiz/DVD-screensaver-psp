#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>

PSP_MODULE_INFO("DVD Screensaver", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(512);

#define SCREEN_W     480
#define SCREEN_H     272
#define BUF_W        512
#define PIXEL_SIZE   4
#define PALETTE_SIZE 7
#define S            4

#define LOGO_W  (23*S)
#define LOGO_H  (17*S)

static unsigned int __attribute__((aligned(16))) displayList[0x40000];
static void *fbp0, *fbp1, *zbp;
static int exitFlag = 0;
static int colorIdx = 0;

typedef struct { float x, y, dx, dy; unsigned int color; } Logo;
typedef struct { short x, y, z; } __attribute__((packed)) Vertex;

static unsigned int palette[PALETTE_SIZE] = {
    0xFF4444FF,
    0xFF44FF44,
    0xFFFF4444,
    0xFF44FFFF,
    0xFFFF44FF,
    0xFFFFFF44,
    0xFFFFFFFF,
};

static const char L_D[7][7] = {
    {1,1,1,1,0,0,0},
    {1,0,0,0,1,1,0},
    {1,0,0,0,0,0,1},
    {1,0,0,0,0,0,1},
    {1,0,0,0,0,0,1},
    {1,0,0,0,1,1,0},
    {1,1,1,1,0,0,0},
};

static const char L_V[7][7] = {
    {1,0,0,0,0,0,1},
    {1,0,0,0,0,0,1},
    {0,1,0,0,0,1,0},
    {0,1,0,0,0,1,0},
    {0,0,1,0,1,0,0},
    {0,0,1,0,1,0,0},
    {0,0,0,1,0,0,0},
};

static const char T_V[5][3] = {{1,0,1},{1,0,1},{1,0,1},{0,1,0},{0,1,0}};
static const char T_I[5][3] = {{1,1,1},{0,1,0},{0,1,0},{0,1,0},{1,1,1}};
static const char T_D[5][3] = {{1,1,0},{1,0,1},{1,0,1},{1,0,1},{1,1,0}};
static const char T_E[5][3] = {{1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,1,1}};
static const char T_O[5][3] = {{0,1,0},{1,0,1},{1,0,1},{1,0,1},{0,1,0}};

int exitCallback(int a, int b, void *c) {
    (void)a; (void)b; (void)c;
    exitFlag = 1;
    return 0;
}

int callbackThread(SceSize a, void *b) {
    (void)a; (void)b;
    int id = sceKernelCreateCallback("ExitCb", exitCallback, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}

void setupCallbacks(void) {
    int t = sceKernelCreateThread("cbT", callbackThread, 0x11, 0xFA0, 0, 0);
    if (t >= 0) sceKernelStartThread(t, 0, 0);
}

void initGU(void) {
    fbp0 = (void *)0;
    fbp1 = (void *)(BUF_W * SCREEN_H * PIXEL_SIZE);
    zbp  = (void *)(BUF_W * SCREEN_H * PIXEL_SIZE * 2);
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, fbp1, BUF_W);
    sceGuDepthBuffer(zbp, BUF_W);
    sceGuOffset(2048-(SCREEN_W/2), 2048-(SCREEN_H/2));
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void drawRect(int x, int y, int w, int h, unsigned int color) {
    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    v[0].x=(short)x;     v[0].y=(short)y;     v[0].z=0;
    v[1].x=(short)(x+w); v[1].y=(short)(y+h); v[1].z=0;
    sceGuColor(color);
    sceGuDrawArray(GU_SPRITES, GU_VERTEX_16BIT|GU_TRANSFORM_2D, 2, 0, v);
}

void drawGlyph7(const char g[7][7], int ox, int oy, unsigned int col) {
    int r, c;
    for (r=0; r<7; r++)
        for (c=0; c<7; c++)
            if (g[r][c]) drawRect(ox+c*S, oy+r*S, S, S, col);
}

void drawGlyph3(const char g[5][3], int ox, int oy, unsigned int col) {
    int r, c;
    for (r=0; r<5; r++)
        for (c=0; c<3; c++)
            if (g[r][c]) drawRect(ox+c*S, oy+r*S, S, S, col);
}

void drawLogo(int ox, int oy, unsigned int color) {
    int lw = 7*S;
    int gap = 2*S;

    drawGlyph7(L_D, ox,            oy, color);
    drawGlyph7(L_V, ox+lw+gap,     oy, color);
    drawGlyph7(L_D, ox+2*(lw+gap), oy, color);

    int barY = oy + 7*S + S;
    drawRect(ox, barY, LOGO_W, S, color);

    int textY = barY + 2*S;
    int tw = 3*S;
    int tg = S;
    int totalW = 5*tw + 4*tg;
    int tx = ox + (LOGO_W - totalW)/2;

    drawGlyph3(T_V, tx+0*(tw+tg), textY, color);
    drawGlyph3(T_I, tx+1*(tw+tg), textY, color);
    drawGlyph3(T_D, tx+2*(tw+tg), textY, color);
    drawGlyph3(T_E, tx+3*(tw+tg), textY, color);
    drawGlyph3(T_O, tx+4*(tw+tg), textY, color);
}

unsigned int nextColor(void) {
    colorIdx = (colorIdx + 1) % PALETTE_SIZE;
    return palette[colorIdx];
}

int main(void) {
    setupCallbacks();
    initGU();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    Logo logo;
    logo.x  = 60.0f;
    logo.y  = 50.0f;
    logo.dx = 1.5f;
    logo.dy = 1.2f;
    logo.color = palette[0];

    while (!exitFlag) {
        logo.x += logo.dx;
        logo.y += logo.dy;

        int bounced = 0;

        if (logo.x <= 0.0f) {
            logo.x = 0.0f; logo.dx = -logo.dx; bounced = 1;
        } else if (logo.x >= (float)(SCREEN_W - LOGO_W)) {
            logo.x = (float)(SCREEN_W - LOGO_W); logo.dx = -logo.dx; bounced = 1;
        }

        if (logo.y <= 0.0f) {
            logo.y = 0.0f; logo.dy = -logo.dy; bounced = 1;
        } else if (logo.y >= (float)(SCREEN_H - LOGO_H)) {
            logo.y = (float)(SCREEN_H - LOGO_H); logo.dy = -logo.dy; bounced = 1;
        }

        if (bounced) logo.color = nextColor();

        sceGuStart(GU_DIRECT, displayList);
        sceGuClearColor(0xFF000000);
        sceGuClear(GU_COLOR_BUFFER_BIT);
        drawLogo((int)logo.x, (int)logo.y, logo.color);
        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}