#version 330 core

in vec4 position_world;
in vec4 normal;
in vec4 position_model;
in vec2 texcoords;

uniform int object_id;
uniform sampler2D TextureImage0; // parede
uniform sampler2D TextureImage1; // opcional para chão/teto

uniform vec4 camera_position;
uniform vec4 light_position;
uniform vec3 light_color;

out vec4 color;

#define OBJ_WALL    0
#define OBJ_FLOOR   1
#define OBJ_CEILING 2

void main()
{
    vec3 albedo;
    vec3 Ks;
    float shininess;

    if (object_id == OBJ_WALL)
    {
        albedo = texture(TextureImage0, texcoords).rgb;
        Ks = vec3(0.85, 0.85, 0.85); // brilho glossy
        shininess = 96.0;
    }
    else if (object_id == OBJ_FLOOR)
    {
        albedo = texture(TextureImage1, texcoords).rgb * 0.7;
        Ks = vec3(0.08, 0.08, 0.08);
        shininess = 10.0;
    }
    else
    {
        albedo = vec3(0.78, 0.78, 0.80);
        Ks = vec3(0.25, 0.25, 0.25);
        shininess = 24.0;
    }

    vec3 P = position_world.xyz;
    vec3 N = normalize(normal.xyz);
    vec3 L = normalize(light_position.xyz - P);
    vec3 V = normalize(camera_position.xyz - P);
    vec3 R = reflect(-L, N);

    float dist = length(light_position.xyz - P);
    float attenuation = 1.0 / (1.0 + 0.12 * dist + 0.032 * dist * dist);

    float lambert = max(dot(N, L), 0.0);
    float spec = 0.0;
    if (lambert > 0.0)
        spec = pow(max(dot(R, V), 0.0), shininess);

    vec3 ambient = 0.10 * albedo;
    vec3 diffuse = lambert * albedo * light_color;
    vec3 specular = spec * Ks * light_color;
    vec3 result = ambient + attenuation * (diffuse + specular);

    color.rgb = pow(result, vec3(1.0 / 2.2));
    color.a = 1.0;
}
