#version 140

// thanks Steven Lu

uniform sampler2D sampler;
in vec2 f_sceneCoord; // this should be pretransformed texcoord to scene tex
// it is also appropriate to use as a "this pixel" position
// (in relation to center, etc)
varying float f_alpha;
varying vec2 f_center;
in vec3 f_vel;
in float samples;
varying mat2 rot_per_sample;

out vec4 fragColor;

void main (void) {
vec2 original_center = f_center - f_vel.xy;
// vel.xy is actually just the dist
vec4 accum = vec4(0,0,0,0);
mat2 cumulativeRotation;
// set identity rotation matrix
cumulativeRotation[0] = vec2(1,0); cumulativeRotation[1] = vec2(0,
1);
float samples_i = ceil(samples);
for(int i=0;i<samples_i;++i) {
float fraction = float(i)/samples_i;
vec2 pos = cumulativeRotation*(f_sceneCoord+f_vel.xy*fraction-f_center)+ f_center;
if (pos.x>1.0f || pos.x<0.0f || pos.y>1.0f || pos.y<0.0f) continue;
vec4 col = texture2D(sampler, pos);
//col=vec4(0.0f,0.0f,0.0f,0.0f);
// 1: translate to orig ctr 2: rotate by i*rot
// 3: translate back to origctr+(curctr-origctr)*i
if (col.a>0.01f) { accum = accum + vec4(col.rgba); }
cumulativeRotation = cumulativeRotation * rot_per_sample;
}
if (accum.a < 1.0/1024.) discard; // prevent divide by zero
fragColor = vec4(accum.rgb*(1.0/accum.a),accum.a/samples_i);
//fragColor=vec4(texture2D(sampler, f_sceneCoord));
//fragColor=vec4(1.0f,1.0f,1.0f,1.0f);
// This should be a non-premultiplied alpha value for use with
// saturate blending.
}
