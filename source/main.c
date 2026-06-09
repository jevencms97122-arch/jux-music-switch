#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#define URL "http://188.115.125.74:3000/jux"
#define W 1280
#define H 720

typedef struct { char *data; size_t size; } Buf;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, Buf *b) {
    size_t n = size * nmemb;
    b->data = realloc(b->data, b->size + n + 1);
    memcpy(b->data + b->size, ptr, n);
    b->size += n;
    b->data[b->size] = 0;
    return n;
}

int main(int argc, char *argv[]) {
    socketInitializeDefault();
    nifmInitialize(NifmServiceType_System);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

    SDL_Window   *win = SDL_CreateWindow("Jux Music",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    // Dark background
    SDL_SetRenderDrawColor(ren, 18, 4, 8, 255);
    SDL_RenderClear(ren);
    SDL_RenderPresent(ren);

    // Load icon from romfs
    romfsInit();
    SDL_Surface *icon_surf = IMG_Load("romfs:/icon.jpg");
    SDL_Texture *icon_tex  = NULL;
    if (icon_surf) {
        icon_tex = SDL_CreateTextureFromSurface(ren, icon_surf);
        SDL_FreeSurface(icon_surf);
    }

    // Fetch page via curl (bypasses NIFM completely)
    CURL *curl = curl_easy_init();
    Buf buf = {0};
    long http_code = 0;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, URL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);
    }

    // Render icon centered
    SDL_SetRenderDrawColor(ren, 18, 4, 8, 255);
    SDL_RenderClear(ren);

    if (icon_tex) {
        SDL_Rect dst = { (W - 256) / 2, (H - 256) / 2, 256, 256 };
        SDL_RenderCopy(ren, icon_tex, NULL, &dst);
    }

    // If connected (2xx), try web applet now that network is active
    if (http_code >= 200 && http_code < 300) {
        SDL_RenderPresent(ren);
        WebCommonConfig cfg;
        WebCommonReply  rep;
        if (R_SUCCEEDED(webPageCreate(&cfg, URL))) {
            webConfigSetWhitelist(&cfg, ".*");
            webConfigSetFooter(&cfg, false);
            webConfigShow(&cfg, &rep);
        }
    }

    SDL_RenderPresent(ren);

    // Wait for + to quit
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        SDL_Delay(16);
    }

    if (icon_tex) SDL_DestroyTexture(icon_tex);
    if (buf.data) free(buf.data);
    romfsExit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    IMG_Quit();
    SDL_Quit();
    nifmExit();
    socketExit();
    return 0;
}
