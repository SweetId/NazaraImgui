#include <NazaraImgui/NazaraImgui.h>

#include <Nazara/Core/DynLib.hpp>
#include <Nazara/Core/Log.hpp>
#include <Nazara/Graphics/Graphics.hpp>
#include <Nazara/Platform/Cursor.hpp>
#include <Nazara/Platform/Clipboard.hpp>
#include <Nazara/Platform/Event.hpp>
#include <Nazara/Platform/Window.hpp>
#include <Nazara/Renderer/CommandBufferBuilder.hpp>
#include <Nazara/Renderer/Renderer.hpp>
#include <Nazara/Renderer/RenderTarget.hpp>
#include <Nazara/Renderer/RenderWindow.hpp>
#include <Nazara/Renderer/Texture.hpp>
#include <NZSL/Parser.hpp>

#include <cassert>
#include <cmath>    // abs
#include <cstddef>  // offsetof, NULL
#include <cstring>  // memcpy
#include <iostream>
#include <map>

#if __cplusplus >= 201103L  // C++11 and above
static_assert(sizeof(GLuint) <= sizeof(ImTextureID),
              "ImTextureID is not large enough to fit GLuint.");
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

const char shaderSource_Textured[] =
#include "Textured.nzsl"
;

const char shaderSource_Untextured[] = 
#include "Untextured.nzsl"
;

namespace
{
    // data
    static bool s_windowHasFocus = false;
    static bool s_mouseMoved = false;

    // various helper functions
    ImColor toImColor(Nz::Color c);
    ImVec2 getTopLeftAbsolute(const Nz::Rectf& rect);
    ImVec2 getDownRightAbsolute(const Nz::Rectf& rect);

    void RenderDrawLists(Nz::RenderWindow& window, Nz::RenderFrame& frame, ImDrawData* draw_data);  // rendering callback function prototype

    // Implementation of ImageButton overload
    bool imageButtonImpl(const Nz::Texture& texture, const Nz::Rectf& textureRect, const Nz::Vector2f& size, const int framePadding, const Nz::Color& bgColor, const Nz::Color& tintColor);

    // clipboard functions
    void setClipboardText(void* userData, const char* text);
    const char* getClipboadText(void* userData);
    std::string s_clipboardText;

    // mouse cursors
    void loadMouseCursor(ImGuiMouseCursor imguiCursorType, Nz::SystemCursor nzCursorType);
    void updateMouseCursor(Nz::Window& window);

    std::shared_ptr<Nz::Cursor> s_mouseCursors[ImGuiMouseCursor_COUNT];
}

namespace Private
{
    static std::shared_ptr<Nz::Texture> FontTexture;

    static struct
    {
        std::shared_ptr<Nz::RenderPipeline> Pipeline;
        Nz::ShaderBindingPtr TextureShaderBinding;
        std::shared_ptr<Nz::TextureSampler> TextureSampler;
    } TexturedPipeline;

    static struct
    {
        std::shared_ptr<Nz::RenderPipeline> Pipeline;
    } UntexturedPipeline;
}

namespace ImGui {
namespace NZ {
    void UpdateFontTexture();

