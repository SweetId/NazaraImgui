#pragma once

#include <NazaraImgui/Config.hpp>

#include <Nazara/Math/Rect.hpp>

namespace Nz
{
    class Texture;
}

namespace ImGui
{
    // custom ImGui widgets for Nazara

    // Image overloads
    NAZARA_IMGUI_API void Image(const Nz::Texture* texture, const Nz::Color& tintColor = Nz::Color::White(), const Nz::Color& borderColor = Nz::Color(0, 0, 0, 0));
    NAZARA_IMGUI_API void Image(const Nz::Texture* texture, const Nz::Vector2f& size, const Nz::Color& tintColor = Nz::Color::White(), const Nz::Color& borderColor = Nz::Color(0, 0, 0, 0));
    NAZARA_IMGUI_API void Image(const Nz::Texture* texture, const Nz::Rectf& textureRect, const Nz::Color& tintColor = Nz::Color::White(), const Nz::Color& borderColor = Nz::Color(0, 0, 0, 0));
    NAZARA_IMGUI_API void Image(const Nz::Texture* texture, const Nz::Vector2f& size, const Nz::Rectf& textureRect, const Nz::Color& tintColor = Nz::Color::White(), const Nz::Color& borderColor = Nz::Color(0, 0, 0, 0));

    // ImageButton overloads
    NAZARA_IMGUI_API bool ImageButton(const Nz::Texture* texture, const int framePadding = -1, const Nz::Color& bgColor = Nz::Color(0, 0, 0, 0), const Nz::Color& tintColor = Nz::Color::White());
    NAZARA_IMGUI_API bool ImageButton(const Nz::Texture* texture, const Nz::Vector2f& size, const int framePadding = -1, const Nz::Color& bgColor = Nz::Color(0, 0, 0, 0), const Nz::Color& tintColor = Nz::Color::White());

    // Draw_list overloads. All positions are in relative coordinates (relative to top-left of the current window)
    NAZARA_IMGUI_API void DrawLine(const Nz::Vector2f& a, const Nz::Vector2f& b, const Nz::Color& col, float thickness = 1.0f);
    NAZARA_IMGUI_API void DrawRect(const Nz::Rectf& rect, const Nz::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F, float thickness = 1.0f);
    NAZARA_IMGUI_API void DrawRectFilled(const Nz::Rectf& rect, const Nz::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F);
}