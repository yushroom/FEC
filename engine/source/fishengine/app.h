#ifndef APP_H
#define APP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_init();
int app_reload();
int app_reload2();
int app_update();
int app_render_ui();

void app_play();
void app_stop();
void app_pause();
void app_step();
bool app_is_playing();
bool app_is_paused();

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
