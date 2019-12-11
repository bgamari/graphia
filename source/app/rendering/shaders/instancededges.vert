#version 330 core

layout (location = 0) in vec3   vertexPosition;
layout (location = 1) in vec3   vertexNormal;
layout (location = 2) in vec2   vertexTexCoord;

layout (location = 3) in vec3   sourcePosition; // The position of the source node
layout (location = 4) in vec3   targetPosition; // The position of the target node
layout (location = 5) in float  sourceSize; // The size of the source node
layout (location = 6) in float  targetSize; // The size of the target node
layout (location = 7) in int    edgeType; // The type of the edge

layout (location = 8) in int    component; // The component index

layout (location = 9)  in float size; // The size of the edge
layout (location = 10) in vec3  outerColor; // The outside color of the edge
layout (location = 11) in vec3  innerColor; // The inside color of the edge (used for multi edges)
layout (location = 12) in float selected;

flat out uint element;
out vec3 vPosition;
out vec3 vNormal;
out vec3 innerVColor;
out vec3 outerVColor;
out float vSelected;
out vec2 uv;
out float lightOffset;
out float lightScale;

uniform samplerBuffer componentData;
uniform int componentDataElementSize;

const float ARROW_HEAD_CUTOFF_Y = 0.24;
const float MAX_ARROW_HEAD_LENGTH = 0.25;

bool equals(float value, float target)
{
    const float EPSILON = 0.001;
    return (abs(value - target) <= EPSILON);
}

mat4 makeOrientationMatrix(vec3 up)
{
    mat3 m;

    m[1] = up;
    m[0].y = -m[1].x;
    m[0].z = m[1].y;
    m[0].x = m[1].z;

    float d = dot(m[0], m[1]);
    m[0] -= (d * m[1]);
    m[0] = normalize(m[0]);
    m[2] = cross(m[0], m[1]);

    return mat4(m);
}

int componentDataOffset()
{
    return component * componentDataElementSize;
}

mat4 mat4FromComponentData(int offset)
{
    mat4 m;
    int index = componentDataOffset() + offset;

    for(int j = 0; j < 4; j++)
    {
        for(int i = 0; i < 4; i++)
        {
            m[j][i] = texelFetch(componentData, index).r;
            index++;
        }
    }

    return m;
}

float floatFromComponentData(int offset)
{
    int index = componentDataOffset() + offset;
    return texelFetch(componentData, index).r;
}

void main()
{
    // Offset the index so that it doesn't overlap with the node indicies
    element = uint(gl_InstanceID) + (1u << 31);

    float edgeLength = distance(sourcePosition, targetPosition);
    float edgeLengthMinusNodeRadii = edgeLength - (sourceSize + targetSize);
    vec3 midpoint = mix(sourcePosition, targetPosition, 0.5);
    mat4 orientationMatrix = makeOrientationMatrix(normalize(targetPosition - sourcePosition));

    // Apply inverse to the normals
    vec3 scaledVertexNormal = vertexNormal;
    scaledVertexNormal.xz /= size;
    scaledVertexNormal.y /= edgeLength;

    mat4 modelViewMatrix = mat4FromComponentData(0);
    mat4 projectionMatrix = mat4FromComponentData(16);
    lightOffset = floatFromComponentData(32);
    lightScale = floatFromComponentData(33);

    vec3 edgeVertexPosition = vec3(0.0);

    // Cylinder Edge
    if(edgeType == 0 && !equals(length(vertexPosition.xz), 1.0))
    {
        // These vertices form the cone of the arrow mesh, which we
        // don't want to render at all, when in cylinder mode
    }
    else if(edgeType == 0 && vertexPosition.y > 0.0)
    {
        // Reposition the top of the cylinder to the target
        vec3 scaledVertexPosition = vertexPosition;
        scaledVertexPosition.xz *= size;
        scaledVertexPosition.y = (edgeLength * 0.5);
        uv = vec2(0.0, (scaledVertexPosition.y / edgeLengthMinusNodeRadii) + 0.5);

        edgeVertexPosition = (orientationMatrix * vec4(scaledVertexPosition, 1.0)).xyz + midpoint;
    }
    // Arrow Head Edge
    else if(edgeType == 1 && vertexPosition.y > ARROW_HEAD_CUTOFF_Y)
    {
        vec3 conePosition = vertexPosition;
        // Position so it points to origin
        conePosition.y -= 0.5;
        // Scale to match edgeSize
        conePosition *= size;
        // Scale arrow head (cone) length
        conePosition.y *= 16;

        // Limit the cone size to MAX_ARROW_HEAD_LENGTH * edge length
        if(abs(conePosition.y) > edgeLength * MAX_ARROW_HEAD_LENGTH)
            conePosition.y = -edgeLength * MAX_ARROW_HEAD_LENGTH;

        uv = vec2(0.0, 1.0 + ((conePosition.y) / edgeLengthMinusNodeRadii));

        // Offset cone position to point at edge of node
        conePosition.y -= targetSize;

        edgeVertexPosition = (orientationMatrix * vec4(conePosition, 1.0)).xyz + targetPosition;
    }
    else
    {
        // Scale edge vertices
        vec3 scaledVertexPosition = vertexPosition;
        scaledVertexPosition.xz *= size;
        scaledVertexPosition.y *= edgeLength;

        uv = vec2(0.0, -sourceSize / edgeLengthMinusNodeRadii);

        edgeVertexPosition = (orientationMatrix * vec4(scaledVertexPosition, 1.0)).xyz + midpoint;
    }

    vec3 edgeVertexPositionViewSpace = vPosition =
        (modelViewMatrix * vec4(edgeVertexPosition, 1.0)).xyz;

    mat3 normalMatrix = transpose(inverse(mat3(modelViewMatrix * orientationMatrix)));
    vNormal = normalMatrix * scaledVertexNormal;
    innerVColor = innerColor;
    outerVColor = outerColor;
    vSelected = selected;
    gl_Position = projectionMatrix * vec4(edgeVertexPositionViewSpace, 1.0);
}
