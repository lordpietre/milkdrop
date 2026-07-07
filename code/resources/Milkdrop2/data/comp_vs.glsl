layout(location = 0) in vec3 vPosIn;
layout(location = 1) in vec4 vDiffuseIn;
layout(location = 2) in vec4 uv_in;
layout(location = 3) in vec2 rad_ang_in;

out vec4 _vDiffuse;
out vec4 _uv;
out vec2 _rad_ang;

void main()
{
    gl_Position = vec4(vPosIn.x, vPosIn.y, vPosIn.z, 1.0);
    _vDiffuse = vDiffuseIn;
    _uv = uv_in;
    _rad_ang = rad_ang_in.xy;
}
