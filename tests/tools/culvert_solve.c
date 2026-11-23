/*
 * Prints the turns that solve a Culvert level, from the generator in
 * holotapes/culvert/logic.c. The QEMU tests run it to play a level.
 *
 *   culvert_solve <level>
 *
 * Output: "size w h sx sy", then one "turn x y n" line per tile that
 * needs n clockwise quarter turns, in row order.
 */
#include <stdio.h>
#include <stdlib.h>

#include "logic.h"

int main(int argc, char **argv)
{
    static cv_level_t lv;
    long level;
    int x, y;

    if (argc != 2 || (level = strtol(argv[1], NULL, 10)) < 1 || level > (long)CV_LEVEL_MAX) {
        fprintf(stderr, "usage: culvert_solve <level 1 to %u>\n", CV_LEVEL_MAX);
        return 2;
    }
    cv_generate(&lv, (uint32_t)level);
    printf("size %d %d %d %d\n", lv.w, lv.h, lv.sx, lv.sy);
    for (y = 0; y < lv.h; y++) {
        for (x = 0; x < lv.w; x++) {
            int n = cv_turns_to(lv.mask[y * lv.w + x], lv.goal[y * lv.w + x]);

            if (n > 0) {
                printf("turn %d %d %d\n", x, y, n);
            }
        }
    }
    return 0;
}
