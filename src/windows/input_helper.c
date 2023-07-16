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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <uiohook.h>
#include <windows.h>

#include "logger.h"
#include "input_helper.h"


static uint16_t modifier_mask;

static const uint16_t vcode_keycode_table[][2] = {
/*  { vcode,               vk_code                }, */
    { VC_CANCEL,              VK_CANCEL              },
    { VC_BACKSPACE,           VK_BACK                },
    { VC_TAB,                 VK_TAB                 },
    { VC_KP_CLEAR,            VK_CLEAR               },
    { VC_KP_CLEAR,            VK_OEM_CLEAR           },
    { VC_ENTER,               VK_RETURN              },
    { VC_KP_ENTER,            VK_RETURN              },
    { VC_SHIFT_L,             VK_LSHIFT              },
    { VC_SHIFT_R,             VK_RSHIFT              },
    { VC_SHIFT_L,             VK_SHIFT               },
    { VC_CONTROL_L,           VK_LCONTROL            },
    { VC_CONTROL_R,           VK_RCONTROL            },
    { VC_CONTROL_L,           VK_CONTROL             },
    { VC_ALT_L,               VK_LMENU               },
    { VC_ALT_R,               VK_RMENU               },
    { VC_ALT_L,               VK_MENU                },
    { VC_PAUSE,               VK_PAUSE               },
    { VC_CAPS_LOCK,           VK_CAPITAL             },
    { VC_KANA,                VK_KANA                },
    { VC_HANGUL,              VK_HANGUL              },
    { VC_IME_ON,              VK_IME_ON              },
    { VC_JUNJA,               VK_JUNJA               },
    { VC_FINAL,               VK_FINAL	             },
    { VC_HANJA,               VK_HANJA	             },
    { VC_KANJI,               VK_KANJI	             },
    { VC_IME_OFF,             VK_IME_OFF             },
    { VC_ESCAPE,              VK_ESCAPE              },
    { VC_CONVERT,             VK_CONVERT             },
    { VC_NONCONVERT,          VK_NONCONVERT          },
    { VC_ACCEPT,              VK_ACCEPT              },
    { VC_MODE_CHANGE,         VK_MODECHANGE          },
    { VC_SPACE,               VK_SPACE               },
    { VC_PAGE_UP,             VK_PRIOR               },
    { VC_PAGE_DOWN,           VK_NEXT                },
    { VC_END,                 VK_END                 },
    { VC_HOME,                VK_HOME                },
    { VC_LEFT,                VK_LEFT                },
    { VC_UP,                  VK_UP                  },
    { VC_RIGHT,               VK_RIGHT               },
    { VC_DOWN,                VK_DOWN                },
    { VC_SELECT,              VK_SELECT              },
    { VC_PRINT,               VK_PRINT               },
    { VC_EXECUTE,             VK_EXECUTE             },
    { VC_PRINT_SCREEN,        VK_SNAPSHOT            },
    { VC_INSERT,              VK_INSERT              },
    { VC_DELETE,              VK_DELETE              },
    { VC_HELP,                VK_HELP                },
    { VC_0,                   0x30 /* 0 */           },
    { VC_1,                   0x31 /* 1 */           },
    { VC_2,                   0x32 /* 2 */           },
    { VC_3,                   0x33 /* 3 */           },
    { VC_4,                   0x34 /* 4 */           },
    { VC_5,                   0x35 /* 5 */           },
    { VC_6,                   0x36 /* 6 */           },
    { VC_7,                   0x37 /* 7 */           },
    { VC_8,                   0x38 /* 8 */           },
    { VC_9,                   0x39 /* 9 */           },
    { VC_A,                   0x41 /* A */           },
    { VC_B,                   0x42 /* B */           },
    { VC_C,                   0x43 /* C */           },
    { VC_D,                   0x44 /* D */           },
    { VC_E,                   0x45 /* E */           },
    { VC_F,                   0x46 /* F */           },
    { VC_G,                   0x47 /* G */           },
    { VC_H,                   0x48 /* H */           },
    { VC_I,                   0x49 /* I */           },
    { VC_J,                   0x4A /* J */           },
    { VC_K,                   0x4B /* K */           },
    { VC_L,                   0x4C /* L */           },
    { VC_M,                   0x4D /* M */           },
    { VC_N,                   0x4E /* N */           },
    { VC_O,                   0x4F /* O */           },
    { VC_P,                   0x50 /* P */           },
    { VC_Q,                   0x51 /* Q */           },
    { VC_R,                   0x52 /* R */           },
    { VC_S,                   0x53 /* S */           },
    { VC_T,                   0x54 /* T */           },
    { VC_U,                   0x55 /* U */           },
    { VC_V,                   0x56 /* V */           },
    { VC_W,                   0x57 /* W */           },
    { VC_X,                   0x58 /* X */           },
    { VC_Y,                   0x59 /* Y */           },
    { VC_Z,                   0x5A /* Z */           },
    { VC_META_L,              VK_LWIN                },
    { VC_META_R,              VK_RWIN                },
    { VC_CONTEXT_MENU,        VK_APPS                },
    { VC_SLEEP,               VK_SLEEP               },
    { VC_KP_0,                VK_NUMPAD0             },
    { VC_KP_1,                VK_NUMPAD1             },
    { VC_KP_2,                VK_NUMPAD2             },
    { VC_KP_3,                VK_NUMPAD3             },
    { VC_KP_4,                VK_NUMPAD4             },
    { VC_KP_5,                VK_NUMPAD5             },
    { VC_KP_6,                VK_NUMPAD6             },
    { VC_KP_7,                VK_NUMPAD7             },
    { VC_KP_8,                VK_NUMPAD8             },
    { VC_KP_9,                VK_NUMPAD9             },
    { VC_KP_9,                VK_NUMPAD9             },
    { VC_KP_MULTIPLY,         VK_MULTIPLY            },
    { VC_KP_ADD,              VK_ADD                 },
    { VC_KP_SEPARATOR,        VK_SEPARATOR           },
    { VC_KP_SUBTRACT,         VK_SUBTRACT            },
    { VC_KP_DECIMAL,          VK_DECIMAL             },
    { VC_KP_DIVIDE,           VK_DIVIDE              },
    { VC_F1,                  VK_F1                  },
    { VC_F2,                  VK_F2                  },
    { VC_F3,                  VK_F3                  },
    { VC_F4,                  VK_F4                  },
    { VC_F5,                  VK_F5                  },
    { VC_F6,                  VK_F6                  },
    { VC_F7,                  VK_F7                  },
    { VC_F8,                  VK_F8                  },
    { VC_F9,                  VK_F9                  },
    { VC_F10,                 VK_F10                 },
    { VC_F11,                 VK_F11                 },
    { VC_F12,                 VK_F12                 },
    { VC_F13,                 VK_F13                 },
    { VC_F14,                 VK_F14                 },
    { VC_F15,                 VK_F15                 },
    { VC_F16,                 VK_F16                 },
    { VC_F17,                 VK_F17                 },
    { VC_F18,                 VK_F18                 },
    { VC_F19,                 VK_F19                 },
    { VC_F20,                 VK_F20                 },
    { VC_F21,                 VK_F21                 },
    { VC_F22,                 VK_F22                 },
    { VC_F23,                 VK_F23                 },
    { VC_F24,                 VK_F24                 },
    { VC_NUM_LOCK,            VK_NUMLOCK             },
    { VC_SCROLL_LOCK,         VK_SCROLL              },
    { VC_KP_EQUALS,           0x92 /* Numpad = */    },
    { VC_BROWSER_BACK,        VK_BROWSER_BACK        },
    { VC_BROWSER_FORWARD,     VK_BROWSER_FORWARD     },
    { VC_BROWSER_REFRESH,     VK_BROWSER_REFRESH     },
    { VC_BROWSER_STOP,        VK_BROWSER_STOP        },
    { VC_BROWSER_SEARCH,      VK_BROWSER_SEARCH      },
    { VC_BROWSER_FAVORITES,   VK_BROWSER_FAVORITES   },
    { VC_BROWSER_HOME,        VK_BROWSER_HOME        },
    { VC_VOLUME_MUTE,         VK_VOLUME_MUTE         },
    { VC_VOLUME_DOWN,         VK_VOLUME_DOWN         },
    { VC_VOLUME_UP,           VK_VOLUME_UP           },
    { VC_MEDIA_NEXT,          VK_MEDIA_NEXT_TRACK    },
    { VC_MEDIA_PREVIOUS,      VK_MEDIA_PREV_TRACK    },
    { VC_MEDIA_STOP,          VK_MEDIA_STOP          },
    { VC_MEDIA_PLAY,          VK_MEDIA_PLAY_PAUSE    },
    { VC_APP_MAIL,            VK_LAUNCH_MAIL         },
    { VC_MEDIA_SELECT,        VK_LAUNCH_MEDIA_SELECT },
    { VC_APP_1,               VK_LAUNCH_APP1         },
    { VC_APP_2,               VK_LAUNCH_APP2         },
    { VC_SEMICOLON,           VK_OEM_1               },
    { VC_EQUALS,              VK_OEM_PLUS            },
    { VC_COMMA,               VK_OEM_COMMA           },
    { VC_MINUS,               VK_OEM_MINUS           },
    { VC_PERIOD,              VK_OEM_PERIOD          },
    { VC_SLASH,               VK_OEM_2               },
    { VC_BACK_QUOTE,          VK_OEM_3               },
    { VC_OPEN_BRACKET,        VK_OEM_4               },
    { VC_BACK_SLASH,          VK_OEM_5               },
    { VC_CLOSE_BRACKET,       VK_OEM_6               },
    { VC_QUOTE,               VK_OEM_7               },
    { VC_MISC,                VK_OEM_8               },
    { VC_102,                 VK_OEM_102             },
    { VC_PROCESS,             VK_PROCESSKEY          },
    { VC_ATTN,                VK_ATTN                },
    { VC_CR_SEL,              VK_CRSEL               },
    { VC_EX_SEL,              VK_EXSEL               },
    { VC_ERASE_EOF,           VK_EREOF               },
    { VC_PLAY,                VK_PLAY                },
    { VC_ZOOM,                VK_ZOOM                },
    { VC_NO_NAME,             VK_NONAME              },
    { VC_PA1,                 VK_PA1                 },
};

