#include "screens/screens.h"

const dx_screen_t *const dx_rom_screens[] = {
    &dx_screen_stat, &dx_screen_inv, &dx_screen_data, &dx_screen_map, &dx_screen_radio,
};

const int dx_rom_screen_count = sizeof(dx_rom_screens) / sizeof(dx_rom_screens[0]);
