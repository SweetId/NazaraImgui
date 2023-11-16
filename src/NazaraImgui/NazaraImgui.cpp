#include <NazaraImgui/NazaraImgui.hpp>
#include <NazaraImgui/ImguiPipelinePass.hpp>
#include <NazaraImgui/ImguiDrawer.hpp>

#include <Nazara/Core/DynLib.hpp>
#include <Nazara/Core/Log.hpp>
#include <Nazara/Platform/Cursor.hpp>
#include <Nazara/Platform/Clipboard.hpp>
#include <Nazara/Platform/Window.hpp>
#include <Nazara/Renderer/CommandBufferBuilder.hpp>
#include <Nazara/Renderer/Renderer.hpp>
#include <Nazara/Renderer/RenderTarget.hpp>
#include <Nazara/Renderer/RenderTarget.hpp>
#include <Nazara/Renderer/Texture.hpp>
#include <NZSL/Parser.hpp>

#include <cassert>
#include <cmath>    // abs
#include <cstddef>  // offsetof, NULL
#include <cstring>  // memcpy
#include <iostream>
#include <map>

#if __cplusplus >= 201103L  // C++11 and above
static_assert(sizeof(void*) <= sizeof(ImTextureID),
              "ImTextureID is not large enough to fit void* ptr.");
#endif

#ifdef NAZARAIMGUI_COMPILER_MSVC
#define NazaraImguiPrefix "./"
#else
#define NazaraImguiPrefix "./lib"
#endif

#ifdef NAZARAIMGUI_DEBUG
#define NazaraImguiDebugSuffix "-d"
#else
#define NazaraImguiDebugSuffix ""
#endif

namespace
{
    inline Nz::SystemCursor ToNz(ImGuiMouseCursor type)
    {
        switch (type)
        {
        case ImGuiMouseCursor_TextInput: return Nz::SystemCursor::Text;
        case ImGuiMouseCursor_Hand: return Nz::SystemCursor::Hand;
#if UNFINISHED_WORK
        case ImGuiMouseCursor_ResizeAll: return Nz::SystemCursor::SizeAll;
        case ImGuiMouseCursor_ResizeNS: return Nz::SystemCursor::SizeVertical;
        case ImGuiMouseCursor_ResizeEW: return Nz::SystemCursor::SizeHorizontal;
        case ImGuiMouseCursor_ResizeNESW: return Nz::SystemCursor::SizeBottomLeftTopRight;
        case ImGuiMouseCursor_ResizeNWSE: return Nz::SystemCursor::SizeTopLeftBottomRight;
#endif
        case ImGuiMouseCursor_Arrow:
        default:
            return Nz::SystemCursor::Default;

        }
    }
}


namespace Nz
{
    Imgui* Imgui::s_instance = nullptr;