unsigned short keycode_to_vcode(DWORD vk_code, DWORD flags) {
    unsigned short vcode = VC_UNDEFINED;

    for (unsigned int i = 0; i < sizeof(vcode_keycode_table) / sizeof(vcode_keycode_table[0]); i++) {
        if (vk_code == vcode_keycode_table[i][1]) {
            vcode = vcode_keycode_table[i][0];
            break;
        }
    }

    if (vcode == VC_ENTER && flags & KEYEVENTF_EXTENDEDKEY) {
        vcode = VC_KP_ENTER;
    }

    return vcode;
}

DWORD vcode_to_keycode(unsigned short vcode) {
    unsigned short keycode = 0x0000;

    for (unsigned int i = 0; i < sizeof(vcode_keycode_table) / sizeof(vcode_keycode_table[0]); i++) {
        if (vcode == vcode_keycode_table[i][0]) {
            keycode = vcode_keycode_table[i][1];
            break;
        }
    }

    return keycode;
}

void set_modifier_mask(uint16_t mask) {
    modifier_mask |= mask;
}

void unset_modifier_mask(uint16_t mask) {
    modifier_mask &= ~mask;
}

uint16_t get_modifiers() {
    return modifier_mask;
}


/***********************************************************************
 * The following code is based on code provided by Marc-André Moreau
 * to work around a failure to support dead keys in the ToUnicode() API.
 * According to the author some parts were taken directly from
 * Microsoft's kbd.h header file that is shipped with the Windows Driver
 * Development Kit.
 *
 * The original code was substantially modified to provide the following:
 *   1) More dynamic code structure.
 *   2) Support for compilers that do not implement _ptr64 (GCC / LLVM).
 *   3) Support for Wow64 at runtime via 32-bit binary.
 *   4) Support for contextual language switching.
 *
 * I have contacted Marc-André Moreau who has granted permission for
 * his original source code to be used under the Public Domain.  Although
 * the libUIOHook library as a whole is currently covered under the LGPLv3,
 * please feel free to use and learn from the source code contained in the
 * following functions under the terms of the Public Domain.
 *
 * For further reading and the original code, please visit:
 *   http://legacy.docdroppers.org/wiki/index.php?title=Writing_Keyloggers
 *   http://www.techmantras.com/content/writing-keyloggers-full-length-tutorial
 *
 ***********************************************************************/

