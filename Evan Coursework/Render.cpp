#include "Render.h"
#include "Texture.h"

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <algorithm>

#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;

bool texturing = false;
bool lightning = true;
bool alpha = true;
bool animating = true;

static double timeScale = 1.0;   
static double full_time = 0.0;
static bool   texturesLoaded = false;

Texture tex_sun, tex_mercury, tex_venus, tex_earth, tex_moon;
Texture tex_mars, tex_jupiter, tex_saturn, tex_uranus, tex_neptune;

enum SimState
{
    ST_SINGULARITY = 0,
    ST_BIGBANG = 1,
    ST_COSMIC_SOUP = 2,
    ST_GALACTIC = 3,
    ST_NEBULA = 4,
    ST_ACCRETION = 5,
    ST_SOLAR = 6
};
static SimState simState = ST_SINGULARITY;
static double   stateTime = 0.0;   // time spent in current state
static double   flashAlpha = 0.0;   // big bang flash

static const char* stateNames[] = {
    "0: The Singularity",
    "1: Cosmic Inflation / Big Bang",
    "2: Condensation / Cosmic Soup",
    "3: Gravitational Clumping",
    "4: Solar Nebula Collapse",
    "5: Planetary Accretion",
    "6: The Solar System"
};

struct Particle
{
    float x, y, z;
    float vx, vy, vz;
    float r, g, b, a;
    float size;
    float life;          
    int   cluster;       
    float angle;         
};

static const int   MAX_P = 5000;
static Particle    P[MAX_P];

static float frand() { return (float)rand() / (float)RAND_MAX; }
static float frand2() { return frand() * 2.f - 1.f; }

static void spawnBigBang()
{
    for (int i = 0;i < MAX_P;++i)
    {
        float phi = acosf(1.f - 2.f * frand());
        float theta = frand() * 2.f * (float)M_PI;
        float speed = 0.5f + frand() * 6.0f;          
        P[i].vx = speed * sinf(phi) * cosf(theta);
        P[i].vy = speed * sinf(phi) * sinf(theta);
        P[i].vz = speed * cosf(phi);
        P[i].x = P[i].y = P[i].z = 0.f;
        float t = frand();
        P[i].r = 1.f;
        P[i].g = 0.3f + 0.7f * (1.f - t);
        P[i].b = 0.05f * (1.f - t);
        P[i].a = 0.9f;
        P[i].size = 3.f + frand() * 4.f;
        P[i].life = 1.f;
    }
    flashAlpha = 1.0;
}

static void spawnCosmicSoup()
{
    for (int i = 0;i < MAX_P;++i)
    {
        float phi = acosf(1.f - 2.f * frand());
        float theta = frand() * 2.f * (float)M_PI;
        float r = 8.f + frand() * 12.f;
        P[i].x = r * sinf(phi) * cosf(theta);
        P[i].y = r * sinf(phi) * sinf(theta) * 0.5f;
        P[i].z = r * cosf(phi);

        P[i].vx = frand2() * 0.8f;
        P[i].vy = frand2() * 0.8f;
        P[i].vz = frand2() * 0.8f;

        float heat = frand();
        P[i].r = 1.f;
        P[i].g = heat * 0.8f;
        P[i].b = heat * heat * 0.4f;
        P[i].a = 0.7f + frand() * 0.3f;
        P[i].size = 3.f + frand() * 4.f;
        P[i].life = 1.f;
    }
}


static const int   NANCHORS = 3;
static const float anchorX[NANCHORS] = { 0.f, 18.f,-18.f };
static const float anchorZ[NANCHORS] = { 0.f,  8.f,  8.f };

static void spawnGalactic()
{
    for (int i = 0;i < MAX_P;++i)
    {

        P[i].x = frand2() * 30.f;
        P[i].y = frand2() * 8.f;
        P[i].z = frand2() * 30.f;
        P[i].vx = frand2() * 0.5f;
        P[i].vy = frand2() * 0.2f;
        P[i].vz = frand2() * 0.5f;

        P[i].cluster = i % NANCHORS;
        P[i].angle = frand() * 2.f * (float)M_PI;

        P[i].r = 0.6f + frand() * 0.4f;
        P[i].g = 0.7f + frand() * 0.3f;
        P[i].b = 1.f;
        P[i].a = 0.7f;
        P[i].size = 2.5f + frand() * 3.f;
        P[i].life = 1.f;
    }
}

static void spawnNebula()
{

    for (int i = 0;i < MAX_P;++i)
    {
        float r = 1.f + frand() * 22.f;
        float theta = frand() * 2.f * (float)M_PI;
        P[i].x = r * cosf(theta);
        P[i].y = frand2() * 1.5f * (1.f - r / 24.f); 
        P[i].z = r * sinf(theta);

        float speed = 0.8f + 3.f / r;
        P[i].vx = -sinf(theta) * speed;
        P[i].vy = frand2() * 0.05f;
        P[i].vz = cosf(theta) * speed;

        float t = r / 24.f;
        P[i].r = 0.9f;
        P[i].g = 0.2f + 0.5f * t;
        P[i].b = 0.8f * (1.f - t);
        P[i].a = 0.6f + 0.4f * (1.f - t);
        P[i].size = 2.f + frand() * 3.f;
        P[i].life = 1.f;
    }
}

static const float accrOrbit[9] =
{ 0.f, 4.5f, 6.5f, 9.f, 12.f, 18.f, 25.f, 32.f, 39.f };

