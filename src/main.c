#include <complex.h>
#include <stdio.h>
#include <raylib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 900
#define WIN_H 550
#define CELL_SIZE 3
#define GW (WIN_W / CELL_SIZE)  // 300 cells
#define GH (WIN_H / CELL_SIZE)  // 183 cells
#define NCELLS (GW * GH)        // 54900 cells
#define FPS 60

static const int EX[9] = {0, 1, 0, -1, 0, 1, -1, -1, 1};
static const int EY[9] = {0, 0, 1, 0, -1, 1, 1, -1, -1};
static const int OP[9] = {0, 3, 4, 1, 2, 7, 8, 5, 6};
static const float WW[9] = {
    4.f / 9.f,
    1.f / 9.f, 1.f / 9.f, 1.f / 9.f, 1.f / 9.f,
    1.f / 36.f, 1.f / 36.f, 1.f / 36.f, 1.f / 36.f
};

static float F[NCELLS][9];
static float T[NCELLS][9];
static float RHO[NCELLS];
static float UX[NCELLS];
static float UY[NCELLS];
static int SOLID[NCELLS];

static float U0 = 0.08f;
static float omega = 1.2f;

static inline int IDX(int x, int y) { return y * GW + x; }

static inline float feq(float rho, float ux, float uy, int i) {
    float cu = (float) EX[i] * ux + (float) EY[i] * uy;
    float u2 = ux * ux + uy * uy;
    return WW[i] * rho * (1.f + 3.f * cu + 4.5f * cu * cu - 1.5f * u2);
}

static void lbm_init(void) {
    for (int n = 0; n < NCELLS; n++) {
        if (SOLID[n]) {
            memset(F[n], 0, sizeof(F[n]));
            RHO[n] = 1.f;
            UX[n] = 0.f;
            UY[n] = 0.f;
            continue;
        }
        RHO[n] = 1.f;
        UX[n] = U0;
        UY[n] = 0.f;
        for (int i = 0; i < 9; i++) {
            F[n][i] = feq(1.f, U0, 0.f, i);
        }
    }
}

static void lbm_step(void) {
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int id = IDX(x, y);
            for (int i = 0; i < 9; i++) {
                int sx = x - EX[i];
                int sy = y - EY[i];

                /* parede top - botton */

                if (sy < 0 || sy >= GH) {
                    T[id][i] = F[id][i];
                    continue;
                }
                /* inlet esquerda */

                if (sx < 0) {
                    T[id][i] = feq(1.f, U0, 0.f, i);
                    continue;
                }

                /* inlet direita */
                if (sx >= GW) {
                    T[id][i] = F[IDX(GW - 1, y)][i];
                    continue;
                }
                T[id][i] = F[IDX(sx, sy)][i];
            }
        }
    }

    /* colisao bgk */

    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int id = IDX(x, y);

            /* solido bounce back */
            if (SOLID[id]) {
                for (int i = 0; i < 9; i++) {
                    F[id][OP[i]] = T[id][i];
                }
                UX[id] = 0.f;
                UY[id] = 0.f;
                RHO[id] = 1.f;
                continue;
            }

            /* macro */

            float rho = 0.f, ux = 0.f, uy = 0.f;
            for (int i = 0; i < 9; i++) {
                float fi = T[id][i];
                if (!isfinite(fi) || fi < 0.f) fi = 0.f;
                rho += fi;
                ux += fi * (float) EX[i];
                uy += fi * (float) EY[i];
                F[id][i] = fi;
            }
            if (rho < 1e-4f) {
                rho = 1.f;
                ux = U0;
                uy = 0.f;
            }

            ux /= rho;
            uy /= rho;

            float spd = sqrtf(ux * ux + uy * uy);
            if (spd > 0.25f) {
                ux *= 0.25f / spd;
                uy *= 0.25f / spd;
            }

            RHO[id] = rho;
            UX[id] = ux;
            UY[id] = uy;

            for (int i = 0; i < 9; i++)
                F[id][i] += omega * (feq(rho, ux, uy, i) - F[id][i]);
        }
    }

    /* INLET */

    for (int y = 0; y < GH; y++) {
        for (int x = 0; x <= 1; x++) {
            int id = IDX(x, y);
            if (SOLID[id]) continue;
            for (int i = 0; i < 9; i++)
                F[id][i] = feq(1.f, U0, 0.f, i);
            RHO[id] = 1.f;
            UX[id] = 0.f;
            UY[id] = 0.f;
        }
    }

    /* OUTLET */

    for (int y = 0; y < GH; y++) {
        int id = IDX(GW - 1, y);
        int idp = IDX(GW - 2, y);
        if (SOLID[id]) continue;
        for (int i = 0; i < 9; i++) F[id][i] = F[idp][i];
        RHO[id] = RHO[idp];
        UX[id] = UX[idp];
        UY[id] = UY[idp];
    }
}

