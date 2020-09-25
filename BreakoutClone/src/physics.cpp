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
        glm::vec2 dummy;
        if (detectCollision(padPosition, padScale, padMoveDirection, paddP, ballPosition, ballScale, t, dummy)) {
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

        // The walls
        glm::vec2 latestReflectedDirection = ballDirection;

        glm::vec2 windowDimensions = level.getWindowDimensions();
        float     left             = leftWallEdge + ballRadius;
        float     right            = rightWallEdge - ballRadius;
        float     top              = ballRadius;
        float     bottom           = windowDimensions.y + ballRadius;

        glm::vec2 topLeftCorner     = {left, top};
        glm::vec2 topRightCorner    = {right, top};
        glm::vec2 bottomLeftCorner  = {left, bottom};
        glm::vec2 bottomRightCorner = {right, bottom};

        // Bottom wall
        if (detectSegmentsCollision(ballPosition, ballTravelPath, bottomLeftCorner, bottomRightCorner - bottomLeftCorner, t)) {
            return LevelState::LOST;
        }

        // Top wall
        if (detectSegmentsCollision(ballPosition, ballTravelPath, topLeftCorner, topRightCorner - topLeftCorner, t)) {
            if (t < minimalT) {
                minimalT                    = t;
                reflectedDirectionOfClosest = {ballDirection.x, -ballDirection.y};
                collisionData.type          = CollisionType::WALL;
            }
        }

        // Left wall
        if (detectSegmentsCollision(ballPosition, ballTravelPath, bottomLeftCorner, topLeftCorner - bottomLeftCorner, t)) {
            if (t < minimalT) {
                minimalT                    = t;
                reflectedDirectionOfClosest = {-ballDirection.x, ballDirection.y};
                collisionData.type          = CollisionType::WALL;
            }
        }

        // Right wall
        if (detectSegmentsCollision(ballPosition, ballTravelPath, bottomRightCorner, topRightCorner - bottomRightCorner, t)) {
            if (t < minimalT) {
                minimalT                    = t;
                reflectedDirectionOfClosest = {-ballDirection.x, ballDirection.y};
                collisionData.type          = CollisionType::WALL;
            }
        }

        // The pad
        if (detectCollision(ballPosition, ballScale, ballDirection, remainingTravelDistance, padPosition, padScale, t, latestReflectedDirection)) {
            if (t < minimalT) {
                minimalT           = t;
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
            if (bricks[i].health > 0 && detectCollision(ballPosition, ballScale, ballDirection, remainingTravelDistance, bricks[i].position, bricks[i].scale, t,
                                                        latestReflectedDirection)) {
                if (t < minimalT) {
                    minimalT                    = t;
                    reflectedDirectionOfClosest = latestReflectedDirection;
                    hitIndex                    = i;
                    collisionData.type          = CollisionType::BRICK;
                }
            }
        }

        minimalT = minimalT < 1.0f ? minimalT : 1.0f;

        // If anything is hit
        if (ballDirection != reflectedDirectionOfClosest) {
            minimalT -= EPSILON;
            ballDirection = reflectedDirectionOfClosest;
        }

        ballPosition += ballTravelPath * minimalT;
        float distanceTraveled = remainingTravelDistance * minimalT;
        remainingTravelDistance -= distanceTraveled;

        switch (collisionData.type) {
            case CollisionType::BRICK: {
                collisionData.hitIndex = static_cast<uint32_t>(hitIndex);
            }
                [[fallthrough]];
            case CollisionType::PAD:
                [[fallthrough]];
            case CollisionType::WALL: {
                if (distanceTraveled < 0) {
                    collisionData.collisionTime = 0;
                } else {
                    collisionData.collisionTime = static_cast<uint32_t>(distanceTraveled / ballSpeed);
                }
                break;
            }
        }

        if (collisionData.type != CollisionType::NONE) {
            collisionInfo.push_back(collisionData);
        }
    }

    return LevelState::STILL_ALIVE;
}

bool Physics::detectCollision(const glm::vec2& center1, const glm::vec2& rect1, const glm::vec2& ballDirection, const float& ballSpeed,
                              const glm::vec2& center2, const glm::vec2& rect2, float& t, glm::vec2& reflectedDirection) {

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

    t = 2.0f;

    float     edgeT;
    bool      collisionDetected = false;
    glm::vec2 ballVelocity      = ballDirection * ballSpeed;

    // Top edge
    if (detectSegmentsCollision(center1, ballVelocity, topLeft, topRight - topLeft, edgeT)) {
        if (edgeT < t) {
            t                  = edgeT;
            collisionDetected  = true;
            reflectedDirection = {ballDirection.x, -ballDirection.y};
        }
    }

    // Right edge
    if (detectSegmentsCollision(center1, ballVelocity, topRight, bottomRight - topRight, edgeT)) {
        if (edgeT < t) {
            t                  = edgeT;
            collisionDetected  = true;
            reflectedDirection = {-ballDirection.x, ballDirection.y};
        }
    }

    // Bottom edge
    if (detectSegmentsCollision(center1, ballVelocity, bottomRight, bottomLeft - bottomRight, edgeT)) {
        if (edgeT < t) {
            t                  = edgeT;
            collisionDetected  = true;
            reflectedDirection = {ballDirection.x, -ballDirection.y};
        }
    }

    // Left edge
    if (detectSegmentsCollision(center1, ballVelocity, bottomLeft, topLeft - bottomLeft, edgeT)) {
        if (edgeT < t) {
            t                  = edgeT;
            collisionDetected  = true;
            reflectedDirection = {-ballDirection.x, ballDirection.y};
        }
    }

    return collisionDetected;
}

bool Physics::detectSegmentCircleCollision(const glm::vec2& segmentStart, const glm::vec2& segmentDir, const glm::vec2& circleCenter, const float& circleRadius,
                                           glm::vec2& reflectedDir) {
    glm::vec2 f = segmentStart - circleCenter;

    float a = glm::dot(segmentDir, segmentDir);
    float b = 2 * glm::dot(f, segmentDir);
    float c = glm::dot(f, f) - circleRadius * circleRadius;

    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0) {
        return false;
    } else {
        discriminant = sqrt(discriminant);

        float t = (-b - discriminant) / (2 * a);

        if (t >= 0 && t <= 1) {
            glm::vec2 normal        = glm::normalize(segmentDir * t - circleCenter);
            glm::vec2 normalizedDir = glm::normalize(segmentDir);
            reflectedDir            = normalizedDir - 2.0f * glm::dot(normalizedDir, normal) * normal;
            return true;
        }
    }
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
