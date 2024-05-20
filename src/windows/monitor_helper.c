#include "monitor_helper.h"
#include "logger.h"

static bool monitors_enumerated = false;
static bool always_enumerate_displays = true;
static LONG left = 0;
static LONG top = 0;

static BOOL CALLBACK enum_monitor_proc(HMONITOR h_monitor, HDC hdc, LPRECT lp_rect, LPARAM dwData) {
    MONITORINFO MonitorInfo = {0};
    MonitorInfo.cbSize = sizeof(MonitorInfo);

    if (GetMonitorInfo(h_monitor, &MonitorInfo)) {
        if (MonitorInfo.rcMonitor.left < left) {
            left = MonitorInfo.rcMonitor.left;
        }

        if (MonitorInfo.rcMonitor.top < top) {
            top = MonitorInfo.rcMonitor.top;
        }
    }

    return TRUE;
}

void enumerate_displays() {
    // Reset coordinates because if a negative monitor moves to positive space,
    // it will still look like there is some monitor in negative space.
    left = 0;
    top = 0;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Enumerating displays\n",
            __FUNCTION__, __LINE__);

    EnumDisplayMonitors(NULL, NULL, enum_monitor_proc, 0);

    monitors_enumerated = true;
}

void set_always_enumerate_displays(bool always) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Setting always_enumerate_displays to %i\n",
            __FUNCTION__, __LINE__, always);

    always_enumerate_displays = always;
}

largest_negative_coordinates get_largest_negative_coordinates() {
    if (!monitors_enumerated || always_enumerate_displays) {
        enumerate_displays();
    }

    largest_negative_coordinates lnc = {
            .left = left,
            .top = top
    };

    return lnc;
}
