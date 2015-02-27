
#include <emscripten.h>

class CustomShaderMaterial : public p3d::IMaterial {
public:
  void __destroy__() {
    EM_ASM_INT({
      var self = Module['getCache'](Module['CustomShaderMaterial'])[$0];
      if (!self.hasOwnProperty('__destroy__')) throw 'a JSImplementation must implement all functions, you forgot CustomShaderMaterial::__destroy__.';
      self.__destroy__();
    }, (int)this);
  }
};

class GenericShaderMaterial : public p3d::IMaterial {
public:
  void SetColor(const p3d::Math::Vec4 arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetColor')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetColor.';
      self.SetColor($1);
    }, (int)this, (int)&arg0);
  }
  void SetSpecular(const p3d::Math::Vec4 arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetSpecular')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetSpecular.';
      self.SetSpecular($1);
    }, (int)this, (int)&arg0);
  }
  void SetColorMap(p3d::Texture arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetColorMap')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetColorMap.';
      self.SetColorMap($1);
    }, (int)this, (int)&arg0);
  }
  void SetSpecularMap(p3d::Texture arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetSpecularMap')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetSpecularMap.';
      self.SetSpecularMap($1);
    }, (int)this, (int)&arg0);
  }
  void SetNormalMap(p3d::Texture arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetNormalMap')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetNormalMap.';
      self.SetNormalMap($1);
    }, (int)this, (int)&arg0);
  }
  void SetEnvMap(p3d::Texture arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetEnvMap')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetEnvMap.';
      self.SetEnvMap($1);
    }, (int)this, (int)&arg0);
  }
  void SetReflectivity(float arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetReflectivity')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetReflectivity.';
      self.SetReflectivity($1);
    }, (int)this, arg0);
  }
  void SetLightingProperties(const p3d::Math::Vec4 arg0, const p3d::Math::Vec4 arg1, const p3d::Math::Vec4 arg2, const p3d::Math::Vec4 arg3, float arg4) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetLightingProperties')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetLightingProperties.';
      self.SetLightingProperties($1,$2,$3,$4,$5);
    }, (int)this, (int)&arg0, (int)&arg1, (int)&arg2, (int)&arg3, arg4);
  }
  void SetKe(const p3d::Math::Vec4 arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetKe')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetKe.';
      self.SetKe($1);
    }, (int)this, (int)&arg0);
  }
  void SetKa(const p3d::Math::Vec4 arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetKa')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetKa.';
      self.SetKa($1);
    }, (int)this, (int)&arg0);
  }
  void SetKd(const p3d::Math::Vec4 arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetKd')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetKd.';
      self.SetKd($1);
    }, (int)this, (int)&arg0);
  }
  void SetKs(const p3d::Math::Vec4 arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetKs')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetKs.';
      self.SetKs($1);
    }, (int)this, (int)&arg0);
  }
  void SetShininess(float arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetShininess')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetShininess.';
      self.SetShininess($1);
    }, (int)this, arg0);
  }
  void PreRender() {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('PreRender')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::PreRender.';
      self.PreRender();
    }, (int)this);
  }
  void AfterRender() {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('AfterRender')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::AfterRender.';
      self.AfterRender();
    }, (int)this);
  }
  void BindTextures() {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('BindTextures')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::BindTextures.';
      self.BindTextures();
    }, (int)this);
  }
  void UnbindTextures() {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('UnbindTextures')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::UnbindTextures.';
      self.UnbindTextures();
    }, (int)this);
  }
  void SetPCFTexelSize(float arg0) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetPCFTexelSize')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetPCFTexelSize.';
      self.SetPCFTexelSize($1);
    }, (int)this, arg0);
  }
  void SetPCFTexelCascadesSize(float arg0, float arg1, float arg2, float arg3) {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('SetPCFTexelCascadesSize')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::SetPCFTexelCascadesSize.';
      self.SetPCFTexelCascadesSize($1,$2,$3,$4);
    }, (int)this, arg0, arg1, arg2, arg3);
  }
  void __destroy__() {
    EM_ASM_INT({
      var self = Module['getCache'](Module['GenericShaderMaterial'])[$0];
      if (!self.hasOwnProperty('__destroy__')) throw 'a JSImplementation must implement all functions, you forgot GenericShaderMaterial::__destroy__.';
      self.__destroy__();
    }, (int)this);
  }
};

class ForwardRenderer : public p3d::IRenderer {
public:
  void __destroy__() {
    EM_ASM_INT({
      var self = Module['getCache'](Module['ForwardRenderer'])[$0];
      if (!self.hasOwnProperty('__destroy__')) throw 'a JSImplementation must implement all functions, you forgot ForwardRenderer::__destroy__.';
      self.__destroy__();
    }, (int)this);
  }
};

