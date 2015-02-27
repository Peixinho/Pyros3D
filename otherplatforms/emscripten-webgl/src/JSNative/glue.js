
// Bindings utilities

function WrapperObject() {
}
WrapperObject.prototype = Object.create(WrapperObject.prototype);
WrapperObject.prototype.constructor = WrapperObject;
WrapperObject.prototype.__class__ = WrapperObject;
WrapperObject.__cache__ = {};
Module['WrapperObject'] = WrapperObject;

function getCache(__class__) {
  return (__class__ || WrapperObject).__cache__;
}
Module['getCache'] = getCache;

function wrapPointer(ptr, __class__) {
  var cache = getCache(__class__);
  var ret = cache[ptr];
  if (ret) return ret;
  ret = Object.create((__class__ || WrapperObject).prototype);
  ret.ptr = ptr;
  return cache[ptr] = ret;
}
Module['wrapPointer'] = wrapPointer;

function castObject(obj, __class__) {
  return wrapPointer(obj.ptr, __class__);
}
Module['castObject'] = castObject;

Module['NULL'] = wrapPointer(0);

function destroy(obj) {
  if (!obj['__destroy__']) throw 'Error: Cannot destroy object. (Did you create it yourself?)';
  obj['__destroy__']();
  // Remove from cache, so the object can be GC'd and refs added onto it released
  delete getCache(obj.__class__)[obj.ptr];
}
Module['destroy'] = destroy;

function compare(obj1, obj2) {
  return obj1.ptr === obj2.ptr;
}
Module['compare'] = compare;

function getPointer(obj) {
  return obj.ptr;
}
Module['getPointer'] = getPointer;

function getClass(obj) {
  return obj.__class__;
}
Module['getClass'] = getClass;

// Converts a value into a C-style string.
var ensureString = (function() {
  var stringCache = {};
  function ensureString(value) {
    if (typeof value == 'string') {
      var cachedVal = stringCache[value];
      if (cachedVal) return cachedVal;
      var ret = allocate(intArrayFromString(value), 'i8', ALLOC_STACK);
      stringCache[value] = ret;
      return ret;
    }
    return value;
  }
  return ensureString;
})();


// Renderable
function Renderable() { throw "cannot construct a Renderable, no constructor in IDL" }
Renderable.prototype = Object.create(WrapperObject.prototype);
Renderable.prototype.constructor = Renderable;
Renderable.prototype.__class__ = Renderable;
Renderable.__cache__ = {};
Module['Renderable'] = Renderable;

  Renderable.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Renderable___destroy___0(self);
}
// Matrix
function Matrix(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  if (arg6 && typeof arg6 === 'object') arg6 = arg6.ptr;
  else arg6 = ensureString(arg6);
  if (arg7 && typeof arg7 === 'object') arg7 = arg7.ptr;
  else arg7 = ensureString(arg7);
  if (arg8 && typeof arg8 === 'object') arg8 = arg8.ptr;
  else arg8 = ensureString(arg8);
  if (arg9 && typeof arg9 === 'object') arg9 = arg9.ptr;
  else arg9 = ensureString(arg9);
  if (arg10 && typeof arg10 === 'object') arg10 = arg10.ptr;
  else arg10 = ensureString(arg10);
  if (arg11 && typeof arg11 === 'object') arg11 = arg11.ptr;
  else arg11 = ensureString(arg11);
  if (arg12 && typeof arg12 === 'object') arg12 = arg12.ptr;
  else arg12 = ensureString(arg12);
  if (arg13 && typeof arg13 === 'object') arg13 = arg13.ptr;
  else arg13 = ensureString(arg13);
  if (arg14 && typeof arg14 === 'object') arg14 = arg14.ptr;
  else arg14 = ensureString(arg14);
  if (arg15 && typeof arg15 === 'object') arg15 = arg15.ptr;
  else arg15 = ensureString(arg15);
  if (arg0 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_0(); getCache(Matrix)[this.ptr] = this;return }
  if (arg1 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_1(arg0); getCache(Matrix)[this.ptr] = this;return }
  if (arg2 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_2(arg0, arg1); getCache(Matrix)[this.ptr] = this;return }
  if (arg3 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_3(arg0, arg1, arg2); getCache(Matrix)[this.ptr] = this;return }
  if (arg4 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_4(arg0, arg1, arg2, arg3); getCache(Matrix)[this.ptr] = this;return }
  if (arg5 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_5(arg0, arg1, arg2, arg3, arg4); getCache(Matrix)[this.ptr] = this;return }
  if (arg6 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_6(arg0, arg1, arg2, arg3, arg4, arg5); getCache(Matrix)[this.ptr] = this;return }
  if (arg7 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_7(arg0, arg1, arg2, arg3, arg4, arg5, arg6); getCache(Matrix)[this.ptr] = this;return }
  if (arg8 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_8(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7); getCache(Matrix)[this.ptr] = this;return }
  if (arg9 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_9(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); getCache(Matrix)[this.ptr] = this;return }
  if (arg10 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_10(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); getCache(Matrix)[this.ptr] = this;return }
  if (arg11 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_11(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); getCache(Matrix)[this.ptr] = this;return }
  if (arg12 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_12(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11); getCache(Matrix)[this.ptr] = this;return }
  if (arg13 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_13(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12); getCache(Matrix)[this.ptr] = this;return }
  if (arg14 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_14(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13); getCache(Matrix)[this.ptr] = this;return }
  if (arg15 === undefined) { this.ptr = _emscripten_bind_Matrix_Matrix_15(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14); getCache(Matrix)[this.ptr] = this;return }
  this.ptr = _emscripten_bind_Matrix_Matrix_16(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15);
  getCache(Matrix)[this.ptr] = this;
};
Matrix.prototype = Object.create(WrapperObject.prototype);
Matrix.prototype.constructor = Matrix;
Matrix.prototype.__class__ = Matrix;
Matrix.__cache__ = {};
Module['Matrix'] = Matrix;

Matrix.prototype['identity'] = function() {
  var self = this.ptr;
  _emscripten_bind_Matrix_identity_0(self);
};

Matrix.prototype['LookAt'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg2 === undefined) { _emscripten_bind_Matrix_LookAt_2(self, arg0, arg1);  return }
  _emscripten_bind_Matrix_LookAt_3(self, arg0, arg1, arg2);
};

Matrix.prototype['Translate'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg1 === undefined) { _emscripten_bind_Matrix_Translate_1(self, arg0);  return }
  if (arg2 === undefined) { _emscripten_bind_Matrix_Translate_2(self, arg0, arg1);  return }
  _emscripten_bind_Matrix_Translate_3(self, arg0, arg1, arg2);
};

Matrix.prototype['TranslateX'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_TranslateX_1(self, arg0);
};

Matrix.prototype['TranslateY'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_TranslateY_1(self, arg0);
};

Matrix.prototype['TranslateZ'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_TranslateZ_1(self, arg0);
};

Matrix.prototype['GetTranslation'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Matrix_GetTranslation_0(self), Vec3);
};

Matrix.prototype['RotationX'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_RotationX_1(self, arg0);
};

Matrix.prototype['RotationY'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_RotationY_1(self, arg0);
};

Matrix.prototype['RotationZ'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_RotationZ_1(self, arg0);
};

Matrix.prototype['SetRotationFromEuler'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_Matrix_SetRotationFromEuler_2(self, arg0, arg1);
};

Matrix.prototype['GetEulerFromRotationMatrix'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return wrapPointer(_emscripten_bind_Matrix_GetEulerFromRotationMatrix_1(self, arg0), Vec3);
};

Matrix.prototype['GetRotation'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return wrapPointer(_emscripten_bind_Matrix_GetRotation_1(self, arg0), Matrix);
};

Matrix.prototype['Scale'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg1 === undefined) { _emscripten_bind_Matrix_Scale_1(self, arg0);  return }
  if (arg2 === undefined) { _emscripten_bind_Matrix_Scale_2(self, arg0, arg1);  return }
  _emscripten_bind_Matrix_Scale_3(self, arg0, arg1, arg2);
};

Matrix.prototype['ScaleX'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_ScaleX_1(self, arg0);
};

Matrix.prototype['ScaleY'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_ScaleY_1(self, arg0);
};

Matrix.prototype['ScaleZ'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_ScaleZ_1(self, arg0);
};

Matrix.prototype['GetScale'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Matrix_GetScale_0(self), Vec3);
};

Matrix.prototype['Determinant'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Matrix_Determinant_0(self);
};

Matrix.prototype['Transpose'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Matrix_Transpose_0(self), Matrix);
};

Matrix.prototype['Inverse'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Matrix_Inverse_0(self), Matrix);
};

Matrix.prototype['PerspectiveMatrix'] = function(arg0, arg1, arg2, arg3) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  return wrapPointer(_emscripten_bind_Matrix_PerspectiveMatrix_4(self, arg0, arg1, arg2, arg3), Matrix);
};

