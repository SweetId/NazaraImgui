
#include <Nazara/Core/Modules.hpp>
#include <Nazara/Graphics/Graphics.hpp>
#include <Nazara/Renderer/RenderWindow.hpp>

#include <NazaraImgui/NazaraImgui.hpp>

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

int WinMain(int argc, char* argv[])
{
	NazaraUnused(argc);
	NazaraUnused(argv);

	Nz::Modules<Nz::Graphics, Nz::Imgui> nazara;

	// Load test texture
	Nz::TextureParams texParams;
	texParams.renderDevice = Nz::Graphics::Instance()->GetRenderDevice();
	texParams.loadFormat = Nz::PixelFormat::RGBA8;
	auto logo = Nz::Texture::LoadFromFile("LogoMini.png", texParams);

	std::string windowTitle = "Nazara Imgui Demo";
	Nz::RenderWindow window;
	if (!window.Create(Nz::Graphics::Instance()->GetRenderDevice(), Nz::VideoMode(1280, 720, 32), windowTitle))
		return false;

	// connect basic handler
	window.GetEventHandler().OnQuit.Connect([&window](const auto* handler) {
		NazaraUnused(handler);
		window.Close();
	});

	Nz::Imgui::Instance()->Init(window);
	
	MyImguiWindow mywindow;
	float val = 0.f;
	float color[4] = { 1,0,0,1 };

	Nz::Clock updateClock;

	while (window.IsOpen())
	{
		window.ProcessEvents();

		Nz::RenderFrame frame = window.AcquireFrame();
		if (!frame)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		float deltaTime = updateClock.GetSeconds();
		Nz::Imgui::Instance()->Update(window, deltaTime);

		ImGui::Begin("Loop Window");
		ImGui::Image(logo.get());
		ImGui::ImageButton(logo.get());
		ImGui::SliderFloat("test", &val, 0, 10);
		ImGui::ColorPicker4("Color", color, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB);
		
		ImGui::InputFloat4("value from 2nd window", mywindow.values, "%.3f", ImGuiInputTextFlags_ReadOnly);
		ImGui::End();

		Nz::Imgui::Instance()->Render(window, frame);

		frame.Present();

		updateClock.Restart();
	}

	return 0;
}