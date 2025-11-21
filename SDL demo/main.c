#include <stdio.h>
#include <SDL3/SDL.h>

#define WIDTH 900
#define HEIGHT 600

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Bouncy Ball", WIDTH, HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    // Set background to black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // SDL3 uses SDL_FRect
    SDL_FRect rect = {300.0f, 300.0f, 300.0f, 300.0f};

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    SDL_RenderFillRect(renderer, &rect);

    SDL_RenderPresent(renderer);
    SDL_Delay(3000);

    SDL_Quit();
    return 0;
}

