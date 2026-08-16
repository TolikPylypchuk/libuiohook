#ifndef MONITOR_HELPER_H
#define MONITOR_HELPER_H

#include <stdbool.h>
#include <stdint.h>

#include <wayland-client.h>

#include <uiohook.h>

/* Binds a wl_output which the registry has just announced. */
void monitor_helper_add_output(struct wl_registry *registry, uint32_t name, uint32_t version);

/* Binds the xdg-output manager which the registry has just announced. */
void monitor_helper_add_output_manager(struct wl_registry *registry, uint32_t name, uint32_t version);

/* Drops the output or the manager which the registry has just removed, if it's one of them. */
void monitor_helper_remove_global(uint32_t name);

/* Requests an xdg-output for every output which doesn't have one yet. */
void monitor_helper_bind_xdg_outputs();

/* Destroys every proxy and forgets the layout. */
void monitor_helper_destroy();

/* Copies the current layout for the caller, who takes ownership of it. */
screen_data *monitor_helper_create_screen_info(unsigned char *count);

/* Gets the size of the bounding box of every output. Returns false if no layout is known yet. */
bool monitor_helper_get_desktop_bounds(uint16_t *width, uint16_t *height);

#endif
