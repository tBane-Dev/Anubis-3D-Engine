#version 330 core

in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform bool uTextured;
uniform vec4 uTint;

uniform bool uPicking;
uniform vec3 uPickColor;

out vec4 fragColor;

void main() {
    // Preserve the exact model ID color during picking.
    if (uPicking) {
        fragColor = vec4(uPickColor, 1.0);
        return;
    }

    vec4 baseColor = vec4(1.0);

    if (uTextured) {
        baseColor = texture(uTexture, vTexCoord);
    }

    fragColor = baseColor * uTint;
}