in vec2 uv;
out vec4 fragColor;

uniform sampler2D sampler_main;
uniform vec4 _c0;
uniform vec4 _c5;
uniform vec4 _c6;

#define srctexsize _c0
#define w1 _c5.x
#define w2 _c5.y
#define d1 _c5.z
#define d2 _c5.w
#define edge_darken_c1 _c6.y
#define edge_darken_c2 _c6.z
#define edge_darken_c3 _c6.w
#define w_div   _c6.x

void main()
{
    vec2 uv2 = uv + srctexsize.zw * vec2(1.0, 0.0);

    vec3 blur =
            ( texture(sampler_main, uv2 + vec2(0.0,  d1 * srctexsize.w) ).xyz
            + texture(sampler_main, uv2 + vec2(0.0, -d1 * srctexsize.w) ).xyz) * w1 +
            ( texture(sampler_main, uv2 + vec2(0.0,  d2 * srctexsize.w) ).xyz
            + texture(sampler_main, uv2 + vec2(0.0, -d2 * srctexsize.w) ).xyz) * w2
            ;
    blur *= w_div;

    float t = min(min(uv.x, uv.y), 1.0 - max(uv.x, uv.y));
    t = sqrt(t);
    t = edge_darken_c1 + edge_darken_c2 * clamp(t * edge_darken_c3, 0.0, 1.0);
    blur *= t;

    fragColor = vec4(blur, 1.0);
}