static void spawnAccretion()
{
    for (int i = 0;i < MAX_P;++i)
    {
        int   track = i % 9;
        float r = accrOrbit[track];
        float scatter = (track == 0) ? 0.5f : 1.2f;
        float theta = frand() * 2.f * (float)M_PI;
        P[i].x = (r + frand2() * scatter) * cosf(theta);
        P[i].y = frand2() * scatter * 0.4f;
        P[i].z = (r + frand2() * scatter) * sinf(theta);
        float speed = (r < 0.1f) ? 0.f : (1.5f / sqrtf(r + 0.1f));
        P[i].vx = -sinf(theta) * speed;
        P[i].vy = 0.f;
        P[i].vz = cosf(theta) * speed;

        P[i].r = 0.7f + frand() * 0.3f;
        P[i].g = 0.5f + frand() * 0.3f;
        P[i].b = 0.2f + frand() * 0.2f;
        P[i].a = 0.6f + frand() * 0.4f;
        P[i].size = 1.f + frand() * 2.5f;
        P[i].life = 1.f;
    }
}

static void updateBigBang(double dt)
{
    for (int i = 0;i < MAX_P;++i)
    {
        float boost = 1.f + 0.3f * (float)stateTime;
        P[i].x += P[i].vx * (float)dt * boost;
        P[i].y += P[i].vy * (float)dt * boost;
        P[i].z += P[i].vz * (float)dt * boost;
        P[i].a = fmaxf(0.f, P[i].a - 0.01f * (float)dt);
    }
    flashAlpha = fmaxf(0.0, flashAlpha - dt * 0.8);
}

static void updateCosmicSoup(double dt)
{
    for (int i = 0;i < MAX_P;++i)
    {
        P[i].x += P[i].vx * (float)dt;
        P[i].y += P[i].vy * (float)dt;
        P[i].z += P[i].vz * (float)dt;

        float dist = sqrtf(P[i].x * P[i].x + P[i].y * P[i].y + P[i].z * P[i].z);
        if (dist > 22.f)
        {
            float nx = P[i].x / dist, ny = P[i].y / dist, nz = P[i].z / dist;
            float dot = P[i].vx * nx + P[i].vy * ny + P[i].vz * nz;
            P[i].vx -= 2.f * dot * nx;
            P[i].vy -= 2.f * dot * ny;
            P[i].vz -= 2.f * dot * nz;
        }

        P[i].g = fminf(1.f, P[i].g + 0.02f * (float)dt);
        P[i].b = fminf(1.f, P[i].b + 0.05f * (float)dt);
    }
}

static void updateGalactic(double dt)
{
    for (int i = 0;i < MAX_P;++i)
    {
        int c = P[i].cluster;
        float tx = anchorX[c], tz = anchorZ[c];
        float dx = tx - P[i].x, dy = -P[i].y, dz = tz - P[i].z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz) + 0.5f;

        float f = 3.0f / (dist * dist + 1.f);
        P[i].vx += dx / dist * f * (float)dt;
        P[i].vy += dy / dist * f * (float)dt * 0.5f;
        P[i].vz += dz / dist * f * (float)dt;

        float cx = -dz / dist, cz = dx / dist;
        P[i].vx += cx * 1.5f * (float)dt;
        P[i].vz += cz * 1.5f * (float)dt;

        float damp = powf(0.995f, (float)dt * 60.f);
        P[i].vx *= damp; P[i].vy *= damp; P[i].vz *= damp;
        P[i].x += P[i].vx * (float)dt;
        P[i].y += P[i].vy * (float)dt;
        P[i].z += P[i].vz * (float)dt;

        P[i].r = fmaxf(0.4f, P[i].r - 0.1f * (float)dt);
    }
}

static void updateNebula(double dt)
{
    for (int i = 0;i < MAX_P;++i)
    {

        P[i].x += P[i].vx * (float)dt;
        P[i].y += P[i].vy * (float)dt;
        P[i].z += P[i].vz * (float)dt;

        float dist = sqrtf(P[i].x * P[i].x + P[i].z * P[i].z) + 0.1f;
        P[i].vx -= (P[i].x / dist) * 0.05f * (float)dt;
        P[i].vz -= (P[i].z / dist) * 0.05f * (float)dt;

        P[i].vy -= P[i].y * 0.5f * (float)dt;
    }
}

static void updateAccretion(double dt)
{
    for (int i = 0;i < MAX_P;++i)
    {
        P[i].x += P[i].vx * (float)dt;
        P[i].y += P[i].vy * (float)dt;
        P[i].z += P[i].vz * (float)dt;

        int   track = i % 9;
        float tr = accrOrbit[track];
        float dist = sqrtf(P[i].x * P[i].x + P[i].z * P[i].z) + 0.01f;
        float pull = (tr - dist) * 0.3f * (float)dt;
        P[i].vx += (P[i].x / dist) * pull;
        P[i].vz += (P[i].z / dist) * pull;

        P[i].vy -= P[i].y * 1.5f * (float)dt;

        float progress = fminf(1.f, (float)stateTime / 8.f);
        P[i].size = (1.f + frand() * 2.f) * (1.f - progress * 0.7f);
        P[i].a = 0.4f + 0.6f * (1.f - progress * 0.5f);
    }
}

static void drawParticles()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);   
    glEnable(GL_POINT_SMOOTH);

    for (int i = 0;i < MAX_P;++i)
    {
        float a = fmaxf(0.f, fminf(1.f, P[i].a));
        if (a < 0.01f) continue;
        glColor4f(P[i].r, P[i].g, P[i].b, a);
        glPointSize(fmaxf(1.f, P[i].size));
        glBegin(GL_POINTS);
        glVertex3f(P[i].x, P[i].y, P[i].z);
        glEnd();
    }

    glDisable(GL_BLEND);
    glDisable(GL_POINT_SMOOTH);
}

