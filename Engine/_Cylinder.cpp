#include "_Cylinder.h"
#include "BaseGeometry.h"
#include "GL3W\include\glew.h"

_Cylinder::_Cylinder(Prim_Type t, const float3 p, bool k, Color c, float lenght, float radius, bool w) :BaseGeometry(t, p, k, c, w)
{
}

_Cylinder::~_Cylinder()
{
}

void _Cylinder::Create()
{

}

void _Cylinder::MakeCylinder(float radius, float length, unsigned int numSteps)
{
}