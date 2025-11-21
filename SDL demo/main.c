#include <stdio.h>
#include<SDL3/SDL.h>

#define WIDTH 900
#define HEIGHT 600

int main(int argc, char *argv[]) {
    printf("Hello Bouncy Ball\n");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindow("Bpuncy Ball", WIDTH, HEIGHT, 0);
}