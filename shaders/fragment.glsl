#version 330 core

flat in vec3 NormalVS;      // Facet normal in view space (flat = uniform per facet)
flat in vec3 FaceCenterVS;  // Facet center in view space (flat = uniform per facet)

out vec4 FragColor;

uniform mat4 view;
uniform vec3 lightDirection;

void main()
{
    // Light direction in view space
    vec3 lightDirVS = normalize((view * vec4(lightDirection, 0.0)).xyz);
    vec3 norm = normalize(NormalVS);
    
    // Ambient + Diffuse
    float ambient = 0.45;
    float diffuse = max(dot(norm, lightDirVS), 0.0);
    
    // Specular
    vec3 viewDir = normalize(-FaceCenterVS);
    vec3 reflectDir = reflect(-lightDirVS, norm);
    float specular = 0.45 * pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    // Final color
    vec3 lighting = vec3(ambient + diffuse + specular);
    vec3 objectColor = vec3(0.75);
    FragColor = vec4(lighting * objectColor, 1.0);
}
