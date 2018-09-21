#ifndef MODE_SNAKE_H
#define MODE_SNAKE_H

#include "mode-base.h"

#define SNAKE_TIME_DELAY_BETWEEN_DRAWS_MS       (1*200)
#define SNAKE_MAX_LEN                           (DISPLAY_WIDTH*DISPLAY_HEIGHT)

typedef struct SnakeCoords {
  int x, y;
} SnakeCoords;

#define SNAKE_DIRECTION_MIN   0
typedef enum {
  SNAKE_NORTH = SNAKE_DIRECTION_MIN,
  SNAKE_EAST,
  SNAKE_SOUTH,
  SNAKE_WEST,
} SnakeDirection;
#define SNAKE_DIRECTION_MAX   SNAKE_WEST

typedef struct {
  SnakeCoords nodes[SNAKE_MAX_LEN];
  SnakeDirection moving_direction;

  int snake_tail_index;
  int snake_head_index;
  int nodes_to_grow;
} Snake;

typedef struct {
  Snake snake;
  SnakeCoords food;
} ModeSnakeState;

void mode_snake_setup();
int mode_snake_draw();
bool mode_snake_direction_input(const SnakeDirection input);
void mode_snake_reset_game();

#endif
