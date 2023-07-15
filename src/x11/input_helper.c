/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
 * https://github.com/kwhat/libuiohook/
 *
 * libUIOHook is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libUIOHook is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef USE_EPOCH_TIME
#include <sys/time.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/XKBlib.h>
static XkbDescPtr keyboard_map;

#include "input_helper.h"
#include "logger.h"

#define BUTTON_TABLE_MAX 256

typedef struct _key_mapping {
    uint16_t uiohook_key;
    char *x11_key;
    unsigned int x11_keycode;
} key_mapping;

static unsigned char *mouse_button_table;
Display *helper_disp;  // Where do we open this display?  FIXME Use the ctrl display via init param

static uint16_t modifier_mask;

static key_mapping keycode_table[] = {
    { .uiohook_key = VC_ESCAPE,                .x11_key = "ESC"  },
    { .uiohook_key = VC_F1,                    .x11_key = "FK01" },
    { .uiohook_key = VC_F2,                    .x11_key = "FK02" },
    { .uiohook_key = VC_F3,                    .x11_key = "FK03" },
    { .uiohook_key = VC_F4,                    .x11_key = "FK04" },
    { .uiohook_key = VC_F5,                    .x11_key = "FK05" },
    { .uiohook_key = VC_F6,                    .x11_key = "FK06" },
    { .uiohook_key = VC_F7,                    .x11_key = "FK07" },
    { .uiohook_key = VC_F8,                    .x11_key = "FK08" },
    { .uiohook_key = VC_F9,                    .x11_key = "FK09" },
    { .uiohook_key = VC_F10,                   .x11_key = "FK10" },
    { .uiohook_key = VC_F11,                   .x11_key = "FK11" },
    { .uiohook_key = VC_F12,                   .x11_key = "FK12" },
    { .uiohook_key = VC_F13,                   .x11_key = "FK13" },
    { .uiohook_key = VC_F14,                   .x11_key = "FK14" },
    { .uiohook_key = VC_F15,                   .x11_key = "FK15" },
    { .uiohook_key = VC_F16,                   .x11_key = "FK16" },
    { .uiohook_key = VC_F17,                   .x11_key = "FK17" },
    { .uiohook_key = VC_F18,                   .x11_key = "FK18" },
    { .uiohook_key = VC_F19,                   .x11_key = "FK19" },
    { .uiohook_key = VC_F20,                   .x11_key = "FK20" },
    { .uiohook_key = VC_F21,                   .x11_key = "FK21" },
    { .uiohook_key = VC_F22,                   .x11_key = "FK22" },
    { .uiohook_key = VC_F23,                   .x11_key = "FK23" },
    { .uiohook_key = VC_F24,                   .x11_key = "FK24" },
    { .uiohook_key = VC_BACK_QUOTE,            .x11_key = "TLDE" },
    { .uiohook_key = VC_1,                     .x11_key = "AE01" },
    { .uiohook_key = VC_2,                     .x11_key = "AE02" },
    { .uiohook_key = VC_3,                     .x11_key = "AE03" },
    { .uiohook_key = VC_4,                     .x11_key = "AE04" },
    { .uiohook_key = VC_5,                     .x11_key = "AE05" },
    { .uiohook_key = VC_6,                     .x11_key = "AE06" },
    { .uiohook_key = VC_7,                     .x11_key = "AE07" },
    { .uiohook_key = VC_8,                     .x11_key = "AE08" },
    { .uiohook_key = VC_9,                     .x11_key = "AE09" },
    { .uiohook_key = VC_0,                     .x11_key = "AE10" },
    { .uiohook_key = VC_MINUS,                 .x11_key = "AE11" },
    { .uiohook_key = VC_EQUALS,                .x11_key = "AE12" },
    { .uiohook_key = VC_BACKSPACE,             .x11_key = "BKSP" },
    { .uiohook_key = VC_Q,                     .x11_key = "AD01" },
    { .uiohook_key = VC_W,                     .x11_key = "AD02" },
    { .uiohook_key = VC_E,                     .x11_key = "AD03" },
    { .uiohook_key = VC_R,                     .x11_key = "AD04" },
    { .uiohook_key = VC_T,                     .x11_key = "AD05" },
    { .uiohook_key = VC_Y,                     .x11_key = "AD06" },
    { .uiohook_key = VC_U,                     .x11_key = "AD07" },
    { .uiohook_key = VC_I,                     .x11_key = "AD08" },
    { .uiohook_key = VC_O,                     .x11_key = "AD09" },
    { .uiohook_key = VC_P,                     .x11_key = "AD10" },
    { .uiohook_key = VC_OPEN_BRACKET,          .x11_key = "AD11" },
    { .uiohook_key = VC_CLOSE_BRACKET,         .x11_key = "AD12" },
    { .uiohook_key = VC_ENTER,                 .x11_key = "RTRN" },
    { .uiohook_key = VC_CAPS_LOCK,             .x11_key = "CAPS" },
    { .uiohook_key = VC_A,                     .x11_key = "AC01" },
    { .uiohook_key = VC_S,                     .x11_key = "AC02" },
    { .uiohook_key = VC_D,                     .x11_key = "AC03" },
    { .uiohook_key = VC_F,                     .x11_key = "AC04" },
    { .uiohook_key = VC_G,                     .x11_key = "AC05" },
    { .uiohook_key = VC_H,                     .x11_key = "AC06" },
    { .uiohook_key = VC_J,                     .x11_key = "AC07" },
    { .uiohook_key = VC_K,                     .x11_key = "AC08" },
    { .uiohook_key = VC_L,                     .x11_key = "AC09" },
    { .uiohook_key = VC_SEMICOLON,             .x11_key = "AC10" },
    { .uiohook_key = VC_QUOTE,                 .x11_key = "AC11" },
    { .uiohook_key = VC_BACK_SLASH,            .x11_key = "AC12" },
    { .uiohook_key = VC_BACK_SLASH,            .x11_key = "BKSL" },
    { .uiohook_key = VC_SHIFT_L,               .x11_key = "LFSH" },
    { .uiohook_key = VC_Z,                     .x11_key = "AB01" },
    { .uiohook_key = VC_X,                     .x11_key = "AB02" },
    { .uiohook_key = VC_C,                     .x11_key = "AB03" },
    { .uiohook_key = VC_V,                     .x11_key = "AB04" },
    { .uiohook_key = VC_B,                     .x11_key = "AB05" },
    { .uiohook_key = VC_N,                     .x11_key = "AB06" },
    { .uiohook_key = VC_M,                     .x11_key = "AB07" },
    { .uiohook_key = VC_COMMA,                 .x11_key = "AB08" },
    { .uiohook_key = VC_PERIOD,                .x11_key = "AB09" },
    { .uiohook_key = VC_SLASH,                 .x11_key = "AB10" },
    { .uiohook_key = VC_SHIFT_R,               .x11_key = "RTSH" },
    { .uiohook_key = VC_102,                   .x11_key = "LSGT" },
    { .uiohook_key = VC_ALT_L,                 .x11_key = "LALT" },
    { .uiohook_key = VC_CONTROL_L,             .x11_key = "LCTL" },
    { .uiohook_key = VC_META_L,                .x11_key = "LWIN" },
    { .uiohook_key = VC_META_L,                .x11_key = "LMTA" },
    { .uiohook_key = VC_SPACE,                 .x11_key = "SPCE" },
    { .uiohook_key = VC_META_R,                .x11_key = "RWIN" },
    { .uiohook_key = VC_META_R,                .x11_key = "RMTA" },
    { .uiohook_key = VC_CONTROL_R,             .x11_key = "RCTL" },
    { .uiohook_key = VC_ALT_R,                 .x11_key = "RALT" },
    { .uiohook_key = VC_COMPOSE,               .x11_key = "COMP" },
    { .uiohook_key = VC_COMPOSE,               .x11_key = "MENU" },
    { .uiohook_key = VC_PRINT_SCREEN,          .x11_key = "PRSC" },
    { .uiohook_key = VC_SCROLL_LOCK,           .x11_key = "SCLK" },
    { .uiohook_key = VC_PAUSE,                 .x11_key = "PAUS" },
    { .uiohook_key = VC_INSERT,                .x11_key = "INS"  },
    { .uiohook_key = VC_HOME,                  .x11_key = "HOME" },
    { .uiohook_key = VC_PAGE_UP,               .x11_key = "PGUP" },
    { .uiohook_key = VC_DELETE,                .x11_key = "DELE" },
    { .uiohook_key = VC_END,                   .x11_key = "END"  },
    { .uiohook_key = VC_PAGE_DOWN,             .x11_key = "PGDN" },
    { .uiohook_key = VC_UP,                    .x11_key = "UP"   },
    { .uiohook_key = VC_LEFT,                  .x11_key = "LEFT" },
    { .uiohook_key = VC_DOWN,                  .x11_key = "DOWN" },
    { .uiohook_key = VC_RIGHT,                 .x11_key = "RGHT" },
    { .uiohook_key = VC_NUM_LOCK,              .x11_key = "NMLK" },
    { .uiohook_key = VC_KP_DIVIDE,             .x11_key = "KPDV" },
    { .uiohook_key = VC_KP_MULTIPLY,           .x11_key = "KPMU" },
    { .uiohook_key = VC_KP_SUBTRACT,           .x11_key = "KPSU" },
    { .uiohook_key = VC_KP_SUBTRACT,           .x11_key = "KPSU" },
    { .uiohook_key = VC_KP_7,                  .x11_key = "KP7"  },
    { .uiohook_key = VC_KP_8,                  .x11_key = "KP8"  },
    { .uiohook_key = VC_KP_9,                  .x11_key = "KP9"  },
    { .uiohook_key = VC_KP_ADD,                .x11_key = "KPAD" },
    { .uiohook_key = VC_KP_4,                  .x11_key = "KP4"  },
    { .uiohook_key = VC_KP_5,                  .x11_key = "KP5"  },
    { .uiohook_key = VC_KP_6,                  .x11_key = "KP6"  },
    { .uiohook_key = VC_KP_1,                  .x11_key = "KP1"  },
    { .uiohook_key = VC_KP_2,                  .x11_key = "KP2"  },
    { .uiohook_key = VC_KP_3,                  .x11_key = "KP3"  },
    { .uiohook_key = VC_KP_ENTER,              .x11_key = "KPEN" },
    { .uiohook_key = VC_KP_0,                  .x11_key = "KP0"  },
    { .uiohook_key = VC_KP_DECIMAL,            .x11_key = "KPDL" },
    { .uiohook_key = VC_KP_EQUALS,             .x11_key = "KPEQ" },
    { .uiohook_key = VC_KATAKANA_HIRAGANA,     .x11_key = "HKTG" },
    { .uiohook_key = VC_UNDERSCORE,            .x11_key = "AB11" },
    { .uiohook_key = VC_CONVERT,               .x11_key = "HENK" },
    { .uiohook_key = VC_NONCONVERT,            .x11_key = "MUHE" },
    { .uiohook_key = VC_YEN,                   .x11_key = "AE13" },
    { .uiohook_key = VC_KATAKANA,              .x11_key = "KATA" },
    { .uiohook_key = VC_HIRAGANA,              .x11_key = "HIRA" },
    { .uiohook_key = VC_JP_COMMA,              .x11_key = "JPCM" },
    { .uiohook_key = VC_HANGUL,                .x11_key = "HNGL" },
    { .uiohook_key = VC_HANJA,                 .x11_key = "HJCV" },
    { .uiohook_key = VC_VOLUME_MUTE,           .x11_key = "MUTE" },
    { .uiohook_key = VC_VOLUME_DOWN,           .x11_key = "VOL-" },
    { .uiohook_key = VC_VOLUME_UP,             .x11_key = "VOL+" },
    { .uiohook_key = VC_POWER,                 .x11_key = "POWR" },
    { .uiohook_key = VC_STOP,                  .x11_key = "STOP" },
    { .uiohook_key = VC_AGAIN,                 .x11_key = "AGAI" },
    { .uiohook_key = VC_PROPS,                 .x11_key = "PROP" },
    { .uiohook_key = VC_UNDO,                  .x11_key = "UNDO" },
    { .uiohook_key = VC_FRONT,                 .x11_key = "FRNT" },
    { .uiohook_key = VC_COPY,                  .x11_key = "COPY" },
    { .uiohook_key = VC_OPEN,                  .x11_key = "OPEN" },
    { .uiohook_key = VC_PASTE,                 .x11_key = "PAST" },
    { .uiohook_key = VC_FIND,                  .x11_key = "FIND" },
    { .uiohook_key = VC_CUT,                   .x11_key = "CUT"  },
    { .uiohook_key = VC_HELP,                  .x11_key = "HELP" },
    { .uiohook_key = VC_SWITCH_VIDEO_MODE,     .x11_key = "OUTP" },
    { .uiohook_key = VC_KEYBOARD_LIGHT_TOGGLE, .x11_key = "KITG" },
    { .uiohook_key = VC_KEYBOARD_LIGHT_DOWN,   .x11_key = "KIDN" },
    { .uiohook_key = VC_KEYBOARD_LIGHT_UP,     .x11_key = "KIUP" },
    { .uiohook_key = VC_LINE_FEED,             .x11_key = "LNFD" },
    { .uiohook_key = VC_MACRO,                 .x11_key = "I120" },
    { .uiohook_key = VC_VOLUME_MUTE,           .x11_key = "I121" },
    { .uiohook_key = VC_VOLUME_DOWN,           .x11_key = "I122" },
    { .uiohook_key = VC_VOLUME_UP,             .x11_key = "I123" },
    { .uiohook_key = VC_POWER,                 .x11_key = "I124" },
    { .uiohook_key = VC_KP_EQUALS,             .x11_key = "I125" },
    { .uiohook_key = VC_KP_PLUS_MINUS,         .x11_key = "I126" },
    { .uiohook_key = VC_PAUSE,                 .x11_key = "I127" },
    { .uiohook_key = VC_SCALE,                 .x11_key = "I128" },
    { .uiohook_key = VC_KP_SEPARATOR,          .x11_key = "I129" },
    { .uiohook_key = VC_HANGUL,                .x11_key = "I130" },
    { .uiohook_key = VC_HANJA,                 .x11_key = "I131" },
    { .uiohook_key = VC_YEN,                   .x11_key = "I132" },
    { .uiohook_key = VC_META_L,                .x11_key = "I133" },
    { .uiohook_key = VC_META_R,                .x11_key = "I134" },
    { .uiohook_key = VC_COMPOSE,               .x11_key = "I135" },
    { .uiohook_key = VC_STOP,                  .x11_key = "I136" },
    { .uiohook_key = VC_AGAIN,                 .x11_key = "I137" },
    { .uiohook_key = VC_PROPS,                 .x11_key = "I138" },
    { .uiohook_key = VC_UNDO,                  .x11_key = "I139" },
    { .uiohook_key = VC_FRONT,                 .x11_key = "I140" },
    { .uiohook_key = VC_COPY,                  .x11_key = "I141" },
    { .uiohook_key = VC_OPEN,                  .x11_key = "I142" },
    { .uiohook_key = VC_PASTE,                 .x11_key = "I143" },
    { .uiohook_key = VC_FIND,                  .x11_key = "I144" },
    { .uiohook_key = VC_CUT,                   .x11_key = "I145" },
    { .uiohook_key = VC_HELP,                  .x11_key = "I146" },
    { .uiohook_key = VC_CONTEXT_MENU,          .x11_key = "I147" },
    { .uiohook_key = VC_APP_CALCULATOR,        .x11_key = "I148" },
    { .uiohook_key = VC_SETUP,                 .x11_key = "I149" },
    { .uiohook_key = VC_SLEEP,                 .x11_key = "I150" },
    { .uiohook_key = VC_WAKE,                  .x11_key = "I151" },
    { .uiohook_key = VC_FILE,                  .x11_key = "I152" },
    { .uiohook_key = VC_SEND_FILE,             .x11_key = "I153" },
    { .uiohook_key = VC_DELETE_FILE,           .x11_key = "I154" },
    { .uiohook_key = VC_MODE_CHANGE,           .x11_key = "I155" },
    { .uiohook_key = VC_APP_1,                 .x11_key = "I156" },
    { .uiohook_key = VC_APP_2,                 .x11_key = "I157" },
    { .uiohook_key = VC_APP_BROWSER,           .x11_key = "I158" },
    { .uiohook_key = VC_MS_DOS,                .x11_key = "I159" },
    { .uiohook_key = VC_LOCK,                  .x11_key = "I160" },
    { .uiohook_key = VC_ROTATE_DISPLAY,        .x11_key = "I161" },
    { .uiohook_key = VC_CYCLE_WINDOWS,         .x11_key = "I162" },
    { .uiohook_key = VC_APP_MAIL,              .x11_key = "I163" },
    { .uiohook_key = VC_BROWSER_FAVORITES,     .x11_key = "I164" },
    { .uiohook_key = VC_COMPUTER,              .x11_key = "I165" },
    { .uiohook_key = VC_BROWSER_BACK,          .x11_key = "I166" },
    { .uiohook_key = VC_BROWSER_FORWARD,       .x11_key = "I167" },
    { .uiohook_key = VC_MEDIA_CLOSE,           .x11_key = "I168" },
    { .uiohook_key = VC_MEDIA_EJECT,           .x11_key = "I169" },
    { .uiohook_key = VC_MEDIA_EJECT_CLOSE,     .x11_key = "I170" },
    { .uiohook_key = VC_MEDIA_NEXT,            .x11_key = "I171" },
    { .uiohook_key = VC_MEDIA_PLAY,            .x11_key = "I172" },
    { .uiohook_key = VC_MEDIA_PREVIOUS,        .x11_key = "I173" },
    { .uiohook_key = VC_MEDIA_STOP,            .x11_key = "I174" },
    { .uiohook_key = VC_MEDIA_RECORD,          .x11_key = "I175" },
    { .uiohook_key = VC_MEDIA_REWIND,          .x11_key = "I176" },
    { .uiohook_key = VC_PHONE,                 .x11_key = "I177" },
    { .uiohook_key = VC_ISO,                   .x11_key = "I178" },
    { .uiohook_key = VC_CONFIG,                .x11_key = "I179" },
    { .uiohook_key = VC_BROWSER_HOME,          .x11_key = "I180" },
    { .uiohook_key = VC_BROWSER_REFRESH,       .x11_key = "I181" },
    { .uiohook_key = VC_EXIT,                  .x11_key = "I182" },
    { .uiohook_key = VC_MOVE,                  .x11_key = "I183" },
    { .uiohook_key = VC_EDIT,                  .x11_key = "I184" },
    { .uiohook_key = VC_SCROLL_UP,             .x11_key = "I185" },
    { .uiohook_key = VC_SCROLL_DOWN,           .x11_key = "I186" },
    { .uiohook_key = VC_KP_LEFT_PARENTHESIS,   .x11_key = "I187" },
    { .uiohook_key = VC_KP_RIGHT_PARENTHESIS,  .x11_key = "I188" },
    { .uiohook_key = VC_NEW,                   .x11_key = "I189" },
    { .uiohook_key = VC_REDO,                  .x11_key = "I190" },
    { .uiohook_key = VC_F13,                   .x11_key = "I191" },
    { .uiohook_key = VC_F14,                   .x11_key = "I192" },
    { .uiohook_key = VC_F15,                   .x11_key = "I193" },
    { .uiohook_key = VC_F16,                   .x11_key = "I194" },
    { .uiohook_key = VC_F17,                   .x11_key = "I195" },
    { .uiohook_key = VC_F18,                   .x11_key = "I196" },
    { .uiohook_key = VC_F19,                   .x11_key = "I197" },
    { .uiohook_key = VC_F20,                   .x11_key = "I198" },
    { .uiohook_key = VC_F21,                   .x11_key = "I199" },
    { .uiohook_key = VC_F22,                   .x11_key = "I200" },
    { .uiohook_key = VC_F23,                   .x11_key = "I201" },
    { .uiohook_key = VC_F24,                   .x11_key = "I202" },
    { .uiohook_key = VC_PLAY_CD,               .x11_key = "I208" },
    { .uiohook_key = VC_PAUSE_CD,              .x11_key = "I209" },
    { .uiohook_key = VC_APP_3,                 .x11_key = "I210" },
    { .uiohook_key = VC_APP_4,                 .x11_key = "I211" },
    { .uiohook_key = VC_DASHBOARD,             .x11_key = "I212" },
    { .uiohook_key = VC_SUSPEND,               .x11_key = "I213" },
    { .uiohook_key = VC_CLOSE,                 .x11_key = "I214" },
    { .uiohook_key = VC_PLAY,                  .x11_key = "I215" },
    { .uiohook_key = VC_FAST_FORWARD,          .x11_key = "I216" },
    { .uiohook_key = VC_BASS_BOOST,            .x11_key = "I217" },
    { .uiohook_key = VC_PRINT,                 .x11_key = "I218" },
    { .uiohook_key = VC_HP,                    .x11_key = "I219" },
    { .uiohook_key = VC_CAMERA,                .x11_key = "I220" },
    { .uiohook_key = VC_SOUND,                 .x11_key = "I221" },
    { .uiohook_key = VC_QUESTION,              .x11_key = "I222" },
    { .uiohook_key = VC_EMAIL,                 .x11_key = "I223" },
    { .uiohook_key = VC_CHAT,                  .x11_key = "I224" },
    { .uiohook_key = VC_BROWSER_SEARCH,        .x11_key = "I225" },
    { .uiohook_key = VC_CONNECT,               .x11_key = "I226" },
    { .uiohook_key = VC_FINANCE,               .x11_key = "I227" },
    { .uiohook_key = VC_SPORT,                 .x11_key = "I228" },
    { .uiohook_key = VC_SHOP,                  .x11_key = "I229" },
    { .uiohook_key = VC_ALT_ERASE,             .x11_key = "I230" },
    { .uiohook_key = VC_CANCEL,                .x11_key = "I231" },
    { .uiohook_key = VC_BRIGTNESS_DOWN,        .x11_key = "I232" },
    { .uiohook_key = VC_BRIGTNESS_UP,          .x11_key = "I233" },
    { .uiohook_key = VC_MEDIA,                 .x11_key = "I234" },
    { .uiohook_key = VC_SWITCH_VIDEO_MODE,     .x11_key = "I235" },
    { .uiohook_key = VC_KEYBOARD_LIGHT_TOGGLE, .x11_key = "I236" },
    { .uiohook_key = VC_KEYBOARD_LIGHT_DOWN,   .x11_key = "I237" },
    { .uiohook_key = VC_KEYBOARD_LIGHT_UP,     .x11_key = "I238" },
    { .uiohook_key = VC_SEND,                  .x11_key = "I239" },
    { .uiohook_key = VC_REPLY,                 .x11_key = "I240" },
    { .uiohook_key = VC_FORWARD_MAIL,          .x11_key = "I241" },
    { .uiohook_key = VC_SAVE,                  .x11_key = "I242" },
    { .uiohook_key = VC_DOCUMENTS,             .x11_key = "I243" },
    { .uiohook_key = VC_BATTERY,               .x11_key = "I244" },
    { .uiohook_key = VC_BLUETOOTH,             .x11_key = "I245" },
    { .uiohook_key = VC_WLAN,                  .x11_key = "I246" },
    { .uiohook_key = VC_UWB,                   .x11_key = "I247" },
    { .uiohook_key = VC_X11_UNKNOWN,           .x11_key = "I248" },
    { .uiohook_key = VC_VIDEO_NEXT,            .x11_key = "I249" },
    { .uiohook_key = VC_VIDEO_PREVIOUS,        .x11_key = "I250" },
    { .uiohook_key = VC_BRIGTNESS_CYCLE,       .x11_key = "I251" },
    { .uiohook_key = VC_BRIGTNESS_AUTO,        .x11_key = "I252" },
    { .uiohook_key = VC_DISPLAY_OFF,           .x11_key = "I253" },
    { .uiohook_key = VC_WWAN,                  .x11_key = "I254" },
    { .uiohook_key = VC_RFKILL,                .x11_key = "I255" },
};

