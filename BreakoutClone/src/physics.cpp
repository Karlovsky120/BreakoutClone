#include "physics.h"

#define MILISECONDS(microseconds) (microseconds * 0.001f)
#define SECONDS(microseconds)     (microseconds * 0.000001f)

void Physics::resolveFrame(const uint32_t& frameTimeUS, std::vector<Instance>& instances) {
    uint32_t padDirectionMovement = 1;

    Instance& ball      = instances[BALL_INDEX];
    Instance& pad       = instances[PAD_INDEX];
    Instance& leftWall  = instances[LEFT_WALL_INDEX];
    Instance& rightWall = instances[RIGHT_WALL_INDEX];
    Instance& topWall   = instances[TOP_WALL_INDEX];

    Instance* bricks = instances.data() + BRICK_START_INDEX;

    // Move the pad as much as the ball allows

    // Move the ball, one collision at the time
}