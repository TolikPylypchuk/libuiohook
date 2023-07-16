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
    uint16_t uiocode;
    char *x11_key_name;
    unsigned int x11_key_code;
} key_mapping;

static unsigned char *mouse_button_table;
Display *helper_disp;  // Where do we open this display?  FIXME Use the ctrl display via init param

static uint16_t modifier_mask;

static key_mapping uiocode_keycode_table[] = {
    { .uiocode = VC_ESCAPE,                .x11_key_name = "ESC"  },
    { .uiocode = VC_F1,                    .x11_key_name = "FK01" },
    { .uiocode = VC_F2,                    .x11_key_name = "FK02" },
    { .uiocode = VC_F3,                    .x11_key_name = "FK03" },
    { .uiocode = VC_F4,                    .x11_key_name = "FK04" },
    { .uiocode = VC_F5,                    .x11_key_name = "FK05" },
    { .uiocode = VC_F6,                    .x11_key_name = "FK06" },
    { .uiocode = VC_F7,                    .x11_key_name = "FK07" },
    { .uiocode = VC_F8,                    .x11_key_name = "FK08" },
    { .uiocode = VC_F9,                    .x11_key_name = "FK09" },
    { .uiocode = VC_F10,                   .x11_key_name = "FK10" },
    { .uiocode = VC_F11,                   .x11_key_name = "FK11" },
    { .uiocode = VC_F12,                   .x11_key_name = "FK12" },
    { .uiocode = VC_F13,                   .x11_key_name = "FK13" },
    { .uiocode = VC_F14,                   .x11_key_name = "FK14" },
    { .uiocode = VC_F15,                   .x11_key_name = "FK15" },
    { .uiocode = VC_F16,                   .x11_key_name = "FK16" },
    { .uiocode = VC_F17,                   .x11_key_name = "FK17" },
    { .uiocode = VC_F18,                   .x11_key_name = "FK18" },
    { .uiocode = VC_F19,                   .x11_key_name = "FK19" },
    { .uiocode = VC_F20,                   .x11_key_name = "FK20" },
    { .uiocode = VC_F21,                   .x11_key_name = "FK21" },
    { .uiocode = VC_F22,                   .x11_key_name = "FK22" },
    { .uiocode = VC_F23,                   .x11_key_name = "FK23" },
    { .uiocode = VC_F24,                   .x11_key_name = "FK24" },
    { .uiocode = VC_BACK_QUOTE,            .x11_key_name = "TLDE" },
    { .uiocode = VC_1,                     .x11_key_name = "AE01" },
    { .uiocode = VC_2,                     .x11_key_name = "AE02" },
    { .uiocode = VC_3,                     .x11_key_name = "AE03" },
    { .uiocode = VC_4,                     .x11_key_name = "AE04" },
    { .uiocode = VC_5,                     .x11_key_name = "AE05" },
    { .uiocode = VC_6,                     .x11_key_name = "AE06" },
    { .uiocode = VC_7,                     .x11_key_name = "AE07" },
    { .uiocode = VC_8,                     .x11_key_name = "AE08" },
    { .uiocode = VC_9,                     .x11_key_name = "AE09" },
    { .uiocode = VC_0,                     .x11_key_name = "AE10" },
    { .uiocode = VC_MINUS,                 .x11_key_name = "AE11" },
    { .uiocode = VC_EQUALS,                .x11_key_name = "AE12" },
    { .uiocode = VC_BACKSPACE,             .x11_key_name = "BKSP" },
    { .uiocode = VC_TAB,                   .x11_key_name = "TAB"  },
    { .uiocode = VC_Q,                     .x11_key_name = "AD01" },
    { .uiocode = VC_W,                     .x11_key_name = "AD02" },
    { .uiocode = VC_E,                     .x11_key_name = "AD03" },
    { .uiocode = VC_R,                     .x11_key_name = "AD04" },
    { .uiocode = VC_T,                     .x11_key_name = "AD05" },
    { .uiocode = VC_Y,                     .x11_key_name = "AD06" },
    { .uiocode = VC_U,                     .x11_key_name = "AD07" },
    { .uiocode = VC_I,                     .x11_key_name = "AD08" },
    { .uiocode = VC_O,                     .x11_key_name = "AD09" },
    { .uiocode = VC_P,                     .x11_key_name = "AD10" },
    { .uiocode = VC_OPEN_BRACKET,          .x11_key_name = "AD11" },
    { .uiocode = VC_CLOSE_BRACKET,         .x11_key_name = "AD12" },
    { .uiocode = VC_ENTER,                 .x11_key_name = "RTRN" },
    { .uiocode = VC_CAPS_LOCK,             .x11_key_name = "CAPS" },
    { .uiocode = VC_A,                     .x11_key_name = "AC01" },
    { .uiocode = VC_S,                     .x11_key_name = "AC02" },
    { .uiocode = VC_D,                     .x11_key_name = "AC03" },
    { .uiocode = VC_F,                     .x11_key_name = "AC04" },
    { .uiocode = VC_G,                     .x11_key_name = "AC05" },
    { .uiocode = VC_H,                     .x11_key_name = "AC06" },
    { .uiocode = VC_J,                     .x11_key_name = "AC07" },
    { .uiocode = VC_K,                     .x11_key_name = "AC08" },
    { .uiocode = VC_L,                     .x11_key_name = "AC09" },
    { .uiocode = VC_SEMICOLON,             .x11_key_name = "AC10" },
    { .uiocode = VC_QUOTE,                 .x11_key_name = "AC11" },
    { .uiocode = VC_BACK_SLASH,            .x11_key_name = "AC12" },
    { .uiocode = VC_BACK_SLASH,            .x11_key_name = "BKSL" },
    { .uiocode = VC_SHIFT_L,               .x11_key_name = "LFSH" },
    { .uiocode = VC_Z,                     .x11_key_name = "AB01" },
    { .uiocode = VC_X,                     .x11_key_name = "AB02" },
    { .uiocode = VC_C,                     .x11_key_name = "AB03" },
    { .uiocode = VC_V,                     .x11_key_name = "AB04" },
    { .uiocode = VC_B,                     .x11_key_name = "AB05" },
    { .uiocode = VC_N,                     .x11_key_name = "AB06" },
    { .uiocode = VC_M,                     .x11_key_name = "AB07" },
    { .uiocode = VC_COMMA,                 .x11_key_name = "AB08" },
    { .uiocode = VC_PERIOD,                .x11_key_name = "AB09" },
    { .uiocode = VC_SLASH,                 .x11_key_name = "AB10" },
    { .uiocode = VC_SHIFT_R,               .x11_key_name = "RTSH" },
    { .uiocode = VC_102,                   .x11_key_name = "LSGT" },
    { .uiocode = VC_ALT_L,                 .x11_key_name = "LALT" },
    { .uiocode = VC_CONTROL_L,             .x11_key_name = "LCTL" },
    { .uiocode = VC_META_L,                .x11_key_name = "LWIN" },
    { .uiocode = VC_META_L,                .x11_key_name = "LMTA" },
    { .uiocode = VC_SPACE,                 .x11_key_name = "SPCE" },
    { .uiocode = VC_META_R,                .x11_key_name = "RWIN" },
    { .uiocode = VC_META_R,                .x11_key_name = "RMTA" },
    { .uiocode = VC_CONTROL_R,             .x11_key_name = "RCTL" },
    { .uiocode = VC_ALT_R,                 .x11_key_name = "RALT" },
    { .uiocode = VC_CONTEXT_MENU,          .x11_key_name = "COMP" },
    { .uiocode = VC_CONTEXT_MENU,          .x11_key_name = "MENU" },
    { .uiocode = VC_PRINT_SCREEN,          .x11_key_name = "PRSC" },
    { .uiocode = VC_SCROLL_LOCK,           .x11_key_name = "SCLK" },
    { .uiocode = VC_PAUSE,                 .x11_key_name = "PAUS" },
    { .uiocode = VC_INSERT,                .x11_key_name = "INS"  },
    { .uiocode = VC_HOME,                  .x11_key_name = "HOME" },
    { .uiocode = VC_PAGE_UP,               .x11_key_name = "PGUP" },
    { .uiocode = VC_DELETE,                .x11_key_name = "DELE" },
    { .uiocode = VC_END,                   .x11_key_name = "END"  },
    { .uiocode = VC_PAGE_DOWN,             .x11_key_name = "PGDN" },
    { .uiocode = VC_UP,                    .x11_key_name = "UP"   },
    { .uiocode = VC_LEFT,                  .x11_key_name = "LEFT" },
    { .uiocode = VC_DOWN,                  .x11_key_name = "DOWN" },
    { .uiocode = VC_RIGHT,                 .x11_key_name = "RGHT" },
    { .uiocode = VC_NUM_LOCK,              .x11_key_name = "NMLK" },
    { .uiocode = VC_KP_DIVIDE,             .x11_key_name = "KPDV" },
    { .uiocode = VC_KP_MULTIPLY,           .x11_key_name = "KPMU" },
    { .uiocode = VC_KP_SUBTRACT,           .x11_key_name = "KPSU" },
    { .uiocode = VC_KP_7,                  .x11_key_name = "KP7"  },
    { .uiocode = VC_KP_8,                  .x11_key_name = "KP8"  },
    { .uiocode = VC_KP_9,                  .x11_key_name = "KP9"  },
    { .uiocode = VC_KP_ADD,                .x11_key_name = "KPAD" },
    { .uiocode = VC_KP_4,                  .x11_key_name = "KP4"  },
    { .uiocode = VC_KP_5,                  .x11_key_name = "KP5"  },
    { .uiocode = VC_KP_6,                  .x11_key_name = "KP6"  },
    { .uiocode = VC_KP_1,                  .x11_key_name = "KP1"  },
    { .uiocode = VC_KP_2,                  .x11_key_name = "KP2"  },
    { .uiocode = VC_KP_3,                  .x11_key_name = "KP3"  },
    { .uiocode = VC_KP_ENTER,              .x11_key_name = "KPEN" },
    { .uiocode = VC_KP_0,                  .x11_key_name = "KP0"  },
    { .uiocode = VC_KP_DECIMAL,            .x11_key_name = "KPDL" },
    { .uiocode = VC_KP_EQUALS,             .x11_key_name = "KPEQ" },
    { .uiocode = VC_KATAKANA_HIRAGANA,     .x11_key_name = "HKTG" },
    { .uiocode = VC_UNDERSCORE,            .x11_key_name = "AB11" },
    { .uiocode = VC_CONVERT,               .x11_key_name = "HENK" },
    { .uiocode = VC_NONCONVERT,            .x11_key_name = "MUHE" },
    { .uiocode = VC_YEN,                   .x11_key_name = "AE13" },
    { .uiocode = VC_KATAKANA,              .x11_key_name = "KATA" },
    { .uiocode = VC_HIRAGANA,              .x11_key_name = "HIRA" },
    { .uiocode = VC_JP_COMMA,              .x11_key_name = "JPCM" },
    { .uiocode = VC_HANGUL,                .x11_key_name = "HNGL" },
    { .uiocode = VC_HANJA,                 .x11_key_name = "HJCV" },
    { .uiocode = VC_VOLUME_MUTE,           .x11_key_name = "MUTE" },
    { .uiocode = VC_VOLUME_DOWN,           .x11_key_name = "VOL-" },
    { .uiocode = VC_VOLUME_UP,             .x11_key_name = "VOL+" },
    { .uiocode = VC_POWER,                 .x11_key_name = "POWR" },
    { .uiocode = VC_STOP,                  .x11_key_name = "STOP" },
    { .uiocode = VC_AGAIN,                 .x11_key_name = "AGAI" },
    { .uiocode = VC_PROPS,                 .x11_key_name = "PROP" },
    { .uiocode = VC_UNDO,                  .x11_key_name = "UNDO" },
    { .uiocode = VC_FRONT,                 .x11_key_name = "FRNT" },
    { .uiocode = VC_COPY,                  .x11_key_name = "COPY" },
    { .uiocode = VC_OPEN,                  .x11_key_name = "OPEN" },
    { .uiocode = VC_PASTE,                 .x11_key_name = "PAST" },
    { .uiocode = VC_FIND,                  .x11_key_name = "FIND" },
    { .uiocode = VC_CUT,                   .x11_key_name = "CUT"  },
    { .uiocode = VC_HELP,                  .x11_key_name = "HELP" },
    { .uiocode = VC_SWITCH_VIDEO_MODE,     .x11_key_name = "OUTP" },
    { .uiocode = VC_KEYBOARD_LIGHT_TOGGLE, .x11_key_name = "KITG" },
    { .uiocode = VC_KEYBOARD_LIGHT_DOWN,   .x11_key_name = "KIDN" },
    { .uiocode = VC_KEYBOARD_LIGHT_UP,     .x11_key_name = "KIUP" },
    { .uiocode = VC_LINE_FEED,             .x11_key_name = "LNFD" },
    { .uiocode = VC_MACRO,                 .x11_key_name = "I120" },
    { .uiocode = VC_VOLUME_MUTE,           .x11_key_name = "I121" },
    { .uiocode = VC_VOLUME_DOWN,           .x11_key_name = "I122" },
    { .uiocode = VC_VOLUME_UP,             .x11_key_name = "I123" },
    { .uiocode = VC_POWER,                 .x11_key_name = "I124" },
    { .uiocode = VC_KP_EQUALS,             .x11_key_name = "I125" },
    { .uiocode = VC_KP_PLUS_MINUS,         .x11_key_name = "I126" },
    { .uiocode = VC_PAUSE,                 .x11_key_name = "I127" },
    { .uiocode = VC_SCALE,                 .x11_key_name = "I128" },
    { .uiocode = VC_KP_SEPARATOR,          .x11_key_name = "I129" },
    { .uiocode = VC_HANGUL,                .x11_key_name = "I130" },
    { .uiocode = VC_HANJA,                 .x11_key_name = "I131" },
    { .uiocode = VC_YEN,                   .x11_key_name = "I132" },
    { .uiocode = VC_META_L,                .x11_key_name = "I133" },
    { .uiocode = VC_META_R,                .x11_key_name = "I134" },
    { .uiocode = VC_CONTEXT_MENU,          .x11_key_name = "I135" },
    { .uiocode = VC_STOP,                  .x11_key_name = "I136" },
    { .uiocode = VC_AGAIN,                 .x11_key_name = "I137" },
    { .uiocode = VC_PROPS,                 .x11_key_name = "I138" },
    { .uiocode = VC_UNDO,                  .x11_key_name = "I139" },
    { .uiocode = VC_FRONT,                 .x11_key_name = "I140" },
    { .uiocode = VC_COPY,                  .x11_key_name = "I141" },
    { .uiocode = VC_OPEN,                  .x11_key_name = "I142" },
    { .uiocode = VC_PASTE,                 .x11_key_name = "I143" },
    { .uiocode = VC_FIND,                  .x11_key_name = "I144" },
    { .uiocode = VC_CUT,                   .x11_key_name = "I145" },
    { .uiocode = VC_HELP,                  .x11_key_name = "I146" },
    { .uiocode = VC_CONTEXT_MENU,          .x11_key_name = "I147" },
    { .uiocode = VC_APP_CALCULATOR,        .x11_key_name = "I148" },
    { .uiocode = VC_SETUP,                 .x11_key_name = "I149" },
    { .uiocode = VC_SLEEP,                 .x11_key_name = "I150" },
    { .uiocode = VC_WAKE,                  .x11_key_name = "I151" },
    { .uiocode = VC_FILE,                  .x11_key_name = "I152" },
    { .uiocode = VC_SEND_FILE,             .x11_key_name = "I153" },
    { .uiocode = VC_DELETE_FILE,           .x11_key_name = "I154" },
    { .uiocode = VC_MODE_CHANGE,           .x11_key_name = "I155" },
    { .uiocode = VC_APP_1,                 .x11_key_name = "I156" },
    { .uiocode = VC_APP_2,                 .x11_key_name = "I157" },
    { .uiocode = VC_APP_BROWSER,           .x11_key_name = "I158" },
    { .uiocode = VC_MS_DOS,                .x11_key_name = "I159" },
    { .uiocode = VC_LOCK,                  .x11_key_name = "I160" },
    { .uiocode = VC_ROTATE_DISPLAY,        .x11_key_name = "I161" },
    { .uiocode = VC_CYCLE_WINDOWS,         .x11_key_name = "I162" },
    { .uiocode = VC_APP_MAIL,              .x11_key_name = "I163" },
    { .uiocode = VC_BROWSER_FAVORITES,     .x11_key_name = "I164" },
    { .uiocode = VC_COMPUTER,              .x11_key_name = "I165" },
    { .uiocode = VC_BROWSER_BACK,          .x11_key_name = "I166" },
    { .uiocode = VC_BROWSER_FORWARD,       .x11_key_name = "I167" },
    { .uiocode = VC_MEDIA_CLOSE,           .x11_key_name = "I168" },
    { .uiocode = VC_MEDIA_EJECT,           .x11_key_name = "I169" },
    { .uiocode = VC_MEDIA_EJECT_CLOSE,     .x11_key_name = "I170" },
    { .uiocode = VC_MEDIA_NEXT,            .x11_key_name = "I171" },
    { .uiocode = VC_MEDIA_PLAY,            .x11_key_name = "I172" },
    { .uiocode = VC_MEDIA_PREVIOUS,        .x11_key_name = "I173" },
    { .uiocode = VC_MEDIA_STOP,            .x11_key_name = "I174" },
    { .uiocode = VC_MEDIA_RECORD,          .x11_key_name = "I175" },
    { .uiocode = VC_MEDIA_REWIND,          .x11_key_name = "I176" },
    { .uiocode = VC_PHONE,                 .x11_key_name = "I177" },
    { .uiocode = VC_ISO,                   .x11_key_name = "I178" },
    { .uiocode = VC_CONFIG,                .x11_key_name = "I179" },
    { .uiocode = VC_BROWSER_HOME,          .x11_key_name = "I180" },
    { .uiocode = VC_BROWSER_REFRESH,       .x11_key_name = "I181" },
    { .uiocode = VC_EXIT,                  .x11_key_name = "I182" },
    { .uiocode = VC_MOVE,                  .x11_key_name = "I183" },
    { .uiocode = VC_EDIT,                  .x11_key_name = "I184" },
    { .uiocode = VC_SCROLL_UP,             .x11_key_name = "I185" },
    { .uiocode = VC_SCROLL_DOWN,           .x11_key_name = "I186" },
    { .uiocode = VC_KP_OPEN_PARENTHESIS,   .x11_key_name = "I187" },
    { .uiocode = VC_KP_CLOSE_PARENTHESIS,  .x11_key_name = "I188" },
    { .uiocode = VC_NEW,                   .x11_key_name = "I189" },
    { .uiocode = VC_REDO,                  .x11_key_name = "I190" },
    { .uiocode = VC_F13,                   .x11_key_name = "I191" },
    { .uiocode = VC_F14,                   .x11_key_name = "I192" },
    { .uiocode = VC_F15,                   .x11_key_name = "I193" },
    { .uiocode = VC_F16,                   .x11_key_name = "I194" },
    { .uiocode = VC_F17,                   .x11_key_name = "I195" },
    { .uiocode = VC_F18,                   .x11_key_name = "I196" },
    { .uiocode = VC_F19,                   .x11_key_name = "I197" },
    { .uiocode = VC_F20,                   .x11_key_name = "I198" },
    { .uiocode = VC_F21,                   .x11_key_name = "I199" },
    { .uiocode = VC_F22,                   .x11_key_name = "I200" },
    { .uiocode = VC_F23,                   .x11_key_name = "I201" },
    { .uiocode = VC_F24,                   .x11_key_name = "I202" },
    { .uiocode = VC_PLAY_CD,               .x11_key_name = "I208" },
    { .uiocode = VC_PAUSE_CD,              .x11_key_name = "I209" },
    { .uiocode = VC_APP_3,                 .x11_key_name = "I210" },
    { .uiocode = VC_APP_4,                 .x11_key_name = "I211" },
    { .uiocode = VC_DASHBOARD,             .x11_key_name = "I212" },
    { .uiocode = VC_SUSPEND,               .x11_key_name = "I213" },
    { .uiocode = VC_CLOSE,                 .x11_key_name = "I214" },
    { .uiocode = VC_PLAY,                  .x11_key_name = "I215" },
    { .uiocode = VC_FAST_FORWARD,          .x11_key_name = "I216" },
    { .uiocode = VC_BASS_BOOST,            .x11_key_name = "I217" },
    { .uiocode = VC_PRINT,                 .x11_key_name = "I218" },
    { .uiocode = VC_HP,                    .x11_key_name = "I219" },
    { .uiocode = VC_CAMERA,                .x11_key_name = "I220" },
    { .uiocode = VC_SOUND,                 .x11_key_name = "I221" },
    { .uiocode = VC_QUESTION,              .x11_key_name = "I222" },
    { .uiocode = VC_EMAIL,                 .x11_key_name = "I223" },
    { .uiocode = VC_CHAT,                  .x11_key_name = "I224" },
    { .uiocode = VC_BROWSER_SEARCH,        .x11_key_name = "I225" },
    { .uiocode = VC_CONNECT,               .x11_key_name = "I226" },
    { .uiocode = VC_FINANCE,               .x11_key_name = "I227" },
    { .uiocode = VC_SPORT,                 .x11_key_name = "I228" },
    { .uiocode = VC_SHOP,                  .x11_key_name = "I229" },
    { .uiocode = VC_ALT_ERASE,             .x11_key_name = "I230" },
    { .uiocode = VC_CANCEL,                .x11_key_name = "I231" },
    { .uiocode = VC_BRIGTNESS_DOWN,        .x11_key_name = "I232" },
    { .uiocode = VC_BRIGTNESS_UP,          .x11_key_name = "I233" },
    { .uiocode = VC_MEDIA,                 .x11_key_name = "I234" },
    { .uiocode = VC_SWITCH_VIDEO_MODE,     .x11_key_name = "I235" },
    { .uiocode = VC_KEYBOARD_LIGHT_TOGGLE, .x11_key_name = "I236" },
    { .uiocode = VC_KEYBOARD_LIGHT_DOWN,   .x11_key_name = "I237" },
    { .uiocode = VC_KEYBOARD_LIGHT_UP,     .x11_key_name = "I238" },
    { .uiocode = VC_SEND,                  .x11_key_name = "I239" },
    { .uiocode = VC_REPLY,                 .x11_key_name = "I240" },
    { .uiocode = VC_FORWARD_MAIL,          .x11_key_name = "I241" },
    { .uiocode = VC_SAVE,                  .x11_key_name = "I242" },
    { .uiocode = VC_DOCUMENTS,             .x11_key_name = "I243" },
    { .uiocode = VC_BATTERY,               .x11_key_name = "I244" },
    { .uiocode = VC_BLUETOOTH,             .x11_key_name = "I245" },
    { .uiocode = VC_WLAN,                  .x11_key_name = "I246" },
    { .uiocode = VC_UWB,                   .x11_key_name = "I247" },
    { .uiocode = VC_X11_UNKNOWN,           .x11_key_name = "I248" },
    { .uiocode = VC_VIDEO_NEXT,            .x11_key_name = "I249" },
    { .uiocode = VC_VIDEO_PREVIOUS,        .x11_key_name = "I250" },
    { .uiocode = VC_BRIGTNESS_CYCLE,       .x11_key_name = "I251" },
    { .uiocode = VC_BRIGTNESS_AUTO,        .x11_key_name = "I252" },
    { .uiocode = VC_DISPLAY_OFF,           .x11_key_name = "I253" },
    { .uiocode = VC_WWAN,                  .x11_key_name = "I254" },
    { .uiocode = VC_RFKILL,                .x11_key_name = "I255" },
};

