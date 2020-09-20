#include "physics.h"
#include "level.h"

#include <algorithm>

#define CROSS2D(first, second) ((first).x * (second).y - (first).y * (second).x)
#define SIGNUM(x)              ((x > 0.0) - (x < 0.0))

void Physics::resolveFrame(const uint32_t& frameTime /*microseconds*/, Level& level, const float& ballSpeed /*pixels per microsecond*/,
                           const float& padSpeed /*pixels per microsecond*/, glm::vec2& ballDirection, bool& gameOver) {

    Instance& ball      = level.m_instances[BALL_INDEX];
    Instance& pad       = level.m_instances[PAD_INDEX];
    Instance& leftWall  = level.m_instances[LEFT_WALL_INDEX];
    Instance& rightWall = level.m_instances[RIGHT_WALL_INDEX];

    Instance* bricks = level.m_instances.data() + BRICK_START_INDEX;

    float leftWallEdge  = leftWall.position.x + leftWall.scale.x * 0.5f;
    float rightWallEdge = rightWall.position.x - rightWall.scale.x * 0.5f;
    float ballRadius    = ball.scale.x * 0.5f;

    // Move the pad as much as the ball and walls allow
    if (padSpeed != 0.0) {
        float paddP = padSpeed * frameTime;
        float t;

        float leftPadEdge  = pad.position.x - pad.scale.x * 0.5f;
        float rightPadEdge = pad.position.x + pad.scale.x * 0.5f;

        // if it would intersect the ball
        glm::vec2 padMoveDirection = glm::vec2(SIGNUM(paddP), 0.0f);
        if (detectCollision(pad.position, pad.scale, padMoveDirection, paddP, ball.position, ball.scale, t)) {
            pad.position.x += paddP * t * (1.0f - EPSILON);
            float leftLimit  = leftWallEdge + 2.1f * ballRadius + pad.scale.x * 0.5f;
            float rightLimit = rightWallEdge - 2.1f * ballRadius - pad.scale.x * 0.5f;
            pad.position.x   = std::clamp(pad.position.x, leftLimit, rightLimit);
        } else if (leftPadEdge + paddP < leftWallEdge) { // if it would intersect the left wall
            pad.position.x = leftWallEdge + pad.scale.x * 0.5f;
        } else if (rightPadEdge + paddP > rightWallEdge) { // if it would intersect the right wall
            pad.position.x = rightWallEdge - pad.scale.x * 0.5f;
        } else {
            pad.position.x += paddP;
        }
    }

    if (ballSpeed == 0.0f) {
        ball.position.x = pad.position.x;
        return;
    }

    float remainingTravelDistance = ballSpeed * frameTime;
    while (remainingTravelDistance > 0.0f) {
        float     minimalT                    = 2.0f;
        glm::vec2 reflectedDirectionOfClosest = ballDirection; // If no redirection is triggered
        glm::vec2 ballTravelPath              = ballDirection * remainingTravelDistance;

        float t;

        glm::vec2 playAreaCenter = {level.m_windowWidth * 0.5f, level.m_windowHeight * 0.5f};
        glm::vec2 playAreaRect   = {level.m_windowWidth - (2.0f * leftWallEdge + 4.0f * ballRadius), level.m_windowHeight - 4.0f * ballRadius};

        glm::vec2 latestReflectedDirection = ballDirection;
        if (detectCollision(ball.position, ball.scale, latestReflectedDirection, remainingTravelDistance, playAreaCenter, playAreaRect, t)) {
            if (latestReflectedDirection.y < 0.0f) {
                gameOver = true;
                // return;
            }

            minimalT                    = t < minimalT ? t : minimalT;
            reflectedDirectionOfClosest = latestReflectedDirection;
        }

        // The pad
        if (detectCollision(ball.position, ball.scale, latestReflectedDirection, remainingTravelDistance, pad.position, pad.scale, t)) {
            if (t < minimalT) {
                if (ballDirection.y == -latestReflectedDirection.y) {
                    glm::vec2 collisionPoint    = ball.position + ballTravelPath * (ballRadius + t);
                    glm::vec2 padTopLeftCorner  = pad.position - 0.5f * pad.scale;
                    float     hitScale          = glm::distance(collisionPoint, padTopLeftCorner) / pad.scale.x;
                    hitScale                    = hitScale * 2.0f - 1.0f;
                    reflectedDirectionOfClosest = glm::normalize(glm::vec2(hitScale * 1.0f, -1.0f));
                } else {
                    reflectedDirectionOfClosest = latestReflectedDirection;
                }
            }
        }

        // The bricks
        size_t hitIndex = UINT32_MAX;
        for (size_t i = 0; i < level.m_brickCount; ++i) {
            if (bricks[i].health > 0 &&
                detectCollision(ball.position, ball.scale, latestReflectedDirection, remainingTravelDistance, bricks[i].position, bricks[i].scale, t)) {
                minimalT                    = t < minimalT ? t : minimalT;
                reflectedDirectionOfClosest = latestReflectedDirection;
                hitIndex                    = i;
            }
        }

        if (hitIndex != UINT32_MAX && bricks[hitIndex].health != UINT32_MAX) {
            --bricks[hitIndex].health;
        }

        minimalT = minimalT > 1.0f ? 1.0f : minimalT;

        if (ballDirection != reflectedDirectionOfClosest) {
            minimalT -= EPSILON;
            ballDirection = reflectedDirectionOfClosest;
        }

        ball.position += ballTravelPath * minimalT;
        remainingTravelDistance -= remainingTravelDistance * minimalT;
    }
}

