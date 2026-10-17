//============================================================================
// Name        : ViewFovScale.h
// Description : A factor on the active camera's field of view, set by a game.
//               A game zooms its view (a soft aim, a binocular, a cutscene)
//               without knowing what the camera's own FOV is: the host - the
//               player, or the editor during Play - multiplies the camera's
//               FOV by this. 1 is the camera as set. Process-wide; the editor
//               puts it back to 1 when Play stops.
//============================================================================

#ifndef VIEWFOVSCALE_H
#define VIEWFOVSCALE_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>

namespace p3d {

	PYROS3D_API f32 GetViewFovScale();
	// clamped to 0.1 .. 2
	PYROS3D_API void SetViewFovScale(const f32 scale);

}

#endif /* VIEWFOVSCALE_H */