Matrix.prototype['OrthoMatrix'] = function(arg0, arg1, arg2, arg3, arg4, arg5) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  return wrapPointer(_emscripten_bind_Matrix_OrthoMatrix_6(self, arg0, arg1, arg2, arg3, arg4, arg5), Matrix);
};

Matrix.prototype['ConvertToQuaternion'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Matrix_ConvertToQuaternion_0(self), Quaternion);
};

Matrix.prototype['mul'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Matrix_mul_1(self, arg0);
};

  Matrix.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Matrix___destroy___0(self);
}
// Texture
function Texture() {
  this.ptr = _emscripten_bind_Texture_Texture_0();
  getCache(Texture)[this.ptr] = this;
};
Texture.prototype = Object.create(WrapperObject.prototype);
Texture.prototype.constructor = Texture;
Texture.prototype.__class__ = Texture;
Texture.__cache__ = {};
Module['Texture'] = Texture;

Texture.prototype['LoadTexture'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  return _emscripten_bind_Texture_LoadTexture_3(self, arg0, arg1, arg2);
};

Texture.prototype['CreateTexture'] = function(arg0, arg1, arg2, arg3, arg4) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg1 === undefined) { return _emscripten_bind_Texture_CreateTexture_1(self, arg0) }
  if (arg2 === undefined) { return _emscripten_bind_Texture_CreateTexture_2(self, arg0, arg1) }
  if (arg3 === undefined) { return _emscripten_bind_Texture_CreateTexture_3(self, arg0, arg1, arg2) }
  if (arg4 === undefined) { return _emscripten_bind_Texture_CreateTexture_4(self, arg0, arg1, arg2, arg3) }
  return _emscripten_bind_Texture_CreateTexture_5(self, arg0, arg1, arg2, arg3, arg4);
};

Texture.prototype['SetMinMagFilter'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_Texture_SetMinMagFilter_2(self, arg0, arg1);
};

Texture.prototype['SetRepeat'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  _emscripten_bind_Texture_SetRepeat_3(self, arg0, arg1, arg2);
};

Texture.prototype['EnableCompareMode'] = function() {
  var self = this.ptr;
  _emscripten_bind_Texture_EnableCompareMode_0(self);
};

Texture.prototype['SetAnysotropy'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Texture_SetAnysotropy_1(self, arg0);
};

Texture.prototype['SetTransparency'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Texture_SetTransparency_1(self, arg0);
};

Texture.prototype['Resize'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_Texture_Resize_2(self, arg0, arg1);
};

Texture.prototype['UpdateData'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Texture_UpdateData_1(self, arg0);
};

Texture.prototype['UpdateMipmap'] = function() {
  var self = this.ptr;
  _emscripten_bind_Texture_UpdateMipmap_0(self);
};

Texture.prototype['SetTextureByteAlignment'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Texture_SetTextureByteAlignment_1(self, arg0);
};

Texture.prototype['GetBindID'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Texture_GetBindID_0(self);
};

Texture.prototype['GetWidth'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Texture_GetWidth_0(self);
};

Texture.prototype['GetHeight'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Texture_GetHeight_0(self);
};

Texture.prototype['Bind'] = function() {
  var self = this.ptr;
  _emscripten_bind_Texture_Bind_0(self);
};

Texture.prototype['Unbind'] = function() {
  var self = this.ptr;
  _emscripten_bind_Texture_Unbind_0(self);
};

Texture.prototype['DeleteTexture'] = function() {
  var self = this.ptr;
  _emscripten_bind_Texture_DeleteTexture_0(self);
};

Texture.prototype['GetLastBindedUnit'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Texture_GetLastBindedUnit_0(self);
};

  Texture.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Texture___destroy___0(self);
}
// Uniform
function Uniform() {
  this.ptr = _emscripten_bind_Uniform_Uniform_0();
  getCache(Uniform)[this.ptr] = this;
};
Uniform.prototype = Object.create(WrapperObject.prototype);
Uniform.prototype.constructor = Uniform;
Uniform.prototype.__class__ = Uniform;
Uniform.__cache__ = {};
Module['Uniform'] = Uniform;

  Uniform.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Uniform___destroy___0(self);
}
// IRenderer
function IRenderer() { throw "cannot construct a IRenderer, no constructor in IDL" }
IRenderer.prototype = Object.create(WrapperObject.prototype);
IRenderer.prototype.constructor = IRenderer;
IRenderer.prototype.__class__ = IRenderer;
IRenderer.__cache__ = {};
Module['IRenderer'] = IRenderer;

IRenderer.prototype['ClearBufferBit'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IRenderer_ClearBufferBit_1(self, arg0);
};

IRenderer.prototype['EnableDepthTest'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableDepthTest_0(self);
};

IRenderer.prototype['DisableDepthTest'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableDepthTest_0(self);
};

IRenderer.prototype['EnableDepthWritting'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableDepthWritting_0(self);
};

IRenderer.prototype['DisableDepthWritting'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableDepthWritting_0(self);
};

IRenderer.prototype['EnableClearDepthBuffer'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableClearDepthBuffer_0(self);
};

IRenderer.prototype['DisableClearDepthBuffer'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableClearDepthBuffer_0(self);
};

IRenderer.prototype['ClearDepthBuffer'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_ClearDepthBuffer_0(self);
};

IRenderer.prototype['EnableStencil'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableStencil_0(self);
};

IRenderer.prototype['DisableStencil'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableStencil_0(self);
};

IRenderer.prototype['ClearStencilBuffer'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_ClearStencilBuffer_0(self);
};

IRenderer.prototype['StencilFunction'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  _emscripten_bind_IRenderer_StencilFunction_3(self, arg0, arg1, arg2);
};

IRenderer.prototype['StencilOperation'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  _emscripten_bind_IRenderer_StencilOperation_3(self, arg0, arg1, arg2);
};

IRenderer.prototype['EnableBlending'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableBlending_0(self);
};

IRenderer.prototype['DisableBlending'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableBlending_0(self);
};

IRenderer.prototype['BlendingFunction'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_IRenderer_BlendingFunction_2(self, arg0, arg1);
};

IRenderer.prototype['BlendingEquation'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IRenderer_BlendingEquation_1(self, arg0);
};

IRenderer.prototype['EnableScissorTest'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableScissorTest_0(self);
};

IRenderer.prototype['DisableScissorTest'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableScissorTest_0(self);
};

IRenderer.prototype['ScissorTestRect'] = function(arg0, arg1, arg2, arg3) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  _emscripten_bind_IRenderer_ScissorTestRect_4(self, arg0, arg1, arg2, arg3);
};

IRenderer.prototype['EnableWireFrame'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableWireFrame_0(self);
};

IRenderer.prototype['DisableWireFrame'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableWireFrame_0(self);
};

IRenderer.prototype['ColorMask'] = function(arg0, arg1, arg2, arg3) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  _emscripten_bind_IRenderer_ColorMask_4(self, arg0, arg1, arg2, arg3);
};

IRenderer.prototype['EnableSorting'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableSorting_0(self);
};

IRenderer.prototype['DisableSorting'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableSorting_0(self);
};

IRenderer.prototype['EnableLOD'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_EnableLOD_0(self);
};

IRenderer.prototype['DisableLOD'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableLOD_0(self);
};

IRenderer.prototype['IsUsingLOD'] = function() {
  var self = this.ptr;
  return _emscripten_bind_IRenderer_IsUsingLOD_0(self);
};

IRenderer.prototype['SetBackground'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IRenderer_SetBackground_1(self, arg0);
};

IRenderer.prototype['UnsetBackground'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_UnsetBackground_0(self);
};

IRenderer.prototype['SetGlobalLight'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IRenderer_SetGlobalLight_1(self, arg0);
};

IRenderer.prototype['EnableDepthBias'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IRenderer_EnableDepthBias_1(self, arg0);
};

IRenderer.prototype['DisableDepthBias'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DisableDepthBias_0(self);
};

IRenderer.prototype['SetViewPort'] = function(arg0, arg1, arg2, arg3) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  _emscripten_bind_IRenderer_SetViewPort_4(self, arg0, arg1, arg2, arg3);
};

IRenderer.prototype['Resize'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_IRenderer_Resize_2(self, arg0, arg1);
};

IRenderer.prototype['ActivateCulling'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IRenderer_ActivateCulling_1(self, arg0);
};

IRenderer.prototype['DeactivateCulling'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer_DeactivateCulling_0(self);
};