    bool LoadTexturedPipeline()
    {
        auto renderDevice = Nz::Graphics::Instance()->GetRenderDevice();

        nzsl::Ast::ModulePtr shaderModule = nzsl::Parse(std::string_view(shaderSource_Textured, sizeof(shaderSource_Textured)));
        if (!shaderModule)
        {
            std::cout << "Failed to parse shader module" << std::endl;
            return false;
        }

        nzsl::ShaderWriter::States states;
        states.optimize = true;

        auto shader = renderDevice->InstantiateShaderModule(nzsl::ShaderStageType::Fragment | nzsl::ShaderStageType::Vertex, *shaderModule, states);
        if (!shader)
        {
            std::cout << "Failed to instantiate shader" << std::endl;
            return false;
        }


        Private::TexturedPipeline.TextureSampler = renderDevice->InstantiateTextureSampler({});

        Nz::RenderPipelineLayoutInfo pipelineLayoutInfo;

        auto& textureBinding = pipelineLayoutInfo.bindings.emplace_back();
        textureBinding.setIndex = 1;
        textureBinding.bindingIndex = 0;
        textureBinding.shaderStageFlags = nzsl::ShaderStageType::Fragment;
        textureBinding.type = Nz::ShaderBindingType::Texture;

        std::shared_ptr<Nz::RenderPipelineLayout> renderPipelineLayout = renderDevice->InstantiateRenderPipelineLayout(std::move(pipelineLayoutInfo));
        Private::TexturedPipeline.TextureShaderBinding = renderPipelineLayout->AllocateShaderBinding(1);

        Nz::RenderPipelineInfo pipelineInfo;
        pipelineInfo.pipelineLayout = renderPipelineLayout;
        pipelineInfo.faceCulling = true;

        pipelineInfo.depthBuffer = true;
        pipelineInfo.shaderModules.emplace_back(shader);

        pipelineInfo.depthWrite = false;
        pipelineInfo.blending = true;
        pipelineInfo.depthCompare = Nz::RendererComparison::Always;
        pipelineInfo.blend.modeAlpha = Nz::BlendEquation::Max;
        pipelineInfo.blend.srcColor = Nz::BlendFunc::SrcAlpha;
        pipelineInfo.blend.dstColor = Nz::BlendFunc::InvSrcAlpha;
        pipelineInfo.blend.srcAlpha = Nz::BlendFunc::One;
        pipelineInfo.blend.dstAlpha = Nz::BlendFunc::Zero;
        pipelineInfo.faceCulling = false;
        pipelineInfo.scissorTest = true;

        auto& pipelineVertexBuffer = pipelineInfo.vertexBuffers.emplace_back();
        pipelineVertexBuffer.binding = 0;
        pipelineVertexBuffer.declaration = Nz::VertexDeclaration::Get(Nz::VertexLayout::XYZ_Color_UV);

        Private::TexturedPipeline.Pipeline = renderDevice->InstantiateRenderPipeline(pipelineInfo);
        return true;
    }

    bool LoadUntexturedPipeline()
    {
        auto renderDevice = Nz::Graphics::Instance()->GetRenderDevice();

        nzsl::Ast::ModulePtr shaderModule = nzsl::Parse(std::string_view(shaderSource_Untextured, sizeof(shaderSource_Untextured)));
        if (!shaderModule)
        {
            std::cout << "Failed to parse shader module" << std::endl;
            return false;
        }

        nzsl::ShaderWriter::States states;
        states.optimize = true;

        auto shader = renderDevice->InstantiateShaderModule(nzsl::ShaderStageType::Fragment | nzsl::ShaderStageType::Vertex, *shaderModule, states);
        if (!shader)
        {
            std::cout << "Failed to instantiate shader" << std::endl;
            return false;
        }

        Nz::RenderPipelineLayoutInfo pipelineLayoutInfo;
        std::shared_ptr<Nz::RenderPipelineLayout> renderPipelineLayout = renderDevice->InstantiateRenderPipelineLayout(std::move(pipelineLayoutInfo));

        Nz::RenderPipelineInfo pipelineInfo;
        pipelineInfo.pipelineLayout = renderPipelineLayout;
        pipelineInfo.faceCulling = true;

        pipelineInfo.depthBuffer = true;
        pipelineInfo.shaderModules.emplace_back(shader);

        pipelineInfo.depthWrite = false;
        pipelineInfo.blending = true;
        pipelineInfo.depthCompare = Nz::RendererComparison::Always;
        pipelineInfo.blend.modeAlpha = Nz::BlendEquation::Max;
        pipelineInfo.blend.srcColor = Nz::BlendFunc::SrcAlpha;
        pipelineInfo.blend.dstColor = Nz::BlendFunc::InvSrcAlpha;
        pipelineInfo.blend.srcAlpha = Nz::BlendFunc::One;
        pipelineInfo.blend.dstAlpha = Nz::BlendFunc::Zero;
        pipelineInfo.faceCulling = false;
        pipelineInfo.scissorTest = true;

        auto& pipelineVertexBuffer = pipelineInfo.vertexBuffers.emplace_back();
        pipelineVertexBuffer.binding = 0;
        pipelineVertexBuffer.declaration = Nz::VertexDeclaration::Get(Nz::VertexLayout::XYZ_Color_UV);

        Private::UntexturedPipeline.Pipeline = renderDevice->InstantiateRenderPipeline(pipelineInfo);
        return true;
    }

    bool LoadBackend(Nz::RenderWindow& window)
    {
        if (!LoadTexturedPipeline())
            return false;

        if (!LoadUntexturedPipeline())
            return false;

        return true;
    }

    void Init(Nz::RenderWindow& window, bool loadDefaultFont) {
        Init(window, window.GetSize(), loadDefaultFont);
    }

