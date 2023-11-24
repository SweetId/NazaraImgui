#pragma once

#include <Nazara/Core/ModuleBase.hpp>
#include <Nazara/Graphics/Graphics.hpp>
#include <NazaraImgui/Config.hpp>
#include <NazaraImgui/ImguiDrawer.hpp>

#include <imgui.h>
#include <unordered_set>

namespace Nz
{
    class Cursor;
    class RenderTarget;
    class RenderWindow;
    class Swapchain;
    class Texture;
    class Window;
    class WindowEventHandler;

    struct ImguiHandler;

    class NAZARA_IMGUI_API Imgui : public Nz::ModuleBase<Imgui>
    {
        friend ModuleBase;

    public:
        using Dependencies = TypeList<Graphics>;
        struct Config;

        Imgui(Config config);
        ~Imgui();

        bool Init(Nz::Window& window, bool bLoadDefaultFont = true);
        void Update(float dt);
        void Render();
        void Render(Nz::Swapchain* renderTarget, Nz::RenderResources& frame);

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
        Nz::Window* m_window;

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
