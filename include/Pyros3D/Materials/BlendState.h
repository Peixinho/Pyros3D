//============================================================================
// Name        : BlendState.h
// Author      : Duarte Peixinho
// Description : How a material is blended and depth-tested - what IMaterial
//               keeps and the renderer reads
//============================================================================

#ifndef BLENDSTATE_H
#define BLENDSTATE_H

namespace p3d {

	namespace BlendFunc
	{
		enum {
			Zero = 0,
			One,
			Src_Color,
			One_Minus_Src_Color,
			Dst_Color,
			One_Minus_Dst_Color,
			Src_Alpha,
			One_Minus_Src_Alpha,
			Dst_Alpha,
			One_Minus_Dst_Alpha,
			Constant_Color,
			One_Minus_Constant_Color,
			Constant_Alpha,
			One_Minus_Constant_Alpha,
			Src_Alpha_Saturate,
			Src1_Color,
			One_Minus_Src1_Color,
			Src1_Alpha,
			One_Minus_Src1_Alpha
		};
	}

	namespace BlendEq
	{
		enum {
			Add = 0,
			Subtract,
			Reverse_Subtract
		};
	}

	namespace DepthTest
	{
		enum {
			Less = 0,
			Never,
			Greater,
			Equal,
			Always,
			LEqual,
			GEqual,
			NotEqual
		};
	}

}

#endif /* BLENDSTATE_H */