    Imgui::Imgui(Config /*config*/)
        : ModuleBase("Imgui", this)
        , m_bMouseMoved(false)
        , m_bWindowHasFocus(false)
        , m_currentContext(nullptr)
        , m_imguiDrawer(*Nz::Graphics::Instance()->GetRenderDevice())
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.UserData = this;
    }

    Imgui::~Imgui()
    {
        ImGui::GetIO().Fonts->TexID = nullptr;
        ImGui::DestroyContext();

        m_fontTexture.reset();
    }

    bool Imgui::Init(Nz::Window& window, bool bLoadDefaultFont)
    {
        m_currentContext = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        // tell ImGui which features we support
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
        io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
        io.BackendPlatformName = "imgui_nazara";

        // init rendering
        io.DisplaySize = ImVec2(window.GetSize().x * 1.f, window.GetSize().y * 1.f);

        auto& registry = Nz::Graphics::Instance()->GetFramePipelinePassRegistry();
        registry.RegisterPass<ImguiPipelinePass>("Imgui", { "Input" }, { "Output" });

        if (bLoadDefaultFont)
        {
            UpdateFontTexture();
        }

        SetupInputs(window.GetEventHandler());

        m_bWindowHasFocus = window.HasFocus();
        return true;
    }

    void Imgui::Update(Nz::Window& window, float dt)
    {
        // Update OS/hardware mouse cursor if imgui isn't drawing a software cursor
        UpdateMouseCursor(window);

        if (m_bMouseMoved)
        {
            Update(Nz::Mouse::GetPosition(window), window.GetSize(), dt);
        }
        else
        {
            Update({ 0,0 }, window.GetSize(), dt);
        }

#if UNFINISHED_WORK
        if (ImGui::GetIO().MouseDrawCursor) {
            // Hide OS mouse cursor if imgui is drawing it
            window.setMouseCursorVisible(false);
        }
#endif
    }

    void Imgui::SetupInputs(Nz::WindowEventHandler& handler)
    {
        ImGuiIO& io = ImGui::GetIO();

        // init keyboard mapping
        io.KeyMap[ImGuiKey_Tab] = (int)Nz::Keyboard::Scancode::Tab;
        io.KeyMap[ImGuiKey_LeftArrow] = (int)Nz::Keyboard::Scancode::Left;
        io.KeyMap[ImGuiKey_RightArrow] = (int)Nz::Keyboard::Scancode::Right;
        io.KeyMap[ImGuiKey_UpArrow] = (int)Nz::Keyboard::Scancode::Up;
        io.KeyMap[ImGuiKey_DownArrow] = (int)Nz::Keyboard::Scancode::Down;
        io.KeyMap[ImGuiKey_PageUp] = (int)Nz::Keyboard::Scancode::PageUp;
        io.KeyMap[ImGuiKey_PageDown] = (int)Nz::Keyboard::Scancode::PageDown;
        io.KeyMap[ImGuiKey_Home] = (int)Nz::Keyboard::Scancode::Home;
        io.KeyMap[ImGuiKey_End] = (int)Nz::Keyboard::Scancode::End;
        io.KeyMap[ImGuiKey_Insert] = (int)Nz::Keyboard::Scancode::Insert;
        io.KeyMap[ImGuiKey_Delete] = (int)Nz::Keyboard::Scancode::Delete;
        io.KeyMap[ImGuiKey_Backspace] = (int)Nz::Keyboard::Scancode::Backspace;
        io.KeyMap[ImGuiKey_Space] = (int)Nz::Keyboard::Scancode::Space;
        io.KeyMap[ImGuiKey_Enter] = (int)Nz::Keyboard::Scancode::Return;
        io.KeyMap[ImGuiKey_Escape] = (int)Nz::Keyboard::Scancode::Escape;
        io.KeyMap[ImGuiKey_A] = (int)Nz::Keyboard::Scancode::A;
        io.KeyMap[ImGuiKey_C] = (int)Nz::Keyboard::Scancode::C;
        io.KeyMap[ImGuiKey_V] = (int)Nz::Keyboard::Scancode::V;
        io.KeyMap[ImGuiKey_X] = (int)Nz::Keyboard::Scancode::X;
        io.KeyMap[ImGuiKey_Y] = (int)Nz::Keyboard::Scancode::Y;
        io.KeyMap[ImGuiKey_Z] = (int)Nz::Keyboard::Scancode::Z;

        // Setup event handler
        handler.OnMouseMoved.Connect([this](const Nz::WindowEventHandler*, const Nz::WindowEvent::MouseMoveEvent&) {
            if (!m_bWindowHasFocus)
                return;

            m_bMouseMoved = true;
        });

        handler.OnMouseButtonPressed.Connect([this](const Nz::WindowEventHandler*, const Nz::WindowEvent::MouseButtonEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            if (event.button >= Nz::Mouse::Button::Left
                && event.button <= Nz::Mouse::Button::Right)
            {
                ImGuiIO& io = ImGui::GetIO();
                io.MouseDown[event.button] = true;
            }
        });

        handler.OnMouseButtonReleased.Connect([this](const Nz::WindowEventHandler*, const Nz::WindowEvent::MouseButtonEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            if (event.button >= Nz::Mouse::Button::Left
                && event.button <= Nz::Mouse::Button::Right)
            {
                ImGuiIO& io = ImGui::GetIO();
                io.MouseDown[event.button] = false;
            }
        });

        handler.OnMouseWheelMoved.Connect([this](const Nz::WindowEventHandler*, const Nz::WindowEvent::MouseWheelEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            ImGuiIO& io = ImGui::GetIO();
            io.MouseWheel += event.delta;
        });

        handler.OnKeyPressed.Connect([this](const Nz::WindowEventHandler*, const Nz::WindowEvent::KeyEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            ImGuiIO& io = ImGui::GetIO();
            io.KeysDown[(int)event.scancode] = true;
        });

        handler.OnKeyReleased.Connect([this](const Nz::WindowEventHandler*, const Nz::WindowEvent::KeyEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            ImGuiIO& io = ImGui::GetIO();
            io.KeysDown[(int)event.scancode] = false;
        });

        handler.OnTextEntered.Connect([this](const Nz::WindowEventHandler*, const Nz::WindowEvent::TextEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            ImGuiIO& io = ImGui::GetIO();

            // Don't handle the event for unprintable characters
            if (event.character < ' ' || event.character == 127) {
                return;
            }

            io.AddInputCharacter(event.character);
        });

        handler.OnGainedFocus.Connect([this](const Nz::WindowEventHandler*) {
            m_bWindowHasFocus = true;
        });

        handler.OnLostFocus.Connect([this](const Nz::WindowEventHandler*) {
            m_bWindowHasFocus = false;
        });
    }

    void Imgui::Update(const Nz::Vector2i& mousePosition, const Nz::Vector2ui& displaySize, float dt)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(displaySize.x * 1.f, displaySize.y * 1.f);

        io.DeltaTime = dt / 1000.f;

        if (m_bWindowHasFocus) {
            if (io.WantSetMousePos) {
                Nz::Vector2i mousePos(static_cast<int>(io.MousePos.x),
                    static_cast<int>(io.MousePos.y));
                Nz::Mouse::SetPosition(mousePos);
            }
            else {
                io.MousePos = ImVec2(mousePosition.x * 1.f, mousePosition.y * 1.f);
            }
        }

        // Update Ctrl, Shift, Alt, Super state
        io.KeyCtrl = io.KeysDown[(int)Nz::Keyboard::Scancode::LControl] || io.KeysDown[(int)Nz::Keyboard::Scancode::RControl];
        io.KeyAlt = io.KeysDown[(int)Nz::Keyboard::Scancode::LAlt] || io.KeysDown[(int)Nz::Keyboard::Scancode::RAlt];
        io.KeyShift = io.KeysDown[(int)Nz::Keyboard::Scancode::LShift] || io.KeysDown[(int)Nz::Keyboard::Scancode::RShift];
        io.KeySuper = io.KeysDown[(int)Nz::Keyboard::Scancode::LSystem] || io.KeysDown[(int)Nz::Keyboard::Scancode::RSystem];

        assert(io.Fonts->Fonts.Size > 0);  // You forgot to create and set up font
        // atlas (see createFontTexture)

        ImGui::NewFrame();
    }

    void Imgui::Render(Nz::RenderTarget* renderTarget, Nz::RenderFrame& frame)
    {
        m_imguiDrawer.Prepare(frame);

        frame.Execute([this, renderTarget, &frame](Nz::CommandBufferBuilder& builder) {

            ImGuiIO& io = ImGui::GetIO();
            int fb_width = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
            int fb_height = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
            Nz::Recti renderRect(0, 0, fb_width, fb_height);

            builder.BeginDebugRegion("ImGui", Nz::Color::Green());
            builder.BeginRenderPass(renderTarget->GetFramebuffer(frame.GetFramebufferIndex()), renderTarget->GetRenderPass(), renderRect);
            m_imguiDrawer.Draw(builder);
            builder.EndRenderPass();
            builder.EndDebugRegion();

        }, Nz::QueueType::Graphics);
    }

    void Imgui::RenderInternal()
    {
        for (auto* handler : m_handlers)
            handler->OnRenderImgui();

        ImGui::Render();
    }

    void Imgui::AddHandler(ImguiHandler* handler)
    {
        m_handlers.insert(handler);
    }

    void Imgui::RemoveHandler(ImguiHandler* handler)
    {
        m_handlers.erase(handler);
    }

    void Imgui::SetClipboardText(void* userData, const char* text)
    {
        Imgui* backend = static_cast<Imgui*>(userData);
        backend->m_clipboardText = text;

        Nz::Clipboard::SetString(text);
    }

    const char* Imgui::GetClipboardText(void* userData)
    {
        Imgui* backend = static_cast<Imgui*>(userData);
        backend->m_clipboardText = Nz::Clipboard::GetString();
        return backend->m_clipboardText.c_str();
    }

    ImGuiContext* Imgui::GetCurrentContext()
    {
        return ImGui::GetCurrentContext();
    }

    void Imgui::GetAllocatorFunctions(ImGuiMemAllocFunc* allocFunc, ImGuiMemFreeFunc* freeFunc, void** userData)
    {
        ImGui::GetAllocatorFunctions(allocFunc, freeFunc, userData);
    }

    void Imgui::UpdateFontTexture()
    {
        ImGuiIO& io = ImGui::GetIO();
        unsigned char* pixels;
        int width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        auto renderDevice = Nz::Graphics::Instance()->GetRenderDevice();
        Nz::TextureInfo texParams;
        texParams.width = width;
        texParams.height = height;
        texParams.pixelFormat = Nz::PixelFormat::RGBA8;
        texParams.type = Nz::ImageType::E2D;
        m_fontTexture = renderDevice->InstantiateTexture(texParams);
        m_fontTexture->Update(pixels, width, height);

        ImTextureID textureID = m_fontTexture.get();
        io.Fonts->TexID = textureID;
    }

    std::shared_ptr<Nz::Cursor> Imgui::GetMouseCursor(ImGuiMouseCursor cursorType)
    {
        return Nz::Cursor::Get(ToNz(cursorType));
    }

    void Imgui::UpdateMouseCursor(Nz::Window& window)
    {
        ImGuiIO& io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0) {
            ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
            if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None) {
#if UNFINISHED_WORK
                window.setMouseCursorVisible(false);
#endif
            }
            else {
#if UNFINISHED_WORK
                window.setMouseCursorVisible(true);
#endif

                std::shared_ptr<Nz::Cursor> c = GetMouseCursor(cursor);
                if (!c)
                    c = GetMouseCursor(ImGuiMouseCursor_Arrow);
                window.SetCursor(c);
            }
        }
    }
}

