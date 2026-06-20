#ifndef _included_input_helper
#define _included_input_helper

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <windows.h>

#define WCH_NONE        0xF000

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL  0x020E
#endif

/* Get the virtual key-code of the event, taking layouts like AZERTY into account. */
extern DWORD get_vk_code(KBDLLHOOKSTRUCT *kbhook);

/* Converts a uiohook virtual key code to the appropriate Windows VK key code. */
extern DWORD uiocode_to_keycode(uint16_t uiocode);

/* Converts a Windows VK key code to the appropriate uiohook virtual key code. */
extern uint16_t keycode_to_uiocode(DWORD vk_code, DWORD flags);

/* Converts a Windows vk code to it's unicode representation */
extern SIZE_T keycode_to_unicode(DWORD keycode, DWORD scancode, PWCHAR buffer, int size);

/* Set the native modifier mask for future events. */
extern void set_modifier_mask(uint16_t mask);

/* Unset the native modifier mask for future events. */
extern void unset_modifier_mask(uint16_t mask);

/* Clear the native modifier mask for future events. */
extern void clear_modifier_mask();

/* Get the current native modifier mask state. */
extern uint16_t get_modifiers();

extern bool is_scroll_direction_reversed();

#endif
