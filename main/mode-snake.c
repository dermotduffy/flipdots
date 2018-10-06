#include <math.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "displaybuffer.h"
#include "displaydriver.h"
#include "mode-snake.h"

#define SNAKE_START_X           5
#define SNAKE_START_Y           5
#define SNAKE_START_LEN         3
#define SNAKE_FOOD_PROBABILITY  0.1
#define SNAKE_FOOD_GROWTH       2
#define SNAKE_FLASH_COUNT       6

const static char *LOG_TAG = "mode-snake";

static displaybuffer_t buffer_snake;

ModeSnakeState mode_snake_state;

void mode_snake_reset_game() {
  mode_snake_state.snake.snake_head_index = -1;
  mode_snake_state.snake.snake_tail_index = -1;
  mode_snake_state.snake.nodes_to_grow = 0;
  mode_snake_state.snake.moving_direction = SNAKE_EAST;
  mode_snake_state.snake.flash_countdown = -1;

  mode_snake_state.food.x = -1;
  mode_snake_state.food.y = -1;

  buffer_wipe(&buffer_snake);
}

void mode_snake_setup() {
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_snake);
  mode_snake_reset_game();
}

void create_starter_snake() {
  for (int i = 0; i < SNAKE_START_LEN; ++i) {
    mode_snake_state.snake.nodes[i].x = SNAKE_START_X + i;
    mode_snake_state.snake.nodes[i].y = SNAKE_START_Y;
  }
 
  mode_snake_state.snake.snake_tail_index = 0;
  mode_snake_state.snake.snake_head_index = SNAKE_START_LEN - 1;
  mode_snake_state.snake.moving_direction = SNAKE_EAST;
}

int get_snake_length() {
  if (mode_snake_state.snake.snake_tail_index < 0 ||
      mode_snake_state.snake.snake_head_index < 0) {
    return 0;
  }
  if (mode_snake_state.snake.snake_head_index > mode_snake_state.snake.snake_tail_index) {
    return mode_snake_state.snake.snake_head_index - mode_snake_state.snake.snake_tail_index + 1;
  } else if (mode_snake_state.snake.snake_head_index < mode_snake_state.snake.snake_tail_index) {
    return (SNAKE_MAX_LEN - mode_snake_state.snake.snake_tail_index) + mode_snake_state.snake.snake_head_index + 1;
  } else {
    return 1;
  }
}

void draw_food_to_displaybuffer(displaybuffer_t* buffer) {
  SnakeCoords food = mode_snake_state.food;
  if (food.x >= 0 && food.y >= 0) {
    buffer_draw_pixel(food.x, food.y, PIXEL_YELLOW, buffer);
  }
}

void draw_snake_to_displaybuffer(displaybuffer_t* buffer) {
  if (mode_snake_state.snake.snake_tail_index < 0 ||
      mode_snake_state.snake.snake_head_index < 0) {  
    return;
  }

  int i = mode_snake_state.snake.snake_tail_index;
  while (true) {
    buffer_draw_pixel(mode_snake_state.snake.nodes[i].x,
        mode_snake_state.snake.nodes[i].y, PIXEL_YELLOW, buffer);

    if (i == mode_snake_state.snake.snake_head_index) {
      break;
    }
    i = (i + 1) % SNAKE_MAX_LEN;
  } 
}

// Must not be called on a full board (i.e. there must be
// at least 1 empty space).
void add_food() {

  // Choose a random (x, y) and scan from there to the nearest free spot,
  // wrapping if necessary.
  int x_rand = esp_random() % DISPLAY_WIDTH;
  int y_rand = esp_random() % DISPLAY_HEIGHT;

  while (true) {
    if (buffer_get_pixel(x_rand, y_rand, &buffer_snake) == PIXEL_BLACK) {
      mode_snake_state.food.x = x_rand;
      mode_snake_state.food.y = y_rand;
      return;
    } else {
      x_rand += 1;
      if (x_rand == DISPLAY_WIDTH) {
        x_rand = 0;
        y_rand += 1;
      }
      if (y_rand == DISPLAY_HEIGHT) {
        y_rand = 0;
      }
    }
  }
}

