# Nazara Imgui

Nazara Imgui is a [Nazara Engine](https://github.com/NazaraEngine/NazaraEngine) module that integrates [Imgui](https://github.com/ocornut/imgui) features into the Engine.

You can use it in any kind of commercial and non-commercial applications without any restriction ([MIT license](http://opensource.org/licenses/MIT)).

## Authors

Sid - main developper

## How to Use

```
// Add NazaraImgui.hpp to your includes
#include <NazaraImgui/NazaraImgui.hpp>

// main.cpp
{
    // Add Nz::Imgui to the modules list
    Nz::Modules<Nz::Graphics, Nz::Imgui,...> nazara;

    // Create and init renderwindow
    Nz::RenderWindow window;


    // Once window is created, init Imgui instance
    // This will load shaders, create graphic pipeline, install event handlers, ...
    Nz::Imgui::Instance()->Init(window);

    // [...]
    float value = 0.f; // for ImGui::SliderFloat
    float color[] = { 1,0,0,1 }; // for ImGui::ColorPicker4

    // for ImGui::Image
    Nz::TextureParams texParams;
    texParams.renderDevice = device;
    texParams.loadFormat = Nz::PixelFormat::RGBA8;
    std::shared_ptr<Nz::Texture> logo = Nz::Texture::LoadFromFile("MyImage.png", texParams);

    Nz::Clock updateClock;
    while(window.IsOpen())
    {
        // ProcessEvent will send events to imgui
        window.ProcessEvents();

        Nz::RenderFrame frame = window.AcquireFrame();
        if (!frame)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Call update on the instance
        Nz::Imgui::Instance()->Update(window, updateClock.GetSeconds());

        // Then use standard imgui functions
        ImGui::Begin("MyWindow");

        ImGui::SliderFloat("Value", &value, 0, 10);
        
        // display a Nz::Texture
        ImGui::Image(logo.get());

        ImGui::ColorPicker4("Color", color, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB);
        
        ImGui::End();

        // Before presenting frame, render imgui
        Nz::Imgui::Instance()->Render(window, frame);
        frame.Present();

        updateClock.Restart();
    }
}
```


## Contribute

##### Don't hesitate to contribute to Nazara Engine by:
- Extending the [wiki](https://github.com/NazaraEngine/NazaraEngine/wiki)
- Submitting a patch to GitHub  
- Post suggestions/bugs on the forum or the [GitHub tracker](https://github.com/NazaraEngine/NazaraEngine/issues)    
- [Fork the project](https://github.com/NazaraEngine/NazaraEngine/fork) on GitHub and [push your changes](https://github.com/NazaraEngine/NazaraEngine/pulls)  
- Talking about Nazara Engine to other people, spread the word!  
- Doing anything else that might help us

## Links

[Discord](https://discord.gg/MvwNx73)  