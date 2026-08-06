//============================================================================
// Name        : PyrosEmbindMath.cpp
// Description : Embind math types (Vec2/3/4, Quaternion, Matrix).
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Utils/Bindings/PyrosEmbindHelpers.h>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;
using namespace p3d::embind_helpers;

namespace {

	Vec2 Vec2_add(const Vec2 &a, const Vec2 &b) { return a + b; }
	Vec2 Vec2_sub(const Vec2 &a, const Vec2 &b) { return a - b; }
	Vec2 Vec2_mul(const Vec2 &a, const Vec2 &b) { return a * b; }
	Vec2 Vec2_div(const Vec2 &a, const Vec2 &b) { return a / b; }
	Vec2 Vec2_addS(const Vec2 &a, float f) { return a + f; }
	Vec2 Vec2_subS(const Vec2 &a, float f) { return a - f; }
	Vec2 Vec2_mulS(const Vec2 &a, float f) { return a * f; }
	Vec2 Vec2_divS(const Vec2 &a, float f) { return a / f; }

	Vec3 Vec3_add(const Vec3 &a, const Vec3 &b) { return a + b; }
	Vec3 Vec3_sub(const Vec3 &a, const Vec3 &b) { return a - b; }
	Vec3 Vec3_mul(const Vec3 &a, const Vec3 &b) { return a * b; }
	Vec3 Vec3_div(const Vec3 &a, const Vec3 &b) { return a / b; }
	Vec3 Vec3_addS(const Vec3 &a, float f) { return a + f; }
	Vec3 Vec3_subS(const Vec3 &a, float f) { return a - f; }
	Vec3 Vec3_mulS(const Vec3 &a, float f) { return a * f; }
	Vec3 Vec3_divS(const Vec3 &a, float f) { return a / f; }

	Vec4 Vec4_add(const Vec4 &a, const Vec4 &b) { return a + b; }
	Vec4 Vec4_sub(const Vec4 &a, const Vec4 &b) { return a - b; }
	Vec4 Vec4_mul(const Vec4 &a, const Vec4 &b) { return a * b; }
	Vec4 Vec4_div(const Vec4 &a, const Vec4 &b) { return a / b; }
	Vec4 Vec4_addS(const Vec4 &a, float f) { return a + f; }
	Vec4 Vec4_subS(const Vec4 &a, float f) { return a - f; }
	Vec4 Vec4_mulS(const Vec4 &a, float f) { return a * f; }
	Vec4 Vec4_divS(const Vec4 &a, float f) { return a / f; }

	Matrix Matrix_mul(const Matrix &a, const Matrix &b) { return a * b; }
	Matrix Matrix_mulS(const Matrix &a, float f) { return a * f; }
	Vec3 Matrix_mulVec3(const Matrix &a, const Vec3 &v) { return a * v; }
	Vec4 Matrix_mulVec4(const Matrix &a, const Vec4 &v) { return a * v; }

	void Matrix_lookAt3(Matrix &m, const Vec3 &eye, const Vec3 &center, const Vec3 &up) { m.LookAt(eye, center, up); }
	void Matrix_lookAt2(Matrix &m, const Vec3 &eye, const Vec3 &center) { m.LookAt(eye, center); }
	void Matrix_translateXYZ(Matrix &m, float x, float y, float z) { m.Translate(x, y, z); }
	void Matrix_translateVec3(Matrix &m, const Vec3 &v) { m.Translate(v); }
	void Matrix_scaleXYZ(Matrix &m, float x, float y, float z) { m.Scale(x, y, z); }
	void Matrix_scaleVec3(Matrix &m, const Vec3 &v) { m.Scale(v); }

	Quaternion Quaternion_mul(const Quaternion &a, const Quaternion &b) { return a * b; }
	Quaternion Quaternion_mulS(const Quaternion &a, float s) { return a * s; }
	Vec3 Quaternion_mulVec3(const Quaternion &a, const Vec3 &v) { return a * v; }
	Quaternion Quaternion_negate(const Quaternion &q) { return -q; }

} // namespace

