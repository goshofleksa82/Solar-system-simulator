#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>

// ---------------------------------------------
// Simulation scaling
// ---------------------------------------------

// Gravitational constant (scaled for simulation)
const double G = 6.67430e-11;

// Realistic masses (kg) – scaled down (divided by 1e20)
const double MASS_SUN = 1.989e30 / 1e20;
const double MASS_EARTH = 5.972e24 / 1e20;
const double MASS_JUPITER = 1.898e27 / 1e20;

// Distance scale: 1 pixel = 1e9 meters
const double SCALE = 1e9;

// ---------------------------------------------
// Planet struct
// ---------------------------------------------
typedef struct {
    double x, y;   // position (meters)
    double vx, vy; // velocity (meters/second)
    double mass;   // mass (kg)
    int radius;    // screen pixels
    Uint8 r, g, b; // color
} Body;

// ---------------------------------------------
// Apply gravity between two bodies
// ---------------------------------------------
void apply_gravity(Body* a, Body* b, double dt) {
    double dx = b->x - a->x;
    double dy = b->y - a->y;
    double dist = sqrt(dx*dx + dy*dy);

    if (dist < 1) return; // avoid division by zero

    double force = (G * a->mass * b->mass) / (dist * dist);
    double ax = force * dx / dist / a->mass;
    double ay = force * dy / dist / a->mass;
    double bx = -force * dx / dist / b->mass;
    double by = -force * dy / dist / b->mass;

    a->vx += ax * dt;
    a->vy += ay * dt;
    b->vx += bx * dt;
    b->vy += by * dt;
}

// ---------------------------------------------
// Draw circles
// ---------------------------------------------
void draw_circle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int w = -radius; w <= radius; w++) {
        for (int h = -radius; h <= radius; h++) {
            if (w*w + h*h <= radius*radius)
                SDL_RenderPoint(r, cx + w, cy + h);
        }
    }
}

// ---------------------------------------------
// MAIN
// ---------------------------------------------
int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Planet Simulator", 1200, 800, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    if (!window || !renderer) {
        printf("SDL Window/Renderer failed: %s\n", SDL_GetError());
        return 1;
    }

    // ---------------------------------------------
    // Create bodies (positions in meters)
    // ---------------------------------------------

    Body sun = {
        .x = 0,
        .y = 0,
        .vx = 0, .vy = 0,
        .mass = MASS_SUN,
        .radius = 15,
        .r = 255, .g = 200, .b = 0
    };

    Body earth = {
        .x = 150e9,   // 150 million km from the sun
        .y = 0,
        .vx = 0,
        .vy = 30e3,   // 30 km/s orbital speed
        .mass = MASS_EARTH,
        .radius = 6,
        .r = 0, .g = 150, .b = 255
    };

    Body jupiter = {
        .x = 780e9,   // 780 million km
        .y = 0,
        .vx = 0,
        .vy = 13e3,   // 13 km/s
        .mass = MASS_JUPITER,
        .radius = 10,
        .r = 255, .g = 120, .b = 0
    };

    // ---------------------------------------------
    // Simulation loop
    // ---------------------------------------------
    const double dt = 3600; // 1 hour per physics step
    int running = 1;
    SDL_Event e;

    while (running) {
        // Handle quit
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = 0;
        }

        // Apply gravity between all bodies
        apply_gravity(&sun, &earth, dt);
        apply_gravity(&sun, &jupiter, dt);
        apply_gravity(&earth, &jupiter, dt);

        // Update positions
        sun.x += sun.vx * dt;
        sun.y += sun.vy * dt;

        earth.x += earth.vx * dt;
        earth.y += earth.vy * dt;

        jupiter.x += jupiter.vx * dt;
        jupiter.y += jupiter.vy * dt;

        // ---------------------------------------------
        // Draw
        // ---------------------------------------------
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Convert to screen coordinates
        int cx = 600;  // center of screen
        int cy = 400;

        SDL_SetRenderDrawColor(renderer, sun.r, sun.g, sun.b, 255);
        draw_circle(renderer, cx + sun.x / SCALE, cy + sun.y / SCALE, sun.radius);

        SDL_SetRenderDrawColor(renderer, earth.r, earth.g, earth.b, 255);
        draw_circle(renderer, cx + earth.x / SCALE, cy + earth.y / SCALE, earth.radius);

        SDL_SetRenderDrawColor(renderer, jupiter.r, jupiter.g, jupiter.b, 255);
        draw_circle(renderer, cx + jupiter.x / SCALE, cy + jupiter.y / SCALE, jupiter.radius);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);  // ~60 FPS
    }

    SDL_Quit();
    return 0;
}