namespace
{
    namespace
    {
        ImColor toImColor(Nz::Color c)
        {
            return ImColor(c.r, c.g, c.b, c.a);
        }
        ImVec2 getTopLeftAbsolute(const Nz::Rectf& rect)
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            return ImVec2(rect.GetCorner(Nz::RectCorner::LeftTop) + Nz::Vector2f(pos.x, pos.y));
        }
        ImVec2 getDownRightAbsolute(const Nz::Rectf& rect)
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            return ImVec2(rect.GetCorner(Nz::RectCorner::RightBottom) + Nz::Vector2f(pos.x, pos.y));
        }

        bool imageButtonImpl(const Nz::Texture* texture, const Nz::Rectf& textureRect, const Nz::Vector2f& size, const int framePadding, const Nz::Color& bgColor, const Nz::Color& tintColor)
        {
            Nz::Vector2f textureSize(texture->GetSize().x * 1.f, texture->GetSize().y * 1.f);
            ImVec2 uv0(textureRect.GetCorner(Nz::RectCorner::LeftTop) / textureSize);
            ImVec2 uv1(textureRect.GetCorner(Nz::RectCorner::RightBottom) / textureSize);

            return ImGui::ImageButton((ImTextureID)texture, ImVec2(size.x, size.y), uv0, uv1, framePadding, toImColor(bgColor), toImColor(tintColor));
        }
    }  // end of anonymous namespace
}

