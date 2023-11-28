
#include <Nazara/Core/Application.hpp>
#include <Nazara/Core/Clock.hpp>
#include <Nazara/Graphics/Graphics.hpp>
#include <Nazara/Platform/AppWindowingComponent.hpp>
#include <Nazara/Platform/Platform.hpp>
#include <Nazara/Renderer/GpuSwitch.hpp>
#include <Nazara/Renderer/WindowSwapchain.hpp>

#include <NazaraImgui/ImguiHandler.hpp>
#include <NazaraImgui/ImguiWidgets.hpp>
#include <NazaraImgui/NazaraImgui.hpp>

NAZARA_REQUEST_DEDICATED_GPU()

struct MyImguiWindow
	: private Nz::ImguiHandler
{
	MyImguiWindow()
	{
		Nz::Imgui::Instance()->AddHandler(this);
	}

	~MyImguiWindow()
	{
		Nz::Imgui::Instance()->RemoveHandler(this);
	}

	void OnRenderImgui() override
	{
		ImGui::Begin("Handler Window");
		ImGui::InputFloat4("position", values);
		ImGui::End();
	}

	float values[4] = { 0 };
};

#if 1
int main(int argc, char* argv[])
#else
int WinMain(int argc, char* argv[])
#endif
{
	Nz::Application<Nz::Graphics, Nz::Imgui> nazara(argv, argc);
	auto& windowing = nazara.AddComponent<Nz::AppWindowingComponent>();
	std::shared_ptr<Nz::RenderDevice> device = Nz::Graphics::Instance()->GetRenderDevice();

	std::string windowTitle = "Nazara Imgui Demo";
	Nz::Window& window = windowing.CreateWindow(Nz::VideoMode(1280, 720, 32), windowTitle);
	Nz::WindowSwapchain windowSwapchain(device, window);

	// Load test texture
	Nz::TextureParams texParams;
	texParams.renderDevice = Nz::Graphics::Instance()->GetRenderDevice();
	texParams.loadFormat = Nz::PixelFormat::RGBA8;
	auto logo = Nz::Texture::LoadFromFile("LogoMini.png", texParams);

	// connect basic handler
	window.GetEventHandler().OnQuit.Connect([&window](const auto* handler) {
		NazaraUnused(handler);
		window.Close();
	});

	Nz::Imgui::Instance()->Init(window);
	ImGui::EnsureContextOnThisThread();
	
	MyImguiWindow mywindow;
	float val = 0.f;
	float color[4] = { 1,0,0,1 };

	Nz::MillisecondClock updateClock;

	while (window.IsOpen())
	{
		window.ProcessEvents();

		Nz::RenderFrame frame = windowSwapchain.AcquireFrame();
		if (!frame)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		float deltaTime = updateClock.GetElapsedTime().AsSeconds();
		Nz::Imgui::Instance()->Update(deltaTime);

		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Test"))
			{
				if (ImGui::MenuItem("Test", "Ctrl+O"))
					printf("test\n");
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
		if (ImGui::BeginPopupModal("toto"))
		{
			ImGui::EndPopup();
		}
		ImGui::Begin("Loop Window");
		ImGui::Image(logo.get());
		ImGui::ImageButton(logo.get());
		ImGui::SliderFloat("test", &val, 0, 10);
		ImGui::ColorPicker4("Color", color, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB);
		
		ImGui::InputFloat4("value from 2nd window", mywindow.values, "%.3f", ImGuiInputTextFlags_ReadOnly);
		ImGui::End();

		Nz::Imgui::Instance()->Render(windowSwapchain.GetSwapchain(), frame);

		frame.Present();

		updateClock.Restart();
	}

	return 0;
}