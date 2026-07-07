layout(location = 0) in vec3 vPosIn;
layout(location = 1) in vec4 vDiffuseIn;
layout(location = 2) in vec4 uv1;
layout(location = 3) in vec2 uv2;

out vec2 uv;

void main()
{
    gl_Position = vec4(vPosIn.xy, 1.0, 1.0);
    uv = uv1.xy;
}
