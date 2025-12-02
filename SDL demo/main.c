#include <SDL3/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "sqlite3.h"



#define WIDTH 1600
#define HEIGHT 1000

#define NUM_PLANETS    8
#define NUM_ASTEROIDS  150   // reduced for performance
#define NUM_MOONS      10

struct Circle {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;
};

struct Moon {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;

    int parentIndex;     // index into planets array
    float orbitRadius;   // distance from planet (world space)
    float angle;         // current angle
    float angularSpeed;  // speed around planet
};

// ----- Faster filled circle (scanline) -----
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

// ----- Outline circle (kept as is, not too heavy) -----
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

/*
   Project a point in the orbital XZ-plane into 2D screen space, with:

   - yaw, pitch (but we pass in cos/sin to avoid recomputing)
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

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "3D-ish Solar System (Optimized)",
        WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE
    );
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

    int running = 1;
    SDL_Event e;

    // ---- Camera parameters ----
    const float BASE_FOV  = 800.0f;  // base projection scale
    const float CAM_DIST  = 1500.0f; // distance of camera along -Z

    float zoom       = 0.7f;         // start slightly zoomed out
    float zoomSpeed  = 0.1f;

    float camYaw     = 0.5f;         // radians, left-right orbit
    float camPitch   = 0.5f;         // radians, up-down orbit
    float camPanX    = 0.0f;         // screen-space panning (pixels)
    float camPanY    = 0.0f;

    // Mouse state for dragging
    int mouseLeftDown  = 0;
    int mouseRightDown = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;

    // Sensitivities
    const float ROTATE_SENS = 0.005f;   // mouse pixels -> radians
    const float PAN_SENS    = 1.0f;     // mouse pixels -> screen pan pixels

    srand(43); // stable randomness

    // ---------- STAR ----------
    struct Circle sun = {0, 0, 30.0f, 255, 255, 0};

    // ---------- PLANETS ----------
    // Slightly larger radii for visibility
    struct Circle planets[NUM_PLANETS] = {
        {0, 0, 5,   200, 200, 200}, // Mercury
        {0, 0, 7,   255, 200, 130}, // Venus
        {0, 0, 8,     0, 150, 255}, // Earth
        {0, 0, 5,   255,  50,  50}, // Mars
        {0, 0, 14, 255, 180,  80},  // Jupiter
        {0, 0, 12, 200, 200, 150},  // Saturn
        {0, 0, 10, 100, 200, 255},  // Uranus
        {0, 0, 10, 100, 100, 255}   // Neptune
    };

    // Realistic-ish spacing (scaled)
    float orbit_radius[NUM_PLANETS] = {
        100,   // Mercury
        150,   // Venus
        200,   // Earth
        300,   // Mars
        700,   // Jupiter
        1000,  // Saturn
        1400,  // Uranus
        1800   // Neptune
    };

    float angular_speed[NUM_PLANETS] = {
        0.03f,  // Mercury
        0.02f,  // Venus
        0.015f, // Earth
        0.012f, // Mars
        0.009f, // Jupiter
        0.007f, // Saturn
        0.005f, // Uranus
        0.004f  // Neptune
    };

    float angle[NUM_PLANETS] = {0};

    // World X/Z positions + depth/screen radius
    float planetWorldX[NUM_PLANETS];
    float planetWorldZ[NUM_PLANETS];
    float planetDepth[NUM_PLANETS];
    float planetScreenRadius[NUM_PLANETS];

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
        // Earth (idx 2)
        {0,0, 3, 210,210,210, 2, 18.0f, 0.0f,  0.08f}, // Moon

        // Mars (idx 3)
        {0,0, 2, 200,200,200, 3, 10.0f, 1.0f,  0.10f}, // Phobos
        {0,0, 2, 160,160,160, 3, 15.0f, 2.0f,  0.07f}, // Deimos

        // Jupiter (idx 4)
        {0,0, 4, 255,200,180, 4, 30.0f, 0.0f,  0.09f}, // Io
        {0,0, 3, 180,220,255, 4, 40.0f, 1.0f,  0.07f}, // Europa
        {0,0, 5, 220,220,220, 4, 52.0f, 2.0f,  0.05f}, // Ganymede
        {0,0, 4, 200,200,200, 4, 65.0f, 3.0f,  0.04f}, // Callisto

        // Saturn (idx 5)
        {0,0, 4, 230,210,160, 5, 28.0f, 0.5f,  0.06f}, // Titan

        // Uranus (idx 6)
        {0,0, 3, 200,220,255, 6, 24.0f, 1.2f,  0.06f}, // Titania

        // Neptune (idx 7)
        {0,0, 3, 180,200,255, 7, 22.0f, 2.0f,  0.06f}  // Triton
    };

    float moonDepth[NUM_MOONS];

    int selectedPlanet = -1;

    // Sun screen position & radius
    float sunScreenX = 0.0f, sunScreenY = 0.0f, sunDepth = 1.0f, sunScreenRadius = sun.radius;

    // ---------- MAIN LOOP ----------
    while (running)
    {
        // EVENTS
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
                lastMouseX = e.button.x;
                lastMouseY = e.button.y;

                // click selection
                float mx = (float)e.button.x;
                float my = (float)e.button.y;

                int clicked = -1;
                for (int i = 0; i < NUM_PLANETS; i++) {
                    float dx = mx - planets[i].x;
                    float dy = my - planets[i].y;
                    float r  = planetScreenRadius[i];
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
                int mx = e.motion.x;
                int my = e.motion.y;
                int dx = mx - lastMouseX;
                int dy = my - lastMouseY;
                lastMouseX = mx;
                lastMouseY = my;

                if (mouseLeftDown) {
                    // rotate camera
                    camYaw   += dx * ROTATE_SENS;
                    camPitch += dy * ROTATE_SENS;

                    // clamp pitch to avoid flipping over
                    if (camPitch >  1.5f) camPitch =  1.5f;
                    if (camPitch < -1.5f) camPitch = -1.5f;
                }
                if (mouseRightDown) {
                    // pan camera in screen space
                    camPanX += dx * PAN_SENS;
                    camPanY += dy * PAN_SENS;
                }
            }
        }

        // WINDOW / CENTER
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        float cx = winW / 2.0f;
        float cy = winH / 2.0f;

        float fov = BASE_FOV * zoom;

        // Precompute trig for this frame
        float cosYaw   = cosf(camYaw);
        float sinYaw   = sinf(camYaw);
        float cosPitch = cosf(camPitch);
        float sinPitch = sinf(camPitch);

        // --- SUN at (0,0) in XZ plane ---
        ProjectXZ3D(0.0f, 0.0f,
                    cosYaw, sinYaw,
                    cosPitch, sinPitch,
                    CAM_DIST, fov,
                    cx, cy, camPanX, camPanY,
                    &sunScreenX, &sunScreenY, &sunDepth);
        sunScreenRadius = sun.radius * (fov / sunDepth);

        // UPDATE PLANETS (world positions + projection)
        for (int i = 0; i < NUM_PLANETS; i++) {
            angle[i] += angular_speed[i];

            float wx = cosf(angle[i]) * orbit_radius[i];
            float wz = sinf(angle[i]) * orbit_radius[i];

            planetWorldX[i] = wx;
            planetWorldZ[i] = wz;

            float sx, sy, depth;
            ProjectXZ3D(wx, wz,
                        cosYaw, sinYaw,
                        cosPitch, sinPitch,
                        CAM_DIST, fov,
                        cx, cy, camPanX, camPanY,
                        &sx, &sy, &depth);

            planets[i].x = sx;
            planets[i].y = sy;
            planetDepth[i] = depth;
            planetScreenRadius[i] = planets[i].radius * (fov / depth);
        }

        // UPDATE ASTEROIDS (angle only)
        for (int i = 0; i < NUM_ASTEROIDS; i++) {
            asteroid_angle[i] += asteroid_speed[i];
        }

        // UPDATE MOONS (relative to parent planet in world space)
        for (int i = 0; i < NUM_MOONS; i++) {
            struct Moon *m = &moons[i];
            m->angle += m->angularSpeed;

            int pIdx = m->parentIndex;
            float pwx = planetWorldX[pIdx];
            float pwz = planetWorldZ[pIdx];

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

        // DRAW
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // orbits (projected ellipses) with fewer segments
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        const int ORBIT_SEGMENTS = 48;   // reduced from 120
        for (int i = 0; i < NUM_PLANETS; i++) {
            float r = orbit_radius[i];
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

        // asteroid belt (optionally thin out by drawing every 2nd)
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

            // draw only half to reduce overdraw
            if ((i & 1) == 0)
                SDL_RenderPoint(renderer, sx, sy);
        }

        // highlight selected planet (screen-space ring)
        if (selectedPlanet >= 0 && selectedPlanet < NUM_PLANETS) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            float hr = planetScreenRadius[selectedPlanet] + 6.0f;
            DrawCircle(renderer,
                       planets[selectedPlanet].x,
                       planets[selectedPlanet].y,
                       hr);
        }

        // sun
        SDL_SetRenderDrawColor(renderer, sun.r, sun.g, sun.b, 255);
        DrawFillCircle(renderer, sunScreenX, sunScreenY, sunScreenRadius);

        // planets
        for (int i = 0; i < NUM_PLANETS; i++) {
            SDL_SetRenderDrawColor(renderer,
                                   planets[i].r,
                                   planets[i].g,
                                   planets[i].b,
                                   255);
            DrawFillCircle(renderer,
                           planets[i].x,
                           planets[i].y,
                           planetScreenRadius[i]);
        }

        // moons
        for (int i = 0; i < NUM_MOONS; i++) {
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
        SDL_Delay(16); // ~60 FPS target
    }

    SDL_Quit();
    return 0;
}
