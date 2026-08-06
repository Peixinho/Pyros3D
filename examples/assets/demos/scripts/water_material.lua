-- Pure-Lua water material: CustomShaderMaterial + WaterShader.glsl UBOs.
local function createWaterMaterial(shaderPath)
	local mat = CustomShaderMaterial.new(shaderPath)

	mat:addUniform(Uniform.new("uProjectionMatrix", UniformUsage.ProjectionMatrix, 0))
	mat:addUniform(Uniform.new("uViewMatrix", UniformUsage.ViewMatrix, 0))
	mat:addUniform(Uniform.new("uModelMatrix", UniformUsage.ModelMatrix, 0))
	mat:addUniform(Uniform.new("uColor", UniformUsage.Other, UniformDataType.Vec4))
	mat:addUniform(Uniform.new("uTime", UniformUsage.Timer, 0))
	mat:addUniform(Uniform.new("uCameraPos", UniformUsage.CameraPosition, 0))
	mat:addUniform(Uniform.new("uNearFarPlane", UniformUsage.NearFarPlane, 0))

	setMaterialExtraUniformBlock(mat, 0, 40, "WaterVertParams", 208, {
		uProjectionMatrix = 0,
		uViewMatrix = 64,
		uModelMatrix = 128,
		uCameraPos = 192,
	})
	setMaterialExtraUniformBlock(mat, 1, 41, "WaterFragParams", 16, {
		uNearFarPlane = 0,
		uTime = 8,
	})

	-- Water uses FragColor.a from depth; needs alpha blending + sorted
	-- transparent pass (IsTransparent).
	mat:setTransparencyFlag(true)
	mat:enableBlending()
	mat:blendingFunction(BlendFunc.Src_Alpha, BlendFunc.One_Minus_Src_Alpha)
	mat:disableDepthWrite()

	return mat
end

return createWaterMaterial
