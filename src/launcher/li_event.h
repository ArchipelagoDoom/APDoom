
#ifndef __LI_EVENT_H__
#define __LI_EVENT_H__

enum {
	NAV_UP = 0,
	NAV_DOWN,
	NAV_LEFT,
	NAV_RIGHT,

	NAV_ISBUTTON,
	NAV_PRIMARY = NAV_ISBUTTON,
	NAV_SECONDARY,
	NAV_OPTIONS,
	NAV_BACK,

	NUM_NAV
};

typedef char navigation_t[NUM_NAV];

typedef struct {
	unsigned char active;
	int x;
	int y;

	unsigned char primary;
	unsigned char secondary;
} mouse_t;

extern navigation_t nav;
extern mouse_t mouse;

void LI_Init(void);
void LI_HandleEvents(void);

#endif
