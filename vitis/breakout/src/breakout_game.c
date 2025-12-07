#include "breakout_game.h"

/*
 * IMPORTANT: To get math functions to work, you must link them in the project build settings.
 * 1. Right-Click "breakout" application project and go to properties
 * 2. In the left panel, go to C/C++ Build -> Settings
 * 3. In the Tool Settings tab, select ARM gcc linker -> Libraries
 * 4. In Libraries (-l), add "m" to link the math library
 *
 * More info: https://adaptivesupport.amd.com/s/article/52971?language=en_US
 */
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
//#include <unistd.h>

#include "xil_printf.h"
#include "xtime_l.h"
#include "sleep.h"

// ============================================================================
// INCLUDE RENDERER HEADERS
// ============================================================================
#include "renderer.h"
#include "profiler.h"

// ============================================================================
// CONSTANTS AND DEFINES
// ============================================================================
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

#define FPS           60
#define FRAME_DELAY_US (1000000 / FPS) // in microseconds

// Game objects
#define PADDLE_WIDTH   100
#define PADDLE_HEIGHT  15
#define PADDLE_SPEED   6
#define PADDLE_Y       (SCREEN_HEIGHT - 40)

#define BALL_RADIUS    7
#define BALL_SPEED     6

#define BRICK_WIDTH    75
#define BRICK_HEIGHT   15
#define BRICK_ROWS     4
#define BRICK_COLS     10
#define BRICK_PADDING  5

#define MAX_LIVES      3

#define WALL_WIDTH     480
#define MAX_ANGLE      (45 * M_PI / 180.0)  // in radians

// Color format: RGB (8-bit each)
#define COLOR_BLACK_R  0
#define COLOR_BLACK_G  0
#define COLOR_BLACK_B  0

#define COLOR_WHITE_R  255
#define COLOR_WHITE_G  255
#define COLOR_WHITE_B  255

#define COLOR_CYAN_R   0
#define COLOR_CYAN_G   255
#define COLOR_CYAN_B   255

#define COLOR_RED_R    255
#define COLOR_RED_G    0
#define COLOR_RED_B    0

// ============================================================================
// DATA STRUCTURES
// ============================================================================
typedef struct {
    float x;
    float y;
    float vx; // velocity x
    float vy; // velocity y
} Ball;

typedef struct {
    float x;
    float y;
    float vx;
} Paddle;

typedef struct {
    float x;
    float y;
    int   alive;
} Brick;

typedef struct {
    Ball   ball;
    Paddle paddle;
    Brick  bricks[BRICK_ROWS * BRICK_COLS];
    int    score;
    int    lives;
    int    bricks_remaining;
    int    game_running;
    int    ball_launched;
} GameState;

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

profiler_s profiler_breakout[10];

void init_game(GameState *game) {
    // Initialize paddle
    game->paddle.x  = SCREEN_WIDTH / 2 - PADDLE_WIDTH / 2;
    game->paddle.y  = PADDLE_Y;
    game->paddle.vx = 0;

    // Initialize ball
    game->ball.x  = game->paddle.x + PADDLE_WIDTH / 2;
    game->ball.y  = game->paddle.y - BALL_RADIUS - 5;
    game->ball.vx = 0;
    game->ball.vy = 0;

    // Initialize bricks
    int brick_index = 0;
    int brick_offset = (BRICK_WIDTH + BRICK_PADDING) * BRICK_COLS - BRICK_PADDING;
    brick_offset = SCREEN_WIDTH / 2 - brick_offset / 2 - BRICK_PADDING;
    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLS; col++) {
            game->bricks[brick_index].x     =
                brick_offset + col * (BRICK_WIDTH + BRICK_PADDING) + BRICK_PADDING;
            game->bricks[brick_index].y     =
                row * (BRICK_HEIGHT + BRICK_PADDING) + 60;
            game->bricks[brick_index].alive = 1;
            brick_index++;
        }
    }

    game->score            = 0;
    game->lives            = MAX_LIVES;
    game->bricks_remaining = BRICK_ROWS * BRICK_COLS;
    game->game_running     = 1;
    game->ball_launched    = 0;
}

