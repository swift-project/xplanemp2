/*
 * Copyright (c) 2004, Ben Supnik and Chris Serio.
 * Copyright (c) 2018, 2020, Chris Collins.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "Renderer.h"

#include <XPLMUtilities.h>
#include <XPLMDisplay.h>
#include <XPLMProcessing.h>
#include <XPLMCamera.h>

#include "XPMPMultiplayerVars.h"
#include "MapRendering.h"
#include "TCASOverride.h"

using namespace std;

XPLMDataRef gVisDataRef = nullptr;    // Current air visiblity for culling.
XPLMProbeRef gTerrainProbe = nullptr;

void
Renderer_Init()
{
    // SETUP - mostly just fetch datarefs.
    gVisDataRef = XPLMFindDataRef("sim/graphics/view/visibility_effective_m");
    if (gVisDataRef == nullptr) {
        gVisDataRef = XPLMFindDataRef("sim/weather/visibility_effective_m");
    }
    if (gVisDataRef == nullptr) {
        XPLMDebugString(
            "WARNING: Default renderer could not find effective visibility in the sim.\n");
    }

    gTerrainProbe = XPLMCreateProbe(xplm_ProbeY);
    CullInfo::init();
    TCAS::Init();

#if RENDERER_STATS
    XPLMRegisterDataAccessor("hack/renderer/planes", xplmType_Int, 0, GetRendererStat, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             &gTotPlanes, NULL);
    XPLMRegisterDataAccessor("hack/renderer/navlites", xplmType_Int, 0, GetRendererStat, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             &gNavPlanes, NULL);
    XPLMRegisterDataAccessor("hack/renderer/objects", xplmType_Int, 0, GetRendererStat, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             &gOBJPlanes, NULL);
    XPLMRegisterDataAccessor("hack/renderer/acfs", xplmType_Int, 0, GetRendererStat, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             &gACFPlanes, NULL);
#endif
}

double Render_FullPlaneDistance = 0.0;

void
Render_PrepLists()
{
    // guard against multiple calls per frame.
    static int rendLastCycle = -1;
    int thisCycle = XPLMGetCycleNumber();
    if (thisCycle == rendLastCycle) {
        return;
    }
    rendLastCycle = thisCycle;

    TCAS::cleanFrame();

    if (gPlanes.empty()) {
        return;
    }

    // get our view information
    CullInfo gl_camera;
    XPLMCameraPosition_t x_camera;
    XPLMReadCameraPosition(&x_camera);    // only for zoom!

    // Culling - read the camera pos and figure out what's visible.
    //double maxDist = XPLMGetDataf(gVisDataRef);
    Render_FullPlaneDistance = x_camera.zoom * (5280.0 / 3.2) *
                               gConfiguration.maxFullAircraftRenderingDistance;    // Only draw planes fully within 3 miles.

    for (auto &planePair: gPlanes) {
        planePair.second->doInstanceUpdate(gl_camera);
    }

    TCAS::pushPlanes();
}


/*
 * RenderingCallback
 *
 * Originally we had nicely split up elegance.
 *
 * Alas, it was not to to be - in order to render labels at all, we needed to do our 2d render in the 3d view phases
 * with bad hacks to maintain compatibility with FSAA
 */

float
XPMP_PrepListHook(float /*inElapsedSinceLastCall*/,
                  float /*inElapsedTimeSinceLastFlightLoop*/,
                  int /*inCounter*/,
                  void * /*inRefcon*/)
{
    Render_PrepLists();

    return -1.0f;
}

void
Renderer_Attach_Callbacks()
{
    XPLMRegisterFlightLoopCallback(&XPMP_PrepListHook, -1, nullptr);

    TCAS::EnableHooks();
}

void
Renderer_Detach_Callbacks()
{
    TCAS::DisableHooks();

    XPLMUnregisterFlightLoopCallback(&XPMP_PrepListHook, nullptr);
}