namespace ImGui
{
    /////////////// Image Overloads
    void Image(const Nz::Texture* texture, const Nz::Color& tintColor, const Nz::Color& borderColor)
    {
        Image(texture, Nz::Vector2f(texture->GetSize().x * 1.f, texture->GetSize().y * 1.f), tintColor, borderColor);
    }

    void Image(const Nz::Texture* texture, const Nz::Vector2f& size, const Nz::Color& tintColor, const Nz::Color& borderColor)
    {
        ImGui::Image((ImTextureID)texture, ImVec2(size.x, size.y), ImVec2(0, 0), ImVec2(1, 1), toImColor(tintColor), toImColor(borderColor));
    }

    void Image(const Nz::Texture* texture, const Nz::Rectf& textureRect, const Nz::Color& tintColor, const Nz::Color& borderColor)
    {
        Image(texture, Nz::Vector2f(std::abs(textureRect.width), std::abs(textureRect.height)), textureRect, tintColor, borderColor);
    }

    void Image(const Nz::Texture* texture, const Nz::Vector2f& size, const Nz::Rectf& textureRect, const Nz::Color& tintColor, const Nz::Color& borderColor)
    {
        Nz::Vector2f textureSize(texture->GetSize().x * 1.f, texture->GetSize().y * 1.f);
        ImVec2 uv0(textureRect.GetCorner(Nz::RectCorner::LeftTop) / textureSize);
        ImVec2 uv1(textureRect.GetCorner(Nz::RectCorner::RightBottom) / textureSize);

        ImGui::Image((ImTextureID)texture, ImVec2(size.x, size.y), uv0, uv1, toImColor(tintColor), toImColor(borderColor));
    }

