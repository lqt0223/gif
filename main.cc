#include "gifdec.h"
#include <SDL2/SDL.h>
#include <sstream>

int main(int argc, char** argv) {
    assert(argc >= 2 && "no input file");
    char* filename = argv[1];

    GifDecoder gd(filename, pix_fmt_t::ARGB);
    uint16_t w = gd.get_width();
    uint16_t h = gd.get_height();

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("gif decoder", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_STREAMING, w, h);

    SDL_Event e;
    int pitch;
poll:
    while (true) {
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                goto end;
            }
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                goto wait;
            }
        }
        SDL_LockTexture(texture, nullptr, (void**)&gd.buffer, &pitch);
        bool eof = gd.decode_frame();
        if (eof) {
            gd.loop();
            continue;
        }
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
wait:
    while (true) {
        if (SDL_WaitEvent(&e)) {
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                goto poll;
            }
        }
    }
end:
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
