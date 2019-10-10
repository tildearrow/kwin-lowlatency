#version 140

// thanks Steven Lu

// KWin
uniform mat4 projection;
uniform mat4 modelview;
uniform mat4 modelViewProjectionMatrix;
uniform mat4 textureMatrix;

in vec4 position;
in vec4 texcoord;

uniform mat4 mvm;
uniform vec2 viewport;
uniform vec2 in_pos;
// this is vertex position
uniform vec2 in_center;
uniform vec4 in_vel;
uniform vec2 in_sizes;
// those were the per-object velocity related quantities
// I pack omega*dt into in_vel.z, and in_vel.w is max-displacement
// for calculating samples
uniform float in_alpha;
varying float f_alpha;
varying vec2 f_center;
out vec3 f_vel;
out float samples; // # of samples to blur
varying mat2 rot_per_sample; // A special scale-rotate matrix
out vec2 f_sceneCoord; // NDC to tex coord
varying mat4 f_scale;
void main (void) {
mat4 proj_modelview_mat = projection * modelview;
f_scale[0]=vec4(1.0+abs(in_vel.x),0.0,0.0,-abs(in_vel.x)*(in_pos.x+((sign(in_vel.x)>0.5)?in_sizes.x:0.0)));
f_scale[1]=vec4(0.0,1.0+abs(in_vel.y),0.0,-abs(in_vel.y)*(in_pos.y+((sign(in_vel.y)>0.5)?in_sizes.y:0.0)));
f_scale[2]=vec4(0.0,0.0,1.0,0.0);
f_scale[3]=vec4(0.0,0.0,0.0,1.0);
/*f_scale[0]=vec4(1.0,0.0,0.0,0.0);
f_scale[1]=vec4(0.0,1.0,0.0,0.0);
f_scale[2]=vec4(0.0,0.0,1.0,0.0);
f_scale[3]=vec4(0.0,0.0,0.0,1.0);*/
gl_Position = modelViewProjectionMatrix * vec4(position.xyzw*f_scale);
f_sceneCoord = vec2( texcoord)*(1+abs(in_vel.xy))-max(vec2(0.0,0.0),in_vel.xy);
f_center = ((modelViewProjectionMatrix * vec4(in_center,0.0,1.0)).xy
+ vec2(1.0,1.0))*0.5; // transform the center to clip space
f_vel.xy=in_vel.xy;
//f_vel.xy = (proj_modelview_mat * vec4(in_vel.xy,0.0,0.0)).xy * 0.5;
// velocity also need to be in clip space
// careful! We don't shift this one, only scale
f_vel.z = in_vel.z; // Store omega in z-comp in radians
samples = min(50,(modelViewProjectionMatrix * vec4(in_vel.w,0,0,0)).x
* viewport.x * 2 + 1);
// w here is not omega it is the max disp value from CPU
float theta = in_vel.z/(samples);
float cost = cos(theta); float sint = sin(theta);
float aspect = viewport.x/viewport.y;
rot_per_sample[0] = vec2(cost,sint*aspect); rot_per_sample[1] = vec2(-sint/aspect,cost);
// the rotation matrix is actually a scale and rotate matrix.
// the rotation must be correct in world space but is manipulated by the
// fragment shader in NDC which requires aspect correction.
}
