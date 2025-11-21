#include <SDL3/SDL.h>
#include <stdio.h>

#define WIDTH 900
#define HEIGHT 600

struct Circle
{
    int x;
    int y;
    int radius;
};


void FillCircle(SDL_Surface* surface, struct Circle circle)
{

}

int main(int argc, char *argv[]) 
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Centered Rectangle", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    int running = 1;
    SDL_Event e;

    SDL_FRect rect = {0, 0, 200.0f, 200.0f}; // size only, position updated each frame

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = 0;
        }

        // Get window size
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        // Center the rectangle
        rect.x = (winW - rect.w) / 2.0f;
        rect.y = (winH - rect.h) / 2.0f;

        // Draw
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &rect);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_Quit();
    return 0;
}