namespace p3d {
	void PyrosEmbindMathForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_math)
{
	class_<Vec2>("Vec2")
		.constructor<>()
		.constructor<float, float>()
		.property("x", &Vec2::x)
		.property("y", &Vec2::y)
		.function("dotProduct", &Vec2::dotProduct)
		.function("magnitude", &Vec2::magnitude)
		.function("magnitudeSQR", &Vec2::magnitudeSQR)
		.function("distance", &Vec2::distance)
		.function("distanceSQR", &Vec2::distanceSQR)
		.function("normalize", &Vec2::normalize)
		.function("negate", &Vec2::negate)
		.function("abs", &Vec2::Abs)
		.function("add", &Vec2_add)
		.function("sub", &Vec2_sub)
		.function("mul", &Vec2_mul)
		.function("div", &Vec2_div)
		.function("addScalar", &Vec2_addS)
		.function("subScalar", &Vec2_subS)
		.function("mulScalar", &Vec2_mulS)
		.function("divScalar", &Vec2_divS);

	class_<Vec3>("Vec3")
		.constructor<>()
		.constructor<float, float, float>()
		.property("x", &Vec3::x)
		.property("y", &Vec3::y)
		.property("z", &Vec3::z)
		.function("dotProduct", &Vec3::dotProduct)
		.function("magnitude", &Vec3::magnitude)
		.function("magnitudeSQR", &Vec3::magnitudeSQR)
		.function("distance", &Vec3::distance)
		.function("distanceSQR", &Vec3::distanceSQR)
		.function("normalize", &Vec3::normalize)
		.function("normalizeSelf", &Vec3::normalizeSelf)
		.function("normalizeSelft", &Vec3::normalizeSelf) // Lua typo alias
		.function("negate", &Vec3::negate)
		.function("negateSelf", &Vec3::negateSelf)
		.function("abs", &Vec3::Abs)
		.function("cross", &Vec3::cross)
		.function("add", &Vec3_add)
		.function("sub", &Vec3_sub)
		.function("mul", &Vec3_mul)
		.function("div", &Vec3_div)
		.function("addScalar", &Vec3_addS)
		.function("subScalar", &Vec3_subS)
		.function("mulScalar", &Vec3_mulS)
		.function("divScalar", &Vec3_divS);

	class_<Vec4>("Vec4")
		.constructor<>()
		.constructor<float, float, float, float>()
		.property("x", &Vec4::x)
		.property("y", &Vec4::y)
		.property("z", &Vec4::z)
		.property("w", &Vec4::w)
		.function("dotProduct", &Vec4::dotProduct)
		.function("magnitude", &Vec4::magnitude)
		.function("magnitudeSQR", &Vec4::magnitudeSQR)
		.function("abs", &Vec4::Abs)
		.function("add", &Vec4_add)
		.function("sub", &Vec4_sub)
		.function("mul", &Vec4_mul)
		.function("div", &Vec4_div)
		.function("addScalar", &Vec4_addS)
		.function("subScalar", &Vec4_subS)
		.function("mulScalar", &Vec4_mulS)
		.function("divScalar", &Vec4_divS);

	class_<Quaternion>("Quaternion")
		.constructor<>()
		.constructor<float, float, float>()
		.constructor<float, float, float, float>()
		.constructor<Vec3, float>()
		.property("x", &Quaternion::x)
		.property("y", &Quaternion::y)
		.property("z", &Quaternion::z)
		.property("w", &Quaternion::w)
		.function("convertToMatrix", &Quaternion::ConvertToMatrix)
		.function("magnitude", &Quaternion::Magnitude)
		.function("dot", &Quaternion::Dot)
		.function("abs", &Quaternion::Normalize)
		.function("rotation", &Quaternion::Rotation)
		.function("setRotationFromEuler", &Quaternion::SetRotationFromEuler)
		.function("getEulerRotation", &Quaternion::GetEulerFromQuaternion)
		.function("axisToQuaternion", &Quaternion::AxisToQuaternion)
		.function("slerp", &Quaternion::Slerp)
		.function("nlerp", &Quaternion::Nlerp)
		.function("inverse", &Quaternion::Inverse)
		.function("add", select_overload<Quaternion(const Quaternion&)const>(&Quaternion::operator+))
		.function("negate", &Quaternion_negate)
		.function("mul", &Quaternion_mul)
		.function("mulScalar", &Quaternion_mulS)
		.function("mulVec3", &Quaternion_mulVec3);

	class_<Matrix>("Matrix")
		.constructor<>()
		.constructor<float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float>()
		.function("identity", &Matrix::identity)
		.function("lookAt", &Matrix_lookAt3)
		.function("lookAt2", &Matrix_lookAt2)
		.function("translate", &Matrix_translateXYZ)
		.function("translateVec", &Matrix_translateVec3)
		.function("translateX", &Matrix::TranslateX)
		.function("translateY", &Matrix::TranslateY)
		.function("translateZ", &Matrix::TranslateZ)
		.function("getTranslation", &Matrix::GetTranslation)
		.function("rotationX", &Matrix::RotationX)
		.function("rotationY", &Matrix::RotationY)
		.function("rotationZ", &Matrix::RotationZ)
		.function("setRotationFromEuler", &Matrix::SetRotationFromEuler)
		.function("getEuler", &Matrix::GetEulerFromRotationMatrix)
		.function("getRotation", &Matrix::GetRotation)
		.function("scale", &Matrix_scaleXYZ)
		.function("scaleVec", &Matrix_scaleVec3)
		.function("scaleX", &Matrix::ScaleX)
		.function("scaleY", &Matrix::ScaleY)
		.function("scaleZ", &Matrix::ScaleZ)
		.function("getScale", &Matrix::GetScale)
		.function("getDeterminant", &Matrix::Determinant)
		.function("transpose", &Matrix::Transpose)
		.function("inverse", &Matrix::Inverse)
		.class_function("perspectiveMatrix", &Matrix::PerspectiveMatrix)
		.class_function("orthoMatrix", &Matrix::OrthoMatrix)
		.function("convertToQuaternion", &Matrix::ConvertToQuaternion)
		.function("mul", &Matrix_mul)
		.function("mulScalar", &Matrix_mulS)
		.function("mulVec3", &Matrix_mulVec3)
		.function("mulVec4", &Matrix_mulVec4);

	emscripten::function("degToRad", &DegToRad);
}

#endif /* EMSCRIPTEN */