struct Meteor { float x, y, z, vx, vy, vz, life, maxLife; };
static const int   NMETEORS = 40;
static Meteor      meteors[NMETEORS];

static void spawnMeteor(int i)
{
    meteors[i].x = frand2() * 80.f;
    meteors[i].y = 20.f + frand() * 20.f;
    meteors[i].z = frand2() * 80.f;
    float speed = 15.f + frand() * 20.f;
    meteors[i].vx = -0.3f + frand2() * 0.2f;
    meteors[i].vy = -1.0f;
    meteors[i].vz = -0.3f + frand2() * 0.2f;
    float len = sqrtf(meteors[i].vx * meteors[i].vx +
        meteors[i].vy * meteors[i].vy +
        meteors[i].vz * meteors[i].vz);
    meteors[i].vx *= speed / len;
    meteors[i].vy *= speed / len;
    meteors[i].vz *= speed / len;
    meteors[i].maxLife = 1.5f + frand() * 1.f;
    meteors[i].life = meteors[i].maxLife;
}

static void initMeteors()
{
    for (int i = 0;i < NMETEORS;++i) { spawnMeteor(i); meteors[i].life = frand() * meteors[i].maxLife; }
}

static void updateDrawMeteors(double dt)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.5f);

    for (int i = 0;i < NMETEORS;++i)
    {
        meteors[i].life -= (float)dt;
        if (meteors[i].life <= 0.f) spawnMeteor(i);

        float t = meteors[i].life / meteors[i].maxLife;
        meteors[i].x += meteors[i].vx * (float)dt;
        meteors[i].y += meteors[i].vy * (float)dt;
        meteors[i].z += meteors[i].vz * (float)dt;

        float tailLen = 3.f;
        glBegin(GL_LINES);
        glColor4f(1.f, 1.f, 1.f, t * 0.9f);
        glVertex3f(meteors[i].x, meteors[i].y, meteors[i].z);
        glColor4f(0.6f, 0.8f, 1.f, 0.f);
        glVertex3f(meteors[i].x - meteors[i].vx / meteors[i].vx * tailLen * meteors[i].vx * 0.05f,
            meteors[i].y + tailLen * 0.3f,
            meteors[i].z - meteors[i].vz * 0.05f * tailLen);
        glEnd();
    }
    glLineWidth(1.f);
    glDisable(GL_BLEND);
}

//  SOLAR SYSTEM DATA
struct PlanetInfo
{
    const char* name;
    const char* fact;
    int         moons;
    double      orbitR, radius, orbitSpeed, spinSpeed, tilt;
    float       cr, cg, cb, ar, ag, ab, shininess;
    bool        hasRings;
    double      ringInner, ringOuter;
    float       ringR, ringG, ringB;
    bool        hasMoon;
    double      moonOrbitR, moonRadius, moonSpeed;
    Texture* tex;
};

static PlanetInfo planets[] =
{
    {"Sun",     "Surface temp: 5,500 C",                    0,    0.0,  2.0,  0.0,  5.0,  0.0,  1.f,.85f,.1f,  .5f,.4f,.0f,  16.f,  false,0,0,  0,0,0,  false,0,0,0,  nullptr},
    {"Mercury", "Closest planet to Sun",                    0,    4.5, 0.25, 38.0, 10.0,  0.0, .55f,.5f,.4f,  .2f,.2f,.1f,  16.f,  false,0,0,  0,0,0,  false,0,0,0,  nullptr},
    {"Venus",   "Hottest planet: 465 C",                    0,    6.5, 0.55, 15.0,  6.0,  3.0, .9f,.75f,.3f,  .3f,.3f,.1f,  32.f,  false,0,0,  0,0,0,  false,0,0,0,  nullptr},
    {"Earth",   "Only known life in universe",              1,    9.0,  0.6, 10.0, 30.0, 23.0,  .2f,.5f,.9f,  .0f,.1f,.2f,  64.f,  false,0,0,  0,0,0,  true, 1.1,0.18,80.0,  nullptr},
    {"Mars",    "Has the tallest volcano",                  2,   12.0,  0.4,  5.3, 25.0, 25.0, .8f,.3f,.1f,  .2f,.1f,.0f,  32.f,  false,0,0,  0,0,0,  false,0,0,0,  nullptr},
    {"Jupiter", "Largest planet in system",                95,   18.0,  1.4,  2.0, 50.0,  3.0, .8f,.65f,.4f,  .3f,.2f,.1f,  16.f,  false,0,0,  0,0,0,  false,0,0,0,  nullptr},
    {"Saturn",  "Rings span 282,000 km",                   83,   25.0,  1.2,  1.2, 45.0, 27.0, .9f,.8f,.5f,  .3f,.3f,.1f,  32.f,  true, 1.7,2.9,.85f,.75f,.5f,  false,0,0,0,  nullptr},
    {"Uranus",  "Rotates on its side: 98 tilt",            27,   32.0, 0.85,  0.7, 20.0, 98.0, .4f,.8f,.9f,  .1f,.2f,.3f,  64.f,  true, 1.2,1.8,.5f,.8f,.9f,   false,0,0,0,  nullptr},
    {"Neptune", "Fastest winds: 2100 km/h",                14,   39.0,  0.8,  0.4, 18.0, 30.0, .2f,.4f,.9f,  .0f,.1f,.3f,  64.f,  false,0,0,  0,0,0,  false,0,0,0,  nullptr},
};
static const int NPLANETS = 9;