bool Physics::detectCollision(const glm::vec2& center1, const glm::vec2& rect1, glm::vec2& ballDirection, const float& ballSpeed, const glm::vec2& center2,
                              const glm::vec2& rect2, float& t) {

    glm::vec2 corner1 = center1 + 0.5f * rect1;
    glm::vec2 corner2 = center2 + 0.5f * rect2;

    float radius1 = glm::distance(center1, corner1);
    float radius2 = glm::distance(center2, corner2);

    float rectDistance = glm::distance(center1, center2);

    if (rectDistance > radius1 + ballSpeed + radius2) {
        return false;
    }

    glm::vec2 minkowskiMax = center2 + rect2 * 0.5f + rect1 * 0.5f;
    glm::vec2 minkowskiMin = center2 - rect2 * 0.5f - rect1 * 0.5f;

    glm::vec2& topLeft     = minkowskiMin;
    glm::vec2  topRight    = {minkowskiMax.x, minkowskiMin.y};
    glm::vec2& bottomRight = minkowskiMax;
    glm::vec2  bottomLeft  = {minkowskiMin.x, minkowskiMax.y};

    float edgeT;
    bool  collisionDetected = false;
    t                       = 1.0f;

    glm::vec2 reflectedDirection = ballDirection;
    glm::vec2 ballVelocity       = ballDirection * ballSpeed;
    // Top edge
    if (detectSegmentsCollision(center1, ballVelocity, topLeft, topRight - topLeft, edgeT)) {
        collisionDetected  = true;
        t                  = edgeT < t ? edgeT : t;
        reflectedDirection = {ballDirection.x, -ballDirection.y};
    }

    // Right edge
    if (detectSegmentsCollision(center1, ballVelocity, topRight, bottomRight - topRight, edgeT)) {
        collisionDetected  = true;
        t                  = edgeT < t ? edgeT : t;
        reflectedDirection = {-ballDirection.x, ballDirection.y};
    }

    // Bottom edge
    if (detectSegmentsCollision(center1, ballVelocity, bottomRight, bottomLeft - bottomRight, edgeT)) {
        collisionDetected  = true;
        t                  = edgeT < t ? edgeT : t;
        reflectedDirection = {ballDirection.x, -ballDirection.y};
    }

    // Left edge
    if (detectSegmentsCollision(center1, ballVelocity, bottomLeft, topLeft - bottomLeft, edgeT)) {
        collisionDetected  = true;
        t                  = edgeT < t ? edgeT : t;
        reflectedDirection = {-ballDirection.x, ballDirection.y};
    }

    if (collisionDetected) {
        ballDirection = reflectedDirection;
    }

    return collisionDetected;
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
