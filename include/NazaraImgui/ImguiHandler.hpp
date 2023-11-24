#pragma once

/*
    ImguiHandler.hpp
    Inherit from this base class for your Imgui code to be called by the Imgui renderer
*/

namespace Nz
{
    struct ImguiHandler
    {
    public:
        virtual void OnRenderImgui() = 0;
    };
}