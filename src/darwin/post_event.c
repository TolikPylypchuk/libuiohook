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

#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>

#include "input_helper.h"
#include "logger.h"

static CGEventFlags current_modifier_mask = 0x00;
static CGEventType current_motion_event = kCGEventMouseMoved;
static CGMouseButton current_motion_button = 0;

UIOHOOK_API uint64_t hook_get_post_text_delay_x11() {
    // Not applicable on maOS, so does nothing
    return 0;
}

UIOHOOK_API void hook_set_post_text_delay_x11(uint64_t delay) {
    // Not applicable on maOS, so does nothing
}

static int post_key_event(uiohook_event * const event, CGEventSourceRef src) {
    bool is_pressed;

    if (event->type == EVENT_KEY_PRESSED) {
        is_pressed = true;
        switch (event->data.keyboard.keycode) {
            case VC_SHIFT_L:
            case VC_SHIFT_R:
                current_modifier_mask |= kCGEventFlagMaskShift;
                break;

            case VC_CONTROL_L:
            case VC_CONTROL_R:
                current_modifier_mask |= kCGEventFlagMaskControl;
                break;

            case VC_META_L:
            case VC_META_R:
                current_modifier_mask |= kCGEventFlagMaskCommand;
                break;

            case VC_ALT_L:
            case VC_ALT_R:
                current_modifier_mask |= kCGEventFlagMaskAlternate;
                break;
        }
    } else if (event->type == EVENT_KEY_RELEASED) {
        is_pressed = false;
        switch (event->data.keyboard.keycode) {
            case VC_SHIFT_L:
            case VC_SHIFT_R:
                current_modifier_mask &= ~kCGEventFlagMaskShift;
                break;

            case VC_CONTROL_L:
            case VC_CONTROL_R:
                current_modifier_mask &= ~kCGEventFlagMaskControl;
                break;

            case VC_META_L:
            case VC_META_R:
                current_modifier_mask &= ~kCGEventFlagMaskCommand;
                break;

            case VC_ALT_L:
            case VC_ALT_R:
                current_modifier_mask &= ~kCGEventFlagMaskAlternate;
                break;
        }
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid event for keyboard post event: %#X.\n",
                __FUNCTION__, __LINE__, event->type);
        return UIOHOOK_FAILURE;
    }

    CGEventFlags event_mask = current_modifier_mask;
    switch (event->data.keyboard.keycode) {
        case VC_KP_0:
        case VC_KP_1:
        case VC_KP_2:
        case VC_KP_3:
        case VC_KP_4:
        case VC_KP_5:
        case VC_KP_6:
        case VC_KP_7:
        case VC_KP_8:
        case VC_KP_9:

        case VC_NUM_LOCK:
        case VC_KP_ENTER:
        case VC_KP_MULTIPLY:
        case VC_KP_ADD:
        case VC_KP_SEPARATOR:
        case VC_KP_SUBTRACT:
        case VC_KP_DIVIDE:
            event_mask |= kCGEventFlagMaskNumericPad;
            break;
    }

    CGKeyCode keycode = (CGKeyCode) vcode_to_keycode(event->data.keyboard.keycode);
    if (keycode == kVK_Undefined) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Unable to lookup scancode: %li\n",
                __FUNCTION__, __LINE__, event->data.keyboard.keycode);
        return UIOHOOK_FAILURE;
    }

    CGEventRef cg_event = CGEventCreateKeyboardEvent(
        src,
        keycode,
        is_pressed
    );

    if (cg_event == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CGEventCreateKeyboardEvent failed!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    CGEventSetFlags(cg_event, event_mask);

    CGEventPost(kCGHIDEventTap, cg_event); // kCGSessionEventTap also works.
    CFRelease(cg_event);

    return UIOHOOK_SUCCESS;
}