    void Init(Nz::RenderWindow& window, Nz::RenderTarget& target, bool loadDefaultFont) {
        Init(window, target.GetSize(), loadDefaultFont);
    }

void Init(Nz::RenderWindow& window, const Nz::Vector2ui& displaySize, bool loadDefaultFont) {

    if (!LoadBackend(window))
        return;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();


    // tell ImGui which features we support
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendPlatformName = "imgui_nazara";

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

    // init rendering
    io.DisplaySize = ImVec2(displaySize.x * 1.f, displaySize.y * 1.f);

    // clipboard
    io.SetClipboardTextFn = setClipboardText;
    io.GetClipboardTextFn = getClipboadText;

    loadMouseCursor(ImGuiMouseCursor_Arrow, Nz::SystemCursor::Default);
    loadMouseCursor(ImGuiMouseCursor_TextInput, Nz::SystemCursor::Text);
#if UNFINISHED_WORK
    loadMouseCursor(ImGuiMouseCursor_ResizeAll, Nz::SystemCursor::SizeAll);
    loadMouseCursor(ImGuiMouseCursor_ResizeNS, Nz::SystemCursor::SizeVertical);
    loadMouseCursor(ImGuiMouseCursor_ResizeEW, Nz::SystemCursor::SizeHorizontal);
    loadMouseCursor(ImGuiMouseCursor_ResizeNESW,
                    Nz::Cursor::SizeBottomLeftTopRight);
    loadMouseCursor(ImGuiMouseCursor_ResizeNWSE,
                    Nz::Cursor::SizeTopLeftBottomRight);
#endif
    loadMouseCursor(ImGuiMouseCursor_Hand, Nz::SystemCursor::Hand);

    Private::FontTexture.reset();

    if (loadDefaultFont) {
        // this will load default font automatically
        // No need to call AddDefaultFont
        UpdateFontTexture();
    }

    s_windowHasFocus = window.HasFocus();

    // Setup event handler
    window.GetEventHandler().OnMouseMoved.Connect([](const Nz::EventHandler*, const Nz::WindowEvent::MouseMoveEvent& event) {
        if (!s_windowHasFocus)
            return;

        s_mouseMoved = true;
    });
    window.GetEventHandler().OnMouseButtonPressed.Connect([](const Nz::EventHandler*, const Nz::WindowEvent::MouseButtonEvent& event) {
        if (!s_windowHasFocus)
            return;

        if (event.button >= Nz::Mouse::Button::Left
            && event.button <= Nz::Mouse::Button::Right)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDown[event.button] = true;
        }
    });
    window.GetEventHandler().OnMouseButtonReleased.Connect([](const Nz::EventHandler*, const Nz::WindowEvent::MouseButtonEvent& event) {
        if (!s_windowHasFocus)
            return;

        if (event.button >= Nz::Mouse::Button::Left
            && event.button <= Nz::Mouse::Button::Right)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDown[event.button] = false;
        }
        });

    window.GetEventHandler().OnMouseWheelMoved.Connect([](const Nz::EventHandler*, const Nz::WindowEvent::MouseWheelEvent& event) {
        if (!s_windowHasFocus)
            return;

        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheel += event.delta;
    });
    window.GetEventHandler().OnKeyPressed.Connect([](const Nz::EventHandler*, const Nz::WindowEvent::KeyEvent& event) {
        if (!s_windowHasFocus)
            return;

        ImGuiIO& io = ImGui::GetIO();
        io.KeysDown[(int)event.scancode] = true;
    });
    window.GetEventHandler().OnKeyReleased.Connect([](const Nz::EventHandler*, const Nz::WindowEvent::KeyEvent& event) {
        if (!s_windowHasFocus)
            return;

        ImGuiIO& io = ImGui::GetIO();
        io.KeysDown[(int)event.scancode] = false;
    });
    window.GetEventHandler().OnTextEntered.Connect([](const Nz::EventHandler*, const Nz::WindowEvent::TextEvent& event) {
        if (!s_windowHasFocus)
            return;

        ImGuiIO& io = ImGui::GetIO();

        // Don't handle the event for unprintable characters
        if (event.character < ' ' || event.character == 127) {
            return;
        }

        io.AddInputCharacter(event.character);
    });

    window.GetEventHandler().OnGainedFocus.Connect([](const Nz::EventHandler*) {
        s_windowHasFocus = true;
    });

    window.GetEventHandler().OnLostFocus.Connect([](const Nz::EventHandler*) {
        s_windowHasFocus = false;
    });
}

