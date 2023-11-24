#include <NazaraImgui/ImguiPipelinePass.hpp>

#include <NazaraImgui/NazaraImgui.hpp>
#include <NazaraImgui/ImguiDrawer.hpp>

#include <Nazara/Graphics/FrameGraph.hpp>

namespace Nz
{
	ImguiPipelinePass::ImguiPipelinePass(PassData& /*passData*/, std::string passName, const ParameterList& /*parameters*/) :
		FramePipelinePass({})
		, m_passName(std::move(passName))
	{
	}

	void ImguiPipelinePass::Prepare(FrameData& frameData)
	{
		ImguiDrawer& imguiDrawer = Nz::Imgui::Instance()->GetImguiDrawer();
		imguiDrawer.Prepare(frameData.renderResources);
	}

	FramePass& ImguiPipelinePass::RegisterToFrameGraph(FrameGraph& frameGraph, const PassInputOuputs& inputOuputs)
	{
		if (inputOuputs.inputCount != 1)
			throw std::runtime_error("one input expected");

		if (inputOuputs.outputCount != 1)
			throw std::runtime_error("one output expected");

		FramePass& imguiPass = frameGraph.AddPass("Imgui pass");
		imguiPass.AddInput(inputOuputs.inputAttachments[0]);
		imguiPass.AddOutput(inputOuputs.outputAttachments[0]);

		imguiPass.SetExecutionCallback([&]
			{
				return FramePassExecution::UpdateAndExecute;
			});

		imguiPass.SetCommandCallback([this](CommandBufferBuilder& builder, const FramePassEnvironment& /*env*/)
			{
				ImguiDrawer& imguiDrawer = Nz::Imgui::Instance()->GetImguiDrawer();
				imguiDrawer.Draw(builder);
			});

		return imguiPass;
	}
}