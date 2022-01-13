#version 440 core

in vec2 fPos;

out vec4 FragColor;


uniform int NumIters;
uniform dvec2 XLim;
uniform dvec2 YLim;

dvec2 csquare(dvec2 z)
{
    return dvec2(z.x * z.x - z.y * z.y, 2 * z.x * z.y);
}


dvec2 UV2Cart(vec2 UV)
{
    double XLen = XLim.y - XLim.x;
    double YLen = YLim.y - YLim.x;
    return dvec2(double(UV.x) * XLen + XLim.x,
                 double(UV.y) * YLen + YLim.x);
}


int EscapeTime(vec2 UV)
{
    dvec2 z = UV2Cart(UV);
    dvec2 c = z;
    for (int i = 0; i < NumIters; ++i)
    {
        z = csquare(z) + c;
        if (length(z) > 2)
            return NumIters - i;
    }
    return 0;
}



void main()
{
    int k = EscapeTime(fPos);

    float theta = float(k) / float(NumIters);
    FragColor = vec4(theta, theta, theta, 1.0f);
}