uint16_t keycode_to_vcode(KeyCode keycode) {
    uint16_t vcode = VC_UNDEFINED;

    for (unsigned int i = 0; i < sizeof(keycode_table) / sizeof(keycode_table[0]); i++) {
        if (keycode == keycode_table[i].x11_keycode) {
            vcode = keycode_table[i].uiohook_key;
            break;
        }
    }

    return vcode;
}

KeyCode vcode_to_keycode(uint16_t vcode) {
    KeyCode key_code = 0x0;

    for (unsigned int i = 0; i < sizeof(keycode_table) / sizeof(keycode_table[0]); i++) {
        if (vcode == keycode_table[i].uiohook_key) {
            key_code = keycode_table[i].x11_keycode;
            break;
        }
    }

    return key_code;
}

// Set the native modifier mask for future events.
void set_modifier_mask(uint16_t mask) {
    modifier_mask |= mask;
}

// Unset the native modifier mask for future events.
void unset_modifier_mask(uint16_t mask) {
    modifier_mask &= ~mask;
}

// Get the current native modifier mask state.
uint16_t get_modifiers() {
    return modifier_mask;
}

// Initialize the modifier lock masks.
static void initialize_locks() {
    unsigned int led_mask = 0x00;
    if (XkbGetIndicatorState(helper_disp, XkbUseCoreKbd, &led_mask) == Success) {
        if (led_mask & 0x01) {
            set_modifier_mask(MASK_CAPS_LOCK);
        } else {
            unset_modifier_mask(MASK_CAPS_LOCK);
        }

        if (led_mask & 0x02) {
            set_modifier_mask(MASK_NUM_LOCK);
        } else {
            unset_modifier_mask(MASK_NUM_LOCK);
        }

        if (led_mask & 0x04) {
            set_modifier_mask(MASK_SCROLL_LOCK);
        } else {
            unset_modifier_mask(MASK_SCROLL_LOCK);
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XkbGetIndicatorState failed to get current led mask!\n",
                __FUNCTION__, __LINE__);
    }
}

