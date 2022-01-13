#version 440 core

#define MAX_NUM_ROOTS 100

in vec2 fPos;

out vec4 FragColor;


uniform int NumRoots;
layout(std140, binding = 2) uniform RootsBuf
{
    dvec2 Roots[MAX_NUM_ROOTS];
};
uniform int NumIters;
uniform dvec2 XLim;
uniform dvec2 YLim;

dvec2 csquare(dvec2 z)
{
    return dvec2(z.x * z.x - z.y * z.y, 2 * z.x * z.y);
}

dvec2 cmul(dvec2 z1, dvec2 z2)
{
    return dvec2(z1.x * z2.x - z1.y * z2.y,
                 z1.x * z2.y + z1.y * z2.x);
}

dvec2 cdiv(dvec2 z1, dvec2 z2)
{
    double den = z2.x * z2.x + z2.y * z2.y;
    return dvec2(z1.x * z2.x + z1.y * z2.y, z1.y * z2.x - z1.x * z2.y) / den;
}

dvec2 peval(dvec2 z)
{
    dvec2 p = z - Roots[0];
    for (int i = 1; i < NumRoots; ++i)
        p = cmul(p, z - Roots[i]);
    return p;
}

dvec2 dpeval(dvec2 z)
{
    dvec2 dp = dvec2(0.0);
    for (int i = 0; i < NumRoots; ++i)
    {
        dvec2 p = dvec2(1.0, 0.0);
        for (int j = 0; j < NumRoots; ++j)
        {
            if (i == j) continue;
            p = cmul(p, z - Roots[j]);
        }
        dp += p;
    }
    return dp;
}

dvec2 newton(dvec2 z)
{
    for (int i = 0; i < NumIters; ++i)
        z = z - cdiv(peval(z), dpeval(z));
    return z;
}


dvec2 UV2Cart(vec2 UV)
{
    double XLen = XLim.y - XLim.x;
    double YLen = YLim.y - YLim.x;
    return dvec2(double(UV.x) * XLen + XLim.x,
                 double(UV.y) * YLen + YLim.x);
}


int FindRoot(vec2 UV)
{
    dvec2 z = UV2Cart(UV);
    z = newton(z);
    int n = 0;
    double dmin = length(z - Roots[0]);
    for (int i = 1; i < NumRoots; ++i)
    {
        double d = length(z - Roots[i]);
        if (d < dmin)
        {
            n = i;
            dmin = d;
        }
    }
    return n;
    // return float(dmin);
}

vec4 Colors[8] = {
    vec4(0.2422,    0.1504,     0.6603,     1.0f),
    vec4(0.2810,    0.3228,     0.9579,     1.0f),
    vec4(0.1786,    0.5289,     0.9682,     1.0f),
    vec4(0.0689,    0.6948,     0.8394,     1.0f),
    vec4(0.2161,    0.7843,     0.5923,     1.0f),
    vec4(0.6720,    0.7793,     0.2227,     1.0f),
    vec4(0.9970,    0.7659,     0.2199,     1.0f),
    vec4(0.9769,    0.9839,     0.0805,     1.0f)
};

vec4 colormap(int k)
{
    double Theta = double(k) / double(NumRoots);
    int LeftIdx = int(floor(Theta * 8));
    int RightIdx = int(ceil(Theta * 8));
    if (LeftIdx == RightIdx)
        return Colors[LeftIdx];
    
    double l1 = double(LeftIdx) / 8.0;
    double l2 = double(RightIdx) / 8.0;
    Theta = (Theta - l1) / (l2 - l1);
    return Colors[LeftIdx] * float(1 - Theta) + Colors[RightIdx] * float(Theta);
}



void main()
{
    int k = FindRoot(fPos);
    float theta = float(k) / float(NumRoots);
    FragColor = vec4(theta, theta, theta, 1.0f);
    // FragColor = colormap(k);
}