// ============================================================================
// INPUT HANDLING
// ============================================================================
void handle_input_zynq(GameState *game) {
	volatile static unsigned int buttons;
	buttons = *(unsigned int*)0x41210000;

	game->paddle.vx = 0;
	if (buttons & 0b1000)
		game->paddle.vx = -PADDLE_SPEED;
	else if (buttons & 0b0001)
		game->paddle.vx = PADDLE_SPEED;

	if (!game->ball_launched && (buttons & 0b0110)){
		game->ball_launched = TRUE;
		game->ball.vx = 0;
		game->ball.vy = -BALL_SPEED;
	}
}

// ============================================================================
// COLLISION DETECTION
// ============================================================================
int check_rect_collision(float x1, float y1, float w1, float h1,
                         float x2, float y2, float w2, float h2) {
    return (x1 < x2 + w2 &&
            x1 + w1 > x2 &&
            y1 < y2 + h2 &&
            y1 + h1 > y2);
}

int check_circle_rect_collision(float cx, float cy, float r,
                                float rx, float ry, float rw, float rh) {
    float closest_x = (cx < rx)       ? rx :
                      (cx > rx + rw)  ? rx + rw : cx;
    float closest_y = (cy < ry)       ? ry :
                      (cy > ry + rh)  ? ry + rh : cy;

    float dx   = cx - closest_x;
    float dy   = cy - closest_y;
    return dx*dx + dy*dy < r*r;
//    float dist = sqrt(dx * dx + dy * dy);
//    return dist < r;

}

// ============================================================================
// GAME UPDATE
// ============================================================================
void reset_ball_on_paddle(GameState *game) {
    game->ball_launched = 0;
    game->ball.x        = game->paddle.x + PADDLE_WIDTH / 2;
    game->ball.y        = game->paddle.y - BALL_RADIUS - 5;
    game->ball.vx       = 0;
    game->ball.vy       = 0;
}

