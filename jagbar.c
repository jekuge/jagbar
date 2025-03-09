#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BAR_HEIGHT 20
#define REFRESH_INTERVAL 1
#define DEFAULT_REFRESH 1
#define DEFAULT_HEIGHT 20
#define MAX_LINE 256

typedef struct {
    int height;
    int width;
    int refresh;
    unsigned long bgcolor;
    unsigned long fgcolor;
} Config;

float get_cpu_usage() {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    char line[256];
    fgets(line, sizeof(line), fp);
    long user, nice, system, idle;
    sscanf(line, "cpu %ld %ld %ld %ld", &user, &nice, &system, &idle);
    float total = user + nice + system + idle;
    fclose(fp);
    return (total - idle) / total * 100.0;
}

float get_mem_usage() {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return 0.0;
    long mem_total, mem_free;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &mem_total)) continue;
        if (sscanf(line, "MemFree: %ld kB", &mem_free)) break;
    } 
    fclose(fp);
    return (float)(mem_total - mem_free) / mem_total * 100.0;
}

int main()
{
    Display *dpy;
    Window win;
    XEvent ev;
    GC gc; // graphics context for colors
    int screen;

    // open connection to X server
    printf("starting jagbar\n");
    dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    screen = DefaultScreen(dpy);
    int screen_width = DisplayWidth(dpy, screen);

    // create window for status bar
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
        0, 0, screen_width, BAR_HEIGHT, 1, BlackPixel(dpy, screen), WhitePixel(dpy, screen));
    
    // set window type to _NET_WM_WINDOW_TYPE_DOCK
    Atom wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom dock_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, win, wm_window_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&dock_type, 1);

    // set window state to stay above other windows
    Atom wm_state = XInternAtom(dpy, "_NTE_WM_STATE", False);
    Atom state_above = XInternAtom(dpy, "_NEt_WM_STATE_ABOVE", False);
    XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&state_above, //
                    1);

    // 

    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    XMapWindow(dpy, win);
    // main loop
    char status[256];
    while (1) {
        // get current time
        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0';

        // format status text
        float cpu = get_cpu_usage();
        float mem = get_mem_usage();
        snprintf(status, sizeof(status), "Status: %s | CPU: %.1f%% | Mem: %.1f%%", time_str, cpu, mem);

        // draw text
        XClearWindow(dpy, win);
        XDrawString(dpy, win, DefaultGC(dpy, screen), 10, 15, status, strlen(status));

        // flush changes to X server
        XFlush(dpy);

        // sleep until next update
        sleep(REFRESH_INTERVAL);
    }

    // clean up
    XCloseDisplay(dpy);
    return 0;
}