// Initialize the modifier mask to the current modifiers.
static void initialize_modifiers() {
    modifier_mask = 0x0000;

    KeyCode keycode;
    char keymap[32];
    XQueryKeymap(helper_disp, keymap);

    Window unused_win;
    int unused_int;
    unsigned int mask;
    if (XQueryPointer(helper_disp, DefaultRootWindow(helper_disp), &unused_win, &unused_win, &unused_int, &unused_int, &unused_int, &unused_int, &mask)) {
        if (mask & ShiftMask) {
            keycode = XKeysymToKeycode(helper_disp, XK_Shift_L);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_SHIFT_L); }
            keycode = XKeysymToKeycode(helper_disp, XK_Shift_R);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_SHIFT_R); }
        }
        if (mask & ControlMask) {
            keycode = XKeysymToKeycode(helper_disp, XK_Control_L);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_CTRL_L);  }
            keycode = XKeysymToKeycode(helper_disp, XK_Control_R);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_CTRL_R);  }
        }
        if (mask & Mod1Mask) {
            keycode = XKeysymToKeycode(helper_disp, XK_Alt_L);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_ALT_L);   }
            keycode = XKeysymToKeycode(helper_disp, XK_Alt_R);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_ALT_R);   }
        }
        if (mask & Mod4Mask) {
            keycode = XKeysymToKeycode(helper_disp, XK_Super_L);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_META_L);  }
            keycode = XKeysymToKeycode(helper_disp, XK_Super_R);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_META_R);  }
        }

        if (mask & Button1Mask) { set_modifier_mask(MASK_BUTTON1); }
        if (mask & Button2Mask) { set_modifier_mask(MASK_BUTTON2); }
        if (mask & Button3Mask) { set_modifier_mask(MASK_BUTTON3); }
        if (mask & Button4Mask) { set_modifier_mask(MASK_BUTTON4); }
        if (mask & Button5Mask) { set_modifier_mask(MASK_BUTTON5); }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XQueryPointer failed to get current modifiers!\n",
                __FUNCTION__, __LINE__);

        keycode = XKeysymToKeycode(helper_disp, XK_Shift_L);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_SHIFT_L); }
        keycode = XKeysymToKeycode(helper_disp, XK_Shift_R);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_SHIFT_R); }
        keycode = XKeysymToKeycode(helper_disp, XK_Control_L);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_CTRL_L);  }
        keycode = XKeysymToKeycode(helper_disp, XK_Control_R);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_CTRL_R);  }
        keycode = XKeysymToKeycode(helper_disp, XK_Alt_L);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_ALT_L);   }
        keycode = XKeysymToKeycode(helper_disp, XK_Alt_R);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_ALT_R);   }
        keycode = XKeysymToKeycode(helper_disp, XK_Super_L);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_META_L);  }
        keycode = XKeysymToKeycode(helper_disp, XK_Super_R);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_META_R);  }
    }
}