IRenderer.prototype['RenderScene'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  _emscripten_bind_IRenderer_RenderScene_3(self, arg0, arg1, arg2);
};

  IRenderer.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_IRenderer___destroy___0(self);
}
// IMaterial
function IMaterial() {
  this.ptr = _emscripten_bind_IMaterial_IMaterial_0();
  getCache(IMaterial)[this.ptr] = this;
};
IMaterial.prototype = Object.create(WrapperObject.prototype);
IMaterial.prototype.constructor = IMaterial;
IMaterial.prototype.__class__ = IMaterial;
IMaterial.__cache__ = {};
Module['IMaterial'] = IMaterial;

IMaterial.prototype['PreRender'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial_PreRender_0(self);
};

IMaterial.prototype['Render'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial_Render_0(self);
};

IMaterial.prototype['AfterRender'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial_AfterRender_0(self);
};

IMaterial.prototype['SetCullFace'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IMaterial_SetCullFace_1(self, arg0);
};

IMaterial.prototype['GetCullFace'] = function() {
  var self = this.ptr;
  return _emscripten_bind_IMaterial_GetCullFace_0(self);
};

IMaterial.prototype['IsWireFrame'] = function() {
  var self = this.ptr;
  return _emscripten_bind_IMaterial_IsWireFrame_0(self);
};

IMaterial.prototype['GetOpacity'] = function() {
  var self = this.ptr;
  return _emscripten_bind_IMaterial_GetOpacity_0(self);
};

IMaterial.prototype['IsTransparent'] = function() {
  var self = this.ptr;
  return _emscripten_bind_IMaterial_IsTransparent_0(self);
};

IMaterial.prototype['SetOpacity'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IMaterial_SetOpacity_1(self, arg0);
};

IMaterial.prototype['SetTransparencyFlag'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IMaterial_SetTransparencyFlag_1(self, arg0);
};

IMaterial.prototype['SetUniformValue'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg2 === undefined) { _emscripten_bind_IMaterial_SetUniformValue_2(self, arg0, arg1);  return }
  _emscripten_bind_IMaterial_SetUniformValue_3(self, arg0, arg1, arg2);
};

IMaterial.prototype['AddUniform'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_IMaterial_AddUniform_1(self, arg0);
};

IMaterial.prototype['StartRenderWireFrame'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial_StartRenderWireFrame_0(self);
};

IMaterial.prototype['StopRenderWireFrame'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial_StopRenderWireFrame_0(self);
};

IMaterial.prototype['EnableCastingShadows'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial_EnableCastingShadows_0(self);
};

IMaterial.prototype['DisableCastingShadows'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial_DisableCastingShadows_0(self);
};

IMaterial.prototype['IsCastingShadows'] = function() {
  var self = this.ptr;
  return _emscripten_bind_IMaterial_IsCastingShadows_0(self);
};

IMaterial.prototype['Destroy'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial_Destroy_0(self);
};

IMaterial.prototype['GetShader'] = function() {
  var self = this.ptr;
  return _emscripten_bind_IMaterial_GetShader_0(self);
};

IMaterial.prototype['GetInternalID'] = function() {
  var self = this.ptr;
  return _emscripten_bind_IMaterial_GetInternalID_0(self);
};

  IMaterial.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_IMaterial___destroy___0(self);
}
// PyrosJSNative
function PyrosJSNative() {
  this.ptr = _emscripten_bind_PyrosJSNative_PyrosJSNative_0();
  getCache(PyrosJSNative)[this.ptr] = this;
};
PyrosJSNative.prototype = Object.create(WrapperObject.prototype);
PyrosJSNative.prototype.constructor = PyrosJSNative;
PyrosJSNative.prototype.__class__ = PyrosJSNative;
PyrosJSNative.__cache__ = {};
Module['PyrosJSNative'] = PyrosJSNative;

PyrosJSNative.prototype['CreateContext'] = function() {
  var self = this.ptr;
  _emscripten_bind_PyrosJSNative_CreateContext_0(self);
};

  PyrosJSNative.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_PyrosJSNative___destroy___0(self);
}
// VoidPtr
function VoidPtr() { throw "cannot construct a VoidPtr, no constructor in IDL" }
VoidPtr.prototype = Object.create(WrapperObject.prototype);
VoidPtr.prototype.constructor = VoidPtr;
VoidPtr.prototype.__class__ = VoidPtr;
VoidPtr.__cache__ = {};
Module['VoidPtr'] = VoidPtr;

  VoidPtr.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_VoidPtr___destroy___0(self);
}
// SkeletonAnimation
function SkeletonAnimation() {
  this.ptr = _emscripten_bind_SkeletonAnimation_SkeletonAnimation_0();
  getCache(SkeletonAnimation)[this.ptr] = this;
};
SkeletonAnimation.prototype = Object.create(WrapperObject.prototype);
SkeletonAnimation.prototype.constructor = SkeletonAnimation;
SkeletonAnimation.prototype.__class__ = SkeletonAnimation;
SkeletonAnimation.__cache__ = {};
Module['SkeletonAnimation'] = SkeletonAnimation;

SkeletonAnimation.prototype['LoadAnimation'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SkeletonAnimation_LoadAnimation_1(self, arg0);
};

SkeletonAnimation.prototype['GetNumberAnimations'] = function() {
  var self = this.ptr;
  return _emscripten_bind_SkeletonAnimation_GetNumberAnimations_0(self);
};

SkeletonAnimation.prototype['Update'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SkeletonAnimation_Update_1(self, arg0);
};

SkeletonAnimation.prototype['CreateInstance'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return wrapPointer(_emscripten_bind_SkeletonAnimation_CreateInstance_1(self, arg0), SkeletonAnimationInstance);
};

SkeletonAnimation.prototype['DestroyInstance'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SkeletonAnimation_DestroyInstance_1(self, arg0);
};

SkeletonAnimation.prototype['GetAnimationIDByName'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimation_GetAnimationIDByName_1(self, arg0);
};

  SkeletonAnimation.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_SkeletonAnimation___destroy___0(self);
}
// Capsule
function Capsule(arg0, arg1, arg2, arg3, arg4, arg5, arg6) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  if (arg6 && typeof arg6 === 'object') arg6 = arg6.ptr;
  else arg6 = ensureString(arg6);
  this.ptr = _emscripten_bind_Capsule_Capsule_7(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
  getCache(Capsule)[this.ptr] = this;
};
Capsule.prototype = Object.create(WrapperObject.prototype);
Capsule.prototype.constructor = Capsule;
Capsule.prototype.__class__ = Capsule;
Capsule.__cache__ = {};
Module['Capsule'] = Capsule;

  Capsule.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Capsule___destroy___0(self);
}
// Quaternion
function Quaternion(arg0, arg1, arg2, arg3) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg0 === undefined) { this.ptr = _emscripten_bind_Quaternion_Quaternion_0(); getCache(Quaternion)[this.ptr] = this;return }
  if (arg1 === undefined) { this.ptr = _emscripten_bind_Quaternion_Quaternion_1(arg0); getCache(Quaternion)[this.ptr] = this;return }
  if (arg2 === undefined) { this.ptr = _emscripten_bind_Quaternion_Quaternion_2(arg0, arg1); getCache(Quaternion)[this.ptr] = this;return }
  if (arg3 === undefined) { this.ptr = _emscripten_bind_Quaternion_Quaternion_3(arg0, arg1, arg2); getCache(Quaternion)[this.ptr] = this;return }
  this.ptr = _emscripten_bind_Quaternion_Quaternion_4(arg0, arg1, arg2, arg3);
  getCache(Quaternion)[this.ptr] = this;
};
Quaternion.prototype = Object.create(WrapperObject.prototype);
Quaternion.prototype.constructor = Quaternion;
Quaternion.prototype.__class__ = Quaternion;
Quaternion.__cache__ = {};
Module['Quaternion'] = Quaternion;

Quaternion.prototype['ConvertToMatrix'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Quaternion_ConvertToMatrix_0(self), Matrix);
};

Quaternion.prototype['Magnitude'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Quaternion_Magnitude_0(self);
};

Quaternion.prototype['Dot'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_Quaternion_Dot_1(self, arg0);
};

Quaternion.prototype['Normalize'] = function() {
  var self = this.ptr;
  _emscripten_bind_Quaternion_Normalize_0(self);
};

Quaternion.prototype['Rotation'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  _emscripten_bind_Quaternion_Rotation_3(self, arg0, arg1, arg2);
};

Quaternion.prototype['SetRotationFromEuler'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_Quaternion_SetRotationFromEuler_2(self, arg0, arg1);
};

Quaternion.prototype['GetEulerFromQuaternion'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return wrapPointer(_emscripten_bind_Quaternion_GetEulerFromQuaternion_1(self, arg0), Vec3);
};

Quaternion.prototype['AxisToQuaternion'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_Quaternion_AxisToQuaternion_2(self, arg0, arg1);
};

Quaternion.prototype['Slerp'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  return wrapPointer(_emscripten_bind_Quaternion_Slerp_2(self, arg0, arg1), Quaternion);
};

