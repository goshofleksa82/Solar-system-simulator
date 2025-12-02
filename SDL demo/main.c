#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "sqlite3.h"
#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define WIDTH  1600
#define HEIGHT 1000

#define NUM_ASTEROIDS 150
#define NUM_MOONS     10

// ============= BASIC STRUCTS =============

struct Circle {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;
};

typedef struct {
    struct Circle circle;  // visual data (radius + fallback color)
    float orbitRadius;     // distance from sun in world space
    float angularSpeed;    // orbital angular speed
    float angle;           // current angle

    float worldX, worldZ;  // 3D world position in XZ orbital plane
    float depth;           // camera depth
    float screenRadius;    // projected radius in pixels

    SDL_Texture *texture;  // planet texture (may be NULL if loading failed)
    char name[64];         // planet name from DB
} Planet;

struct Moon {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;

    int   parentIndex;     // index into planets array (-1 if not found)
    char  parentName[32];  // name of parent planet (to resolve at startup)

    float orbitRadius;     // distance from planet in world space
    float angle;           // current angle
    float angularSpeed;    // speed around planet
};

// ============= CURL MEMORY CHUNK =============

typedef struct {
    unsigned char *data;
    size_t size;
} MemoryChunk;

static size_t CurlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    MemoryChunk *mem = (MemoryChunk *)userp;

    unsigned char *ptr = (unsigned char *)realloc(mem->data, mem->size + realsize);
    if (!ptr) {
        fprintf(stderr, "CurlWriteCallback: realloc failed\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;

    return realsize;
}

// Download image from URL and create SDL_Texture using SDL3_image
static SDL_Texture* LoadTextureFromURL(SDL_Renderer *renderer, const char *url)
{
    if (!url || !*url) {
        return NULL;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "LoadTextureFromURL: curl_easy_init failed\n");
        return NULL;
    }

    MemoryChunk chunk;
    chunk.data = NULL;
    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "sdl3-solar-system/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed for %s: %s\n",
                url, curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.data);
        return NULL;
    }

    curl_easy_cleanup(curl);

    if (!chunk.data || chunk.size == 0) {
        fprintf(stderr, "LoadTextureFromURL: downloaded zero bytes from %s\n", url);
        free(chunk.data);
        return NULL;
    }

    // Wrap downloaded memory in an SDL_IOStream for SDL3_image
    SDL_IOStream *io = SDL_IOFromConstMem(chunk.data, (int)chunk.size);
    if (!io) {
        fprintf(stderr, "SDL_IOFromConstMem failed: %s\n", SDL_GetError());
        free(chunk.data);
        return NULL;
    }

    SDL_Texture *tex = IMG_LoadTexture_IO(renderer, io, true); // closeio = true
    if (!tex) {
        fprintf(stderr, "IMG_LoadTexture_IO failed for '%s': %s\n",
                url, IMG_GetError());
    }

    // SDL_IOFromConstMem does not free our buffer, so we must:
    free(chunk.data);

    return tex;
}

// ============= DRAW HELPERS =============

// Filled circle (for sun, moons, and fallback planets)
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
        int x1 = -xSpan;
        int x2 =  xSpan;
        SDL_RenderLine(renderer, cx + x1, cy + y, cx + x2, cy + y);
    }
}

// Outline circle (for selection highlight)
void DrawCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    if (radius <= 0.5f) {
        SDL_RenderPoint(renderer, cx, cy);
        return;
    }

    float x = radius;
    float y = 0;
    float decision = 1 - x;

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

        if (decision <= 0)
            decision += 2 * y + 1;
        else {
            x--;
            decision += 2 * (y - x) + 1;
        }
    }
}

// ============= 3D PROJECTION =============

