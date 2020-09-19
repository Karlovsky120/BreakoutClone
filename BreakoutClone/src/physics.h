#pragma once

#include "common.h"

#include "level.h"

#include <vector>

class Physics {
  public:
    static void resolveFrame(const uint32_t& frameTime /*microseconds*/, std::vector<Instance>& instances, const float& ballSpeed /*pixels per microsecond*/,
                             const float& padSpeedX /*pixels per microsecond*/, glm::vec2& ballDirection);

  private:
    static bool detectSegmentsCollision(const glm::vec2& start1, const glm::vec2 dir1, const glm::vec2& start2, const glm::vec2& dir2, float& t);
    static bool detectCollision(const glm::vec2& center1, const glm::vec2& rect1, const glm::vec2& moveVector, const glm::vec2& center2, const glm::vec2& rect2,
                                float& t);
};
