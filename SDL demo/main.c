#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define WIDTH  1600
#define HEIGHT 1000

#define NUM_ASTEROIDS 150
#define NUM_MOONS     10

// =========================================
// BASIC STRUCTS
// =========================================

struct Circle {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;
};

typedef struct {
    struct Circle circle;   // visual
    float orbitRadius;
    float angularSpeed;
    float angle;

    float worldX, worldZ;
    float depth;
    float screenRadius;

    char name[64];
} Planet;

struct Moon {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;

    int   parentIndex;
    char  parentName[32];

    float orbitRadius;
    float angle;
    float angularSpeed;
};

// =========================================
// UI TEXT FIELD
// =========================================

typedef struct {
    const char *label;
    char  text[64];
    int   maxLen;
    bool  numericOnly;
} TextField;

typedef enum {
    FIELD_NAME = 0,
    FIELD_ORBIT,
    FIELD_SPEED,
    FIELD_RADIUS,
    FIELD_R,
    FIELD_G,
    FIELD_B,
    FIELD_COUNT
} FieldId;

// =========================================
// DRAW HELPERS (CIRCLES)
// =========================================

void DrawFillCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    if (radius <= 0.5f) {
        SDL_RenderPoint(renderer, cx, cy);
        return;
    }

    int r = (int)(radius + 0.5f);
    int r2 = r * r;

    for (int y = -r; y <= r; ++y) {
        int yy = y * y;
        if (yy > r2) continue;
        int xSpan = (int)sqrtf((float)(r2 - yy));
        SDL_RenderLine(renderer, cx - xSpan, cy + y, cx + xSpan, cy + y);
    }
}

void DrawCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    if (radius <= 0.5f) {
        SDL_RenderPoint(renderer, cx, cy);
        return;
    }

    float x = radius;
    float y = 0;
    float d = 1 - x;

    while (y <= x)
    {
        SDL_RenderPoint(renderer, cx + x, cy + y);
        SDL_RenderPoint(renderer, cx + y, cy + x);
        SDL_RenderPoint(renderer, cx - y, cy + x);
        SDL_RenderPoint(renderer, cx - x, cy + y);
        SDL_RenderPoint(renderer, cx - x, cy - y);
        SDL_RenderPoint(renderer, cx - y, cy - x);
        SDL_RenderPoint(renderer, cx + y, cy - x);
        SDL_RenderPoint(renderer, cx + x, cy - y);

        y++;

        if (d <= 0)
            d += 2 * y + 1;
        else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

// =========================================
// 5x7 BITMAP FONT (DIGITS + A-Z)
// =========================================

static const unsigned char font5x7[36][7] = {
    // 0
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    // 1
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    // 2
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
    // 3
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    // 4
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    // 5
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    // 6
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    // 7
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    // 8
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    // 9
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},

    // A
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    // B
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    // C
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    // D
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    // E
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    // F
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    // G
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E},
    // H
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    // I
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    // J
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},

    // K
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    // L
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    // M
    {0x11,0x1B,0x15,0x11,0x11,0x11,0x11},
    // N
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    // O
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    // P
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    // Q
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    // R
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    // S
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    // T
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},

    // U
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    // V
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    // W
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    // X
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    // Y
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    // Z
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}
};

static int FontIndexForChar(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    return -1;
}

