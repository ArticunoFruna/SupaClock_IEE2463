#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#define USE_SDL 1

#if USE_SDL
#  define SDL_HOR_RES     240
#  define SDL_VER_RES     280
#  define SDL_ZOOM        1
#  define SDL_INCLUDE_PATH <SDL2/SDL.h>
#endif

#endif /* LV_DRV_CONF_H */
