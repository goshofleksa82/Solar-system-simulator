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
// STRUCTS
// =========================================

struct Circle {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;
};

typedef struct {
    struct Circle circle;
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
// DRAW HELPERS
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
// Format: name orbit_radius angular_speed radius color_r color_g color_b
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

        Planet *tmp = realloc(planets, sizeof(Planet) * (count + 1));
        if (!tmp) {
            fclose(fp);
            free(planets);
            return 0;
        }
        planets = tmp;

        Planet *p = &planets[count];
        memset(p, 0, sizeof(*p));

        strcpy(p->name, name);
        p->orbitRadius  = orbitRadius;
        p->angularSpeed = angularSpeed;
        p->circle.radius = radius;
        p->circle.r = r;
        p->circle.g = g;
        p->circle.b = b;

        count++;
    }

    fclose(fp);

    *outPlanets = planets;
    *outCount   = count;

    return 1;
}

// =========================================
// RESOLVE MOON PARENTS
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
    }
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
        "3D-ish Solar System (No Textures)",
        WIDTH, HEIGHT,
        SDL_WINDOW_RESIZABLE);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

    // Load planets
    Planet *planets = NULL;
    int numPlanets = 0;

    if (!LoadPlanetsFromTextFile("planets.txt", &planets, &numPlanets)) {
        fprintf(stderr, "Failed to load planets.\n");
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

    // ============================
    // ASTEROID BELT BETWEEN MARS & JUPITER
    // Mars = 150, Jupiter = 250
    // ============================
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

    while (running)
    {
        // EVENTS
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = 0;

            else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                zoom += (e.wheel.y > 0 ? 0.1f : -0.1f);
                if (zoom < 0.2f) zoom = 0.2f;
                if (zoom > 5.0f) zoom = 5.0f;
            }

            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) mouseLeft = 1;
                if (e.button.button == SDL_BUTTON_RIGHT) mouseRight = 1;
                lastX = e.button.x;
                lastY = e.button.y;

                // Click selection
                selectedPlanet = -1;
                float mx = e.button.x;
                float my = e.button.y;

                for (int i = 0; i < numPlanets; i++) {
                    float dx = mx - planets[i].circle.x;
                    float dy = my - planets[i].circle.y;
                    if (dx*dx + dy*dy <= planets[i].screenRadius * planets[i].screenRadius)
                        selectedPlanet = i;
                }
            }

            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (e.button.button == SDL_BUTTON_LEFT) mouseLeft = 0;
                if (e.button.button == SDL_BUTTON_RIGHT) mouseRight = 0;
            }

            else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                int mx = e.motion.x, my = e.motion.y;
                int dx = mx - lastX;
                int dy = my - lastY;
                lastX = mx;
                lastY = my;

                if (mouseLeft) {
                    camYaw += dx * ROTATE_SENS;
                    camPitch += dy * ROTATE_SENS;
                    if (camPitch >  1.5f) camPitch = 1.5f;
                    if (camPitch < -1.5f) camPitch = -1.5f;
                }
                if (mouseRight) {
                    camPanX += dx * PAN_SENS;
                    camPanY += dy * PAN_SENS;
                }
            }
        }

        // CAMERA
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        float cx = winW/2.0f;
        float cy = winH/2.0f;

        float fov = BASE_FOV * zoom;

        float cosYaw   = cosf(camYaw);
        float sinYaw   = sinf(camYaw);
        float cosPitch = cosf(camPitch);
        float sinPitch = sinf(camPitch);

        // SUN projection
        ProjectXZ3D(0,0,
            cosYaw, sinYaw,
            cosPitch, sinPitch,
            CAM_DIST, fov,
            cx, cy, camPanX, camPanY,
            &sunScreenX, &sunScreenY, &sunDepth);
        sunScreenRadius = sun.radius * (fov / sunDepth);

        // PLANET positions
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

        // Planet orbits
        SDL_SetRenderDrawColor(renderer, 80,80,80,255);
        const int SEG = 48;

        for (int i = 0; i < numPlanets; i++) {
            float r = planets[i].orbitRadius;
            float px, py;
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

                if (hasPrev)
                    SDL_RenderLine(renderer, px, py, sx, sy);

                px = sx;
                py = sy;
                hasPrev = 1;
            }
        }

        // ASTEROIDS
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

        // SELECTED PLANET
        if (selectedPlanet >= 0) {
            Planet *p = &planets[selectedPlanet];
            SDL_SetRenderDrawColor(renderer, 255,255,255,255);
            DrawCircle(renderer, p->circle.x, p->circle.y, p->screenRadius + 6);
        }

        // SUN
        SDL_SetRenderDrawColor(renderer, sun.r, sun.g, sun.b, 255);
        DrawFillCircle(renderer, sunScreenX, sunScreenY, sunScreenRadius);

        // PLANETS
        for (int i = 0; i < numPlanets; i++) {
            Planet *p = &planets[i];
            SDL_SetRenderDrawColor(renderer, p->circle.r, p->circle.g, p->circle.b, 255);
            DrawFillCircle(renderer, p->circle.x, p->circle.y, p->screenRadius);
        }

        // MOONS
        for (int i = 0; i < NUM_MOONS; i++) {
            float r = moons[i].radius * (fov / moonDepth[i]);
            SDL_SetRenderDrawColor(renderer, moons[i].r, moons[i].g, moons[i].b, 255);
            DrawFillCircle(renderer, moons[i].x, moons[i].y, r);
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
