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

#include <stdint.h>
#include <stdio.h>

#include "input_helper.h"
#include "minunit.h"
#include "uiohook.h"

#if !defined(__APPLE__) && !defined(__MACH__) && !defined(_WIN32)
#define UIOHOOK_X11
#endif

#ifdef UIOHOOK_X11
static unsigned int back_slash1;
static unsigned int back_slash2;
static unsigned int left_meta1;
static unsigned int left_meta2;
static unsigned int right_meta1;
static unsigned int right_meta2;
static unsigned int context_menu1;
static unsigned int context_menu2;

static void load_x11_keycodes() {
    back_slash1 = get_x11_keycode("AC12");
    back_slash2 = get_x11_keycode("BKSL");
    left_meta1 = get_x11_keycode("LWIN");
    left_meta2 = get_x11_keycode("BKSL");
    right_meta1 = get_x11_keycode("LWIN");
    right_meta2 = get_x11_keycode("BKSL");
    context_menu1 = get_x11_keycode("COMP");
    context_menu2 = get_x11_keycode("MENU");
}
#endif

static bool check_keycode_equality(unsigned short expected_keycode, uint32_t actual_keycode) {
    return expected_keycode == actual_keycode
    #ifdef _WIN32
        || expected_keycode == VK_CLEAR && actual_keycode == VK_OEM_CLEAR
        || actual_keycode == VK_CLEAR && expected_keycode == VK_OEM_CLEAR
    #elif defined(UIOHOOK_X11)
        || expected_keycode == back_slash1 && actual_keycode == back_slash2
        || expected_keycode == back_slash2 && actual_keycode == back_slash1
        || expected_keycode == left_meta1 && actual_keycode == left_meta2
        || expected_keycode == left_meta2 && actual_keycode == left_meta1
        || expected_keycode == right_meta1 && actual_keycode == right_meta2
        || expected_keycode == right_meta2 && actual_keycode == right_meta1
        || expected_keycode == context_menu1 && actual_keycode == context_menu2
        || expected_keycode == context_menu2 && actual_keycode == context_menu1
    #endif
    ;
}

/* Make sure all native keycodes map to virtual uiocodes */
static char * test_bidirectional_keycode() {
    #ifdef UIOHOOK_X11
    load_x11_keycodes();
    #endif

    for (unsigned short i = 0; i < 256; i++) {
        printf("Testing keycode\t\t\t%3u\t[0x%04X]\n", i, i);

        #ifdef _WIN32
        if ((i > 6 && i < 16) || i > 18) {
        #endif
            // Lookup the virtual uiocode...
            #ifdef _WIN32
            uint16_t uiocode = keycode_to_uiocode(i, 0x0);
            #else
            uint16_t uiocode = keycode_to_uiocode(i);
            #endif
            printf("\tproduced uiocode\t%3u\t[0x%04X]\n", uiocode, uiocode);

            // Lookup the native keycode...
            uint16_t keycode = (uint16_t) uiocode_to_keycode(uiocode);
            printf("\treproduced keycode\t%3u\t[0x%04X]\n", keycode, keycode);

            // If the returned virtual uiocode > 127, we used an offset to
            // calculate the keycode index used above.
            if (uiocode > 127) {
                printf("\t\tusing offset\t%3u\t[0x%04X]\n", (uiocode & 0x7F) | 0x80, (uiocode & 0x7F) | 0x80);
            }

            printf("\n");

            if (uiocode != VC_UNDEFINED) {
                mu_assert("error, uiocode to keycode failed to convert back", check_keycode_equality(i, keycode));
            }
        #ifdef _WIN32
        }
        #endif
    }

    return NULL;
}

/* Make sure all virtual uiocodes map to native keycodes */
static char * test_bidirectional_uiocode() {
    for (unsigned short i = 0; i < 256; i++) {
        printf("Testing uiocode\t\t%3u\t[0x%04X]\n", i, i);

        // Lookup the native keycode...
        uint16_t keycode = (uint16_t) uiocode_to_keycode(i);
        printf("\treproduced keycode\t%3u\t[0x%04X]\n", keycode, keycode);

        // Lookup the virtual uiocode...
        #ifdef _WIN32
        uint16_t uiocode = keycode_to_uiocode(keycode, i == VC_KP_ENTER ? KEYEVENTF_EXTENDEDKEY : 0x0);
        #else
        uint16_t uiocode = keycode_to_uiocode(keycode);
        #endif
        printf("\tproduced uiocode\t%3u\t[0x%04X]\n", uiocode, uiocode);

        // If the returned virtual uiocode > 127, we used an offset to
        // calculate the keycode index used above.
        if (uiocode > 127) {
            // Fix the uiocode for upper offsets.
            uiocode = (uiocode & 0x7F) | 0x80;
            printf("\t\tusing offset\t%3u\t[0x%04X]\n", uiocode, uiocode);
        }

        printf("\n");

        #if defined(__APPLE__) && defined(__MACH__)
        if (keycode != 255) {
        #else
        if (keycode != 0x0000) {
        #endif    
            mu_assert("error, uiocode to keycode failed to convert back", i == uiocode);
        }
    }

    return NULL;
}

char * input_helper_tests() {
    mu_run_test(test_bidirectional_keycode);
    mu_run_test(test_bidirectional_uiocode);

    return NULL;
}
