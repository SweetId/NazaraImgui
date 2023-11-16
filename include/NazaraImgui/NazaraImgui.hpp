#pragma once

#include <Nazara/Core/ModuleBase.hpp>
#include <Nazara/Graphics/Graphics.hpp>
#include <Nazara/Math/Rect.hpp>
#include <NazaraImgui/Config.hpp>
#include <NazaraImgui/ImguiDrawer.hpp>

#include <imgui.h>
#include <unordered_set>

namespace Nz
{
    class Cursor;
    class RenderWindow;
    class Texture;
    class Window;
    class WindowEventHandler;

    struct ImguiHandler
    {
    public:
        virtual void OnRenderImgui() = 0;
    };

    class NAZARA_IMGUI_API Imgui : public Nz::ModuleBase<Imgui>
    {
        friend ModuleBase;

    public:
        using Dependencies = TypeList<Graphics>;
        struct Config;

        Imgui(Config config);
        ~Imgui();

        bool Init(Nz::Window& window, bool bLoadDefaultFont = true);
        void Update(Nz::Window& window, float dt);
        void Render(Nz::RenderTarget* renderTarget, Nz::RenderFrame& frame);

        inline ImguiDrawer& GetImguiDrawer() { return m_imguiDrawer; }
        inline const ImguiDrawer& GetImguiDrawer() const { return m_imguiDrawer; }

        // User-defined
        void AddHandler(ImguiHandler* handler);
        void RemoveHandler(ImguiHandler* handler);

        // Clipboard functions
        static void SetClipboardText(void* userData, const char* text);
        static const char* GetClipboardText(void* userData);

        struct Config
        {
            Nz::Vector2f framebufferSize;
        };

        static ImGuiContext* GetCurrentContext();
        static void GetAllocatorFunctions(ImGuiMemAllocFunc* allocFunc, ImGuiMemFreeFunc* freeFunc, void** userData);

    private:
        void RenderInternal();

        void SetupInputs(Nz::WindowEventHandler& handler);
        void Update(const Nz::Vector2i& mousePosition, const Nz::Vector2ui& displaySize, float dt);

        // Cursor functions
        std::shared_ptr<Nz::Cursor> GetMouseCursor(ImGuiMouseCursor cursorType);
        void UpdateMouseCursor(Nz::Window& window);

        void UpdateFontTexture();

        ImGuiContext* m_currentContext;
        std::string m_clipboardText;

        bool m_bWindowHasFocus;
        bool m_bMouseMoved;

        ImguiDrawer m_imguiDrawer;
        std::shared_ptr<Nz::Texture> m_fontTexture;
        std::unordered_set<ImguiHandler*> m_handlers;

        static Imgui* s_instance;

        friend class ImguiDrawer;
    };
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
    NAZARA_IMGUI_API bool ImageButton(const Nz::Texture* texture, const int framePadding = -1, const Nz::Color& bgColor = Nz::Color(0,0,0,0), const Nz::Color& tintColor = Nz::Color::White());
    NAZARA_IMGUI_API bool ImageButton(const Nz::Texture* texture, const Nz::Vector2f& size, const int framePadding = -1, const Nz::Color& bgColor = Nz::Color(0,0,0,0), const Nz::Color& tintColor = Nz::Color::White());

    // Draw_list overloads. All positions are in relative coordinates (relative to top-left of the current window)
    NAZARA_IMGUI_API void DrawLine(const Nz::Vector2f& a, const Nz::Vector2f& b, const Nz::Color& col, float thickness = 1.0f);
    NAZARA_IMGUI_API void DrawRect(const Nz::Rectf& rect, const Nz::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F, float thickness = 1.0f);
    NAZARA_IMGUI_API void DrawRectFilled(const Nz::Rectf& rect, const Nz::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F);

    inline void EnsureContextOnThisThread()
    {
        auto* context = Nz::Imgui::GetCurrentContext();
        SetCurrentContext(context);

        ImGuiMemAllocFunc allocFunc = nullptr;
        ImGuiMemFreeFunc freeFunc = nullptr;
        void* userData = nullptr;
        Nz::Imgui::GetAllocatorFunctions(&allocFunc, &freeFunc, &userData);
        SetAllocatorFunctions(allocFunc, freeFunc, userData);
    }
}