Quaternion.prototype['Nlerp'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  return wrapPointer(_emscripten_bind_Quaternion_Nlerp_3(self, arg0, arg1, arg2), Quaternion);
};

Quaternion.prototype['Inverse'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Quaternion_Inverse_0(self), Quaternion);
};

Quaternion.prototype['mul'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Quaternion_mul_1(self, arg0);
};

  Quaternion.prototype['get_x']= function() {
  var self = this.ptr;
  return _emscripten_bind_Quaternion_get_x_0(self);
}
    Quaternion.prototype['set_x']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Quaternion_set_x_1(self, arg0);
}
  Quaternion.prototype['get_y']= function() {
  var self = this.ptr;
  return _emscripten_bind_Quaternion_get_y_0(self);
}
    Quaternion.prototype['set_y']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Quaternion_set_y_1(self, arg0);
}
  Quaternion.prototype['get_z']= function() {
  var self = this.ptr;
  return _emscripten_bind_Quaternion_get_z_0(self);
}
    Quaternion.prototype['set_z']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Quaternion_set_z_1(self, arg0);
}
  Quaternion.prototype['get_w']= function() {
  var self = this.ptr;
  return _emscripten_bind_Quaternion_get_w_0(self);
}
    Quaternion.prototype['set_w']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Quaternion_set_w_1(self, arg0);
}
  Quaternion.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Quaternion___destroy___0(self);
}
// Plane
function Plane(arg0, arg1, arg2, arg3) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  this.ptr = _emscripten_bind_Plane_Plane_4(arg0, arg1, arg2, arg3);
  getCache(Plane)[this.ptr] = this;
};
Plane.prototype = Object.create(WrapperObject.prototype);
Plane.prototype.constructor = Plane;
Plane.prototype.__class__ = Plane;
Plane.__cache__ = {};
Module['Plane'] = Plane;

  Plane.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Plane___destroy___0(self);
}
// TextureAnimationInstance
function TextureAnimationInstance(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  this.ptr = _emscripten_bind_TextureAnimationInstance_TextureAnimationInstance_2(arg0, arg1);
  getCache(TextureAnimationInstance)[this.ptr] = this;
};
TextureAnimationInstance.prototype = Object.create(WrapperObject.prototype);
TextureAnimationInstance.prototype.constructor = TextureAnimationInstance;
TextureAnimationInstance.prototype.__class__ = TextureAnimationInstance;
TextureAnimationInstance.__cache__ = {};
Module['TextureAnimationInstance'] = TextureAnimationInstance;

TextureAnimationInstance.prototype['Play'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_TextureAnimationInstance_Play_1(self, arg0);
};

TextureAnimationInstance.prototype['Pause'] = function() {
  var self = this.ptr;
  _emscripten_bind_TextureAnimationInstance_Pause_0(self);
};

TextureAnimationInstance.prototype['Stop'] = function() {
  var self = this.ptr;
  _emscripten_bind_TextureAnimationInstance_Stop_0(self);
};

TextureAnimationInstance.prototype['IsPlaying'] = function() {
  var self = this.ptr;
  return _emscripten_bind_TextureAnimationInstance_IsPlaying_0(self);
};

TextureAnimationInstance.prototype['YoYo'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_TextureAnimationInstance_YoYo_1(self, arg0);
};

TextureAnimationInstance.prototype['GetTexture'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_TextureAnimationInstance_GetTexture_0(self), Texture);
};

TextureAnimationInstance.prototype['GetFrame'] = function() {
  var self = this.ptr;
  return _emscripten_bind_TextureAnimationInstance_GetFrame_0(self);
};

  TextureAnimationInstance.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_TextureAnimationInstance___destroy___0(self);
}
// SkeletonAnimationInstance
function SkeletonAnimationInstance(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  this.ptr = _emscripten_bind_SkeletonAnimationInstance_SkeletonAnimationInstance_2(arg0, arg1);
  getCache(SkeletonAnimationInstance)[this.ptr] = this;
};
SkeletonAnimationInstance.prototype = Object.create(WrapperObject.prototype);
SkeletonAnimationInstance.prototype.constructor = SkeletonAnimationInstance;
SkeletonAnimationInstance.prototype.__class__ = SkeletonAnimationInstance;
SkeletonAnimationInstance.__cache__ = {};
Module['SkeletonAnimationInstance'] = SkeletonAnimationInstance;

SkeletonAnimationInstance.prototype['Play'] = function(arg0, arg1, arg2, arg3, arg4, arg5) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  return _emscripten_bind_SkeletonAnimationInstance_Play_6(self, arg0, arg1, arg2, arg3, arg4, arg5);
};

SkeletonAnimationInstance.prototype['ChangeProperties'] = function(arg0, arg1, arg2, arg3, arg4) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  _emscripten_bind_SkeletonAnimationInstance_ChangeProperties_5(self, arg0, arg1, arg2, arg3, arg4);
};

SkeletonAnimationInstance.prototype['Pause'] = function() {
  var self = this.ptr;
  _emscripten_bind_SkeletonAnimationInstance_Pause_0(self);
};

SkeletonAnimationInstance.prototype['PauseAnimation'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SkeletonAnimationInstance_PauseAnimation_1(self, arg0);
};

SkeletonAnimationInstance.prototype['ResumeAnimation'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SkeletonAnimationInstance_ResumeAnimation_1(self, arg0);
};

SkeletonAnimationInstance.prototype['Resume'] = function() {
  var self = this.ptr;
  _emscripten_bind_SkeletonAnimationInstance_Resume_0(self);
};

SkeletonAnimationInstance.prototype['StopAnimation'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SkeletonAnimationInstance_StopAnimation_1(self, arg0);
};

SkeletonAnimationInstance.prototype['Stop'] = function() {
  var self = this.ptr;
  _emscripten_bind_SkeletonAnimationInstance_Stop_0(self);
};

SkeletonAnimationInstance.prototype['IsPaused'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg0 === undefined) { return _emscripten_bind_SkeletonAnimationInstance_IsPaused_0(self) }
  return _emscripten_bind_SkeletonAnimationInstance_IsPaused_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationCurrentProgress'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationCurrentProgress_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationDuration'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationDuration_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationCurrentTime'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationCurrentTime_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationSpeed'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationSpeed_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationStartTime'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationStartTime_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationStartTimeProgress'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationStartTimeProgress_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationID'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationID_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationScale'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationScale_1(self, arg0);
};

SkeletonAnimationInstance.prototype['GetAnimationPositionInVector'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_GetAnimationPositionInVector_1(self, arg0);
};

SkeletonAnimationInstance.prototype['CreateLayer'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_SkeletonAnimationInstance_CreateLayer_1(self, arg0);
};

SkeletonAnimationInstance.prototype['AddBone'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_SkeletonAnimationInstance_AddBone_2(self, arg0, arg1);
};

SkeletonAnimationInstance.prototype['AddBoneAndChilds'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  _emscripten_bind_SkeletonAnimationInstance_AddBoneAndChilds_3(self, arg0, arg1, arg2);
};

SkeletonAnimationInstance.prototype['RemoveBone'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_SkeletonAnimationInstance_RemoveBone_2(self, arg0, arg1);
};

SkeletonAnimationInstance.prototype['RemoveBoneAndChilds'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_SkeletonAnimationInstance_RemoveBoneAndChilds_2(self, arg0, arg1);
};

