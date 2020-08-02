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

#include <vector>
#include <XPLMDataAccess.h>
#include <XPLMPlanes.h>
#include <XPLMDisplay.h>
#include <XPLMProcessing.h>

#include "XPMPMultiplayerVars.h"

#include "TCASHack.h"

using namespace std;

/*
 * Note, this was originally in XPMPMultiplayer.cpp but was split-out.
 *
 * CC - 2018/03/02
 */

/******************************************************************************

	T H E   T C A S   H A C K

 The 1.0 SDK provides no way to add TCAS blips to a panel - we simply don't know
 where on the panel to draw.  The only way to get said blips is to manipulate
 Austin's "9 planes" plane objects, which he refers to when drawing the moving
 map.

 But how do we integrate this with our system, which relies on us doing
 the drawing (either by calling Austin's low level drawing routine or just
 doing it ourselves with OpenGL)?

 The answer is the TCAS hack.  Basically we set Austin's number of multiplayer
 planes to zero while 3-d drawing is happening so he doesn't draw.  Then during
 2-d drawing we pop this number back up to the number of planes that are
 visible on TCAS and set the datarefs to move them, so that they appear on TCAS.

 One note: since all TCAS blips are the same, we do no model matching for
 TCAS - we just place the first 9 planes at the right place.

 Our rendering loop records for us in gEnableCount how many TCAS planes should
 be visible.

 ******************************************************************************/

std::vector<XPLMDataRef>			TCAS::gMultiRef_X;
std::vector<XPLMDataRef>			TCAS::gMultiRef_Y;
std::vector<XPLMDataRef>			TCAS::gMultiRef_Z;

XPLMDataRef							TCAS::gAltitudeRef = nullptr;	// Current aircraft altitude (for TCAS)
bool								TCAS::gTCASHooksRegistered = false;
int 								TCAS::gEnableCount = 1;
int									TCAS::gMaxTCASItems = 0;


void
TCAS::Init()
{
	gAltitudeRef = XPLMFindDataRef("sim/flightmodel/position/elevation");

	// We don't know how many multiplayer planes there are - fetch as many as we can.
	int n = 1;
	char buf[100];
	XPLMDataRef d;
	while (1) {
		sprintf(buf, "sim/multiplayer/position/plane%d_x", n);
		d = XPLMFindDataRef(buf);
		if (!d) {
			break;
		}
		gMultiRef_X.push_back(d);
		sprintf(buf, "sim/multiplayer/position/plane%d_y", n);
		d = XPLMFindDataRef(buf);
		if (!d) {
			break;
		}
		gMultiRef_Y.push_back(d);
		sprintf(buf, "sim/multiplayer/position/plane%d_z", n);
		d = XPLMFindDataRef(buf);
		if (!d) {
			break;
		}
		gMultiRef_Z.push_back(d);
		++n;
	}
	gMaxTCASItems = n-1;
}

// This callback ping-pongs the multiplayer count up and back depending
// on whether we're drawing the TCAS gauges or not.
int	TCAS::ControlPlaneCount(
	XPLMDrawingPhase     /*inPhase*/,
	int                  /*inIsBefore*/,
	void *               inRefcon)
{
	if (inRefcon == nullptr) {
		XPLMSetActiveAircraftCount(1);
	} else {
		// quickly splat over multiplayer datarefs
		int tcasItems = min((int)gTCASPlanes.size(), gMaxTCASItems);
		auto tcasIter = gTCASPlanes.cbegin();
		for (int c = 0; c < tcasItems; c++, tcasIter++) {
			XPLMSetDataf(gMultiRef_X[c], tcasIter->second.x);
			XPLMSetDataf(gMultiRef_Y[c], tcasIter->second.y);
			XPLMSetDataf(gMultiRef_Z[c], tcasIter->second.z);
		}
		// and set the count
		XPLMSetActiveAircraftCount(tcasItems+1);
	}
	return 1;
}

void
TCAS::EnableHooks()
{
	if (!gTCASHooksRegistered) {
		XPLMRegisterDrawCallback(
			&TCAS::ControlPlaneCount, xplm_Phase_Gauges, 0, /* after*/ nullptr /* hide planes*/);
		XPLMRegisterDrawCallback(
			&TCAS::ControlPlaneCount, xplm_Phase_Gauges, 1, /* before */ (void *) -1 /* show planes*/);
		gTCASHooksRegistered = true;
	}
}

void
TCAS::DisableHooks()
{
	if (gTCASHooksRegistered) {
		XPLMUnregisterDrawCallback(&TCAS::ControlPlaneCount, xplm_Phase_Gauges, 0, nullptr);
		XPLMUnregisterDrawCallback(&TCAS::ControlPlaneCount, xplm_Phase_Gauges, 1, (void *) -1);
		gTCASHooksRegistered = false;
	}
}

TCAS::TCASMap TCAS::gTCASPlanes;

void
TCAS::cleanFrame()
{
	gTCASPlanes.clear();
}

void
TCAS::addPlane(float distanceSqr, float x, float y, float z, bool /*isReportingAltitude*/)
{
	gTCASPlanes.emplace( distanceSqr, plane_record{x, y, z} );
}