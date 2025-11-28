#include <SDL3/SDL.h>
#include <stdio.h>
#include <math.h>

#define WIDTH 900
#define HEIGHT 600

struct Circle {
    float x;
    float y;
    float radius;
    Uint8 r, g, b;
};

// ----- Draw Filled Circle -----
void DrawFillCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    for (float w = -radius; w <= radius; w++) {
        for (float h = -radius; h <= radius; h++) {
            if (w*w + h*h <= radius*radius) {
                SDL_RenderPoint(renderer, cx + w, cy + h);
            }
        }
    }
}

// ----- Draw NOT Filled Circle (Orbit) -----
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

    SDL_Window *window = SDL_CreateWindow("Orbiting Planet", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

    int running = 1;
    SDL_Event e;

    // STAR (center)
    struct Circle star = {0, 0, 20.0f, 255, 255, 0}; // yellow

    // PLANET
    struct Circle planet = {0, 0, 10.0f, 0, 150, 255}; // blue

    // Orbit settings
    float orbit_radius = 150.0f;
    float angle = 0.0f;          // current angle
    float angular_speed = 0.03f; // orbit speed

    while (running)
    {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = 0;
        }

        // Get window size
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        // Center the star
        star.x = winW / 2.0f;
        star.y = winH / 2.0f;

        // Move the planet along a circular orbit
        angle += angular_speed;
        planet.x = star.x + cosf(angle) * orbit_radius;
        planet.y = star.y + sinf(angle) * orbit_radius;

        // ---- DRAWING ----
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // black background
        SDL_RenderClear(renderer);

        // Draw orbit
        SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255); 
        DrawCircle(renderer, star.x, star.y, orbit_radius);

        // Draw star
        SDL_SetRenderDrawColor(renderer, star.r, star.g, star.b, 255);
        DrawFillCircle(renderer, star.x, star.y, star.radius);

        // Draw planet
        SDL_SetRenderDrawColor(renderer, planet.r, planet.g, planet.b, 255);
        DrawFillCircle(renderer, planet.x, planet.y, planet.radius);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_Quit();
    return 0;
}
