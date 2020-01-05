#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;
layout(set = 1, binding = 0) uniform sampler2D texture1;
layout(set = 2, binding = 0) uniform sampler2D u_NormalMap;

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord0;
layout(location = 2) in vec2 frag_tex_coord1;
layout(location = 3) in vec4 var_LightDir;
layout(location = 4) in float u_NormalScale;
layout(location = 5) in vec4 var_Normal;
layout(location = 6) in vec4 var_Tangent;
layout(location = 7) in vec4 var_Bitangent;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int alpha_test_func = 0;

void main() {
    vec3 N, L, E;
    float NL, NH, NE, EH, attenuation;
    
    mat3 tangentToWorld = mat3(var_Tangent.xyz, var_Bitangent.xyz, var_Normal.xyz);
	/*viewDir = vec3(var_Normal.w, var_Tangent.w, var_Bitangent.w);
	E = normalize(viewDir);*/
	
    L = var_LightDir.xyz;
    float sqrLightDist = dot(L, L);
	L /= sqrt(sqrLightDist);
    attenuation  = 1.0;
    
    
    N.xy = texture(u_NormalMap, frag_tex_coord0).rg - vec2(0.5);
    
    N.xy *= u_NormalScale;
	N.z = sqrt(clamp((0.25 - N.x * N.x) - N.y * N.y, 0.0, 1.0));
	N = tangentToWorld * N;
	N = normalize(N);
	
	NL = clamp(dot(N, L), 0.0, 1.0);
	//NE = clamp(dot(N, E), 0.0, 1.0);
	//H = normalize(L + E);
	//EH = clamp(dot(E, H), 0.0, 1.0);
	//NH = clamp(dot(N, H), 0.0, 1.0);
	
    out_color = frag_color * texture(texture0, frag_tex_coord0) * texture(texture1, frag_tex_coord1) * NL;

    if (alpha_test_func == 1) {
        if (out_color.a == 0.0f) discard;
    } else if (alpha_test_func == 2) {
        if (out_color.a >= 0.5f) discard;
    } else if (alpha_test_func == 3) {
        if (out_color.a < 0.5f) discard;
    } else if (alpha_test_func == 3) {
        if (out_color.a < 0.75f) discard;
    }
}
