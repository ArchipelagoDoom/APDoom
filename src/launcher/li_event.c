
#include "SDL.h"

#include "i_system.h"
#include "li_event.h"
#include "m_misc.h"

enum {
    AXIS_REGION_NEGATIVE = -2,
    AXIS_REGION_NBUFFER = -1,
    AXIS_REGION_NEUTRAL = 0,
    AXIS_REGION_PBUFFER = 1,
    AXIS_REGION_POSITIVE = 2
};

mouse_t mouse;
navigation_t nav;

static char *text_input_buffer;
static int text_input_size;

static navigation_t held_nav; // Time each key is held
static char axis_regions[16][2];

static void SetNavKey(SDL_Keycode key, char state)
{
    switch (key)
    {
    case SDLK_UP:        held_nav[NAV_UP] = state;        break;
    case SDLK_DOWN:      held_nav[NAV_DOWN] = state;      break;
    case SDLK_LEFT:      held_nav[NAV_LEFT] = state;      break;
    case SDLK_RIGHT:     held_nav[NAV_RIGHT] = state;     break;
    case SDLK_RETURN:    held_nav[NAV_PRIMARY] = state;   break;
    case SDLK_SPACE:     held_nav[NAV_SECONDARY] = state; break;
    case SDLK_o:         held_nav[NAV_OPTIONS] = state;   break;
    case SDLK_ESCAPE:    held_nav[NAV_BACK] = state;      break;
    case SDLK_BACKSPACE: held_nav[NAV_BACKSPACE] = state; break;
    default: return;
    }
}

static void SetNavControllerButton(SDL_JoystickID which, Uint8 button, char state)
{
    if (which >= 16)
        return;
    switch (button)
    {
    case SDL_CONTROLLER_BUTTON_A: /* south */ held_nav[NAV_PRIMARY] = state;   break;
    case SDL_CONTROLLER_BUTTON_B: /* east  */ held_nav[NAV_BACK] = state;      break;
    case SDL_CONTROLLER_BUTTON_X: /* west  */ held_nav[NAV_SECONDARY] = state; break;
    case SDL_CONTROLLER_BUTTON_Y: /* north */ held_nav[NAV_OPTIONS] = state;   break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       held_nav[NAV_UP] = state;        break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     held_nav[NAV_DOWN] = state;      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     held_nav[NAV_LEFT] = state;      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    held_nav[NAV_RIGHT] = state;     break;
    }
}

static void SetNavControllerAxis(SDL_JoystickID which, Uint8 axis, Sint16 value)
{
    static const int neg_dir[2] = {NAV_LEFT,  NAV_UP};
    static const int pos_dir[2] = {NAV_RIGHT, NAV_DOWN};

    if (which >= 16 || axis >= 2)
        return;

    char new_region = AXIS_REGION_NEUTRAL;
    if (value < -25000)      new_region = AXIS_REGION_NEGATIVE;
    else if (value > 25000)  new_region = AXIS_REGION_POSITIVE;
    else if (value < -24000) new_region = AXIS_REGION_NBUFFER;
    else if (value > 24000)  new_region = AXIS_REGION_PBUFFER;

    if (new_region != axis_regions[which][axis])
    {
        axis_regions[which][axis] = new_region;
        switch (new_region)
        {
        case AXIS_REGION_POSITIVE:
            if (!held_nav[pos_dir[axis]])
                held_nav[pos_dir[axis]] = true;
            // fall through
        case AXIS_REGION_PBUFFER:
            held_nav[neg_dir[axis]] = false;
            break;

        case AXIS_REGION_NEGATIVE:
            if (!held_nav[neg_dir[axis]])
                held_nav[neg_dir[axis]] = true;
            // fall through
        case AXIS_REGION_NBUFFER:
            held_nav[pos_dir[axis]] = false;
            break;

        default:
            held_nav[pos_dir[axis]] = false;
            held_nav[neg_dir[axis]] = false;
            break;
        }
    }
}

