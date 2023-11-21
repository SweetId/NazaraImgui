
#include <Nazara/Core.hpp>
#include <Nazara/Graphics.hpp>
#include <Nazara/Platform.hpp>
#include <Nazara/Renderer.hpp>
#include <Nazara/Utility.hpp>

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
	NazaraUnused(argc);
	NazaraUnused(argv);

	Nz::Application<Nz::Graphics, Nz::Imgui> nazara;
	auto& windowing = nazara.AddComponent<Nz::AppWindowingComponent>();
	std::shared_ptr<Nz::RenderDevice> device = Nz::Graphics::Instance()->GetRenderDevice();

	auto& ecs = nazara.AddComponent<Nz::AppEntitySystemComponent>();
	auto& world = ecs.AddWorld<Nz::EnttWorld>();

	auto& renderer = world.AddSystem<Nz::RenderSystem>();

	std::string windowTitle = "Nazara Imgui Demo";
	Nz::Window& window = windowing.CreateWindow(Nz::VideoMode(1280, 720, 32), windowTitle);
	auto& swapchain = renderer.CreateSwapchain(window);

	// Init imgui now that the window is created
	Nz::Imgui::Instance()->Init(window);
	ImGui::EnsureContextOnThisThread();

	// configure camera
	auto camera = world.CreateEntity();
	auto passList = Nz::PipelinePassList::LoadFromFile("example.passlist");
	camera.emplace<Nz::NodeComponent>();

	auto& cameraComponent = camera.emplace<Nz::CameraComponent>(std::make_shared<Nz::RenderWindow>(swapchain), passList, Nz::ProjectionType::Perspective);
	cameraComponent.UpdateFOV(70.f);
	cameraComponent.UpdateClearColor(Nz::Color(0.46f, 0.48f, 0.84f, 1.f));

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
	
	MyImguiWindow mywindow;
	float val = 0.f;
	float color[4] = { 0,1,0,1 };

	Nz::MillisecondClock updateClock;

	nazara.AddUpdaterFunc(Nz::ApplicationBase::Interval{ Nz::Time::Milliseconds(16) }, [&](Nz::Time elapsed) {
		if (!window.IsOpen())
			return;

		window.ProcessEvents();

		Nz::Imgui::Instance()->Update(elapsed.AsMilliseconds() / 1000.f);

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
		cameraComponent.UpdateClearColor(Nz::Color(color[0], color[1], color[2], color[3])); // This will work, eventually (engine bug)

		ImGui::InputFloat4("value from 2nd window", mywindow.values, "%.3f", ImGuiInputTextFlags_ReadOnly);
		ImGui::End();

		Nz::Imgui::Instance()->Render();
	});

	return nazara.Run();
}