in vec4 _uv;
out vec4 fragColor;

void main()
{
    vec3 ret = texture(sampler_main, _uv.xy).xyz;
    fragColor = vec4(ret, 1.0);
}
