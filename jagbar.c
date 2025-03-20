#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <regex.h>

#define BAR_HEIGHT 20
#define REFRESH_INTERVAL 1
#define DEFAULT_REFRESH 1
#define DEFAULT_HEIGHT 20
#define MAX_LINE 256

typedef struct {
    int height;
    int width;
    int x;
    int y;
    int refresh;
    unsigned long bgcolor;
    unsigned long fgcolor;
    int corner_radius;
} Config;

// jagbar.conf
int load_config(const char *filename, Config *cfg) {
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		perror("Cannot open config file");
		return -1;
	}

	// config defaults
	cfg->height = DEFAULT_HEIGHT;
	cfg->width = 0;
	cfg->x = 0;
	cfg->y = 0;
	cfg->refresh = DEFAULT_REFRESH;
	cfg->bgcolor = 0xFFFFFF;
	cfg->fgcolor = 0x000000;
	cfg->corner_radius = 10;

	// regex for key value pattern matching
	regex_t regex; 
	int ret = regcomp(&regex, "^\\s*([^=]+)=([^#\\n]*)", REG_EXTENDED);
	if (ret) {
		fprintf(stderr, "Could not compile regex\n");
		fclose(fp);
		return -1;
	}
	regmatch_t matches[3];

	char line[MAX_LINE];
	int line_num = 0;
	while(fgets(line, sizeof(line), fp)) {
		line_num++;
		line[strcspn(line, "\n")] = 0;


		if(regexec(&regex, line, 3, matches, 0) == 0) {
			char key[MAX_LINE] = {0};
			char value[MAX_LINE] = {0};

			int key_len = matches[1].rm_eo - matches[1].rm_so;
			int val_len = matches[2].rm_eo - matches[2].rm_so;
			strncpy(key, line + matches[1].rm_so, key_len);
			strncpy(value, line + matches[2].rm_so, val_len);

			if (key[0] == '#') continue;

			if (strcmp(key, "height") == 0) cfg->height = atoi(value);
			else if (strcmp(key, "width") == 0) cfg->width = atoi(value);
			else if (strcmp(key, "x") == 0) cfg->x = atoi(value);
			else if (strcmp(key, "y") == 0) cfg->y = atoi(value);
			else if (strcmp(key, "refresh") == 0) cfg->y = atoi(value);
			else if (strcmp(key, "bgcolor") == 0) cfg->bgcolor = strtoul(value, NULL, 16);
			else if (strcmp(key, "fgcolor") == 0) cfg->fgcolor = strtoul(value, NULL, 16);
			else if (strcmp(key, "corner_radius") == 0) cfg->fgcolor = atoi(value);
			else if (strcmp(key, "text_offset") == 0) cfg->fgcolor = atoi(value);	
			else fprintf(stderr, "Unknown key '%s' at line %d of jagbar.conf\n", key, line_num);
		} else if (line[0] && line[0] != '#') {
			fprintf(stderr, "Malformed line %d: %s in jagbar.conf\n", line_num, line);
		}
	}

	regfree(&regex);
	fclose(fp);
	return 0;
}			

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

int main() {
	Config cfg;
	load_config("jagbar.conf", &cfg);
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
