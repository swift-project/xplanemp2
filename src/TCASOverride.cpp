/*
 * Copyright (c) 2004, Ben Supnik and Chris Serio.
 * Copyright (c) 2018, Chris Collins.
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

#include "TCASOverride.h"
#include "XPMPMultiplayerVars.h"

#include <vector>
#include <algorithm>
#include <XPLMDataAccess.h>
#include <XPLMPlanes.h>
#include <XPLMDisplay.h>
#include <XPLMProcessing.h>

/******************************************************************************

 The "TCAS hack" is no longer needed as Laminar now provide a proper TCAS API:
 https://developer.x-plane.com/article/overriding-tcas-and-providing-traffic-information/

 ******************************************************************************/

XPLMDataRef							TCAS::gOverrideRef = nullptr;
XPLMDataRef							TCAS::gXCoordRef = nullptr;
XPLMDataRef							TCAS::gYCoordRef = nullptr;
XPLMDataRef							TCAS::gZCoordRef = nullptr;
XPLMDataRef							TCAS::gModeSRef = nullptr;
XPLMDataRef							TCAS::gAltitudeRef = nullptr;	// Current aircraft altitude (for TCAS)
bool								TCAS::gTCASHooksRegistered = false;
const std::size_t					TCAS::gMaxTCASItems = 63;


void
TCAS::Init()
{
	gOverrideRef = XPLMFindDataRef("sim/operation/override/override_TCAS");
	gXCoordRef = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/x");
	gYCoordRef = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/y");
	gZCoordRef = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/z");
	gModeSRef = XPLMFindDataRef("sim/cockpit2/tcas/targets/modeS_id");
	gAltitudeRef = XPLMFindDataRef("sim/flightmodel/position/elevation");
}

void
TCAS::EnableHooks()
{
	if (!gTCASHooksRegistered) {
		XPLMSetDatai(gOverrideRef, 1);
		gTCASHooksRegistered = true;
	}
}

void
TCAS::DisableHooks()
{
	if (gTCASHooksRegistered) {
		XPLMSetDatai(gOverrideRef, 0);
		gTCASHooksRegistered = false;
	}
}

std::vector<TCAS::plane_record> TCAS::gTCASPlanes;

void
TCAS::cleanFrame()
{
	gTCASPlanes.clear();
}

void
TCAS::addPlane(float distanceSqr, float x, float y, float z, bool /*isReportingAltitude*/, void *plane)
{
	int mode_S = reinterpret_cast<std::uintptr_t>(plane) & 0xffffffu;
	gTCASPlanes.push_back({ distanceSqr, x, y, z, mode_S });
}

void
TCAS::pushPlanes()
{
	std::sort(gTCASPlanes.begin(), gTCASPlanes.end());
	const int count = static_cast<int>((std::min)(gMaxTCASItems, gTCASPlanes.size()));
	XPLMSetActiveAircraftCount(count + 1);
	auto plane = gTCASPlanes.begin();
	for (int i = 0; i < count; ++i, ++plane)
	{
		XPLMSetDatavf(gXCoordRef, &plane->x, i + 1, 1);
		XPLMSetDatavf(gYCoordRef, &plane->y, i + 1, 1);
		XPLMSetDatavf(gZCoordRef, &plane->z, i + 1, 1);
		XPLMSetDatavi(gModeSRef, &plane->mode_S, i + 1, 1);
	}
}
