#pragma once

#include "common.h"
#include "commonExternal.h"

#define SECONDS_TO_MILISECONDS(seconds)           (seconds * 1'000)
#define SECONDS_TO_MICROSECONDS(seconds)          (seconds * 1'000'000)
#define MILISECONDS_TO_SECONDS(miliseconds)       (miliseconds * 0.001f;)
#define MICROSECONDS_TO_SECONDS(microseconds)     (microseconds * 0.000'001f)
#define MICROSECONDS_TO_MILISECONDS(microseconds) (microseconds * 0.001f)

#define EPSILON 0.01f

class Level;

enum class LevelState { STILL_ALIVE, HUGE_SUCCESS, LOST, CAKE };

enum class CollisionType { NONE, BRICK, PAD, WALL };

struct CollisionData {
    CollisionType type          = CollisionType::NONE;
    uint32_t      hitIndex      = 0;
    uint32_t      collisionTime = 0;
};

class Physics {
  public:
    LevelState resolveFrame(const uint32_t& frameTime /*microseconds*/, Level& level, const float& ballSpeedModifier /*pixels per microsecond*/,
                            const float& padSpeedModifier /*pixels per microsecond*/, glm::vec2& ballDirection, std::vector<CollisionData>& collisionInfo);

  private:
    bool detectSegmentsCollision(const glm::vec2& start1, const glm::vec2 dir1, const glm::vec2& start2, const glm::vec2& dir2, float& t);
    bool detectCollision(const glm::vec2& center1, const glm::vec2& rect1, glm::vec2& ballDirection, const float& ballSpeed, const glm::vec2& center2,
                         const glm::vec2& rect2, float& t);
};
