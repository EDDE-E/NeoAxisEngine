/*-------------------------------------------------------------------------
This source file is a part of OGRE
(Object-oriented Graphics Rendering Engine)

For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2009 Torus Knot Software Ltd
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE

You may alternatively use this source under the terms of a specific version of
the OGRE Unrestricted License provided you have obtained such a license from
Torus Knot Software Ltd.
-------------------------------------------------------------------------*/
#ifndef __SceneManager_H__
#define __SceneManager_H__

// Precompiler options
#include "OgrePrerequisites.h"

#include "OgreString.h"
#include "OgrePlane.h"
#include "OgreQuaternion.h"
#include "OgreColourValue.h"
#include "OgreCommon.h"
//#include "OgreAutoParamDataSource.h"
//#include "OgreAnimationState.h"
#include "OgrePixelFormat.h"
#include "OgreResourceGroupManager.h"
#include "OgreMatrix4.h"
//#include "OgreCamera.h"
//#include "OgreGpuParametersContainer.h"
//#include "OgreLodListener.h"

namespace Ogre {
	/** \addtogroup Core
	*  @{
	*/
	/** \addtogroup Scene
	*  @{
	*/

	class _OgreExport SceneManager : public SceneMgtAlloc
    {
    public:
		 public:

		/// Instance name
		String mName;

        ///// Current ambient light, cached for RenderSystem
        //ColourValue mAmbientLight;

        /// The rendering system to send the scene to
        RenderSystem *mDestRenderSystem;

    public:
        /** Constructor.
        */
        SceneManager(const String& instanceName);

        /** Default destructor.
        */
        virtual ~SceneManager();

		RenderSystem *getDestinationRenderSystem();
    };

	/** @} */
	/** @} */


} // Namespace



#endif
