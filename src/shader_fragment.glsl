#version 330 core

in vec4 position_world;
in vec4 normal;
in vec4 position_model;
in vec2 texcoords;

uniform vec4 camera_position;

struct Material
{
    sampler2D diffuse_texture;
    float specular_strength;
    float shininess;
    float ambient_strength;
    vec2 uv_scale;
};

struct PointLight
{
    vec3 position;
    vec3 color;
    float ambient_strength;
    float diffuse_strength;
    float specular_strength;
    float constant;
    float linear;
    float quadratic;
};

#define MAX_LIGHTS 12

uniform int num_lights;
uniform Material material;
uniform PointLight lights[MAX_LIGHTS];

out vec4 color;

void main()
{
    vec2 uv = texcoords * material.uv_scale;
    vec3 albedo = texture(material.diffuse_texture, uv).rgb;

    vec3 P = position_world.xyz;
    vec3 N = normalize(normal.xyz);
    vec3 V = normalize(camera_position.xyz - P);

    vec3 result = vec3(0.0);
    int light_count = min(num_lights, MAX_LIGHTS);
    for (int i = 0; i < light_count; ++i)
    {
        vec3 L = normalize(lights[i].position - P);
        float dist = length(lights[i].position - P);
        float attenuation = 1.0 / (lights[i].constant + lights[i].linear * dist + lights[i].quadratic * dist * dist);

        float lambert = max(dot(N, L), 0.0);
        vec3 R = reflect(-L, N);

        float spec = 0.0;
        if (lambert > 0.0)
            spec = pow(max(dot(R, V), 0.0), material.shininess);

        vec3 ambient = lights[i].ambient_strength * material.ambient_strength * albedo * lights[i].color;
        vec3 diffuse = lights[i].diffuse_strength * lambert * albedo * lights[i].color;
        vec3 specular = lights[i].specular_strength * spec * material.specular_strength * lights[i].color;

        result += ambient + attenuation * (diffuse + specular);
    }

    color.rgb = pow(result, vec3(1.0 / 2.2));
    color.a = 1.0;
}
