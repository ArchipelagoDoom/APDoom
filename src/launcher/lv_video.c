
#include "SDL.h"
#include "lv_video.h"
#include "i_swap.h"
#include "i_system.h"


#include "w_wad.h"
#include "z_zone.h"

#define WINDOW_WIDTH  SCREEN_WIDTH*2
#define WINDOW_HEIGHT SCREEN_HEIGHT*2
#define PIXEL_FORMAT  SDL_PIXELFORMAT_ARGB8888

static SDL_Window *main_window;
static SDL_Renderer *renderer;

layer_t layertop;
layer_t *layerbot = NULL;

layer_t* LV_MakeLayer(void)
{
    if (!layerbot)
        I_Error("LV_MakeLayer: never initialized video");

    layer_t *layernew = malloc(sizeof(layer_t));
    layernew->surf = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, PIXEL_FORMAT);
    layernew->tex = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    layernew->brightness = layernew->_old_brightness = 255;
    layernew->_next = NULL;

    if (!layernew->surf || !layernew->tex)
        I_Error("LV_MakeLayer: couldn't make new layer");

    SDL_SetTextureBlendMode(layernew->tex, SDL_BLENDMODE_BLEND);
    layerbot = (layerbot->_next = layernew);
    return layernew;
}

// ------------------------------------------------------------------------------------

static uint32_t palette[10][256];
static uint32_t *active_palette = palette[0];
static int active_palette_num = 0;

static void LV_InitPalette(void)
{
    byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
    for (int pnum = 0; pnum < 10; ++pnum)
    {
        for (int c = 0; c < 256; ++c)
        {
            const int idx = (pnum * 768) + (c * 3);
            const byte r = playpal[idx];
            const byte g = playpal[idx+1];
            const byte b = playpal[idx+2];
            palette[pnum][c] = 0xFF000000 | (r << 16) | (g << 8) | (b);
        }        
    }
    active_palette = palette[0];
    W_ReleaseLumpName("PLAYPAL");
}

void LV_SetPalette(int palnum)
{
    active_palette_num = (palnum >= 0 && palnum <= 9 ? palnum : 0);
    active_palette = palette[active_palette_num];
}

int LV_GetPalette(void)
{
    return active_palette_num;
}

// ------------------------------------------------------------------------------------

static void LV_OnExit(void)
{
    layer_t *layer2free = layertop._next;
    while (layer2free)
    {
        if (layer2free->tex)
            SDL_DestroyTexture(layer2free->tex);
        SDL_FreeSurface(layer2free->surf);

        layer_t *layernext = layer2free->_next;
        free(layer2free);
        layer2free = layernext;
    }

    layerbot = NULL;
}

