#pragma once

#include <NazaraImgui/Config.hpp>

#include <Nazara/Renderer/ShaderBinding.hpp>

#include <imgui.h>

namespace Nz
{
	class CommandBufferBuilder;
	class RenderBuffer;
	class RenderDevice;
	class RenderFrame;
	class RenderPipeline;

	class NAZARA_IMGUI_API ImguiDrawer
	{
	public:
		ImguiDrawer(RenderDevice& renderDevice);
		ImguiDrawer(const ImguiDrawer&) = delete;
		ImguiDrawer(ImguiDrawer&&) noexcept = default;
		~ImguiDrawer();

		ImguiDrawer& operator=(const ImguiDrawer&) = delete;
		ImguiDrawer& operator=(ImguiDrawer&&) = delete;

		void Prepare(RenderFrame& renderFrame);

		void Reset(RenderFrame& renderFrame);

		void Draw(CommandBufferBuilder& builder);
	
	private:
		bool LoadTexturedPipeline();
		bool LoadUntexturedPipeline();

		RenderDevice& m_renderDevice;

		struct DrawCall
		{
			size_t vertex_offset, indice_offset;
			std::vector<ImDrawCmd> cmdBuffer;
		};
		std::vector<DrawCall> m_drawCalls;

		struct
		{
			std::shared_ptr<RenderPipeline> pipeline;
			std::unordered_map<Texture*, ShaderBindingPtr> textureShaderBindings;
			Nz::ShaderBindingPtr uboShaderBinding;
			std::shared_ptr<TextureSampler> textureSampler;
		} m_texturedPipeline;

		struct
		{
			std::shared_ptr<RenderPipeline> pipeline;
			Nz::ShaderBindingPtr uboShaderBinding;
		} m_untexturedPipeline;

		std::shared_ptr<RenderBuffer> m_vertexBuffer;
		std::shared_ptr<RenderBuffer> m_indexBuffer;
		std::shared_ptr<RenderBuffer> m_uboBuffer;
	};
}