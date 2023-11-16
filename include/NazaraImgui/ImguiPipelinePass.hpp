#pragma once

#include <NazaraImgui/Config.hpp>

#include <Nazara/Core/ParameterList.hpp>
#include <Nazara/Graphics/FramePipelinePass.hpp>

namespace Nz
{
	class PassData;

	class NAZARA_IMGUI_API ImguiPipelinePass
		: public FramePipelinePass
	{
	public:
		ImguiPipelinePass(PassData& passData, std::string passName, const ParameterList& parameters = {});
		ImguiPipelinePass(const ImguiPipelinePass&) = delete;
		ImguiPipelinePass(ImguiPipelinePass&&) = delete;
		~ImguiPipelinePass() = default;

		void Prepare(FrameData& frameData) override;
		FramePass& RegisterToFrameGraph(FrameGraph& frameGraph, const PassInputOuputs& inputOuputs) override;

	private:
		std::string m_passName;
	};
}