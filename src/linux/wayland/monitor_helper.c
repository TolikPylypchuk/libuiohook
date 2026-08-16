#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>

#include <logger.h>
#include <uiohook.h>

#include "monitor_helper.h"
#include "wayland-xdg-output-unstable-v1-client-protocol.h"

#define WL_OUTPUT_DONE_VERSION      2
#define XDG_OUTPUT_MANAGER_VERSION  3

typedef struct _monitor {
    uint32_t name;
    uint32_t version;

    struct wl_output *output;
    struct zxdg_output_v1 *xdg_output;

    // Logical geometry from xdg-output, accumulated until the compositor commits it.
    int32_t pending_x, pending_y;
    int32_t pending_width, pending_height;
    bool has_pending_position;
    bool has_pending_size;

    // Device geometry from wl_output, which is only used when xdg-output is unavailable.
    int32_t output_x, output_y;
    int32_t output_width, output_height;
    int32_t output_scale;
    bool has_output_position;
    bool has_output_size;

    // The committed geometry which the published layout is built from.
    int32_t x, y, width, height;
    bool resolved;

    struct _monitor *next;
} monitor;

static monitor *monitors = NULL;
static struct zxdg_output_manager_v1 *output_manager = NULL;
static uint32_t output_manager_name = 0;

static pthread_mutex_t layout_mutex = PTHREAD_MUTEX_INITIALIZER;
static screen_data *layout = NULL;
static uint8_t layout_count = 0;
static uint16_t layout_width = 0;
static uint16_t layout_height = 0;

static bool fallback_geometry_logged = false;
static bool out_of_range_logged = false;

static int16_t clamp_to_int16(int32_t value) {
    if (value < INT16_MIN || value > INT16_MAX) {
        if (!out_of_range_logged) {
            logger(LOG_LEVEL_WARN, "%s [%u]: The screen layout doesn't fit into the event coordinates!\n",
                    __FUNCTION__, __LINE__);

            out_of_range_logged = true;
        }

        return value < INT16_MIN ? INT16_MIN : INT16_MAX;
    }

    return (int16_t) value;
}

static uint16_t clamp_to_uint16(int32_t value) {
    if (value < 0 || value > UINT16_MAX) {
        if (!out_of_range_logged) {
            logger(LOG_LEVEL_WARN, "%s [%u]: The screen layout doesn't fit into the event coordinates!\n",
                    __FUNCTION__, __LINE__);

            out_of_range_logged = true;
        }

        return value < 0 ? 0 : UINT16_MAX;
    }

    return (uint16_t) value;
}

static int compare_screens(const void *left, const void *right) {
    const screen_data *first = left;
    const screen_data *second = right;

    if (first->y != second->y) {
        return first->y < second->y ? -1 : 1;
    }

    if (first->x != second->x) {
        return first->x < second->x ? -1 : 1;
    }

    return 0;
}

static void publish_layout() {
    unsigned int resolved_count = 0;
    for (monitor *current = monitors; current != NULL; current = current->next) {
        if (current->resolved) {
            resolved_count++;
        }
    }

    if (resolved_count > UINT8_MAX) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Screen count overflow detected!\n",
                __FUNCTION__, __LINE__);

        resolved_count = UINT8_MAX;
    }

    screen_data *new_layout = NULL;
    uint8_t new_count = 0;
    uint16_t new_width = 0;
    uint16_t new_height = 0;

    if (resolved_count > 0) {
        new_layout = malloc(sizeof(screen_data) * resolved_count);

        if (new_layout == NULL) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for the screen layout!\n",
                    __FUNCTION__, __LINE__);

            return;
        }

        int32_t min_x = INT32_MAX, min_y = INT32_MAX, max_x = INT32_MIN, max_y = INT32_MIN;

        for (monitor *current = monitors; current != NULL && new_count < resolved_count; current = current->next) {
            if (!current->resolved) {
                continue;
            }

            new_layout[new_count] = (screen_data) {
                .number = 0,
                .x = clamp_to_int16(current->x),
                .y = clamp_to_int16(current->y),
                .width = clamp_to_uint16(current->width),
                .height = clamp_to_uint16(current->height)
            };

            new_count++;

            if (current->x < min_x) {
                min_x = current->x;
            }

            if (current->y < min_y) {
                min_y = current->y;
            }

            if (current->x + current->width > max_x) {
                max_x = current->x + current->width;
            }

            if (current->y + current->height > max_y) {
                max_y = current->y + current->height;
            }
        }

        // The registry announces globals in no particular order, so the layout is numbered by
        // position instead, which is the same on every run.
        qsort(new_layout, new_count, sizeof(screen_data), compare_screens);

        for (uint8_t i = 0; i < new_count; i++) {
            new_layout[i].number = i + 1;
        }

        new_width = clamp_to_uint16(max_x - min_x);
        new_height = clamp_to_uint16(max_y - min_y);
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Resolved %u screen(s) over %u x %u.\n",
            __FUNCTION__, __LINE__, new_count, new_width, new_height);

    pthread_mutex_lock(&layout_mutex);

    free(layout);

    layout = new_layout;
    layout_count = new_count;
    layout_width = new_width;
    layout_height = new_height;

    pthread_mutex_unlock(&layout_mutex);
}

