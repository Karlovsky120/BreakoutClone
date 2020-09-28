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
#define BRICK_START_INDEX 4

#define LEVEL_COUNT_DIGITS 2
#define LIFE_COUNT_DIGITS  2
#define SCORE_COUNT_DIGITS 7

#define UI_TITLE_RATIO    8.0f
#define UI_SUBTITLE_RATIO 12.0f
#define UI_LABEL_RATIO    3.0f
#define UI_NUMBER_RATIO   1.0f

#define PAD_SPEED_FACTOR 0.00000025f
//#define BALL_SPEED_FACTOR 0.00000005f
#define BALL_SPEED_FACTOR 0.00000025f

#define DEPTH_UI         0.2f
#define DEPTH_FOREGROUND 0.4f
#define DEPTH_GAME       0.6f
#define DEPTH_BACKGROUND 0.8f

/// <summary>
/// Structure holding static data for a brick type.
/// </summary>
struct BrickType {

    /// <summary>
    /// Id of the brick.
    /// </summary>
    uint32_t id = 0;

    /// <summary>
    /// Max health of the brick
    /// </summary>
    uint32_t hitPoints = 0;

    /// <summary>
    /// Score awarded to player for breaking the block.
    /// </summary>
    int32_t breakScore = 0;

    /// <summary>
    /// Path to the texture file of the brick, relative to the textures folder.
    /// </summary>
    std::string texturePath = "";

    /// <summary>
    /// Path to the sound file for hitting the brick, relative to the sounds folder.
    /// </summary>
    std::string hitSoundPath = "";

    /// <summary>
    /// Path to the sound file for breaking the brick, relative to the sounds folder.
    /// </summary>
    std::string breakSoundPath = "";
};

/// <summary>
/// Structure holding data that needs to be reset when level is reset.
/// </summary>
struct DynamicLevelData {
    /// <summary>
    /// Amount of breakable bricks still unbroken.
    /// </summary>
    uint32_t remainingBrickCount = 0;

    /// <summary>
    /// Vector of instance data, containing info on background, side walls, pad, ball, every brick, foreground and UI.
    /// </summary>
    std::vector<Instance> instances;
};

struct Buffer;
class Renderer;
class TextureManager;

/// <summary>
/// Holds all information and state relevant to a particular level.
/// </summary>
class Level {
  public:
    /// <summary>
    /// Loads and parses the level xml, populates instances buffer for use on CPU and GPU, storing it in backup.
    /// </summary>
    /// <param name="levelPath">Full path to the level.</param>
    /// <param name="windowWidth">Width of the main window.</param>
    /// <param name="windowHeight">Height of the main window.</param>
    /// <param name="renderer">Pointer to the renderer.</param>
    /// <param name="textureManager">Pointer to the texture manager.</param>
    Level(const char* levelPath, const uint32_t& windowWidth, const uint32_t& windowHeight, Renderer* const renderer, TextureManager* const textureManager);

    /// <summary>
    /// Waits for the GPU to finish all work before allowing automatic destruction of instances buffer that may still be in use."
    ~Level();

    /// <summary>
    /// Copies backup of instances vector to be used, uploads the data to the GPU, sets up command buffers and sets the UI values.
    /// </summary>
    /// <param name="lifeCount">Number of lives to be displayed on the HUD.</param>
    /// <param name="score">Score to be displayed on the HUD.</param>
    /// <param name="levelIndex">Level ordinal number to be displayed on the HUD.</param>
    void load(const uint32_t& lifeCount, const uint32_t& score, const uint32_t& levelIndex);

    /// <summary>
    /// Uploads the current state of the instance vector to the GPU. It needs to be called to make any changes applied to the instance vector visible.
    /// </summary>
    void updateGPUData() const;

    /// <summary>
    /// Sets the alpha value for the rectangle covering the whole screen, allowing for fade in and fade out effects. The GPU is NOT updated.
    /// </summary>
    /// <param name="alpha">Value to be set to rectangle alpha.</param>
    void setForegroundVisibility(const float& alpha);

    /// <summary>
    /// Sets the alpha value for the title text, allowing for fade in and fade out effects. The GPU is NOT updated.
    /// </summary>
    /// <param name="alpha">Value to be set to title alpha.</param>
    void setTitleVisibility(const float& alpha);