static double orbitAngle[NPLANETS] = { 0 };
static double spinAngle[NPLANETS] = { 0 };
static double moonAngle[NPLANETS] = { 0 };

static double accretionRadius[NPLANETS] = { 0 };

//  STARFIELD BACKGROUND
struct Star { float x, y, z, brightness, size; };
static const int NSTARS = 1500;
static Star stars[NSTARS];

static void initStars()
{
    for (int i = 0;i < NSTARS;++i)
    {
        float phi = acosf(1.f - 2.f * frand());
        float theta = frand() * 2.f * (float)M_PI;
        float r = 180.f + frand() * 60.f;   
        stars[i].x = r * sinf(phi) * cosf(theta);
        stars[i].y = r * sinf(phi) * sinf(theta);
        stars[i].z = r * cosf(phi);
        stars[i].brightness = 0.4f + frand() * 0.6f;
        stars[i].size = 1.0f + frand() * 2.0f;
    }
}

static void drawStarfield()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);   
    glEnable(GL_POINT_SMOOTH);
    glBegin(GL_POINTS);
    for (int i = 0;i < NSTARS;++i)
    {
        float tw = stars[i].brightness *
            (0.8f + 0.2f * (float)sin(full_time * 1.3f + i * 0.7f));
        glColor3f(tw, tw, tw * 0.95f + 0.05f);   
        glPointSize(stars[i].size);
        glVertex3f(stars[i].x, stars[i].y, stars[i].z);
    }
    glEnd();
    glDisable(GL_POINT_SMOOTH);
    glEnable(GL_DEPTH_TEST);
}

//  ASTEROID BELT
struct Asteroid { float r, angle, y, size; };
static const int   NAST = 700;
static Asteroid    ast[NAST];

static void initAsteroids()
{
    for (int i = 0;i < NAST;++i)
    {
        ast[i].r = 14.5f + frand() * 2.f;
        ast[i].angle = frand() * 360.f;
        ast[i].y = frand2() * 0.4f;
        ast[i].size = 1.f + frand() * 2.5f;
    }
}

//  CAMERA
static int  camTarget = -1;    
static bool surfaceView = false;
static double surfaceAngle = 0.0; 

//  B-SPLINE COMET
struct V3 { double x, y, z; };
static const V3 cometCP[] = {
    { 35.0, 3.0,  0.0},{ 15.0, 5.0, 22.0},{-15.0, 2.0, 22.0},
    {-35.0, 0.0,  0.0},{-15.0,-3.0,-22.0},{ 15.0, 1.0,-22.0},
    { 35.0, 3.0,  0.0} };
static const int COMET_N = 7;
static double    cometT = 0.0;

static V3 evalBSpline(double t, const V3* cp, int n)
{
    double tC = fmod(t, (double)(n - 2)); if (tC < 0) tC += (double)(n - 2);
    int i = (int)tC; double lt = tC - i;
    double b0 = .5 * (1 - lt) * (1 - lt), b1 = .5 + lt - lt * lt, b2 = .5 * lt * lt;
    int i0 = i % n, i1 = (i + 1) % n, i2 = (i + 2) % n;
    return{ cp[i0].x * b0 + cp[i1].x * b1 + cp[i2].x * b2,
           cp[i0].y * b0 + cp[i1].y * b1 + cp[i2].y * b2,
           cp[i0].z * b0 + cp[i1].z * b1 + cp[i2].z * b2 };
}

//  GEOMETRY
static void drawSphere(double r, int sl, int st)
{
    for (int i = 0;i < st;++i)
    {
        double p0 = M_PI * (-0.5 + (double)i / st);
        double p1 = M_PI * (-0.5 + (double)(i + 1) / st);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0;j <= sl;++j)
        {
            double th = 2 * M_PI * (double)j / sl;
            double ct = cos(th), st_ = sin(th);
            double c0 = cos(p0), s0 = sin(p0);
            glNormal3d(c0 * ct, s0, c0 * st_);
            glTexCoord2f((float)j / sl, (float)i / st);
            glVertex3d(r * c0 * ct, r * s0, r * c0 * st_);
            double c1 = cos(p1), s1 = sin(p1);
            glNormal3d(c1 * ct, s1, c1 * st_);
            glTexCoord2f((float)j / sl, (float)(i + 1) / st);
            glVertex3d(r * c1 * ct, r * s1, r * c1 * st_);
        }
        glEnd();
    }
}

static void drawRing(double ri, double ro, int seg)
{
    glBegin(GL_QUAD_STRIP);
    for (int i = 0;i <= seg;++i)
    {
        double th = 2 * M_PI * (double)i / seg;
        double ct = cos(th), st = sin(th);
        glNormal3d(0, 1, 0);
        glTexCoord2d((double)i / seg, 0); glVertex3d(ri * ct, 0, ri * st);
        glTexCoord2d((double)i / seg, 1); glVertex3d(ro * ct, 0, ro * st);
    }
    glEnd();
}

static void drawOrbit(double r, int seg)
{
    glBegin(GL_LINE_LOOP);
    for (int i = 0;i < seg;++i)
    {
        double th = 2 * M_PI * (double)i / seg;
        glVertex3d(r * cos(th), 0, r * sin(th));
    }
    glEnd();
}

static void setMat(float ar, float ag, float ab,
    float dr, float dg, float db,
    float sr, float sg, float sb, float sh,
    float er = 0, float eg = 0, float eb = 0)
{
    float A[] = { ar,ag,ab,1 }, D[] = { dr,dg,db,1 },
        S[] = { sr,sg,sb,1 }, E[] = { er,eg,eb,1 };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, A);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, D);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, S);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, sh);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, E);
}

