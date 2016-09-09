//============================================================================
// Name        : VR_Shaders.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VR Shaders
//============================================================================

#include <string>

namespace p3d {

	const char *VR_SHADER_CONTROLLER =	"#ifdef VERTEX\n"
		"uniform mat4 matrix;\n"
		"attribute vec4 position;\n"
		"attribute vec3 v3ColorIn;\n"
		"varying vec4 v4Color;\n"
		"void main()\n"
		"{\n"
		"	v4Color.xyz = v3ColorIn; v4Color.a = 1.0;\n"
		"	gl_Position = matrix * position;\n"
		"}\n"
		"#endif\n"
		"#ifdef FRAGMENT\n"
		"varying vec4 v4Color;\n"
		"void main()\n"
		"{\n"
		"   gl_FragColor = v4Color;\n"
		"}\n"
		"#endif";

	const char *VR_SHADER_DISTORTION =	"#ifdef VERTEX\n"
		"attribute vec4 position;\n"
		"attribute vec2 v2UVredIn;\n"
		"attribute vec2 v2UVGreenIn;\n"
		"attribute in vec2 v2UVblueIn;\n"
		"noperspective varying vec2 v2UVred;\n"
		"noperspective varying vec2 v2UVgreen;\n"
		"noperspective varying vec2 v2UVblue;\n"
		"void main()\n"
		"{\n"
		"	v2UVred = v2UVredIn;\n"
		"	v2UVgreen = v2UVGreenIn;\n"
		"	v2UVblue = v2UVblueIn;\n"
		"	gl_Position = position;\n"
		"}\n"
		"#endif\n"
		"#ifdef FRAGMENT\n"
		"#version 410 core\n"
		"uniform sampler2D mytexture;\n"
		"noperspective varying vec2 v2UVred;\n"
		"noperspective varying vec2 v2UVgreen;\n"
		"noperspective varying vec2 v2UVblue;\n"
		"void main()\n"
		"{\n"
		"	float fBoundsCheck = ( (dot( vec2( lessThan( v2UVgreen.xy, vec2(0.05, 0.05)) ), vec2(1.0, 1.0))+dot( vec2( greaterThan( v2UVgreen.xy, vec2( 0.95, 0.95)) ), vec2(1.0, 1.0))) );\n"
		"	if( fBoundsCheck > 1.0 )\n"
		"	{ outputColor = vec4( 0, 0, 0, 1.0 ); }\n"
		"	else\n"
		"	{\n"
		"		float red = texture2D(mytexture, v2UVred).x;\n"
		"		float green = texture2D(mytexture, v2UVgreen).y;\n"
		"		float blue = texture2D(mytexture, v2UVblue).z;\n"
		"		gl_FragColor = vec4( red, green, blue, 1.0  ); }\n"
		"}\n"
		"#endif";
}