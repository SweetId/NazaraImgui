#include <NazaraImgui/ImguiDrawer.hpp>

#include <NazaraImgui/NazaraImgui.hpp>

#include <Nazara/Renderer/CommandBufferBuilder.hpp>
#include <Nazara/Renderer/RenderDevice.hpp>
#include <Nazara/Renderer/RenderFrame.hpp>
#include <Nazara/Renderer/UploadPool.hpp>
#include <Nazara/Utility/VertexStruct.hpp>


#include <NZSL/Parser.hpp>

const char shaderSource_Textured[] =
#include "Textured.nzsl.h"
;

const char shaderSource_Untextured[] =
#include "Untextured.nzsl.h"
;

namespace
{
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

	ImguiDrawer::ImguiDrawer(RenderDevice& renderDevice)
        : m_renderDevice(renderDevice)
	{
        LoadTexturedPipeline();
        LoadUntexturedPipeline();
	}

	ImguiDrawer::~ImguiDrawer()
	{
		m_untexturedPipeline.uboShaderBinding.reset();
		m_untexturedPipeline.pipeline.reset();

		m_texturedPipeline.uboShaderBinding.reset();
		m_texturedPipeline.textureShaderBindings.clear();
		m_texturedPipeline.textureSampler.reset();
		m_texturedPipeline.pipeline.reset();
	}