static int post_mouse_event(uiohook_event * const event, CGEventSourceRef src) {
    CGEventType type = kCGEventNull;
    CGMouseButton button = 0;

    switch (event->type) {
        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_PRESSED_IGNORE_COORDS:
            if (event->data.mouse.button == MOUSE_NOBUTTON) {
                // FIXME Warning
                return UIOHOOK_FAILURE;
            } else if (event->data.mouse.button == MOUSE_BUTTON1) {
                type = kCGEventLeftMouseDown;
                button = kCGMouseButtonLeft;
                current_motion_event = kCGEventLeftMouseDragged;
            } else if (event->data.mouse.button == MOUSE_BUTTON2) {
                type = kCGEventRightMouseDown;
                button = kCGMouseButtonRight;
                current_motion_event = kCGEventRightMouseDragged;
            } else {
                type = kCGEventOtherMouseDown;
                button = event->data.mouse.button - 1;
                current_motion_event = kCGEventOtherMouseDragged;
            }
            current_motion_button = button;
            break;

        case EVENT_MOUSE_RELEASED:
        case EVENT_MOUSE_RELEASED_IGNORE_COORDS:
            if (event->data.mouse.button == MOUSE_NOBUTTON) {
                // FIXME Warning
                return UIOHOOK_FAILURE;
            } else if (event->data.mouse.button == MOUSE_BUTTON1) {
                type = kCGEventLeftMouseUp;
                button = kCGMouseButtonLeft;
                if (current_motion_event == kCGEventLeftMouseDragged) {
                    current_motion_event = kCGEventMouseMoved;
                }
            } else if (event->data.mouse.button == MOUSE_BUTTON2) {
                type = kCGEventRightMouseUp;
                button = kCGMouseButtonRight;
                if (current_motion_event == kCGEventRightMouseDragged) {
                    current_motion_event = kCGEventMouseMoved;
                }
            } else {
                type = kCGEventOtherMouseUp;
                button = event->data.mouse.button - 1;
                if (current_motion_event == kCGEventOtherMouseDragged) {
                    current_motion_event = kCGEventMouseMoved;
                }
            }
            current_motion_button = button;
            break;

        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_DRAGGED:
        case EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR:
            type = current_motion_event;
            button = current_motion_button;
            break;

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid mouse event: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            return UIOHOOK_FAILURE;
    }

    CGPoint point;

    if (event->type == EVENT_MOUSE_PRESSED_IGNORE_COORDS || event->type == EVENT_MOUSE_RELEASED_IGNORE_COORDS) {
        CGEventRef null_event = CGEventCreate(NULL);
        point = CGEventGetLocation(null_event);
        CFRelease(null_event);
    } else if (event->type == EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR) {
        CGEventRef null_event = CGEventCreate(NULL);
        point = CGEventGetLocation(null_event);
        CFRelease(null_event);
        point.x += (CGFloat) event->data.mouse.x;
        point.y += (CGFloat) event->data.mouse.y;
    } else {
        point = CGPointMake(
                (CGFloat) event->data.mouse.x,
                (CGFloat) event->data.mouse.y
        );
    }

    CGEventRef cg_event = CGEventCreateMouseEvent(
            src,
            type,
            point,
            button
    );

    if (cg_event == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CGEventCreateMouseEvent failed!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    if (event->data.mouse.clicks != 0) {
        CGEventSetIntegerValueField(cg_event, kCGMouseEventClickState, event->data.mouse.clicks);
    }

    CGEventPost(kCGHIDEventTap, cg_event); // kCGSessionEventTap also works.
    CFRelease(cg_event);

    return UIOHOOK_SUCCESS;
}

static int post_mouse_wheel_event(uiohook_event * const event, CGEventSourceRef src) {
    CGScrollEventUnit scroll_unit;
    if (event->data.wheel.type == WHEEL_BLOCK_SCROLL) {
        // Scrolling data is line-based.
        scroll_unit = kCGScrollEventUnitLine;
    } else {
        // Scrolling data is pixel-based.
        scroll_unit = kCGScrollEventUnitPixel;
    }

    int32_t wheel1 = 0;
    int32_t wheel2 = 0;

    if (event->data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION) {
        wheel2 = event->data.wheel.rotation;
    } else {
        wheel1 = event->data.wheel.rotation;
    }

    CGEventRef cg_event = CGEventCreateScrollWheelEvent(
        src,
        scroll_unit,
        (CGWheelCount) 2, // 1 for Y-only, 2 for Y-X, 3 for Y-X-Z
        wheel1, // TODO Is this value correct? Do we need PPL?
        wheel2
    );

    if (cg_event == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CGEventCreateScrollWheelEvent failed!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    CGEventPost(kCGHIDEventTap, cg_event); // kCGSessionEventTap also works.
    CFRelease(cg_event);

    return UIOHOOK_SUCCESS;
}

UIOHOOK_API int hook_post_event(uiohook_event * const event) {
    CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (src == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CGEventSourceCreate failed!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    int status = UIOHOOK_FAILURE;
    switch (event->type) {
        case EVENT_KEY_PRESSED:
        case EVENT_KEY_RELEASED:
            status = post_key_event(event, src);
            break;

        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_RELEASED:
        case EVENT_MOUSE_PRESSED_IGNORE_COORDS:
        case EVENT_MOUSE_RELEASED_IGNORE_COORDS:

        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_DRAGGED:
        case EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR:
            status = post_mouse_event(event, src);
            break;

        case EVENT_MOUSE_WHEEL:
            status = post_mouse_wheel_event(event, src);
            break;

        case EVENT_KEY_TYPED:
        case EVENT_MOUSE_CLICKED:

        case EVENT_HOOK_ENABLED:
        case EVENT_HOOK_DISABLED:

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Ignoring post event: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            status = UIOHOOK_FAILURE;
    }

    CFRelease(src);

    return status;
}

UIOHOOK_API int hook_post_text(const uint16_t * const text) {
    if (text == NULL) {
        return UIOHOOK_ERROR_POST_TEXT_NULL;
    }

    CGEventRef downEvent = CGEventCreateKeyboardEvent(NULL, 0, true);
    CGEventRef upEvent = CGEventCreateKeyboardEvent(NULL, 0, false);

    UniCharCount count = 0;

    for (int i = 0; text[i] != 0; i++) {
        count++;
    }

    CGEventKeyboardSetUnicodeString(downEvent, count, (UniChar*)text);
    CGEventKeyboardSetUnicodeString(upEvent, count, (UniChar*)text);

    CGEventPost(kCGSessionEventTap, downEvent);
    CGEventPost(kCGSessionEventTap, upEvent);

    CFRelease(downEvent);
    CFRelease(upEvent);
}
