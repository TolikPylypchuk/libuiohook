#include <stdbool.h>
#include <windows.h>

extern bool dispatch_hook_enable(uint64_t timestamp);

extern bool dispatch_hook_disable(uint64_t timestamp);

extern bool dispatch_key_press(uint64_t timestamp, KBDLLHOOKSTRUCT *kbhook);

extern bool dispatch_key_release(uint64_t timestamp, KBDLLHOOKSTRUCT *kbhook);

extern bool dispatch_button_press(uint64_t timestamp, MSLLHOOKSTRUCT *mshook, uint16_t button);

extern bool dispatch_button_release(uint64_t timestamp, MSLLHOOKSTRUCT *mshook, uint16_t button);

extern bool dispatch_mouse_move(uint64_t timestamp, MSLLHOOKSTRUCT *mshook);

extern bool dispatch_mouse_wheel(uint64_t timestamp, MSLLHOOKSTRUCT *mshook, uint8_t direction);