	void ImguiDrawer::Prepare(RenderResources& frame)
	{
        m_drawCalls.clear();

        ImDrawData* drawData = ImGui::GetDrawData();
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
				builder.BeginDebugRegion("Imgui UBO Update", Nz::Color::Yellow());
				{
					builder.PreTransferBarrier();
					builder.CopyBuffer(allocation, m_uboBuffer.get());
					builder.PostTransferBarrier();
				}
				builder.EndDebugRegion();
			}, Nz::QueueType::Transfer);

        drawData->ScaleClipRects(io.DisplayFramebufferScale);

        // first pass over cmd lists to prepare buffers
        std::vector<Nz::VertexStruct_XYZ_Color_UV> vertices;
        std::vector<uint16_t> indices;
        for (int n = 0; n < drawData->CmdListsCount; ++n) {
            const ImDrawList* cmd_list = drawData->CmdLists[n];

            DrawCall drawCall;
            drawCall.vertex_offset = vertices.size();
            drawCall.indice_offset = indices.size();

            vertices.reserve(vertices.size() + cmd_list->VtxBuffer.size());
            for (auto& vertex : cmd_list->VtxBuffer)
                vertices.push_back({ ToNzVec3(vertex.pos), ToNzColor(vertex.col), ToNzVec2(vertex.uv) });

            indices.reserve(indices.size() + cmd_list->IdxBuffer.size());
            for (auto indice : cmd_list->IdxBuffer)
                indices.push_back(uint16_t(drawCall.vertex_offset + indice));

            for (auto& cmd : cmd_list->CmdBuffer)
                drawCall.cmdBuffer.push_back(cmd);

            m_drawCalls.push_back(std::move(drawCall));
        }

        frame.PushForRelease(std::move(m_vertexBuffer));
        frame.PushForRelease(std::move(m_indexBuffer));

        // now that we have macro buffers, allocate them on gpu
        size_t size = vertices.size() * sizeof(Nz::VertexStruct_XYZ_Color_UV);
        m_vertexBuffer = m_renderDevice.InstantiateBuffer(Nz::BufferType::Vertex, size, Nz::BufferUsage::DeviceLocal | Nz::BufferUsage::Dynamic, vertices.data());
        size = indices.size() * sizeof(uint16_t);
        m_indexBuffer = m_renderDevice.InstantiateBuffer(Nz::BufferType::Index, size, Nz::BufferUsage::DeviceLocal | Nz::BufferUsage::Dynamic, indices.data());

	}

    void ImguiDrawer::Draw(CommandBufferBuilder& builder)
    {
        if (m_drawCalls.empty())
            return;

        ImGuiIO& io = ImGui::GetIO();
        int fb_width = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        int fb_height = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);

        builder.SetViewport(Nz::Recti{ 0, 0, fb_width, fb_height });
        builder.BindIndexBuffer(*m_indexBuffer, Nz::IndexType::U16);
        builder.BindVertexBuffer(0, *m_vertexBuffer);

        for (auto& drawCall : m_drawCalls)
        {
            Nz::UInt64 indexOffset = drawCall.indice_offset;
            for (auto& cmd : drawCall.cmdBuffer)
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
                                    Nz::ShaderBinding::SampledTextureBinding {
                                        texture, m_texturedPipeline.textureSampler.get()
                                    }
                                }
                                });
                            m_texturedPipeline.textureShaderBindings[texture] = std::move(binding);
                        }

                        builder.BindRenderPipeline(*m_texturedPipeline.pipeline);
                        builder.BindRenderShaderBinding(0, *m_texturedPipeline.uboShaderBinding);
                        builder.BindRenderShaderBinding(1, *m_texturedPipeline.textureShaderBindings[texture]);
                    }
                    else
                    {
                        builder.BindRenderPipeline(*m_untexturedPipeline.pipeline);
                        builder.BindRenderShaderBinding(0, *m_untexturedPipeline.uboShaderBinding);
                    }

                    builder.SetScissor(Nz::Recti{ int(rect.x), int(rect.y), int(rect.z - rect.x), int(rect.w - rect.y) });// Nz::Recti{ int(rect.x), int(fb_height - rect.w), int(rect.z - rect.x), int(rect.w - rect.y) });

                    builder.DrawIndexed(count, 1, Nz::UInt32(indexOffset));
                }
                indexOffset += cmd.ElemCount;
            }
        }
    }

    void ImguiDrawer::Reset(RenderResources& /*renderFrame*/)
    {
        m_drawCalls.clear();
    }

    bool ImguiDrawer::LoadTexturedPipeline()
    {
        nzsl::Ast::ModulePtr shaderModule = nzsl::Parse(std::string_view(shaderSource_Textured, sizeof(shaderSource_Textured)));
        if (!shaderModule)
            throw std::runtime_error("Failed to parse shader module");

        nzsl::ShaderWriter::States states;
        states.optimize = true;

        auto shader = m_renderDevice.InstantiateShaderModule(nzsl::ShaderStageType::Fragment | nzsl::ShaderStageType::Vertex, *shaderModule, states);
        if (!shader)
            throw std::runtime_error("Failed to instantiate shader");

        m_texturedPipeline.textureSampler = m_renderDevice.InstantiateTextureSampler({});

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
        textureBinding.type = Nz::ShaderBindingType::Sampler;

        std::shared_ptr<Nz::RenderPipelineLayout> renderPipelineLayout = m_renderDevice.InstantiateRenderPipelineLayout(std::move(pipelineLayoutInfo));

        Nz::RenderPipelineInfo pipelineInfo;
        pipelineInfo.pipelineLayout = renderPipelineLayout;
        pipelineInfo.shaderModules.emplace_back(shader);

        pipelineInfo.depthBuffer = false;
        pipelineInfo.faceCulling = Nz::FaceCulling::None;
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

        m_texturedPipeline.pipeline = m_renderDevice.InstantiateRenderPipeline(pipelineInfo);

        m_uboBuffer = m_renderDevice.InstantiateBuffer(Nz::BufferType::Uniform, sizeof(ImguiUbo), Nz::BufferUsage::DeviceLocal | Nz::BufferUsage::Dynamic);

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

    bool ImguiDrawer::LoadUntexturedPipeline()
    {
        nzsl::Ast::ModulePtr shaderModule = nzsl::Parse(std::string_view(shaderSource_Untextured, sizeof(shaderSource_Untextured)));
        if (!shaderModule)
            throw std::runtime_error("Failed to parse shader module");

        nzsl::ShaderWriter::States states;
        states.optimize = true;

        auto shader = m_renderDevice.InstantiateShaderModule(nzsl::ShaderStageType::Fragment | nzsl::ShaderStageType::Vertex, *shaderModule, states);
        if (!shader)
            throw std::runtime_error("Failed to instantiate shader");

        Nz::RenderPipelineLayoutInfo pipelineLayoutInfo;

        auto& uboBinding = pipelineLayoutInfo.bindings.emplace_back();
        uboBinding.setIndex = 0;
        uboBinding.bindingIndex = 0;
        uboBinding.shaderStageFlags = nzsl::ShaderStageType::Vertex;
        uboBinding.type = Nz::ShaderBindingType::UniformBuffer;

        std::shared_ptr<Nz::RenderPipelineLayout> renderPipelineLayout = m_renderDevice.InstantiateRenderPipelineLayout(std::move(pipelineLayoutInfo));

        Nz::RenderPipelineInfo pipelineInfo;
        pipelineInfo.pipelineLayout = renderPipelineLayout;
        pipelineInfo.shaderModules.emplace_back(shader);

        pipelineInfo.depthBuffer = false;
        pipelineInfo.faceCulling = Nz::FaceCulling::None;
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

        m_untexturedPipeline.pipeline = m_renderDevice.InstantiateRenderPipeline(pipelineInfo);

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
}