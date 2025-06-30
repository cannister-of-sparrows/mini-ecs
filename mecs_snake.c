// SPDX-License-Identifier: GPL-3.0-or-later
//
// MECS Snake — a contrived example using an Entity Component System (ECS).
// All data is stored in components; all behavior is implemented in systems.
// The system has no explicit concept of a "snake" — only data and logic.
// This is obviously not the best way to implement Snake; it's just a demo.
//
// Suggested ECS-based extensions:
// - Add multiple snakes (each with its own Interactable + Direction)
// - Implement timed hazards or enemy entities with AI movement
// - Introduce powerups (e.g., speed boost, shrink) as Edible variants
// - Track entity lifetimes with a Decay component
// - Add portals: position-linked entities that warp consumers
// - Visual effects via transient Drawable-only "particles"

#include "mini_ecs.h"
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

#define WIDTH 20
#define HEIGHT 10
#define INVALID_ENTITY ((Entity)-1)

struct termios orig_termios;

typedef struct { } Collidable;
typedef struct { } Consumer;
typedef struct { } Interactable;
typedef enum { UP, DOWN, LEFT, RIGHT } Direction;
typedef struct { char symbol; } Drawable;
typedef struct { int points; bool grows; bool resets; } Edible;
typedef struct { Entity lead; Entity follower; } Following;
typedef struct { int x, y; } Position;

#define positions_equal(a, b) ((a).x == (b).x && (a).y == (b).y)

typedef struct {
    EntityManager em;
    MECS_DEFINE_COMPONENT(Collidable, collidable);
    MECS_DEFINE_COMPONENT(Consumer, consumer);
    MECS_DEFINE_COMPONENT(Direction, direction);
    MECS_DEFINE_COMPONENT(Drawable, drawable);
    MECS_DEFINE_COMPONENT(Edible, edible);
    MECS_DEFINE_COMPONENT(Entity, follower);
    MECS_DEFINE_COMPONENT(Interactable, interactable);
    MECS_DEFINE_COMPONENT(Position, position);
    int score;
} SnakeWorld;

void clear_components(SnakeWorld* game, Entity e) {
    MECS_CLEAR_COMPONENT(game, collidable, e);
    MECS_CLEAR_COMPONENT(game, consumer, e);
    MECS_CLEAR_COMPONENT(game, direction, e);
    MECS_CLEAR_COMPONENT(game, drawable, e);
    MECS_CLEAR_COMPONENT(game, edible, e);
    MECS_CLEAR_COMPONENT(game, follower, e);
    MECS_CLEAR_COMPONENT(game, interactable, e);
    MECS_CLEAR_COMPONENT(game, position, e);
}

static inline void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

// Game lifecycle
static SnakeWorld* new_game();
static void free_game(SnakeWorld* game);
static void destroy_entity(SnakeWorld* game, Entity e);

// Snake initialization and growth
static void init_snake(SnakeWorld* game, int length);
static Entity create_snake_head(SnakeWorld* game, Position pos, Direction dir);
static Entity create_snake_segment(SnakeWorld* game, Position pos, Entity follows);
static Entity last_follower(SnakeWorld* game, Entity lead);
static void grow(SnakeWorld* game, Entity lead);

// Apple/edible logic
static Entity init_apple(SnakeWorld* game);
static bool is_occupied(SnakeWorld* game, Position pos);
static void place_edible(SnakeWorld* game, Entity edible);

// Game state and logic updates
static void update_state(SnakeWorld* game);
static void update_interactables(SnakeWorld* game);
static void update_followers_of(SnakeWorld* game, Entity leader);
static void update_edibles(SnakeWorld* game);
static bool game_over(SnakeWorld* game);

// Input and rendering
static void handle_input(SnakeWorld* game);
static int kbhit(); // Returns true if a key was pressed (non-blocking)
static char get_key(); // Reads a single key if available, or 0 otherwise
static void render(SnakeWorld* game);

// Terminal and system setup
static void reset_terminal_mode(); // Restore terminal to original settings on exit
static void set_conio_terminal_mode(); // Switch to raw terminal mode (disable buffering + echo)
static void init_system();
static void teardown_system();

