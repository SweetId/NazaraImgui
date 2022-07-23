#pragma once

#include <Nazara/Math/Rect.hpp>

#include "Config.h"

#include <imgui/imgui.h>

namespace Nz
{
    class Event;
    class RenderFrame;
    class RenderTarget;
    class RenderWindow;
    class Texture;
    class Window;
}

namespace ImGui
{
    namespace NZ
    {
        NAZARA_IMGUI_API void Init(Nz::RenderWindow& window, bool loadDefaultFont = true);
        NAZARA_IMGUI_API void Init(Nz::RenderWindow& window, Nz::RenderTarget& target, bool loadDefaultFont = true);
        NAZARA_IMGUI_API void Init(Nz::RenderWindow& window, const Nz::Vector2ui& displaySize, bool loadDefaultFont = true);

        NAZARA_IMGUI_API void ProcessEvent(const Nz::Event& event);

        NAZARA_IMGUI_API void Update(Nz::Window& window, float dt);
        NAZARA_IMGUI_API void Update(const Nz::Vector2i& mousePos, const Nz::Vector2ui& displaySize, float dt);

        NAZARA_IMGUI_API void Render(Nz::RenderWindow& window, Nz::RenderFrame& frame);

        NAZARA_IMGUI_API void Shutdown();
    }

    // custom ImGui widgets for SFML stuff

    // Image overloads
    /*void Image(const Nz::Texture& texture,
        const Nz::Color& tintColor = Nz::Color::White,
        const Nz::Color& borderColor = Nz::Color::Transparent);
    void Image(const Nz::Texture& texture, const Nz::Vector2f& size,
        const Nz::Color& tintColor = Nz::Color::White,
        const Nz::Color& borderColor = Nz::Color::Transparent);
    void Image(const Nz::Texture& texture, const Nz::Rectf& textureRect,
        const Nz::Color& tintColor = Nz::Color::White,
        const Nz::Color& borderColor = Nz::Color::Transparent);
    void Image(const Nz::Texture& texture, const Nz::Vector2f& size, const Nz::Rectf& textureRect,
        const Nz::Color& tintColor = Nz::Color::White,
        const Nz::Color& borderColor = Nz::Color::Transparent);

    // ImageButton overloads
    bool ImageButton(const Nz::Texture& texture, const int framePadding = -1,
        const Nz::Color& bgColor = Nz::Color::Transparent,
        const Nz::Color& tintColor = Nz::Color::White);
    bool ImageButton(const Nz::Texture& texture, const Nz::Vector2f& size, const int framePadding = -1,
        const Nz::Color& bgColor = Nz::Color::Transparent, const Nz::Color& tintColor = Nz::Color::White);*/

    // Draw_list overloads. All positions are in relative coordinates (relative to top-left of the current window)
    void DrawLine(const Nz::Vector2f& a, const Nz::Vector2f& b, const Nz::Color& col, float thickness = 1.0f);
    void DrawRect(const Nz::Rectf& rect, const Nz::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F, float thickness = 1.0f);
    void DrawRectFilled(const Nz::Rectf& rect, const Nz::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F);
}