uint16_t keycode_to_uiocode(KeyCode keycode) {
    uint16_t uiocode = VC_UNDEFINED;

    for (unsigned int i = 0; i < sizeof(uiocode_keycode_table) / sizeof(uiocode_keycode_table[0]); i++) {
        if (keycode == uiocode_keycode_table[i].x11_key_name) {
            uiocode = uiocode_keycode_table[i].uiocode;
            break;
        }
    }

    return uiocode;
}

KeyCode uiocode_to_keycode(uint16_t uiocode) {
    KeyCode keycode = 0x0;

    for (unsigned int i = 0; i < sizeof(uiocode_keycode_table) / sizeof(uiocode_keycode_table[0]); i++) {
        if (uiocode == uiocode_keycode_table[i].uiocode) {
            keycode = uiocode_keycode_table[i].x11_key_name;
            break;
        }
    }

    return keycode;
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

/* Based on mappings from _XWireToEvent in Xlibinit.c */
void wire_data_to_event(XRecordInterceptData *recorded_data, XEvent *x_event) {
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

size_t event_to_unicode(XKeyEvent *x_event, wchar_t *surrogate, size_t length, KeySym *keysym) {
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
            // TODO Can we just replace all this with `count = mbstowcs(surrogate, buffer, count);`?
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
        for (int i = 0; i < sizeof(uiocode_keycode_table) / sizeof(*uiocode_keycode_table); i++) {
            if (strncmp(uiocode_keycode_table[i].x11_key, xkb->names->keys[key_code].name, XkbKeyNameLength) == 0) {
                uiocode_keycode_table[i].x11_key_code = key_code;
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

        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    initialize_modifiers();

    return UIOHOOK_SUCCESS;
}

void unload_input_helper() {
    if (mouse_button_table != NULL) {
        free(mouse_button_table);
        mouse_button_table = NULL;
    }
}