    /// <summary>
    /// Sets the texture, and hence the text, of the title. The GPU is NOT updated.
    /// </summary>
    /// <param name="textureId">Path to the texture relative to the textures folder.</param>
    void setTitle(const std::string& textureId);

    /// <summary>
    /// Sets the alpha value for the subtitle text, allowing for fade in and fade out effects. The GPU is NOT updated.
    /// </summary>
    /// <param name="alpha">Value to be set to subtitle alpha.</param>
    void setSubtitleVisibility(const float& alpha);

    /// <summary>
    /// Sets the texture, and hence the text, of the subtitle. The GPU is NOT updated.
    /// </summary>
    /// <param name="textureId">Path to the texture relative to the textures folder.</param>
    void setSubtitle(const std::string& textureId);

    /// <summary>
    /// Sets the value of the score on the HUD. The GPU is NOT updated.
    /// </summary>
    /// <param name="score">Value of the score to be set on the HUD.</param>
    void setScore(const uint32_t& score);

    /// <summary>
    /// Sets the value of the score on the HUD. The GPU is NOT updated.
    /// </summary>
    /// <param name="score">Value of the score to be set on the HUD.</param>
    void setLifeCount(const uint32_t& lifeCount);

    /// <summary>
    /// Getter for instance vector currently in use.
    /// </summary>
    /// <returns>Currently used instance vector.</returns>
    std::vector<Instance>& getInstances();

    /// <summary>
    /// Returns the structure containing static data for the brick type.
    /// </summary>
    /// <param name="id">The id of the brick info is requested for.</param>
    /// <returns>The BrickType value for the brick with the provided id.</returns>
    const BrickType& getBrickData(const uint32_t& id) const;

    /// <summary>
    /// Returns the number of remaining destructable bricks in the level.
    /// </summary>
    /// <returns>Number of remaining destructable bricks.</returns>
    const uint32_t& getRemainingBrickCount() const;

    /// <summary>
    /// Returns the total number of bricks in the level.
    /// </summary>
    /// <returns>The total number of bricks.</returns>
    const uint32_t& getTotalBrickCount() const;

    /// <summary>
    /// Returns the pointer to the instance vector at the index of the first brick.
    /// </summary>
    /// <returns>The pointer to the instance vector at the index of the first brick.</returns>
    Instance* const getBricksPtr();

    /// <summary>
    /// Decrements the amount of remaining destructable bricks in the level.
    /// </summary>
    /// <returns>Number of remaining destructable bricks in the level.</returns>
    const uint32_t& destroyBrick();

    /// <summary>
    /// Returns the dimensions of the main window.
    /// </summary>
    /// <returns>The dimensions of the main window.</returns>
    const glm::vec2 getWindowDimensions() const;

    /// <summary>
    /// Getter for index of instance that holds ball information.
    /// </summary>
    /// <returns>Index of instance that holds ball information.</returns>
    const uint32_t& getBallIndex() const;

    /// <summary>
    /// Calculates the initial direction of the ball based on the pad position in the play area.
    /// </summary>
    /// <returns>The initial direction of the ball.</returns>
    const glm::vec2 getStartingBallDirection() const;

    /// <summary>
    /// Returns the base speed of the pad for this level.
    /// </summary>
    /// <returns>The base speed of the pad.</returns>
    const float& getBasePadSpeed() const;

    /// <summary>
    /// Returns the base speed of the ball for this level.
    /// </summary>
    /// <returns>The base speed of the ball.</returns>
    const float& getBaseBallSpeed() const;

    /// <summary>
    /// Resets the pad to the center of the screen and set the ball on top of it.
    /// </summary>
    void resetPadAndBall();

  private:
    /// <summary>
    /// Path to the background texture, relative to the textures folder.
    /// </summary>
    std::string m_backgroundTexturePath = "";

    /// <summary>
    /// Backup of the dynamic level data that is copied on level reset.
    /// </summary>
    DynamicLevelData m_backup;

    /// <summary>
    /// Dynamic level data currently in use.
    /// </summary>
    DynamicLevelData m_inUse;

    /// <summary>
    /// Number of brick rows in the level.
    /// </summary>
    uint32_t m_rowCount;

    /// <summary>
    /// Number of brick columns in the level.
    /// </summary>
    uint32_t m_columnCount;

