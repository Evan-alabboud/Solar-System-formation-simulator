// shaders/sun.vert
// Вершинный шейдер солнца — пульсирующее смещение по нормали
uniform float Time;

void main()
{
    // пульсация радиуса: ±4 % с частотой 1.5 Гц
    float pulse = 1.0 + 0.04 * sin(Time * 1.5 * 6.2832);

    vec4 pos = gl_Vertex;
    pos.xyz  *= pulse;

    gl_Position    = gl_ModelViewProjectionMatrix * pos;
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_FrontColor  = gl_Color;
}