static void LV_CreateRenderer(void)
{
    main_window = SDL_CreateWindow("Archipelago Doom - Launcher", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    renderer = SDL_CreateRenderer(main_window, -1, 0);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

static void LV_EnterBackground(void)
{
    printf("LV_EnterBackground: Closing window and entering background.\n");

    for (layer_t *layercur = layertop._next; layercur; layercur = layercur->_next)
    {
        if (layercur->tex)
            SDL_DestroyTexture(layercur->tex);
        layercur->tex = NULL;
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(main_window);
}

static void LV_LeaveBackground(void)
{
    printf("LV_LeaveBackground: Restoring window.\n");

    LV_CreateRenderer();
    for (layer_t *layercur = layertop._next; layercur; layercur = layercur->_next)
    {
        layercur->tex = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
        SDL_SetTextureBlendMode(layercur->tex, SDL_BLENDMODE_BLEND);
    }
}


void LV_InitVideo(void)
{
    if (layerbot)
        return;

    printf("LV_InitVideo: initializing launcher video system.\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        I_Error("LV_InitVideo: couldn't init SDL video");

    LV_CreateRenderer();
    LV_InitPalette();

    layerbot = &layertop;
    I_AtExit(LV_OnExit, true);
}

void LV_EnterMinimalMode(int (*unminimize_callback)(void))
{
    bool was_closed = false;
    SDL_Event ev;

    // It is possible to enter this code path uninitialized, so we must test for that.
    if (layerbot)
    {
        // Causes all elements to fade in from black when resuming.
        for (layer_t *layercur = layertop._next; layercur; layercur = layercur->_next)
        {
            layercur->_old_brightness = 0;
            layercur->_bspeed = 16; 
        }

        LV_EnterBackground();        
    }

    while (!unminimize_callback())
    {
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
            {
                // It's unlikely this happens with the window being closed when we enter minimal mode, but.
                was_closed = true;
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            }
        }
        SDL_Delay(250);
    }

    // If we got a quit event, fully quit now that child process is gone
    if (was_closed)
        I_Quit();

    if (layerbot)
        LV_LeaveBackground();
}

static void LV_Delay(void)
{
#define TARGET_FPS   60
#define TICK_DIVISOR (1000.0/TARGET_FPS)
    uint64_t tick = SDL_GetTicks64();

#if 0 // DISPLAY_FPS
    static int total_tick_count = 0;
    static uint64_t lastsec = 0;
    boolean update_fps = false;

    while (tick - lastsec >= 1000)
    {
        lastsec += 1000;
        update_fps = true;
    }
    if (update_fps)
    {
        printf("fps: %d/%d\n", total_tick_count, TARGET_FPS);
        total_tick_count = 0;
    }
    ++total_tick_count;
#endif

    uint64_t target_tick = (int)(((int)((tick + 1) / TICK_DIVISOR)+1) * TICK_DIVISOR);
    SDL_Delay(target_tick - tick);
}

void LV_RenderFrame(void)
{
    if (!renderer)
        return;

    for (layer_t *layercur = layertop._next; layercur; layercur = layercur->_next)
    {
        if (layercur->brightness != layercur->_old_brightness)
        {
            const int ofs = layercur->brightness - layercur->_old_brightness;
            if (abs(ofs) < layercur->_bspeed)
                layercur->_old_brightness = layercur->brightness;
            else if (ofs < 0)
                layercur->_old_brightness -= layercur->_bspeed;
            else
                layercur->_old_brightness += layercur->_bspeed;
            SDL_SetTextureColorMod(layercur->tex,
                                   layercur->_old_brightness,
                                   layercur->_old_brightness,
                                   layercur->_old_brightness);
        }
        SDL_UpdateTexture(layercur->tex, NULL, layercur->surf->pixels, layercur->surf->pitch);
    }

    SDL_RenderClear(renderer);

    for (layer_t *layercur = layertop._next; layercur; layercur = layercur->_next)
        SDL_RenderCopy(renderer, layercur->tex, NULL, NULL);

    SDL_RenderPresent(renderer);

    LV_Delay();
}

void LV_ClearLayer(layer_t *layer)
{
    memset(layer->surf->pixels, 0, SCREEN_WIDTH*SCREEN_HEIGHT*SDL_BYTESPERPIXEL(PIXEL_FORMAT));
}

void LV_SetBrightness(layer_t *layer, byte brightness, char fade_speed)
{
    layer->brightness = brightness;
    if (!fade_speed)
        layer->_old_brightness = brightness;
    else
        layer->_bspeed = fade_speed;
}

void LV_OutlineRect(layer_t *layer, int x, int y, int w, int h, int size, unsigned int c)
{
    SDL_Rect outline[4] = {
        {x,              y,              w,    size          },
        {x,              y + size,       size, h - (size * 2)},
        {x,              (y + h) - size, w,    size          },
        {(x + w) - size, y + size,       size, h - (size * 2)}
    };
    SDL_FillRects(layer->surf, outline, 4, c);
}

void LV_FillRect(layer_t *layer, int x, int y, int w, int h, unsigned int c)
{
    SDL_Rect fill = {x, y, w, h};
    SDL_FillRect(layer->surf, &fill, c);
}

void LV_DrawPatch(layer_t *layer, int x, int y, patch_t *patch)
{
    uint32_t *dest = (uint32_t*)(layer->surf->pixels);
    uint32_t *dest_p;
    byte *source_p;

    int w = SHORT(patch->width);
    x -= SHORT(patch->leftoffset);
    y -= SHORT(patch->topoffset);


    if (y >= SCREEN_HEIGHT)
        return;

    int col = (x < 0 ? -x : 0);
    x += col;

    for (; col < w; ++x, ++col)
    {
        if (x >= SCREEN_WIDTH)
            return;

        column_t *column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));
        int true_topdelta = -1; // for tall patches

        while (column->topdelta != 0xFF)
        {
            if (column->topdelta <= true_topdelta)
                true_topdelta += column->topdelta;
            else
                true_topdelta = column->topdelta;

            int top = y + true_topdelta;
            int count = column->length;

            if (top >= SCREEN_HEIGHT)
                break;
            if (top + count > SCREEN_HEIGHT)
                count = SCREEN_HEIGHT - top;

            dest_p = &dest[x + (top * SCREEN_WIDTH)];
            source_p = (byte *)column + 3;
            while (count--)
            {
                if (top++ >= 0)
                    *dest_p = active_palette[*source_p++];
                dest_p += SCREEN_WIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}