//  FONT / HUD
static GLuint fontBase = 0;

static void initFont()
{
    HDC hdc = wglGetCurrentDC();
    HFONT hf = CreateFontA(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Courier New");
    SelectObject(hdc, hf);
    fontBase = glGenLists(256);
    wglUseFontBitmapsA(hdc, 0, 256, fontBase);
}

static void glPrint(int x, int y, const char* fmt, ...)
{
    char buf[512]; va_list a; va_start(a, fmt);
    vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
    glRasterPos2i(x, y);
    glListBase(fontBase);
    glCallLists((GLsizei)strlen(buf), GL_UNSIGNED_BYTE, buf);
}

static void begin2D()
{
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, gl.getWidth(), 0, gl.getHeight(), -1, 1);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
}
static void end2D()
{
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}

// Panel background
static void drawPanel(int x, int y, int w, int h)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.f, 0.f, 0.05f, 0.72f);
    glBegin(GL_QUADS);
    glVertex2i(x, y); glVertex2i(x + w, y);
    glVertex2i(x + w, y + h); glVertex2i(x, y + h);
    glEnd();
    // border
    glColor4f(0.3f, 0.5f, 1.f, 0.5f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x, y); glVertex2i(x + w, y);
    glVertex2i(x + w, y + h); glVertex2i(x, y + h);
    glEnd();
    glDisable(GL_BLEND);
}

static void drawHUD(double dt)
{
    begin2D();

    int W = gl.getWidth(), H = gl.getHeight();
    int ls = 18, tx = 16, ty;

    // ── left panel: state + controls
    drawPanel(8, H - 320, 340, 315);

    // state colour coding
    float sc[7][3] = { {1,1,1},{1,.5f,.1f},{1,.3f,.3f},
                    {.4f,.6f,1},{.8f,.4f,1},{.9f,.7f,.3f},{.2f,1,.4f} };
    ty = H - 24;
    glColor3fv(sc[simState]);
    glPrint(tx, ty, "STATE: %s", stateNames[simState]); ty -= ls + 4;

    glColor3f(.7f, .7f, .7f);
    glPrint(tx, ty, "SPACE - next state"); ty -= ls;
    glPrint(tx, ty, "T-tex[%s] L-light[%s] A-rings[%s]",
        texturing ? "ON " : "OFF", lightning ? "ON " : "OFF", alpha ? "ON " : "OFF"); ty -= ls;
    glPrint(tx, ty, "P-pause[%s]  +/- time scale: %.1fx",
        animating ? "RUN" : "STP", timeScale); ty -= ls;
    glPrint(tx, ty, "F-light to cam  G/G+LMB move light"); ty -= ls;

    if (simState == ST_SOLAR)
    {
        glPrint(tx, ty, "1-9 snap cam to planet  0-free"); ty -= ls;
        glPrint(tx, ty, "W - surface view (when locked)"); ty -= ls;
        ty -= 4;
        if (camTarget < 0)
            glPrint(tx, ty, "Camera: FREE");
        else
        {
            glColor3f(.3f, 1.f, .6f);
            glPrint(tx, ty, "Locked: %s  [W=surface]",
                planets[camTarget].name);
        }
        ty -= ls;

        glColor3f(.6f, .9f, 1.f);
        V3 cp = evalBSpline(cometT, cometCP, COMET_N);
        glPrint(tx, ty, "Comet:(%.1f,%.1f,%.1f)", cp.x, cp.y, cp.z);
    }

    // ── right panel: planet info (when locked)
    if (simState == ST_SOLAR && camTarget >= 0)
    {
        PlanetInfo& pi = planets[camTarget];
        drawPanel(W - 310, H - 175, 305, 170);
        ty = H - 28; tx = W - 300;
        glColor3f(1.f, .8f, .2f);
        glPrint(tx, ty, "[ %s ]", pi.name); ty -= ls + 4;
        glColor3f(.8f, .9f, 1.f);
        glPrint(tx, ty, "Orbit radius: %.0f AU (scaled)", pi.orbitR); ty -= ls;
        glPrint(tx, ty, "Size (scaled): %.2f", pi.radius);           ty -= ls;
        glPrint(tx, ty, "Moons: %d", pi.moons);                      ty -= ls;
        glPrint(tx, ty, "Tilt: %.0f deg", pi.tilt);                  ty -= ls;
        glColor3f(1.f, 1.f, .5f);
        glPrint(tx, ty, ">> %s", pi.fact);
    }

    // bottom centre: surface view label 
    if (surfaceView && camTarget > 0)
    {
        drawPanel(W / 2 - 160, 8, 320, 30);
        glColor3f(.3f, 1.f, .5f);
        glPrint(W / 2 - 150, 16, "SURFACE VIEW: %s  [0=exit]",
            planets[camTarget].name);
    }

    end2D();
}

//  BIG BANG FLASH overlay
static void drawFlash()
{
    if (flashAlpha <= 0.0) return;
    begin2D();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.f, 1.f, 1.f, (float)flashAlpha);
    glBegin(GL_QUADS);
    glVertex2i(0, 0); glVertex2i(gl.getWidth(), 0);
    glVertex2i(gl.getWidth(), gl.getHeight());
    glVertex2i(0, gl.getHeight());
    glEnd();
    glDisable(GL_BLEND);
    end2D();
}