static void DrawChar(SDL_Renderer *renderer, float x, float y, char c, float scale)
{
    int idx = FontIndexForChar(c);
    if (idx < 0) return;

    for (int row = 0; row < 7; ++row) {
        unsigned char bits = font5x7[idx][row];
        for (int col = 0; col < 5; ++col) {
            if (bits & (1 << (4 - col))) {
                SDL_FRect r;
                r.x = x + col * scale;
                r.y = y + row * scale;
                r.w = scale;
                r.h = scale;
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }
}

static void DrawText(SDL_Renderer *renderer, float x, float y, const char *text, float scale)
{
    float cx = x;
    for (const char *p = text; *p; ++p) {
        if (*p == ' ') {
            cx += 6.0f * scale;
            continue;
        }
        DrawChar(renderer, cx, y, *p, scale);
        cx += 6.0f * scale;
    }
}

// =========================================
// 3D PROJECTION
// =========================================

static void ProjectXZ3D(
    float worldX, float worldZ,
    float cosYaw, float sinYaw,
    float cosPitch, float sinPitch,
    float camDist, float fov,
    float cx, float cy,
    float panX, float panY,
    float *outX, float *outY, float *outDepth)
{
    float x1 =  worldX * cosYaw + worldZ * sinYaw;
    float z1 = -worldX * sinYaw + worldZ * cosYaw;

    float y1 = 0.0f;

    float y2 =  y1 * cosPitch - z1 * sinPitch;
    float z2 =  y1 * sinPitch + z1 * cosPitch;
    float x2 =  x1;

    float cz = z2 + camDist;
    if (cz < 1.0f) cz = 1.0f;

    float inv = fov / cz;
    *outX = cx + panX + x2 * inv;
    *outY = cy + panY + y2 * inv;

    if (outDepth) *outDepth = cz;
}

// =========================================
// LOAD PLANETS FROM TEXT FILE
// =========================================

static int LoadPlanetsFromTextFile(const char *filename,
                                   Planet **outPlanets,
                                   int *outCount)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open planets file '%s'\n", filename);
        return 0;
    }

    Planet *planets = NULL;
    int count = 0;
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || strlen(line) < 5)
            continue;

        char name[64];
        float orbitRadius, angularSpeed, radius;
        int r, g, b;

        int n = sscanf(line, "%63s %f %f %f %d %d %d",
                       name, &orbitRadius, &angularSpeed,
                       &radius, &r, &g, &b);

        if (n != 7)
            continue;

        Planet *tmp = (Planet *)realloc(planets, sizeof(Planet) * (count + 1));
        if (!tmp) {
            fclose(fp);
            free(planets);
            return 0;
        }
        planets = tmp;

        Planet *p = &planets[count];
        memset(p, 0, sizeof(*p));

        strncpy(p->name, name, sizeof(p->name)-1);
        p->orbitRadius  = orbitRadius;
        p->angularSpeed = angularSpeed;
        p->angle        = 0.0f;

        p->circle.radius = radius;
        p->circle.r = (Uint8)r;
        p->circle.g = (Uint8)g;
        p->circle.b = (Uint8)b;

        p->worldX = 0.0f;
        p->worldZ = p->orbitRadius;
        p->depth  = 1.0f;
        p->screenRadius = 0.0f;

        count++;
    }

    fclose(fp);

    *outPlanets = planets;
    *outCount   = count;

    printf("Loaded %d planets from '%s'\n", count, filename);
    return (count > 0);
}

// =========================================
// MOON PARENT RESOLUTION
// =========================================

static void ResolveMoonParents(struct Moon *moons,
                               int numMoons,
                               Planet *planets,
                               int numPlanets)
{
    for (int i = 0; i < numMoons; i++) {
        moons[i].parentIndex = -1;
        for (int p = 0; p < numPlanets; p++) {
            if (strcmp(moons[i].parentName, planets[p].name) == 0) {
                moons[i].parentIndex = p;
                break;
            }
        }
        if (moons[i].parentIndex < 0) {
            fprintf(stderr, "Warning: moon parent '%s' not found.\n",
                    moons[i].parentName);
        }
    }
}

// =========================================
// UI HELPERS
// =========================================

static bool PointInRect(float x, float y, SDL_FRect *rect)
{
    return (x >= rect->x && x <= rect->x + rect->w &&
            y >= rect->y && y <= rect->y + rect->h);
}

static void ClampColorInt(int *v)
{
    if (*v < 0) *v = 0;
    if (*v > 255) *v = 255;
}

