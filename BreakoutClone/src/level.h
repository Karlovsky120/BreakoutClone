#pragma once

#include "common.h"
#include "sharedStructures.h"

#include "commonExternal.h"

// Smaller the number, stronger the blur
#define SIDE_BLUR_STRENGTH (1.0f / 16.0f)

#define LEVEL_FOLDER "//resources//levels//"

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

#define PAD_SPEED_FACTOR  0.00000025f
#define BALL_SPEED_FACTOR 0.00000025f

#define DEPTH_UI         0.2f
#define DEPTH_FOREGROUND 0.4f
#define DEPTH_GAME       0.6f
#define DEPTH_BACKGROUND 0.8f

struct BrickType {
    uint32_t    id             = 0;
    uint32_t    hitPoints      = 0;
    int32_t     breakScore     = 0;
    std::string texturePath    = "";
    std::string hitSoundPath   = "";
    std::string breakSoundPath = "";
};

struct DynamicLevelData {
    uint32_t              remainingBrickCount = 0;
    glm::vec2             ballDirection       = {0.0f, 0.0f};
    std::vector<Instance> instances;
    bool                  commandBufferRecorded = false;
};

struct Buffer;
class Renderer;
class TextureManager;

class Level {
  public:
    Level(const char* levelPath, const uint32_t& levelIndex, const uint32_t& windowWidth, const uint32_t& windowHeight, Renderer* const renderer,
          TextureManager* const textureManager);
    ~Level();

    void load(const uint32_t& lifeCount, const uint32_t& score, const uint32_t& levelIndex);

    void updateGPUData() const;

    void setForegroundVisibility(const float& alpha);
    void setTitleVisibility(const float& alpha);
    void setTitle(const std::string& texturePath);
    void setSubtitleVisibility(const float& alpha);
    void setSubtitle(const std::string& textureId);

    void setScore(const uint32_t& score);
    void setLifeCount(const uint32_t& lifeCount);

    std::vector<Instance>& getInstances();
    const BrickType&       getBrickData(const uint32_t& id) const;
    const uint32_t&        getRemainingBrickCount() const;
    const uint32_t&        getTotalBrickCount() const;
    Instance* const        getBricksPtr();
    const uint32_t&        destroyBrick();

    const glm::vec2 getWindowDimensions() const;

    const glm::vec2 getStartingBallDirection() const;
    const float&    getBasePadSpeed() const;
    const float&    getBaseBallSpeed() const;
    void            resetPadAndBall();

  private:
    std::string m_backgroundTexturePath = "";

    DynamicLevelData m_backup;
    DynamicLevelData m_inUse;

    uint32_t m_rowCount;
    uint32_t m_columnCount;
    uint32_t m_rowSpacing;
    uint32_t m_columnSpacing;

    uint32_t m_levelIndex      = 0;
    uint32_t m_totalBrickCount = 0;

    uint32_t m_windowWidth  = 0;
    uint32_t m_windowHeight = 0;

    uint32_t m_foregroundIndex = 0;
    uint32_t m_titleIndex;
    uint32_t m_subtitleIndex;
    uint32_t m_levelLabelIndex;
    uint32_t m_levelCountStartIndex;
    uint32_t m_livesLabelIndex;
    uint32_t m_livesCountStartIndex;
    uint32_t m_scoreLabelIndex;
    uint32_t m_scoreCountStartIndex;

    glm::vec2 m_padInitialPosition;
    glm::vec2 m_ballInitialPosition;

    float m_playAreaWidth;
    float m_wallWidth;

    float m_basePadSpeed;
    float m_baseBallSpeed;

    std::vector<std::vector<uint32_t>> m_levelLayout;

    std::map<uint32_t, BrickType> m_brickTypes;

    Renderer* const       m_renderer;
    TextureManager* const m_textureManager;

    std::unique_ptr<Buffer> m_instanceBuffer;
    uint32_t                m_instanceDataBufferSize;

    void setNumber(const uint32_t& instanceIndex, const uint32_t& digitCount, uint32_t number);

    void parseXml(const char* fullPath);
    void generateRenderData();
};
