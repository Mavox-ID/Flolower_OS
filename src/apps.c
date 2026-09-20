#include "shell.h"
#include <string.h>
#include <stdio.h>
#ifdef HAVE_FS
#include <stdlib.h>
#endif

void app_launch(ShellState *s, const char *key, const char *display_title) {
    if (strcmp(key, "calculator") == 0) {
        app_open_calculator(s);
        return;
    }
    if (strcmp(key, "paint") == 0) {
        app_open_paint(s);
        return;
    }
    if (strcmp(key, "calendar") == 0) {
        app_open_calendar(s);
        return;
    }
    if (strcmp(key, "task_manager") == 0) {
        app_open_taskmanager(s);
        return;
    }
    if (strcmp(key, "timer") == 0) {
        app_open_timer(s);
        return;
    }
    if (strcmp(key, "settings") == 0) {
        app_open_settings(s);
        return;
    }
    if (strcmp(key, "smile_game") == 0) {
        app_open_smile_game(s);
        return;
    }
#ifdef HAVE_FS
    if (strcmp(key, "terminal") == 0) {
        app_open_terminal(s);
        return;
    }
    if (strcmp(key, "files") == 0) {
        app_open_files(s);
        return;
    }
    if (strcmp(key, "text_editor") == 0) {
        char path[600];
        const char *home = getenv("HOME");
        snprintf(path, sizeof(path), "%s/Untitled.txt", home ? home : ".");
        app_open_text_editor(s, path);
        return;
    }
    if (strcmp(key, "browser") == 0) {
        app_open_browser(s);
        return;
    }
    if (strcmp(key, "notes") == 0) {
        app_open_notes(s);
        return;
    }
    if (strcmp(key, "trash") == 0) {
        app_open_trash(s);
        return;
    }
#else
    if (strcmp(key, "terminal") == 0 || strcmp(key, "files") == 0 || strcmp(key, "text_editor") == 0 ||
        strcmp(key, "browser") == 0 || strcmp(key, "notes") == 0 || strcmp(key, "trash") == 0) {
        notify_show(s, "Needs a filesystem driver", 2200);
        return;
    }
#endif

    AppWindow *w = wm_create_window(s, display_title, 0.5f, 0.55f);
    if (!w) notify_show(s, "Too many windows open", 2000);
}
