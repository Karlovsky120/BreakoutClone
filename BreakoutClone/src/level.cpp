#include "level.h"

#pragma warning(push, 0)         // This doesn't work for warnings below for some reason
#pragma warning(disable : 6011)  // Dereferencing NULL pointer *.
#pragma warning(disable : 6319)  // Use of the comma-operator in a tested expression causes the left argument to be ignored when it has no side-effects.
#pragma warning(disable : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value.
#pragma warning(disable : 26495) // Variable * is uninitialized.Always initialize a member variable.
#include "tinyxml2.cpp"
#include "tinyxml2.h"
#pragma warning(enable : 26495)
#pragma warning(enable : 26451)
#pragma warning(enable : 6319)
#pragma warning(enable : 6011)
#pragma warning(pop)

#include <map>
#include <stdexcept>

Level::Level(const char* fullPath) {
    tinyxml2::XMLDocument levelXml;

    if (levelXml.LoadFile(fullPath)) {
        char error[512];
        sprintf_s(error, "Failed to open file at location %s", fullPath);
        throw std::runtime_error(error);
    }

    const tinyxml2::XMLElement* levelData = levelXml.FirstChild()->ToElement();

    levelData->FindAttribute("RowCount")->QueryUnsignedValue(&rowCount);
    levelData->FindAttribute("ColumnCount")->QueryUnsignedValue(&columnCount);
    levelData->FindAttribute("RowSpacing")->QueryUnsignedValue(&rowSpacing);
    levelData->FindAttribute("ColumnSpacing")->QueryUnsignedValue(&columnSpacing);
    backgroundTexturePath = levelData->FindAttribute("BackgroundTexture")->Value();

    std::map<const char, uint32_t> idMap;
    uint32_t                       idCounter = 1000;

    const tinyxml2::XMLElement* brickTypesNode = levelData->FirstChildElement("BrickTypes");
    for (const tinyxml2::XMLElement* brickTypeElement = brickTypesNode->FirstChildElement(); brickTypeElement != NULL;
         brickTypeElement                             = brickTypeElement->NextSiblingElement()) {
        BrickType brick;

        const char* brickId = brickTypeElement->FindAttribute("Id")->Value();
        idMap[brickId[0]]   = idCounter;
        brick.id            = idCounter;
        ++idCounter;

        brick.texturePath     = brickTypeElement->FindAttribute("Texture")->Value();
        const char* hitPoints = brickTypeElement->FindAttribute("HitPoints")->Value();
        if (!strcmp(hitPoints, "Infinite")) {
            brick.hitPoints = 0;
        } else {
            brickTypeElement->FindAttribute("HitPoints")->QueryUnsignedValue(&brick.hitPoints);
        }

        brick.hitSoundPath = brickTypeElement->FindAttribute("HitSound")->Value();

        if (brickTypeElement->FindAttribute("BreakSound")) {
            brick.breakSoundPath = brickTypeElement->FindAttribute("BreakSound")->Value();
        }

        if (brickTypeElement->FindAttribute("BreakScore")) {
            brickTypeElement->FindAttribute("BreakScore")->QueryIntValue(&brick.breakScore);
        }

        brickTypes[brick.id] = brick;
    }

    const char* layoutData = levelData->FirstChildElement("Bricks")->GetText();

    uint32_t i = 0;
    levelLayout.push_back(std::vector<uint32_t>());
    levelState.push_back(std::vector<BrickState>());

    while (layoutData[i] != NULL) {
        if (layoutData[i] == '\n' && levelLayout[lastFilledRow].size() != 0) {
            levelLayout.push_back(std::vector<uint32_t>());
            levelState.push_back(std::vector<BrickState>());
            ++lastFilledRow;
        } else {
            auto id = idMap.find(layoutData[i]);
            if (id != idMap.end()) {
                levelLayout[lastFilledRow].push_back(id->second);
                levelState[lastFilledRow].push_back({id->second, brickTypes[id->second].hitPoints});
            }
        }
        ++i;
    }

    if (levelLayout[lastFilledRow].size() == 0) {
        levelLayout.pop_back();
        levelState.pop_back();
        --lastFilledRow;
    }
}
