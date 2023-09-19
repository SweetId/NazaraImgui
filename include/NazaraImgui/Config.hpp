#pragma once

#include <Nazara/Core/Color.hpp>
#include <Nazara/Math/Vector2.hpp>

#define IM_VEC2_CLASS_EXTRA                                             \
    template <typename T>                                               \
    ImVec2(const Nz::Vector2<T>& v) {                                   \
        x = static_cast<float>(v.x);                                    \
        y = static_cast<float>(v.y);                                    \
    }                                                                   \
                                                                        \
    template <typename T>                                               \
    operator Nz::Vector2<T>() const {                                   \
        return Nz::Vector2<T>(x, y);                                    \
    }

#define IM_VEC4_CLASS_EXTRA                                             \
    ImVec4(const Nz::Color & c)                                         \
        : x(c.r), y(c.g), z(c.b), w(c.a)                                \
    {}                                                                  \
    operator Nz::Color() const {                                        \
        return Nz::Color(x, y , z, w);                                  \
    }

#if defined(NAZARA_STATIC)
	#define NAZARA_IMGUI_API
#else
    #define NAZARA_IMGUI_API
	#ifdef NAZARA_IMGUI_BUILD
	    //#define NAZARA_IMGUI_API NAZARA_EXPORT
	#else
        //#define NAZARA_IMGUI_API NAZARA_IMPORT
	#endif
#endif