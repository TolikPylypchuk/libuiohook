#ifndef UIOHOOK_H
#define UIOHOOK_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

/* Begin Error Codes */
#define UIOHOOK_SUCCESS                                       0x00
#define UIOHOOK_FAILURE                                       0x01

// System-level errors.
#define UIOHOOK_ERROR_OUT_OF_MEMORY                           0x02
#define UIOHOOK_ERROR_NULL                                    0x03
#define UIOHOOK_ERROR_UNSUPPORTED_FEATURE                     0x04

// Linux-specific errors.
#define UIOHOOK_ERROR_LINUX_LOAD_BACKEND                      0x10
#define UIOHOOK_ERROR_LINUX_INIT_UDEV                         0x11
#define UIOHOOK_ERROR_LINUX_INIT_LIBINPUT                     0x12
#define UIOHOOK_ERROR_LINUX_ASSIGN_SEAT                       0x13
#define UIOHOOK_ERROR_LINUX_INIT_STOP_NOTIFICATION            0x14
#define UIOHOOK_ERROR_LINUX_EXEC_STOP_NOTIFICATION            0x15
#define UIOHOOK_ERROR_LINUX_NO_INPUT_DEVICES                  0x16
#define UIOHOOK_ERROR_LINUX_OPEN_UINPUT                       0x17
#define UIOHOOK_ERROR_LINUX_CREATE_UINPUT_DEVICE              0x18
#define UIOHOOK_ERROR_LINUX_WRITE_UINPUT                      0x19
#define UIOHOOK_ERROR_LINUX_OPEN_WAYLAND_DISPLAY              0x1A
#define UIOHOOK_ERROR_LINUX_VIRTUAL_DEVICES_NOT_INITIALIZED   0x1B

// XRecord back-end specific errors.
#define UIOHOOK_ERROR_X_OPEN_DISPLAY                          0x20
#define UIOHOOK_ERROR_X_RECORD_NOT_FOUND                      0x21
#define UIOHOOK_ERROR_X_RECORD_ALLOC_RANGE                    0x22
#define UIOHOOK_ERROR_X_RECORD_CREATE_CONTEXT                 0x23
#define UIOHOOK_ERROR_X_RECORD_ENABLE_CONTEXT                 0x24
#define UIOHOOK_ERROR_X_RECORD_GET_CONTEXT                    0x25

// Windows-specific errors.
#define UIOHOOK_ERROR_SET_WINDOWS_HOOK_EX                     0x30
#define UIOHOOK_ERROR_GET_MODULE_HANDLE                       0x31
#define UIOHOOK_ERROR_CREATE_INVISIBLE_WINDOW                 0x32

// macOS-specific errors.
#define UIOHOOK_ERROR_AXAPI_DISABLED                          0x40
#define UIOHOOK_ERROR_AXAPI_REVOKED                           0x41
#define UIOHOOK_ERROR_CREATE_EVENT_PORT                       0x42
#define UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE                  0x43
#define UIOHOOK_ERROR_GET_RUNLOOP                             0x44
#define UIOHOOK_ERROR_CREATE_OBSERVER                         0x45
/* End Error Codes */

/* Begin Optional Features */
#define UIOHOOK_FEATURE_EVENT_SUPPRESSION              (1 << 0)
#define UIOHOOK_FEATURE_KEY_TYPED_EVENTS               (1 << 1)
#define UIOHOOK_FEATURE_POST_TEXT                      (1 << 2)
#define UIOHOOK_FEATURE_KEY_AUTOREPEAT                 (1 << 3)
#define UIOHOOK_FEATURE_ABSOLUTE_MOUSE_MOVEMENT        (1 << 4)
#define UIOHOOK_FEATURE_ABSOLUTE_MOUSE_BUTTON_COORDS   (1 << 5)
#define UIOHOOK_FEATURE_POINTER_PROPERTIES             (1 << 6)
/* End Optional Features */

/* Begin Linux Modes */
#define LINUX_MODE_AUTO_XRECORD     0x0
#define LINUX_MODE_AUTO_LOW_LEVEL   0x1
#define LINUX_MODE_XRECORD          0x2
#define LINUX_MODE_X11              0x3
#define LINUX_MODE_WAYLAND          0x4
/* End Linux Modes */