void update_game(GameState *game) {
    if (!game->game_running) {
        return;
    }

    // Update paddle position
    game->paddle.x += game->paddle.vx;

    // Keep paddle in bounds
    if (game->paddle.x < WALL_WIDTH)
        game->paddle.x = WALL_WIDTH;
    if (game->paddle.x + PADDLE_WIDTH >= SCREEN_WIDTH - WALL_WIDTH)
        game->paddle.x = SCREEN_WIDTH - WALL_WIDTH - PADDLE_WIDTH - 1;

    // If ball not launched, keep it on paddle
    if (!game->ball_launched) {
        game->ball.x = game->paddle.x + PADDLE_WIDTH / 2;
        game->ball.y = game->paddle.y - BALL_RADIUS - 5;
        return;
    }

    // Update ball position
    game->ball.x += game->ball.vx;
    game->ball.y += game->ball.vy;

    // Ball collision with walls
    if (game->ball.x - BALL_RADIUS < WALL_WIDTH) {
        game->ball.x  = BALL_RADIUS + WALL_WIDTH;
        game->ball.vx = -game->ball.vx;
    }
    if (game->ball.x + BALL_RADIUS >= SCREEN_WIDTH - WALL_WIDTH) {
        game->ball.x  = SCREEN_WIDTH - WALL_WIDTH - BALL_RADIUS - 1;
        game->ball.vx = -game->ball.vx;
    }
    if (game->ball.y - BALL_RADIUS < 0) {
        game->ball.y  = BALL_RADIUS;
        game->ball.vy = -game->ball.vy;
    }

    // Ball collision with paddle
    if (check_circle_rect_collision(game->ball.x, game->ball.y, BALL_RADIUS,
                                    game->paddle.x, game->paddle.y,
                                    PADDLE_WIDTH, PADDLE_HEIGHT)) {
    	float vx = game->ball.vx;
    	float vy = game->ball.vy;
    	float spd = sqrt(vx*vx + vy*vy);

        game->ball.y  = game->paddle.y - BALL_RADIUS - 5;
//        game->ball.vy = -game->ball.vy;


        //get x offset between ball center and paddle center
        int offset = game->ball.x - (game->paddle.x + PADDLE_WIDTH/2);
        float magnitude = 1.0 * offset / (PADDLE_WIDTH/2);
        if (magnitude > 1.0) magnitude = 1.0;
        if (magnitude < -1.0) magnitude = -1.0;
        printf("ball offset = %d, magnitude = %f\n\r", offset, magnitude);

        //range will be [-MAX_ANGLE, MAX_ANGLE]
        float angle = MAX_ANGLE * magnitude;
        game->ball.vy = -spd * cos(angle);
        game->ball.vx = spd * sin(angle);


//        if (game->paddle.vx != 0) {
//            game->ball.vx = game->paddle.vx * 0.8f;
//        }
    }

    // Ball collision with bricks
    for (int i = 0; i < BRICK_ROWS * BRICK_COLS; i++) {
        if (game->bricks[i].alive) {
            if (check_circle_rect_collision(game->ball.x, game->ball.y, BALL_RADIUS,
                                            game->bricks[i].x, game->bricks[i].y,
                                            BRICK_WIDTH, BRICK_HEIGHT)) {
                game->bricks[i].alive = 0;
                game->bricks_remaining--;
                game->ball.vy = -game->ball.vy;
                game->score  += 10;

                if (game->bricks_remaining <= 0) {
                    xil_printf("You win! Final score: %d\n\r", game->score);
                    game->game_running = 0;
                }
                break;
            }
        }
    }

    // Ball goes out of bounds (bottom) -> lose a life
    if (game->ball.y - BALL_RADIUS > SCREEN_HEIGHT) {
        game->lives--;

        if (game->lives > 0) {
            xil_printf("Life lost! Lives remaining: %d\n\r", game->lives);
            reset_ball_on_paddle(game);
        } else {
            xil_printf("Game Over! You lost. Final score: %d\n\r", game->score);
            game->game_running = 0;
        }
    }
}

// ============================================================================
// DRAWING PRIMITIVES (using renderer_draw_pixel)
// ============================================================================
void draw_rect(int x, int y, int w, int h, u8 r, u8 g, u8 b) {
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if (px >= 0 && px < SCREEN_WIDTH &&
                py >= 0 && py < SCREEN_HEIGHT) {
                renderer_draw_pixel(px, py, r, g, b);
            }
        }
    }
}

void draw_filled_circle(int cx, int cy, int radius, u8 r, u8 g, u8 b) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < SCREEN_WIDTH &&
                    py >= 0 && py < SCREEN_HEIGHT) {
                    renderer_draw_pixel(px, py, r, g, b);
                }
            }
        }
    }
}