// Append new planet based on text fields; return 1 on success
static int SavePlanetFromFieldsToFile(const char *filename, TextField fields[FIELD_COUNT])
{
    const char *name   = fields[FIELD_NAME].text;
    const char *orbitS = fields[FIELD_ORBIT].text;
    const char *speedS = fields[FIELD_SPEED].text;
    const char *radS   = fields[FIELD_RADIUS].text;
    const char *rS     = fields[FIELD_R].text;
    const char *gS     = fields[FIELD_G].text;
    const char *bS     = fields[FIELD_B].text;

    if (name[0] == '\0') {
        fprintf(stderr, "Name cannot be empty.\n");
        return 0;
    }

    float orbitRadius  = (float)atof(orbitS);
    float angularSpeed = (float)atof(speedS);
    float radius       = (float)atof(radS);
    int r = atoi(rS);
    int g = atoi(gS);
    int b = atoi(bS);
    ClampColorInt(&r);
    ClampColorInt(&g);
    ClampColorInt(&b);

    FILE *fp = fopen(filename, "a");
    if (!fp) {
        fprintf(stderr, "Failed to open '%s' for appending.\n", filename);
        return 0;
    }

    fprintf(fp, "%s %.3f %.5f %.3f %d %d %d\n",
            name, orbitRadius, angularSpeed, radius, r, g, b);
    fclose(fp);

    printf("Added planet: %s\n", name);
    return 1;
}

// =========================================
// MAIN
// =========================================