//  STATE TRANSITION
static void advanceState()
{
    if (simState == ST_SOLAR) return;
    simState = (SimState)(simState + 1);
    stateTime = 0.0;

    switch (simState)
    {
    case ST_BIGBANG:     spawnBigBang();     break;
    case ST_COSMIC_SOUP: spawnCosmicSoup();  break;
    case ST_GALACTIC:    spawnGalactic();    break;
    case ST_NEBULA:      spawnNebula();      break;
    case ST_ACCRETION:
        spawnAccretion();
        for (int i = 0;i < NPLANETS;++i) accretionRadius[i] = 0.0;
        break;
    case ST_SOLAR:
        camera.setPosition(0.0, 30.0, 60.0);
        camTarget = -1; surfaceView = false;
        break;
    default: break;
    }
}

//  KEY HANDLER
void switchModes(OpenGL* sender, KeyEventArg arg)
{
    if (arg.key == VK_SPACE) { advanceState(); return; }

    auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));
    switch (key)
    {
    case 'L': lightning = !lightning; break;
    case 'T': texturing = !texturing; break;
    case 'A': alpha = !alpha;         break;
    case 'P': animating = !animating; break;
    case 'W':
        if (camTarget >= 0 && simState == ST_SOLAR)
            surfaceView = !surfaceView;
        break;

    case '1': camTarget = 0; surfaceView = false; break;
    case '2': camTarget = 1; surfaceView = false; break;
    case '3': camTarget = 2; surfaceView = false; break;
    case '4': camTarget = 3; surfaceView = false; break;
    case '5': camTarget = 4; surfaceView = false; break;
    case '6': camTarget = 5; surfaceView = false; break;
    case '7': camTarget = 6; surfaceView = false; break;
    case '8': camTarget = 7; surfaceView = false; break;
    case '9': camTarget = 8; surfaceView = false; break;
    case '0': camTarget = -1; surfaceView = false; break;

    case '+': case '=': timeScale = fmin(10.0, timeScale + 0.5); break;
    case '-': case '_': timeScale = fmax(0.1, timeScale - 0.5); break;
    }
}

//  PLANET POSITION
static void getPlanetPos(int idx, double& px, double& pz)
{
    px = planets[idx].orbitR * cos(orbitAngle[idx] * M_PI / 180.0);
    pz = planets[idx].orbitR * sin(orbitAngle[idx] * M_PI / 180.0);
}

//  initRender
void initRender()
{
    srand(42);
    initStars();
    initAsteroids();
    initMeteors();
    initFont();

    planets[0].tex = &tex_sun;     planets[1].tex = &tex_mercury;
    planets[2].tex = &tex_venus;   planets[3].tex = &tex_earth;
    planets[4].tex = &tex_mars;    planets[5].tex = &tex_jupiter;
    planets[6].tex = &tex_saturn;  planets[7].tex = &tex_uranus;
    planets[8].tex = &tex_neptune;

     tex_sun.LoadTexture    ("textures/sun.png");
     tex_mercury.LoadTexture("textures/mercury.png");
     tex_venus.LoadTexture  ("textures/venus.png");
     tex_earth.LoadTexture  ("textures/earth.png");
     tex_moon.LoadTexture   ("textures/moon.png");
     tex_mars.LoadTexture   ("textures/mars.png");
     tex_jupiter.LoadTexture("textures/jupiter.png");
     tex_saturn.LoadTexture ("textures/saturn.png");
     tex_uranus.LoadTexture ("textures/uranus.png");
     tex_neptune.LoadTexture("textures/neptune.png");
     texturesLoaded=true; texturing=true;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    light.SetPosition(0, 2, 0);

    camera.caclulateCameraPos();
    camera.setPosition(0.0, 5.0, 15.0);

    gl.WheelEvent.reaction(&camera, &Camera::Zoom);
    gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
    gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
    gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
    gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
    gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
    gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
    gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
    gl.KeyDownEvent.reaction(switchModes);
}