int main() {
    init_system();
    SnakeWorld* game = new_game();
init_snake(game, 3);
    place_edible(game, init_apple(game));

    while(1) {
        handle_input(game);
        update_state(game);
        if (game_over(game)) break;
        render(game);
        sleep_ms(200);
    }

    printf("Game Over!\n");
    printf("Press any key to exit...\n");
    getchar();
    free_game(game);
    teardown_system();
}

SnakeWorld* new_game() {
    SnakeWorld* game = calloc(1, sizeof(SnakeWorld));
    return game;
}

void free_game(SnakeWorld* game) {
    free(game);
}

void destroy_entity(SnakeWorld* game, Entity e) {
    clear_components(game, e);
    mecs_entity_destroy(&game->em, e);
}

void init_snake(SnakeWorld* game, int length) {
    Entity snake[length];

    for (int i = 0; i < length; ++i) {
        Position pos = {5 - i, 5};

        if (i == 0) {
            snake[i] = create_snake_head(game, pos, RIGHT);
        } else {
            snake[i] = create_snake_segment(game, pos, snake[i - 1]);
        }
    }
}

Entity create_snake_head(SnakeWorld* game, Position pos, Direction dir) {
    Entity head = mecs_entity_create(&game->em);
    MECS_SET_COMPONENT(game, interactable, head, ((Interactable){ }));
    MECS_SET_COMPONENT(game, direction, head, dir);
    MECS_SET_COMPONENT(game, consumer, head, ((Consumer){ }));
    MECS_SET_COMPONENT(game, drawable, head, ((Drawable){ 'O' }));
    MECS_SET_COMPONENT(game, position, head, pos);
    MECS_SET_COMPONENT(game, collidable, head, ((Collidable){ }));
    return head;
}

Entity create_snake_segment(SnakeWorld* game, Position pos, Entity follows) {
    Entity segment = mecs_entity_create(&game->em);
    MECS_SET_COMPONENT(game, position, segment, pos);
    MECS_SET_COMPONENT(game, follower, segment, follows);
    MECS_SET_COMPONENT(game, drawable, segment, ((Drawable){ 'o' }));
    MECS_SET_COMPONENT(game, collidable, segment, ((Collidable){ }));
    return segment;
}

Entity last_follower(SnakeWorld* game, Entity lead) {
    Entity map[MAX_ENTITIES];
    memset(map, 0xFF, sizeof(map)); // Set all to INVALID_ENTITY

    // Build map: lead -> follower
    MECS_FOREACH_2(game, position, follower, e) {
        Entity l = game->follower[e];
        if (l < MAX_ENTITIES)
            map[l] = e;
    }

    // Walk the chain starting from `lead`
    Entity current = lead;
    while (current < MAX_ENTITIES && map[current] != INVALID_ENTITY)
        current = map[current];

    return current;
}

void grow(SnakeWorld* game, Entity lead) {
    Entity tail = last_follower(game, lead);
    create_snake_segment(game, game->position[tail], tail);
}

Entity init_apple(SnakeWorld* game) {
    Entity apple = mecs_entity_create(&game->em);
    MECS_SET_COMPONENT(game, drawable, apple, ((Drawable){ '@' }));
    MECS_SET_COMPONENT(game, edible, apple, ((Edible){ 1, true, true }));
    MECS_SET_COMPONENT(game, position, apple, ((Position){ 0, 0 }));
    return apple;
}

bool is_occupied(SnakeWorld* game, Position pos) {
    MECS_FOREACH_1(game, position, e) {
        if (positions_equal(game->position[e], pos)) {
            return true;
        }
    }

    return false;
}

void place_edible(SnakeWorld* game, Entity edible) {
    Position pos;
    do {
        pos.x = rand() % WIDTH;
        pos.y = rand() % HEIGHT;
    } while (is_occupied(game, pos));

    MECS_SET_COMPONENT(game, position, edible, pos);
}

void update_state(SnakeWorld* game) {

    update_interactables(game);
    update_edibles(game);
}