#ifdef USE_EPOCH_TIME
/* Get the current timestamp in unix epoch time. */
static uint64_t get_unix_timestamp() {
    struct timeval system_time;

    // Get the local system time in UTC.
    gettimeofday(&system_time, NULL);

    // Convert the local system time to a Unix epoch in MS.
    uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

    return timestamp;
}
#endif

/* Based on mappings from _XWireToEvent in Xlibinit.c */
void wire_data_to_event(XRecordInterceptData *recorded_data, XEvent *x_event) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = (uint64_t) recorded_data->server_time;
    #endif

	((XAnyEvent *) x_event)->serial = timestamp;

    if (recorded_data->category == XRecordFromServer) {
        XRecordDatum *data = (XRecordDatum *) recorded_data->data;
        switch (recorded_data->category) {
            //case XRecordFromClient: // TODO Should we be listening for Client Events?
            case XRecordFromServer:
                x_event->type = data->event.u.u.type;
                ((XAnyEvent *) x_event)->display = helper_disp;
                ((XAnyEvent *) x_event)->send_event = (bool) (data->event.u.u.type & 0x80);

                switch (data->type) {
                    case KeyPress:
                    case KeyRelease:
                        ((XKeyEvent *) x_event)->root           = data->event.u.keyButtonPointer.root;
                        ((XKeyEvent *) x_event)->window         = data->event.u.keyButtonPointer.event;
                        ((XKeyEvent *) x_event)->subwindow      = data->event.u.keyButtonPointer.child;
                        ((XKeyEvent *) x_event)->time           = data->event.u.keyButtonPointer.time;
                        ((XKeyEvent *) x_event)->x              = cvtINT16toInt(data->event.u.keyButtonPointer.eventX);
                        ((XKeyEvent *) x_event)->y              = cvtINT16toInt(data->event.u.keyButtonPointer.eventY);
                        ((XKeyEvent *) x_event)->x_root         = cvtINT16toInt(data->event.u.keyButtonPointer.rootX);
                        ((XKeyEvent *) x_event)->y_root         = cvtINT16toInt(data->event.u.keyButtonPointer.rootY);
                        ((XKeyEvent *) x_event)->state          = data->event.u.keyButtonPointer.state;
                        ((XKeyEvent *) x_event)->same_screen    = data->event.u.keyButtonPointer.sameScreen;
                        ((XKeyEvent *) x_event)->keycode        = data->event.u.u.detail;
                        break;

                    case ButtonPress:
                    case ButtonRelease:
                        ((XButtonEvent *) x_event)->root        = data->event.u.keyButtonPointer.root;
                        ((XButtonEvent *) x_event)->window      = data->event.u.keyButtonPointer.event;
                        ((XButtonEvent *) x_event)->subwindow   = data->event.u.keyButtonPointer.child;
                        ((XButtonEvent *) x_event)->time        = data->event.u.keyButtonPointer.time;
                        ((XButtonEvent *) x_event)->x           = cvtINT16toInt(data->event.u.keyButtonPointer.eventX);
                        ((XButtonEvent *) x_event)->y           = cvtINT16toInt(data->event.u.keyButtonPointer.eventY);
                        ((XButtonEvent *) x_event)->x_root      = cvtINT16toInt(data->event.u.keyButtonPointer.rootX);
                        ((XButtonEvent *) x_event)->y_root      = cvtINT16toInt(data->event.u.keyButtonPointer.rootY);
                        ((XButtonEvent *) x_event)->state       = data->event.u.keyButtonPointer.state;
                        ((XButtonEvent *) x_event)->same_screen = data->event.u.keyButtonPointer.sameScreen;
                        ((XButtonEvent *) x_event)->button      = data->event.u.u.detail;
                        break;

                    case MotionNotify:
                        ((XMotionEvent *) x_event)->root        = data->event.u.keyButtonPointer.root;
                        ((XMotionEvent *) x_event)->window      = data->event.u.keyButtonPointer.event;
                        ((XMotionEvent *) x_event)->subwindow   = data->event.u.keyButtonPointer.child;
                        ((XMotionEvent *) x_event)->time        = data->event.u.keyButtonPointer.time;
                        ((XMotionEvent *) x_event)->x           = cvtINT16toInt(data->event.u.keyButtonPointer.eventX);
                        ((XMotionEvent *) x_event)->y           = cvtINT16toInt(data->event.u.keyButtonPointer.eventY);
                        ((XMotionEvent *) x_event)->x_root      = cvtINT16toInt(data->event.u.keyButtonPointer.rootX);
                        ((XMotionEvent *) x_event)->y_root      = cvtINT16toInt(data->event.u.keyButtonPointer.rootY);
                        ((XMotionEvent *) x_event)->state       = data->event.u.keyButtonPointer.state;
                        ((XMotionEvent *) x_event)->same_screen = data->event.u.keyButtonPointer.sameScreen;
                        ((XMotionEvent *) x_event)->is_hint     = data->event.u.u.detail;
                        break;
                }
                break;
        }
    }
}

