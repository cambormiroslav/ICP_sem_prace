#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D texture_diffuse;

// Ambient light
uniform vec3 ambientColor;

// Directional light
uniform vec3 dirLightDirection;
uniform vec3 dirLightColor;

// Spotlight
uniform vec3 spotPos;
uniform vec3 spotDir;
uniform float spotCutOff;
uniform float spotOuterCutOff;
uniform vec3 spotColor;

// View position (camera)
uniform vec3 viewPos;

void main()
{
    // Texturovaný materiál
    vec4 texColor = texture(texture_diffuse, TexCoord); // <-- bereme texColor i s alpha!

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // === Ambient ===
    vec3 ambient = ambientColor * texColor.rgb;

    // === Directional light ===
    vec3 lightDir = normalize(-dirLightDirection);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * dirLightColor * texColor.rgb;

    // === Specular ===
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = spec * vec3(1.0);

    // === Spotlight ===
    vec3 spotLightDir = normalize(spotPos - FragPos);
    float theta = dot(spotLightDir, normalize(-spotDir));
    float epsilon = spotCutOff - spotOuterCutOff;
    float intensity = clamp((theta - spotOuterCutOff) / epsilon, 0.0, 1.0);

    float diffSpot = max(dot(norm, -spotLightDir), 0.0);
    vec3 spotDiffuse = diffSpot * spotColor * texColor.rgb * intensity;

    vec3 result = ambient + diffuse + specular + spotDiffuse;

    FragColor = vec4(result, texColor.a); // <-- zde použijeme průhlednost!
}
