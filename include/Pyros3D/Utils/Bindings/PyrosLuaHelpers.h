//============================================================================
// Name        : PyrosLuaHelpers.h
// Description : Shared free functions used by split Lua binding modules.
//============================================================================
#ifdef LUA_BINDINGS
#ifndef PYROSLUAHELPERS_H
#define PYROSLUAHELPERS_H

#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>
#include <memory>
#include <tuple>
#include <vector>

namespace p3d {

Vec2 Vec2_operator_add(Vec2 &v1, const Vec2 &v2);
Vec2 Vec2_operator_sub(Vec2 &v1, const Vec2 &v2);
Vec2 Vec2_operator_mul(Vec2 &v1, const Vec2 &v2);
Vec2 Vec2_operator_div(Vec2 &v1, const Vec2 &v2);
Vec2 Vec2_operator_addS(Vec2 &v1, float f);
Vec2 Vec2_operator_subS(Vec2 &v1, float f);
Vec2 Vec2_operator_mulS(Vec2 &v1, float f);
Vec2 Vec2_operator_divS(Vec2 &v1, float f);
void Vec2_operator_Eadd(Vec2 &v1, const Vec2 &v2);
void Vec2_operator_Esub(Vec2 &v1, const Vec2 &v2);
void Vec2_operator_Emul(Vec2 &v1, const Vec2 &v2);
void Vec2_operator_Ediv(Vec2 &v1, const Vec2 &v2);
void Vec2_operator_EaddS(Vec2 &v1, float f);
void Vec2_operator_EsubS(Vec2 &v1, float f);
void Vec2_operator_EmulS(Vec2 &v1, float f);
void Vec2_operator_EdivS(Vec2 &v1, float f);
Vec3 Vec3_operator_add(Vec3 &v1, const Vec3 &v2);
Vec3 Vec3_operator_sub(Vec3 &v1, const Vec3 &v2);
Vec3 Vec3_operator_mul(Vec3 &v1, const Vec3 &v2);
Vec3 Vec3_operator_div(Vec3 &v1, const Vec3 &v2);
Vec3 Vec3_operator_addS(Vec3 &v1, float f);
Vec3 Vec3_operator_subS(Vec3 &v1, float f);
Vec3 Vec3_operator_mulS(Vec3 &v1, float f);
Vec3 Vec3_operator_divS(Vec3 &v1, float f);
void Vec3_operator_Eadd(Vec3 &v1, const Vec3 &v2);
void Vec3_operator_Esub(Vec3 &v1, const Vec3 &v2);
void Vec3_operator_Emul(Vec3 &v1, const Vec3 &v2);
void Vec3_operator_Ediv(Vec3 &v1, const Vec3 &v2);
void Vec3_operator_EaddS(Vec3 &v1, float f);
void Vec3_operator_EsubS(Vec3 &v1, float f);
void Vec3_operator_EmulS(Vec3 &v1, float f);
void Vec3_operator_EdivS(Vec3 &v1, float f);
Vec4 Vec4_operator_add(Vec4 &v1, const Vec4 &v2);
Vec4 Vec4_operator_sub(Vec4 &v1, const Vec4 &v2);
Vec4 Vec4_operator_mul(Vec4 &v1, const Vec4 &v2);
Vec4 Vec4_operator_div(Vec4 &v1, const Vec4 &v2);
Vec4 Vec4_operator_addS(Vec4 &v1, float f);
Vec4 Vec4_operator_subS(Vec4 &v1, float f);
Vec4 Vec4_operator_mulS(Vec4 &v1, float f);
Vec4 Vec4_operator_divS(Vec4 &v1, float f);
void Vec4_operator_Eadd(Vec4 &v1, const Vec4 &v2);
void Vec4_operator_Esub(Vec4 &v1, const Vec4 &v2);
void Vec4_operator_Emul(Vec4 &v1, const Vec4 &v2);
void Vec4_operator_Ediv(Vec4 &v1, const Vec4 &v2);
void Vec4_operator_EaddS(Vec4 &v1, float f);
void Vec4_operator_EsubS(Vec4 &v1, float f);
void Vec4_operator_EmulS(Vec4 &v1, float f);
void Vec4_operator_EdivS(Vec4 &v1, float f);
Matrix Matrix_operator_mul(Matrix &m1, const Matrix &m2);
Matrix Matrix_operator_mulS(Matrix &m, const f32 f);
Vec3 Matrix_operator_mulVec3(Matrix &m, const Vec3 &v);
Vec4 Matrix_operator_mulVec4(Matrix &m, const Vec4 &v);
void Matrix_operator_Emul(Matrix &m1, const Matrix &m2);
void Matrix_lookAt(Math::Matrix &m, const Math::Vec3 &eye, const Math::Vec3 &center, const Math::Vec3 &up);
void Matrix_lookAt2(Math::Matrix &m, const Math::Vec3 &eye, const Math::Vec3 &center);
void Matrix_translateXYZ(Math::Matrix &m, float x, float y, float z);
void Matrix_translateVec3(Math::Matrix &m, const Math::Vec3 &v);
void Matrix_scaleXYZ(Math::Matrix &m, float x, float y, float z);
void Matrix_scaleVec3(Math::Matrix &m, const Math::Vec3 &v);
Quaternion Quaternion_operator_mul(Quaternion &q1, const Quaternion &q2);
Quaternion Quaternion_operator_mulS(Quaternion &q, const f32 s);
Vec3 Quaternion_operator_mulVec3(Quaternion &q, const Vec3 &v);
Quaternion Quaternion_operator_negate(Quaternion &q);
bool GameObject_HaveTagSTR(GameObject &g, const std::string &tag);
bool GameObject_HaveTagUINT(GameObject &g, const uint32 tag);
std::shared_ptr<IComponent> LuaObjectToComponent(const sol::object &o);
void GameObject_AddComponentObj(GameObject &go, sol::object compObj);
void GameObject_RemoveComponentObj(GameObject &go, sol::object compObj);
std::shared_ptr<GameObject> LuaObjectToGameObject(const sol::object &o);
GameObject* LuaObjectToGameObjectPtr(const sol::object &o);
GameObject* AsGameObject(sol::object o);
void SceneGraph_AddObj(SceneGraph &scene, sol::object goObj);
void SceneGraph_RemoveObj(SceneGraph &scene, sol::object goObj);
void SceneGraph_AddGameObjectObj(SceneGraph &scene, sol::object goObj);
void SceneGraph_RemoveGameObjectObj(SceneGraph &scene, sol::object goObj);
void GameObject_AddGameObjectObj(GameObject &parent, sol::object childObj);
void GameObject_RemoveGameObjectObj(GameObject &parent, sol::object childObj);
sol::object GameObject_GetComponent(GameObject &go, const std::string &typeName, sol::this_state s);
void ForwardRenderer_EnableClipPlane(ForwardRenderer &r, uint32 numberOfClipPlanes);
void ForwardRenderer_EnableClipPlaneDefault(ForwardRenderer &r);
void ForwardRenderer_ClearBufferBit(ForwardRenderer &r, uint32 option);
void DeferredRenderer_EnableClipPlane(DeferredRenderer &r, uint32 numberOfClipPlanes);
void DeferredRenderer_EnableClipPlaneDefault(DeferredRenderer &r);
void DeferredRenderer_ClearBufferBit(DeferredRenderer &r, uint32 option);
void ForwardRenderer_PreRender(ForwardRenderer &r, sol::object camObj, SceneGraph* Scene);
void ForwardRenderer_PreRenderTag(ForwardRenderer &r, sol::object camObj, SceneGraph* Scene, const std::string &tag);
void ForwardRenderer_RenderScene(ForwardRenderer &r, sol::object projObj, sol::object camObj, SceneGraph* Scene);
void DeferredRenderer_PreRender(DeferredRenderer &r, sol::object camObj, SceneGraph* Scene);
void DeferredRenderer_PreRenderTag(DeferredRenderer &r, sol::object camObj, SceneGraph* Scene, const std::string &tag);
void DeferredRenderer_RenderScene(DeferredRenderer &r, const p3d::Projection &projection, sol::object camObj, SceneGraph* Scene);
void DeferredRenderer_RenderSceneOptions(DeferredRenderer &r, const p3d::Projection &projection, sol::object camObj, SceneGraph* Scene, const uint32 BufferOptions);
void Shader_SendUniform(Shader &s, const Uniform &uniform, int32 Handle);
void Shader_SendUniformPTR(Shader &s, const Uniform &uniform, void* data, int32 Handle, uint32 elementCount);
void SkeletonAnimationInstance_AddBone(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone);
void SkeletonAnimationInstance_AddBoneSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone);
void SkeletonAnimationInstance_AddBoneAndChilds(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone, bool inclusive);
void SkeletonAnimationInstance_AddBoneAndChildsSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone, bool inclusive);
void SkeletonAnimationInstance_RemoveBone(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone);
void SkeletonAnimationInstance_RemoveBoneSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone);
void SkeletonAnimationInstance_RemoveBoneAndChilds(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone, bool inclusive);
void SkeletonAnimationInstance_RemoveBoneAndChildsSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone, bool inclusive);
bool SkeletonAnimationInstance_IsPaused(SkeletonAnimationInstance &a);
bool SkeletonAnimationInstance_IsPausedID(SkeletonAnimationInstance &a, int ID);
void SkeletonAnimationInstance_DestroyLayer(SkeletonAnimationInstance &a, int id);
void SkeletonAnimationInstance_DestroyLayerSTR(SkeletonAnimationInstance &a, const std::string &str);
void Text_UpdateText(Text &t, const std::string &text, const Vec4 &color);
void Text_UpdateTextColors(Text &t, const std::string &text, const std::vector<Vec4> &color);
std::shared_ptr<IPhysicsComponent> IPhysics_CreateTriangleMeshRCOMP(IPhysics &p, RenderingComponent* rcomp, const f32 mass);
std::shared_ptr<IPhysicsComponent> IPhysics_CreateTriangleMesh(IPhysics &p, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass);
std::shared_ptr<IPhysicsComponent> IPhysics_CreateConvexTriangleMeshRCOMP(IPhysics &p, RenderingComponent* rcomp, const f32 mass);
std::shared_ptr<IPhysicsComponent> IPhysics_CreateConvexTriangleMesh(IPhysics &p, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass);
void FrameBuffer_InitTex(FrameBuffer &f, const uint32 attachmentFormat, const uint32 texType, const std::shared_ptr<Texture> &attachment);
void FrameBuffer_InitRenderBuffer4(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height);
void FrameBuffer_InitRenderBuffer5(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa);
void FrameBuffer_AddAttachTex(FrameBuffer &f, const uint32 attachmentFormat, const uint32 texType, const std::shared_ptr<Texture> &attachment);
void FrameBuffer_AddAttachRenderBuffer4(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height);
void FrameBuffer_AddAttachRenderBuffer5(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa);
void FrameBuffer_Bind(FrameBuffer &f, uint32 access);
void FrameBuffer_BindDefault(FrameBuffer &f);
void Texture_Resize3(Texture &t, uint32 width, uint32 height, uint32 level);
void Texture_Resize2(Texture &t, uint32 width, uint32 height);
SkeletonAnimationInstance* RenderingComponent_GetActiveSkeletonAnimation(RenderingComponent &rc);
TextureAnimationInstance* RenderingComponent_GetActiveTextureAnimation(RenderingComponent &rc);
std::shared_ptr<GenericShaderMaterial> RenderingMesh_GetGenericMaterial(RenderingMesh &m);
std::shared_ptr<Renderable> LuaObjectToRenderable(const sol::object &o);
std::shared_ptr<IMaterial> LuaObjectToMaterial(const sol::object &o);
bool LuaObjectToMaterialOptions(const sol::object &o, uint32 &out);
void RenderingComponent_ADDLOD(RenderingComponent* rcomp, sol::object renderableObj, const f32 Distance, sol::object materialOrOptions);
void RenderingComponent_ADDLOD_DistOnly(RenderingComponent* rcomp, sol::object renderableObj, const f32 Distance);
std::shared_ptr<LUA_RenderingComponent> LuaNewRenderingComponent(sol::object renderableObj, sol::object materialOrOptions);
std::shared_ptr<LUA_RenderingComponent> LuaNewRenderingComponentDist(sol::object renderableObj, sol::object materialOrOptions, float distance);
bool PlaceDecalAtCursor(float winW, float winH, float mouseX, float mouseY, GameObject* camera, Projection* projection, SceneGraph* scene, sol::object materialObj, const Vec3 &dimensions);

// Screen <-> world for scene Lua. The engine already had the Mouse3D
// machinery for this but only ever used it internally (PlaceDecalAtCursor),
// so a script that wanted to drag something in 3D had no way to ask where the
// cursor is pointing.
//
// Projects a world point to pixels. Returns false when the point is behind the
// camera, in which case the coordinates are meaningless rather than merely
// off-screen.
bool SetIKConstraintEnabled(GameObject* go, const std::string &chain, bool enabled);
bool SetIKConstraintWeight(GameObject* go, const std::string &chain, float weight);
bool WorldToScreen(float winW, float winH, GameObject* camera, Projection* projection,
	const Vec3 &worldPos, float* outX, float* outY);
// Inverse, resolved onto the plane through `refWorldPos` parallel to the
// screen - i.e. "where would the cursor be if it kept this thing's distance
// from the camera". That is the natural behaviour for dragging an object:
// depth stays put, only the screen-parallel position follows the mouse.
Vec3 ScreenToWorldAtDepth(float winW, float winH, float mouseX, float mouseY,
	GameObject* camera, Projection* projection, const Vec3 &refWorldPos);

} // namespace p3d

#endif
#endif