// Structure and pointers for the keyboard locale cache.
typedef struct _KeyboardLocale {
    HKL id;                                 // Locale ID
    HINSTANCE library;                      // Keyboard DLL instance.
    PVK_TO_BIT pVkToBit;                    // Pointers struct arrays.
    PVK_TO_WCHAR_TABLE pVkToWcharTable;
    PDEADKEY pDeadKey;
    struct _KeyboardLocale* next;
} KeyboardLocale;

static KeyboardLocale* locale_first = NULL;
static KeyboardLocale* locale_current = NULL;
static WCHAR deadChar = WCH_NONE;

// Amount of pointer padding to apply for Wow64 instances.
static unsigned short int ptr_padding = 0;

#if defined(_WIN32) && !defined(_WIN64)
// Small function to check and see if we are executing under Wow64.
static BOOL is_wow64() {
    BOOL status = FALSE;

    LPFN_ISWOW64PROCESS pIsWow64Process = (LPFN_ISWOW64PROCESS)
            GetProcAddress(GetModuleHandle("kernel32"), "IsWow64Process");

    if (pIsWow64Process != NULL) {
        HANDLE current_proc = GetCurrentProcess();

        if (!pIsWow64Process(current_proc, &status)) {
            status = FALSE;

            logger(LOG_LEVEL_DEBUG, "%s [%u]: pIsWow64Process(%#p, %#p) failed!\n",
                    __FUNCTION__, __LINE__, current_proc, &status);
        }
    }

    return status;
}
#endif

