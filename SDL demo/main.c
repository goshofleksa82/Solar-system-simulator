#include <SDL3/SDL.h>
#include <stdio.h>
#include <math.h>

#define WIDTH 1600
#define HEIGHT 1000

struct Circle {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;
};

// ----- Filled circle -----
void DrawFillCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    for (float w = -radius; w <= radius; w++) {
        for (float h = -radius; h <= radius; h++) {
            if ((w*w + h*h) <= radius * radius) {
                SDL_RenderPoint(renderer, cx + w, cy + h);
            }
        }
    }
}

// ----- Outline circle (orbit) -----
void DrawCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
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

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "Solar System (SDL3)", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

    int running = 1;
    SDL_Event e;

    // ---------- STAR ----------
    struct Circle sun = {0, 0, 30.0f, 255, 255, 0};

    // ---------- PLANETS ----------
    struct Circle planets[8] = {
        {0, 0, 5,   200, 200, 200}, // Mercury
        {0, 0, 8,   255, 200, 130}, // Venus
        {0, 0, 8,     0, 150, 255}, // Earth
        {0, 0, 6,   255,  50,  50}, // Mars
        {0, 0, 20, 255, 180,  80},  // Jupiter
        {0, 0, 18, 200, 200, 150},  // Saturn
        {0, 0, 14, 100, 200, 255},  // Uranus
        {0, 0, 14, 100, 100, 255}   // Neptune
    };

    // Distances scaled to screen (not real meters)
    float orbit_radius[8] = {
        60,   // Mercury
        90,   // Venus
        130,  // Earth
        170,  // Mars
        260,  // Jupiter
        340,  // Saturn
        420,  // Uranus
        480   // Neptune
    };

    // Angular speeds (relative to real orbital periods)
    float angular_speed[8] = {
        0.03f,  // Mercury – fast
        0.02f,  // Venus
        0.015f, // Earth
        0.012f, // Mars
        0.009f, // Jupiter
        0.007f, // Saturn
        0.005f, // Uranus
        0.004f  // Neptune – slow
    };

    float angle[8] = {0};

    while (running)
    {
        // --- EVENTS ---
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = 0;
        }

        // Get window size for dynamic centering
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        // Sun at center
        sun.x = winW / 2.0f;
        sun.y = winH / 2.0f;

        // Update planet positions
        for (int i = 0; i < 8; i++) {
            angle[i] += angular_speed[i];
            planets[i].x = sun.x + cosf(angle[i]) * orbit_radius[i];
            planets[i].y = sun.y + sinf(angle[i]) * orbit_radius[i];
        }

        // --- DRAW ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw orbit rings
        SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
        for (int i = 0; i < 8; i++) {
            DrawCircle(renderer, sun.x, sun.y, orbit_radius[i]);
        }

        // Draw sun
        SDL_SetRenderDrawColor(renderer, sun.r, sun.g, sun.b, 255);
        DrawFillCircle(renderer, sun.x, sun.y, sun.radius);

        // Draw planets
        for (int i = 0; i < 8; i++) {
            SDL_SetRenderDrawColor(renderer,
                                   planets[i].r,
                                   planets[i].g,
                                   planets[i].b,
                                   255);
            DrawFillCircle(renderer, planets[i].x, planets[i].y, planets[i].radius);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_Quit();
    return 0;
}
