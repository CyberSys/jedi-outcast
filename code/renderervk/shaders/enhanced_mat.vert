#version 450

layout(push_constant) uniform Transform {
    mat4 mvp;
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord0;
layout(location = 3) in vec2 in_tex_coord1;
layout(location = 4) in vec3 in_attr_normal;
layout(location = 5) in vec4 in_attr_tangent;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 2) out vec2 frag_tex_coord1;
layout(location = 3) out vec4 var_LightDir;
layout(location = 4) out float u_NormalScale;
layout(location = 5) out vec4 var_Normal;
layout(location = 6) out vec4 var_Tangent;
layout(location = 7) out vec4 var_Bitangent;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    var_LightDir = vec4(in_light_dir, 0.0);
    u_NormalScale = in_normal_scale;
    
    vec3 viewDir = in_u_viewOrigin - in_position;
    vec3 tangent   = in_attr_tangent.xyz;
    vec3 bitangent = cross(in_attr_normal, tangent) * in_attr_tangent.w;
    
	// store view direction in tangent space to save on varyings
	var_Normal    = vec4(in_attr_normal,    viewDir.x);
	var_Tangent   = vec4(tangent,   viewDir.y);
	var_Bitangent = vec4(bitangent, viewDir.z);
    
    frag_color = in_color;
    frag_tex_coord0 = in_tex_coord0;
    frag_tex_coord1 = in_tex_coord1;
}
