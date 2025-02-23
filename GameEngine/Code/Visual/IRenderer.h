#pragma once

#include <string>

#include "Window.h"
#include "Utils/Vector.h"
#include "ModelInstanceBase.h"

namespace Engine::Visual
{
    class IRenderer
    {
    public:

        virtual void init(const Window& window) = 0;
        virtual void clearBackground(float r, float g, float b, float a) = 0;
        virtual void draw(
            const IModelInstance& model,
            const Utils::Vector3& position,
            const Utils::Vector3& rotation,
            const Utils::Vector3& scale) = 0;
        virtual void setCameraProperties(const Utils::Vector3& position, const Utils::Vector3& rotation) = 0;
        virtual void render() = 0;

        virtual bool loadModel(const std::string& filename) = 0;
        virtual bool loadTexture(const std::string& filename) = 0;
        virtual std::unique_ptr<IModelInstance> createModelInstance(const std::string& filename) = 0;

        virtual void destroyModelInstance(IModelInstance& modelInstance) = 0;
        virtual void unloadTexture(const std::string& filename) = 0;
        virtual void unloadModel(const std::string& filename) = 0;

        virtual void cleanUp() = 0;


        virtual ~IRenderer() = default;

    protected:
        static inline const std::string DEFAULT_TEXTURE = "../../Models/default.png";
    };

}