int main(int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "3D-ish Solar System (Add Planet UI)",
        WIDTH, HEIGHT,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const char *PLANETS_FILE = "planets.txt";

    Planet *planets = NULL;
    int numPlanets = 0;

    if (!LoadPlanetsFromTextFile(PLANETS_FILE, &planets, &numPlanets) || numPlanets == 0) {
        fprintf(stderr, "No planets loaded. Ensure 'planets.txt' exists.\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // CAMERA
    const float BASE_FOV  = 800.0f;
    const float CAM_DIST  = 1500.0f;

    float zoom = 0.7f;
    float camYaw   = 0.5f;
    float camPitch = 0.5f;
    float camPanX  = 0.0f;
    float camPanY  = 0.0f;

    const float ROTATE_SENS = 0.005f;
    const float PAN_SENS    = 1.0f;

    int mouseLeft = 0, mouseRight = 0;
    int lastX = 0, lastY = 0;

    srand(42);

    // SUN
    struct Circle sun = {0,0,30,255,255,0};
    float sunScreenX=0, sunScreenY=0, sunDepth=1, sunScreenRadius=1;

    // ASTEROID BELT between Mars (150) and Jupiter (250)
    float innerBelt = 170.0f;
    float outerBelt = 230.0f;

    float asteroid_radius[NUM_ASTEROIDS];
    float asteroid_angle[NUM_ASTEROIDS];
    float asteroid_speed[NUM_ASTEROIDS];

    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        asteroid_radius[i] = innerBelt + (float)rand()/RAND_MAX*(outerBelt-innerBelt);
        asteroid_angle[i]  = (float)rand()/RAND_MAX * 6.283185f;
        asteroid_speed[i]  = 0.01f + (float)rand()/RAND_MAX * 0.005f;
    }

    // MOONS
    struct Moon moons[NUM_MOONS] = {
        {0,0, 3,210,210,210, -1, "Earth",  18,0,0.08f},
        {0,0, 2,200,200,200, -1, "Mars",   10,1,0.10f},
        {0,0, 2,160,160,160, -1, "Mars",   15,2,0.07f},
        {0,0, 4,255,200,180, -1, "Jupiter",30,0,0.09f},
        {0,0, 3,180,220,255, -1, "Jupiter",40,1,0.07f},
        {0,0, 5,220,220,220, -1, "Jupiter",52,2,0.05f},
        {0,0, 4,200,200,200, -1, "Jupiter",65,3,0.04f},
        {0,0, 4,230,210,160, -1, "Saturn", 28,0.5f,0.06f},
        {0,0, 3,200,220,255, -1, "Uranus", 24,1.2f,0.06f},
        {0,0, 3,180,200,255, -1, "Neptune",22,2.0f,0.06f}
    };
    float moonDepth[NUM_MOONS];

    ResolveMoonParents(moons, NUM_MOONS, planets, numPlanets);

    int selectedPlanet = -1;
    int running = 1;

    SDL_Event e;

    // "Add Planet" button
    SDL_FRect addButton = {10.0f, 10.0f, 160.0f, 40.0f};

    // Add Planet UI
    bool  addPanelOpen = false;
    FieldId activeField = FIELD_NAME;
    TextField fields[FIELD_COUNT] = {
        {"NAME",        "",  32, false},
        {"ORBIT",       "",  16, true },
        {"SPEED",       "",  16, true },
        {"RADIUS",      "",  16, true },
        {"COLOR R",     "",  4,  true },
        {"COLOR G",     "",  4,  true },
        {"COLOR B",     "",  4,  true }
    };

    // Defaults for color
    strcpy(fields[FIELD_R].text, "200");
    strcpy(fields[FIELD_G].text, "200");
    strcpy(fields[FIELD_B].text, "200");

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    while (running) {
        // EVENTS
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = 0;
            }
            else if (!addPanelOpen && e.type == SDL_EVENT_MOUSE_WHEEL) {
                zoom += (e.wheel.y > 0 ? 0.1f : -0.1f);
                if (zoom < 0.2f) zoom = 0.2f;
                if (zoom > 5.0f) zoom = 5.0f;
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) mouseLeft = 1;
                if (e.button.button == SDL_BUTTON_RIGHT) mouseRight = 1;
                lastX = (int)e.button.x;
                lastY = (int)e.button.y;

                float mx = (float)e.button.x;
                float my = (float)e.button.y;

                if (!addPanelOpen) {
                    // Check Add button
                    if (PointInRect(mx, my, &addButton)) {
                        addPanelOpen = true;
                        activeField = FIELD_NAME;
                        SDL_StartTextInput(window);  // SDL3: needs window

                        // Clear some input, keep default colors
                        fields[FIELD_NAME].text[0]   = '\0';
                        fields[FIELD_ORBIT].text[0]  = '\0';
                        fields[FIELD_SPEED].text[0]  = '\0';
                        fields[FIELD_RADIUS].text[0] = '\0';
                    } else {
                        // Planet selection
                        selectedPlanet = -1;
                        for (int i = 0; i < numPlanets; i++) {
                            float dx = mx - planets[i].circle.x;
                            float dy = my - planets[i].circle.y;
                            if (dx*dx + dy*dy <= planets[i].screenRadius * planets[i].screenRadius)
                                selectedPlanet = i;
                        }
                    }
                } else {
                    // Click inside add panel UI for field selection / buttons
                    int winW, winH;
                    SDL_GetWindowSize(window, &winW, &winH);
                    SDL_FRect panel = {
                        winW * 0.5f - 350.0f,
                        winH * 0.5f - 220.0f,
                        700.0f,
                        440.0f
                    };

                    float px = panel.x + 30.0f;
                    float py = panel.y + 60.0f;
                    float rowH = 40.0f;
                    float labelWidth = 140.0f;
                    SDL_FRect fieldRects[FIELD_COUNT];

                    for (int i = 0; i < FIELD_COUNT; i++) {
                        SDL_FRect r = {
                            px + labelWidth,
                            py + i*rowH,
                            300.0f,
                            28.0f
                        };
                        fieldRects[i] = r;
                    }

                    // Buttons
                    SDL_FRect saveBtn = {
                        panel.x + 80.0f,
                        panel.y + panel.h - 70.0f,
                        180.0f,
                        40.0f
                    };
                    SDL_FRect cancelBtn = {
                        panel.x + panel.w - 260.0f,
                        panel.y + panel.h - 70.0f,
                        180.0f,
                        40.0f
                    };

                    // Click on fields
                    bool fieldHit = false;
                    for (int i = 0; i < FIELD_COUNT; i++) {
                        if (PointInRect(mx, my, &fieldRects[i])) {
                            activeField = (FieldId)i;
                            fieldHit = true;
                            break;
                        }
                    }

                    if (!fieldHit) {
                        // Click Save
                        if (PointInRect(mx, my, &saveBtn)) {
                            if (SavePlanetFromFieldsToFile(PLANETS_FILE, fields)) {
                                // Reload planets
                                free(planets);
                                planets = NULL;
                                numPlanets = 0;
                                if (!LoadPlanetsFromTextFile(PLANETS_FILE, &planets, &numPlanets)) {
                                    fprintf(stderr, "Failed to reload planets after save.\n");
                                    running = 0;
                                } else {
                                    ResolveMoonParents(moons, NUM_MOONS, planets, numPlanets);
                                }
                            }
                            addPanelOpen = false;
                            SDL_StopTextInput(window);  // SDL3: needs window
                        }
                        // Click Cancel
                        else if (PointInRect(mx, my, &cancelBtn)) {
                            addPanelOpen = false;
                            SDL_StopTextInput(window);  // SDL3
                        }
                    }
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (e.button.button == SDL_BUTTON_LEFT) mouseLeft = 0;
                if (e.button.button == SDL_BUTTON_RIGHT) mouseRight = 0;
            }
            else if (!addPanelOpen && e.type == SDL_EVENT_MOUSE_MOTION) {
                int mx = (int)e.motion.x;
                int my = (int)e.motion.y;
                int dx = mx - lastX;
                int dy = my - lastY;
                lastX = mx;
                lastY = my;

                if (mouseLeft) {
                    camYaw   += dx * ROTATE_SENS;
                    camPitch += dy * ROTATE_SENS;
                    if (camPitch >  1.5f) camPitch =  1.5f;
                    if (camPitch < -1.5f) camPitch = -1.5f;
                }
                if (mouseRight) {
                    camPanX += dx * PAN_SENS;
                    camPanY += dy * PAN_SENS;
                }
            }
            else if (addPanelOpen && e.type == SDL_EVENT_TEXT_INPUT) {
                // Text input for current field
                TextField *tf = &fields[activeField];
                int len = (int)strlen(tf->text);
                const char *p = e.text.text;
                while (*p && len < tf->maxLen - 1) {
                    char ch = *p++;
                    if (tf->numericOnly) {
                        if (!((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')) {
                            continue;
                        }
                    }
                    tf->text[len++] = ch;
                    tf->text[len] = '\0';
                }
            }
            else if (addPanelOpen && e.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode key = e.key.key;    // SDL3: key, not keysym.sym
                TextField *tf = &fields[activeField];

                if (key == SDLK_BACKSPACE) {
                    int len = (int)strlen(tf->text);
                    if (len > 0) {
                        tf->text[len-1] = '\0';
                    }
                } else if (key == SDLK_TAB) {
                    activeField = (FieldId)((activeField + 1) % FIELD_COUNT);
                } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                    if (SavePlanetFromFieldsToFile(PLANETS_FILE, fields)) {
                        free(planets);
                        planets = NULL;
                        numPlanets = 0;
                        if (!LoadPlanetsFromTextFile(PLANETS_FILE, &planets, &numPlanets)) {
                            fprintf(stderr, "Failed to reload planets after save.\n");
                            running = 0;
                        } else {
                            ResolveMoonParents(moons, NUM_MOONS, planets, numPlanets);
                        }
                    }
                    addPanelOpen = false;
                    SDL_StopTextInput(window);   // SDL3
                } else if (key == SDLK_ESCAPE) {
                    addPanelOpen = false;
                    SDL_StopTextInput(window);   // SDL3
                }
            }
        } // end events

        // CAMERA & PROJECTION
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        float cx = winW / 2.0f;
        float cy = winH / 2.0f;

        float fov = BASE_FOV * zoom;

        float cosYaw   = cosf(camYaw);
        float sinYaw   = sinf(camYaw);
        float cosPitch = cosf(camPitch);
        float sinPitch = sinf(camPitch);

        // SUN
        ProjectXZ3D(0,0,
            cosYaw, sinYaw,
            cosPitch, sinPitch,
            CAM_DIST, fov,
            cx, cy, camPanX, camPanY,
            &sunScreenX, &sunScreenY, &sunDepth);
        sunScreenRadius = sun.radius * (fov / sunDepth);

        // PLANETS
        for (int i = 0; i < numPlanets; i++) {
            Planet *p = &planets[i];

            p->angle += p->angularSpeed;

            p->worldX = cosf(p->angle) * p->orbitRadius;
            p->worldZ = sinf(p->angle) * p->orbitRadius;

            ProjectXZ3D(
                p->worldX, p->worldZ,
                cosYaw, sinYaw,
                cosPitch, sinPitch,
                CAM_DIST, fov,
                cx, cy, camPanX, camPanY,
                &p->circle.x, &p->circle.y, &p->depth);

            p->screenRadius = p->circle.radius * (fov / p->depth);
        }

        // ASTEROIDS
        for (int i = 0; i < NUM_ASTEROIDS; i++)
            asteroid_angle[i] += asteroid_speed[i];

        // MOONS
        for (int i = 0; i < NUM_MOONS; i++) {
            struct Moon *m = &moons[i];
            m->angle += m->angularSpeed;

            if (m->parentIndex < 0) {
                moonDepth[i] = 1;
                continue;
            }

            Planet *parent = &planets[m->parentIndex];

            float mwx = parent->worldX + cosf(m->angle) * m->orbitRadius;
            float mwz = parent->worldZ + sinf(m->angle) * m->orbitRadius;

            ProjectXZ3D(mwx, mwz,
                cosYaw, sinYaw,
                cosPitch, sinPitch,
                CAM_DIST, fov,
                cx, cy, camPanX, camPanY,
                &m->x, &m->y, &moonDepth[i]);
        }

        // DRAW FRAME
        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        // Orbits
        SDL_SetRenderDrawColor(renderer, 80,80,80,255);
        const int SEG = 48;
        for (int i = 0; i < numPlanets; i++) {
            float r = planets[i].orbitRadius;
            float px=0, py=0;
            int hasPrev = 0;
            for (int s = 0; s <= SEG; s++) {
                float t = (float)s/SEG * 6.283185f;
                float wx = cosf(t)*r;
                float wz = sinf(t)*r;

                float sx, sy, d;
                ProjectXZ3D(wx, wz,
                    cosYaw, sinYaw,
                    cosPitch, sinPitch,
                    CAM_DIST, fov,
                    cx, cy, camPanX, camPanY,
                    &sx, &sy, &d);

                if (hasPrev) SDL_RenderLine(renderer, px, py, sx, sy);
                px = sx; py = sy;
                hasPrev = 1;
            }
        }

        // Asteroids
        SDL_SetRenderDrawColor(renderer, 160,160,160,255);
        for (int i = 0; i < NUM_ASTEROIDS; i++) {
            float wx = cosf(asteroid_angle[i]) * asteroid_radius[i];
            float wz = sinf(asteroid_angle[i]) * asteroid_radius[i];

            float sx, sy, d;
            ProjectXZ3D(wx, wz,
                        cosYaw, sinYaw,
                        cosPitch, sinPitch,
                        CAM_DIST, fov,
                        cx, cy, camPanX, camPanY,
                        &sx, &sy, &d);

            if ((i & 1) == 0)
                SDL_RenderPoint(renderer, sx, sy);
        }

        // Add Planet button
        SDL_SetRenderDrawColor(renderer, 40,40,120,255);
        SDL_RenderFillRect(renderer, &addButton);
        SDL_SetRenderDrawColor(renderer, 220,220,255,255);
        SDL_RenderRect(renderer, &addButton);
        SDL_SetRenderDrawColor(renderer, 255,255,255,255);
        DrawText(renderer, addButton.x + 20, addButton.y + 12, "ADD PLANET", 2.0f);

        // Selected planet outline
        if (selectedPlanet >= 0 && selectedPlanet < numPlanets) {
            Planet *p = &planets[selectedPlanet];
            SDL_SetRenderDrawColor(renderer, 255,255,255,255);
            DrawCircle(renderer, p->circle.x, p->circle.y, p->screenRadius + 6);
        }

        // Sun
        SDL_SetRenderDrawColor(renderer, sun.r, sun.g, sun.b, 255);
        DrawFillCircle(renderer, sunScreenX, sunScreenY, sunScreenRadius);

        // Planets
        for (int i = 0; i < numPlanets; i++) {
            Planet *p = &planets[i];
            SDL_SetRenderDrawColor(renderer, p->circle.r, p->circle.g, p->circle.b, 255);
            DrawFillCircle(renderer, p->circle.x, p->circle.y, p->screenRadius);
        }

        // Moons
        for (int i = 0; i < NUM_MOONS; i++) {
            float r = moons[i].radius * (fov / moonDepth[i]);
            SDL_SetRenderDrawColor(renderer, moons[i].r, moons[i].g, moons[i].b, 255);
            DrawFillCircle(renderer, moons[i].x, moons[i].y, r);
        }

        // Add Planet panel
        if (addPanelOpen) {
            SDL_SetRenderDrawColor(renderer, 0,0,0,160);
            SDL_FRect overlay = {0,0,(float)winW,(float)winH};
            SDL_RenderFillRect(renderer, &overlay);

            SDL_FRect panel = {
                winW * 0.5f - 350.0f,
                winH * 0.5f - 220.0f,
                700.0f,
                440.0f
            };

            SDL_SetRenderDrawColor(renderer, 30,30,30,240);
            SDL_RenderFillRect(renderer, &panel);
            SDL_SetRenderDrawColor(renderer, 220,220,220,255);
            SDL_RenderRect(renderer, &panel);

            DrawText(renderer, panel.x + 20, panel.y + 15, "ADD NEW PLANET", 2.5f);

            float px = panel.x + 30.0f;
            float py = panel.y + 60.0f;
            float rowH = 40.0f;
            float labelWidth = 140.0f;

            for (int i = 0; i < FIELD_COUNT; i++) {
                float y = py + i*rowH;
                // Label
                SDL_SetRenderDrawColor(renderer, 200,200,200,255);
                DrawText(renderer, px, y, fields[i].label, 1.8f);

                SDL_FRect box = {
                    px + labelWidth,
                    y,
                    300.0f,
                    28.0f
                };

                if (i == activeField) {
                    SDL_SetRenderDrawColor(renderer, 80,80,160,255);
                    SDL_RenderFillRect(renderer, &box);
                    SDL_SetRenderDrawColor(renderer, 230,230,255,255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 50,50,50,255);
                    SDL_RenderFillRect(renderer, &box);
                    SDL_SetRenderDrawColor(renderer, 180,180,180,255);
                }
                SDL_RenderRect(renderer, &box);

                SDL_SetRenderDrawColor(renderer, 255,255,255,255);
                DrawText(renderer, box.x + 4, box.y + 5, fields[i].text, 1.8f);
            }

            // Save / Cancel buttons
            SDL_FRect saveBtn = {
                panel.x + 80.0f,
                panel.y + panel.h - 70.0f,
                180.0f,
                40.0f
            };
            SDL_FRect cancelBtn = {
                panel.x + panel.w - 260.0f,
                panel.y + panel.h - 70.0f,
                180.0f,
                40.0f
            };

            SDL_SetRenderDrawColor(renderer, 40,120,40,255);
            SDL_RenderFillRect(renderer, &saveBtn);
            SDL_SetRenderDrawColor(renderer, 220,255,220,255);
            SDL_RenderRect(renderer, &saveBtn);
            SDL_SetRenderDrawColor(renderer, 255,255,255,255);
            DrawText(renderer, saveBtn.x + 40, saveBtn.y + 12, "SAVE", 2.0f);

            SDL_SetRenderDrawColor(renderer, 120,40,40,255);
            SDL_RenderFillRect(renderer, &cancelBtn);
            SDL_SetRenderDrawColor(renderer, 255,220,220,255);
            SDL_RenderRect(renderer, &cancelBtn);
            SDL_SetRenderDrawColor(renderer, 255,255,255,255);
            DrawText(renderer, cancelBtn.x + 25, cancelBtn.y + 12, "CANCEL", 2.0f);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    free(planets);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