/* Begin Linux Back-ends */
#define LINUX_LOADED_BACKEND_NONE      0x0
#define LINUX_LOADED_BACKEND_XRECORD   0x1
#define LINUX_LOADED_BACKEND_X11       0x2
#define LINUX_LOADED_BACKEND_WAYLAND   0x3
/* End Linux Back-ends */

/* Begin Log Levels and Function Prototype */
typedef enum _log_level {
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level;

// Logger callback function prototype.
typedef void (*logger_t)(unsigned int, void *, const char *, va_list);
/* End Log Levels and Function Prototype */

/* Begin Virtual Event Types and Data Structures */
#define EVENT_HOOK_ENABLED                   0x01
#define EVENT_HOOK_DISABLED                  0x02
#define EVENT_KEY_TYPED                      0x03
#define EVENT_KEY_PRESSED                    0x04
#define EVENT_KEY_RELEASED                   0x05
#define EVENT_MOUSE_CLICKED                  0x06
#define EVENT_MOUSE_PRESSED                  0x07
#define EVENT_MOUSE_PRESSED_IGNORE_COORDS    0x08
#define EVENT_MOUSE_RELEASED                 0x09
#define EVENT_MOUSE_RELEASED_IGNORE_COORDS   0x0A
#define EVENT_MOUSE_MOVED                    0x0B
#define EVENT_MOUSE_MOVED_RELATIVE           0x0C
#define EVENT_MOUSE_DRAGGED                  0x0D
#define EVENT_MOUSE_DRAGGED_RELATIVE         0x0E
#define EVENT_MOUSE_WHEEL                    0x0F

typedef struct _screen_data {
    uint8_t number;
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} screen_data;

typedef struct _keyboard_event_data {
    uint16_t keycode;
    uint16_t rawcode;
    uint16_t keychar;
} keyboard_event_data;

typedef struct _mouse_event_data {
    uint16_t button;
    uint16_t clicks;
    int16_t x;
    int16_t y;
} mouse_event_data;

typedef struct _mouse_wheel_event_data {
    int16_t x;
    int16_t y;
    int16_t rotation;
    uint16_t delta;
    uint8_t type;
    uint8_t direction;
} mouse_wheel_event_data;

typedef struct _uiohook_event {
    uint64_t time;
    uint32_t mask;
    uint16_t type;
    union {
        keyboard_event_data keyboard;
        mouse_event_data mouse;
        mouse_wheel_event_data wheel;
    } data;
} uiohook_event;

typedef void (*dispatcher_t)(uiohook_event * const, void *);

typedef int (*device_open_t)(const char *path, int flags, void *user_data);

typedef void (*device_close_t)(int fd, void *user_data);
/* End Virtual Event Types and Data Structures */


/* Begin Virtual Key Codes */
#define VC_ESCAPE                                0x01

// Begin Function Keys
#define VC_F1                                    0x02
#define VC_F2                                    0x03
#define VC_F3                                    0x04
#define VC_F4                                    0x05
#define VC_F5                                    0x06
#define VC_F6                                    0x07
#define VC_F7                                    0x08
#define VC_F8                                    0x09
#define VC_F9                                    0x0A
#define VC_F10                                   0x0B
#define VC_F11                                   0x0C
#define VC_F12                                   0x0D

#define VC_F13                                   0x0E
#define VC_F14                                   0x0F
#define VC_F15                                   0x10
#define VC_F16                                   0x11
#define VC_F17                                   0x12
#define VC_F18                                   0x13
#define VC_F19                                   0x14
#define VC_F20                                   0x15
#define VC_F21                                   0x16
#define VC_F22                                   0x17
#define VC_F23                                   0x18
#define VC_F24                                   0x19
// End Function Keys


// Begin Alphanumeric Zone
#define VC_BACK_QUOTE                            0x20

#define VC_1                                     0x21
#define VC_2                                     0x22
#define VC_3                                     0x23
#define VC_4                                     0x24
#define VC_5                                     0x25
#define VC_6                                     0x26
#define VC_7                                     0x27
#define VC_8                                     0x28
#define VC_9                                     0x29
#define VC_0                                     0x2A

#define VC_MINUS                                 0x2B
#define VC_EQUALS                                0x2C

#define VC_BACKSPACE                             0x2D

#define VC_TAB                                   0x2E
#define VC_CAPS_LOCK                             0x2F

#define VC_A                                     0x30
#define VC_B                                     0x31
#define VC_C                                     0x32
#define VC_D                                     0x33
#define VC_E                                     0x34
#define VC_F                                     0x35
#define VC_G                                     0x36
#define VC_H                                     0x37
#define VC_I                                     0x38
#define VC_J                                     0x39
#define VC_K                                     0x3A
#define VC_L                                     0x3B
#define VC_M                                     0x3C
#define VC_N                                     0x3D
#define VC_O                                     0x3E
#define VC_P                                     0x3F
#define VC_Q                                     0x40
#define VC_R                                     0x41
#define VC_S                                     0x42
#define VC_T                                     0x43
#define VC_U                                     0x44
#define VC_V                                     0x45
#define VC_W                                     0x46
#define VC_X                                     0x47
#define VC_Y                                     0x48
#define VC_Z                                     0x49

#define VC_OPEN_BRACKET                          0x4A
#define VC_CLOSE_BRACKET                         0x4B
#define VC_BACK_SLASH                            0x4C

#define VC_SEMICOLON                             0x4D
#define VC_QUOTE                                 0x4E
#define VC_ENTER                                 0x4F

#define VC_COMMA                                 0x50
#define VC_PERIOD                                0x51
#define VC_SLASH                                 0x52

#define VC_SPACE                                 0x53

#define VC_SECTION                               0x54
#define VC_MISC                                  0x55
// End Alphanumeric Zone


// Begin Edit Key Zone
#define VC_PRINT_SCREEN                          0x60 // SYSRQ
#define VC_SCROLL_LOCK                           0x61
#define VC_PAUSE                                 0x62
#define VC_CANCEL                                0x63 // BREAK
#define VC_HELP                                  0x64

#define VC_INSERT                                0x65
#define VC_DELETE                                0x66
#define VC_HOME                                  0x67
#define VC_END                                   0x68
#define VC_PAGE_UP                               0x69
#define VC_PAGE_DOWN                             0x6A
// End Edit Key Zone


// Begin Cursor Key Zone
#define VC_UP                                    0x6B
#define VC_LEFT                                  0x6C
#define VC_RIGHT                                 0x6D
#define VC_DOWN                                  0x6E
// End Cursor Key Zone


// Begin Numeric Zone
#define VC_NUM_LOCK                              0x70

#define VC_KP_1                                  0x71
#define VC_KP_2                                  0x72
#define VC_KP_3                                  0x73
#define VC_KP_4                                  0x74
#define VC_KP_5                                  0x75
#define VC_KP_6                                  0x76
#define VC_KP_7                                  0x77
#define VC_KP_8                                  0x78
#define VC_KP_9                                  0x79
#define VC_KP_0                                  0x7A

#define VC_KP_CLEAR                              0x7B
#define VC_KP_DIVIDE                             0x7C
#define VC_KP_MULTIPLY                           0x7D
#define VC_KP_SUBTRACT                           0x7E
#define VC_KP_EQUALS                             0x7F
#define VC_KP_ADD                                0x80
#define VC_KP_ENTER                              0x81
#define VC_KP_DECIMAL                            0x82
#define VC_KP_SEPARATOR                          0x83
// End Numeric Zone


// Begin Modifier and Control Keys
#define VC_SHIFT_L                               0x90
#define VC_SHIFT_R                               0x91
#define VC_CONTROL_L                             0x92
#define VC_CONTROL_R                             0x93
#define VC_ALT_L                                 0x94 // Option or Alt Key
#define VC_ALT_R                                 0x95 // Option or Alt Key
#define VC_META_L                                0x96 // Windows or Command Key
#define VC_META_R                                0x97 // Windows or Command Key
#define VC_CONTEXT_MENU                          0x98
#define VC_FUNCTION                              0x99 // macOS only
#define VC_CHANGE_INPUT_SOURCE                   0x9A // macOS only
// End Modifier and Control Keys


// Begin Shortcut Keys
#define VC_POWER                                 0xA0
#define VC_SLEEP                                 0xA1

#define VC_MEDIA_PLAY                            0xA2
#define VC_MEDIA_STOP                            0xA3
#define VC_MEDIA_PREVIOUS                        0xA4
#define VC_MEDIA_NEXT                            0xA5
#define VC_MEDIA_SELECT                          0xA6
#define VC_MEDIA_EJECT                           0xA7

#define VC_VOLUME_MUTE                           0xA8
#define VC_VOLUME_DOWN                           0xA9
#define VC_VOLUME_UP                             0xAA

#define VC_APP_1                                 0xAB
#define VC_APP_2                                 0xAC
#define VC_APP_3                                 0xAD
#define VC_APP_4                                 0xAE
#define VC_APP_BROWSER                           0xAF
#define VC_APP_CALCULATOR                        0xB0
#define VC_APP_MAIL                              0xB1

#define VC_BROWSER_SEARCH                        0xB2
#define VC_BROWSER_HOME                          0xB3
#define VC_BROWSER_BACK                          0xB4
#define VC_BROWSER_FORWARD                       0xB5
#define VC_BROWSER_STOP                          0xB6
#define VC_BROWSER_REFRESH                       0xB7
#define VC_BROWSER_FAVORITES                     0xB8
// End Shortcut Keys

// Begin Asian Language Keys
#define VC_KATAKANA_HIRAGANA                     0xC0
#define VC_KATAKANA                              0xC1
#define VC_HIRAGANA                              0xC2
#define VC_KANA                                  0xC3
#define VC_JUNJA                                 0xC4
#define VC_FINAL                                 0xC5
#define VC_HANJA                                 0xC6

#define VC_ACCEPT                                0xC7
#define VC_CONVERT                               0xC8
#define VC_NONCONVERT                            0xC9
#define VC_IME_ON                                0xCA
#define VC_IME_OFF                               0xCB
#define VC_MODE_CHANGE                           0xCC
#define VC_PROCESS                               0xCD

#define VC_ALPHANUMERIC                          0xCE
#define VC_UNDERSCORE                            0xCF
#define VC_YEN                                   0xD1
#define VC_JP_COMMA                              0xD2
// End Asian Language Keys

#define VC_UNDEFINED                             0x00    // KeyCode Unknown

#define CHAR_UNDEFINED                           0xFFFF    // CharCode Unknown
/* End Virtual Key Codes */


/* Begin Virtual Modifier Masks */
#define MASK_SHIFT_L                             (1 << 0)
#define MASK_CTRL_L                              (1 << 1)
#define MASK_META_L                              (1 << 2)
#define MASK_ALT_L                               (1 << 3)

#define MASK_SHIFT_R                             (1 << 4)
#define MASK_CTRL_R                              (1 << 5)
#define MASK_META_R                              (1 << 6)
#define MASK_ALT_R                               (1 << 7)

#define MASK_SHIFT                               (MASK_SHIFT_L | MASK_SHIFT_R)
#define MASK_CTRL                                (MASK_CTRL_L  | MASK_CTRL_R)
#define MASK_META                                (MASK_META_L  | MASK_META_R)
#define MASK_ALT                                 (MASK_ALT_L   | MASK_ALT_R)

#define MASK_BUTTON1                             (1 << 8)
#define MASK_BUTTON2                             (1 << 9)
#define MASK_BUTTON3                             (1 << 10)
#define MASK_BUTTON4                             (1 << 11)
#define MASK_BUTTON5                             (1 << 12)

#define MASK_NUM_LOCK                            (1 << 13)
#define MASK_CAPS_LOCK                           (1 << 14)
#define MASK_SCROLL_LOCK                         (1 << 15)

#define MASK_EMULATED                            (1 << 30)
#define MASK_CONSUMED                            (1U << 31)
/* End Virtual Modifier Masks */


/* Begin Virtual Mouse Buttons */
#define MOUSE_NOBUTTON                           0    // Any Button
#define MOUSE_BUTTON1                            1    // Left Button
#define MOUSE_BUTTON2                            2    // Right Button
#define MOUSE_BUTTON3                            3    // Middle Button
#define MOUSE_BUTTON4                            4    // Extra Mouse Button
#define MOUSE_BUTTON5                            5    // Extra Mouse Button

#define WHEEL_UNIT_SCROLL                        1    // Scroll by line
#define WHEEL_BLOCK_SCROLL                       2    // Scroll by page

#define WHEEL_VERTICAL_DIRECTION                 3
#define WHEEL_HORIZONTAL_DIRECTION               4
/* End Virtual Mouse Buttons */

#ifdef __cplusplus
extern "C" {
#endif

    /* Begin Main Functions */

    // Set the logger callback function.
    void hook_set_logger_proc(logger_t logger_proc, void *user_data);

    // Set the event callback function.
    void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data);

    // Insert the event hook for all events.
    int hook_run();

    // Insert the event hook for keyboard events.
    int hook_run_keyboard();

    // Insert the event hook for mouse events.
    int hook_run_mouse();

    // Withdraw the event hook.
    int hook_stop();

    // Send a virtual event back to the system.
    int hook_post_event(uiohook_event * const event);

    // Send virtual events back to the system.
    int hook_post_events(uiohook_event * const events, uint32_t size);

    // Send text back to the system.
    int hook_post_text(const uint16_t * const text);

    // Initialize the virtual devices used for event simulation.
    int hook_init_virtual_devices(const char * const application_name);

    // Destroy the virtual devices used for event simulation.
    int hook_destroy_virtual_devices();

    /* End Main Functions */

    /* Begin Platform-Independent Configuration Functions */

    // Get the bitmask of the optional features which are supported on the current platform.
    uint32_t hook_get_optional_feature_support();

    // Check whether key typed events are enabled.
    bool hook_is_key_typed_enabled();

    // Enable or disable key typed events.
    void hook_set_key_typed_enabled(bool enabled);

    /* End Platform-Independent Configuration Functions */

    /* Begin macOS Configuration Functions */

    // Check whether access to macOS Accessibility API is enabled, optionally prompting the user if it is not.
    bool hook_is_ax_api_enabled(bool promptUserIfDisabled);

    // Gets whether to prompt the user if access to macOS Accessibility API is disabled.
    bool hook_get_prompt_user_if_ax_api_disabled();

    // Sets whether to prompt the user if access to macOS Accessibility API is disabled.
    void hook_set_prompt_user_if_ax_api_disabled(bool promptUserIfDisabled);

    // Gets the frequency for polling access to macOS Accessibility API.
    uint32_t hook_get_ax_poll_frequency();

    // Sets the frequency for polling access to macOS Accessibility API.
    void hook_set_ax_poll_frequency(uint32_t frequency);

    /* End macOS Configuration Functions */

    /* Begin Linux Configuration Functions */

    // Get the delay between character sending when posting text on X11.
    uint64_t hook_get_post_text_delay_x11();

    // Set the delay between character sending when posting text on X11.
    void hook_set_post_text_delay_x11(uint64_t delay);

    // Get the mode which selects the back-end on Linux.
    int hook_get_linux_mode();

    // Set the mode which selects the back-end on Linux.
    int hook_set_linux_mode(int mode);

    // Get the back-end which is currently loaded on Linux.
    int hook_get_loaded_linux_backend();

    // Supply the device node descriptors instead of opening them directly.
    void hook_set_device_procs(device_open_t open_proc, device_close_t close_proc, void *user_data);

    /* End Linux Configuration Functions */

    /* Begin System Info Functions */

    // Retrieves an array of screen data for each available monitor.
    screen_data* hook_create_screen_info(unsigned char *count);

    // Retrieves the keyboard auto repeat rate.
    long int hook_get_auto_repeat_rate();

    // Retrieves the keyboard auto repeat delay.
    long int hook_get_auto_repeat_delay();

    // Retrieves the mouse acceleration multiplier.
    long int hook_get_pointer_acceleration_multiplier();

    // Retrieves the mouse acceleration threshold.
    long int hook_get_pointer_acceleration_threshold();

    // Retrieves the mouse sensitivity.
    long int hook_get_pointer_sensitivity();

    // Retrieves the double/triple click interval.
    long int hook_get_multi_click_time();

    /* End System Info Functions */

#ifdef __cplusplus
}
#endif

#endif
