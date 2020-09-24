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

class TextureManager {
  public:
    TextureManager(Renderer* const renderer);
    ~TextureManager();

    const uint32_t                             getTextureId(const std::string& texturePath, const float& scale = 1.0f);
    const std::vector<std::unique_ptr<Image>>& getTextureArray() const;

  private:
    Renderer*                           m_renderer;
    std::map<std::string, uint32_t>     m_textureMap;
    std::vector<std::unique_ptr<Image>> m_textures;

    const uint32_t loadTexture(const std::string& pathToTexture, const float& scale = 1.0f);
};
