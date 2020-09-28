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

/// <summary>
/// State of the level after physics is resolved.
/// </summary>
enum class LevelState { STILL_ALIVE, HUGE_SUCCESS, LOST, CAKE };

/// <summary>
/// Object that the collision occured with.
/// </summary>
enum class CollisionType { NONE, BRICK, PAD, WALL };

/// <summary>
/// Structure holding information on a collision.
/// </summary>
struct CollisionData {
    /// <summary>
    /// Object collided with.
    /// </summary>
    CollisionType type = CollisionType::NONE;

    /// <summary>
    /// Index of the brick in the instance vector, only applicable for CollisionType::BRICK.
    /// </summary>
    uint32_t hitBrickIndex = 0;
};

/// <summary>
/// Class that resolves physics calculcations for a frame
/// </summary>
class Physics {
  public:
    /// <summary>
    /// Main and only public functions that calculates the trajectory of the pad and the ball for the current fame. First the pad is moved as much it can before
    /// touching the ball, then the ball is moved as much as it can in a frame.
    /// </summary>
    /// <param name="frameTime">Duration of the frame.</param>
    /// <param name="level">Currently active level.</param>
    /// <param name="ballSpeedModifier">Speed of the ball.</param>
    /// <param name="padSpeedModifier">Speed of the pad.</param>
    /// <param name="ballDirection">Direction of the ball, normalized. Method updates this value.</param>
    /// <param name="collisionInfo">Vector of CollisionData structures. Fills with any collisions ball encountered.</param>
    /// <returns>State of the level after physics is resolved.</returns>
    LevelState resolveFrame(const uint32_t& frameTime /*microseconds*/, Level& level, const float& ballSpeedModifier /*pixels per microsecond*/,
                            const float& padSpeedModifier /*pixels per microsecond*/, glm::vec2& ballDirection, std::vector<CollisionData>& collisionInfo);

  private:
    /// <summary>
    /// Calculates the intersection point between a lineary traveling rectangle and a stationary rectangle along with the resulting reflected vector, assuming
    /// an elastic collision.
    /// </summary>
    /// <param name="travelingRectCenter">Center of the traveling rectangle.</param>
    /// <param name="travelingRectDimensions">Dimensions of the traveling rectangle.</param>
    /// <param name="travelingRectNormalizedTravelDirection">Direction of the traveling rectangle</param>
    /// <param name="travelingRectDistanceTraveled">Distance traveled by the traveling rectangle.</param>
    /// <param name="stationaryRectCenter">Center of the stationary rectangle.</param>
    /// <param name="stationaryRectDimensions">Dimensions of the stationary rectangle.</param>
    /// <param name="t">Parameter of the point of collision on the traveling rect normalized travel direction multiplied with distance traveled, filled if
    /// collision is found.</param>
    /// <param name="travelingRectNormalizedReflectedCollisionDirection">Direction of the elastic reflection at the collision point,
    /// filled if the collision is found.</param>
    /// <returns>True if collision occurs, false otherwise. Also calculates the point of collision (as parameter of the path of traveling rectangle) and the
    /// reflected vector at the point of collision as the last two parameters.</returns>
    bool rectRectCollisionDynamic(const glm::vec2& travelingRectCenter, const glm::vec2& travelingRectDimensions,
                                  const glm::vec2& travelingRectNormalizedTravelDirection, const float& travelingRectDistanceTraveled,
                                  const glm::vec2& stationaryRectCenter, const glm::vec2& stationaryRectDimensions, float& t,
                                  glm::vec2& travelingRectNormalizedReflectedCollisionDirection);

    /// <summary>
    /// Calculates the intersection point between a lineary traveling circle and a stationary rectangle along with the resulting reflected vector, assuming an
    /// elastic collision.
    /// </summary>
    /// <param name="circleCenter">Center of the traveling circle.</param>
    /// <param name="circleRadius">Radius of the traveling circle.</param>
    /// <param name="circleNormalizedTravelDirection">Direction of the traveling circle.</param>
    /// <param name="circleDistanceTraveled">Distance traveled by the traveling rectangle</param>
    /// <param name="rectCenter">Center of the stationary rectangle.</param>
    /// <param name="rectDimensions">Dimensions of the stationary rectangle.</param>
    /// <param name="t">Parameter of the point of collision on the traveling circle normalized travel direction multiplied with distance traveled, filled if
    /// collision is found.</param>
    /// <param name="circleNormalizedReflectedCollisionDirection">Direction of the elastic reflection at the collision point, filled
    /// if the collision is found.</param>
    /// <returns>True if collision occurs, false otherwise. Also calculates the point of collision (as parameter of the path of traveling circle) and the
    /// reflected vector at the point of collision as the last two parameters.</returns>
    bool circleRectCollisionDynamic(const glm::vec2& circleCenter, const float& circleRadius, const glm::vec2& circleNormalizedTravelDirection,
                                    const float& circleDistanceTraveled, const glm::vec2& rectCenter, const glm::vec2& rectDimensions, float& t,
                                    glm::vec2& circleNormalizedReflectedCollisionDirection);
    /// <summary>
    /// Calculates the intersection point between a line segment and a circle.
    /// </summary>
    /// <param name="segmentStart">Start point of line segment.</param>
    /// <param name="segmentDir">Direction of the line segment multiplied with the length of the segment.</param>
    /// <param name="circleCenter">Center of the circle.</param>
    /// <param name="circleRadius">Radius of the circle.</param>
    /// <param name="t">Parameter of the point of collision on the segment direction multipled with the segment length, filled if the collision is
    /// found.</param>
    /// <param name="reflectedDir">Direction of the elastic reflection at the collision point, filled if the collision is found.</param>
    /// <returns>True if collision occurs, false otherwise. Also calculates the point of the collision (as a parameter of the line segment) and the
    /// reflected vector at the point of collision as the last two parameters.</returns>
    bool segmentCircleCollisionStatic(const glm::vec2& segmentStart, const glm::vec2& segmentDir, const glm::vec2& circleCenter, const float& circleRadius,
                                      float& t, glm::vec2& reflectedDir);

    /// <summary>
    /// Calculates the intersection point between two line segments.
    /// </summary>
    /// <param name="firstSegmentStart">Start point of the first line segment.</param>
    /// <param name="firstSegmentDirection">Direction of the first line segment multiplied with the length of the first line segment.</param>
    /// <param name="secondSegmentStart">Start point of the second line segment.</param>
    /// <param name="secondSegmentDirection">Direction of the second line segment multiplied with the length of the second line segment.</param>
    /// <param name="t">Parameter of the point of collision on the first line segment.</param>
    /// <returns>True if collision occurs, false otherwise. Also calculates the point of the collision as a parameter of the first line segment.</returns>
    bool segmentSegmentCollisionStatic(const glm::vec2& firstSegmentStart, const glm::vec2 firstSegmentDirection, const glm::vec2& secondSegmentStart,
                                       const glm::vec2& secondSegmentDirection, float& t);
};