int mode_snake_draw() {
  Snake* snake = &mode_snake_state.snake;

  // Flash the screen on win/lose a set number of times.
  if (snake->flash_countdown > 0) {
    buffer_inverse(&buffer_draw);
    buffer_commit_drawing();
    snake->flash_countdown--;
    return SNAKE_TIME_DELAY_BETWEEN_DRAWS_MS;
  } else if (snake->flash_countdown == 0) {
    snake->flash_countdown = -1;
    return 0;
  }

  if (snake->snake_tail_index < 0 || snake->snake_head_index < 0) {
    create_starter_snake();
    draw_snake_to_displaybuffer(&buffer_draw);
    buffer_commit_drawing();
    return SNAKE_TIME_DELAY_BETWEEN_DRAWS_MS;
  }

  if (get_snake_length() == SNAKE_MAX_LEN) {
    ESP_LOGI(LOG_TAG, "Snake win!");
    snake->flash_countdown = SNAKE_FLASH_COUNT;
    return SNAKE_TIME_DELAY_BETWEEN_DRAWS_MS;
  }

  struct SnakeCoords new_head = snake->nodes[snake->snake_head_index];

  switch (mode_snake_state.snake.moving_direction) {
    case SNAKE_NORTH:
      new_head.y -= 1;
      break;
    case SNAKE_EAST:
      new_head.x += 1;
      break;
    case SNAKE_SOUTH:
      new_head.y += 1;
      break;
    case SNAKE_WEST:
      new_head.x -= 1;
      break;
  }

  if (new_head.x < 0 || new_head.x >= DISPLAY_WIDTH ||
      new_head.y < 0 || new_head.y >= DISPLAY_HEIGHT) {
    ESP_LOGI(LOG_TAG, "Snake dead via wall collison :-(");
    snake->flash_countdown = SNAKE_FLASH_COUNT;
    return SNAKE_TIME_DELAY_BETWEEN_DRAWS_MS;
  }

  // This section of the code may refer to buffer_snake, which
  // contains the *previous* frame -- this is used to check for collisions
  // conveniently.

  if (new_head.x == mode_snake_state.food.x &&
      new_head.y == mode_snake_state.food.y) {
    snake->nodes_to_grow += SNAKE_FOOD_GROWTH; 
    mode_snake_state.food.x = -1;
    mode_snake_state.food.y = -1;
  } else if (buffer_get_pixel(new_head.x, new_head.y, &buffer_snake) == PIXEL_YELLOW) {
    ESP_LOGI(LOG_TAG, "Snake dead via self collison :-(");
    snake->flash_countdown = SNAKE_FLASH_COUNT;
    return SNAKE_TIME_DELAY_BETWEEN_DRAWS_MS;
  }
      
  // Add node to the front.
  int new_head_index = (snake->snake_head_index + 1) % SNAKE_MAX_LEN;
  snake->nodes[new_head_index] = new_head;
  snake->snake_head_index = new_head_index;

  if (snake->nodes_to_grow == 0) {
    // Remove tail node.
    snake->snake_tail_index = (snake->snake_tail_index + 1) % SNAKE_MAX_LEN;
  } else {
    snake->nodes_to_grow--;
  }

  if ((mode_snake_state.food.x < 0 || mode_snake_state.food.y < 0) &&
      (esp_random() / (double)UINT32_MAX < SNAKE_FOOD_PROBABILITY)) {
    add_food();
  }

  buffer_wipe(&buffer_snake);
  draw_snake_to_displaybuffer(&buffer_snake);
  draw_food_to_displaybuffer(&buffer_snake);

  buffer_wipe(&buffer_draw);
  buffer_fill_from_template(PIXEL_YELLOW, &buffer_snake, &buffer_draw);
  buffer_commit_drawing();

  return SNAKE_TIME_DELAY_BETWEEN_DRAWS_MS;
}

bool mode_snake_direction_input(const SnakeDirection input) {
  if (input < SNAKE_DIRECTION_MIN || input > SNAKE_DIRECTION_MAX) {
    return false;
  }
  SnakeDirection direction = mode_snake_state.snake.moving_direction;

  if ((direction == SNAKE_NORTH && input == SNAKE_SOUTH) ||
      (direction == SNAKE_SOUTH && input == SNAKE_NORTH) ||
      (direction == SNAKE_WEST && input == SNAKE_EAST) ||
      (direction == SNAKE_EAST && input == SNAKE_WEST)) {
    // Snake cannot go back on itself.
    return false;
  }
  mode_snake_state.snake.moving_direction = input;
  return true;
}

bool mode_snake_rel_direction_input(bool left, bool right) {
  SnakeDirection now_direction = mode_snake_state.snake.moving_direction;
  SnakeDirection new_direction = SNAKE_NORTH;

  if (left && right) {
    return false;
  } else if (left) {
    switch (now_direction) {
      case SNAKE_NORTH:
        new_direction = SNAKE_WEST;
        break;
      case SNAKE_EAST:
        new_direction = SNAKE_NORTH;
        break;
      case SNAKE_SOUTH:
        new_direction = SNAKE_EAST;
        break;
      case SNAKE_WEST:
        new_direction = SNAKE_SOUTH;
        break;
    }
  } else if (right) {
    switch (now_direction) {
      case SNAKE_NORTH:
        new_direction = SNAKE_EAST;
        break;
      case SNAKE_EAST:
        new_direction = SNAKE_SOUTH;
        break;
      case SNAKE_SOUTH:
        new_direction = SNAKE_WEST;
        break;
      case SNAKE_WEST:
        new_direction = SNAKE_NORTH;
        break;
    }
  }

  mode_snake_state.snake.moving_direction =  new_direction;
  return true;
}
