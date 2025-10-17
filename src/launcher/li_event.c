
#include "SDL.h"

#include "i_system.h"
#include "li_event.h"
#include "m_misc.h"

mouse_t mouse;
navigation_t nav;

static char *text_input_buffer;
static int text_input_size;

static navigation_t held_nav; // Time each key is held

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

static void SetNavJoyButton(Uint8 button, char state)
{
	switch (button)
	{
	case 0: /* south */ held_nav[NAV_PRIMARY] = state;   break;
	case 1: /* east  */ held_nav[NAV_BACK] = state;      break;
	case 2: /* west  */ held_nav[NAV_SECONDARY] = state; break;
	case 3: /* north */ held_nav[NAV_OPTIONS] = state;   break;
	}
}

static void SetNavJoyHat(Uint8 hat_dir)
{
	held_nav[NAV_UP] = !!(hat_dir & SDL_HAT_UP);
	held_nav[NAV_DOWN] = !!(hat_dir & SDL_HAT_DOWN);
	held_nav[NAV_LEFT] = !!(hat_dir & SDL_HAT_LEFT);
	held_nav[NAV_RIGHT] = !!(hat_dir & SDL_HAT_RIGHT);
}

static void SetNavJoyAxis(Uint8 axis, Sint16 value)
{
	switch (axis)
	{
	case 0: // Primary stick X
		held_nav[NAV_LEFT] = (value < -24000);
		held_nav[NAV_RIGHT] = (value > 24000);
		break;
	case 1: // Primary stick Y
		held_nav[NAV_UP] = (value < -24000);
		held_nav[NAV_DOWN] = (value > 24000);
	default: break;
	}
}

void LI_Init(void)
{
	memset(held_nav, 0, sizeof(navigation_t));
	memset(&mouse, 0, sizeof(mouse_t));

	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	int num_joy = SDL_NumJoysticks();
	for (int i = 0; i < num_joy; ++i)
		SDL_JoystickOpen(i);
}

void LI_SetTextInput(char *buffer, int size)
{
	text_input_buffer = buffer;
	text_input_size = size;
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
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			SetNavJoyButton(ev.jbutton.button, ev.type == SDL_JOYBUTTONDOWN);
			break;
		case SDL_JOYHATMOTION:
			SetNavJoyHat(ev.jhat.value);
			break;
		case SDL_JOYAXISMOTION:
			SetNavJoyAxis(ev.jaxis.axis, ev.jaxis.value);
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
