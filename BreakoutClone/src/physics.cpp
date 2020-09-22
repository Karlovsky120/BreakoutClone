#include "physics.h"
#include "level.h"

#include <algorithm>

#define CROSS2D(first, second) ((first).x * (second).y - (first).y * (second).x)
#define SIGNUM(x)              ((x > 0.0) - (x < 0.0))

LevelState Physics::resolveFrame(const uint32_t& frameTime /*microseconds*/, Level& level, const float& ballSpeedModifier /*pixels per microsecond*/,
                                 const float& padSpeedModifier /*pixels per microsecond*/, glm::vec2& ballDirection,
                                 std::vector<CollisionData>& collisionInfo) {
    std::vector<Instance>& instances = level.getInstances();

    glm::vec2& ballPosition = instances[BALL_INDEX].position;
    glm::vec2& ballScale    = instances[BALL_INDEX].scale;
    glm::vec2& padPosition  = instances[PAD_INDEX].position;
    glm::vec2& padScale     = instances[PAD_INDEX].scale;

    Instance& leftWall  = instances[LEFT_WALL_INDEX];
    Instance& rightWall = instances[RIGHT_WALL_INDEX];

    Instance* bricks = instances.data() + BRICK_START_INDEX;

    float leftWallEdge  = leftWall.position.x + leftWall.scale.x * 0.5f;
    float rightWallEdge = rightWall.position.x - rightWall.scale.x * 0.5f;
    float ballRadius    = ballScale.x * 0.5f;

    float padSpeed = level.getBasePadSpeed() * padSpeedModifier;

    // Move the pad as much as the ball and walls allow
    if (padSpeed != 0.0) {
        float paddP = padSpeed * frameTime;
        float t;

        float leftPadEdge  = padPosition.x - padScale.x * 0.5f;
        float rightPadEdge = padPosition.x + padScale.x * 0.5f;

        // if it would intersect the ball
        glm::vec2 padMoveDirection = glm::vec2(SIGNUM(paddP), 0.0f);
        if (detectCollision(padPosition, padScale, padMoveDirection, paddP, ballPosition, ballScale, t)) {
            padPosition.x += paddP * t * (1.0f - EPSILON);
            float leftLimit  = leftWallEdge + 2.1f * ballRadius + padScale.x * 0.5f;
            float rightLimit = rightWallEdge - 2.1f * ballRadius - padScale.x * 0.5f;
            padPosition.x    = std::clamp(padPosition.x, leftLimit, rightLimit);
        } else if (leftPadEdge + paddP < leftWallEdge) { // if it would intersect the left wall
            padPosition.x = leftWallEdge + padScale.x * 0.5f;
        } else if (rightPadEdge + paddP > rightWallEdge) { // if it would intersect the right wall
            padPosition.x = rightWallEdge - padScale.x * 0.5f;
        } else {
            padPosition.x += paddP;
        }
    }

    float ballSpeed = level.getBaseBallSpeed() * ballSpeedModifier;

    if (ballSpeed == 0.0f) {
        ballPosition.x = padPosition.x;
        return LevelState::STILL_ALIVE;
    }

    float remainingTravelDistance = ballSpeed * frameTime;
    float travelTime              = 0;
    while (remainingTravelDistance > 0.0f) {
        CollisionData collisionData;
        float         minimalT                    = 2.0f;
        glm::vec2     reflectedDirectionOfClosest = ballDirection; // If no redirection is triggered
        glm::vec2     ballTravelPath              = ballDirection * remainingTravelDistance;

        float t;

        glm::vec2 windowDimensions = level.getWindowDimensions();
        glm::vec2 playAreaCenter   = {windowDimensions.x * 0.5f, windowDimensions.y * 0.5f};
        glm::vec2 playAreaRect     = {windowDimensions.x - (2.0f * leftWallEdge), windowDimensions.y};

        // The walls
        glm::vec2 latestReflectedDirection = ballDirection;
        // Pass inverted ball dimensions so minkowski sum is calculcated correctly since the edges are being tested from the inside
        if (detectCollision(ballPosition, -ballScale, latestReflectedDirection, remainingTravelDistance, playAreaCenter, playAreaRect, t)) {
            // If bottom edge is hit
            if (ballDirection.y != latestReflectedDirection.y && latestReflectedDirection.y < 0.0f) {
                return LevelState::LOST;
            }

            minimalT                    = t < minimalT ? t : minimalT;
            reflectedDirectionOfClosest = latestReflectedDirection;
            collisionData.type          = CollisionType::WALL;
        }

        // The pad
        if (detectCollision(ballPosition, ballScale, latestReflectedDirection, remainingTravelDistance, padPosition, padScale, t)) {
            if (t < minimalT) {
                collisionData.type = CollisionType::PAD;
                if (ballDirection.y == -latestReflectedDirection.y) {
                    float collisionPoint        = (ballPosition + ballTravelPath * (ballRadius + t)).x;
                    float padLeftCorner         = padPosition.x - 0.5f * padScale.x;
                    float hitScale              = (collisionPoint - padLeftCorner) / padScale.x;
                    hitScale                    = hitScale * 2.0f - 1.0f;
                    reflectedDirectionOfClosest = glm::normalize(glm::vec2(hitScale * 1.0f, -1.0f));
                } else {
                    reflectedDirectionOfClosest = latestReflectedDirection;
                }
            }
        }

        // The bricks
        size_t          hitIndex   = UINT32_MAX;
        const uint32_t& brickCount = level.getTotalBrickCount();
        for (size_t i = 0; i < brickCount; ++i) {
            if (bricks[i].health > 0 &&
                detectCollision(ballPosition, ballScale, latestReflectedDirection, remainingTravelDistance, bricks[i].position, bricks[i].scale, t)) {

                minimalT                    = t < minimalT ? t : minimalT;
                reflectedDirectionOfClosest = latestReflectedDirection;
                hitIndex                    = i;
            }
        }

        if (hitIndex != UINT32_MAX) {
            collisionData.type = CollisionType::BRICK;
        }

        minimalT = minimalT > 1.0f ? 1.0f : minimalT;

        // If anything is hit
        if (ballDirection != reflectedDirectionOfClosest) {
            minimalT -= EPSILON;
            ballDirection = reflectedDirectionOfClosest;
        }

        ballPosition += ballTravelPath * minimalT;
        float distanceTraveled = remainingTravelDistance * minimalT;
        remainingTravelDistance -= distanceTraveled;

        if (collisionData.type == CollisionType::BRICK) {
            collisionData.hitIndex      = hitIndex;
            collisionData.collisionTime = distanceTraveled / ballSpeed;
        }

        if (collisionData.type != CollisionType::NONE) {
            collisionInfo.push_back(collisionData);
        }
    }

    return LevelState::STILL_ALIVE;
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