// Applies whichever geometry the compositor has provided for a monitor.
// xdg-output is preferred because it reports logical coordinates.
static void commit_monitor(monitor *current) {
    if (current->has_pending_position && current->has_pending_size) {
        current->x = current->pending_x;
        current->y = current->pending_y;
        current->width = current->pending_width;
        current->height = current->pending_height;
        current->resolved = true;
    } else if (output_manager == NULL && current->has_output_position && current->has_output_size) {
        int32_t scale = current->output_scale > 0 ? current->output_scale : 1;

        if (!fallback_geometry_logged) {
            logger(LOG_LEVEL_WARN, "%s [%u]: The compositor doesn't support xdg-output, so the screen "
                    "layout is approximated and will be wrong under fractional scaling.\n",
                    __FUNCTION__, __LINE__);

            fallback_geometry_logged = true;
        }

        current->x = current->output_x;
        current->y = current->output_y;
        current->width = current->output_width / scale;
        current->height = current->output_height / scale;
        current->resolved = true;
    } else {
        return;
    }

    publish_layout();
}

static void xdg_output_logical_position(void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y) {
    monitor *current = data;

    current->pending_x = x;
    current->pending_y = y;
    current->has_pending_position = true;
}

static void xdg_output_logical_size(void *data, struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height) {
    monitor *current = data;

    current->pending_width = width;
    current->pending_height = height;
    current->has_pending_size = true;
}

static void xdg_output_done(void *data, struct zxdg_output_v1 *xdg_output) {
    // Deprecated since version 3, which commits on wl_output.done instead, so both are handled.
    commit_monitor((monitor *) data);
}

static void xdg_output_name(void *data, struct zxdg_output_v1 *xdg_output, const char *name) {
}

static void xdg_output_description(void *data, struct zxdg_output_v1 *xdg_output, const char *description) {
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = xdg_output_logical_position,
    .logical_size = xdg_output_logical_size,
    .done = xdg_output_done,
    .name = xdg_output_name,
    .description = xdg_output_description
};

static void output_geometry(
        void *data,
        struct wl_output *output,
        int32_t x,
        int32_t y,
        int32_t physical_width,
        int32_t physical_height,
        int32_t subpixel,
        const char *make,
        const char *model,
        int32_t transform) {
    monitor *current = data;

    current->output_x = x;
    current->output_y = y;
    current->has_output_position = true;

    // Version 1 never reports done, so there is nothing to wait for.
    if (current->version < WL_OUTPUT_DONE_VERSION) {
        commit_monitor(current);
    }
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    monitor *current = data;

    if (!(flags & WL_OUTPUT_MODE_CURRENT)) {
        return;
    }

    current->output_width = width;
    current->output_height = height;
    current->has_output_size = true;

    if (current->version < WL_OUTPUT_DONE_VERSION) {
        commit_monitor(current);
    }
}

static void output_done(void *data, struct wl_output *output) {
    commit_monitor((monitor *) data);
}

static void output_scale(void *data, struct wl_output *output, int32_t factor) {
    ((monitor *) data)->output_scale = factor;
}