void Render(double delta_time)
{
    double dt = delta_time * timeScale;
    full_time += delta_time;
    if (animating) stateTime += dt;

    if (simState == ST_SOLAR && animating)
    {
        cometT += dt * 0.22;
        for (int i = 1;i < NPLANETS;++i)
        {
            orbitAngle[i] += dt * planets[i].orbitSpeed;
            spinAngle[i] += dt * planets[i].spinSpeed;
            if (planets[i].hasMoon) moonAngle[i] += dt * planets[i].moonSpeed;
        }
        spinAngle[0] += dt * planets[0].spinSpeed;
    }

    if (gl.isKeyPressed('F'))
        light.SetPosition(camera.x(), camera.y(), camera.z());

   
    if (simState == ST_SOLAR && camTarget >= 0)
    {
        double px = 0.0, py = 0.0, pz = 0.0;
        if (camTarget > 0) getPlanetPos(camTarget, px, pz);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        if (surfaceView && camTarget >= 0)
        {
            if (animating) surfaceAngle += delta_time * 6.0;
            double sa = surfaceAngle * M_PI / 180.0;
            double pr = planets[camTarget].radius;
            double sr = pr * 1.05;

            double eyeX = px + sr * cos(sa);
            double eyeY = py + pr * 0.15;
            double eyeZ = pz + sr * sin(sa);

            gluLookAt(eyeX, eyeY, eyeZ,
                px, py, pz,
                0.0, 1.0, 0.0);
        }
        else
        {
            double pr = planets[camTarget].radius;
            double dist = pr * 6.0 + 7.0;

            double eyeX = px + dist;
            double eyeY = dist * 0.4;
            double eyeZ = pz + dist;

            gluLookAt(eyeX, eyeY, eyeZ,
                px, 0.0, pz,
                0.0, 1.0, 0.0);
        }
    }
    else
    {
        camera.SetUpCamera();
    }

    light.SetUpLight();
    gl.DrawAxes();

    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glClearColor(0.0f, 0.0f, 0.02f, 1.0f);   
    drawStarfield();

    //  STATE 0: SINGULARITY
    if (simState == ST_SINGULARITY)
    {
        glEnable(GL_POINT_SMOOTH);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        float pulse = 8.f + 6.f * (float)sin(full_time * 3.0);

        for (int ring = 0;ring < 4;++ring)
        {
            float rphase = (float)ring * 0.5f;
            float rr = fmodf((float)full_time * 2.f + rphase, 6.f);
            float ra = fmaxf(0.f, 1.f - rr / 6.f);
            glColor4f(1.f, .8f, .2f, ra * 0.3f);
            glDisable(GL_LIGHTING);
            glBegin(GL_LINE_LOOP);
            for (int k = 0;k < 64;++k)
            {
                double a = 2 * M_PI * k / 64.0;
                glVertex3d(rr * cos(a), 0, rr * sin(a));
            }
            glEnd();
        }

        glPointSize(pulse * 5.f); glColor4f(1.f, .7f, .1f, .08f);
        glBegin(GL_POINTS); glVertex3d(0, 0, 0); glEnd();
        glPointSize(pulse * 2.f); glColor4f(1.f, .9f, .3f, .3f);
        glBegin(GL_POINTS); glVertex3d(0, 0, 0); glEnd();
        glPointSize(pulse);     glColor4f(1.f, 1.f, 1.f, 1.f);
        glBegin(GL_POINTS); glVertex3d(0, 0, 0); glEnd();

        glDisable(GL_BLEND); glDisable(GL_POINT_SMOOTH);

        begin2D();
        glColor3f(1.f, .9f, .3f);
        glPrint(gl.getWidth() / 2 - 90, gl.getHeight() / 2 + 10,
            "THE SINGULARITY");
        glColor3f(.8f, .8f, .8f);
        glPrint(gl.getWidth() / 2 - 105, gl.getHeight() / 2 - 14,
            "Press SPACE to begin");
        end2D();
    }

    //  STATE 1: BIG BANG
    else if (simState == ST_BIGBANG)
    {
        if (animating) updateBigBang(dt);
        drawParticles();
        drawFlash();

        if (stateTime < 2.0)
        {
            float fa = (float)(1.0 - stateTime / 2.0);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
            setMat(0, 0, 0, 0, 0, 0, 0, 0, 0, 1, fa, fa * 0.8f, fa * 0.4f);
            glPushMatrix();
            double s = stateTime * 3.0 + 0.1;
            glScaled(s, s, s);
            drawSphere(1.0, 20, 20);
            glPopMatrix();
            glDisable(GL_BLEND);
        }
    }

    //  STATE 2: COSMIC SOUP
    else if (simState == ST_COSMIC_SOUP)
    {
        if (animating) updateCosmicSoup(dt);

        glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        float pulse2 = 0.5f + 0.5f * (float)sin(full_time * 1.5);
        glColor4f(1.f, 0.4f, 0.1f, 0.08f * pulse2);
        drawSphere(22.0, 24, 24);
        glDisable(GL_BLEND);

        drawParticles();
    }

    //  STATE 3: GALACTIC CLUMPING
    else if (simState == ST_GALACTIC)
    {
        if (animating) updateGalactic(dt);

        glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        for (int c = 0;c < NANCHORS;++c)
        {
            float gp = 1.5f + 1.f * (float)sin(full_time * 2.f + c);
            glColor4f(0.4f, 0.6f, 1.f, 0.15f);
            glPushMatrix();
            glTranslatef(anchorX[c], 0, anchorZ[c]);
            drawSphere(gp, 12, 12);
            glPopMatrix();
        }
        glDisable(GL_BLEND);
        drawParticles();
    }

    //  STATE 4: SOLAR NEBULA
    else if (simState == ST_NEBULA)
    {
        if (animating) updateNebula(dt);

        glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        float glow = 0.6f + 0.4f * (float)sin(full_time * 2.0);
        glColor4f(1.f, .7f, .2f, 0.25f * glow);
        drawSphere(2.5, 16, 16);
        glColor4f(1.f, .9f, .5f, 0.5f * glow);
        drawSphere(1.0, 12, 12);
        glDisable(GL_BLEND);

        drawParticles();
    }

    //  STATE 5: PLANETARY ACCRETION
    else if (simState == ST_ACCRETION)
    {
        if (animating) updateAccretion(dt);
        drawParticles();

        float progress = fminf(1.f, (float)stateTime / 10.f);
        glEnable(GL_LIGHTING);
        float noE[] = { 0,0,0,1 };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noE);

        for (int i = 0;i < NPLANETS;++i)
        {
            double tr = accrOrbit[i];

            accretionRadius[i] = fmin(planets[i].radius,
                planets[i].radius * progress * 1.2);
            if (accretionRadius[i] < 0.05) continue;

            PlanetInfo& pl = planets[i];
            setMat(pl.ar, pl.ag, pl.ab, pl.cr, pl.cg, pl.cb,
                .4f, .4f, .5f, pl.shininess);

            glPushMatrix();
            double ang = orbitAngle[i] * M_PI / 180.0;
            glTranslated(tr * cos(ang), 0, tr * sin(ang));
            drawSphere(accretionRadius[i], 16, 16);
            glPopMatrix();
        }
        if (lightning) glEnable(GL_LIGHTING);
    }

    //  STATE 6: SOLAR SYSTEM
    else if (simState == ST_SOLAR)
    {

        if (lightning) glEnable(GL_LIGHTING);
        if (texturing) glEnable(GL_TEXTURE_2D);


        if (animating) updateDrawMeteors(dt);


        glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
        glColor3d(0.2, 0.2, 0.25);
        for (int i = 1;i < NPLANETS;++i) drawOrbit(planets[i].orbitR, 120);


        glPointSize(2.f); glEnable(GL_POINT_SMOOTH);
        glColor3f(0.5f, 0.45f, 0.35f);
        glBegin(GL_POINTS);
        for (int i = 0;i < NAST;++i)
        {
            if (animating) ast[i].angle += delta_time * 2.5f;
            float a = ast[i].angle * (float)M_PI / 180.f;
            glVertex3f(ast[i].r * cosf(a), ast[i].y, ast[i].r * sinf(a));
        }
        glEnd();
        glDisable(GL_POINT_SMOOTH);

        // SUN 
        {
            PlanetInfo& s = planets[0];
            float er = s.cr * 0.85f, eg = s.cg * 0.65f;
            setMat(s.ar, s.ag, s.ab, s.cr, s.cg, s.cb,
                1.f, 1.f, .8f, s.shininess, er, eg, 0.f);
            glDisable(GL_LIGHTING);
            if (texturing && texturesLoaded) { glEnable(GL_TEXTURE_2D); s.tex->Bind(); }
            else glDisable(GL_TEXTURE_2D);
            glPushMatrix();
            glRotated(spinAngle[0], 0, 1, 0);
            drawSphere(s.radius, 40, 40);
            glPopMatrix();
            float noE[] = { 0,0,0,1 };
            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noE);
            if (lightning) glEnable(GL_LIGHTING);
        }

        // PLANETS 
        for (int i = 1;i < NPLANETS;++i)
        {
            PlanetInfo& p = planets[i];
            double px, pz; getPlanetPos(i, px, pz);

            setMat(p.ar * 0.3f, p.ag * 0.3f, p.ab * 0.3f,
                p.cr, p.cg, p.cb,
                0.5f, 0.5f, 0.6f, p.shininess);

            if (texturing && texturesLoaded && p.tex)
            {
                glEnable(GL_TEXTURE_2D); p.tex->Bind();
            }
            else
            {
                glDisable(GL_TEXTURE_2D); glColor3f(p.cr, p.cg, p.cb);
            }

            glPushMatrix();
            glTranslated(px, 0, pz);
            glRotated(p.tilt, 0, 0, 1);
            glRotated(spinAngle[i], 0, 1, 0);
            drawSphere(p.radius, 32, 32);

            // rings
            if (p.hasRings && alpha)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
                float rd[] = { p.ringR,p.ringG,p.ringB,0.45f };
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, rd);
                glColor4f(p.ringR, p.ringG, p.ringB, 0.45f);
                glPushMatrix();
                glRotated(90, 1, 0, 0);
                drawRing(p.ringInner, p.ringOuter, 80);
                glPopMatrix();
                glDisable(GL_BLEND);
                if (lightning) glEnable(GL_LIGHTING);
            }

            // moon (Earth)
            if (p.hasMoon)
            {
                glDisable(GL_TEXTURE_2D);
                setMat(.08f, .08f, .08f, .55f, .55f, .55f, .2f, .2f, .2f, 16.f);
                glColor3f(.75f, .75f, .7f);
                if (texturing && texturesLoaded)
                {
                    glEnable(GL_TEXTURE_2D); tex_moon.Bind();
                }
                double mx = p.moonOrbitR * cos(moonAngle[i] * M_PI / 180.0);
                double mz = p.moonOrbitR * sin(moonAngle[i] * M_PI / 180.0);
                glPushMatrix();
                glTranslated(mx, 0, mz);
                drawSphere(p.moonRadius, 16, 16);
                glPopMatrix();
            }

            glPopMatrix();
        }

        //  COMET 
        {
            V3 cp = evalBSpline(cometT, cometCP, COMET_N);
            glDisable(GL_TEXTURE_2D);
            setMat(.1f, .1f, .1f, .7f, .75f, .8f, .9f, .9f, 1.f, 128.f);
            glPushMatrix();
            glTranslated(cp.x, cp.y, cp.z);
            glRotated(full_time * 120.0, 1, 1, 0);
            drawSphere(0.3, 12, 12);
            glPopMatrix();
            // tail
            if (alpha)
            {
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
                glBegin(GL_TRIANGLE_FAN);
                glColor4f(.5f, .8f, 1.f, .7f);
                glVertex3d(cp.x, cp.y, cp.z);
                glColor4f(.2f, .4f, 1.f, 0.f);
                for (int k = 0;k <= 16;++k)
                {
                    double a = 2 * M_PI * k / 16.0;
                    glVertex3d(cp.x + .4 * cos(a) + 3.0, cp.y + .4 * sin(a), cp.z);
                }
                glEnd();
                glDisable(GL_BLEND);
                if (lightning) glEnable(GL_LIGHTING);
            }
            // dashed spline path
            glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
            glEnable(GL_LINE_STIPPLE); glLineStipple(1, 0xAAAA);
            glColor3d(.25, .6, .25);
            glBegin(GL_LINE_LOOP);
            for (int s = 0;s < 180;++s)
            {
                V3 p2 = evalBSpline((double)(COMET_N - 2) * s / 180.0, cometCP, COMET_N);
                glVertex3d(p2.x, p2.y, p2.z);
            }
            glEnd();
            glDisable(GL_LINE_STIPPLE);
        }

    } 

    light.DrawLightGizmo();
    drawHUD(delta_time);
}