void LI_Reset(void)
{
    memset(held_nav, 0, sizeof(navigation_t));
    memset(&mouse, 0, sizeof(mouse_t));

    text_input_buffer = NULL;
    text_input_size = 0;
}

void LI_Init(void)
{
    LI_Reset();
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    int num_joy = SDL_NumJoysticks();
    for (int i = 0; i < num_joy; ++i)
    {
        if (SDL_IsGameController(i))
            SDL_GameControllerOpen(i);
        // Right now I don't support regular joysticks. Should I?
    }
}

void LI_SetTextInput(char *buffer, int size)
{
    text_input_buffer = buffer;
    text_input_size = size;
}

void LI_HandleEvents(void)
{
    mouse.primary = mouse.secondary = false;
    mouse.wheel = 0;

    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        switch (ev.type)
        {
        default:
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            mouse.active = false;
            SetNavControllerButton(ev.cbutton.which, ev.cbutton.button, ev.type == SDL_CONTROLLERBUTTONDOWN);
            break;
        case SDL_CONTROLLERAXISMOTION:
            mouse.active = false;
            SetNavControllerAxis(ev.caxis.which, ev.caxis.axis, ev.caxis.value);
            break;
        case SDL_TEXTINPUT:
            if (text_input_buffer)
                M_StringConcat(text_input_buffer, ev.text.text, text_input_size);
            break;
        case SDL_KEYUP:
        case SDL_KEYDOWN:
            mouse.active = false;
            if (ev.key.repeat)
                break; // Ignore repeat events always

            if (ev.type == SDL_KEYDOWN
                && (ev.key.keysym.mod & KMOD_CTRL)
                && ev.key.keysym.scancode == SDL_SCANCODE_V)
            {
                // Clipboard paste into text field
                if (!text_input_buffer)
                    break;
                if (SDL_HasClipboardText())
                {
                    char *cb = SDL_GetClipboardText();
                    M_StringConcat(text_input_buffer, cb, text_input_size);
                    SDL_free(cb);
                }
                break;
            }
            SetNavKey(ev.key.keysym.sym, ev.type == SDL_KEYDOWN);
            break;
        case SDL_MOUSEBUTTONDOWN:
        //case SDL_MOUSEBUTTONUP: // We don't care about mouseup
            if (ev.button.button == SDL_BUTTON_LEFT)
                mouse.primary = true;
            else if (ev.button.button == SDL_BUTTON_RIGHT)
                mouse.secondary = true;

            mouse.active = true;
            mouse.x = ev.button.x >> 1;
            mouse.y = ev.button.y >> 1;
            break;
        case SDL_MOUSEMOTION:
            if (mouse.primary || mouse.secondary)
                break; // Button takes priority over motion

            mouse.active = true;
            // Scale mouse coordinates down to match render coords
            mouse.x = ev.motion.x >> 1;
            mouse.y = ev.motion.y >> 1;
            break;
        case SDL_MOUSEWHEEL:
            mouse.active = true;
            if (ev.wheel.y != 0)
                mouse.wheel = ev.wheel.y;
            if (ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                mouse.wheel *= -1;
            break;
        case SDL_QUIT:
            I_Quit();
        }
    }

    memset(nav, 0, sizeof(navigation_t));
    for (int i = 0; i < NUM_NAV; ++i)
    {
        if (!held_nav[i])
            continue;
        else if (held_nav[i] == 1)
            nav[i] = 1;
        else if (held_nav[i] == 20)
        {
            held_nav[i] = 17;
            if (!(i & NAV_ISBUTTON))
                nav[i] = 1;
        }
        ++held_nav[i];
    }

    if (nav[NAV_BACKSPACE] && text_input_buffer)
    {
        int idx = strlen(text_input_buffer);
        if (idx > 0)
            text_input_buffer[idx - 1] = '\0';
    }
}