void update_interactables(SnakeWorld* game) {
    MECS_FOREACH_2(game, position, direction, e) {
        Position* p = &game->position[e];
        Direction d = game->direction[e];
        update_followers_of(game, e);
        switch (d) {
            case UP:    p->y--; break;
            case DOWN:  p->y++; break;
            case LEFT:  p->x--; break;
            case RIGHT: p->x++; break;
        }
    }
}

void update_followers_of(SnakeWorld* game, Entity leader) {
    Position* leader_pos = &game->position[leader];

    MECS_FOREACH_2(game, position, follower, e) {
        if (game->follower[e] == leader) {
            update_followers_of(game, e);
            game->position[e] = *leader_pos;
        }
    }
}

void update_edibles(SnakeWorld* game) {
    MECS_FOREACH_2(game, position, consumer, mouth) {
        Position* mouth_pos = &game->position[mouth];

        MECS_FOREACH_2(game, position, edible, food) {
            if (positions_equal(*mouth_pos, game->position[food])) {
                Edible* ef = &game->edible[food];
                game->score += ef->points;
                if (ef->grows) grow(game, mouth);
                if (ef->resets) {
                    place_edible(game, food);
                } else {
                    destroy_entity(game, food);
                }
            }
        }
    }
}

bool game_over(SnakeWorld* game) {
    MECS_FOREACH_2(game, position, interactable, i) {
        Position ipos = game->position[i];

        // Hit wall
        if (ipos.x < 0 || ipos.x >= WIDTH || ipos.y < 0 || ipos.y >= HEIGHT) {
            return true;
        }

        // Hit something collidable (like a snake segment)
        MECS_FOREACH_2(game, position, collidable, c) {
            if (i == c) continue;

            Position cpos = game->position[c];
            if(positions_equal(ipos, cpos)) {
                return true;
            }
        }
    }

    return false;
}

void handle_input(SnakeWorld* game) {

    bool direction_input = false;
    Direction dir;

    char key = get_key();
    if (key == 'w') direction_input = true, dir = UP;
    if (key == 's') direction_input = true, dir = DOWN;
    if (key == 'a') direction_input = true, dir = LEFT;
    if (key == 'd') direction_input = true, dir = RIGHT;

    if (!direction_input) {
        return;
    }

    MECS_FOREACH_2(game, interactable, direction, e) {
        Direction* current = &game->direction[e];
        if (dir == UP && *current != DOWN) *current = UP;
        if (dir == DOWN && *current != UP) *current = DOWN;
        if (dir == RIGHT && *current != LEFT) *current = RIGHT;
        if (dir == LEFT && *current != RIGHT) *current = LEFT;
    }
}

int kbhit() {
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    return poll(&pfd, 1, 0) > 0;
}

char get_key() {
    if (kbhit()) return getchar();
    return 0;
}

void render(SnakeWorld* game) {
    char grid[HEIGHT][WIDTH];
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            grid[y][x] = '.';

    MECS_FOREACH_2(game, drawable, position, e) {
        Position p = game->position[e];
        if ((unsigned)p.x < WIDTH && (unsigned)p.y < HEIGHT) {
            grid[p.y][p.x] = game->drawable[e].symbol;
        }
    }

    printf("\033[2J\033[H"); // ANSI escape: clear screen and reset cursor position

    // Top border
    printf("┌");
    for (int i = 0; i < WIDTH; ++i) printf("─");
    printf("┐\n");

    // Grid with vertical borders
    for (int y = 0; y < HEIGHT; ++y) {
        printf("│");
        for (int x = 0; x < WIDTH; ++x) {
            printf("%c", grid[y][x]);
        }
        printf("│\n");
    }

    // Bottom border
    printf("└");
    for (int i = 0; i < WIDTH; ++i) printf("─");
    printf("┘\n");

    printf("Score: %i\n", game->score);
}

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode() {
    struct termios new_termios;

    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));
    atexit(reset_terminal_mode);

    new_termios.c_lflag &= ~(ICANON | ECHO); // disable line buffering and echo
    tcsetattr(0, TCSANOW, &new_termios);
}

void init_system() {
    system("clear");
    printf("\033[?25l");
    set_conio_terminal_mode();
    srand(time(NULL));
}

void teardown_system() {
    reset_terminal_mode();
    printf("\033[?25h"); // show cursor
}
