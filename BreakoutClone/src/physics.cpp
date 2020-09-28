#include "physics.h"
#include "level.h"

#include <algorithm>

#define CROSS2D(first, second) ((first).x * (second).y - (first).y * (second).x)
#define SIGNUM(x)              ((x > 0.0) - (x < 0.0))

LevelState Physics::resolveFrame(const uint32_t& frameTime /*microseconds*/, Level& level, const float& ballSpeedModifier /*pixels per microsecond*/,
                                 const float& padSpeedModifier /*pixels per microsecond*/, glm::vec2& ballDirection,
                                 std::vector<CollisionData>& collisionInfo) {
    std::vector<Instance>& instances = level.getInstances();

    glm::vec2& ballPosition = instances[level.getBallIndex()].position;
    glm::vec2& ballScale    = instances[level.getBallIndex()].scale;
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
        bool  padOutsideBall = (padScale.y * 0.5f + ballScale.y * 0.5f < std::abs(padPosition.y - ballPosition.y)) ||
                              (padScale.x * 0.5f + ballScale.x * 0.5f < std::abs(padPosition.x - ballPosition.x));
        glm::vec2 dummy;
        if (padOutsideBall && !rectRectCollisionDynamic(padPosition, padScale, {SIGNUM(paddP), 0.0f}, std::abs(paddP), ballPosition, ballScale, t, dummy)) {
            float mostLeft  = leftWallEdge + padScale.x * 0.5f;
            float mostRight = rightWallEdge - padScale.x * 0.5f;

            padPosition.x = std::clamp(padPosition.x + paddP, mostLeft, mostRight);
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
        if (segmentSegmentCollisionStatic(ballPosition, ballTravelPath, bottomLeftCorner, bottomRightCorner - bottomLeftCorner, t)) {
            return LevelState::LOST;
        }

        // Top wall
        if (segmentSegmentCollisionStatic(ballPosition, ballTravelPath, topLeftCorner, topRightCorner - topLeftCorner, t)) {
            if (t < minimalT) {
                minimalT                    = t;
                reflectedDirectionOfClosest = {ballDirection.x, -ballDirection.y};
                collisionData.type          = CollisionType::WALL;
            }
        }

        // Left wall
        if (segmentSegmentCollisionStatic(ballPosition, ballTravelPath, bottomLeftCorner, topLeftCorner - bottomLeftCorner, t)) {
            if (t < minimalT) {
                minimalT                    = t;
                reflectedDirectionOfClosest = {-ballDirection.x, ballDirection.y};
                collisionData.type          = CollisionType::WALL;
            }
        }

        // Right wall
        if (segmentSegmentCollisionStatic(ballPosition, ballTravelPath, bottomRightCorner, topRightCorner - bottomRightCorner, t)) {
            if (t < minimalT) {
                minimalT                    = t;
                reflectedDirectionOfClosest = {-ballDirection.x, ballDirection.y};
                collisionData.type          = CollisionType::WALL;
            }
        }

        // The pad
        glm::vec2 latestReflectedDirection;
        if (circleRectCollisionDynamic(ballPosition, ballRadius, ballDirection, remainingTravelDistance, padPosition, padScale, t, latestReflectedDirection)) {
            if (t < minimalT) {
                minimalT           = t;
                collisionData.type = CollisionType::PAD;
                if (ballDirection.y == -latestReflectedDirection.y) {
                    float collisionPoint        = (ballPosition + ballTravelPath * (ballRadius + t)).x;
                    float padLeftCorner         = padPosition.x - 0.5f * padScale.x + ballRadius;
                    float hitScale              = (collisionPoint - padLeftCorner) / (padScale.x - 2.0f * ballRadius);
                    hitScale                    = hitScale * 2.0f - 1.0f;
                    reflectedDirectionOfClosest = glm::normalize(glm::vec2(hitScale * 1.0f, -1.0f));
                } else {
                    reflectedDirectionOfClosest = latestReflectedDirection;
                }
            }
        }

        // The bricks
        uint32_t        hitBrickIndex = UINT32_MAX;
        const uint32_t& brickCount    = level.getTotalBrickCount();
        for (size_t i = 0; i < brickCount; ++i) {
            if (bricks[i].health > 0 && circleRectCollisionDynamic(ballPosition, ballRadius, ballDirection, remainingTravelDistance, bricks[i].position,
                                                                   bricks[i].scale, t, latestReflectedDirection)) {
                if (t < minimalT) {
                    minimalT                    = t;
                    reflectedDirectionOfClosest = latestReflectedDirection;
                    hitBrickIndex               = static_cast<uint32_t>(i);
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

        if (collisionData.type == CollisionType::BRICK) {
            collisionData.hitBrickIndex = hitBrickIndex;
        }

        if (collisionData.type != CollisionType::NONE) {
            collisionInfo.push_back(collisionData);
        }
    }

    return LevelState::STILL_ALIVE;
}

bool Physics::rectRectCollisionDynamic(const glm::vec2& travelingRectCenter, const glm::vec2& travelingRectDimensions,
                                       const glm::vec2& travelingRectNormalizedTravelDirection, const float& travelingRectDistanceTraveled,
                                       const glm::vec2& stationaryRectCenter, const glm::vec2& stationaryRectDimensions, float& t,
                                       glm::vec2& travelingRectNormalizedReflectedCollisionDirection) {

    glm::vec2 corner1 = travelingRectCenter + 0.5f * travelingRectDimensions;
    glm::vec2 corner2 = stationaryRectCenter + 0.5f * stationaryRectDimensions;

    float radius1 = glm::distance(travelingRectCenter, corner1);
    float radius2 = glm::distance(stationaryRectCenter, corner2);

    float rectDistance = glm::distance(travelingRectCenter, stationaryRectCenter);

    if (rectDistance > radius1 + travelingRectDistanceTraveled + radius2) {
        return false;
    }

    glm::vec2 minkowskiMax = stationaryRectCenter + stationaryRectDimensions * 0.5f + travelingRectDimensions * 0.5f;
    glm::vec2 minkowskiMin = stationaryRectCenter - stationaryRectDimensions * 0.5f - travelingRectDimensions * 0.5f;

    glm::vec2& topLeft     = minkowskiMin;
    glm::vec2  topRight    = {minkowskiMax.x, minkowskiMin.y};
    glm::vec2& bottomRight = minkowskiMax;
    glm::vec2  bottomLeft  = {minkowskiMin.x, minkowskiMax.y};

    t = 2.0f;

    float     edgeT;
    bool      collisionDetected = false;
    glm::vec2 rectVelocity1     = travelingRectNormalizedTravelDirection * travelingRectDistanceTraveled;

    // Top edge
    if (segmentSegmentCollisionStatic(travelingRectCenter, rectVelocity1, topLeft, topRight - topLeft, edgeT)) {
        if (edgeT < t) {
            t                                                  = edgeT;
            collisionDetected                                  = true;
            travelingRectNormalizedReflectedCollisionDirection = {travelingRectNormalizedTravelDirection.x, -travelingRectNormalizedTravelDirection.y};
        }
    }

    // Right edge
    if (segmentSegmentCollisionStatic(travelingRectCenter, rectVelocity1, topRight, bottomRight - topRight, edgeT)) {
        if (edgeT < t) {
            t                                                  = edgeT;
            collisionDetected                                  = true;
            travelingRectNormalizedReflectedCollisionDirection = {-travelingRectNormalizedTravelDirection.x, travelingRectNormalizedTravelDirection.y};
        }
    }

    // Bottom edge
    if (segmentSegmentCollisionStatic(travelingRectCenter, rectVelocity1, bottomRight, bottomLeft - bottomRight, edgeT)) {
        if (edgeT < t) {
            t                                                  = edgeT;
            collisionDetected                                  = true;
            travelingRectNormalizedReflectedCollisionDirection = {travelingRectNormalizedTravelDirection.x, -travelingRectNormalizedTravelDirection.y};
        }
    }

    // Left edge
    if (segmentSegmentCollisionStatic(travelingRectCenter, rectVelocity1, bottomLeft, topLeft - bottomLeft, edgeT)) {
        if (edgeT < t) {
            t                                                  = edgeT;
            collisionDetected                                  = true;
            travelingRectNormalizedReflectedCollisionDirection = {-travelingRectNormalizedTravelDirection.x, travelingRectNormalizedTravelDirection.y};
        }
    }

    return collisionDetected;
}

bool Physics::circleRectCollisionDynamic(const glm::vec2& circleCenter, const float& circleRadius, const glm::vec2& circleNormalizedTravelDirection,
                                         const float& circleDistanceTraveled, const glm::vec2& rectCenter, const glm::vec2& rectDimensions, float& t,
                                         glm::vec2& circleNormalizedReflectedCollisionDirection) {

    glm::vec2 rectCorner          = rectCenter + 0.5f * rectDimensions;
    float     rectExscribedRadius = glm::distance(rectCenter, rectCorner);

    float rectDistance = glm::distance(circleCenter, rectCenter);

    if (rectDistance > circleRadius + circleDistanceTraveled + rectExscribedRadius) {
        return false;
    }

    glm::vec2 max = rectCenter + rectDimensions * 0.5f;
    glm::vec2 min = rectCenter - rectDimensions * 0.5f;

    glm::vec2& topLeft     = min;
    glm::vec2  topRight    = {max.x, min.y};
    glm::vec2& bottomRight = max;
    glm::vec2  bottomLeft  = {min.x, max.y};

    glm::vec2 minkowskiMax = rectCenter + rectDimensions * 0.5f + circleRadius;
    glm::vec2 minkowskiMin = rectCenter - rectDimensions * 0.5f - circleRadius;

    glm::vec2 topLeftMinkowski     = {min.x, minkowskiMin.y};
    glm::vec2 topRightMinkowski    = {max.x, minkowskiMin.y};
    glm::vec2 leftTopMinkowski     = {minkowskiMin.x, min.y};
    glm::vec2 rightTopMinkowski    = {minkowskiMax.x, min.y};
    glm::vec2 bottomLeftMinkowski  = {min.x, minkowskiMax.y};
    glm::vec2 bottomRightMinkowski = {max.x, minkowskiMax.y};
    glm::vec2 leftBottomMinkowski  = {minkowskiMin.x, max.y};
    glm::vec2 rightBottomMinkowski = {minkowskiMax.x, max.y};

    t = 2.0f;

    float     partT;
    bool      collisionDetected = false;
    glm::vec2 ballVelocity      = circleNormalizedTravelDirection * circleDistanceTraveled;
    glm::vec2 reflectedDir;

    // Top edge
    if (segmentSegmentCollisionStatic(circleCenter, ballVelocity, topLeftMinkowski, topRightMinkowski - topLeftMinkowski, partT)) {
        if (partT < t) {
            t                                           = partT;
            collisionDetected                           = true;
            circleNormalizedReflectedCollisionDirection = {circleNormalizedTravelDirection.x, -circleNormalizedTravelDirection.y};
        }
    }

    // Right edge
    if (segmentSegmentCollisionStatic(circleCenter, ballVelocity, rightTopMinkowski, rightBottomMinkowski - rightTopMinkowski, partT)) {
        if (partT < t) {
            t                                           = partT;
            collisionDetected                           = true;
            circleNormalizedReflectedCollisionDirection = {-circleNormalizedTravelDirection.x, circleNormalizedTravelDirection.y};
        }
    }

    // Bottom edge
    if (segmentSegmentCollisionStatic(circleCenter, ballVelocity, bottomRightMinkowski, bottomLeftMinkowski - bottomRightMinkowski, partT)) {
        if (partT < t) {
            t                                           = partT;
            collisionDetected                           = true;
            circleNormalizedReflectedCollisionDirection = {circleNormalizedTravelDirection.x, -circleNormalizedTravelDirection.y};
        }
    }

    // Left edge
    if (segmentSegmentCollisionStatic(circleCenter, ballVelocity, leftBottomMinkowski, leftTopMinkowski - leftBottomMinkowski, partT)) {
        if (partT < t) {
            t                                           = partT;
            collisionDetected                           = true;
            circleNormalizedReflectedCollisionDirection = {-circleNormalizedTravelDirection.x, circleNormalizedTravelDirection.y};
        }
    }

    // Top left corner
    if (segmentCircleCollisionStatic(circleCenter, ballVelocity, topLeft, circleRadius, partT, reflectedDir)) {
        if (partT < t) {
            t                                           = partT;
            collisionDetected                           = true;
            circleNormalizedReflectedCollisionDirection = reflectedDir;
        }
    }

    // Top right corner
    if (segmentCircleCollisionStatic(circleCenter, ballVelocity, topRight, circleRadius, partT, reflectedDir)) {
        if (partT < t) {
            t                                           = partT;
            collisionDetected                           = true;
            circleNormalizedReflectedCollisionDirection = reflectedDir;
        }
    }

    // Bottom left corner
    if (segmentCircleCollisionStatic(circleCenter, ballVelocity, bottomLeft, circleRadius, partT, reflectedDir)) {
        if (partT < t) {
            t                                           = partT;
            collisionDetected                           = true;
            circleNormalizedReflectedCollisionDirection = reflectedDir;
        }
    }

    // Bottom right corner
    if (segmentCircleCollisionStatic(circleCenter, ballVelocity, bottomRight, circleRadius, partT, reflectedDir)) {
        if (partT < t) {
            t                                           = partT;
            collisionDetected                           = true;
            circleNormalizedReflectedCollisionDirection = reflectedDir;
        }
    }

    return collisionDetected;
}

bool Physics::segmentCircleCollisionStatic(const glm::vec2& segmentStart, const glm::vec2& segmentDir, const glm::vec2& circleCenter, const float& circleRadius,
                                           float& t, glm::vec2& reflectedDir) {
    glm::vec2 f = segmentStart - circleCenter;

    float a = glm::dot(segmentDir, segmentDir);
    float b = 2.0f * glm::dot(f, segmentDir);
    float c = glm::dot(f, f) - circleRadius * circleRadius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0) {
        return false;
    } else {
        discriminant = sqrt(discriminant);

        t = (-b - discriminant) / (2.0f * a);

        if (t >= 0.0f && t <= 1.0f) {
            glm::vec2 normal        = glm::normalize(segmentStart + segmentDir * t - circleCenter);
            glm::vec2 normalizedDir = glm::normalize(segmentDir);
            reflectedDir            = normalizedDir - 2.0f * glm::dot(normalizedDir, normal) * normal;
            return true;
        }

        return false;
    }
}

bool Physics::segmentSegmentCollisionStatic(const glm::vec2& firstSegmentStart, const glm::vec2 firstSegmentDirection, const glm::vec2& secondSegmentStart,
                                            const glm::vec2& secondSegmentDirection, float& t) {
    float directionsCross = CROSS2D(firstSegmentDirection, secondSegmentDirection);

    if (directionsCross == 0) {
        return false;
    }

    t       = CROSS2D(secondSegmentStart - firstSegmentStart, secondSegmentDirection) / directionsCross;
    float u = CROSS2D(secondSegmentStart - firstSegmentStart, firstSegmentDirection) / directionsCross;

    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        return true;
    }

    return false;
}