void Update(Nz::Window& window, float dt) {
    // Update OS/hardware mouse cursor if imgui isn't drawing a software cursor
    updateMouseCursor(window);

    if (s_mouseMoved) {
        Update(Nz::Mouse::GetPosition(window), window.GetSize(), dt);
    }
    else
    {
        {
            Update({ 0,0 }, window.GetSize(), dt);
        }
    }

#if UNFINISHED_WORK
    if (ImGui::GetIO().MouseDrawCursor) {
        // Hide OS mouse cursor if imgui is drawing it
        window.setMouseCursorVisible(false);
    }
#endif
}

void Update(const Nz::Vector2i& mousePos, const Nz::Vector2ui& displaySize, float dt) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(displaySize.x * 1.f, displaySize.y * 1.f);
    
    io.DeltaTime = dt / 1000.f;

    if (s_windowHasFocus) {
        if (io.WantSetMousePos) {
            Nz::Vector2i mousePos(static_cast<int>(io.MousePos.x),
                                  static_cast<int>(io.MousePos.y));
            Nz::Mouse::SetPosition(mousePos);
        } else {
            io.MousePos = ImVec2(mousePos.x * 1.f, mousePos.y * 1.f);
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

void Render(Nz::RenderWindow& window, Nz::RenderFrame& frame) {
    ImGui::Render();
    RenderDrawLists(window, frame, ImGui::GetDrawData());
}

void Shutdown() {
    ImGui::GetIO().Fonts->TexID = 0;

    for (int i = 0; i < ImGuiMouseCursor_COUNT; ++i) {
        s_mouseCursors[i].reset();
    }

    ImGui::DestroyContext();

    Private::TexturedPipeline.TextureSampler.reset();
    Private::TexturedPipeline.TextureShaderBinding.reset();
    Private::TexturedPipeline.Pipeline.reset();

    Private::UntexturedPipeline.Pipeline.reset();
    Private::FontTexture.reset();
}

void UpdateFontTexture() {
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
    Private::FontTexture = renderDevice->InstantiateTexture(texParams);
    Private::FontTexture->Update(pixels, width, height);

    ImTextureID textureID = Private::FontTexture.get();
    io.Fonts->TexID = textureID;
}

}  // end of namespace Nz

/////////////// Image Overloads

/*void Image(const Nz::Texture& texture, const Nz::Color& tintColor,
           const Nz::Color& borderColor) {
    Image(texture, Nz::Vector2f(texture.GetSize().x * 1.f, texture.GetSize().y * 1.f), tintColor, borderColor);
}

void Image(const Nz::Texture& texture, const Nz::Vector2f& size,
           const Nz::Color& tintColor, const Nz::Color& borderColor) {
    ImTextureID textureID = Private::Backend->GetTextureNativeHandler(&texture);
    
    ImGui::Image(textureID, ImVec2(size.x,size.y), ImVec2(0, 0), ImVec2(1, 1), toImColor(tintColor), toImColor(borderColor));
}

void Image(const Nz::Texture& texture, const Nz::Rectf& textureRect,
           const Nz::Color& tintColor, const Nz::Color& borderColor) {
    Image(texture, Nz::Vector2f(std::abs(textureRect.width), std::abs(textureRect.height)), textureRect, tintColor, borderColor);
}

void Image(const Nz::Texture& texture, const Nz::Vector2f& size,
           const Nz::Rectf& textureRect, const Nz::Color& tintColor,
           const Nz::Color& borderColor) {
    Nz::Vector2f textureSize(texture.GetSize().x * 1.f, texture.GetSize().y * 1.f);
    ImVec2 uv0(textureRect.GetCorner(Nz::RectCorner::LeftTop) / textureSize);
    ImVec2 uv1(textureRect.GetCorner(Nz::RectCorner::RightBottom) / textureSize);

    ImTextureID textureID = Private::Backend->GetTextureNativeHandler(&texture);
    ImGui::Image(textureID, ImVec2(size.x, size.y), uv0, uv1, toImColor(tintColor), toImColor(borderColor));
}

/////////////// Image Button Overloads

bool ImageButton(const Nz::Texture& texture, const int framePadding,
                 const Nz::Color& bgColor, const Nz::Color& tintColor) {
    return ImageButton(texture, Nz::Vector2f(texture.GetSize().x * 1.f, texture.GetSize().y * 1.f), framePadding, bgColor, tintColor);
}

bool ImageButton(const Nz::Texture& texture, const Nz::Vector2f& size,
                 const int framePadding, const Nz::Color& bgColor,
                 const Nz::Color& tintColor) {
    Nz::Vector2f textureSize(texture.GetSize().x * 1.f, texture.GetSize().y * 1.f);
    return ::imageButtonImpl(texture, Nz::Rectf(0.f, 0.f, textureSize.x, textureSize.y), size, framePadding, bgColor, tintColor);
}*/

/////////////// Draw_list Overloads

void DrawLine(const Nz::Vector2f& a, const Nz::Vector2f& b,
              const Nz::Color& color, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    draw_list->AddLine(ImVec2(a.x + pos.x, a.y + pos.y), ImVec2(b.x + pos.x, b.y + pos.y), ColorConvertFloat4ToU32(toImColor(color)), thickness);
}

void DrawRect(const Nz::Rectf& rect, const Nz::Color& color, float rounding,
              int rounding_corners, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(getTopLeftAbsolute(rect), getDownRightAbsolute(rect), ColorConvertFloat4ToU32(toImColor(color)), rounding, rounding_corners, thickness);
}

void DrawRectFilled(const Nz::Rectf& rect, const Nz::Color& color,
                    float rounding, int rounding_corners) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(getTopLeftAbsolute(rect), getDownRightAbsolute(rect), ColorConvertFloat4ToU32(toImColor(color)), rounding, rounding_corners);
}

}  // end of namespace ImGui

