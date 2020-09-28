#pragma once

#include "common.h"
#include "commonExternal.h"

#define TEXTURE_FOLDER "\\resources\\textures\\"

#define TEXTURE_PAD        "pad.png"
#define TEXTURE_BALL       "ball.png"
#define TEXTURE_CRACKS     "bricks\\cracks.png"
#define TEXTURE_FOREGROUND "boards\\foreground.png"

#define TEXTURE_UI_NUMBER(number) "ui\\" + std::to_string(number) + ".png"

#define TEXTURE_UI_VICTORY        "ui\\victory.png"
#define TEXTURE_UI_GAME_OVER      "ui\\gameOver.png"
#define TEXTURE_UI_LOADING_LEVEL  "ui\\loadingLevel.png"
#define TEXTURE_UI_LEVEL_COMPLETE "ui\\levelComplete.png"

#define TEXTURE_UI_LEVEL "ui\\level.png"
#define TEXTURE_UI_LIVES "ui\\lives.png"
#define TEXTURE_UI_SCORE "ui\\score.png"

#define TEXTURE_UI_TRY     "ui\\pressSpaceToTryAgain.png"
#define TEXTURE_UI_RELEASE "ui\\pressSpaceToReleaseTheBall.png"

class Renderer;
struct Image;

/// <summary>
/// Class used to load and manage texture.
/// </summary>
class TextureManager {
  public:
    /// <summary>
    /// Creates texture manager.
    /// </summary>
    /// <param name="renderer">Pointer to the global renderer.</param>
    TextureManager(Renderer* const renderer);
    ~TextureManager();

    /// <summary>
    /// Returns the texture id of the requested texture, at the requested scale. If the texture is not yet loaded, loads it.
    /// </summary>
    /// <param name="textureId">Path to the texture relative to the texturs folder.</param>
    /// <param name="scale">Scale of the texture.</param>
    /// <returns>Id of the requested texture.</returns>
    const uint32_t getTextureId(const std::string& textureId, const float& scale = 1.0f);

    /// <summary>
    /// Returns the vector of texture for use with the renderer.
    /// </summary>
    /// <returns>The vector of textures.</returns>
    const std::vector<std::unique_ptr<Image>>& getTextureArray() const;

  private:
    /// <summary>
    /// pointer to global renderer.
    /// </summary>
    Renderer* m_renderer;

    /// <summary>
    /// Map of all loaded textures.
    /// </summary>
    std::map<std::string, uint32_t> m_textureMap;

    /// <summary>
    /// Images holding the textures.
    /// </summary>
    std::vector<std::unique_ptr<Image>> m_textures;

    /// <summary>
    /// Loads the texture.
    /// </summary>
    /// <param name="pathToTexture">Path to the texture relative to the textures folder.</param>
    /// <param name="scale">Scale at which the texture is to be loaded.</param>
    /// <returns>The id of loaded texture.</returns>
    const uint32_t loadTexture(const std::string& pathToTexture, const float& scale = 1.0f);
};
