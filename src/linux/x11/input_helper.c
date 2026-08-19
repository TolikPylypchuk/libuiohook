#include <stdbool.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "input_helper.h"
#include "logger.h"

Display *helper_disp;  // Where do we open this display?  FIXME Use the ctrl display via init param

size_t event_to_unicode(XKeyEvent *x_event, XIC xic, uint16_t *surrogate, size_t length) {
    size_t count = 0;
    char buffer[5] = {};

    if (xic != NULL) {
        count = Xutf8LookupString(xic, x_event, buffer, sizeof(buffer), NULL, NULL);
    } else {
        count = XLookupString(x_event, buffer, sizeof(buffer), NULL, NULL);
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

            if (count >= sizeof(utf8_bitmask_table)) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Cannot convert %zu bytes to a single character!\n",
                        __FUNCTION__, __LINE__, count);

                return 0;
            }

            uint32_t codepoint = utf8_bitmask_table[count] & buffer[0];
            for (unsigned int i = 1; i < count; i++) {
                codepoint = (codepoint << 6) | (utf8_bitmask_table[0] & buffer[i]);
            }

            if (codepoint <= 0xFFFF) {
                count = 1;
                surrogate[0] = (uint16_t) codepoint;
            } else if (length > 1) {
                // if codepoint > 0xFFFF, split into lead (high) / trail (low) surrogate ranges
                // See https://unicode.org/faq/utf_bom.html#utf16-4
                const uint32_t lead_offset = 0xD800 - (0x10000 >> 10);

                count = 2;
                surrogate[0] = (uint16_t) (lead_offset + (codepoint >> 10)); // lead,  first  [0]
                surrogate[1] = (uint16_t) (0xDC00 + (codepoint & 0x3FF));    // trail, second [1]
            } else {
                count = 0;
                logger(LOG_LEVEL_WARN, "%s [%u]: Surrogate buffer overflow detected!\n",
                        __FUNCTION__, __LINE__);
            }
        }

    }

    return count;
}
