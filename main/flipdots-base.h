#ifndef FLIPDOTS_BASE_H
#define FLIPDOTS_BASE_H

// Dimensions for a single flip board.
#define FLIP_BOARD_HEIGHT                14
#define FLIP_BOARD_WIDTH                 28

// Dimensions of entire display space (2 boards)
#define DISPLAY_HEIGHT                   (FLIP_BOARD_HEIGHT*2)
#define DISPLAY_WIDTH                    FLIP_BOARD_WIDTH

#define DISPLAY_PIXELS                   (DISPLAY_HEIGHT*DISPLAY_WIDTH)

#define DISPLAY_ROTATION_0     0
#define DISPLAY_ROTATION_90    1
#define DISPLAY_ROTATION_180   2
#define DISPLAY_ROTATION_270   3
#define DISPLAY_ROTATION       DISPLAY_ROTATION_0

#endif