SkeletonAnimationInstance.prototype['DestroyLayer'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SkeletonAnimationInstance_DestroyLayer_1(self, arg0);
};

  SkeletonAnimationInstance.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_SkeletonAnimationInstance___destroy___0(self);
}
// Cone
function Cone(arg0, arg1, arg2, arg3, arg4, arg5, arg6) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  if (arg6 && typeof arg6 === 'object') arg6 = arg6.ptr;
  else arg6 = ensureString(arg6);
  this.ptr = _emscripten_bind_Cone_Cone_7(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
  getCache(Cone)[this.ptr] = this;
};
Cone.prototype = Object.create(WrapperObject.prototype);
Cone.prototype.constructor = Cone;
Cone.prototype.__class__ = Cone;
Cone.__cache__ = {};
Module['Cone'] = Cone;

  Cone.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Cone___destroy___0(self);
}
// IComponent
function IComponent() { throw "cannot construct a IComponent, no constructor in IDL" }
IComponent.prototype = Object.create(WrapperObject.prototype);
IComponent.prototype.constructor = IComponent;
IComponent.prototype.__class__ = IComponent;
IComponent.__cache__ = {};
Module['IComponent'] = IComponent;

  IComponent.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_IComponent___destroy___0(self);
}
// Torus
function Torus(arg0, arg1, arg2, arg3, arg4, arg5) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  this.ptr = _emscripten_bind_Torus_Torus_6(arg0, arg1, arg2, arg3, arg4, arg5);
  getCache(Torus)[this.ptr] = this;
};
Torus.prototype = Object.create(WrapperObject.prototype);
Torus.prototype.constructor = Torus;
Torus.prototype.__class__ = Torus;
Torus.__cache__ = {};
Module['Torus'] = Torus;

  Torus.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Torus___destroy___0(self);
}
// TorusKnot
function TorusKnot(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  if (arg6 && typeof arg6 === 'object') arg6 = arg6.ptr;
  else arg6 = ensureString(arg6);
  if (arg7 && typeof arg7 === 'object') arg7 = arg7.ptr;
  else arg7 = ensureString(arg7);
  if (arg8 && typeof arg8 === 'object') arg8 = arg8.ptr;
  else arg8 = ensureString(arg8);
  this.ptr = _emscripten_bind_TorusKnot_TorusKnot_9(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
  getCache(TorusKnot)[this.ptr] = this;
};
TorusKnot.prototype = Object.create(WrapperObject.prototype);
TorusKnot.prototype.constructor = TorusKnot;
TorusKnot.prototype.__class__ = TorusKnot;
TorusKnot.__cache__ = {};
Module['TorusKnot'] = TorusKnot;

  TorusKnot.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_TorusKnot___destroy___0(self);
}
// Shader
function Shader() {
  this.ptr = _emscripten_bind_Shader_Shader_0();
  getCache(Shader)[this.ptr] = this;
};
Shader.prototype = Object.create(WrapperObject.prototype);
Shader.prototype.constructor = Shader;
Shader.prototype.__class__ = Shader;
Shader.__cache__ = {};
Module['Shader'] = Shader;

Shader.prototype['LoadShaderFile'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Shader_LoadShaderFile_1(self, arg0);
};

Shader.prototype['LoadShaderText'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Shader_LoadShaderText_1(self, arg0);
};

Shader.prototype['GetType'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Shader_GetType_0(self);
};

Shader.prototype['CompileShader'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Shader_CompileShader_1(self, arg0);
};

Shader.prototype['DeleteShader'] = function() {
  var self = this.ptr;
  _emscripten_bind_Shader_DeleteShader_0(self);
};

Shader.prototype['LinkProgram'] = function() {
  var self = this.ptr;
  _emscripten_bind_Shader_LinkProgram_0(self);
};

Shader.prototype['ShaderProgram'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Shader_ShaderProgram_0(self);
};

Shader.prototype['GetUniformLocation'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  return _emscripten_bind_Shader_GetUniformLocation_2(self, arg0, arg1);
};

Shader.prototype['GetAttributeLocation'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  return _emscripten_bind_Shader_GetAttributeLocation_2(self, arg0, arg1);
};

Shader.prototype['SendUniform'] = function(arg0, arg1, arg2, arg3) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg2 === undefined) { _emscripten_bind_Shader_SendUniform_2(self, arg0, arg1);  return }
  if (arg3 === undefined) { _emscripten_bind_Shader_SendUniform_3(self, arg0, arg1, arg2);  return }
  _emscripten_bind_Shader_SendUniform_4(self, arg0, arg1, arg2, arg3);
};

  Shader.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Shader___destroy___0(self);
}
// CustomShaderMaterial
function CustomShaderMaterial(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  this.ptr = _emscripten_bind_CustomShaderMaterial_CustomShaderMaterial_2(arg0, arg1);
  getCache(CustomShaderMaterial)[this.ptr] = this;
};
CustomShaderMaterial.prototype = Object.create(IMaterial.prototype);
CustomShaderMaterial.prototype.constructor = CustomShaderMaterial;
CustomShaderMaterial.prototype.__class__ = CustomShaderMaterial;
CustomShaderMaterial.__cache__ = {};
Module['CustomShaderMaterial'] = CustomShaderMaterial;

  CustomShaderMaterial.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_CustomShaderMaterial___destroy___0(self);
}
// Cube
function Cube(arg0, arg1, arg2, arg3, arg4) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  this.ptr = _emscripten_bind_Cube_Cube_5(arg0, arg1, arg2, arg3, arg4);
  getCache(Cube)[this.ptr] = this;
};
Cube.prototype = Object.create(WrapperObject.prototype);
Cube.prototype.constructor = Cube;
Cube.prototype.__class__ = Cube;
Cube.__cache__ = {};
Module['Cube'] = Cube;

  Cube.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Cube___destroy___0(self);
}
// PointLight
function PointLight(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg0 === undefined) { this.ptr = _emscripten_bind_PointLight_PointLight_0(); getCache(PointLight)[this.ptr] = this;return }
  if (arg1 === undefined) { this.ptr = _emscripten_bind_PointLight_PointLight_1(arg0); getCache(PointLight)[this.ptr] = this;return }
  this.ptr = _emscripten_bind_PointLight_PointLight_2(arg0, arg1);
  getCache(PointLight)[this.ptr] = this;
};
PointLight.prototype = Object.create(WrapperObject.prototype);
PointLight.prototype.constructor = PointLight;
PointLight.prototype.__class__ = PointLight;
PointLight.__cache__ = {};
Module['PointLight'] = PointLight;

PointLight.prototype['Start'] = function() {
  var self = this.ptr;
  _emscripten_bind_PointLight_Start_0(self);
};

PointLight.prototype['Update'] = function() {
  var self = this.ptr;
  _emscripten_bind_PointLight_Update_0(self);
};

PointLight.prototype['Destroy'] = function() {
  var self = this.ptr;
  _emscripten_bind_PointLight_Destroy_0(self);
};

  PointLight.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_PointLight___destroy___0(self);
}
// GenericShaderMaterial
function GenericShaderMaterial(arg0) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  this.ptr = _emscripten_bind_GenericShaderMaterial_GenericShaderMaterial_1(arg0);
  getCache(GenericShaderMaterial)[this.ptr] = this;
};
GenericShaderMaterial.prototype = Object.create(IMaterial.prototype);
GenericShaderMaterial.prototype.constructor = GenericShaderMaterial;
GenericShaderMaterial.prototype.__class__ = GenericShaderMaterial;
GenericShaderMaterial.__cache__ = {};
Module['GenericShaderMaterial'] = GenericShaderMaterial;

GenericShaderMaterial.prototype['SetColor'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetColor_1(self, arg0);
};

GenericShaderMaterial.prototype['SetSpecular'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetSpecular_1(self, arg0);
};

GenericShaderMaterial.prototype['SetColorMap'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetColorMap_1(self, arg0);
};

GenericShaderMaterial.prototype['SetSpecularMap'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetSpecularMap_1(self, arg0);
};

GenericShaderMaterial.prototype['SetNormalMap'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetNormalMap_1(self, arg0);
};

GenericShaderMaterial.prototype['SetEnvMap'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetEnvMap_1(self, arg0);
};

GenericShaderMaterial.prototype['SetReflectivity'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetReflectivity_1(self, arg0);
};

GenericShaderMaterial.prototype['SetLightingProperties'] = function(arg0, arg1, arg2, arg3, arg4) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  _emscripten_bind_GenericShaderMaterial_SetLightingProperties_5(self, arg0, arg1, arg2, arg3, arg4);
};

GenericShaderMaterial.prototype['SetKe'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetKe_1(self, arg0);
};

GenericShaderMaterial.prototype['SetKa'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetKa_1(self, arg0);
};

GenericShaderMaterial.prototype['SetKd'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetKd_1(self, arg0);
};

GenericShaderMaterial.prototype['SetKs'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetKs_1(self, arg0);
};

GenericShaderMaterial.prototype['SetShininess'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetShininess_1(self, arg0);
};

GenericShaderMaterial.prototype['PreRender'] = function() {
  var self = this.ptr;
  _emscripten_bind_GenericShaderMaterial_PreRender_0(self);
};

GenericShaderMaterial.prototype['AfterRender'] = function() {
  var self = this.ptr;
  _emscripten_bind_GenericShaderMaterial_AfterRender_0(self);
};

GenericShaderMaterial.prototype['BindTextures'] = function() {
  var self = this.ptr;
  _emscripten_bind_GenericShaderMaterial_BindTextures_0(self);
};

GenericShaderMaterial.prototype['UnbindTextures'] = function() {
  var self = this.ptr;
  _emscripten_bind_GenericShaderMaterial_UnbindTextures_0(self);
};

GenericShaderMaterial.prototype['SetPCFTexelSize'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GenericShaderMaterial_SetPCFTexelSize_1(self, arg0);
};