    /// <summary>
    /// Spacing between the brick rows.
    /// </summary>
    uint32_t m_rowSpacing;

    /// <summary>
    /// Spacing between the brick columns.
    /// </summary>
    uint32_t m_columnSpacing;

    /// <summary>
    /// Total number of bricks in the level.
    /// </summary>
    uint32_t m_totalBrickCount;

    /// <summary>
    /// Width of the program window.
    /// </summary>
    uint32_t m_windowWidth;

    /// <summary>
    /// Height of the program window.
    /// </summary>
    uint32_t m_windowHeight;

    /// <summary>
    /// Instance index for instance that holds data for the ball.
    /// </summary>
    uint32_t m_ballIndex;

    /// <summary>
    /// Instance index for instance that holds data for the foreground.
    /// </summary>
    uint32_t m_foregroundIndex;

    /// <summary>
    /// Instance index for instance that holds data for the main title text.
    /// </summary>
    uint32_t m_titleIndex;

    /// <summary>
    /// Instance index for instance that holds data for the subtitle text.
    /// </summary>
    uint32_t m_subtitleIndex;

    /// <summary>
    /// Instance index for instance that holds data for the level count label.
    /// </summary>
    uint32_t m_levelLabelIndex;

    /// <summary>
    /// Instance start index for instance that holds data for the level count digits.
    /// </summary>
    uint32_t m_levelCountStartIndex;

    /// <summary>
    /// Instance index for instance that holds data for the lives count label.
    /// </summary>
    uint32_t m_livesLabelIndex;

    /// <summary>
    /// Instance index for instance that holds data for the lives count digits.
    /// </summary>
    uint32_t m_livesCountStartIndex;

    /// <summary>
    /// Instance index for instance that holds data for the score label.
    /// </summary>
    uint32_t m_scoreLabelIndex;

    /// <summary>
    /// Instance index for instance that holds data for the score digits.
    /// </summary>
    uint32_t m_scoreCountStartIndex;

    /// <summary>
    /// Initial position of the pad, centered on the play area.
    /// </summary>
    glm::vec2 m_padInitialPosition;

    /// <summary>
    /// Initial position of the ball, on the center of the pad.
    /// </summary>
    glm::vec2 m_ballInitialPosition;

    /// <summary>
    /// Width of the play area (window width reduced by the walls).
    /// </summary>
    float m_playAreaWidth;

    /// <summary>
    /// Width of one of the side walls.
    /// </summary>
    float m_wallWidth;

    /// <summary>
    /// Base speed of the pad, dependent on the play area width.
    /// </summary>
    float m_basePadSpeed;

    /// <summary>
    /// Base speed of the pad, dependent on the play area width.
    /// </summary>
    float m_baseBallSpeed;

    /// <summary>
    /// 2D vector of brick ids of the level, in their positions.
    /// </summary>
    std::vector<std::vector<uint32_t>> m_levelLayout;

    /// <summary>
    /// Map of all brick types and their ids.
    /// </summary>
    std::map<uint32_t, BrickType> m_brickTypes;

    /// <summary>
    ///  Vulkan buffer holding the data in the instance vector.
    /// </summary>
    std::unique_ptr<Buffer> m_instanceBuffer;

    /// <summary>
    /// Size of instance buffer in bytes.
    /// </summary>
    uint32_t m_instanceDataBufferSize;

    /// <summary>
    /// Pointer to global renderer.
    /// </summary>
    Renderer* const m_renderer;

    /// <summary>
    /// Pointe to global texture manager.
    /// </summary>
    TextureManager* const m_textureManager;

    /// <summary>
    /// Sets the number on the HUD. Only sets least important digitCount of digits.
    /// </summary>
    /// <param name="instanceIndex">Index of the starting number texture in the instance vector.</param>
    /// <param name="digitCount">Number of digits available to display the number.</param>
    /// <param name="number">Value to set the HUD number to.</param>
    void setNumber(const uint32_t& instanceIndex, const uint32_t& digitCount, uint32_t number);

    /// <summary>
    /// Loads the xml file containg level data and parses it
    /// </summary>
    /// <param name="fullLevelPath">Full path to the level xml file.</param>
    void parseXml(const char* fullLevelPath);

    /// <summary>
    /// Generates and populates instance vector based on data parsed from the xml.
    /// </summary>
    void generateRenderData();
};