namespace {
ImColor toImColor(Nz::Color c ) {
    return ImColor(static_cast<int>(c.r), static_cast<int>(c.g), static_cast<int>(c.b), static_cast<int>(c.a));
}
ImVec2 getTopLeftAbsolute(const Nz::Rectf& rect) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    return ImVec2(rect.GetCorner(Nz::RectCorner::LeftTop) + Nz::Vector2f(pos.x, pos.y));
}
ImVec2 getDownRightAbsolute(const Nz::Rectf& rect) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    return ImVec2(rect.GetCorner(Nz::RectCorner::RightBottom) + Nz::Vector2f(pos.x, pos.y));
}

inline Nz::Vector2f ToNzVec2(ImVec2 v)
{
    return { v.x, v.y };
}

inline Nz::Vector3f ToNzVec3(ImVec2 v)
{
    return { v.x, v.y, 0.f };
}

inline Nz::Color ToNzColor(ImU32 color)
{
    auto c = ImGui::ColorConvertU32ToFloat4(color);
    return { c.x, c.y, c.z, c.w };
}

// Rendering callback
void RenderDrawLists(Nz::RenderWindow& window, Nz::RenderFrame& frame, ImDrawData* draw_data) {
    ImGui::GetDrawData();
    if (draw_data->CmdListsCount == 0) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    assert(io.Fonts->TexID !=
        (ImTextureID)NULL);  // You forgot to create and set font texture

    // scale stuff (needed for proper handling of window resize)
    int fb_width =
        static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height =
        static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width == 0 || fb_height == 0) {
        return;
    }
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    auto renderDevice = Nz::Graphics::Instance()->GetRenderDevice();

    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const unsigned char* vtx_buffer =
            (const unsigned char*)&cmd_list->VtxBuffer.front();
        const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();

        std::vector<Nz::VertexStruct_XYZ_Color_UV> vertices;
        vertices.reserve(cmd_list->VtxBuffer.size());
        for (auto& vertex : cmd_list->VtxBuffer)
            vertices.push_back({ ToNzVec3(vertex.pos), ToNzColor(vertex.col), ToNzVec2(vertex.uv)});

        size_t size = vertices.size() * sizeof(Nz::VertexStruct_XYZ_Color_UV);
        auto vertexBuffer = renderDevice->InstantiateBuffer(Nz::BufferType::Vertex, size, Nz::BufferUsage::DeviceLocal | Nz::BufferUsage::Dynamic, vertices.data());
        auto indexBuffer = renderDevice->InstantiateBuffer(Nz::BufferType::Index, cmd_list->IdxBuffer.size() * sizeof(ImWchar), Nz::BufferUsage::DeviceLocal | Nz::BufferUsage::Dynamic, idx_buffer);

        auto* windowRT = window.GetRenderTarget();

        std::vector<ImDrawCmd> cmdBuffer;
        cmdBuffer.reserve(cmd_list->CmdBuffer.size());
        for (auto& cmd : cmd_list->CmdBuffer)
            cmdBuffer.push_back(cmd);

        frame.Execute([windowRT, &frame, fb_width, fb_height, cmdBuffer, vertexBuffer, indexBuffer](Nz::CommandBufferBuilder& builder) {
            builder.BeginDebugRegion("ImGui", Nz::Color::Green);
            {
                Nz::Recti renderRect(0, 0, fb_width, fb_height);

                Nz::CommandBufferBuilder::ClearValues clearValues[2];
                clearValues[0].color = Nz::Color::Black;
                clearValues[1].depth = 1.f;
                clearValues[1].stencil = 0;

                builder.BeginRenderPass(windowRT->GetFramebuffer(frame.GetFramebufferIndex()), windowRT->GetRenderPass(), renderRect, { clearValues[0], clearValues[1] });
                {
                    Nz::UInt64 indexOffset = 0;
                    for (auto& cmd : cmdBuffer)
                    {
                        if (!cmd.UserCallback)
                        {
                            auto rect = cmd.ClipRect;
                            auto count = cmd.ElemCount;
                            auto texture = static_cast<Nz::Texture*>(cmd.GetTexID());

                            if (nullptr != texture)
                            {
                                Private::TexturedPipeline.TextureShaderBinding->Update({
                                {
                                    0,
                                    Nz::ShaderBinding::TextureBinding {
                                        texture, Private::TexturedPipeline.TextureSampler.get()
                                    }
                                }
                                    });

                                builder.BindPipeline(*Private::TexturedPipeline.Pipeline);
                                builder.BindShaderBinding(1, *Private::TexturedPipeline.TextureShaderBinding);
                            }
                            else
                            {
                                builder.BindPipeline(*Private::UntexturedPipeline.Pipeline);
                            }

                            builder.SetViewport(Nz::Recti{ 0, 0, fb_width, fb_height });
                            builder.SetScissor(Nz::Recti{ int(rect.x), int(rect.y), int(rect.z - rect.x), int(rect.w - rect.y) });// Nz::Recti{ int(rect.x), int(fb_height - rect.w), int(rect.z - rect.x), int(rect.w - rect.y) });

                            builder.BindIndexBuffer(*indexBuffer, Nz::IndexType::U16, indexOffset * sizeof(ImWchar));
                            builder.BindVertexBuffer(0, *vertexBuffer);

                            builder.DrawIndexed(count);
                        }
                        indexOffset += cmd.ElemCount;
                    }
                }
                builder.EndRenderPass();
            }
            builder.EndDebugRegion();
            }, Nz::QueueType::Graphics);
    }
}

