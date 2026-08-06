//============================================================================
// Name        : PyrosLuaMath.cpp
// Description : Math types (Vec2/3/4, Quaternion, Matrix).
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaMath(sol::state* lua)
	{
		{
			// VEC2
			sol::constructors<sol::types<>, sol::types<float, float>> con;
			lua->new_usertype<Math::Vec2>("Vec2",
				con,
				"x", &Math::Vec2::x,
				"y", &Math::Vec2::y,
				"dotProduct", &Math::Vec2::dotProduct,
				"magnitude", &Math::Vec2::magnitude,
				"magnitudeSQR", &Math::Vec2::magnitudeSQR,
				"distance", &Math::Vec2::distance,
				"distanceSQR", &Math::Vec2::distanceSQR,
				"normalize", &Math::Vec2::normalize,
				"negate", &Math::Vec2::negate,
				"abs", &Math::Vec2::Abs,
				"__add", sol::overload(
					&Vec2_operator_add,
					&Vec2_operator_addS
				),
				"__sub", sol::overload(
					&Vec2_operator_sub,
					&Vec2_operator_subS
				),
				"__mul", sol::overload(
					&Vec2_operator_mul,
					&Vec2_operator_mulS
				),
				"__div", sol::overload(
					&Vec2_operator_div,
					&Vec2_operator_divS
				),
				"__eq", &Math::Vec2::operator==,
				"__lt", &Math::Vec2::operator<,
				"__le", &Math::Vec2::operator<=
				);
		}

		{
			// VEC3
			sol::constructors<sol::types<>, sol::types<float, float, float>> con;
			lua->new_usertype<Math::Vec3>("Vec3",
				con,
				"x", &Math::Vec3::x,
				"y", &Math::Vec3::y,
				"z", &Math::Vec3::z,
				"dotProduct", &Math::Vec3::dotProduct,
				"magnitude", &Math::Vec3::magnitude,
				"magnitudeSQR", &Math::Vec3::magnitudeSQR,
				"distance", &Math::Vec3::distance,
				"distanceSQR", &Math::Vec3::distanceSQR,
				"normalize", &Math::Vec3::normalize,
				"normalizeSelft", &Math::Vec3::normalizeSelf,
				"negate", &Math::Vec3::negate,
				"negateSelf", &Math::Vec3::negateSelf,
				"abs", &Math::Vec3::Abs,
				"cross", &Math::Vec3::cross,
				"__add", sol::overload(
					&Vec3_operator_add,
					&Vec3_operator_addS
				),
				"__sub", sol::overload(
					&Vec3_operator_sub,
					&Vec3_operator_subS
				),
				"__mul", sol::overload(
					&Vec3_operator_mul,
					&Vec3_operator_mulS
				),
				"__div", sol::overload(
					&Vec3_operator_div,
					&Vec3_operator_divS
				),
				"__eq", &Math::Vec3::operator==,
				"__lt", &Math::Vec3::operator<,
				"__le", &Math::Vec3::operator<=
				);
		}

		{
			// VEC4
			sol::constructors<sol::types<>, sol::types<float, float, float, float>> con;
			lua->new_usertype<Math::Vec4>("Vec4",
				con,
				"x", &Math::Vec4::x,
				"y", &Math::Vec4::y,
				"z", &Math::Vec4::z,
				"w", &Math::Vec4::w,
				"dotProduct", &Math::Vec4::dotProduct,
				"magnitude", &Math::Vec4::magnitude,
				"magnitudeSQR", &Math::Vec4::magnitudeSQR,
				"abs", &Math::Vec4::Abs,
				"__add", sol::overload(
					&Vec4_operator_add,
					&Vec4_operator_addS
				),
				"__sub", sol::overload(
					&Vec4_operator_sub,
					&Vec4_operator_subS
				),
				"__mul", sol::overload(
					&Vec4_operator_mul,
					&Vec4_operator_mulS
				),
				"__div", sol::overload(
					&Vec4_operator_div,
					&Vec4_operator_divS
				),
				"__eq", &Math::Vec4::operator==,
				"__lt", &Math::Vec4::operator<,
				"__le", &Math::Vec4::operator<=
				);
		}

		{
			// Quaternion
			sol::constructors<sol::types<>, sol::types<float, float, float>, sol::types<float, float, float, float>, sol::types<Vec3, float>> con;
			lua->new_usertype<Math::Quaternion>("Quaternion",
				con,
				"x", &Math::Quaternion::x,
				"y", &Math::Quaternion::y,
				"z", &Math::Quaternion::z,
				"w", &Math::Quaternion::w,
				"convertToMatrix", &Math::Quaternion::ConvertToMatrix,
				"magnitude", &Math::Quaternion::Magnitude,
				"dot", &Math::Quaternion::Dot,
				"abs", &Math::Quaternion::Normalize,
				"rotation", &Math::Quaternion::Rotation,
				"setRotationFromEuler", &Math::Quaternion::SetRotationFromEuler,
				"getEulerRotation", &Math::Quaternion::GetEulerFromQuaternion,
				"axisToQuaternion", &Math::Quaternion::AxisToQuaternion,
				"slerp", &Math::Quaternion::Slerp,
				"nlerp", &Math::Quaternion::Nlerp,
				"inverse", &Math::Quaternion::Inverse,
				"__add", &Quaternion::operator+,
				"__sub", &Quaternion_operator_negate,
				"__mul", sol:: overload(
					&Quaternion_operator_mul,
					&Quaternion_operator_mulS,
					&Quaternion_operator_mulVec3
				)
				);
		}

		{
			// Matrix
			sol::constructors<sol::types<>, sol::types<float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float>> con;
			lua->new_usertype<Math::Matrix>("Matrix",
				con,
				"identity", &Math::Matrix::identity,
				"lookAt", sol::overload(&Matrix_lookAt, &Matrix_lookAt2),
				"translate", sol::overload(&Matrix_translateXYZ, &Matrix_translateVec3),
				"translateX", &Math::Matrix::TranslateX,
				"translateY", &Math::Matrix::TranslateY,
				"translateZ", &Math::Matrix::TranslateZ,
				"getTranslation", &Math::Matrix::GetTranslation,
				"rotationX", &Math::Matrix::RotationX,
				"rotationY", &Math::Matrix::RotationY,
				"rotationZ", &Math::Matrix::RotationZ,
				"setRotationFromEuler", &Math::Matrix::SetRotationFromEuler,
				"getEuler", &Math::Matrix::GetEulerFromRotationMatrix,
				"getRotation", &Math::Matrix::GetRotation,
				"scale", sol::overload(&Matrix_scaleXYZ, &Matrix_scaleVec3),
				"scaleX", &Math::Matrix::ScaleX,
				"scaleY", &Math::Matrix::ScaleY,
				"scaleZ", &Math::Matrix::ScaleZ,
				"getScale", &Math::Matrix::GetScale,
				"getDeterminant", &Math::Matrix::Determinant,
				"transpose", &Math::Matrix::Transpose,
				"inverse", &Math::Matrix::Inverse,
				"perspectiveMatrix", &Math::Matrix::PerspectiveMatrix,
				"orthoMatrix", &Math::Matrix::OrthoMatrix,
				"convertToQuaternion", &Math::Matrix::ConvertToQuaternion,
				"__mul", sol::overload(
					&Matrix_operator_mul,
					&Matrix_operator_mulS,
					&Matrix_operator_mulVec3,
					&Matrix_operator_mulVec4
				),
				"__eq", &Math::Matrix::operator==
				);
		}

	}

} // namespace p3d

#endif
