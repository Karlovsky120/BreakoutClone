#pragma once
#include "physics.h"
#include "resources.h"
#include "sharedStructures.h"

#include "common.h"
#include "commonExternal.h"

// Smaller the number, stronger the blur
#define SIDE_BLUR_STRENGTH (1.0f / 16.0f)

#define MAX_COLUMN_COUNT 35
#define MAX_ROW_COUNT    30

#define MAX_COLUMN_SPACING 5
#define MAX_ROW_SPACING    5

#define BACKGROUND_INDEX  0
#define LEFT_WALL_INDEX   1
#define RIGHT_WALL_INDEX  2
#define PAD_INDEX         3
#define BALL_INDEX        4
#define BRICK_START_INDEX 5

#define LEVEL_COUNT_DIGITS 2
#define LIFE_COUNT_DIGITS  2
#define SCORE_COUNT_DIGITS 7

#define UI_TITLE_RATIO    8.0f
#define UI_SUBTITLE_RATIO 12.0f
#define UI_LABEL_RATIO    3.0f
#define UI_NUMBER_RATIO   1.0f

#define PAD_SPEED_FACTOR  0.5f
#define BALL_SPEED_FACTOR 0.5f

#define UI_DEPTH         0.2f
#define FOREGROUND_DEPTH 0.4f
#define GAME_DEPTH       0.6f
#define BACKGROUND_DEPTH 0.8f

struct BrickType {
    uint32_t    id             = 0;
    uint32_t    hitPoints      = 0;
    int32_t     breakScore     = 0;
    std::string texturePath    = "";
    std::string hitSoundPath   = "";
    std::string breakSoundPath = "";
};

class Level {
  public:
    void                   load(const uint32_t& lifeCount, const uint32_t& score, const uint32_t& levelIndex);
    void                   updateGPUData() const;
    std::vector<Instance>& getInstances();
    const glm::vec2        getWindowDimensions() const;
    const uint32_t&        getTotalBrickCount() const;
    const uint32_t&        getRemainingBrickCount() const;
    const float&           getBasePadSpeed() const;
    const float&           getBaseBallSpeed() const;
    const uint32_t&        destroyBrick();
    glm::vec2&             getBallDirection();
    Instance&              getForeground();

    const bool& isCommandBufferRecorded() const;
    void        updateCommandBuffers() const;

    void setForegroundVisibility(const float& alpha);
    void setTitleVisibility(const float& alpha);
    void setTitle(const std::string& texturePath);
    void setSubtitleVisibility(const float& alpha);
    void setSubtitle(const std::string& textureId);

    void setScore(const uint32_t& score);
    void setLifeCount(const uint32_t& lifeCount);

    void setHUDVisibility(const float& alpha);

    void resetPadAndBall();
    Level(const char* fullPath, const uint32_t& levelIndex, const uint32_t& windowWidth, const uint32_t& windowHeight);
    void destroy();

  private:
    std::string m_backgroundTexturePath = "";

    uint32_t m_rowCount      = 0;
    uint32_t m_columnCount   = 0;
    uint32_t m_rowSpacing    = 0;
    uint32_t m_columnSpacing = 0;

    uint32_t m_levelIndex          = 0;
    uint32_t m_totalBrickCount     = 0;
    uint32_t m_remainingBrickCount = 0;

    uint32_t m_windowWidth  = 0;
    uint32_t m_windowHeight = 0;

    uint32_t m_foregroundIndex = 0;

    uint32_t m_titleIndex    = 0;
    uint32_t m_subtitleIndex = 0;

    uint32_t m_levelLabelIndex      = 0;
    uint32_t m_levelCountStartIndex = 0;

    uint32_t m_livesLabelIndex      = 0;
    uint32_t m_livesCountStartIndex = 0;

    uint32_t m_scoreLabelIndex      = 0;
    uint32_t m_scoreCountStartIndex = 0;

    glm::vec2 m_padInitialPosition;
    glm::vec2 m_ballInitialPosition;

    float m_basePadSpeed;
    float m_baseBallSpeed;

    glm::vec2 m_ballDirection = {0.0f, 0.0f};

    std::vector<Instance>              m_instances;
    std::vector<std::vector<uint32_t>> m_levelLayout;

    std::map<uint32_t, BrickType> m_brickTypes;

    Buffer   m_instanceBuffer;
    uint32_t m_instanceDataBufferSize;

    bool m_commandBufferRecorded = false;

    void setNumber(const uint32_t& instanceIndex, const uint32_t& digitCount, uint32_t number);

    void parseXml(const char* fullPath);
    void generateRenderData();
};
