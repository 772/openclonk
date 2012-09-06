
#version 110

// Input textures
uniform sampler2D landscapeTex[2];
uniform sampler2D lightTex;
uniform sampler2D scalerTex;
uniform sampler3D materialTex;

// Resolution of the landscape texture
uniform vec2 resolution;

// Center position
uniform vec2 center;

// Use sampler if the GPU doesn't support enough uniforms to
// get the matMap as an array
#if MAX_FRAGMENT_UNIFORM_COMPONENTS < 259
#define BROKEN_ARRAYS_WORKAROUND
#endif

// Texture map
#define BROKEN_ARRAYS_WORKAROUND
#ifdef BROKEN_ARRAYS_WORKAROUND
uniform sampler1D matMapTex;
#else
uniform float matMap[256];
#endif
uniform int materialDepth;

// Expected parameters for the scaler
const vec2 scalerStepX = vec2(1.0 / 8.0, 0.0);
const vec2 scalerStepY = vec2(0.0, 1.0 / 32.0);
const vec2 scalerOffset = scalerStepX / 3.0 + scalerStepY / 3.0;
const vec2 scalerPixel = vec2(scalerStepX.x, scalerStepY.y) / 3.0;

#ifdef NO_TEXTURE_LOD_IN_FRAGMENT
#define texture1DLod(t,c,l) texture1D(t,c)
#define texture2DLod(t,c,l) texture2D(t,c)
#endif

// Converts the pixel range 0.0..1.0 into the integer range 0..255
int f2i(float x) {
	return int(x * 255.9);
}

float queryMatMap(int pix)
{
#ifdef BROKEN_ARRAYS_WORKAROUND
	int idx = f2i(texture1D(matMapTex, float(pix) / 256.0 + 0.5 / 256.0).r);
	return float(idx) / 256.0 + 0.5 / float(materialDepth);
#else
	return matMap[pix];
#endif
}

float dotc(vec2 v1, vec2 v2)
{
	vec3 v1p = normalize(vec3(v1, 0.3));
	vec3 v2p = normalize(vec3(v2, 0.3));
	return dot(v1p, v2p);
}

void main()
{

	// full pixel steps in the landscape texture (depends on landscape resolution)
	vec2 fullStep = vec2(1.0, 1.0) / resolution;
	vec2 fullStepX = vec2(fullStep.x, 0.0);
	vec2 fullStepY = vec2(0.0, fullStep.y);

	vec2 texCoo = gl_TexCoord[0].st;
	
	// calculate pixel position in landscape, find center of current pixel
	vec2 pixelCoo = texCoo * resolution;
	vec2 centerCoo = (floor(pixelCoo) + vec2(0.5, 0.5)) / resolution;

	// our pixel color (with and without interpolation)
	vec4 lpx = texture2D(landscapeTex[0], centerCoo);
	vec4 rlpx = texture2D(landscapeTex[0], texCoo);

	// find scaler coordinate
	vec2 scalerCoo = scalerOffset + mod(pixelCoo, vec2(1.0, 1.0)) * scalerPixel;

#ifdef SCALER_IN_GPU
	if(texture2D(landscapeTex[0], centerCoo - fullStepX - fullStepY).r == lpx.r)
		scalerCoo += scalerStepX;
	if(texture2D(landscapeTex[0], centerCoo             - fullStepY).r == lpx.r)
		scalerCoo += 2.0 * scalerStepX;
	if(texture2D(landscapeTex[0], centerCoo + fullStepX - fullStepY).r == lpx.r)
		scalerCoo += 4.0 * scalerStepX;
		
	if(texture2D(landscapeTex[0], centerCoo - fullStepX            ).r == lpx.r)
		scalerCoo += scalerStepY;
	if(texture2D(landscapeTex[0], centerCoo + fullStepX            ).r == lpx.r)
		scalerCoo += 2.0 * scalerStepY;
	
	if(texture2D(landscapeTex[0], centerCoo - fullStepX + fullStepY).r == lpx.r)
		scalerCoo += 4.0 * scalerStepY;
	if(texture2D(landscapeTex[0], centerCoo             + fullStepY).r == lpx.r)
		scalerCoo += 8.0 * scalerStepY;
	if(texture2D(landscapeTex[0], centerCoo + fullStepX + fullStepY).r == lpx.r)
		scalerCoo += 16.0 * scalerStepY;

#else

	int iScaler = f2i(lpx.a), iRow = iScaler / 8;
	scalerCoo.x += float(iScaler - iRow * 8) / 8.0;
	scalerCoo.y += float(iScaler / 8) / 32.0;
	
#endif

	// Note: scalerCoo will jump around a lot, causing some GPUs to apparantly get confused with
	//       the level-of-detail calculation. We therefore try to disable LOD.
	vec4 spx = texture2DLod(scalerTex, scalerCoo, 0.0);

	// gen3 other coordinate calculation. Still struggles a bit with 3-ways
	vec2 otherCoo = centerCoo + fullStep * floor(vec2(-0.5, -0.5) + spx.gb * 255.0 / 64.0);
	vec4 lopx = texture2D(landscapeTex[0], otherCoo);

	// Get material pixels
	float mi = queryMatMap(f2i(lpx.r));
	vec2 tcoo = texCoo * resolution / vec2(512.0, 512.0) * vec2(4.0, 4.0);
	vec4 mpx = texture3D(materialTex, vec3(tcoo, mi));
	vec4 npx = texture3D(materialTex, vec3(tcoo, mi+0.5));
	float omi = queryMatMap(f2i(lopx.r));
	vec4 ompx = texture3D(materialTex, vec3(tcoo, omi));

	// Brightness
	vec4 lipx = texture2D(lightTex, gl_TexCoord[1].st);
	float ambientBright = lipx.r, shadeBright = ambientBright;	
	vec2 normal = (mix(rlpx.yz, lpx.yz, spx.a) + npx.xy - vec2(1.0, 1.0));
	vec2 normal2 = (lopx.yz + npx.xy - vec2(1.0, 1.0));
	vec2 light_dir = vec2(1.0, 1.0) - lipx.yz * 3.0;
	float bright = 2.0 * shadeBright * dotc(normal, light_dir);
	float bright2 = 2.0 * shadeBright * dotc(normal2, light_dir);

	gl_FragColor = mix(
		vec4(bright2 * ompx.rgb, ompx.a),
		vec4(bright * mpx.rgb, mpx.a),
		spx.r);
}
