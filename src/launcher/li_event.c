
#include "SDL.h"

#include "i_system.h"
#include "li_event.h"

mouse_t mouse;
navigation_t nav;
static navigation_t held_nav; // Time each key is held

static void SetNavKey(SDL_Keycode key, char state)
{
	switch (key)
	{
	case SDLK_UP:     held_nav[NAV_UP] = state;        break;
	case SDLK_DOWN:   held_nav[NAV_DOWN] = state;      break;
	case SDLK_LEFT:   held_nav[NAV_LEFT] = state;      break;
	case SDLK_RIGHT:  held_nav[NAV_RIGHT] = state;     break;
	case SDLK_RETURN: held_nav[NAV_PRIMARY] = state;   break;
	case SDLK_SPACE:  held_nav[NAV_SECONDARY] = state; break;
	case SDLK_o:      held_nav[NAV_OPTIONS] = state;   break;
	case SDLK_ESCAPE: held_nav[NAV_BACK] = state;      break;
	default: return;
	}
}

void LI_Init(void)
{
	memset(held_nav, 0, sizeof(navigation_t));
	memset(&mouse, 0, sizeof(mouse_t));
}

void LI_HandleEvents(void)
{
	mouse.primary = mouse.secondary = false;

	SDL_Event ev;
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
		default:
			break;
		case SDL_KEYUP:
		case SDL_KEYDOWN:
			mouse.active = false;
			if (!ev.key.repeat)
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
}