/*bool imageButtonImpl(const Nz::Texture& texture,
                     const Nz::Rectf& textureRect, const Nz::Vector2f& size,
                     const int framePadding, const Nz::Color& bgColor,
                     const Nz::Color& tintColor) {

    Nz::Vector2f textureSize(texture.GetSize().x * 1.f, texture.GetSize().y * 1.f);
    ImVec2 uv0(textureRect.GetCorner(Nz::RectCorner::LeftTop) / textureSize);
    ImVec2 uv1(textureRect.GetCorner(Nz::RectCorner::RightBottom) / textureSize);

    ImTextureID textureID = Private::Backend->GetTextureNativeHandler(&texture);
    return ImGui::ImageButton(textureID, ImVec2(size.x,size.y), uv0, uv1, framePadding, toImColor(bgColor), toImColor(tintColor));
}*/

void setClipboardText(void* /*userData*/, const char* text) {
    Nz::Clipboard::SetString(text);
}

const char* getClipboadText(void* /*userData*/) {
    s_clipboardText = Nz::Clipboard::GetString();
    return s_clipboardText.c_str();
}

void loadMouseCursor(ImGuiMouseCursor imguiCursorType,
                     Nz::SystemCursor nzCursorType) {
    s_mouseCursors[imguiCursorType] = Nz::Cursor::Get(nzCursorType);
}

void updateMouseCursor(Nz::Window& window) {
    ImGuiIO& io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0) {
        ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
        if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None) {
#if UNFINISHED_WORK
            window.setMouseCursorVisible(false);
#endif
        } else {
#if UNFINISHED_WORK
            window.setMouseCursorVisible(true);
#endif

            std::shared_ptr<Nz::Cursor> c = s_mouseCursors[cursor] ? s_mouseCursors[cursor] : s_mouseCursors[ImGuiMouseCursor_Arrow];
            window.SetCursor(c);
        }
    }
}

}  // end of anonymous namespace