// Locate the DLL that contains the current keyboard layout.
static int get_keyboard_layout_file(char *layoutFile, DWORD bufferSize) {
    int status = UIOHOOK_FAILURE;
    HKEY hKey;
    DWORD varType = REG_SZ;

    char kbdName[KL_NAMELENGTH * 4];
    if (GetKeyboardLayoutName(kbdName)) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Found keyboard layout \"%s\".\n",
                __FUNCTION__, __LINE__, kbdName);

        const char *regPrefix = "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\";
        size_t regPathSize = strlen(regPrefix) + strlen(kbdName) + 1;
        char *regPath = malloc(regPathSize);
        if (regPath != NULL) {
            strcpy_s(regPath, regPathSize, regPrefix);
            strcat_s(regPath, regPathSize, kbdName);

            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, (LPCTSTR) regPath, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
                const char *regKey =  "Layout File";
                if (RegQueryValueEx(hKey, regKey, NULL, &varType, (LPBYTE) layoutFile, &bufferSize) == ERROR_SUCCESS) {
                    RegCloseKey(hKey);
                    status = UIOHOOK_SUCCESS;
                } else {
                    logger(LOG_LEVEL_WARN, "%s [%u]: RegOpenKeyEx failed to open key: \"%s\"!\n",
                            __FUNCTION__, __LINE__, regKey);
                }
            } else {
                logger(LOG_LEVEL_WARN, "%s [%u]: RegOpenKeyEx failed to open key: \"%s\"!\n",
                        __FUNCTION__, __LINE__, regPath);
            }

            free(regPath);
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: malloc(%u) failed!\n",
                    __FUNCTION__, __LINE__, regPathSize);
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: GetKeyboardLayoutName() failed!\n",
                __FUNCTION__, __LINE__);
    }

    return status;
}

