#pragma once

#include "camera.h"
#include "mesh.h"
#include "pvl/UniformGrid.hpp"
#include <functional>

class FrameBufferWidget;

namespace Mpcv {

struct Pixel {
    Pvl::Vec3f color = Pvl::Vec3f(0);
    int weight = 0;

    void add(const Pvl::Vec3f& c) {
        color = (weight * color + c) / (weight + 1);
        ++weight;
    }
};


using FrameBuffer = Pvl::UniformGrid<Pixel, 2>;

enum class RenderWire {
    NOTHING,
    DOTS,
    EDGES,
};

void renderMeshes(FrameBufferWidget* widget,
    const std::vector<TexturedMesh*>& meshes,
    const Pvl::Vec3f& dirToSun,
    const Camera camera,
    const RenderWire wire = RenderWire::NOTHING);

bool ambientOcclusion(std::vector<TexturedMesh>& meshes,
    std::function<bool(float)> progress,
    int sampleCntX = 20,
    int sampleCntY = 10);

} // namespace Mpcv