GenericShaderMaterial.prototype['SetPCFTexelCascadesSize'] = function(arg0, arg1, arg2, arg3) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  _emscripten_bind_GenericShaderMaterial_SetPCFTexelCascadesSize_4(self, arg0, arg1, arg2, arg3);
};

  GenericShaderMaterial.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_GenericShaderMaterial___destroy___0(self);
}
// DirectionalLight
function DirectionalLight(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg0 === undefined) { this.ptr = _emscripten_bind_DirectionalLight_DirectionalLight_0(); getCache(DirectionalLight)[this.ptr] = this;return }
  if (arg1 === undefined) { this.ptr = _emscripten_bind_DirectionalLight_DirectionalLight_1(arg0); getCache(DirectionalLight)[this.ptr] = this;return }
  this.ptr = _emscripten_bind_DirectionalLight_DirectionalLight_2(arg0, arg1);
  getCache(DirectionalLight)[this.ptr] = this;
};
DirectionalLight.prototype = Object.create(WrapperObject.prototype);
DirectionalLight.prototype.constructor = DirectionalLight;
DirectionalLight.prototype.__class__ = DirectionalLight;
DirectionalLight.__cache__ = {};
Module['DirectionalLight'] = DirectionalLight;

DirectionalLight.prototype['Start'] = function() {
  var self = this.ptr;
  _emscripten_bind_DirectionalLight_Start_0(self);
};

DirectionalLight.prototype['Update'] = function() {
  var self = this.ptr;
  _emscripten_bind_DirectionalLight_Update_0(self);
};

DirectionalLight.prototype['Destroy'] = function() {
  var self = this.ptr;
  _emscripten_bind_DirectionalLight_Destroy_0(self);
};

  DirectionalLight.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_DirectionalLight___destroy___0(self);
}
// Model
function Model(arg0, arg1, arg2) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  this.ptr = _emscripten_bind_Model_Model_3(arg0, arg1, arg2);
  getCache(Model)[this.ptr] = this;
};
Model.prototype = Object.create(WrapperObject.prototype);
Model.prototype.constructor = Model;
Model.prototype.__class__ = Model;
Model.__cache__ = {};
Module['Model'] = Model;

  Model.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Model___destroy___0(self);
}
// ForwardRenderer
function ForwardRenderer(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  this.ptr = _emscripten_bind_ForwardRenderer_ForwardRenderer_2(arg0, arg1);
  getCache(ForwardRenderer)[this.ptr] = this;
};
ForwardRenderer.prototype = Object.create(IRenderer.prototype);
ForwardRenderer.prototype.constructor = ForwardRenderer;
ForwardRenderer.prototype.__class__ = ForwardRenderer;
ForwardRenderer.__cache__ = {};
Module['ForwardRenderer'] = ForwardRenderer;

  ForwardRenderer.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_ForwardRenderer___destroy___0(self);
}
// SceneGraph
function SceneGraph() {
  this.ptr = _emscripten_bind_SceneGraph_SceneGraph_0();
  getCache(SceneGraph)[this.ptr] = this;
};
SceneGraph.prototype = Object.create(WrapperObject.prototype);
SceneGraph.prototype.constructor = SceneGraph;
SceneGraph.prototype.__class__ = SceneGraph;
SceneGraph.__cache__ = {};
Module['SceneGraph'] = SceneGraph;

SceneGraph.prototype['Update'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SceneGraph_Update_1(self, arg0);
};

SceneGraph.prototype['Add'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SceneGraph_Add_1(self, arg0);
};

SceneGraph.prototype['Remove'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SceneGraph_Remove_1(self, arg0);
};

SceneGraph.prototype['AddGameObject'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SceneGraph_AddGameObject_1(self, arg0);
};

SceneGraph.prototype['RemoveGameObject'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_SceneGraph_RemoveGameObject_1(self, arg0);
};

SceneGraph.prototype['GetTime'] = function() {
  var self = this.ptr;
  return _emscripten_bind_SceneGraph_GetTime_0(self);
};

  SceneGraph.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_SceneGraph___destroy___0(self);
}
// TextureAnimation
function TextureAnimation() {
  this.ptr = _emscripten_bind_TextureAnimation_TextureAnimation_0();
  getCache(TextureAnimation)[this.ptr] = this;
};
TextureAnimation.prototype = Object.create(WrapperObject.prototype);
TextureAnimation.prototype.constructor = TextureAnimation;
TextureAnimation.prototype.__class__ = TextureAnimation;
TextureAnimation.__cache__ = {};
Module['TextureAnimation'] = TextureAnimation;

TextureAnimation.prototype['GetFrame'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return wrapPointer(_emscripten_bind_TextureAnimation_GetFrame_1(self, arg0), Texture);
};

TextureAnimation.prototype['GetNumberFrames'] = function() {
  var self = this.ptr;
  return _emscripten_bind_TextureAnimation_GetNumberFrames_0(self);
};

TextureAnimation.prototype['AddFrame'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_TextureAnimation_AddFrame_1(self, arg0);
};

TextureAnimation.prototype['Update'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_TextureAnimation_Update_1(self, arg0);
};

TextureAnimation.prototype['CreateInstance'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return wrapPointer(_emscripten_bind_TextureAnimation_CreateInstance_1(self, arg0), TextureAnimationInstance);
};

TextureAnimation.prototype['DestroyInstance'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_TextureAnimation_DestroyInstance_1(self, arg0);
};

  TextureAnimation.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_TextureAnimation___destroy___0(self);
}
// Cylinder
function Cylinder(arg0, arg1, arg2, arg3, arg4, arg5, arg6) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  if (arg6 && typeof arg6 === 'object') arg6 = arg6.ptr;
  else arg6 = ensureString(arg6);
  this.ptr = _emscripten_bind_Cylinder_Cylinder_7(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
  getCache(Cylinder)[this.ptr] = this;
};
Cylinder.prototype = Object.create(WrapperObject.prototype);
Cylinder.prototype.constructor = Cylinder;
Cylinder.prototype.__class__ = Cylinder;
Cylinder.__cache__ = {};
Module['Cylinder'] = Cylinder;

  Cylinder.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Cylinder___destroy___0(self);
}
// Projection
function Projection() {
  this.ptr = _emscripten_bind_Projection_Projection_0();
  getCache(Projection)[this.ptr] = this;
};
Projection.prototype = Object.create(WrapperObject.prototype);
Projection.prototype.constructor = Projection;
Projection.prototype.__class__ = Projection;
Projection.__cache__ = {};
Module['Projection'] = Projection;

Projection.prototype['Perspective'] = function(arg0, arg1, arg2, arg3) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  _emscripten_bind_Projection_Perspective_4(self, arg0, arg1, arg2, arg3);
};

Projection.prototype['Ortho'] = function(arg0, arg1, arg2, arg3, arg4, arg5) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  _emscripten_bind_Projection_Ortho_6(self, arg0, arg1, arg2, arg3, arg4, arg5);
};

Projection.prototype['GetProjectionMatrix'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Projection_GetProjectionMatrix_0(self), Matrix);
};

  Projection.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Projection___destroy___0(self);
}
// GameObject
function GameObject() {
  this.ptr = _emscripten_bind_GameObject_GameObject_0();
  getCache(GameObject)[this.ptr] = this;
};
GameObject.prototype = Object.create(WrapperObject.prototype);
GameObject.prototype.constructor = GameObject;
GameObject.prototype.__class__ = GameObject;
GameObject.__cache__ = {};
Module['GameObject'] = GameObject;

GameObject.prototype['Init'] = function() {
  var self = this.ptr;
  _emscripten_bind_GameObject_Init_0(self);
};

GameObject.prototype['Update'] = function() {
  var self = this.ptr;
  _emscripten_bind_GameObject_Update_0(self);
};

GameObject.prototype['Destroy'] = function() {
  var self = this.ptr;
  _emscripten_bind_GameObject_Destroy_0(self);
};

GameObject.prototype['GetLocalTransformation'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetLocalTransformation_0(self), Matrix);
};

GameObject.prototype['GetPosition'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetPosition_0(self), Vec3);
};

GameObject.prototype['GetRotation'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetRotation_0(self), Vec3);
};

GameObject.prototype['GetScale'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetScale_0(self), Vec3);
};

GameObject.prototype['GetDirection'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetDirection_0(self), Vec3);
};

GameObject.prototype['GetWorldTransformation'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetWorldTransformation_0(self), Matrix);
};

GameObject.prototype['GetWorldPosition'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetWorldPosition_0(self), Vec3);
};

GameObject.prototype['GetWorldRotation'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetWorldRotation_0(self), Vec3);
};

GameObject.prototype['SetPosition'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_SetPosition_1(self, arg0);
};

GameObject.prototype['SetRotation'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_SetRotation_1(self, arg0);
};

GameObject.prototype['SetScale'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_SetScale_1(self, arg0);
};

GameObject.prototype['SetTransformationMatrix'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_SetTransformationMatrix_1(self, arg0);
};

GameObject.prototype['LookAtGameObject'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_LookAtGameObject_1(self, arg0);
};

GameObject.prototype['LookAtVec'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_LookAtVec_1(self, arg0);
};

GameObject.prototype['AddComponent'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_AddComponent_1(self, arg0);
};