/* CORES */

typedef enum { VIS_SPEED = 0, VIS_PRESSURE, VIS_VORTEX } VisMode;

static Color vel_color(float t) {
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    return ColorFromHSV(240.f - t * 240.f, 1.f, 1.f);
}

static Color pres_color(float rho) {
    float t = (rho - 0.95f) / 0.1f;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    return ColorFromHSV(240.f - t * 240.f, 1.f, 1.f);
}

static Color vort_color(float v) {
    float t = v * 50.f + 0.5f;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    if (t < 0.5f) {
        unsigned char b = (unsigned char) ((0.5f - t) * 2.f * 255.f);
        return (Color)
        {
            0, 0, b, 255
        };
    } else {
        unsigned char r = (unsigned char) ((t - 0.5f) * 2.f * 255.f);
        return (Color)
        {
            r, 0, 0, 255
        };
    }
}

static float curl(int x, int y) {
    int xr = (x < GW - 1) ? x + 1 : x;
    int xl = (x > 0) ? x - 1 : x;
    int yu = (y < GH - 1) ? y + 1 : y;
    int yd = (y > 0) ? y - 1 : y;
    return (UY[IDX(xr, y)] - UY[IDX(xl, y)]) * 0.5f
           - (UX[IDX(x, yu)] - UX[IDX(x, yd)]) * 0.5f;
}

/* RENDERIZACAO */

static void render_pixels(Color *buf, VisMode mode, float max_spd) {
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int id = IDX(x, y);
            Color col;

            if (SOLID[id]) {
                col = (Color)
                {
                    60, 60, 70, 255
                };
            } else {
                switch (mode) {
                    case VIS_SPEED: {
                        float s = sqrtf(UX[id] * UX[id] + UY[id] * UY[id]);
                        col = vel_color(s / max_spd);
                        break;
                    }
                    case VIS_PRESSURE: {
                        col = pres_color(RHO[id]);
                        break;
                    }
                    case VIS_VORTEX: {
                        col = vort_color(curl(x, y));
                        break;
                    }
                    default: col = BLACK;
                        break;
                }
            }
            int px = x * CELL_SIZE;
            int py = y * CELL_SIZE;
            for (int dy = 0; dy < CELL_SIZE; dy++) {
                for (int dx = 0; dx < CELL_SIZE; dx++) {
                    buf[(py + dy) * WIN_W + (px + dx)] = col;
                }
            }
        }
    }
}

/* PINCEL */

