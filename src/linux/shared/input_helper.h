#ifndef SHARED_INPUT_HELPER_H
#define SHARED_INPUT_HELPER_H

#include <stdint.h>

/* Converts an evdev key code to the appropriate uiohook virtual key code. */
uint16_t evdev_code_to_uiocode(uint16_t evdev_code);

/* Converts a uiohook virtual key code to the appropriate evdev key code. */
uint16_t uiocode_to_evdev_code(uint16_t uiocode);

/* Converts an evdev button code to the appropriate uiohook mouse button. */
uint16_t evdev_code_to_button(uint16_t evdev_code);

/* Converts a uiohook mouse button to the appropriate evdev button code. */
uint16_t button_to_evdev_code(uint16_t button);

/* Gets the modifier mask which a uiohook virtual key code toggles, or 0 if it isn't a modifier. */
uint16_t get_modifier_mask_for_uiocode(uint16_t uiocode);

/* Gets the lock mask which a uiohook virtual key code toggles, or 0 if it isn't a lock key. */
uint16_t get_lock_mask_for_uiocode(uint16_t uiocode);

/* Gets the modifier mask which a uiohook mouse button toggles, or 0 if it has no mask. */
uint16_t get_modifier_mask_for_button(uint16_t button);

/* Sets the modifier mask for the current event. */
void set_modifier_mask(uint16_t mask);

/* Unsets the modifier mask for the current event. */
void unset_modifier_mask(uint16_t mask);

/* Clears the modifier mask for the current events. */
void clear_modifier_mask();

/* Gets the current modifier mask state. */
uint16_t get_modifiers();

/* Seeds the modifier mask from the key, button, and lock state of an input device. */
void seed_modifier_mask(int fd);

#endif
