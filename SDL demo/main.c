#include <SDL3/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define WIDTH 1600
#define HEIGHT 1000

#define NUM_PLANETS    8
#define NUM_ASTEROIDS  300
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
    float orbitRadius;   // distance from planet (in world space)
    float angle;         // current angle
    float angularSpeed;  // speed around planet
};

// ----- Filled circle -----
void DrawFillCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    if (radius <= 0.5f) {
        SDL_RenderPoint(renderer, cx, cy);
        return;
    }

    for (int w = (int)(-radius); w <= (int)(radius); w++) {
        for (int h = (int)(-radius); h <= (int)(radius); h++) {
            if ((w*w + h*h) <= radius * radius) {
                SDL_RenderPoint(renderer, cx + w, cy + h);
            }
        }
    }
}

// ----- Outline circle (orbit / highlight) -----
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

// ----- Project world (x,z) into screen space with tilt + perspective -----
static void ProjectXZ(
    float worldX, float worldZ,
    float tiltSin, float tiltCos,
    float camDist, float fov,
    float screenCX, float screenCY,
    float *outX, float *outY, float *outDepth
){
    // Rotate around X-axis to tilt the orbital plane
    float wx = worldX;
    float wy = -worldZ * tiltSin; // y is derived from z
    float wz =  worldZ * tiltCos;

    // Camera is at (0,0,-camDist), looking toward +Z
    float cz = wz + camDist;
    if (cz < 1.0f) cz = 1.0f; // avoid divide by zero / flipping

    float inv = fov / cz;
    *outX = screenCX + wx * inv;
    *outY = screenCY + wy * inv;

    if (outDepth) {
        *outDepth = cz;
    }
}

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "3D-ish Solar System (Perspective + Zoom)",
        WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE
    );
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

    int running = 1;
    SDL_Event e;

    // ---- "3D camera" parameters ----
    const float BASE_FOV = 800.0f;  // projection factor
    const float CAM_DIST = 1200.0f; // camera distance along -Z
    const float TILT_ANGLE = 0.6f;  // radians ~ 34 degrees
    const float tiltSin = sinf(TILT_ANGLE);
    const float tiltCos = cosf(TILT_ANGLE);

    float zoom = 1.0f;       // zoom (scales FOV)
    float zoomSpeed = 0.1f;  // mouse wheel step

    srand(43); // stable randomness

    // ---------- STAR ----------
    struct Circle sun = {0, 0, 30.0f, 255, 255, 0};

    // ---------- PLANETS ----------
    // Slightly larger radii for visibility
    // 0-Mercury, 1-Venus, 2-Earth, 3-Mars, 4-Jupiter, 5-Saturn, 6-Uranus, 7-Neptune
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

    // More realistic interplanetary spacing (scaled)
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
        0.03f,  // Mercury – fast
        0.02f,  // Venus
        0.015f, // Earth
        0.012f, // Mars
        0.009f, // Jupiter
        0.007f, // Saturn
        0.005f, // Uranus
        0.004f  // Neptune – slow
    };

    float angle[NUM_PLANETS] = {0};

    // World-space (x,z) positions and depth for planets
    float planetWorldX[NUM_PLANETS];
    float planetWorldZ[NUM_PLANETS];
    float planetDepth[NUM_PLANETS];
    float planetScreenRadius[NUM_PLANETS];

    // ---------- ASTEROID BELT ----------
    float asteroid_radius[NUM_ASTEROIDS];
    float asteroid_angle[NUM_ASTEROIDS];
    float asteroid_speed[NUM_ASTEROIDS];

    // Place belt between Mars and Jupiter (scaled)
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

    int selectedPlanet = -1; // -1 = none

    // Variables for sun in screen space
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
                if (e.wheel.y > 0)      zoom += zoomSpeed;   // scroll up = zoom in
                else if (e.wheel.y < 0) zoom -= zoomSpeed;   // scroll down = zoom out

                if (zoom < 0.2f) zoom = 0.2f;   // prevent too much zoom-out
                if (zoom > 5.0f) zoom = 5.0f;   // prevent extreme zoom-in
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float mx = (float)e.button.x;
                float my = (float)e.button.y;

                int clicked = -1;
                // check planets (using screen coords + projected radius)
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
        }

        // WINDOW / CENTER
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        float cx = winW / 2.0f;
        float cy = winH / 2.0f;

        float fov = BASE_FOV * zoom;

        // --- SUN in world space is at (0,0) in the orbital plane (x,z) ---
        ProjectXZ(0.0f, 0.0f, tiltSin, tiltCos, CAM_DIST, fov, cx, cy,
                  &sunScreenX, &sunScreenY, &sunDepth);
        sunScreenRadius = sun.radius * (fov / sunDepth);

        // UPDATE PLANETS (world positions + projection)
        for (int i = 0; i < NUM_PLANETS; i++) {
            angle[i] += angular_speed[i];

            // orbital plane: x-z
            float wx = cosf(angle[i]) * orbit_radius[i];
            float wz = sinf(angle[i]) * orbit_radius[i];

            planetWorldX[i] = wx;
            planetWorldZ[i] = wz;

            float sx, sy, depth;
            ProjectXZ(wx, wz, tiltSin, tiltCos, CAM_DIST, fov, cx, cy,
                      &sx, &sy, &depth);

            planets[i].x = sx;
            planets[i].y = sy;
            planetDepth[i] = depth;
            planetScreenRadius[i] = planets[i].radius * (fov / depth);
        }

        // UPDATE ASTEROIDS (angle only; projected in draw)
        for (int i = 0; i < NUM_ASTEROIDS; i++) {
            asteroid_angle[i] += asteroid_speed[i];
        }

        // UPDATE MOONS (relative to their parent planet in world space)
        for (int i = 0; i < NUM_MOONS; i++) {
            struct Moon *m = &moons[i];
            m->angle += m->angularSpeed;

            int pIdx = m->parentIndex;
            float pwx = planetWorldX[pIdx];
            float pwz = planetWorldZ[pIdx];

            float mwx = pwx + cosf(m->angle) * m->orbitRadius;
            float mwz = pwz + sinf(m->angle) * m->orbitRadius;

            float sx, sy, depth;
            ProjectXZ(mwx, mwz, tiltSin, tiltCos, CAM_DIST, fov, cx, cy,
                      &sx, &sy, &depth);

            m->x = sx;
            m->y = sy;
            moonDepth[i] = depth;
        }

        // DRAW
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // orbits (3D projected ellipses)
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        const int ORBIT_SEGMENTS = 120;
        for (int i = 0; i < NUM_PLANETS; i++) {
            float r = orbit_radius[i];
            for (int seg = 0; seg < ORBIT_SEGMENTS; seg++) {
                float t0 = (float)seg       / ORBIT_SEGMENTS * 6.283185f;
                float t1 = (float)(seg + 1) / ORBIT_SEGMENTS * 6.283185f;

                float wx0 = cosf(t0) * r;
                float wz0 = sinf(t0) * r;
                float wx1 = cosf(t1) * r;
                float wz1 = sinf(t1) * r;

                float x0, y0, d0;
                float x1, y1, d1;
                ProjectXZ(wx0, wz0, tiltSin, tiltCos, CAM_DIST, fov, cx, cy,
                          &x0, &y0, &d0);
                ProjectXZ(wx1, wz1, tiltSin, tiltCos, CAM_DIST, fov, cx, cy,
                          &x1, &y1, &d1);

                SDL_RenderLine(renderer, x0, y0, x1, y1);
            }
        }

        // asteroid belt (3D projected ring)
        SDL_SetRenderDrawColor(renderer, 140, 140, 140, 255);
        for (int i = 0; i < NUM_ASTEROIDS; i++) {
            float wx = cosf(asteroid_angle[i]) * asteroid_radius[i];
            float wz = sinf(asteroid_angle[i]) * asteroid_radius[i];

            float sx, sy, depth;
            ProjectXZ(wx, wz, tiltSin, tiltCos, CAM_DIST, fov, cx, cy,
                      &sx, &sy, &depth);

            SDL_RenderPoint(renderer, sx, sy);
        }

        // highlight selected planet (ring in screen space)
        if (selectedPlanet >= 0 && selectedPlanet < NUM_PLANETS) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            float hr = planetScreenRadius[selectedPlanet] + 6.0f;
            DrawCircle(renderer, planets[selectedPlanet].x, planets[selectedPlanet].y, hr);
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
        SDL_Delay(16); // ~60 FPS
    }

    SDL_Quit();
    return 0;
}
