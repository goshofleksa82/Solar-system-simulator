#include <SDL3/SDL.h>
#include <stdio.h>

#define WIDTH 900
#define HEIGHT 600

struct Circle {
    float x;
    float y;
    float radius;
};

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

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("Centered Circle", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

    int running = 1;
    SDL_Event e;

    struct Circle circle = {0, 0, 100.0f};

    while (running)
    {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = 0;
        }

        // Get window size
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        // center the circle
        circle.x = winW / 2.0f;
        circle.y = winH / 2.0f;

        // Draw
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // background black
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // white circle
        DrawFillCircle(renderer, circle.x, circle.y, circle.radius);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}