/*
   Project a point in the orbital XZ-plane into 2D screen space, with:

   - yaw, pitch (cos/sin precomputed)
   - camDist : distance of camera along -Z
   - fov     : projection scale
   - cx,cy   : screen center
   - panX,panY: screen-space panning

   world point = (worldX, 0, worldZ)
*/
static void ProjectXZ3D(
    float worldX, float worldZ,
    float cosYaw, float sinYaw,
    float cosPitch, float sinPitch,
    float camDist, float fov,
    float cx, float cy,
    float panX, float panY,
    float *outX, float *outY, float *outDepth
){
    // 1) rotate around Y (yaw)
    float x1 =  worldX * cosYaw + worldZ * sinYaw;
    float z1 = -worldX * sinYaw + worldZ * cosYaw;
    float y1 = 0.0f;

    // 2) rotate around X (pitch)
    float y2 =  y1 * cosPitch - z1 * sinPitch;
    float z2 =  y1 * sinPitch + z1 * cosPitch;
    float x2 =  x1;

    // 3) camera is at (0,0,-camDist), looking toward +Z
    float cz = z2 + camDist;
    if (cz < 1.0f) cz = 1.0f;

    float inv = fov / cz;
    *outX = cx + panX + x2 * inv;
    *outY = cy + panY + y2 * inv;

    if (outDepth) *outDepth = cz;
}

// ============= SQLITE: LOAD PLANETS =============

static int LoadPlanetsFromDB(sqlite3 *db, SDL_Renderer *renderer,
                             Planet **outPlanets, int *outCount)
{
    const char *sql =
        "SELECT name, orbit_radius, angular_speed, radius, "
        "       color_r, color_g, color_b, texture_url "
        "FROM planets "
        "ORDER BY orbit_radius ASC;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite3_prepare_v2 failed: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    Planet *planets = NULL;
    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *nameText  = sqlite3_column_text(stmt, 0);
        double orbitRadius             = sqlite3_column_double(stmt, 1);
        double angularSpeed            = sqlite3_column_double(stmt, 2);
        double radius                  = sqlite3_column_double(stmt, 3);
        int colorR                     = sqlite3_column_int(stmt, 4);
        int colorG                     = sqlite3_column_int(stmt, 5);
        int colorB                     = sqlite3_column_int(stmt, 6);
        const unsigned char *urlText   = sqlite3_column_text(stmt, 7);

        Planet *tmp = (Planet *)realloc(planets, sizeof(Planet) * (count + 1));
        if (!tmp) {
            fprintf(stderr, "LoadPlanetsFromDB: realloc failed\n");
            free(planets);
            sqlite3_finalize(stmt);
            return 0;
        }
        planets = tmp;

        Planet *p = &planets[count];
        memset(p, 0, sizeof(*p));

        if (nameText) {
            snprintf(p->name, sizeof(p->name), "%s", (const char *)nameText);
        } else {
            snprintf(p->name, sizeof(p->name), "Planet%d", count);
        }

        p->orbitRadius  = (float)orbitRadius;
        p->angularSpeed = (float)angularSpeed;
        p->angle        = 0.0f;

        p->circle.x      = 0.0f;
        p->circle.y      = 0.0f;
        p->circle.radius = (float)radius;
        p->circle.r      = (Uint8)colorR;
        p->circle.g      = (Uint8)colorG;
        p->circle.b      = (Uint8)colorB;

        p->worldX = 0.0f;
        p->worldZ = p->orbitRadius;
        p->depth  = 1.0f;

        p->texture = NULL;

        if (urlText) {
            const char *url = (const char *)urlText;
            p->texture = LoadTextureFromURL(renderer, url);
            if (!p->texture) {
                fprintf(stderr,
                        "Warning: failed to load texture for %s from %s, using color fallback.\n",
                        p->name, url);
            } else {
                printf("Loaded texture for %s from %s\n", p->name, url);
            }
        }

        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "sqlite3_step ended with error: %d\n", rc);
        sqlite3_finalize(stmt);
        free(planets);
        return 0;
    }

    sqlite3_finalize(stmt);

    *outPlanets = planets;
    *outCount   = count;

    printf("Loaded %d planets from DB.\n", count);
    return 1;
}

// ============= MOON PARENT RESOLUTION =============

static void ResolveMoonParents(struct Moon *moons, int numMoons,
                               Planet *planets, int numPlanets)
{
    for (int i = 0; i < numMoons; i++) {
        struct Moon *m = &moons[i];
        m->parentIndex = -1;
        for (int p = 0; p < numPlanets; p++) {
            if (strcmp(m->parentName, planets[p].name) == 0) {
                m->parentIndex = p;
                break;
            }
        }
        if (m->parentIndex < 0) {
            fprintf(stderr,
                    "Warning: moon with parentName '%s' has no matching planet in DB.\n",
                    m->parentName);
        }
    }
}