GameObject.prototype['RemoveComponent'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_RemoveComponent_1(self, arg0);
};

GameObject.prototype['AddGameObject'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_AddGameObject_1(self, arg0);
};

GameObject.prototype['RemoveGameObject'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_RemoveGameObject_1(self, arg0);
};

GameObject.prototype['GetParent'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_GameObject_GetParent_0(self), GameObject);
};

GameObject.prototype['HaveParent'] = function() {
  var self = this.ptr;
  return _emscripten_bind_GameObject_HaveParent_0(self);
};

GameObject.prototype['AddTag'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_AddTag_1(self, arg0);
};

GameObject.prototype['RemoveTag'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_GameObject_RemoveTag_1(self, arg0);
};

GameObject.prototype['HaveTag'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_GameObject_HaveTag_1(self, arg0);
};

GameObject.prototype['IsStatic'] = function() {
  var self = this.ptr;
  return _emscripten_bind_GameObject_IsStatic_0(self);
};

  GameObject.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_GameObject___destroy___0(self);
}
// RenderingComponent
function RenderingComponent(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  this.ptr = _emscripten_bind_RenderingComponent_RenderingComponent_2(arg0, arg1);
  getCache(RenderingComponent)[this.ptr] = this;
};
RenderingComponent.prototype = Object.create(WrapperObject.prototype);
RenderingComponent.prototype.constructor = RenderingComponent;
RenderingComponent.prototype.__class__ = RenderingComponent;
RenderingComponent.__cache__ = {};
Module['RenderingComponent'] = RenderingComponent;

RenderingComponent.prototype['AddLOD'] = function(arg0, arg1, arg2) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  _emscripten_bind_RenderingComponent_AddLOD_3(self, arg0, arg1, arg2);
};

RenderingComponent.prototype['Register'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_RenderingComponent_Register_1(self, arg0);
};

RenderingComponent.prototype['Init'] = function() {
  var self = this.ptr;
  _emscripten_bind_RenderingComponent_Init_0(self);
};

RenderingComponent.prototype['Update'] = function() {
  var self = this.ptr;
  _emscripten_bind_RenderingComponent_Update_0(self);
};

RenderingComponent.prototype['Destroy'] = function() {
  var self = this.ptr;
  _emscripten_bind_RenderingComponent_Destroy_0(self);
};

RenderingComponent.prototype['Unregister'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_RenderingComponent_Unregister_1(self, arg0);
};

RenderingComponent.prototype['SetCullingGeometry'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_RenderingComponent_SetCullingGeometry_1(self, arg0);
};

RenderingComponent.prototype['EnableCastShadows'] = function() {
  var self = this.ptr;
  _emscripten_bind_RenderingComponent_EnableCastShadows_0(self);
};

RenderingComponent.prototype['DisableCastShadows'] = function() {
  var self = this.ptr;
  _emscripten_bind_RenderingComponent_DisableCastShadows_0(self);
};

RenderingComponent.prototype['IsCastingShadows'] = function() {
  var self = this.ptr;
  return _emscripten_bind_RenderingComponent_IsCastingShadows_0(self);
};

RenderingComponent.prototype['HasBones'] = function() {
  var self = this.ptr;
  return _emscripten_bind_RenderingComponent_HasBones_0(self);
};

RenderingComponent.prototype['GetLODSize'] = function() {
  var self = this.ptr;
  return _emscripten_bind_RenderingComponent_GetLODSize_0(self);
};

RenderingComponent.prototype['GetLODByDistance'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_RenderingComponent_GetLODByDistance_1(self, arg0);
};

RenderingComponent.prototype['UpdateLOD'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_RenderingComponent_UpdateLOD_1(self, arg0);
};

RenderingComponent.prototype['GetBoundingSphereRadius'] = function() {
  var self = this.ptr;
  return _emscripten_bind_RenderingComponent_GetBoundingSphereRadius_0(self);
};

RenderingComponent.prototype['GetBoundingSphereCenter'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_RenderingComponent_GetBoundingSphereCenter_0(self), Vec3);
};

RenderingComponent.prototype['GetBoundingMinValue'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_RenderingComponent_GetBoundingMinValue_0(self), Vec3);
};

RenderingComponent.prototype['GetBoundingMaxValue'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_RenderingComponent_GetBoundingMaxValue_0(self), Vec3);
};

  RenderingComponent.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_RenderingComponent___destroy___0(self);
}
// Sphere
function Sphere(arg0, arg1, arg2, arg3, arg4, arg5) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  this.ptr = _emscripten_bind_Sphere_Sphere_6(arg0, arg1, arg2, arg3, arg4, arg5);
  getCache(Sphere)[this.ptr] = this;
};
Sphere.prototype = Object.create(WrapperObject.prototype);
Sphere.prototype.constructor = Sphere;
Sphere.prototype.__class__ = Sphere;
Sphere.__cache__ = {};
Module['Sphere'] = Sphere;

  Sphere.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Sphere___destroy___0(self);
}
// Vec4
function Vec4(arg0, arg1, arg2, arg3) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg0 === undefined) { this.ptr = _emscripten_bind_Vec4_Vec4_0(); getCache(Vec4)[this.ptr] = this;return }
  if (arg1 === undefined) { this.ptr = _emscripten_bind_Vec4_Vec4_1(arg0); getCache(Vec4)[this.ptr] = this;return }
  if (arg2 === undefined) { this.ptr = _emscripten_bind_Vec4_Vec4_2(arg0, arg1); getCache(Vec4)[this.ptr] = this;return }
  if (arg3 === undefined) { this.ptr = _emscripten_bind_Vec4_Vec4_3(arg0, arg1, arg2); getCache(Vec4)[this.ptr] = this;return }
  this.ptr = _emscripten_bind_Vec4_Vec4_4(arg0, arg1, arg2, arg3);
  getCache(Vec4)[this.ptr] = this;
};
Vec4.prototype = Object.create(WrapperObject.prototype);
Vec4.prototype.constructor = Vec4;
Vec4.prototype.__class__ = Vec4;
Vec4.__cache__ = {};
Module['Vec4'] = Vec4;

Vec4.prototype['dotProduct'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_Vec4_dotProduct_1(self, arg0);
};

Vec4.prototype['magnitude'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Vec4_magnitude_0(self);
};

Vec4.prototype['magnitudeSQR'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Vec4_magnitudeSQR_0(self);
};

Vec4.prototype['Abs'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Vec4_Abs_0(self), Vec4);
};

Vec4.prototype['add'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_add_1(self, arg0);
};

Vec4.prototype['sub'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_sub_1(self, arg0);
};

Vec4.prototype['mul'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_mul_1(self, arg0);
};

Vec4.prototype['div'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_div_1(self, arg0);
};

Vec4.prototype['add_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_add_scalar_1(self, arg0);
};

Vec4.prototype['sub_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_sub_scalar_1(self, arg0);
};

Vec4.prototype['mul_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_mul_scalar_1(self, arg0);
};

Vec4.prototype['div_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_div_scalar_1(self, arg0);
};

  Vec4.prototype['get_x']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec4_get_x_0(self);
}
    Vec4.prototype['set_x']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_set_x_1(self, arg0);
}
  Vec4.prototype['get_y']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec4_get_y_0(self);
}
    Vec4.prototype['set_y']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_set_y_1(self, arg0);
}
  Vec4.prototype['get_z']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec4_get_z_0(self);
}
    Vec4.prototype['set_z']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_set_z_1(self, arg0);
}
  Vec4.prototype['get_w']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec4_get_w_0(self);
}
    Vec4.prototype['set_w']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec4_set_w_1(self, arg0);
}
  Vec4.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Vec4___destroy___0(self);
}
// Vec2
function Vec2(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg0 === undefined) { this.ptr = _emscripten_bind_Vec2_Vec2_0(); getCache(Vec2)[this.ptr] = this;return }
  if (arg1 === undefined) { this.ptr = _emscripten_bind_Vec2_Vec2_1(arg0); getCache(Vec2)[this.ptr] = this;return }
  this.ptr = _emscripten_bind_Vec2_Vec2_2(arg0, arg1);
  getCache(Vec2)[this.ptr] = this;
};
Vec2.prototype = Object.create(WrapperObject.prototype);
Vec2.prototype.constructor = Vec2;
Vec2.prototype.__class__ = Vec2;
Vec2.__cache__ = {};
Module['Vec2'] = Vec2;

Vec2.prototype['dotProduct'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_Vec2_dotProduct_1(self, arg0);
};

Vec2.prototype['magnitude'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Vec2_magnitude_0(self);
};

Vec2.prototype['magnitudeSQR'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Vec2_magnitudeSQR_0(self);
};

Vec2.prototype['distance'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_Vec2_distance_1(self, arg0);
};

Vec2.prototype['distanceSQR'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_Vec2_distanceSQR_1(self, arg0);
};