static void paint(int solid, int brush) {
    Vector2 mp = GetMousePosition();
    int cx = (int) (mp.x / CELL_SIZE);
    int cy = (int) (mp.y / CELL_SIZE);
    for (int dy = -brush; dy <= brush; dy++) {
        for (int dx = -brush; dx <= brush; dx++) {
            if (dx * dx + dy * dy > brush * brush) continue;
            int gx = cx + dx, gy = cy + dy;
            if (gx < 2 || gx >= GW - 1) continue;
            if (gy < 1 || gy >= GH - 1) continue;
            int id = IDX(gx, gy);
            SOLID[id] = solid;
            if (!solid) {
                for (int i = 0; i < 9; i++) {
                    F[id][i] = feq(1.f, U0, 0.f, i);
                }
                RHO[id] = 1.f;
                UX[id] = U0;
                UY[id] = 0.f;
            }
        }
    }
}

/* HUD */

static const char *MODE_LBL[] = {"VELOCIDADE", "PRESSÃO", "VÓRTICE"};
static const Color MODE_COL[] = {
    {0, 220, 255, 255}, {255, 160, 30, 255}, {200, 100, 255, 255}
};

static void draw_hud(VisMode mode, int paused, float max_spd, int brush) {
    (void) max_spd;
    DrawRectangle(0, 0, 205, WIN_H, (Color)
    {
        0, 0, 0, 165
    });
    DrawLine(205, 0, 205, WIN_H, (Color)
    {
        50, 50, 70, 255
    });

    int y = 10, lh = 21;
    DrawText("WINDSIM", 10, y, 22, (Color)
    {
        0, 220, 255, 255
    });
    y += 26;
    DrawLine(10, y, 195, y, (Color)
    {
        45, 45, 65, 255
    });
    y += 20;

    DrawText("MODO:", 10, y, 13, (Color)
    {
        110, 110, 140, 255
    });
    y += lh;
    DrawText(MODE_LBL[mode], 10, y, 17, MODE_COL[mode]);
    y += lh + 4;
    DrawLine(10, y, 195, y, (Color)
    {
        45, 45, 65, 255
    });
    y += 10;

    /* Reynolds */
    float nu = (1.f / omega - 0.5f) / 3.f;
    int sc = 0;
    for (int i = 0; i < NCELLS; i++) sc += SOLID[i];
    float L = (sc > 0) ? sqrtf((float) sc) : 1.f;
    float Re = (nu > 1e-6f) ? U0 * L / nu : 0.f;

    DrawText("FISICAS:", 10, y, 13, (Color)
    {
        110, 110, 140, 255
    });
    y += lh;
    DrawText(TextFormat("Re   = %.1f", Re), 10, y, 14, WHITE);
    y += lh;
    DrawText(TextFormat("u0   = %.3f", U0), 10, y, 14, WHITE);
    y += lh;
    DrawText(TextFormat("w    = %.2f", omega), 10, y, 14, WHITE);
    y += lh;
    DrawText(TextFormat("Solidos: %d", sc), 10, y, 13, (Color)
    {
        155, 155, 180, 255
    });
    y += lh + 4;

    /* barra velocidade */
    DrawLine(10, y, 195, y, (Color)
    {
        45, 45, 65, 255
    });
    y += 10;
    DrawText("VENTO:", 10, y, 13, (Color)
    {
        110, 110, 140, 255
    });
    y += 18;
    int bw = (int) (190.f * (U0 / 0.18f));
    DrawRectangle(10, y, 190, 9, (Color)
    {
        22, 22, 38, 255
    });
    DrawRectangle(10, y, bw, 9, (Color)
    {
        0, 200, 255, 200
    });
    DrawRectangleLines(10, y, 190, 9, (Color)
    {
        55, 55, 80, 255
    });
    y += 18;

    DrawLine(10, y, 195, y, (Color)
    {
        45, 45, 65, 255
    });
    y += 10;
    DrawText("CONTROLES:", 10, y, 13, (Color)
    {
        110, 110, 140, 255
    });
    y += lh;

    const char *ctrls[] = {
        "LMB  desenhar", "RMB  apagar",
        "SPACE  pausar", "C  limpar tudo",
        "1 / 2 / 3  modo", "+ / -  vento",
        "[ L ]  pincel +", "[ R ] pincel -", "S  screenshot"
    };
    for (int k = 0; k < 8; k++) {
        DrawText(ctrls[k], 10, y, 12, (Color)
        {
            148, 188, 148, 255
        });
        y += lh - 3;
    }
    DrawText(TextFormat("pincel: %d", brush), 10, y, 12, (Color)
    {
        128, 168, 128, 255
    });
    y += lh;

    DrawLine(10, y, 195, y, (Color)
    {
        45, 45, 65, 255
    });
    y += 8;
    DrawText(TextFormat("FPS: %d", GetFPS()), 10, y, 14, (Color)
    {
        100, 255, 100, 255
    });

    /* pausado */
    if (paused) {
        DrawRectangle(215, 8, 175, 30, (Color)
        {
            0, 0, 0, 200
        });
        DrawText("[ PAUSADO ]", 222, 13, 20, (Color)
        {
            255, 210, 0, 255
        });
    }

    /* barra de escala de cor (direita) */
    int bx = WIN_W - 26, by = 15, bh = WIN_H - 30;
    for (int py = 0; py < bh; py++) {
        float t = 1.f - (float) py / bh;
        Color c;
        switch (mode) {
            case VIS_SPEED: c = vel_color(t);
                break;
            case VIS_PRESSURE: c = pres_color(0.95f + t * 0.10f);
                break;
            default: c = vort_color((t - 0.5f) / 25.f);
                break;
        }
        DrawLine(bx, by + py, bx + 14, by + py, c);
    }
    DrawRectangleLines(bx, by, 14, bh, (Color)
    {
        70, 70, 90, 255
    });
    DrawText("Hi", bx - 2, by - 14, 11, WHITE);
    DrawText("Lo", bx - 2, by + bh + 2, 11, WHITE);
}