// ============= MAIN =============

int main(int argc, char *argv[])
{
    // --- SDL core ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // --- SDL_image ---
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
    if ((IMG_Init(imgFlags) & imgFlags) != imgFlags) {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // --- libcurl ---
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        fprintf(stderr, "curl_global_init failed\n");
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "3D-ish Solar System (DB + Internet Textures)",
        WIDTH, HEIGHT,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        curl_global_cleanup();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        curl_global_cleanup();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // --- Open SQLite DB ---
    sqlite3 *db = NULL;
    if (sqlite3_open("planets.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Failed to open DB 'planets.db': %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        curl_global_cleanup();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // --- Load planets from DB ---
    Planet *planets = NULL;
    int numPlanets  = 0;
    if (!LoadPlanetsFromDB(db, renderer, &planets, &numPlanets) || numPlanets == 0) {
        fprintf(stderr, "No planets loaded from DB, exiting.\n");
        sqlite3_close(db);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        curl_global_cleanup();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // ---- Camera parameters ----
    const float BASE_FOV  = 800.0f;
    const float CAM_DIST  = 1500.0f;

    float zoom       = 0.7f;
    float zoomSpeed  = 0.1f;

    float camYaw     = 0.5f;
    float camPitch   = 0.5f;
    float camPanX    = 0.0f;
    float camPanY    = 0.0f;

    int mouseLeftDown  = 0;
    int mouseRightDown = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;

    const float ROTATE_SENS = 0.005f;
    const float PAN_SENS    = 1.0f;

    srand(43);

    // ---------- SUN ----------
    struct Circle sun = {0, 0, 30.0f, 255, 255, 0};
    float sunScreenX = 0.0f, sunScreenY = 0.0f, sunDepth = 1.0f, sunScreenRadius = sun.radius;

    // ---------- ASTEROID BELT ----------
    float asteroid_radius[NUM_ASTEROIDS];
    float asteroid_angle[NUM_ASTEROIDS];
    float asteroid_speed[NUM_ASTEROIDS];

    float innerBelt = 400.0f;
    float outerBelt = 500.0f;

    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        asteroid_radius[i] = innerBelt + (float)rand() / RAND_MAX * (outerBelt - innerBelt);
        asteroid_angle[i]  = ((float)rand() / RAND_MAX) * 6.283185f;
        asteroid_speed[i]  = 0.010f + ((float)rand() / RAND_MAX) * 0.005f;
    }

    // ---------- MOONS ----------
    struct Moon moons[NUM_MOONS] = {
        // Earth
        {0,0, 3, 210,210,210, -1, "Earth",  18.0f, 0.0f,  0.08f},
        // Mars
        {0,0, 2, 200,200,200, -1, "Mars",   10.0f, 1.0f,  0.10f},
        {0,0, 2, 160,160,160, -1, "Mars",   15.0f, 2.0f,  0.07f},
        // Jupiter
        {0,0, 4, 255,200,180, -1, "Jupiter",30.0f, 0.0f,  0.09f},
        {0,0, 3, 180,220,255, -1, "Jupiter",40.0f, 1.0f,  0.07f},
        {0,0, 5, 220,220,220, -1, "Jupiter",52.0f, 2.0f,  0.05f},
        {0,0, 4, 200,200,200, -1, "Jupiter",65.0f, 3.0f,  0.04f},
        // Saturn
        {0,0, 4, 230,210,160, -1, "Saturn", 28.0f, 0.5f,  0.06f},
        // Uranus
        {0,0, 3, 200,220,255, -1, "Uranus", 24.0f, 1.2f,  0.06f},
        // Neptune
        {0,0, 3, 180,200,255, -1, "Neptune",22.0f, 2.0f,  0.06f}
    };

    float moonDepth[NUM_MOONS];

    int selectedPlanet = -1;

    // Resolve moon parent indices based on names from DB
    ResolveMoonParents(moons, NUM_MOONS, planets, numPlanets);

    int running = 1;
    SDL_Event e;

    while (running)
    {
        // ---------- EVENTS ----------
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = 0;
            }
            else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                if (e.wheel.y > 0)      zoom += zoomSpeed;
                else if (e.wheel.y < 0) zoom -= zoomSpeed;

                if (zoom < 0.2f) zoom = 0.2f;
                if (zoom > 5.0f) zoom = 5.0f;
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouseLeftDown = 1;
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    mouseRightDown = 1;
                }
                lastMouseX = (int)e.button.x;
                lastMouseY = (int)e.button.y;

                // click selection
                float mx = (float)e.button.x;
                float my = (float)e.button.y;

                int clicked = -1;
                for (int i = 0; i < numPlanets; i++) {
                    float dx = mx - planets[i].circle.x;
                    float dy = my - planets[i].circle.y;
                    float r  = planets[i].screenRadius;
                    if (r > 0.0f && (dx*dx + dy*dy) <= r * r) {
                        clicked = i;
                    }
                }
                selectedPlanet = clicked;
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouseLeftDown = 0;
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    mouseRightDown = 0;
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                int mx = (int)e.motion.x;
                int my = (int)e.motion.y;
                int dx = mx - lastMouseX;
                int dy = my - lastMouseY;
                lastMouseX = mx;
                lastMouseY = my;

                if (mouseLeftDown) {
                    camYaw   += dx * ROTATE_SENS;
                    camPitch += dy * ROTATE_SENS;
                    if (camPitch >  1.5f) camPitch =  1.5f;
                    if (camPitch < -1.5f) camPitch = -1.5f;
                }
                if (mouseRightDown) {
                    camPanX += dx * PAN_SENS;
                    camPanY += dy * PAN_SENS;
                }
            }
        }

        // ---------- WINDOW / CAMERA ----------
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        float cx = winW / 2.0f;
        float cy = winH / 2.0f;

        float fov = BASE_FOV * zoom;

        float cosYaw   = cosf(camYaw);
        float sinYaw   = sinf(camYaw);
        float cosPitch = cosf(camPitch);
        float sinPitch = sinf(camPitch);

        // Sun at origin
        ProjectXZ3D(0.0f, 0.0f,
                    cosYaw, sinYaw,
                    cosPitch, sinPitch,
                    CAM_DIST, fov,
                    cx, cy, camPanX, camPanY,
                    &sunScreenX, &sunScreenY, &sunDepth);
        sunScreenRadius = sun.radius * (fov / sunDepth);

        // ---------- PLANETS ----------
        for (int i = 0; i < numPlanets; i++) {
            Planet *p = &planets[i];

            p->angle += p->angularSpeed;

            float wx = cosf(p->angle) * p->orbitRadius;
            float wz = sinf(p->angle) * p->orbitRadius;

            p->worldX = wx;
            p->worldZ = wz;

            float sx, sy, depth;
            ProjectXZ3D(wx, wz,
                        cosYaw, sinYaw,
                        cosPitch, sinPitch,
                        CAM_DIST, fov,
                        cx, cy, camPanX, camPanY,
                        &sx, &sy, &depth);

            p->circle.x = sx;
            p->circle.y = sy;
            p->depth = depth;
            p->screenRadius = p->circle.radius * (fov / depth);
        }

        // ---------- ASTEROIDS ----------
        for (int i = 0; i < NUM_ASTEROIDS; i++) {
            asteroid_angle[i] += asteroid_speed[i];
        }

        // ---------- MOONS ----------
        for (int i = 0; i < NUM_MOONS; i++) {
            struct Moon *m = &moons[i];
            m->angle += m->angularSpeed;

            if (m->parentIndex < 0 || m->parentIndex >= numPlanets) {
                moonDepth[i] = 1.0f;
                continue;
            }

            Planet *parent = &planets[m->parentIndex];

            float pwx = parent->worldX;
            float pwz = parent->worldZ;

            float mwx = pwx + cosf(m->angle) * m->orbitRadius;
            float mwz = pwz + sinf(m->angle) * m->orbitRadius;

            float sx, sy, depth;
            ProjectXZ3D(mwx, mwz,
                        cosYaw, sinYaw,
                        cosPitch, sinPitch,
                        CAM_DIST, fov,
                        cx, cy, camPanX, camPanY,
                        &sx, &sy, &depth);

            m->x = sx;
            m->y = sy;
            moonDepth[i] = depth;
        }

        // ---------- DRAW ----------
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Orbits
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        const int ORBIT_SEGMENTS = 48;
        for (int i = 0; i < numPlanets; i++) {
            float r = planets[i].orbitRadius;
            float prevX = 0, prevY = 0;
            int hasPrev = 0;

            for (int seg = 0; seg <= ORBIT_SEGMENTS; seg++) {
                float t = (float)seg / ORBIT_SEGMENTS * 6.283185f;
                float wx = cosf(t) * r;
                float wz = sinf(t) * r;

                float sx, sy, depth;
                ProjectXZ3D(wx, wz,
                            cosYaw, sinYaw,
                            cosPitch, sinPitch,
                            CAM_DIST, fov,
                            cx, cy, camPanX, camPanY,
                            &sx, &sy, &depth);

                if (hasPrev) {
                    SDL_RenderLine(renderer, prevX, prevY, sx, sy);
                }
                prevX = sx;
                prevY = sy;
                hasPrev = 1;
            }
        }

        // Asteroid belt
        SDL_SetRenderDrawColor(renderer, 140, 140, 140, 255);
        for (int i = 0; i < NUM_ASTEROIDS; i++) {
            float wx = cosf(asteroid_angle[i]) * asteroid_radius[i];
            float wz = sinf(asteroid_angle[i]) * asteroid_radius[i];

            float sx, sy, depth;
            ProjectXZ3D(wx, wz,
                        cosYaw, sinYaw,
                        cosPitch, sinPitch,
                        CAM_DIST, fov,
                        cx, cy, camPanX, camPanY,
                        &sx, &sy, &depth);

            if ((i & 1) == 0)
                SDL_RenderPoint(renderer, sx, sy);
        }

        // Highlight selected planet
        if (selectedPlanet >= 0 && selectedPlanet < numPlanets) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            float hr = planets[selectedPlanet].screenRadius + 6.0f;
            DrawCircle(renderer,
                       planets[selectedPlanet].circle.x,
                       planets[selectedPlanet].circle.y,
                       hr);
        }

        // Sun
        SDL_SetRenderDrawColor(renderer, sun.r, sun.g, sun.b, 255);
        DrawFillCircle(renderer, sunScreenX, sunScreenY, sunScreenRadius);

        // Planets: texture if available, otherwise colored circle
        for (int i = 0; i < numPlanets; i++) {
            Planet *p = &planets[i];
            if (p->screenRadius <= 0.0f) continue;

            if (p->texture) {
                SDL_FRect dst;
                dst.w = p->screenRadius * 2.0f;
                dst.h = p->screenRadius * 2.0f;
                dst.x = p->circle.x - p->screenRadius;
                dst.y = p->circle.y - p->screenRadius;
                SDL_RenderTexture(renderer, p->texture, NULL, &dst);
            } else {
                SDL_SetRenderDrawColor(renderer,
                                       p->circle.r,
                                       p->circle.g,
                                       p->circle.b,
                                       255);
                DrawFillCircle(renderer,
                               p->circle.x,
                               p->circle.y,
                               p->screenRadius);
            }
        }

        // Moons
        for (int i = 0; i < NUM_MOONS; i++) {
            if (moonDepth[i] <= 0.0f) continue;
            float r = moons[i].radius * (fov / moonDepth[i]);
            SDL_SetRenderDrawColor(renderer,
                                   moons[i].r,
                                   moons[i].g,
                                   moons[i].b,
                                   255);
            DrawFillCircle(renderer,
                           moons[i].x,
                           moons[i].y,
                           r);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    // ---------- CLEANUP ----------
    for (int i = 0; i < numPlanets; i++) {
        if (planets[i].texture) {
            SDL_DestroyTexture(planets[i].texture);
        }
    }
    free(planets);

    sqlite3_close(db);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    curl_global_cleanup();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