static void output_name(void *data, struct wl_output *output, const char *name) {
}

static void output_description(void *data, struct wl_output *output, const char *description) {
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description
};

static void bind_xdg_output(monitor *current) {
    if (output_manager == NULL || current->xdg_output != NULL) {
        return;
    }

    current->xdg_output = zxdg_output_manager_v1_get_xdg_output(output_manager, current->output);
    if (current->xdg_output != NULL) {
        zxdg_output_v1_add_listener(current->xdg_output, &xdg_output_listener, current);
    }
}

static void destroy_monitor(monitor *current) {
    if (current->xdg_output != NULL) {
        zxdg_output_v1_destroy(current->xdg_output);
    }

    if (current->output != NULL) {
        wl_output_destroy(current->output);
    }

    free(current);
}

void monitor_helper_add_output(struct wl_registry *registry, uint32_t name, uint32_t version) {
    monitor *current = calloc(1, sizeof(monitor));
    if (current == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for a monitor!\n",
                __FUNCTION__, __LINE__);

        return;
    }

    current->name = name;
    current->version = version < WL_OUTPUT_DONE_VERSION ? version : WL_OUTPUT_DONE_VERSION;
    current->output_scale = 1;

    current->output = wl_registry_bind(registry, name, &wl_output_interface, current->version);
    if (current->output == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to bind an output!\n",
                __FUNCTION__, __LINE__);

        free(current);
        return;
    }

    wl_output_add_listener(current->output, &output_listener, current);

    current->next = monitors;
    monitors = current;

    // The manager is already known when this is a hotplugged output rather than an initial one.
    bind_xdg_output(current);
}

void monitor_helper_add_output_manager(struct wl_registry *registry, uint32_t name, uint32_t version) {
    if (output_manager != NULL) {
        return;
    }

    uint32_t bind_version = version < XDG_OUTPUT_MANAGER_VERSION ? version : XDG_OUTPUT_MANAGER_VERSION;

    output_manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, bind_version);
    if (output_manager == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to bind the xdg-output manager!\n",
                __FUNCTION__, __LINE__);

        return;
    }

    output_manager_name = name;
}

void monitor_helper_remove_global(uint32_t name) {
    if (output_manager != NULL && name == output_manager_name) {
        zxdg_output_manager_v1_destroy(output_manager);
        output_manager = NULL;

        return;
    }

    monitor **link = &monitors;

    while (*link != NULL) {
        monitor *current = *link;

        if (current->name == name) {
            *link = current->next;
            destroy_monitor(current);
            publish_layout();

            return;
        }

        link = &current->next;
    }
}

void monitor_helper_bind_xdg_outputs() {
    for (monitor *current = monitors; current != NULL; current = current->next) {
        bind_xdg_output(current);
    }
}

void monitor_helper_destroy() {
    monitor *current = monitors;

    while (current != NULL) {
        monitor *next = current->next;
        destroy_monitor(current);
        current = next;
    }

    monitors = NULL;

    if (output_manager != NULL) {
        zxdg_output_manager_v1_destroy(output_manager);
        output_manager = NULL;
    }

    pthread_mutex_lock(&layout_mutex);

    free(layout);

    layout = NULL;
    layout_count = 0;
    layout_width = 0;
    layout_height = 0;

    pthread_mutex_unlock(&layout_mutex);
}

screen_data *monitor_helper_create_screen_info(unsigned char *count) {
    *count = 0;
    screen_data *result = NULL;

    pthread_mutex_lock(&layout_mutex);

    if (layout_count > 0) {
        result = malloc(sizeof(screen_data) * layout_count);

        if (result != NULL) {
            memcpy(result, layout, sizeof(screen_data) * layout_count);
            *count = layout_count;
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for the screen information!\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: The screen layout is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    pthread_mutex_unlock(&layout_mutex);

    return result;
}

bool monitor_helper_get_desktop_bounds(uint16_t *width, uint16_t *height) {
    pthread_mutex_lock(&layout_mutex);

    bool available = layout_count > 0;
    if (available) {
        *width = layout_width;
        *height = layout_height;
    }

    pthread_mutex_unlock(&layout_mutex);

    return available;
}
