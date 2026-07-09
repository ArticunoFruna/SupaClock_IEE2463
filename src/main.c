// Stub principal — los includes pesados sólo se traen cuando el env
// correspondiente lo activa, para que envs livianos (p.ej. capture_c3)
// no necesiten LVGL ni los drivers del display.
#if defined(ENV_MAIN_APP) || defined(ENV_MAIN_NOTOUCH)
#include "supaclock_app.h"

void app_main(void) {
    supaclock_app_run();
}
#endif