uint8_t button_map_lookup(uint8_t button) {
    unsigned int map_button = button;

    if (helper_disp != NULL) {
        if (mouse_button_table != NULL) {
            int map_size = XGetPointerMapping(helper_disp, mouse_button_table, BUTTON_TABLE_MAX);
            if (map_button > 0 && map_button <= map_size) {
                map_button = mouse_button_table[map_button -1];
            }
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: Mouse button map memory is unavailable!\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    // X11 numbers buttons 2 & 3 backwards from other platforms so we normalize them.
    if      (map_button == Button2) { map_button = Button3; }
    else if (map_button == Button3) { map_button = Button2; }

    return map_button;
}

bool enable_key_repeat() {
    // Attempt to setup detectable autorepeat.
    // NOTE: is_auto_repeat is NOT stdbool!
    Bool is_auto_repeat = False;

    // Enable detectable auto-repeat.
    XkbSetDetectableAutoRepeat(helper_disp, True, &is_auto_repeat);

    return is_auto_repeat;
}

size_t x_key_event_lookup(XKeyEvent *x_event, wchar_t *surrogate, size_t length, KeySym *keysym) {
    XIC xic = NULL;
    XIM xim = NULL;

    // KeyPress events can use Xutf8LookupString but KeyRelease events cannot.
    if (x_event->type == KeyPress) {
        XSetLocaleModifiers("");
        xim = XOpenIM(helper_disp, NULL, NULL, NULL);
        if (xim == NULL) {
            // fallback to internal input method
            XSetLocaleModifiers("@im=none");
            xim = XOpenIM(helper_disp, NULL, NULL, NULL);
        }

        if (xim != NULL) {
            Window root_default = XDefaultRootWindow(helper_disp);
            xic = XCreateIC(xim,
                XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
                XNClientWindow, root_default,
                XNFocusWindow,  root_default,
                NULL);

            if (xic == NULL) {
                logger(LOG_LEVEL_WARN, "%s [%u]: XCreateIC() failed!\n",
                        __FUNCTION__, __LINE__);
            }
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: XOpenIM() failed!\n",
                    __FUNCTION__, __LINE__);
        }
    }

    size_t count = 0;
    char buffer[5] = {};
    
    if (xic != NULL) {
        count = Xutf8LookupString(xic, x_event, buffer, sizeof(buffer), keysym, NULL);
        XDestroyIC(xic);
    } else {
        count = XLookupString(x_event, buffer, sizeof(buffer), keysym, NULL);
    }

    if (xim != NULL) {
        XCloseIM(xim);
    }

    // If we produced a string and we have a buffer, convert to 16-bit surrogate pairs.
    if (count > 0) {
        if (length == 0 || surrogate == NULL) {
            count = 0;
        } else {
            // See https://en.wikipedia.org/wiki/UTF-8#Examples
            const uint8_t utf8_bitmask_table[] = {
                0x3F, // 00111111, non-first (if > 1 byte)
                0x7F, // 01111111, first (if 1 byte)
                0x1F, // 00011111, first (if 2 bytes)
                0x0F, // 00001111, first (if 3 bytes)
                0x07  // 00000111, first (if 4 bytes)
            };

            uint32_t codepoint = utf8_bitmask_table[count] & buffer[0];
            for (unsigned int i = 1; i < count; i++) {
                codepoint = (codepoint << 6) | (utf8_bitmask_table[0] & buffer[i]);
            }

            if (codepoint <= 0xFFFF) {
                count = 1;
                surrogate[0] = codepoint;
            } else if (length > 1) {
                // if codepoint > 0xFFFF, split into lead (high) / trail (low) surrogate ranges
                // See https://unicode.org/faq/utf_bom.html#utf16-4
                const uint32_t lead_offset = 0xD800 - (0x10000 >> 10);

                count = 2;
                surrogate[0] = lead_offset + (codepoint >> 10); // lead,  first  [0]
                surrogate[1] = 0xDC00 + (codepoint & 0x3FF);    // trail, second [1]
            } else {
                count = 0;
                logger(LOG_LEVEL_WARN, "%s [%u]: Surrogate buffer overflow detected!\n",
                        __FUNCTION__, __LINE__);
            }
        }

    }

    return count;
}

void load_input_helper() {
    int ev, err, major = XkbMajorVersion, minor = XkbMinorVersion, res;
    Display* dpy = XkbOpenDisplay(NULL, &ev, &err, &major, &minor, &res);

    if(!dpy) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XkbOpenDisplay failed! (%#X)\n",
                __FUNCTION__, __LINE__, res);

        return;
    }

    XkbDescPtr xkb = XkbGetKeyboard(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
    for (int key_code = xkb->min_key_code; key_code < xkb->max_key_code; key_code++) {
        for (int i = 0; i < sizeof(keycode_table) / sizeof(*keycode_table); i++) {
            if (strncmp(keycode_table[i].x11_key, xkb->names->keys[key_code].name, XkbKeyNameLength) == 0) {
                keycode_table[i].x11_keycode = key_code;
            }
        }
    }

    XkbFreeKeyboard(xkb, XkbAllComponentsMask, True);
    XFree(dpy);

    // Setup memory for mouse button mapping.
    mouse_button_table = malloc(sizeof(unsigned char) * BUTTON_TABLE_MAX);
    if (mouse_button_table == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for mouse button map!\n",
                __FUNCTION__, __LINE__);

        //return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }
}

void unload_input_helper() {
    if (mouse_button_table != NULL) {
        free(mouse_button_table);
        mouse_button_table = NULL;
    }
}
