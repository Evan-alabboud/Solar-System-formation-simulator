// shaders/sun.frag
// Фрагментный шейдер солнца — пульсирующий тёплый цвет + текстура
uniform float     Time;
uniform sampler2D tex;

void main()
{
    vec4 texColor = texture2D(tex, gl_TexCoord[0].st);

    // мягкое мерцание яркости
    float glow = 1.0 + 0.12 * sin(Time * 2.5);

    // тёплый жёлто-оранжевый базовый цвет
    vec3 sunColor = vec3(1.0, 0.75 + 0.15 * sin(Time * 1.2), 0.05);

    // смешиваем текстуру с базовым цветом
    vec3 final = mix(sunColor, texColor.rgb, 0.55) * glow;

    gl_FragColor = vec4(clamp(final, 0.0, 1.0), 1.0);
}