// Returns the number of locales that were loaded.
static int refresh_locale_list() {
    int count = 0;

    // Get the number of layouts the user has activated.
    int hkl_size = GetKeyboardLayoutList(0, NULL);
    if (hkl_size > 0) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: GetKeyboardLayoutList(0, NULL) found %i layouts.\n",
                __FUNCTION__, __LINE__, hkl_size);

        // Get the thread id that currently has focus for our default.
        DWORD focus_pid = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
        HKL hlk_focus = GetKeyboardLayout(focus_pid);
        HKL hlk_default = GetKeyboardLayout(0);
        HKL *hkl_list = malloc(sizeof(HKL) * hkl_size);

        int new_size = GetKeyboardLayoutList(hkl_size, hkl_list);
        if (new_size > 0) {
            if (new_size != hkl_size) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Locale size mismatch!  Expected %i, received %i!\n",
                        __FUNCTION__, __LINE__, hkl_size, new_size);
            } else {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: Received %i locales.\n",
                        __FUNCTION__, __LINE__, new_size);
            }

            KeyboardLocale* locale_previous = NULL;
            KeyboardLocale* locale_item = locale_first;

            // Go though the linked list and remove KeyboardLocale's that are
            // no longer loaded.
            while (locale_item != NULL) {
                // Check to see if the old HKL is in the new list.
                bool is_loaded = false;
                for (int i = 0; i < new_size && !is_loaded; i++) {
                    if (locale_item->id == hkl_list[i]) {
                        // Flag and jump out of the loop.
                        hkl_list[i] = NULL;
                        is_loaded = true;
                    }
                }


                if (is_loaded) {
                    logger(LOG_LEVEL_DEBUG, "%s [%u]: Found locale ID %#p in the cache.\n",
                            __FUNCTION__, __LINE__, locale_item->id);

                    // Set the previous local to the current locale.
                    locale_previous = locale_item;

                    // Check and see if the locale is our current active locale.
                    if (locale_item->id == hlk_focus) {
                        locale_current = locale_item;
                    }

                    count++;
                } else {
                    logger(LOG_LEVEL_DEBUG, "%s [%u]: Removing locale ID %#p from the cache.\n",
                            __FUNCTION__, __LINE__, locale_item->id);

                    // If the old id is not in the new list, remove it.
                    locale_previous->next = locale_item->next;

                    // Make sure the locale_current points NULL or something valid.
                    if (locale_item == locale_current) {
                        locale_current = NULL;
                    }

                    // Free the memory used by locale_item;
                    free(locale_item);

                    // Set the item to the pervious item to guarantee a next.
                    locale_item = locale_previous;
                }

                // Iterate to the next linked list item.
                locale_item = locale_item->next;
            }


            // Insert anything new into the linked list.
            for (int i = 0; i < new_size; i++) {
                // Check to see if the item was already in the list.
                if (hkl_list[i] != NULL) {
                    // Set the active keyboard layout for this thread to the HKL.
                    ActivateKeyboardLayout(hkl_list[i], 0x00);

                    // Try to pull the current keyboard layout DLL from the registry.
                    char layoutFile[MAX_PATH];
                    if (get_keyboard_layout_file(layoutFile, MAX_PATH) == UIOHOOK_SUCCESS) {
                        // You can't trust the %SYSPATH%, look it up manually.
                        char systemDirectory[MAX_PATH];
                        if (GetSystemDirectory(systemDirectory, MAX_PATH) != 0) {
                            char kbdLayoutFilePath[MAX_PATH];
                            snprintf(kbdLayoutFilePath, MAX_PATH, "%s\\%s", systemDirectory, layoutFile);

                            logger(LOG_LEVEL_DEBUG, "%s [%u]: Loading layout for %#p: %s.\n",
                                    __FUNCTION__, __LINE__, hkl_list[i], layoutFile);

                            // Create the new locale item.
                            locale_item = malloc(sizeof(KeyboardLocale));
                            locale_item->id = hkl_list[i];
                            locale_item->library = LoadLibrary(kbdLayoutFilePath);

                            #if __GNUC__
                            #pragma GCC diagnostic push
                            #pragma GCC diagnostic ignored "-Wcast-function-type"
                            #endif
                            // Get the function pointer from the library to get the keyboard layer descriptor.
                            KbdLayerDescriptor pKbdLayerDescriptor = (KbdLayerDescriptor) GetProcAddress(locale_item->library, "KbdLayerDescriptor");
                            #if __GNUC__
                            #pragma GCC diagnostic pop
                            #endif

                            if (pKbdLayerDescriptor != NULL) {
                                PKBDTABLES pKbd = pKbdLayerDescriptor();

                                // Store the memory address of the following 3 structures.
                                BYTE *base = (BYTE *) pKbd;

                                // First element of each structure, no offset adjustment needed.
                                locale_item->pVkToBit = pKbd->pCharModifiers->pVkToBit;

                                // Second element of pKbd, +4 byte offset on wow64.
                                locale_item->pVkToWcharTable = *((PVK_TO_WCHAR_TABLE *) (base + offsetof(KBDTABLES, pVkToWcharTable) + ptr_padding));

                                // Third element of pKbd, +8 byte offset on wow64.
                                locale_item->pDeadKey = *((PDEADKEY *) (base + offsetof(KBDTABLES, pDeadKey) + (ptr_padding * 2)));

                                // This will always be added to the end of the list.
                                locale_item->next = NULL;

                                // Insert the item into the linked list.
                                if (locale_previous == NULL) {
                                    // If nothing came before, the list is empty.
                                    locale_first = locale_item;
                                } else {
                                    // Append the new locale to the end of the list.
                                    locale_previous->next = locale_item;
                                }

                                // Check and see if the locale is our current active locale.
                                if (locale_item->id == hlk_focus) {
                                    locale_current = locale_item;
                                }

                                // Set the pervious locale item to the new one.
                                locale_previous = locale_item;

                                count++;
                            } else {
                                logger(LOG_LEVEL_ERROR, "%s [%u]: GetProcAddress() failed for KbdLayerDescriptor!\n",
                                        __FUNCTION__, __LINE__);

                                FreeLibrary(locale_item->library);
                                free(locale_item);
                                locale_item = NULL;
                            }
                        } else {
                            logger(LOG_LEVEL_ERROR, "%s [%u]: GetSystemDirectory() failed!\n",
                                    __FUNCTION__, __LINE__);
                        }
                    } else {
                        logger(LOG_LEVEL_ERROR, "%s [%u]: Could not find keyboard map for locale %#p!\n",
                                __FUNCTION__, __LINE__, hkl_list[i]);
                    }
                }
            }
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: GetKeyboardLayoutList() failed!\n",
                    __FUNCTION__, __LINE__);

            // TODO Try and recover by using the current layout.
            // Hint: Use locale_id instead of hkl_list[i] in the loop above.
        }

        free(hkl_list);
        ActivateKeyboardLayout(hlk_default, 0x00);
    }

    return count;
}