/* MAIN */

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(WIN_W, WIN_H, "WindSim - Tunel de Vento (2d)");
    SetTargetFPS(FPS);

    memset(SOLID, 0, sizeof(SOLID));
    lbm_init();

    Color *pixels = (Color *) malloc(WIN_W * WIN_H * sizeof(Color));
    memset(pixels, 0, WIN_W * WIN_H * sizeof(Color));

    Image img;
    img.data = pixels,
            img.width = WIN_W,
            img.height = WIN_H,
            img.mipmaps = 1,
            img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    Texture2D tex = LoadTextureFromImage(img);

    int paused = 0;
    VisMode mode = VIS_SPEED;
    int brush = 5;
    float max_spd = 0.20f;

    while (!WindowShouldClose()) {
        /* inputs */

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) paint(1, brush);
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) paint(0, brush);

        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_ONE)) mode = VIS_SPEED;
        if (IsKeyPressed(KEY_TWO)) mode = VIS_PRESSURE;
        if (IsKeyPressed(KEY_THREE)) mode = VIS_VORTEX;
        if (IsKeyPressed(KEY_S)) TakeScreenshot("screenshot.png");

        if (IsKeyPressed(KEY_C)) {
            memset(SOLID, 0, sizeof(SOLID));
            lbm_init();
        }

        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD)) {
            U0 += 0.001f;
            if (U0 > 0.18f) U0 = 0.18f;
        }

        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) {
            U0 -= 0.001f;
            if (U0 < 0.02f) U0 = 0.02f;
        }

        if (IsKeyPressed(KEY_L) && brush > 1) brush--;
        if (IsKeyPressed(KEY_R) && brush < 15) brush++;

        if (!paused) {
            lbm_step();
            lbm_step();
            lbm_step();
            lbm_step();
        }

        render_pixels(pixels, mode, max_spd);
        UpdateTexture(tex, pixels);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(tex, 0, 0, WHITE);
        draw_hud(mode, paused, max_spd, brush);
        EndDrawing();
    }

    UnloadTexture(tex);
    free(pixels);
    CloseWindow();
    return 0;
}
