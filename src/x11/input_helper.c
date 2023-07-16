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
    uint16_t vcode;
    char *x11_key_name;
    unsigned int x11_key_code;
} key_mapping;

static unsigned char *mouse_button_table;
Display *helper_disp;  // Where do we open this display?  FIXME Use the ctrl display via init param

static uint16_t modifier_mask;

static key_mapping vcode_keycode_table[] = {
    { .vcode = VC_ESCAPE,                .x11_key_name = "ESC"  },
    { .vcode = VC_F1,                    .x11_key_name = "FK01" },
    { .vcode = VC_F2,                    .x11_key_name = "FK02" },
    { .vcode = VC_F3,                    .x11_key_name = "FK03" },
    { .vcode = VC_F4,                    .x11_key_name = "FK04" },
    { .vcode = VC_F5,                    .x11_key_name = "FK05" },
    { .vcode = VC_F6,                    .x11_key_name = "FK06" },
    { .vcode = VC_F7,                    .x11_key_name = "FK07" },
    { .vcode = VC_F8,                    .x11_key_name = "FK08" },
    { .vcode = VC_F9,                    .x11_key_name = "FK09" },
    { .vcode = VC_F10,                   .x11_key_name = "FK10" },
    { .vcode = VC_F11,                   .x11_key_name = "FK11" },
    { .vcode = VC_F12,                   .x11_key_name = "FK12" },
    { .vcode = VC_F13,                   .x11_key_name = "FK13" },
    { .vcode = VC_F14,                   .x11_key_name = "FK14" },
    { .vcode = VC_F15,                   .x11_key_name = "FK15" },
    { .vcode = VC_F16,                   .x11_key_name = "FK16" },
    { .vcode = VC_F17,                   .x11_key_name = "FK17" },
    { .vcode = VC_F18,                   .x11_key_name = "FK18" },
    { .vcode = VC_F19,                   .x11_key_name = "FK19" },
    { .vcode = VC_F20,                   .x11_key_name = "FK20" },
    { .vcode = VC_F21,                   .x11_key_name = "FK21" },
    { .vcode = VC_F22,                   .x11_key_name = "FK22" },
    { .vcode = VC_F23,                   .x11_key_name = "FK23" },
    { .vcode = VC_F24,                   .x11_key_name = "FK24" },
    { .vcode = VC_BACK_QUOTE,            .x11_key_name = "TLDE" },
    { .vcode = VC_1,                     .x11_key_name = "AE01" },
    { .vcode = VC_2,                     .x11_key_name = "AE02" },
    { .vcode = VC_3,                     .x11_key_name = "AE03" },
    { .vcode = VC_4,                     .x11_key_name = "AE04" },
    { .vcode = VC_5,                     .x11_key_name = "AE05" },
    { .vcode = VC_6,                     .x11_key_name = "AE06" },
    { .vcode = VC_7,                     .x11_key_name = "AE07" },
    { .vcode = VC_8,                     .x11_key_name = "AE08" },
    { .vcode = VC_9,                     .x11_key_name = "AE09" },
    { .vcode = VC_0,                     .x11_key_name = "AE10" },
    { .vcode = VC_MINUS,                 .x11_key_name = "AE11" },
    { .vcode = VC_EQUALS,                .x11_key_name = "AE12" },
    { .vcode = VC_BACKSPACE,             .x11_key_name = "BKSP" },
    { .vcode = VC_TAB,                   .x11_key_name = "TAB"  },
    { .vcode = VC_Q,                     .x11_key_name = "AD01" },
    { .vcode = VC_W,                     .x11_key_name = "AD02" },
    { .vcode = VC_E,                     .x11_key_name = "AD03" },
    { .vcode = VC_R,                     .x11_key_name = "AD04" },
    { .vcode = VC_T,                     .x11_key_name = "AD05" },
    { .vcode = VC_Y,                     .x11_key_name = "AD06" },
    { .vcode = VC_U,                     .x11_key_name = "AD07" },
    { .vcode = VC_I,                     .x11_key_name = "AD08" },
    { .vcode = VC_O,                     .x11_key_name = "AD09" },
    { .vcode = VC_P,                     .x11_key_name = "AD10" },
    { .vcode = VC_OPEN_BRACKET,          .x11_key_name = "AD11" },
    { .vcode = VC_CLOSE_BRACKET,         .x11_key_name = "AD12" },
    { .vcode = VC_ENTER,                 .x11_key_name = "RTRN" },
    { .vcode = VC_CAPS_LOCK,             .x11_key_name = "CAPS" },
    { .vcode = VC_A,                     .x11_key_name = "AC01" },
    { .vcode = VC_S,                     .x11_key_name = "AC02" },
    { .vcode = VC_D,                     .x11_key_name = "AC03" },
    { .vcode = VC_F,                     .x11_key_name = "AC04" },
    { .vcode = VC_G,                     .x11_key_name = "AC05" },
    { .vcode = VC_H,                     .x11_key_name = "AC06" },
    { .vcode = VC_J,                     .x11_key_name = "AC07" },
    { .vcode = VC_K,                     .x11_key_name = "AC08" },
    { .vcode = VC_L,                     .x11_key_name = "AC09" },
    { .vcode = VC_SEMICOLON,             .x11_key_name = "AC10" },
    { .vcode = VC_QUOTE,                 .x11_key_name = "AC11" },
    { .vcode = VC_BACK_SLASH,            .x11_key_name = "AC12" },
    { .vcode = VC_BACK_SLASH,            .x11_key_name = "BKSL" },
    { .vcode = VC_SHIFT_L,               .x11_key_name = "LFSH" },
    { .vcode = VC_Z,                     .x11_key_name = "AB01" },
    { .vcode = VC_X,                     .x11_key_name = "AB02" },
    { .vcode = VC_C,                     .x11_key_name = "AB03" },
    { .vcode = VC_V,                     .x11_key_name = "AB04" },
    { .vcode = VC_B,                     .x11_key_name = "AB05" },
    { .vcode = VC_N,                     .x11_key_name = "AB06" },
    { .vcode = VC_M,                     .x11_key_name = "AB07" },
    { .vcode = VC_COMMA,                 .x11_key_name = "AB08" },
    { .vcode = VC_PERIOD,                .x11_key_name = "AB09" },
    { .vcode = VC_SLASH,                 .x11_key_name = "AB10" },
    { .vcode = VC_SHIFT_R,               .x11_key_name = "RTSH" },
    { .vcode = VC_102,                   .x11_key_name = "LSGT" },
    { .vcode = VC_ALT_L,                 .x11_key_name = "LALT" },
    { .vcode = VC_CONTROL_L,             .x11_key_name = "LCTL" },
    { .vcode = VC_META_L,                .x11_key_name = "LWIN" },
    { .vcode = VC_META_L,                .x11_key_name = "LMTA" },
    { .vcode = VC_SPACE,                 .x11_key_name = "SPCE" },
    { .vcode = VC_META_R,                .x11_key_name = "RWIN" },
    { .vcode = VC_META_R,                .x11_key_name = "RMTA" },
    { .vcode = VC_CONTROL_R,             .x11_key_name = "RCTL" },
    { .vcode = VC_ALT_R,                 .x11_key_name = "RALT" },
    { .vcode = VC_CONTEXT_MENU,          .x11_key_name = "COMP" },
    { .vcode = VC_CONTEXT_MENU,          .x11_key_name = "MENU" },
    { .vcode = VC_PRINT_SCREEN,          .x11_key_name = "PRSC" },
    { .vcode = VC_SCROLL_LOCK,           .x11_key_name = "SCLK" },
    { .vcode = VC_PAUSE,                 .x11_key_name = "PAUS" },
    { .vcode = VC_INSERT,                .x11_key_name = "INS"  },
    { .vcode = VC_HOME,                  .x11_key_name = "HOME" },
    { .vcode = VC_PAGE_UP,               .x11_key_name = "PGUP" },
    { .vcode = VC_DELETE,                .x11_key_name = "DELE" },
    { .vcode = VC_END,                   .x11_key_name = "END"  },
    { .vcode = VC_PAGE_DOWN,             .x11_key_name = "PGDN" },
    { .vcode = VC_UP,                    .x11_key_name = "UP"   },
    { .vcode = VC_LEFT,                  .x11_key_name = "LEFT" },
    { .vcode = VC_DOWN,                  .x11_key_name = "DOWN" },
    { .vcode = VC_RIGHT,                 .x11_key_name = "RGHT" },
    { .vcode = VC_NUM_LOCK,              .x11_key_name = "NMLK" },
    { .vcode = VC_KP_DIVIDE,             .x11_key_name = "KPDV" },
    { .vcode = VC_KP_MULTIPLY,           .x11_key_name = "KPMU" },
    { .vcode = VC_KP_SUBTRACT,           .x11_key_name = "KPSU" },
    { .vcode = VC_KP_7,                  .x11_key_name = "KP7"  },
    { .vcode = VC_KP_8,                  .x11_key_name = "KP8"  },
    { .vcode = VC_KP_9,                  .x11_key_name = "KP9"  },
    { .vcode = VC_KP_ADD,                .x11_key_name = "KPAD" },
    { .vcode = VC_KP_4,                  .x11_key_name = "KP4"  },
    { .vcode = VC_KP_5,                  .x11_key_name = "KP5"  },
    { .vcode = VC_KP_6,                  .x11_key_name = "KP6"  },
    { .vcode = VC_KP_1,                  .x11_key_name = "KP1"  },
    { .vcode = VC_KP_2,                  .x11_key_name = "KP2"  },
    { .vcode = VC_KP_3,                  .x11_key_name = "KP3"  },
    { .vcode = VC_KP_ENTER,              .x11_key_name = "KPEN" },
    { .vcode = VC_KP_0,                  .x11_key_name = "KP0"  },
    { .vcode = VC_KP_DECIMAL,            .x11_key_name = "KPDL" },
    { .vcode = VC_KP_EQUALS,             .x11_key_name = "KPEQ" },
    { .vcode = VC_KATAKANA_HIRAGANA,     .x11_key_name = "HKTG" },
    { .vcode = VC_UNDERSCORE,            .x11_key_name = "AB11" },
    { .vcode = VC_CONVERT,               .x11_key_name = "HENK" },
    { .vcode = VC_NONCONVERT,            .x11_key_name = "MUHE" },
    { .vcode = VC_YEN,                   .x11_key_name = "AE13" },
    { .vcode = VC_KATAKANA,              .x11_key_name = "KATA" },
    { .vcode = VC_HIRAGANA,              .x11_key_name = "HIRA" },
    { .vcode = VC_JP_COMMA,              .x11_key_name = "JPCM" },
    { .vcode = VC_HANGUL,                .x11_key_name = "HNGL" },
    { .vcode = VC_HANJA,                 .x11_key_name = "HJCV" },
    { .vcode = VC_VOLUME_MUTE,           .x11_key_name = "MUTE" },
    { .vcode = VC_VOLUME_DOWN,           .x11_key_name = "VOL-" },
    { .vcode = VC_VOLUME_UP,             .x11_key_name = "VOL+" },
    { .vcode = VC_POWER,                 .x11_key_name = "POWR" },
    { .vcode = VC_STOP,                  .x11_key_name = "STOP" },
    { .vcode = VC_AGAIN,                 .x11_key_name = "AGAI" },
    { .vcode = VC_PROPS,                 .x11_key_name = "PROP" },
    { .vcode = VC_UNDO,                  .x11_key_name = "UNDO" },
    { .vcode = VC_FRONT,                 .x11_key_name = "FRNT" },
    { .vcode = VC_COPY,                  .x11_key_name = "COPY" },
    { .vcode = VC_OPEN,                  .x11_key_name = "OPEN" },
    { .vcode = VC_PASTE,                 .x11_key_name = "PAST" },
    { .vcode = VC_FIND,                  .x11_key_name = "FIND" },
    { .vcode = VC_CUT,                   .x11_key_name = "CUT"  },
    { .vcode = VC_HELP,                  .x11_key_name = "HELP" },
    { .vcode = VC_SWITCH_VIDEO_MODE,     .x11_key_name = "OUTP" },
    { .vcode = VC_KEYBOARD_LIGHT_TOGGLE, .x11_key_name = "KITG" },
    { .vcode = VC_KEYBOARD_LIGHT_DOWN,   .x11_key_name = "KIDN" },
    { .vcode = VC_KEYBOARD_LIGHT_UP,     .x11_key_name = "KIUP" },
    { .vcode = VC_LINE_FEED,             .x11_key_name = "LNFD" },
    { .vcode = VC_MACRO,                 .x11_key_name = "I120" },
    { .vcode = VC_VOLUME_MUTE,           .x11_key_name = "I121" },
    { .vcode = VC_VOLUME_DOWN,           .x11_key_name = "I122" },
    { .vcode = VC_VOLUME_UP,             .x11_key_name = "I123" },
    { .vcode = VC_POWER,                 .x11_key_name = "I124" },
    { .vcode = VC_KP_EQUALS,             .x11_key_name = "I125" },
    { .vcode = VC_KP_PLUS_MINUS,         .x11_key_name = "I126" },
    { .vcode = VC_PAUSE,                 .x11_key_name = "I127" },
    { .vcode = VC_SCALE,                 .x11_key_name = "I128" },
    { .vcode = VC_KP_SEPARATOR,          .x11_key_name = "I129" },
    { .vcode = VC_HANGUL,                .x11_key_name = "I130" },
    { .vcode = VC_HANJA,                 .x11_key_name = "I131" },
    { .vcode = VC_YEN,                   .x11_key_name = "I132" },
    { .vcode = VC_META_L,                .x11_key_name = "I133" },
    { .vcode = VC_META_R,                .x11_key_name = "I134" },
    { .vcode = VC_CONTEXT_MENU,          .x11_key_name = "I135" },
    { .vcode = VC_STOP,                  .x11_key_name = "I136" },
    { .vcode = VC_AGAIN,                 .x11_key_name = "I137" },
    { .vcode = VC_PROPS,                 .x11_key_name = "I138" },
    { .vcode = VC_UNDO,                  .x11_key_name = "I139" },
    { .vcode = VC_FRONT,                 .x11_key_name = "I140" },
    { .vcode = VC_COPY,                  .x11_key_name = "I141" },
    { .vcode = VC_OPEN,                  .x11_key_name = "I142" },
    { .vcode = VC_PASTE,                 .x11_key_name = "I143" },
    { .vcode = VC_FIND,                  .x11_key_name = "I144" },
    { .vcode = VC_CUT,                   .x11_key_name = "I145" },
    { .vcode = VC_HELP,                  .x11_key_name = "I146" },
    { .vcode = VC_CONTEXT_MENU,          .x11_key_name = "I147" },
    { .vcode = VC_APP_CALCULATOR,        .x11_key_name = "I148" },
    { .vcode = VC_SETUP,                 .x11_key_name = "I149" },
    { .vcode = VC_SLEEP,                 .x11_key_name = "I150" },
    { .vcode = VC_WAKE,                  .x11_key_name = "I151" },
    { .vcode = VC_FILE,                  .x11_key_name = "I152" },
    { .vcode = VC_SEND_FILE,             .x11_key_name = "I153" },
    { .vcode = VC_DELETE_FILE,           .x11_key_name = "I154" },
    { .vcode = VC_MODE_CHANGE,           .x11_key_name = "I155" },
    { .vcode = VC_APP_1,                 .x11_key_name = "I156" },
    { .vcode = VC_APP_2,                 .x11_key_name = "I157" },
    { .vcode = VC_APP_BROWSER,           .x11_key_name = "I158" },
    { .vcode = VC_MS_DOS,                .x11_key_name = "I159" },
    { .vcode = VC_LOCK,                  .x11_key_name = "I160" },
    { .vcode = VC_ROTATE_DISPLAY,        .x11_key_name = "I161" },
    { .vcode = VC_CYCLE_WINDOWS,         .x11_key_name = "I162" },
    { .vcode = VC_APP_MAIL,              .x11_key_name = "I163" },
    { .vcode = VC_BROWSER_FAVORITES,     .x11_key_name = "I164" },
    { .vcode = VC_COMPUTER,              .x11_key_name = "I165" },
    { .vcode = VC_BROWSER_BACK,          .x11_key_name = "I166" },
    { .vcode = VC_BROWSER_FORWARD,       .x11_key_name = "I167" },
    { .vcode = VC_MEDIA_CLOSE,           .x11_key_name = "I168" },
    { .vcode = VC_MEDIA_EJECT,           .x11_key_name = "I169" },
    { .vcode = VC_MEDIA_EJECT_CLOSE,     .x11_key_name = "I170" },
    { .vcode = VC_MEDIA_NEXT,            .x11_key_name = "I171" },
    { .vcode = VC_MEDIA_PLAY,            .x11_key_name = "I172" },
    { .vcode = VC_MEDIA_PREVIOUS,        .x11_key_name = "I173" },
    { .vcode = VC_MEDIA_STOP,            .x11_key_name = "I174" },
    { .vcode = VC_MEDIA_RECORD,          .x11_key_name = "I175" },
    { .vcode = VC_MEDIA_REWIND,          .x11_key_name = "I176" },
    { .vcode = VC_PHONE,                 .x11_key_name = "I177" },
    { .vcode = VC_ISO,                   .x11_key_name = "I178" },
    { .vcode = VC_CONFIG,                .x11_key_name = "I179" },
    { .vcode = VC_BROWSER_HOME,          .x11_key_name = "I180" },
    { .vcode = VC_BROWSER_REFRESH,       .x11_key_name = "I181" },
    { .vcode = VC_EXIT,                  .x11_key_name = "I182" },
    { .vcode = VC_MOVE,                  .x11_key_name = "I183" },
    { .vcode = VC_EDIT,                  .x11_key_name = "I184" },
    { .vcode = VC_SCROLL_UP,             .x11_key_name = "I185" },
    { .vcode = VC_SCROLL_DOWN,           .x11_key_name = "I186" },
    { .vcode = VC_KP_OPEN_PARENTHESIS,   .x11_key_name = "I187" },
    { .vcode = VC_KP_CLOSE_PARENTHESIS,  .x11_key_name = "I188" },
    { .vcode = VC_NEW,                   .x11_key_name = "I189" },
    { .vcode = VC_REDO,                  .x11_key_name = "I190" },
    { .vcode = VC_F13,                   .x11_key_name = "I191" },
    { .vcode = VC_F14,                   .x11_key_name = "I192" },
    { .vcode = VC_F15,                   .x11_key_name = "I193" },
    { .vcode = VC_F16,                   .x11_key_name = "I194" },
    { .vcode = VC_F17,                   .x11_key_name = "I195" },
    { .vcode = VC_F18,                   .x11_key_name = "I196" },
    { .vcode = VC_F19,                   .x11_key_name = "I197" },
    { .vcode = VC_F20,                   .x11_key_name = "I198" },
    { .vcode = VC_F21,                   .x11_key_name = "I199" },
    { .vcode = VC_F22,                   .x11_key_name = "I200" },
    { .vcode = VC_F23,                   .x11_key_name = "I201" },
    { .vcode = VC_F24,                   .x11_key_name = "I202" },
    { .vcode = VC_PLAY_CD,               .x11_key_name = "I208" },
    { .vcode = VC_PAUSE_CD,              .x11_key_name = "I209" },
    { .vcode = VC_APP_3,                 .x11_key_name = "I210" },
    { .vcode = VC_APP_4,                 .x11_key_name = "I211" },
    { .vcode = VC_DASHBOARD,             .x11_key_name = "I212" },
    { .vcode = VC_SUSPEND,               .x11_key_name = "I213" },
    { .vcode = VC_CLOSE,                 .x11_key_name = "I214" },
    { .vcode = VC_PLAY,                  .x11_key_name = "I215" },
    { .vcode = VC_FAST_FORWARD,          .x11_key_name = "I216" },
    { .vcode = VC_BASS_BOOST,            .x11_key_name = "I217" },
    { .vcode = VC_PRINT,                 .x11_key_name = "I218" },
    { .vcode = VC_HP,                    .x11_key_name = "I219" },
    { .vcode = VC_CAMERA,                .x11_key_name = "I220" },
    { .vcode = VC_SOUND,                 .x11_key_name = "I221" },
    { .vcode = VC_QUESTION,              .x11_key_name = "I222" },
    { .vcode = VC_EMAIL,                 .x11_key_name = "I223" },
    { .vcode = VC_CHAT,                  .x11_key_name = "I224" },
    { .vcode = VC_BROWSER_SEARCH,        .x11_key_name = "I225" },
    { .vcode = VC_CONNECT,               .x11_key_name = "I226" },
    { .vcode = VC_FINANCE,               .x11_key_name = "I227" },
    { .vcode = VC_SPORT,                 .x11_key_name = "I228" },
    { .vcode = VC_SHOP,                  .x11_key_name = "I229" },
    { .vcode = VC_ALT_ERASE,             .x11_key_name = "I230" },
    { .vcode = VC_CANCEL,                .x11_key_name = "I231" },
    { .vcode = VC_BRIGTNESS_DOWN,        .x11_key_name = "I232" },
    { .vcode = VC_BRIGTNESS_UP,          .x11_key_name = "I233" },
    { .vcode = VC_MEDIA,                 .x11_key_name = "I234" },
    { .vcode = VC_SWITCH_VIDEO_MODE,     .x11_key_name = "I235" },
    { .vcode = VC_KEYBOARD_LIGHT_TOGGLE, .x11_key_name = "I236" },
    { .vcode = VC_KEYBOARD_LIGHT_DOWN,   .x11_key_name = "I237" },
    { .vcode = VC_KEYBOARD_LIGHT_UP,     .x11_key_name = "I238" },
    { .vcode = VC_SEND,                  .x11_key_name = "I239" },
    { .vcode = VC_REPLY,                 .x11_key_name = "I240" },
    { .vcode = VC_FORWARD_MAIL,          .x11_key_name = "I241" },
    { .vcode = VC_SAVE,                  .x11_key_name = "I242" },
    { .vcode = VC_DOCUMENTS,             .x11_key_name = "I243" },
    { .vcode = VC_BATTERY,               .x11_key_name = "I244" },
    { .vcode = VC_BLUETOOTH,             .x11_key_name = "I245" },
    { .vcode = VC_WLAN,                  .x11_key_name = "I246" },
    { .vcode = VC_UWB,                   .x11_key_name = "I247" },
    { .vcode = VC_X11_UNKNOWN,           .x11_key_name = "I248" },
    { .vcode = VC_VIDEO_NEXT,            .x11_key_name = "I249" },
    { .vcode = VC_VIDEO_PREVIOUS,        .x11_key_name = "I250" },
    { .vcode = VC_BRIGTNESS_CYCLE,       .x11_key_name = "I251" },
    { .vcode = VC_BRIGTNESS_AUTO,        .x11_key_name = "I252" },
    { .vcode = VC_DISPLAY_OFF,           .x11_key_name = "I253" },
    { .vcode = VC_WWAN,                  .x11_key_name = "I254" },
    { .vcode = VC_RFKILL,                .x11_key_name = "I255" },
};

uint16_t keycode_to_vcode(KeyCode keycode) {
    uint16_t vcode = VC_UNDEFINED;

    for (unsigned int i = 0; i < sizeof(vcode_keycode_table) / sizeof(vcode_keycode_table[0]); i++) {
        if (keycode == vcode_keycode_table[i].x11_key_code) {
            vcode = vcode_keycode_table[i].vcode;
            break;
        }
    }

    return vcode;
}

KeyCode vcode_to_keycode(uint16_t vcode) {
    KeyCode key_code = 0x0;

    for (unsigned int i = 0; i < sizeof(vcode_keycode_table) / sizeof(vcode_keycode_table[0]); i++) {
        if (vcode == vcode_keycode_table[i].vcode) {
            key_code = vcode_keycode_table[i].x11_key_code;
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
        for (int i = 0; i < sizeof(vcode_keycode_table) / sizeof(*vcode_keycode_table); i++) {
            if (strncmp(vcode_keycode_table[i].x11_key_name, xkb->names->keys[key_code].name, XkbKeyNameLength) == 0) {
                vcode_keycode_table[i].x11_key_code = key_code;
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