// ============================================================================
// RENDERING TO HDMI
// ============================================================================
void render_game_hdmi(GameState *game) {
    // Screen is already cleared to grey from render_render() call
	// Draw the black void in between the walls
	for (int y = 0; y < SCREEN_HEIGHT; y++){
		renderer_draw_grey_row(
				WALL_WIDTH + 1,
				y,
				SCREEN_WIDTH - (WALL_WIDTH * 2) - 2,
				0
		);
	}


    // Draw paddle (cyan)
    draw_rect((int)game->paddle.x, (int)game->paddle.y,
              PADDLE_WIDTH, PADDLE_HEIGHT,
              COLOR_CYAN_R, COLOR_CYAN_G, COLOR_CYAN_B);

    // Draw ball (white)
    draw_filled_circle((int)game->ball.x, (int)game->ball.y,
                       BALL_RADIUS,
                       COLOR_WHITE_R, COLOR_WHITE_G, COLOR_WHITE_B);

    // Draw bricks (red)
    for (int i = 0; i < BRICK_ROWS * BRICK_COLS; i++) {
        if (game->bricks[i].alive) {
            draw_rect((int)game->bricks[i].x, (int)game->bricks[i].y,
                      BRICK_WIDTH, BRICK_HEIGHT,
                      COLOR_RED_R, COLOR_RED_G, COLOR_RED_B);
        }
    }

    //draw lives display (cyan)
    for (int i = 0; i < game->lives; i++){
    	draw_rect(WALL_WIDTH + 10 + (20 + 10) * i, 10, 20, 20, 0, 255, 255);
    }

    // Print game state to UART for debugging
    static int print_counter = 0;
    print_counter++;
    if (print_counter % 60 == 0) {
        xil_printf("Score: %d | Lives: %d | Bricks: %d | Ball: (%.0f, %.0f)\n\r",
                   game->score, game->lives, game->bricks_remaining,
                   game->ball.x, game->ball.y);
    }
}

// ============================================================================
// MAIN BREAKOUT GAME LOOP
// ============================================================================
void breakout_game_run() {
    GameState game;
    init_game(&game);

    xil_printf("Breakout Game Started!\n\r");
    xil_printf("Screen: %d x %d\n\r", SCREEN_WIDTH, SCREEN_HEIGHT);

    long frame_counter         = 0;
    int  debug_update_interval = 3; // Print debug every 3 seconds

    while (game.game_running) {
        profiler_start(&profiler_breakout[0]);

        // Input handling
        profiler_start(&profiler_breakout[1]);
        handle_input_zynq(&game);
        profiler_end(&profiler_breakout[1]);

        // Game update
        profiler_start(&profiler_breakout[2]);
        update_game(&game);
        profiler_end(&profiler_breakout[2]);

        // Render to HDMI
        profiler_start(&profiler_breakout[3]);
        render_game_hdmi(&game);
        profiler_end(&profiler_breakout[3]);

        // Push frame to display
        profiler_start(&profiler_breakout[4]);
        renderer_render(100);
        profiler_end(&profiler_breakout[4]);

        // Debug profiler prints
        frame_counter++;
        if (frame_counter % (FPS * debug_update_interval) == 0) {
            xil_printf("Frame: %lu\n\r", frame_counter);
            xil_printf("Input time: %lu us\n\r",   profiler_breakout[1].elapsed_us);
            xil_printf("Update time: %lu us\n\r",  profiler_breakout[2].elapsed_us);
            xil_printf("Render time: %lu us\n\r",  profiler_breakout[3].elapsed_us);
            xil_printf("Display time: %lu us\n\r", profiler_breakout[4].elapsed_us);
            xil_printf("Total time: %lu us\n\r",
                       profiler_breakout[1].elapsed_us +
                       profiler_breakout[2].elapsed_us +
                       profiler_breakout[3].elapsed_us +
                       profiler_breakout[4].elapsed_us);
            xil_printf("\n\r");
        }

        profiler_end(&profiler_breakout[0]);

        // Frame rate capping
        if (profiler_breakout[0].elapsed_us < FRAME_DELAY_US) {
            usleep(FRAME_DELAY_US - profiler_breakout[0].elapsed_us);
        }
    }

    xil_printf("\n\rGame Over!\n\r");
    xil_printf("Final Score: %d\n\r", game.score);
    xil_printf("Lives Remaining: %d\n\r", game.lives);
}

// ============================================================================
// MAIN ENTRY POINT (for Vitis SDK)
// ============================================================================
int main2() {
    xil_printf("==============================================\n\r");
    xil_printf("Zynq Breakout Game - HDMI Output\n\r");
    xil_printf("==============================================\n\r\n\r");

    // Initialize renderer (VDMA + Display Controller)
    renderer_initialize();

    // Run the breakout game
    breakout_game_run();

    return 0;
}