Vec2.prototype['normalize'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Vec2_normalize_0(self), Vec2);
};

Vec2.prototype['negate'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Vec2_negate_0(self), Vec2);
};

Vec2.prototype['Abs'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Vec2_Abs_0(self), Vec2);
};

Vec2.prototype['add'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_add_1(self, arg0);
};

Vec2.prototype['sub'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_sub_1(self, arg0);
};

Vec2.prototype['mul'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_mul_1(self, arg0);
};

Vec2.prototype['div'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_div_1(self, arg0);
};

Vec2.prototype['add_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_add_scalar_1(self, arg0);
};

Vec2.prototype['sub_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_sub_scalar_1(self, arg0);
};

Vec2.prototype['mul_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_mul_scalar_1(self, arg0);
};

Vec2.prototype['div_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_div_scalar_1(self, arg0);
};

  Vec2.prototype['get_x']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec2_get_x_0(self);
}
    Vec2.prototype['set_x']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_set_x_1(self, arg0);
}
  Vec2.prototype['get_y']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec2_get_y_0(self);
}
    Vec2.prototype['set_y']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec2_set_y_1(self, arg0);
}
  Vec2.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Vec2___destroy___0(self);
}
// Vec3
function Vec3(arg0, arg1, arg2) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg0 === undefined) { this.ptr = _emscripten_bind_Vec3_Vec3_0(); getCache(Vec3)[this.ptr] = this;return }
  if (arg1 === undefined) { this.ptr = _emscripten_bind_Vec3_Vec3_1(arg0); getCache(Vec3)[this.ptr] = this;return }
  if (arg2 === undefined) { this.ptr = _emscripten_bind_Vec3_Vec3_2(arg0, arg1); getCache(Vec3)[this.ptr] = this;return }
  this.ptr = _emscripten_bind_Vec3_Vec3_3(arg0, arg1, arg2);
  getCache(Vec3)[this.ptr] = this;
};
Vec3.prototype = Object.create(WrapperObject.prototype);
Vec3.prototype.constructor = Vec3;
Vec3.prototype.__class__ = Vec3;
Vec3.__cache__ = {};
Module['Vec3'] = Vec3;

Vec3.prototype['dotProduct'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_Vec3_dotProduct_1(self, arg0);
};

Vec3.prototype['magnitude'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Vec3_magnitude_0(self);
};

Vec3.prototype['magnitudeSQR'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Vec3_magnitudeSQR_0(self);
};

Vec3.prototype['distance'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_Vec3_distance_1(self, arg0);
};

Vec3.prototype['distanceSQR'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return _emscripten_bind_Vec3_distanceSQR_1(self, arg0);
};

Vec3.prototype['cross'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  return wrapPointer(_emscripten_bind_Vec3_cross_1(self, arg0), Vec3);
};

Vec3.prototype['normalize'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Vec3_normalize_0(self), Vec3);
};

Vec3.prototype['normalizeSelf'] = function() {
  var self = this.ptr;
  _emscripten_bind_Vec3_normalizeSelf_0(self);
};

Vec3.prototype['negate'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Vec3_negate_0(self), Vec3);
};

Vec3.prototype['negateSelf'] = function() {
  var self = this.ptr;
  _emscripten_bind_Vec3_negateSelf_0(self);
};

Vec3.prototype['Abs'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Vec3_Abs_0(self), Vec3);
};

Vec3.prototype['add'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_add_1(self, arg0);
};

Vec3.prototype['sub'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_sub_1(self, arg0);
};

Vec3.prototype['mul'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_mul_1(self, arg0);
};

Vec3.prototype['div'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_div_1(self, arg0);
};

Vec3.prototype['add_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_add_scalar_1(self, arg0);
};

Vec3.prototype['sub_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_sub_scalar_1(self, arg0);
};

Vec3.prototype['mul_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_mul_scalar_1(self, arg0);
};

Vec3.prototype['div_scalar'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_div_scalar_1(self, arg0);
};

  Vec3.prototype['get_x']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec3_get_x_0(self);
}
    Vec3.prototype['set_x']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_set_x_1(self, arg0);
}
  Vec3.prototype['get_y']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec3_get_y_0(self);
}
    Vec3.prototype['set_y']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_set_y_1(self, arg0);
}
  Vec3.prototype['get_z']= function() {
  var self = this.ptr;
  return _emscripten_bind_Vec3_get_z_0(self);
}
    Vec3.prototype['set_z']= function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Vec3_set_z_1(self, arg0);
}
  Vec3.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Vec3___destroy___0(self);
}
// Text
function Text(arg0, arg1, arg2, arg3, arg4, arg5) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg5 && typeof arg5 === 'object') arg5 = arg5.ptr;
  else arg5 = ensureString(arg5);
  this.ptr = _emscripten_bind_Text_Text_6(arg0, arg1, arg2, arg3, arg4, arg5);
  getCache(Text)[this.ptr] = this;
};
Text.prototype = Object.create(WrapperObject.prototype);
Text.prototype.constructor = Text;
Text.prototype.__class__ = Text;
Text.__cache__ = {};
Module['Text'] = Text;

Text.prototype['UpdateText'] = function(arg0, arg1) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  _emscripten_bind_Text_UpdateText_2(self, arg0, arg1);
};

  Text.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Text___destroy___0(self);
}
// Font
function Font(arg0, arg1) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  this.ptr = _emscripten_bind_Font_Font_2(arg0, arg1);
  getCache(Font)[this.ptr] = this;
};
Font.prototype = Object.create(WrapperObject.prototype);
Font.prototype.constructor = Font;
Font.prototype.__class__ = Font;
Font.__cache__ = {};
Module['Font'] = Font;

Font.prototype['CreateText'] = function(arg0) {
  var self = this.ptr;
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  _emscripten_bind_Font_CreateText_1(self, arg0);
};

Font.prototype['GetTexture'] = function() {
  var self = this.ptr;
  return wrapPointer(_emscripten_bind_Font_GetTexture_0(self), Texture);
};

Font.prototype['GetFontSize'] = function() {
  var self = this.ptr;
  return _emscripten_bind_Font_GetFontSize_0(self);
};

  Font.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_Font___destroy___0(self);
}
// SpotLight
function SpotLight(arg0, arg1, arg2, arg3, arg4) {
  if (arg0 && typeof arg0 === 'object') arg0 = arg0.ptr;
  else arg0 = ensureString(arg0);
  if (arg1 && typeof arg1 === 'object') arg1 = arg1.ptr;
  else arg1 = ensureString(arg1);
  if (arg2 && typeof arg2 === 'object') arg2 = arg2.ptr;
  else arg2 = ensureString(arg2);
  if (arg3 && typeof arg3 === 'object') arg3 = arg3.ptr;
  else arg3 = ensureString(arg3);
  if (arg4 && typeof arg4 === 'object') arg4 = arg4.ptr;
  else arg4 = ensureString(arg4);
  if (arg0 === undefined) { this.ptr = _emscripten_bind_SpotLight_SpotLight_0(); getCache(SpotLight)[this.ptr] = this;return }
  if (arg1 === undefined) { this.ptr = _emscripten_bind_SpotLight_SpotLight_1(arg0); getCache(SpotLight)[this.ptr] = this;return }
  if (arg2 === undefined) { this.ptr = _emscripten_bind_SpotLight_SpotLight_2(arg0, arg1); getCache(SpotLight)[this.ptr] = this;return }
  if (arg3 === undefined) { this.ptr = _emscripten_bind_SpotLight_SpotLight_3(arg0, arg1, arg2); getCache(SpotLight)[this.ptr] = this;return }
  if (arg4 === undefined) { this.ptr = _emscripten_bind_SpotLight_SpotLight_4(arg0, arg1, arg2, arg3); getCache(SpotLight)[this.ptr] = this;return }
  this.ptr = _emscripten_bind_SpotLight_SpotLight_5(arg0, arg1, arg2, arg3, arg4);
  getCache(SpotLight)[this.ptr] = this;
};
SpotLight.prototype = Object.create(WrapperObject.prototype);
SpotLight.prototype.constructor = SpotLight;
SpotLight.prototype.__class__ = SpotLight;
SpotLight.__cache__ = {};
Module['SpotLight'] = SpotLight;

SpotLight.prototype['Start'] = function() {
  var self = this.ptr;
  _emscripten_bind_SpotLight_Start_0(self);
};

SpotLight.prototype['Update'] = function() {
  var self = this.ptr;
  _emscripten_bind_SpotLight_Update_0(self);
};

SpotLight.prototype['Destroy'] = function() {
  var self = this.ptr;
  _emscripten_bind_SpotLight_Destroy_0(self);
};

  SpotLight.prototype['__destroy__'] = function() {
  var self = this.ptr;
  _emscripten_bind_SpotLight___destroy___0(self);
}
