#include "physics.h"

#pragma warning(push, 0)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/glm.hpp"
#pragma warning(pop)

#define MILISECONDS(microseconds) (microseconds * 0.001f)
#define SECONDS(microseconds)     (microseconds * 0.000001f)

#define CROSS2D(first, second) ((first).x * (second).y - (first).y * (second).x)
#define SIGNUM(x)              ((x > 0.0) - (x < 0.0))

void Physics::resolveFrame(const uint32_t& frameTime /*microseconds*/, std::vector<Instance>& instances, const float& ballSpeed /*pixels per microsecond*/,
                           const float& padSpeed /*pixels per microsecond*/, glm::vec2& ballDirection) {

    Instance& ball = instances[BALL_INDEX];

    Instance& pad       = instances[PAD_INDEX];
    Instance& leftWall  = instances[LEFT_WALL_INDEX];
    Instance& rightWall = instances[RIGHT_WALL_INDEX];
    Instance& topWall   = instances[TOP_WALL_INDEX];

    Instance* bricks = instances.data() + BRICK_START_INDEX;

    // Move the pad as much as the ball and walls allow
    if (padSpeed != 0.0) {
        float paddP = padSpeed * frameTime;
        float t;

        // if intersects ball
        if (detectCollision(pad.position, pad.scale, glm::vec2(paddP, 0.0f), ball.position, ball.scale, t)) {
            pad.position.x += paddP * t * 0.999f;
        } else {
            float leftWallEdge = leftWall.position.x + leftWall.scale.x * 0.5f;
            float leftPadEdge  = pad.position.x - pad.scale.x * 0.5f;

            // if intersects left wall
            if (leftPadEdge + paddP < leftWallEdge) {
                pad.position.x = leftWallEdge + pad.scale.x * 0.5f;
            } else {
                float rightWallEdge = rightWall.position.x - rightWall.scale.x * 0.5f;
                float rightPadEdge  = pad.position.x + pad.scale.x * 0.5f;

                // if intersects right wall
                if (rightPadEdge + paddP > rightWallEdge) {
                    pad.position.x = rightWallEdge - pad.scale.x * 0.5f;
                } else {
                    pad.position.x += paddP * 0.999f;
                }
            }
        }
    }

    // Move the ball, one collision at the time
}

bool Physics::detectSegmentsCollision(const glm::vec2& start1, const glm::vec2 dir1, const glm::vec2& start2, const glm::vec2& dir2, float& t) {
    float directionsCross = CROSS2D(dir1, dir2);

    if (directionsCross == 0) {
        return false;
    }

    t       = CROSS2D(start2 - start1, dir2) / directionsCross;
    float u = CROSS2D(start2 - start1, dir1) / directionsCross;

    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        return true;
    }

    return false;
}

bool Physics::detectCollision(const glm::vec2& center1, const glm::vec2& rect1, const glm::vec2& moveVector, const glm::vec2& center2, const glm::vec2& rect2,
                              float& t) {

    float radius1 = std::fmaxf(rect1.x, rect1.y) * 0.5f;
    float radius2 = std::fmaxf(rect2.x, rect2.y) * 0.5f;

    float rectDistance = glm::distance(center1, center2);

    if (rectDistance > radius1 + glm::length(moveVector) + radius2) {
        return false;
    }

    glm::vec2 minkowskiMax = center2 + rect2 * 0.5f + rect1 * 0.5f;
    glm::vec2 minkowskiMin = center2 - rect2 * 0.5f - rect1 * 0.5f;

    glm::vec2& topLeft     = minkowskiMin;
    glm::vec2  topRight    = {minkowskiMax.x, minkowskiMin.y};
    glm::vec2& bottomRight = minkowskiMax;
    glm::vec2  bottomLeft  = {minkowskiMin.x, minkowskiMax.y};

    float testedT;
    bool  collisionDetected = false;
    t                       = 1.0f;

    // Top edge
    if (detectSegmentsCollision(center1, moveVector, topLeft, topRight - topLeft, testedT)) {
        collisionDetected = true;
        t                 = testedT < t ? testedT : t;
    }

    // Right edge
    if (detectSegmentsCollision(center1, moveVector, topRight, bottomRight - topRight, testedT)) {
        collisionDetected = true;
        t                 = testedT < t ? testedT : t;
    }

    // Bottom edge
    if (detectSegmentsCollision(center1, moveVector, bottomRight, bottomLeft - bottomRight, testedT)) {
        collisionDetected = true;
        t                 = testedT < t ? testedT : t;
    }

    // Left edge
    if (detectSegmentsCollision(center1, moveVector, bottomLeft, topLeft - bottomLeft, testedT)) {
        collisionDetected = true;
        t                 = testedT < t ? testedT : t;
    }

    return collisionDetected;
}