    /////////////// Image Button Overloads
    bool ImageButton(const Nz::Texture* texture, const int framePadding, const Nz::Color& bgColor, const Nz::Color& tintColor)
    {
        return ImageButton(texture, Nz::Vector2f(texture->GetSize().x * 1.f, texture->GetSize().y * 1.f), framePadding, bgColor, tintColor);
    }

    bool ImageButton(const Nz::Texture* texture, const Nz::Vector2f& size, const int framePadding, const Nz::Color& bgColor, const Nz::Color& tintColor)
    {
        Nz::Vector2f textureSize(texture->GetSize().x * 1.f, texture->GetSize().y * 1.f);
        return ::imageButtonImpl(texture, Nz::Rectf(0.f, 0.f, textureSize.x, textureSize.y), size, framePadding, bgColor, tintColor);
    }

    /////////////// Draw_list Overloads
    void DrawLine(const Nz::Vector2f& a, const Nz::Vector2f& b, const Nz::Color& color, float thickness)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        draw_list->AddLine(ImVec2(a.x + pos.x, a.y + pos.y), ImVec2(b.x + pos.x, b.y + pos.y), ColorConvertFloat4ToU32(toImColor(color)), thickness);
    }

    void DrawRect(const Nz::Rectf& rect, const Nz::Color& color, float rounding, int rounding_corners, float thickness)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(getTopLeftAbsolute(rect), getDownRightAbsolute(rect), ColorConvertFloat4ToU32(toImColor(color)), rounding, rounding_corners, thickness);
    }

    void DrawRectFilled(const Nz::Rectf& rect, const Nz::Color& color, float rounding, int rounding_corners)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(getTopLeftAbsolute(rect), getDownRightAbsolute(rect), ColorConvertFloat4ToU32(toImColor(color)), rounding, rounding_corners);
    }

}  // end of namespace ImGui
