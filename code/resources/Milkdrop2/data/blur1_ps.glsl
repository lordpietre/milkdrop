in vec2 uv;
out vec4 fragColor;

uniform sampler2D sampler_main;
uniform vec4 _c0;
uniform vec4 _c1;
uniform vec4 _c2;
uniform vec4 _c3;

#define srctexsize _c0
#define w1 _c1.x
#define w2 _c1.y
#define w3 _c1.z
#define w4 _c1.w
#define d1 _c2.x
#define d2 _c2.y
#define d3 _c2.z
#define d4 _c2.w
#define fscale _c3.x
#define fbias  _c3.y
#define w_div  _c3.z

void main()
{
    vec2 uv2 = uv + srctexsize.zw * vec2(1.0, 1.0);

    vec3 blur =
            ( texture(sampler_main, uv2 + vec2( d1 * srctexsize.z, 0.0) ).xyz
            + texture(sampler_main, uv2 + vec2(-d1 * srctexsize.z, 0.0) ).xyz) * w1 +
            ( texture(sampler_main, uv2 + vec2( d2 * srctexsize.z, 0.0) ).xyz
            + texture(sampler_main, uv2 + vec2(-d2 * srctexsize.z, 0.0) ).xyz) * w2 +
            ( texture(sampler_main, uv2 + vec2( d3 * srctexsize.z, 0.0) ).xyz
            + texture(sampler_main, uv2 + vec2(-d3 * srctexsize.z, 0.0) ).xyz) * w3 +
            ( texture(sampler_main, uv2 + vec2( d4 * srctexsize.z, 0.0) ).xyz
            + texture(sampler_main, uv2 + vec2(-d4 * srctexsize.z, 0.0) ).xyz) * w4
            ;
    blur *= w_div;
    blur = blur * fscale + fbias;

    fragColor = vec4(blur, 1.0);
}
