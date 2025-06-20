#version 430 core

uniform vec3 view_position;
uniform mat4 matrix_viewProjection;
uniform sampler2D blueNoiseTex32;
uniform int plane;  // 0: x (yz), 1: y (xz), 2: z (xy)

in vec3 worldFar;
in vec3 worldNear;

out vec4 FragColor;

// Plane definitions
const vec4 planes[3] = vec4[3](
    vec4(1.0, 0.0, 0.0, 0.0),  // X plane
    vec4(0.0, 1.0, 0.0, 0.0),  // Y plane
    vec4(0.0, 0.0, 1.0, 0.0)   // Z plane
);

// Axis colors
const vec3 colors[3] = vec3[3](
    vec3(1.0, 0.2, 0.2),  // Red for X
    vec3(0.2, 1.0, 0.2),  // Green for Y
    vec3(0.2, 0.2, 1.0)   // Blue for Z
);

// Axis indices for each plane
const int axis0[3] = int[3](1, 0, 0);  // First axis index
const int axis1[3] = int[3](2, 2, 1);  // Second axis index

bool intersectPlane(inout float t, vec3 pos, vec3 dir, vec4 plane) {
    float d = dot(dir, plane.xyz);
    if (abs(d) < 1e-06) {
        return false;
    }
    
    float n = -(dot(pos, plane.xyz) + plane.w) / d;
    if (n < 0.0) {
        return false;
    }
    
    t = n;
    return true;
}

// Grid rendering using derivatives for anti-aliasing
float pristineGrid(in vec2 uv, in vec2 ddx, in vec2 ddy, vec2 lineWidth) {
    vec2 uvDeriv = vec2(length(vec2(ddx.x, ddy.x)), length(vec2(ddx.y, ddy.y)));
    bvec2 invertLine = bvec2(lineWidth.x > 0.5, lineWidth.y > 0.5);
    vec2 targetWidth = vec2(
        invertLine.x ? 1.0 - lineWidth.x : lineWidth.x,
        invertLine.y ? 1.0 - lineWidth.y : lineWidth.y
    );
    vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    vec2 lineAA = uvDeriv * 1.5;
    vec2 gridUV = abs(fract(uv) * 2.0 - 1.0);
    gridUV.x = invertLine.x ? gridUV.x : 1.0 - gridUV.x;
    gridUV.y = invertLine.y ? gridUV.y : 1.0 - gridUV.y;
    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    
    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);
    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2.x = invertLine.x ? 1.0 - grid2.x : grid2.x;
    grid2.y = invertLine.y ? 1.0 - grid2.y : grid2.y;
    
    return mix(grid2.x, 1.0, grid2.y);
}

float calcDepth(vec3 p) {
    vec4 v = matrix_viewProjection * vec4(p, 1.0);
    return (v.z / v.w) * 0.5 + 0.5;
}

bool writeDepth(float alpha) {
    vec2 uv = fract(gl_FragCoord.xy / 32.0);
    float noise = texture(blueNoiseTex32, uv).r;
    return alpha > noise;
}

void main() {
    vec3 p = worldNear;
    vec3 v = normalize(worldFar - worldNear);
    
    // Intersect ray with the grid plane
    float t;
    if (!intersectPlane(t, p, v, planes[plane])) {
        discard;
    }
    
    // Calculate grid intersection point
    vec3 worldPos = p + v * t;
    vec2 pos;
    
    // Extract 2D position based on plane
    if (plane == 0) {
        pos = worldPos.yz;  // YZ plane
    } else if (plane == 1) {
        pos = worldPos.xz;  // XZ plane (ground)
    } else {
        pos = worldPos.xy;  // XY plane
    }
    
    vec2 ddx = dFdx(pos);
    vec2 ddy = dFdy(pos);
    
    float epsilon = 1.0 / 255.0;
    
    // Calculate fade based on distance
    float fade = 1.0 - smoothstep(400.0, 1000.0, length(worldPos - view_position));
    if (fade < epsilon) {
        discard;
    }
    
    vec2 levelPos;
    float levelSize;
    float levelAlpha;
    
    // 10m grid with colored main axes
    levelPos = pos * 0.1;
    levelSize = 2.0 / 1000.0;
    levelAlpha = pristineGrid(levelPos, ddx * 0.1, ddy * 0.1, vec2(levelSize)) * fade;
    if (levelAlpha > epsilon) {
        vec3 color;
        vec2 loc = abs(levelPos);
        if (loc.x < levelSize) {
            if (loc.y < levelSize) {
                color = vec3(1.0);  // Origin
            } else {
                color = colors[axis1[plane]];  // Second axis
            }
        } else if (loc.y < levelSize) {
            color = colors[axis0[plane]];  // First axis
        } else {
            color = vec3(0.9);  // Regular grid
        }
        FragColor = vec4(color, levelAlpha);
        gl_FragDepth = writeDepth(levelAlpha) ? calcDepth(worldPos) : 1.0;
        return;
    }
    
    // 1m grid
    levelPos = pos;
    levelSize = 1.0 / 100.0;
    levelAlpha = pristineGrid(levelPos, ddx, ddy, vec2(levelSize)) * fade;
    if (levelAlpha > epsilon) {
        FragColor = vec4(vec3(0.7), levelAlpha);
        gl_FragDepth = writeDepth(levelAlpha) ? calcDepth(worldPos) : 1.0;
        return;
    }
    
    // 0.1m grid
    levelPos = pos * 10.0;
    levelSize = 1.0 / 100.0;
    levelAlpha = pristineGrid(levelPos, ddx * 10.0, ddy * 10.0, vec2(levelSize)) * fade;
    if (levelAlpha > epsilon) {
        FragColor = vec4(vec3(0.7), levelAlpha);
        gl_FragDepth = writeDepth(levelAlpha) ? calcDepth(worldPos) : 1.0;
        return;
    }
    
    discard;
}