extern "C" {

// Not using size_t for array indices as the values used by the javascript code are signed.
void array_bounds_check(const int array_size, const int array_idx) {
  if (array_idx < 0 || array_idx >= array_size) {
    EM_ASM_INT({
      throw 'Array index ' + $0 + ' out of bounds: [0,' + $1 + ')';
    }, array_idx, array_size);
  }
}

// Renderable

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Renderable___destroy___0(p3d::Renderable* self) {
  delete self;
}

// Matrix

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Matrix_0() {
  return new p3d::Math::Matrix();
}

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Matrix_16(float arg0, float arg1, float arg2, float arg3, float arg4, float arg5, float arg6, float arg7, float arg8, float arg9, float arg10, float arg11, float arg12, float arg13, float arg14, float arg15) {
  return new p3d::Math::Matrix(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_identity_0(p3d::Math::Matrix* self) {
  self->identity();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_LookAt_2(p3d::Math::Matrix* self, p3d::Math::Vec3* arg0, p3d::Math::Vec3* arg1) {
  self->LookAt(*arg0, *arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_LookAt_3(p3d::Math::Matrix* self, p3d::Math::Vec3* arg0, p3d::Math::Vec3* arg1, p3d::Math::Vec3* arg2) {
  self->LookAt(*arg0, *arg1, *arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Translate_1(p3d::Math::Matrix* self, p3d::Math::Vec3* arg0) {
  self->Translate(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Translate_3(p3d::Math::Matrix* self, float arg0, float arg1, float arg2) {
  self->Translate(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_TranslateX_1(p3d::Math::Matrix* self, float arg0) {
  self->TranslateX(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_TranslateY_1(p3d::Math::Matrix* self, float arg0) {
  self->TranslateY(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_TranslateZ_1(p3d::Math::Matrix* self, float arg0) {
  self->TranslateZ(arg0);
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_GetTranslation_0(p3d::Math::Matrix* self) {
  static p3d::Math::Vec3 temp;
  return (temp = self->GetTranslation(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_RotationX_1(p3d::Math::Matrix* self, float arg0) {
  self->RotationX(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_RotationY_1(p3d::Math::Matrix* self, float arg0) {
  self->RotationY(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_RotationZ_1(p3d::Math::Matrix* self, float arg0) {
  self->RotationZ(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_SetRotationFromEuler_2(p3d::Math::Matrix* self, p3d::Math::Vec3* arg0, int arg1) {
  self->SetRotationFromEuler(*arg0, arg1);
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_GetEulerFromRotationMatrix_1(p3d::Math::Matrix* self, int arg0) {
  static p3d::Math::Vec3 temp;
  return (temp = self->GetEulerFromRotationMatrix(arg0), &temp);
}

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_GetRotation_1(p3d::Math::Matrix* self, p3d::Math::Vec3* arg0) {
  static p3d::Math::Matrix temp;
  return (temp = self->GetRotation(*arg0), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Scale_1(p3d::Math::Matrix* self, p3d::Math::Vec3* arg0) {
  self->Scale(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Scale_3(p3d::Math::Matrix* self, float arg0, float arg1, float arg2) {
  self->Scale(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_ScaleX_1(p3d::Math::Matrix* self, float arg0) {
  self->ScaleX(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_ScaleY_1(p3d::Math::Matrix* self, float arg0) {
  self->ScaleY(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_ScaleZ_1(p3d::Math::Matrix* self, float arg0) {
  self->ScaleZ(arg0);
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_GetScale_0(p3d::Math::Matrix* self) {
  static p3d::Math::Vec3 temp;
  return (temp = self->GetScale(), &temp);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Determinant_0(p3d::Math::Matrix* self) {
  return self->Determinant();
}

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Transpose_0(p3d::Math::Matrix* self) {
  static p3d::Math::Matrix temp;
  return (temp = self->Transpose(), &temp);
}

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_Inverse_0(p3d::Math::Matrix* self) {
  static p3d::Math::Matrix temp;
  return (temp = self->Inverse(), &temp);
}

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_PerspectiveMatrix_4(p3d::Math::Matrix* self, float arg0, float arg1, float arg2, float arg3) {
  static p3d::Math::Matrix temp;
  return (temp = self->PerspectiveMatrix(arg0, arg1, arg2, arg3), &temp);
}

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_OrthoMatrix_6(p3d::Math::Matrix* self, float arg0, float arg1, float arg2, float arg3, float arg4, float arg5) {
  static p3d::Math::Matrix temp;
  return (temp = self->OrthoMatrix(arg0, arg1, arg2, arg3, arg4, arg5), &temp);
}

p3d::Math::Quaternion* EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_ConvertToQuaternion_0(p3d::Math::Matrix* self) {
  static p3d::Math::Quaternion temp;
  return (temp = self->ConvertToQuaternion(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix_mul_1(p3d::Math::Matrix* self, p3d::Math::Matrix* arg0) {
  (*self *= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Matrix___destroy___0(p3d::Math::Matrix* self) {
  delete self;
}

// Texture

p3d::Texture* EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_Texture_0() {
  return new p3d::Texture();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_LoadTexture_3(p3d::Texture* self, char* arg0, int arg1, bool arg2) {
  return self->LoadTexture(arg0, arg1, arg2);
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_CreateTexture_1(p3d::Texture* self, bool arg0) {
  return self->CreateTexture(arg0);
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_CreateTexture_5(p3d::Texture* self, int arg0, int arg1, int arg2, int arg3, bool arg4) {
  return self->CreateTexture(arg0, arg1, arg2, arg3, arg4);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_SetMinMagFilter_2(p3d::Texture* self, int arg0, int arg1) {
  self->SetMinMagFilter(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_SetRepeat_3(p3d::Texture* self, int arg0, int arg1, int arg2) {
  self->SetRepeat(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_EnableCompareMode_0(p3d::Texture* self) {
  self->EnableCompareMode();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_SetAnysotropy_1(p3d::Texture* self, int arg0) {
  self->SetAnysotropy(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_SetTransparency_1(p3d::Texture* self, float arg0) {
  self->SetTransparency(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_Resize_2(p3d::Texture* self, int arg0, int arg1) {
  self->Resize(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_UpdateData_1(p3d::Texture* self, void* arg0) {
  self->UpdateData(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_UpdateMipmap_0(p3d::Texture* self) {
  self->UpdateMipmap();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_SetTextureByteAlignment_1(p3d::Texture* self, int arg0) {
  self->SetTextureByteAlignment(arg0);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_GetBindID_0(p3d::Texture* self) {
  return self->GetBindID();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_GetWidth_0(p3d::Texture* self) {
  return self->GetWidth();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_GetHeight_0(p3d::Texture* self) {
  return self->GetHeight();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_Bind_0(p3d::Texture* self) {
  self->Bind();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_Unbind_0(p3d::Texture* self) {
  self->Unbind();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_DeleteTexture_0(p3d::Texture* self) {
  self->DeleteTexture();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture_GetLastBindedUnit_0(p3d::Texture* self) {
  return self->GetLastBindedUnit();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Texture___destroy___0(p3d::Texture* self) {
  delete self;
}

// Uniform

p3d::Uniforms::Uniform* EMSCRIPTEN_KEEPALIVE emscripten_bind_Uniform_Uniform_0() {
  return new p3d::Uniforms::Uniform();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Uniform___destroy___0(p3d::Uniforms::Uniform* self) {
  delete self;
}

// IRenderer

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_ClearBufferBit_1(p3d::IRenderer* self, int arg0) {
  self->ClearBufferBit(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableDepthTest_0(p3d::IRenderer* self) {
  self->EnableDepthTest();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableDepthTest_0(p3d::IRenderer* self) {
  self->DisableDepthTest();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableDepthWritting_0(p3d::IRenderer* self) {
  self->EnableDepthWritting();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableDepthWritting_0(p3d::IRenderer* self) {
  self->DisableDepthWritting();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableClearDepthBuffer_0(p3d::IRenderer* self) {
  self->EnableClearDepthBuffer();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableClearDepthBuffer_0(p3d::IRenderer* self) {
  self->DisableClearDepthBuffer();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_ClearDepthBuffer_0(p3d::IRenderer* self) {
  self->ClearDepthBuffer();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableStencil_0(p3d::IRenderer* self) {
  self->EnableStencil();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableStencil_0(p3d::IRenderer* self) {
  self->DisableStencil();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_ClearStencilBuffer_0(p3d::IRenderer* self) {
  self->ClearStencilBuffer();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_StencilFunction_3(p3d::IRenderer* self, int arg0, int arg1, int arg2) {
  self->StencilFunction(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_StencilOperation_3(p3d::IRenderer* self, int arg0, int arg1, int arg2) {
  self->StencilOperation(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableBlending_0(p3d::IRenderer* self) {
  self->EnableBlending();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableBlending_0(p3d::IRenderer* self) {
  self->DisableBlending();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_BlendingFunction_2(p3d::IRenderer* self, int arg0, int arg1) {
  self->BlendingFunction(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_BlendingEquation_1(p3d::IRenderer* self, int arg0) {
  self->BlendingEquation(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableScissorTest_0(p3d::IRenderer* self) {
  self->EnableScissorTest();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableScissorTest_0(p3d::IRenderer* self) {
  self->DisableScissorTest();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_ScissorTestRect_4(p3d::IRenderer* self, float arg0, float arg1, float arg2, float arg3) {
  self->ScissorTestRect(arg0, arg1, arg2, arg3);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableWireFrame_0(p3d::IRenderer* self) {
  self->EnableWireFrame();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableWireFrame_0(p3d::IRenderer* self) {
  self->DisableWireFrame();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_ColorMask_4(p3d::IRenderer* self, float arg0, float arg1, float arg2, float arg3) {
  self->ColorMask(arg0, arg1, arg2, arg3);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableSorting_0(p3d::IRenderer* self) {
  self->EnableSorting();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableSorting_0(p3d::IRenderer* self) {
  self->DisableSorting();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableLOD_0(p3d::IRenderer* self) {
  self->EnableLOD();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableLOD_0(p3d::IRenderer* self) {
  self->DisableLOD();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_IsUsingLOD_0(p3d::IRenderer* self) {
  return self->IsUsingLOD();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_SetBackground_1(p3d::IRenderer* self, p3d::Math::Vec4* arg0) {
  self->SetBackground(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_UnsetBackground_0(p3d::IRenderer* self) {
  self->UnsetBackground();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_SetGlobalLight_1(p3d::IRenderer* self, p3d::Math::Vec4* arg0) {
  self->SetGlobalLight(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_EnableDepthBias_1(p3d::IRenderer* self, p3d::Math::Vec2* arg0) {
  self->EnableDepthBias(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DisableDepthBias_0(p3d::IRenderer* self) {
  self->DisableDepthBias();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_SetViewPort_4(p3d::IRenderer* self, int arg0, int arg1, int arg2, int arg3) {
  self->SetViewPort(arg0, arg1, arg2, arg3);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_Resize_2(p3d::IRenderer* self, int arg0, int arg1) {
  self->Resize(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_ActivateCulling_1(p3d::IRenderer* self, int arg0) {
  self->ActivateCulling(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_DeactivateCulling_0(p3d::IRenderer* self) {
  self->DeactivateCulling();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer_RenderScene_3(p3d::IRenderer* self, p3d::Projection* arg0, p3d::GameObject* arg1, p3d::SceneGraph* arg2) {
  self->RenderScene(*arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IRenderer___destroy___0(p3d::IRenderer* self) {
  delete self;
}

// IMaterial

p3d::IMaterial* EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_IMaterial_0() {
  return new p3d::IMaterial();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_PreRender_0(p3d::IMaterial* self) {
  self->PreRender();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_Render_0(p3d::IMaterial* self) {
  self->Render();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_AfterRender_0(p3d::IMaterial* self) {
  self->AfterRender();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_SetCullFace_1(p3d::IMaterial* self, int arg0) {
  self->SetCullFace(arg0);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_GetCullFace_0(p3d::IMaterial* self) {
  return self->GetCullFace();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_IsWireFrame_0(p3d::IMaterial* self) {
  return self->IsWireFrame();
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_GetOpacity_0(p3d::IMaterial* self) {
  return self->GetOpacity();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_IsTransparent_0(p3d::IMaterial* self) {
  return self->IsTransparent();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_SetOpacity_1(p3d::IMaterial* self, float arg0) {
  self->SetOpacity(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_SetTransparencyFlag_1(p3d::IMaterial* self, bool arg0) {
  self->SetTransparencyFlag(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_SetUniformValue_2(p3d::IMaterial* self, char* arg0, float arg1) {
  self->SetUniformValue(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_SetUniformValue_3(p3d::IMaterial* self, char* arg0, void* arg1, int arg2) {
  self->SetUniformValue(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_AddUniform_1(p3d::IMaterial* self, p3d::Uniforms::Uniform* arg0) {
  self->AddUniform(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_StartRenderWireFrame_0(p3d::IMaterial* self) {
  self->StartRenderWireFrame();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_StopRenderWireFrame_0(p3d::IMaterial* self) {
  self->StopRenderWireFrame();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_EnableCastingShadows_0(p3d::IMaterial* self) {
  self->EnableCastingShadows();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_DisableCastingShadows_0(p3d::IMaterial* self) {
  self->DisableCastingShadows();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_IsCastingShadows_0(p3d::IMaterial* self) {
  return self->IsCastingShadows();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_Destroy_0(p3d::IMaterial* self) {
  self->Destroy();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_GetShader_0(p3d::IMaterial* self) {
  return self->GetShader();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial_GetInternalID_0(p3d::IMaterial* self) {
  return self->GetInternalID();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IMaterial___destroy___0(p3d::IMaterial* self) {
  delete self;
}

// PyrosJSNative

PyrosJSNative* EMSCRIPTEN_KEEPALIVE emscripten_bind_PyrosJSNative_PyrosJSNative_0() {
  return new PyrosJSNative();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_PyrosJSNative_CreateContext_0(PyrosJSNative* self) {
  self->CreateContext();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_PyrosJSNative___destroy___0(PyrosJSNative* self) {
  delete self;
}

// VoidPtr

void EMSCRIPTEN_KEEPALIVE emscripten_bind_VoidPtr___destroy___0(void** self) {
  delete self;
}

// SkeletonAnimation

p3d::SkeletonAnimation* EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimation_SkeletonAnimation_0() {
  return new p3d::SkeletonAnimation();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimation_LoadAnimation_1(p3d::SkeletonAnimation* self, char* arg0) {
  self->LoadAnimation(arg0);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimation_GetNumberAnimations_0(p3d::SkeletonAnimation* self) {
  return self->GetNumberAnimations();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimation_Update_1(p3d::SkeletonAnimation* self, float arg0) {
  self->Update(arg0);
}

p3d::SkeletonAnimationInstance* EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimation_CreateInstance_1(p3d::SkeletonAnimation* self, p3d::RenderingComponent* arg0) {
  return self->CreateInstance(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimation_DestroyInstance_1(p3d::SkeletonAnimation* self, p3d::SkeletonAnimationInstance* arg0) {
  self->DestroyInstance(arg0);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimation_GetAnimationIDByName_1(p3d::SkeletonAnimation* self, char* arg0) {
  return self->GetAnimationIDByName(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimation___destroy___0(p3d::SkeletonAnimation* self) {
  delete self;
}

// Capsule

p3d::Capsule* EMSCRIPTEN_KEEPALIVE emscripten_bind_Capsule_Capsule_7(float arg0, float arg1, float arg2, int arg3, int arg4, bool arg5, bool arg6) {
  return new p3d::Capsule(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Capsule___destroy___0(p3d::Capsule* self) {
  delete self;
}

// Quaternion

p3d::Math::Quaternion* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Quaternion_0() {
  return new p3d::Math::Quaternion();
}

p3d::Math::Quaternion* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Quaternion_2(p3d::Math::Vec3* arg0, float arg1) {
  return new p3d::Math::Quaternion(*arg0, arg1);
}

p3d::Math::Quaternion* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Quaternion_3(float arg0, float arg1, float arg2) {
  return new p3d::Math::Quaternion(arg0, arg1, arg2);
}

p3d::Math::Quaternion* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Quaternion_4(float arg0, float arg1, float arg2, float arg3) {
  return new p3d::Math::Quaternion(arg0, arg1, arg2, arg3);
}

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_ConvertToMatrix_0(p3d::Math::Quaternion* self) {
  static p3d::Math::Matrix temp;
  return (temp = self->ConvertToMatrix(), &temp);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Magnitude_0(p3d::Math::Quaternion* self) {
  return self->Magnitude();
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Dot_1(p3d::Math::Quaternion* self, p3d::Math::Quaternion* arg0) {
  return self->Dot(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Normalize_0(p3d::Math::Quaternion* self) {
  self->Normalize();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Rotation_3(p3d::Math::Quaternion* self, float arg0, float arg1, float arg2) {
  self->Rotation(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_SetRotationFromEuler_2(p3d::Math::Quaternion* self, p3d::Math::Vec3* arg0, int arg1) {
  self->SetRotationFromEuler(*arg0, arg1);
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_GetEulerFromQuaternion_1(p3d::Math::Quaternion* self, int arg0) {
  static p3d::Math::Vec3 temp;
  return (temp = self->GetEulerFromQuaternion(arg0), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_AxisToQuaternion_2(p3d::Math::Quaternion* self, p3d::Math::Vec3* arg0, float arg1) {
  self->AxisToQuaternion(*arg0, arg1);
}

p3d::Math::Quaternion* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Slerp_2(p3d::Math::Quaternion* self, p3d::Math::Quaternion* arg0, float arg1) {
  static p3d::Math::Quaternion temp;
  return (temp = self->Slerp(*arg0, arg1), &temp);
}

p3d::Math::Quaternion* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Nlerp_3(p3d::Math::Quaternion* self, p3d::Math::Quaternion* arg0, float arg1, bool arg2) {
  static p3d::Math::Quaternion temp;
  return (temp = self->Nlerp(*arg0, arg1, arg2), &temp);
}

p3d::Math::Quaternion* EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_Inverse_0(p3d::Math::Quaternion* self) {
  static p3d::Math::Quaternion temp;
  return (temp = self->Inverse(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_mul_1(p3d::Math::Quaternion* self, p3d::Math::Quaternion* arg0) {
  (*self *= *arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_get_x_0(p3d::Math::Quaternion* self) {
  return self->x;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_set_x_1(p3d::Math::Quaternion* self, float arg0) {
  self->x = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_get_y_0(p3d::Math::Quaternion* self) {
  return self->y;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_set_y_1(p3d::Math::Quaternion* self, float arg0) {
  self->y = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_get_z_0(p3d::Math::Quaternion* self) {
  return self->z;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_set_z_1(p3d::Math::Quaternion* self, float arg0) {
  self->z = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_get_w_0(p3d::Math::Quaternion* self) {
  return self->w;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion_set_w_1(p3d::Math::Quaternion* self, float arg0) {
  self->w = arg0;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Quaternion___destroy___0(p3d::Math::Quaternion* self) {
  delete self;
}

// Plane

p3d::Plane* EMSCRIPTEN_KEEPALIVE emscripten_bind_Plane_Plane_4(float arg0, float arg1, bool arg2, bool arg3) {
  return new p3d::Plane(arg0, arg1, arg2, arg3);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Plane___destroy___0(p3d::Plane* self) {
  delete self;
}

// TextureAnimationInstance

p3d::TextureAnimationInstance* EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance_TextureAnimationInstance_2(p3d::TextureAnimation* arg0, float arg1) {
  return new p3d::TextureAnimationInstance(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance_Play_1(p3d::TextureAnimationInstance* self, int arg0) {
  self->Play(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance_Pause_0(p3d::TextureAnimationInstance* self) {
  self->Pause();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance_Stop_0(p3d::TextureAnimationInstance* self) {
  self->Stop();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance_IsPlaying_0(p3d::TextureAnimationInstance* self) {
  return self->IsPlaying();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance_YoYo_1(p3d::TextureAnimationInstance* self, bool arg0) {
  self->YoYo(arg0);
}

p3d::Texture* EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance_GetTexture_0(p3d::TextureAnimationInstance* self) {
  return self->GetTexture();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance_GetFrame_0(p3d::TextureAnimationInstance* self) {
  return self->GetFrame();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimationInstance___destroy___0(p3d::TextureAnimationInstance* self) {
  delete self;
}

// SkeletonAnimationInstance

p3d::SkeletonAnimationInstance* EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_SkeletonAnimationInstance_2(p3d::SkeletonAnimation* arg0, p3d::RenderingComponent* arg1) {
  return new p3d::SkeletonAnimationInstance(arg0, arg1);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_Play_6(p3d::SkeletonAnimationInstance* self, int arg0, float arg1, float arg2, float arg3, float arg4, char* arg5) {
  return self->Play(arg0, arg1, arg2, arg3, arg4, arg5);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_ChangeProperties_5(p3d::SkeletonAnimationInstance* self, int arg0, float arg1, float arg2, float arg3, float arg4) {
  self->ChangeProperties(arg0, arg1, arg2, arg3, arg4);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_Pause_0(p3d::SkeletonAnimationInstance* self) {
  self->Pause();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_PauseAnimation_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  self->PauseAnimation(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_ResumeAnimation_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  self->ResumeAnimation(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_Resume_0(p3d::SkeletonAnimationInstance* self) {
  self->Resume();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_StopAnimation_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  self->StopAnimation(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_Stop_0(p3d::SkeletonAnimationInstance* self) {
  self->Stop();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_IsPaused_0(p3d::SkeletonAnimationInstance* self) {
  return self->IsPaused();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_IsPaused_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->IsPaused(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationCurrentProgress_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationCurrentProgress(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationDuration_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationDuration(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationCurrentTime_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationCurrentTime(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationSpeed_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationSpeed(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationStartTime_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationStartTime(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationStartTimeProgress_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationStartTimeProgress(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationID_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationID(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationScale_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationScale(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_GetAnimationPositionInVector_1(p3d::SkeletonAnimationInstance* self, int arg0) {
  return self->GetAnimationPositionInVector(arg0);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_CreateLayer_1(p3d::SkeletonAnimationInstance* self, char* arg0) {
  return self->CreateLayer(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_AddBone_2(p3d::SkeletonAnimationInstance* self, char* arg0, char* arg1) {
  self->AddBone(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_AddBoneAndChilds_3(p3d::SkeletonAnimationInstance* self, char* arg0, char* arg1, bool arg2) {
  self->AddBoneAndChilds(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_RemoveBone_2(p3d::SkeletonAnimationInstance* self, char* arg0, char* arg1) {
  self->RemoveBone(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_RemoveBoneAndChilds_2(p3d::SkeletonAnimationInstance* self, char* arg0, char* arg1) {
  self->RemoveBoneAndChilds(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance_DestroyLayer_1(p3d::SkeletonAnimationInstance* self, char* arg0) {
  self->DestroyLayer(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SkeletonAnimationInstance___destroy___0(p3d::SkeletonAnimationInstance* self) {
  delete self;
}

// Cone

p3d::Cone* EMSCRIPTEN_KEEPALIVE emscripten_bind_Cone_Cone_7(float arg0, float arg1, int arg2, int arg3, bool arg4, bool arg5, bool arg6) {
  return new p3d::Cone(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Cone___destroy___0(p3d::Cone* self) {
  delete self;
}

// IComponent

void EMSCRIPTEN_KEEPALIVE emscripten_bind_IComponent___destroy___0(p3d::IComponent* self) {
  delete self;
}

// Torus

p3d::Torus* EMSCRIPTEN_KEEPALIVE emscripten_bind_Torus_Torus_6(float arg0, float arg1, int arg2, int arg3, bool arg4, bool arg5) {
  return new p3d::Torus(arg0, arg1, arg2, arg3, arg4, arg5);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Torus___destroy___0(p3d::Torus* self) {
  delete self;
}

// TorusKnot

p3d::TorusKnot* EMSCRIPTEN_KEEPALIVE emscripten_bind_TorusKnot_TorusKnot_9(float arg0, float arg1, int arg2, int arg3, float arg4, float arg5, int arg6, bool arg7, bool arg8) {
  return new p3d::TorusKnot(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TorusKnot___destroy___0(p3d::TorusKnot* self) {
  delete self;
}

// Shader

p3d::Shader* EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_Shader_0() {
  return new p3d::Shader();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_LoadShaderFile_1(p3d::Shader* self, char* arg0) {
  self->LoadShaderFile(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_LoadShaderText_1(p3d::Shader* self, char* arg0) {
  self->LoadShaderText(arg0);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_GetType_0(p3d::Shader* self) {
  return self->GetType();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_CompileShader_1(p3d::Shader* self, int arg0) {
  self->CompileShader(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_DeleteShader_0(p3d::Shader* self) {
  self->DeleteShader();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_LinkProgram_0(p3d::Shader* self) {
  self->LinkProgram();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_ShaderProgram_0(p3d::Shader* self) {
  return self->ShaderProgram();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_GetUniformLocation_2(p3d::Shader* self, int arg0, char* arg1) {
  return self->GetUniformLocation(arg0, arg1);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_GetAttributeLocation_2(p3d::Shader* self, int arg0, char* arg1) {
  return self->GetAttributeLocation(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_SendUniform_2(p3d::Shader* self, p3d::Uniforms::Uniform* arg0, int arg1) {
  self->SendUniform(*arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader_SendUniform_4(p3d::Shader* self, p3d::Uniforms::Uniform* arg0, void* arg1, int arg2, int arg3) {
  self->SendUniform(*arg0, arg1, arg2, arg3);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Shader___destroy___0(p3d::Shader* self) {
  delete self;
}

// CustomShaderMaterial

p3d::CustomShaderMaterial* EMSCRIPTEN_KEEPALIVE emscripten_bind_CustomShaderMaterial_CustomShaderMaterial_2(char* arg0, char* arg1) {
  return new p3d::CustomShaderMaterial(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_CustomShaderMaterial___destroy___0(p3d::CustomShaderMaterial* self) {
  delete self;
}

// Cube

p3d::Cube* EMSCRIPTEN_KEEPALIVE emscripten_bind_Cube_Cube_5(float arg0, float arg1, float arg2, bool arg3, bool arg4) {
  return new p3d::Cube(arg0, arg1, arg2, arg3, arg4);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Cube___destroy___0(p3d::Cube* self) {
  delete self;
}

// PointLight

p3d::PointLight* EMSCRIPTEN_KEEPALIVE emscripten_bind_PointLight_PointLight_0() {
  return new p3d::PointLight();
}

p3d::PointLight* EMSCRIPTEN_KEEPALIVE emscripten_bind_PointLight_PointLight_2(p3d::Math::Vec4* arg0, float arg1) {
  return new p3d::PointLight(*arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_PointLight_Start_0(p3d::PointLight* self) {
  self->Start();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_PointLight_Update_0(p3d::PointLight* self) {
  self->Update();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_PointLight_Destroy_0(p3d::PointLight* self) {
  self->Destroy();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_PointLight___destroy___0(p3d::PointLight* self) {
  delete self;
}

// GenericShaderMaterial

p3d::GenericShaderMaterial* EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_GenericShaderMaterial_1(int arg0) {
  return new p3d::GenericShaderMaterial(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetColor_1(p3d::GenericShaderMaterial* self, p3d::Math::Vec4* arg0) {
  self->SetColor(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetSpecular_1(p3d::GenericShaderMaterial* self, p3d::Math::Vec4* arg0) {
  self->SetSpecular(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetColorMap_1(p3d::GenericShaderMaterial* self, p3d::Texture* arg0) {
  self->SetColorMap(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetSpecularMap_1(p3d::GenericShaderMaterial* self, p3d::Texture* arg0) {
  self->SetSpecularMap(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetNormalMap_1(p3d::GenericShaderMaterial* self, p3d::Texture* arg0) {
  self->SetNormalMap(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetEnvMap_1(p3d::GenericShaderMaterial* self, p3d::Texture* arg0) {
  self->SetEnvMap(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetReflectivity_1(p3d::GenericShaderMaterial* self, float arg0) {
  self->SetReflectivity(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetLightingProperties_5(p3d::GenericShaderMaterial* self, p3d::Math::Vec4* arg0, p3d::Math::Vec4* arg1, p3d::Math::Vec4* arg2, p3d::Math::Vec4* arg3, float arg4) {
  self->SetLightingProperties(*arg0, *arg1, *arg2, *arg3, arg4);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetKe_1(p3d::GenericShaderMaterial* self, p3d::Math::Vec4* arg0) {
  self->SetKe(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetKa_1(p3d::GenericShaderMaterial* self, p3d::Math::Vec4* arg0) {
  self->SetKa(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetKd_1(p3d::GenericShaderMaterial* self, p3d::Math::Vec4* arg0) {
  self->SetKd(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetKs_1(p3d::GenericShaderMaterial* self, p3d::Math::Vec4* arg0) {
  self->SetKs(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetShininess_1(p3d::GenericShaderMaterial* self, float arg0) {
  self->SetShininess(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_PreRender_0(p3d::GenericShaderMaterial* self) {
  self->PreRender();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_AfterRender_0(p3d::GenericShaderMaterial* self) {
  self->AfterRender();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_BindTextures_0(p3d::GenericShaderMaterial* self) {
  self->BindTextures();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_UnbindTextures_0(p3d::GenericShaderMaterial* self) {
  self->UnbindTextures();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetPCFTexelSize_1(p3d::GenericShaderMaterial* self, float arg0) {
  self->SetPCFTexelSize(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial_SetPCFTexelCascadesSize_4(p3d::GenericShaderMaterial* self, float arg0, float arg1, float arg2, float arg3) {
  self->SetPCFTexelCascadesSize(arg0, arg1, arg2, arg3);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GenericShaderMaterial___destroy___0(p3d::GenericShaderMaterial* self) {
  delete self;
}

// DirectionalLight

p3d::DirectionalLight* EMSCRIPTEN_KEEPALIVE emscripten_bind_DirectionalLight_DirectionalLight_0() {
  return new p3d::DirectionalLight();
}

p3d::DirectionalLight* EMSCRIPTEN_KEEPALIVE emscripten_bind_DirectionalLight_DirectionalLight_1(p3d::Math::Vec4* arg0) {
  return new p3d::DirectionalLight(*arg0);
}

p3d::DirectionalLight* EMSCRIPTEN_KEEPALIVE emscripten_bind_DirectionalLight_DirectionalLight_2(p3d::Math::Vec4* arg0, p3d::Math::Vec3* arg1) {
  return new p3d::DirectionalLight(*arg0, *arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_DirectionalLight_Start_0(p3d::DirectionalLight* self) {
  self->Start();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_DirectionalLight_Update_0(p3d::DirectionalLight* self) {
  self->Update();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_DirectionalLight_Destroy_0(p3d::DirectionalLight* self) {
  self->Destroy();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_DirectionalLight___destroy___0(p3d::DirectionalLight* self) {
  delete self;
}

// Model

p3d::Model* EMSCRIPTEN_KEEPALIVE emscripten_bind_Model_Model_3(char* arg0, bool arg1, int arg2) {
  return new p3d::Model(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Model___destroy___0(p3d::Model* self) {
  delete self;
}

// ForwardRenderer

p3d::ForwardRenderer* EMSCRIPTEN_KEEPALIVE emscripten_bind_ForwardRenderer_ForwardRenderer_2(int arg0, int arg1) {
  return new p3d::ForwardRenderer(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_ForwardRenderer___destroy___0(p3d::ForwardRenderer* self) {
  delete self;
}

// SceneGraph

p3d::SceneGraph* EMSCRIPTEN_KEEPALIVE emscripten_bind_SceneGraph_SceneGraph_0() {
  return new p3d::SceneGraph();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SceneGraph_Update_1(p3d::SceneGraph* self, double arg0) {
  self->Update(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SceneGraph_Add_1(p3d::SceneGraph* self, p3d::GameObject* arg0) {
  self->Add(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SceneGraph_Remove_1(p3d::SceneGraph* self, p3d::GameObject* arg0) {
  self->Remove(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SceneGraph_AddGameObject_1(p3d::SceneGraph* self, p3d::GameObject* arg0) {
  self->AddGameObject(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SceneGraph_RemoveGameObject_1(p3d::SceneGraph* self, p3d::GameObject* arg0) {
  self->RemoveGameObject(arg0);
}

double EMSCRIPTEN_KEEPALIVE emscripten_bind_SceneGraph_GetTime_0(p3d::SceneGraph* self) {
  return self->GetTime();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SceneGraph___destroy___0(p3d::SceneGraph* self) {
  delete self;
}

// TextureAnimation

p3d::TextureAnimation* EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimation_TextureAnimation_0() {
  return new p3d::TextureAnimation();
}

p3d::Texture* EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimation_GetFrame_1(p3d::TextureAnimation* self, int arg0) {
  return self->GetFrame(arg0);
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimation_GetNumberFrames_0(p3d::TextureAnimation* self) {
  return self->GetNumberFrames();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimation_AddFrame_1(p3d::TextureAnimation* self, p3d::Texture* arg0) {
  self->AddFrame(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimation_Update_1(p3d::TextureAnimation* self, float arg0) {
  self->Update(arg0);
}

p3d::TextureAnimationInstance* EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimation_CreateInstance_1(p3d::TextureAnimation* self, float arg0) {
  return self->CreateInstance(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimation_DestroyInstance_1(p3d::TextureAnimation* self, p3d::TextureAnimationInstance* arg0) {
  self->DestroyInstance(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_TextureAnimation___destroy___0(p3d::TextureAnimation* self) {
  delete self;
}

// Cylinder

p3d::Cylinder* EMSCRIPTEN_KEEPALIVE emscripten_bind_Cylinder_Cylinder_7(float arg0, float arg1, int arg2, int arg3, bool arg4, bool arg5, bool arg6) {
  return new p3d::Cylinder(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Cylinder___destroy___0(p3d::Cylinder* self) {
  delete self;
}

// Projection

p3d::Projection* EMSCRIPTEN_KEEPALIVE emscripten_bind_Projection_Projection_0() {
  return new p3d::Projection();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Projection_Perspective_4(p3d::Projection* self, float arg0, float arg1, float arg2, float arg3) {
  self->Perspective(arg0, arg1, arg2, arg3);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Projection_Ortho_6(p3d::Projection* self, float arg0, float arg1, float arg2, float arg3, float arg4, float arg5) {
  self->Ortho(arg0, arg1, arg2, arg3, arg4, arg5);
}

p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_Projection_GetProjectionMatrix_0(p3d::Projection* self) {
  static p3d::Math::Matrix temp;
  return (temp = self->GetProjectionMatrix(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Projection___destroy___0(p3d::Projection* self) {
  delete self;
}

// GameObject

p3d::GameObject* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GameObject_0() {
  return new p3d::GameObject();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_Init_0(p3d::GameObject* self) {
  self->Init();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_Update_0(p3d::GameObject* self) {
  self->Update();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_Destroy_0(p3d::GameObject* self) {
  self->Destroy();
}

const p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetLocalTransformation_0(p3d::GameObject* self) {
  return &self->GetLocalTransformation();
}

const p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetPosition_0(p3d::GameObject* self) {
  return &self->GetPosition();
}

const p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetRotation_0(p3d::GameObject* self) {
  return &self->GetRotation();
}

const p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetScale_0(p3d::GameObject* self) {
  return &self->GetScale();
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetDirection_0(p3d::GameObject* self) {
  static p3d::Math::Vec3 temp;
  return (temp = self->GetDirection(), &temp);
}

const p3d::Math::Matrix* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetWorldTransformation_0(p3d::GameObject* self) {
  return &self->GetWorldTransformation();
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetWorldPosition_0(p3d::GameObject* self) {
  static p3d::Math::Vec3 temp;
  return (temp = self->GetWorldPosition(), &temp);
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetWorldRotation_0(p3d::GameObject* self) {
  static p3d::Math::Vec3 temp;
  return (temp = self->GetWorldRotation(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_SetPosition_1(p3d::GameObject* self, p3d::Math::Vec3* arg0) {
  self->SetPosition(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_SetRotation_1(p3d::GameObject* self, p3d::Math::Vec3* arg0) {
  self->SetRotation(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_SetScale_1(p3d::GameObject* self, p3d::Math::Vec3* arg0) {
  self->SetScale(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_SetTransformationMatrix_1(p3d::GameObject* self, p3d::Math::Matrix* arg0) {
  self->SetTransformationMatrix(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_LookAtGameObject_1(p3d::GameObject* self, p3d::GameObject* arg0) {
  self->LookAtGameObject(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_LookAtVec_1(p3d::GameObject* self, p3d::Math::Vec3* arg0) {
  self->LookAtVec(*arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_AddComponent_1(p3d::GameObject* self, p3d::IComponent* arg0) {
  self->AddComponent(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_RemoveComponent_1(p3d::GameObject* self, p3d::IComponent* arg0) {
  self->RemoveComponent(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_AddGameObject_1(p3d::GameObject* self, p3d::GameObject* arg0) {
  self->AddGameObject(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_RemoveGameObject_1(p3d::GameObject* self, p3d::GameObject* arg0) {
  self->RemoveGameObject(arg0);
}

p3d::GameObject* EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_GetParent_0(p3d::GameObject* self) {
  return self->GetParent();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_HaveParent_0(p3d::GameObject* self) {
  return self->HaveParent();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_AddTag_1(p3d::GameObject* self, char* arg0) {
  self->AddTag(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_RemoveTag_1(p3d::GameObject* self, char* arg0) {
  self->RemoveTag(arg0);
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_HaveTag_1(p3d::GameObject* self, char* arg0) {
  return self->HaveTag(arg0);
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject_IsStatic_0(p3d::GameObject* self) {
  return self->IsStatic();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_GameObject___destroy___0(p3d::GameObject* self) {
  delete self;
}

// RenderingComponent

p3d::RenderingComponent* EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_RenderingComponent_2(p3d::Renderable* arg0, p3d::IMaterial* arg1) {
  return new p3d::RenderingComponent(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_AddLOD_3(p3d::RenderingComponent* self, p3d::Renderable* arg0, float arg1, p3d::IMaterial* arg2) {
  self->AddLOD(arg0, arg1, arg2);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_Register_1(p3d::RenderingComponent* self, p3d::SceneGraph* arg0) {
  self->Register(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_Init_0(p3d::RenderingComponent* self) {
  self->Init();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_Update_0(p3d::RenderingComponent* self) {
  self->Update();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_Destroy_0(p3d::RenderingComponent* self) {
  self->Destroy();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_Unregister_1(p3d::RenderingComponent* self, p3d::SceneGraph* arg0) {
  self->Unregister(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_SetCullingGeometry_1(p3d::RenderingComponent* self, int arg0) {
  self->SetCullingGeometry(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_EnableCastShadows_0(p3d::RenderingComponent* self) {
  self->EnableCastShadows();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_DisableCastShadows_0(p3d::RenderingComponent* self) {
  self->DisableCastShadows();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_IsCastingShadows_0(p3d::RenderingComponent* self) {
  return self->IsCastingShadows();
}

bool EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_HasBones_0(p3d::RenderingComponent* self) {
  return self->HasBones();
}

const int EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_GetLODSize_0(p3d::RenderingComponent* self) {
  return self->GetLODSize();
}

int EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_GetLODByDistance_1(p3d::RenderingComponent* self, float arg0) {
  return self->GetLODByDistance(arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_UpdateLOD_1(p3d::RenderingComponent* self, int arg0) {
  self->UpdateLOD(arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_GetBoundingSphereRadius_0(p3d::RenderingComponent* self) {
  return self->GetBoundingSphereRadius();
}

const p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_GetBoundingSphereCenter_0(p3d::RenderingComponent* self) {
  return &self->GetBoundingSphereCenter();
}

const p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_GetBoundingMinValue_0(p3d::RenderingComponent* self) {
  return &self->GetBoundingMinValue();
}

const p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent_GetBoundingMaxValue_0(p3d::RenderingComponent* self) {
  return &self->GetBoundingMaxValue();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_RenderingComponent___destroy___0(p3d::RenderingComponent* self) {
  delete self;
}

// Sphere

p3d::Sphere* EMSCRIPTEN_KEEPALIVE emscripten_bind_Sphere_Sphere_6(float arg0, int arg1, int arg2, bool arg3, bool arg4, bool arg5) {
  return new p3d::Sphere(arg0, arg1, arg2, arg3, arg4, arg5);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Sphere___destroy___0(p3d::Sphere* self) {
  delete self;
}

// Vec4

p3d::Math::Vec4* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_Vec4_0() {
  return new p3d::Math::Vec4();
}

p3d::Math::Vec4* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_Vec4_4(float arg0, float arg1, float arg2, float arg3) {
  return new p3d::Math::Vec4(arg0, arg1, arg2, arg3);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_dotProduct_1(p3d::Math::Vec4* self, p3d::Math::Vec4* arg0) {
  return self->dotProduct(*arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_magnitude_0(p3d::Math::Vec4* self) {
  return self->magnitude();
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_magnitudeSQR_0(p3d::Math::Vec4* self) {
  return self->magnitudeSQR();
}

p3d::Math::Vec4* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_Abs_0(p3d::Math::Vec4* self) {
  static p3d::Math::Vec4 temp;
  return (temp = self->Abs(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_add_1(p3d::Math::Vec4* self, p3d::Math::Vec4* arg0) {
  (*self += *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_sub_1(p3d::Math::Vec4* self, p3d::Math::Vec4* arg0) {
  (*self -= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_mul_1(p3d::Math::Vec4* self, p3d::Math::Vec4* arg0) {
  (*self *= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_div_1(p3d::Math::Vec4* self, p3d::Math::Vec4* arg0) {
  (*self /= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_add_scalar_1(p3d::Math::Vec4* self, float arg0) {
  (*self += arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_sub_scalar_1(p3d::Math::Vec4* self, float arg0) {
  (*self -= arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_mul_scalar_1(p3d::Math::Vec4* self, float arg0) {
  (*self *= arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_div_scalar_1(p3d::Math::Vec4* self, float arg0) {
  (*self /= arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_get_x_0(p3d::Math::Vec4* self) {
  return self->x;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_set_x_1(p3d::Math::Vec4* self, float arg0) {
  self->x = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_get_y_0(p3d::Math::Vec4* self) {
  return self->y;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_set_y_1(p3d::Math::Vec4* self, float arg0) {
  self->y = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_get_z_0(p3d::Math::Vec4* self) {
  return self->z;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_set_z_1(p3d::Math::Vec4* self, float arg0) {
  self->z = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_get_w_0(p3d::Math::Vec4* self) {
  return self->w;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4_set_w_1(p3d::Math::Vec4* self, float arg0) {
  self->w = arg0;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec4___destroy___0(p3d::Math::Vec4* self) {
  delete self;
}

// Vec2

p3d::Math::Vec2* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_Vec2_0() {
  return new p3d::Math::Vec2();
}

p3d::Math::Vec2* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_Vec2_2(float arg0, float arg1) {
  return new p3d::Math::Vec2(arg0, arg1);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_dotProduct_1(p3d::Math::Vec2* self, p3d::Math::Vec2* arg0) {
  return self->dotProduct(*arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_magnitude_0(p3d::Math::Vec2* self) {
  return self->magnitude();
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_magnitudeSQR_0(p3d::Math::Vec2* self) {
  return self->magnitudeSQR();
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_distance_1(p3d::Math::Vec2* self, p3d::Math::Vec2* arg0) {
  return self->distance(*arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_distanceSQR_1(p3d::Math::Vec2* self, p3d::Math::Vec2* arg0) {
  return self->distanceSQR(*arg0);
}

p3d::Math::Vec2* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_normalize_0(p3d::Math::Vec2* self) {
  static p3d::Math::Vec2 temp;
  return (temp = self->normalize(), &temp);
}

p3d::Math::Vec2* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_negate_0(p3d::Math::Vec2* self) {
  static p3d::Math::Vec2 temp;
  return (temp = self->negate(), &temp);
}

p3d::Math::Vec2* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_Abs_0(p3d::Math::Vec2* self) {
  static p3d::Math::Vec2 temp;
  return (temp = self->Abs(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_add_1(p3d::Math::Vec2* self, p3d::Math::Vec2* arg0) {
  (*self += *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_sub_1(p3d::Math::Vec2* self, p3d::Math::Vec2* arg0) {
  (*self -= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_mul_1(p3d::Math::Vec2* self, p3d::Math::Vec2* arg0) {
  (*self *= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_div_1(p3d::Math::Vec2* self, p3d::Math::Vec2* arg0) {
  (*self /= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_add_scalar_1(p3d::Math::Vec2* self, float arg0) {
  (*self += arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_sub_scalar_1(p3d::Math::Vec2* self, float arg0) {
  (*self -= arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_mul_scalar_1(p3d::Math::Vec2* self, float arg0) {
  (*self *= arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_div_scalar_1(p3d::Math::Vec2* self, float arg0) {
  (*self /= arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_get_x_0(p3d::Math::Vec2* self) {
  return self->x;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_set_x_1(p3d::Math::Vec2* self, float arg0) {
  self->x = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_get_y_0(p3d::Math::Vec2* self) {
  return self->y;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2_set_y_1(p3d::Math::Vec2* self, float arg0) {
  self->y = arg0;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec2___destroy___0(p3d::Math::Vec2* self) {
  delete self;
}

// Vec3

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_Vec3_0() {
  return new p3d::Math::Vec3();
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_Vec3_3(float arg0, float arg1, float arg2) {
  return new p3d::Math::Vec3(arg0, arg1, arg2);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_dotProduct_1(p3d::Math::Vec3* self, p3d::Math::Vec3* arg0) {
  return self->dotProduct(*arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_magnitude_0(p3d::Math::Vec3* self) {
  return self->magnitude();
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_magnitudeSQR_0(p3d::Math::Vec3* self) {
  return self->magnitudeSQR();
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_distance_1(p3d::Math::Vec3* self, p3d::Math::Vec3* arg0) {
  return self->distance(*arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_distanceSQR_1(p3d::Math::Vec3* self, p3d::Math::Vec3* arg0) {
  return self->distanceSQR(*arg0);
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_cross_1(p3d::Math::Vec3* self, p3d::Math::Vec3* arg0) {
  static p3d::Math::Vec3 temp;
  return (temp = self->cross(*arg0), &temp);
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_normalize_0(p3d::Math::Vec3* self) {
  static p3d::Math::Vec3 temp;
  return (temp = self->normalize(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_normalizeSelf_0(p3d::Math::Vec3* self) {
  self->normalizeSelf();
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_negate_0(p3d::Math::Vec3* self) {
  static p3d::Math::Vec3 temp;
  return (temp = self->negate(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_negateSelf_0(p3d::Math::Vec3* self) {
  self->negateSelf();
}

p3d::Math::Vec3* EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_Abs_0(p3d::Math::Vec3* self) {
  static p3d::Math::Vec3 temp;
  return (temp = self->Abs(), &temp);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_add_1(p3d::Math::Vec3* self, p3d::Math::Vec3* arg0) {
  (*self += *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_sub_1(p3d::Math::Vec3* self, p3d::Math::Vec3* arg0) {
  (*self -= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_mul_1(p3d::Math::Vec3* self, p3d::Math::Vec3* arg0) {
  (*self *= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_div_1(p3d::Math::Vec3* self, p3d::Math::Vec3* arg0) {
  (*self /= *arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_add_scalar_1(p3d::Math::Vec3* self, float arg0) {
  (*self += arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_sub_scalar_1(p3d::Math::Vec3* self, float arg0) {
  (*self -= arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_mul_scalar_1(p3d::Math::Vec3* self, float arg0) {
  (*self *= arg0);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_div_scalar_1(p3d::Math::Vec3* self, float arg0) {
  (*self /= arg0);
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_get_x_0(p3d::Math::Vec3* self) {
  return self->x;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_set_x_1(p3d::Math::Vec3* self, float arg0) {
  self->x = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_get_y_0(p3d::Math::Vec3* self) {
  return self->y;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_set_y_1(p3d::Math::Vec3* self, float arg0) {
  self->y = arg0;
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_get_z_0(p3d::Math::Vec3* self) {
  return self->z;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3_set_z_1(p3d::Math::Vec3* self, float arg0) {
  self->z = arg0;
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Vec3___destroy___0(p3d::Math::Vec3* self) {
  delete self;
}

// Text

p3d::Text* EMSCRIPTEN_KEEPALIVE emscripten_bind_Text_Text_6(p3d::Font* arg0, char* arg1, float arg2, float arg3, p3d::Math::Vec4* arg4, bool arg5) {
  return new p3d::Text(arg0, arg1, arg2, arg3, *arg4, arg5);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Text_UpdateText_2(p3d::Text* self, char* arg0, p3d::Math::Vec4* arg1) {
  self->UpdateText(arg0, *arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Text___destroy___0(p3d::Text* self) {
  delete self;
}

// Font

p3d::Font* EMSCRIPTEN_KEEPALIVE emscripten_bind_Font_Font_2(char* arg0, float arg1) {
  return new p3d::Font(arg0, arg1);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Font_CreateText_1(p3d::Font* self, char* arg0) {
  self->CreateText(arg0);
}

p3d::Texture* EMSCRIPTEN_KEEPALIVE emscripten_bind_Font_GetTexture_0(p3d::Font* self) {
  return self->GetTexture();
}

float EMSCRIPTEN_KEEPALIVE emscripten_bind_Font_GetFontSize_0(p3d::Font* self) {
  return self->GetFontSize();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_Font___destroy___0(p3d::Font* self) {
  delete self;
}

// SpotLight

p3d::SpotLight* EMSCRIPTEN_KEEPALIVE emscripten_bind_SpotLight_SpotLight_0() {
  return new p3d::SpotLight();
}

p3d::SpotLight* EMSCRIPTEN_KEEPALIVE emscripten_bind_SpotLight_SpotLight_5(p3d::Math::Vec4* arg0, float arg1, p3d::Math::Vec3* arg2, float arg3, float arg4) {
  return new p3d::SpotLight(*arg0, arg1, *arg2, arg3, arg4);
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SpotLight_Start_0(p3d::SpotLight* self) {
  self->Start();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SpotLight_Update_0(p3d::SpotLight* self) {
  self->Update();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SpotLight_Destroy_0(p3d::SpotLight* self) {
  self->Destroy();
}

void EMSCRIPTEN_KEEPALIVE emscripten_bind_SpotLight___destroy___0(p3d::SpotLight* self) {
  delete self;
}

}