// Returns the number of chars written to the buffer.
SIZE_T keycode_to_unicode(DWORD keycode, DWORD scancode, PWCHAR buffer, LPWORD char_types, int size) {
    // Get the thread id that currently has focus and ask for its current locale.
    DWORD focus_pid = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    HKL locale_id = GetKeyboardLayout(focus_pid);
    if (locale_id == NULL) {
        // Default to the current thread's layout if the focused window fails.
        locale_id = GetKeyboardLayout(0);
    }

    // If the current Locale is not the new locale, search the linked list.
    if (locale_current == NULL || locale_current->id != locale_id) {
        locale_current = NULL;
        KeyboardLocale* locale_item = locale_first;

        // Search the linked list...
        while (locale_item != NULL && locale_item->id != locale_id) {
            locale_item = locale_item->next;
        }

        // You may already be a winner!
        if (locale_item != NULL && locale_item->id == locale_id) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Activating keyboard layout %#p.\n",
                    __FUNCTION__, __LINE__, locale_item->id);

            // Switch the current locale.
            locale_current = locale_item;
            locale_item = NULL;

            // If they layout changes the dead key state needs to be reset.
            // This is consistent with the way Windows handles locale changes.
            deadChar = WCH_NONE;
        } else {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Refreshing locale cache.\n",
                    __FUNCTION__, __LINE__);

            refresh_locale_list();
        }
    }

    // Initialize to empty.
    SIZE_T charCount = 0;

    // Check and make sure the Unicode helper was loaded.
    if (locale_current != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Using keyboard layout %#p.\n",
                __FUNCTION__, __LINE__, locale_current->id);

        BYTE keyboard_state[256] = { 0 };
        GetKeyState(0); // This apparently forces GetKeyboardState to get more up-to-date data.
        
        // Get current keyboard state (to know which modifiers have been pressed, as well as Caps Lock).
        BOOL success = GetKeyboardState(keyboard_state);

        if (!success) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: GetKeyboardState() failed! (%#lX)\n",
                __FUNCTION__, __LINE__, (unsigned long)GetLastError());
            return 0;
        }

        // Look up the Unicode code for the key without changing the keyboard state.
        int result = ToUnicodeEx(keycode, scancode, keyboard_state, buffer, size, 1 << 2, locale_current->id);
        charCount = result > 0 ? result : 0;

        success = GetStringTypeW(CT_CTYPE1, buffer, size, char_types);

        if (!success) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: GetStringTypeW() failed! (%#lX)\n",
                __FUNCTION__, __LINE__, (unsigned long)GetLastError());
            return 0;
        }
    }

    return charCount;
}

// Returns the number of locales that were loaded.
int load_input_helper() {
    #if defined(_WIN32) && !defined(_WIN64)
    if (is_wow64()) {
        ptr_padding = sizeof(void *);
    }
    #endif

    int count = refresh_locale_list();

    logger(LOG_LEVEL_DEBUG, "%s [%u]: refresh_locale_list() found %i locale(s).\n",
            __FUNCTION__, __LINE__, count);

    return count;
}

// Returns the number of locales that were removed.
int unload_input_helper() {
    int count = 0;

    // Cleanup and free memory from the old list.
    KeyboardLocale* locale_item = locale_first;
    while (locale_item != NULL) {
        // Remove the first item from the linked list.
        FreeLibrary(locale_item->library);
        locale_first = locale_item->next;
        free(locale_item);
        locale_item = locale_first;

        count++;
    }

    // Reset the current local.
    locale_current = NULL;

    return count;
}
