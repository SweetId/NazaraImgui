#pragma once

#include <Nazara/Core/ModuleBase.hpp>
#include <Nazara/Graphics/Graphics.hpp>
#include <Nazara/Math/Rect.hpp>
#include <Nazara/Utils/TypeList.hpp>

#include "Config.h"

#include <imgui/imgui.h>

namespace Nz
{
    class Cursor;
    class EventHandler;
    class RenderFrame;
    class RenderTarget;
    class RenderWindow;
    class Texture;
    class Window;

    class Imgui : public Nz::ModuleBase<Imgui>
    {
        friend ModuleBase;

    public:
        using Dependencies = TypeList<Graphics>;
        struct Config;

        Imgui(Config config);
        ~Imgui();

        bool Init(Nz::Window& window, bool bLoadDefaultFont = true);
        void Update(Nz::Window& window, float dt);
        void Render(Nz::RenderWindow& window, Nz::RenderFrame& frame);

        // Clipboard functions
        static void SetClipboardText(void* userData, const char* text);
        static const char* GetClipboardText(void* userData);

        struct Config
        {
            Nz::Vector2f framebufferSize;
        };

    private:
        void SetupInputs(Nz::EventHandler& handler);
        void Update(const Nz::Vector2i& mousePosition, const Nz::Vector2ui& displaySize, float dt);

        // Cursor functions
        std::shared_ptr<Nz::Cursor> GetMouseCursor(ImGuiMouseCursor cursorType);
        void UpdateMouseCursor(Nz::Window& window);

        bool LoadTexturedPipeline();
        bool LoadUntexturedPipeline();
        void UpdateFontTexture();

        void RenderDrawLists(Nz::RenderWindow& window, Nz::RenderFrame& frame, ImDrawData* drawData);

        std::string m_clipboardText;

        bool m_bWindowHasFocus;
        bool m_bMouseMoved;

        struct
        {
            std::shared_ptr<Nz::RenderPipeline> pipeline;
            std::unordered_map<Nz::Texture*, Nz::ShaderBindingPtr> textureShaderBindings;
            Nz::ShaderBindingPtr uboShaderBinding;
            std::shared_ptr<Nz::TextureSampler> textureSampler;
        } m_texturedPipeline;

        struct
        {
            std::shared_ptr<Nz::RenderPipeline> pipeline;
            Nz::ShaderBindingPtr uboShaderBinding;
        } m_untexturedPipeline;

        std::shared_ptr<Nz::RenderBuffer> m_uboBuffer;
        std::shared_ptr<Nz::Texture> m_fontTexture;

        static Imgui* s_instance;
    };
}

namespace ImGui
{
    // custom ImGui widgets for SFML stuff

    // Image overloads
    void Image(const Nz::Texture* texture, const Nz::Color& tintColor = Nz::Color::White, const Nz::Color& borderColor = Nz::Color::Transparent);
    void Image(const Nz::Texture* texture, const Nz::Vector2f& size, const Nz::Color& tintColor = Nz::Color::White, const Nz::Color& borderColor = Nz::Color::Transparent);
    void Image(const Nz::Texture* texture, const Nz::Rectf& textureRect, const Nz::Color& tintColor = Nz::Color::White, const Nz::Color& borderColor = Nz::Color::Transparent);
    void Image(const Nz::Texture* texture, const Nz::Vector2f& size, const Nz::Rectf& textureRect, const Nz::Color& tintColor = Nz::Color::White, const Nz::Color& borderColor = Nz::Color::Transparent);

    // ImageButton overloads
    bool ImageButton(const Nz::Texture* texture, const int framePadding = -1, const Nz::Color& bgColor = Nz::Color::Transparent, const Nz::Color& tintColor = Nz::Color::White);
    bool ImageButton(const Nz::Texture* texture, const Nz::Vector2f& size, const int framePadding = -1, const Nz::Color& bgColor = Nz::Color::Transparent, const Nz::Color& tintColor = Nz::Color::White);

    // Draw_list overloads. All positions are in relative coordinates (relative to top-left of the current window)
    void DrawLine(const Nz::Vector2f& a, const Nz::Vector2f& b, const Nz::Color& col, float thickness = 1.0f);
    void DrawRect(const Nz::Rectf& rect, const Nz::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F, float thickness = 1.0f);
    void DrawRectFilled(const Nz::Rectf& rect, const Nz::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F);
}
