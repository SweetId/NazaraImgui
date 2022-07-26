#include <NazaraImgui/NazaraImgui.hpp>

#include <Nazara/Core/DynLib.hpp>
#include <Nazara/Core/Log.hpp>
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
#include "Textured.nzsl.h"
;

const char shaderSource_Untextured[] = 
#include "Untextured.nzsl.h"
;

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

}


namespace Nz
{
    struct ImguiUbo
    {
        float screenWidth;
        float screenHeight;
    };

    Imgui* Imgui::s_instance = nullptr;

    Imgui::Imgui(Config config)
        : ModuleBase("Imgui", this)
        , m_bMouseMoved(false)
        , m_bWindowHasFocus(false)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.UserData = this;
    }

    Imgui::~Imgui()
    {
        m_untexturedPipeline.uboShaderBinding.reset();
        m_untexturedPipeline.pipeline.reset();

        m_texturedPipeline.uboShaderBinding.reset();
        m_texturedPipeline.textureShaderBindings.clear();
        m_texturedPipeline.textureSampler.reset();
        m_texturedPipeline.pipeline.reset();

        ImGui::GetIO().Fonts->TexID = nullptr;
        ImGui::DestroyContext();

        m_fontTexture.reset();
    }

    bool Imgui::Init(Nz::Window& window, bool bLoadDefaultFont)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        // tell ImGui which features we support
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
        io.BackendPlatformName = "imgui_nazara";

        // init rendering
        io.DisplaySize = ImVec2(window.GetSize().x * 1.f, window.GetSize().y * 1.f);

        if (!LoadTexturedPipeline())
            return false;

        if (!LoadUntexturedPipeline())
            return false;

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

    void Imgui::SetupInputs(Nz::EventHandler& handler)
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
        handler.OnMouseMoved.Connect([this](const Nz::EventHandler*, const Nz::WindowEvent::MouseMoveEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            m_bMouseMoved = true;
        });

        handler.OnMouseButtonPressed.Connect([this](const Nz::EventHandler*, const Nz::WindowEvent::MouseButtonEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            if (event.button >= Nz::Mouse::Button::Left
                && event.button <= Nz::Mouse::Button::Right)
            {
                ImGuiIO& io = ImGui::GetIO();
                io.MouseDown[event.button] = true;
            }
        });

        handler.OnMouseButtonReleased.Connect([this](const Nz::EventHandler*, const Nz::WindowEvent::MouseButtonEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            if (event.button >= Nz::Mouse::Button::Left
                && event.button <= Nz::Mouse::Button::Right)
            {
                ImGuiIO& io = ImGui::GetIO();
                io.MouseDown[event.button] = false;
            }
        });

        handler.OnMouseWheelMoved.Connect([this](const Nz::EventHandler*, const Nz::WindowEvent::MouseWheelEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            ImGuiIO& io = ImGui::GetIO();
            io.MouseWheel += event.delta;
        });

        handler.OnKeyPressed.Connect([this](const Nz::EventHandler*, const Nz::WindowEvent::KeyEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            ImGuiIO& io = ImGui::GetIO();
            io.KeysDown[(int)event.scancode] = true;
        });

        handler.OnKeyReleased.Connect([this](const Nz::EventHandler*, const Nz::WindowEvent::KeyEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            ImGuiIO& io = ImGui::GetIO();
            io.KeysDown[(int)event.scancode] = false;
        });

        handler.OnTextEntered.Connect([this](const Nz::EventHandler*, const Nz::WindowEvent::TextEvent& event) {
            if (!m_bWindowHasFocus)
                return;

            ImGuiIO& io = ImGui::GetIO();

            // Don't handle the event for unprintable characters
            if (event.character < ' ' || event.character == 127) {
                return;
            }

            io.AddInputCharacter(event.character);
        });

        handler.OnGainedFocus.Connect([this](const Nz::EventHandler*) {
            m_bWindowHasFocus = true;
        });

        handler.OnLostFocus.Connect([this](const Nz::EventHandler*) {
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

    void Imgui::Render(Nz::RenderWindow& window, Nz::RenderFrame& frame)
    {
        ImGui::Render();
        RenderDrawLists(window, frame, ImGui::GetDrawData());
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

    bool Imgui::LoadTexturedPipeline()
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


        m_texturedPipeline.textureSampler = renderDevice->InstantiateTextureSampler({});

        Nz::RenderPipelineLayoutInfo pipelineLayoutInfo;

        auto& uboBinding = pipelineLayoutInfo.bindings.emplace_back();
        uboBinding.setIndex = 0;
        uboBinding.bindingIndex = 0;
        uboBinding.shaderStageFlags = nzsl::ShaderStageType::Vertex;
        uboBinding.type = Nz::ShaderBindingType::UniformBuffer;

        auto& textureBinding = pipelineLayoutInfo.bindings.emplace_back();
        textureBinding.setIndex = 1;
        textureBinding.bindingIndex = 0;
        textureBinding.shaderStageFlags = nzsl::ShaderStageType::Fragment;
        textureBinding.type = Nz::ShaderBindingType::Texture;

        std::shared_ptr<Nz::RenderPipelineLayout> renderPipelineLayout = renderDevice->InstantiateRenderPipelineLayout(std::move(pipelineLayoutInfo));

        Nz::RenderPipelineInfo pipelineInfo;
        pipelineInfo.pipelineLayout = renderPipelineLayout;
        pipelineInfo.shaderModules.emplace_back(shader);

        pipelineInfo.depthBuffer = false;
        pipelineInfo.faceCulling = false;
        pipelineInfo.scissorTest = true;

        pipelineInfo.blending = true;
        pipelineInfo.blend.modeAlpha = Nz::BlendEquation::Add;
        pipelineInfo.blend.srcColor = Nz::BlendFunc::SrcAlpha;
        pipelineInfo.blend.dstColor = Nz::BlendFunc::InvSrcAlpha;
        pipelineInfo.blend.srcAlpha = Nz::BlendFunc::One;
        pipelineInfo.blend.dstAlpha = Nz::BlendFunc::Zero;

        auto& pipelineVertexBuffer = pipelineInfo.vertexBuffers.emplace_back();
        pipelineVertexBuffer.binding = 0;
        pipelineVertexBuffer.declaration = Nz::VertexDeclaration::Get(Nz::VertexLayout::XYZ_Color_UV);

        m_texturedPipeline.pipeline = renderDevice->InstantiateRenderPipeline(pipelineInfo);

        m_uboBuffer = renderDevice->InstantiateBuffer(Nz::BufferType::Uniform, sizeof(ImguiUbo), Nz::BufferUsage::DeviceLocal | Nz::BufferUsage::Dynamic);

        m_texturedPipeline.uboShaderBinding = renderPipelineLayout->AllocateShaderBinding(0);
        m_texturedPipeline.uboShaderBinding->Update({
            {
                0,
                Nz::ShaderBinding::UniformBufferBinding {
                    m_uboBuffer.get(), 0, sizeof(ImguiUbo)
                }
            }
        });

        return true;
    }

    bool Imgui::LoadUntexturedPipeline()
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

        auto& uboBinding = pipelineLayoutInfo.bindings.emplace_back();
        uboBinding.setIndex = 0;
        uboBinding.bindingIndex = 0;
        uboBinding.shaderStageFlags = nzsl::ShaderStageType::Vertex;
        uboBinding.type = Nz::ShaderBindingType::UniformBuffer;

        std::shared_ptr<Nz::RenderPipelineLayout> renderPipelineLayout = renderDevice->InstantiateRenderPipelineLayout(std::move(pipelineLayoutInfo));

        Nz::RenderPipelineInfo pipelineInfo;
        pipelineInfo.pipelineLayout = renderPipelineLayout;
        pipelineInfo.shaderModules.emplace_back(shader);

        pipelineInfo.depthBuffer = false;
        pipelineInfo.faceCulling = false;
        pipelineInfo.scissorTest = true;

        pipelineInfo.blending = true;
        pipelineInfo.blend.modeAlpha = Nz::BlendEquation::Add;
        pipelineInfo.blend.srcColor = Nz::BlendFunc::SrcAlpha;
        pipelineInfo.blend.dstColor = Nz::BlendFunc::InvSrcAlpha;
        pipelineInfo.blend.srcAlpha = Nz::BlendFunc::One;
        pipelineInfo.blend.dstAlpha = Nz::BlendFunc::Zero;

        auto& pipelineVertexBuffer = pipelineInfo.vertexBuffers.emplace_back();
        pipelineVertexBuffer.binding = 0;
        pipelineVertexBuffer.declaration = Nz::VertexDeclaration::Get(Nz::VertexLayout::XYZ_Color_UV);

        m_untexturedPipeline.pipeline = renderDevice->InstantiateRenderPipeline(pipelineInfo);

        m_untexturedPipeline.uboShaderBinding = renderPipelineLayout->AllocateShaderBinding(0);
        m_untexturedPipeline.uboShaderBinding->Update({
            {
                0,
                Nz::ShaderBinding::UniformBufferBinding {
                    m_uboBuffer.get(), 0, sizeof(ImguiUbo)
                }
            }
        });
        return true;
    }

    // Rendering callback
    void Imgui::RenderDrawLists(Nz::RenderWindow& window, Nz::RenderFrame& frame, ImDrawData* drawData)
    {
        if (drawData->CmdListsCount == 0)
            return;

        ImGuiIO& io = ImGui::GetIO();
        assert(io.Fonts->TexID != (ImTextureID)NULL);  // You forgot to create and set font texture

        // scale stuff (needed for proper handling of window resize)
        int fb_width = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        int fb_height = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
        if (fb_width == 0 || fb_height == 0)
            return;

        ImguiUbo ubo{ fb_width / 2.f, fb_height / 2.f };
        auto& allocation = frame.GetUploadPool().Allocate(sizeof(ImguiUbo));

        std::memcpy(allocation.mappedPtr, &ubo, sizeof(ImguiUbo));

        frame.Execute([&](Nz::CommandBufferBuilder& builder)
            {
                builder.BeginDebugRegion("UBO Update", Nz::Color::Yellow);
                {
                    builder.PreTransferBarrier();
                    builder.CopyBuffer(allocation, m_uboBuffer.get());
                    builder.PostTransferBarrier();
                }
                builder.EndDebugRegion();
            }, Nz::QueueType::Transfer);

        drawData->ScaleClipRects(io.DisplayFramebufferScale);

        auto renderDevice = Nz::Graphics::Instance()->GetRenderDevice();

        for (int n = 0; n < drawData->CmdListsCount; ++n) {
            const ImDrawList* cmd_list = drawData->CmdLists[n];
            const unsigned char* vtx_buffer =
                (const unsigned char*)&cmd_list->VtxBuffer.front();
            const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();

            std::vector<Nz::VertexStruct_XYZ_Color_UV> vertices;
            vertices.reserve(cmd_list->VtxBuffer.size());
            for (auto& vertex : cmd_list->VtxBuffer)
                vertices.push_back({ ToNzVec3(vertex.pos), ToNzColor(vertex.col), ToNzVec2(vertex.uv) });

            size_t size = vertices.size() * sizeof(Nz::VertexStruct_XYZ_Color_UV);
            auto vertexBuffer = renderDevice->InstantiateBuffer(Nz::BufferType::Vertex, size, Nz::BufferUsage::DeviceLocal | Nz::BufferUsage::Dynamic, vertices.data());
            auto indexBuffer = renderDevice->InstantiateBuffer(Nz::BufferType::Index, cmd_list->IdxBuffer.size() * sizeof(ImWchar), Nz::BufferUsage::DeviceLocal | Nz::BufferUsage::Dynamic, idx_buffer);

            auto* windowRT = window.GetRenderTarget();

            std::vector<ImDrawCmd> cmdBuffer;
            cmdBuffer.reserve(cmd_list->CmdBuffer.size());
            for (auto& cmd : cmd_list->CmdBuffer)
                cmdBuffer.push_back(cmd);

            frame.Execute([this, windowRT, &frame, fb_width, fb_height, cmdBuffer, vertexBuffer, indexBuffer](Nz::CommandBufferBuilder& builder) {
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
                                    if (std::end(m_texturedPipeline.textureShaderBindings) == m_texturedPipeline.textureShaderBindings.find(texture))
                                    {
                                        auto binding = m_texturedPipeline.pipeline->GetPipelineInfo().pipelineLayout->AllocateShaderBinding(1);
                                        binding->Update({
                                            {
                                                0,
                                                Nz::ShaderBinding::TextureBinding {
                                                    texture, m_texturedPipeline.textureSampler.get()
                                                }
                                            }
                                        });
                                        m_texturedPipeline.textureShaderBindings[texture] = std::move(binding);
                                    }

                                    builder.BindPipeline(*m_texturedPipeline.pipeline);
                                    builder.BindShaderBinding(0, *m_texturedPipeline.uboShaderBinding);
                                    builder.BindShaderBinding(1, *m_texturedPipeline.textureShaderBindings[texture]);
                                }
                                else
                                {
                                    builder.BindPipeline(*m_untexturedPipeline.pipeline);
                                    builder.BindShaderBinding(0, *m_untexturedPipeline.uboShaderBinding);
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
