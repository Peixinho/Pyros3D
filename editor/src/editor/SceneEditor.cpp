//============================================================================
// Name        : Scene.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ( ͡° ͜ʖ ͡°)
// Description : Pyros Scene
//============================================================================

#include <cmath>

#include "SceneEditor.h"
#include "libgizmo/GizmoTransformRender.h"
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Core/InputManager/InputManager.h>

using namespace p3d;

	SceneEditor::SceneEditor(bool* open, bool* openTree)
	{
		Open = open;
		OpenTree = openTree;
		_scale = Vec3(1, 1, 1);
		showDir = false;
		showingSceneDialog = false;
		sceneDialogIsSave = false;
		sceneDialogBrowse = false;

		// The Add form's fields are only ever written by its ImGui widgets,
		// and DragFloat/DragInt clamp to their min/max only once the user
		// actually edits them - open the form and press Add without touching
		// anything and whatever garbage these held went straight into the
		// primitive constructors. An uninitialized segment count is enough to
		// throw std::length_error out of the geometry's vertex vector and
		// terminate the editor.
		AddForm_w = AddForm_h = AddForm_d = 1.f;
		AddForm_p = 2.f;
		AddForm_q = 3.f;
		AddForm_oc = 45.f;
		AddForm_ic = 30.f;
		AddForm_sw = AddForm_sh = 16;
		AddForm_r = 8;
		AddForm_hscale = 1;
		AddForm_sn = true;
		AddForm_fn = false;
		AddForm_cgo = true;
		AddForm_hs = false;
		AddForm_oe = false;
		AddForm_cs = false;
		AddForm_dir = Vec3(0.f, -1.f, 0.f);
		AddForm_color = Vec4(1.f, 1.f, 1.f, 1.f);
		// What the Cast Shadows checkbox used to hardcode; now just the
		// starting value of an editable property.
		PropertiesShadowBiasFactor = 5.f;
		PropertiesShadowBiasUnits = 3.f;
		PropertiesShadowMapSize = 2048;
		PropertiesShadowNear = 0.01f;
		PropertiesShadowFar = 50.f;
		PropertiesShadowCascades = 1;
	}

	void SceneEditor::OnResize(const uint32 width, const uint32 height)
	{
		Width = width;
		Height = height;

		// Resize
		Renderer->Resize(width, height);
		Renderer->SetViewPort(0, 0, width, height);
	}

	void SceneEditor::Init(const uint32 width, const uint32 height)
	{
		Width = width;
		Height = height;

		// Initialize Scene
		scene = new SceneGraph();

		// Initialize Renderer
		Renderer = new ForwardRenderer(Width, Height);
		Renderer->SetViewPort(0, 0, Width, Height);
		Renderer->SetBackground(Vec4(0.2, 0.2, 0.2, 1.0));

		// Projection
		isPerspective = true;
		zoomOrtho = 5;

		// Physics
		physics = new Physics();
		physics->InitPhysics();
		physics->EnableDebugDraw();

		// Create Camera
		Camera = std::make_shared<GameObject>();
		Camera->SetPosition(Vec3(0, 10, 20));
		Camera->SetRotation(Vec3(-0.464, 0, 0));
		CameraPivot = std::make_shared<GameObject>();
		CameraPivot->Add(Camera);
		scene->Add(CameraPivot);
		scene->Add(Camera);

		// Create Grid
		grid = std::make_shared<GameObject>();
		gridhandle = std::make_shared<Grid>(30, 30, 1);
		GridMaterial = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color);
		GridMaterial->SetColor(Vec4(0.35f, 0.35f, 0.35f, 1.f));
		rGrid = std::make_shared<RenderingComponent>(gridhandle, GridMaterial);
		rGrid->DisableCastShadows();
		grid->Add(rGrid);
		RenderingMesh* rGridMesh = rGrid->GetMeshes()[0];
		rGridMesh->drawingType = DrawingType::Lines;

		// Add GameObject to Scene
		scene->Add(grid);

		_leftMouse = _middleMouse = _rightMouse = _mousePanned = false;

		gizmo = NULL;
		localTransform = false;

		// Null GameObject
		SelectedSceneObject = NULL;
		sceneObjects = new SceneObjects(scene);

		// Picking
		Picking = new PainterPick(Width, Height);
		Picking->SetViewPort(0, 0, Width, Height);

		// Translation Gizmo By Default
		UseTranslationManipulator();

		EffectsManager = new PostEffectsManager(Width, Height);

		InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &SceneEditor::MouseMove);
		InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &SceneEditor::MouseWheel);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &SceneEditor::MouseLeftPress);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Mouse::Left, this, &SceneEditor::MouseLeftRelease);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Middle, this, &SceneEditor::MouseMiddlePress);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Mouse::Middle, this, &SceneEditor::MouseMiddleRelease);
		InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Right, this, &SceneEditor::MouseRightPress);
		InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Mouse::Right, this, &SceneEditor::MouseRightRelease);

		axisHelper = new AxisHelper();

		node_clicked = -1;

		draggin_id = -1;
		droppin_id = -1;
		sub_selection = -1;

		showingAddFrom = false;
		showingAddFormType = 0;
		showRightMenu = false;

		SelectedMeshMaterial = std::make_shared<SelectedMaterial>();
		SelectedMeshMaterial->EnableDepthTest(DepthTest::LEqual);
		SelectedMeshMaterial->EnableBlending();
		SelectedMeshMaterial->BlendingEquation(BlendEq::Add);
		SelectedMeshMaterial->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		SelectedMesh = NULL;
		SelectedRenderingComponent.reset();

		debugRenderer = new DebugRenderer();
		// The gizmo draws through it - see CGizmoTransformRender.
		CGizmoTransformRender::SetDebugRenderer(debugRenderer);

		// Load Icons
		icons = new Texture();
		icons->LoadTexture("assets/icons/icons.png");
	}

	void SceneEditor::Show()
	{
		bool scene_view_open_flag = ImGui::Begin("Scene View", Open);
		if (scene_view_open_flag)
		{
			dim = Vec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
		}
		// A panel with no area yet - the frame the docked "Scene View"
		// window is first laid out reports a content region of (16, 0), and
		// a user can drag a split down to zero at any time. Resizing the
		// post-effect framebuffer to a zero dimension leaves it incomplete,
		// so the very next glClear() raises GL_INVALID_FRAMEBUFFER_OPERATION
		// and the engine's GLCHECKER traps. Nothing to draw into anyway:
		// skip the whole render for this frame.
		if (scene_view_open_flag && dim.x >= 1 && dim.y >= 1)
		{
			// Render Scene
			projection.Perspective(70.f, (f32)dim.x / (f32)dim.y, 0.1f, 100000.f);

			// Getting correct ortho dim
			{
				if (dim.x > dim.y)
				{
					r = zoomOrtho;
					l = -r;
					t = dim.y*zoomOrtho / dim.x;
					b = -t;
				}
				else if (dim.x < dim.y)
				{
					r = dim.x*zoomOrtho / dim.y;
					l = -r;
					t = zoomOrtho;
					b = -t;
				}
				else
				{
					r = zoomOrtho;
					l = -r;
					t = zoomOrtho;
					b = -t;
				}
				projectionOrtho.Ortho(l, r, b, t, 0.1f, 100000.f);
			}

			EffectsManager->ProcessPostEffects((isPerspective?&projection:&projectionOrtho));
			EffectsManager->Resize(dim.x, dim.y);
			Renderer->Resize(dim.x, dim.y);
			Renderer->ResetViewPort();
			Renderer->SetViewPort(0, 0, dim.x, dim.y);
			Renderer->PreRender(Camera.get(), scene);
			EffectsManager->CaptureFrame();
			Renderer->RenderScene((isPerspective ? projection : projectionOrtho), Camera.get(), scene);

			// The gizmo feeds DebugRenderer now (it no longer issues GL of its
			// own), so it has to submit its geometry *before* the flush below
			// rather than after - otherwise its primitives would sit in the
			// buffer until the next frame's Render(), one frame stale.
			// Both the gizmo and the bounding volumes go through DebugRenderer,
			// which cannot draw on Vulkan/Metal yet - see the comment on
			// PYROS_EDITOR_HAS_DEBUG_DRAW in CMakeLists.txt.
#if defined(PYROS_EDITOR_HAS_DEBUG_DRAW)
			if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT && gizmo != NULL)
			{
				gizmo->Draw();
				// No ClearBufferBit(Depth) here, deliberately: it *sets* the
				// renderer's persistent clear mask rather than clearing
				// anything, so from the first frame an object was selected
				// RenderScene()'s ClearScreen() stopped clearing colour and
				// the viewport smeared every frame after.
			}

			debugRenderer->Render(Camera->GetWorldTransformation().Inverse(), (isPerspective ? projection : projectionOrtho).GetProjectionMatrix());
#endif

			// Viewport Y origin differs by backend: GL's is bottom-left, so
			// dim.y - 90 puts the widget at the *top*; Vulkan and Metal use
			// top-left and the same value put it at the bottom. Express the
			// intent ("90 down from the top") per convention.
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
			axisHelper->Render(dim.x - 90, 10, 80, 80, isPerspective);
#else
			axisHelper->Render(dim.x - 90, dim.y - 90, 80, 80, isPerspective);
#endif

			//Renderer->ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
			EffectsManager->EndCapture();

			// Through the device rather than handing ImGui a raw GL texture
			// name: Vulkan needs a VkDescriptorSet and Metal an id<MTLTexture>,
			// and GetImGuiTextureID() is what each backend provides (the same
			// call DemoLauncher's render-target viewer uses).
			Texture* color = EffectsManager->GetColor();
			void* viewportTex = GetActiveRenderDevice().GetImGuiTextureID(color->GetBindID(), color->GetTextureType());
			// UV origin differs by backend. GL render targets are written
			// bottom-up relative to how ImGui samples them, so the V axis has
			// to be flipped; Vulkan and Metal write top-down and need the
			// straight mapping - with GL's flip they came out upside down.
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
			const ImVec2 uv0(0, 0), uv1(1, 1);
#else
			const ImVec2 uv0(0, 1), uv1(1, 0);
#endif
			if (viewportTex != NULL)
				ImGui::Image((ImTextureID)viewportTex, ImVec2(dim.x, dim.y), uv0, uv1);
			else
				ImGui::Dummy(ImVec2(dim.x, dim.y));
			mPos = Vec2(ImGui::GetMousePos().x - ImGui::GetWindowPos().x - ImGui::GetCursorStartPos().x, ImGui::GetMousePos().y - ImGui::GetWindowPos().y - ImGui::GetCursorStartPos().y);

		}
		ImGui::End();

		bool scene_tree_open_flag = ImGui::Begin("Scene Tree", OpenTree);
		if (scene_tree_open_flag)
		{

            ImGui::SetNextItemOpen(true);
			if (ImGui::TreeNode("Scene"))
			{

				DrawNodes();

				if (node_clicked != -1)
				{
					if (ImGui::GetIO().KeyCtrl)
					{
						if (selection.size() == 1 && sceneObjects->GetList().at(selection[0])->GetType() != SceneObjectTypes::GAMEOBJECT) selection.clear();
						if (sceneObjects->GetList().at(node_clicked)->GetType() != SceneObjectTypes::GAMEOBJECT) node_clicked = -1;
						sub_selection = -1;
						selection.push_back(node_clicked);
					}
					else {
						selection.clear();
						selection.push_back(node_clicked);
					}
				}
				node_clicked = -1;
				ImGui::TreePop();
			}

		}
		ImGui::End();

		// Right Menu
		if (showRightMenu)
		{
			ImGui::OpenPopup("rightmenu");
			showRightMenu = false;
		}
		if (ImGui::BeginPopup("rightmenu"))
		{
			ShowRightMenu();
			ImGui::EndPopup();
		}

		if (showingAddFrom) ShowAddForm();
		else editorDisabled = false;
	}

	void SceneEditor::DrawNodes(uint32 parentID, uint32 depth)
	{
		// Prevent infinite recursion by limiting depth
		if (depth > 100) {
			return;
		}
		
		bool parent_selected = false;
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
		{
			if ((*i).second == NULL) {
				continue;
			}
			if ((*i).second->GetParentID() == parentID)
			{
				ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow;
				for (int k = 0; k<selection.size(); k++)
					if (selection[k] == (*i).second->GetID())
					{
						base_flags |= ImGuiTreeNodeFlags_Selected;
						parent_selected = true;
						break;
					}

				bool node_open = false;

				string prefix;
				switch ((*i).second->GetType())
				{
				case SceneObjectTypes::GAMEOBJECT:
				{
					prefix = "[G]";
                    if ((*i).second->GetID() != draggin_id) {
                        // GameObjects can have children, so they're not leaf nodes
                        ImGuiTreeNodeFlags gameobject_flags = base_flags;
                        node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), gameobject_flags, "%s", string(prefix + (*i).second->GetName()).c_str());
                    } else {
                        node_open = false;
                    }
					break;
				}
				case SceneObjectTypes::RENDERING_COMPONENT:
				{
					prefix = "[R]";
                    if ((*i).second->GetID() != draggin_id) {
                        ImGuiTreeNodeFlags rendering_flags = base_flags;
                        node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), rendering_flags, "%s", string(prefix + (*i).second->GetName()).c_str());
                    } else {
                        node_open = false;
                    }
					break;
				}
				case SceneObjectTypes::PHYSICS_COMPONENT:
				{
					prefix = "[P]";
					ImGuiTreeNodeFlags physics_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if ((*i).second->GetID() != draggin_id) {
                        node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), physics_flags, "%s", string(prefix + (*i).second->GetName()).c_str());
                    } else {
                        node_open = false;
                    }
					break;
				}
				case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
				case SceneObjectTypes::POINTLIGHT_COMPONENT:
				case SceneObjectTypes::SPOTLIGHT_COMPONENT:
				{
					prefix = "[L]";
					ImGuiTreeNodeFlags light_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if ((*i).second->GetID() != draggin_id) {
                        node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*i).second->GetID(), light_flags, "%s", string(prefix + (*i).second->GetName()).c_str());
                    } else {
                        node_open = false;
                    }
					break;
				}
				default:
					break;
				}

				// Selected - only check for clicks if we actually created a TreeNode and it's not a dragging object
				if ((*i).second->GetID() != draggin_id && node_open) {
					if (ImGui::IsItemClicked())
					{
						node_clicked = (*i).second->GetID();
						if (sub_selection >= 0)
						{
							DeselectMesh();
							sub_selection = -1;
						}
						SelectSceneObject(sceneObjects->GetSceneObject(node_clicked));
					}
				}


//							}
//
//						}
//						else // No parent selected
//						{
//							if ((*i).second->GetType() == SceneObjectTypes::GAMEOBJECT)
//							{
//								GameObject* child = (GameObject*)sceneObjects->GetSceneObject(draggin_id)->GetPTR();
//								if (child->GetParent() != NULL)
//									child->GetParent()->Remove(child);
//
//								sceneObjects->GetSceneObject(draggin_id)->SetParentID(0);
//							}
//						}
//						draggin_id = -1;
//					}
//				}
//
//				if ((*i).second->GetType() == SceneObjectTypes::GAMEOBJECT)
//				{
//					// Mark for Dropping
//					if (ImGui::IsMouseDragging() && ImGui::IsItemHovered() && draggin_id != (*i).second->GetID())
//						droppin_id = (*i).second->GetID();
//
//					if (ImGui::IsMouseDragging() && !ImGui::IsItemHovered() && droppin_id == (*i).second->GetID())
//						droppin_id = -1;
//				}
//
				if (node_open)
				{
					if ((*i).second->GetType()==SceneObjectTypes::RENDERING_COMPONENT)
					{
						uint32 meshesCounter = 0;
						RenderingComponent* r = (RenderingComponent*)(*i).second->GetPTR();
						for (std::vector<RenderingMesh*>::iterator k = r->GetMeshes().begin(); k != r->GetMeshes().end(); k++)
						{
							ImGuiTreeNodeFlags mesh_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick;

							if (sub_selection == meshesCounter && parent_selected)
								mesh_flags |= ImGuiTreeNodeFlags_Selected;

							ImGui::TreeNodeEx((void*)(intptr_t)(*k), mesh_flags, "Mesh[%d]", meshesCounter);
							if (ImGui::IsItemClicked())
							{
								DeselectMesh();
								SelectMesh((*k));
								sub_selection = meshesCounter;
								node_clicked = (*i).second->GetID();
								SelectSceneObject(sceneObjects->GetSceneObject((*i).second->GetID()));
							}

							meshesCounter++;
						}
					}
					// Only call DrawNodes for non-leaf nodes that actually have children
					if ((*i).second->GetType() != SceneObjectTypes::PHYSICS_COMPONENT &&
						(*i).second->GetType() != SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT &&
						(*i).second->GetType() != SceneObjectTypes::POINTLIGHT_COMPONENT &&
						(*i).second->GetType() != SceneObjectTypes::SPOTLIGHT_COMPONENT)
					{
						// Check if this object actually has any children before making recursive call
						bool hasChildren = false;
						for (std::map<uint32, SceneObject*>::const_iterator j = sceneObjects->GetList().begin(); j != sceneObjects->GetList().end(); j++)
						{
							if ((*j).second != NULL && (*j).second->GetParentID() == (*i).second->GetID())
							{
								hasChildren = true;
								break;
							}
						}
						
						// Prevent infinite recursion by checking if object ID equals parent ID
						if (hasChildren && (*i).second->GetID() != parentID) {
							DrawNodes((*i).second->GetID(), depth + 1);
						} else {
							if ((*i).second->GetID() == parentID) {
							} else {
							}
						}
					}
					// Only call TreePop for non-leaf nodes
					if ((*i).second->GetType() != SceneObjectTypes::PHYSICS_COMPONENT &&
						(*i).second->GetType() != SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT &&
						(*i).second->GetType() != SceneObjectTypes::POINTLIGHT_COMPONENT &&
						(*i).second->GetType() != SceneObjectTypes::SPOTLIGHT_COMPONENT)
					{
						// Ensure we have a valid TreeNode to pop
						if (node_open) {
							ImGui::TreePop();
						} else {
						}
					}
				}
			}
		}
	}

	void SceneEditor::DeselectMesh()
	{
		if (SelectedRenderingComponent)
		{
			// Detaching drops the GameObject's reference; ours goes with
			// the reset() below.
			SelectedRenderingComponent->GetOwner()->Remove(SelectedRenderingComponent);
		}
		SelectedMesh = NULL;
		SelectedRenderingComponent.reset();
	}

	void SceneEditor::SelectMesh(RenderingMesh* rmesh)
	{
		if (SelectedMesh != rmesh)
		{
			DeselectMesh();

			SelectedMesh = rmesh;
			SelectedRenderingComponent = std::make_shared<RenderingComponent>(SelectedMesh->renderingComponent->GetRenderableShared(), SelectedMeshMaterial);
			rmesh->renderingComponent->GetOwner()->Add(SelectedRenderingComponent);
			for (int i = 0; i < SelectedRenderingComponent->GetMeshes().size(); i++)
			{
				SelectedRenderingComponent->GetMeshes()[i]->Active = false;
				if (SelectedRenderingComponent->GetMeshes()[i]->Geometry->GetInternalID() == rmesh->Geometry->GetInternalID())
					SelectedRenderingComponent->GetMeshes()[i]->Active = true;
			}
		}
	}

	void SceneEditor::Update(const f64 time)
	{
		// Update Physics
		physics->Update(time * 0.001, 10);

		// Update Light Helpers
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
		{
			if ((*i).second->Helper)
			{
				IHelper* helper = (IHelper*)(*i).second->Helper.get();
				if (helper->type == HELPER_TYPE::LIGHT || helper->owner->GetComponents().size() == 0)
				{
					helper->rcomp->Enable();
					helper->Update(Camera.get(), projection.GetProjectionMatrix(), isPerspective, projectionOrtho.Right, projectionOrtho.Top);
				}
				else
					helper->rcomp->Disable();
			}
		}

		// Update Scene
		scene->Update(time);

		// Send Mouse Coordinates in viewport space
		axisHelper->Update(time,Camera.get(),Vec2(mPos.x-dim.x+90, mPos.y-10));

		if (_middleMouse)
		{
			qX.AxisToQuaternion(Vec3(-1.f, 0.f, 0.f), DEGTORAD(mousePosition.y - mouse.y));
			qY.AxisToQuaternion(Vec3(0.f, -1.f, 0.f), DEGTORAD(mousePosition.x - mouse.x));
			rotation = (rotY * qY) * (rotX * qX);
			Matrix m = rotation.ConvertToMatrix();
			m.Translate(CameraPivot->GetPosition());
			CameraPivot->SetTransformationMatrix(m);

		}
		else if (_rightMouse)
		{
			// Pan
			rotation = rotY * rotX;
			Matrix m = rotation.ConvertToMatrix();
			Matrix m2; m2.Translate((rotY * rotX).ConvertToMatrix()*(pos - Vec3((mousePosition.x - mouse.x) / 75.f, -(mousePosition.y - mouse.y) / 75.f, 0)));
			CameraPivot->SetTransformationMatrix(m2*m);
			if (mousePosition.x - mouse.x != 0 || mousePosition.y - mouse.y != 0)
				_mousePanned = true;
		}

		if (SelectedSceneObject != NULL)
		{
			if (gizmo != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
			{
				// Gizmo Scale
				gizmo->SetDisplayScale(isPerspective ? .15f : .22f);
				// Gizmo mouse
				Mouse3D mray;
				mray.GenerateRay(dim.x, dim.y, mPos.x, mPos.y, Matrix(), Camera->GetWorldTransformation().Inverse(), projectionOrtho.GetProjectionMatrix());
				gizmo->SetOrthoMouse(mray.GetOrigin().x, mray.GetOrigin().y, mray.GetOrigin().z, mray.GetDirection().x, mray.GetDirection().y, mray.GetDirection().z);
				gizmo->SetScreenDimension(dim.x, dim.y, isPerspective, l, r, b, t);

				if (GizmoInUse == GizmoFunction::SCALE) {
					if (((GameObject*)SelectedSceneObject->GetPTR())->HaveParent())
						gizmo->SetLocalTransform((float*)&((GameObject*)SelectedSceneObject->GetPTR())->GetWorldTransformation().m);
					else
						gizmo->SetLocalTransform((float*)&SelectedSceneObject->LocalTransform.m);
					gizmo->SetEditMatrix((float*)&SelectedSceneObject->ScaleTransform.m);
				}
				else if (GizmoInUse == GizmoFunction::ROTATION && !localTransform)
				{
					if (((GameObject*)SelectedSceneObject->GetPTR())->HaveParent())
						gizmo->SetLocalTransform((float*)&((GameObject*)SelectedSceneObject->GetPTR())->GetWorldTransformation().m);
					else
						gizmo->SetLocalTransform((float*)&SelectedSceneObject->LocalTransform.m);
					gizmo->SetEditMatrix((float*)&SelectedSceneObject->globalRotation.m);
				}
				else { // POSITION LOCAL & GLOBAL AND LOCAL ROTATION
					if (((GameObject*)SelectedSceneObject->GetPTR())->HaveParent())
					{
						gizmo->SetLocalTransform((float*)&((GameObject*)SelectedSceneObject->GetPTR())->GetWorldTransformation().m);
						gizmo->SetGlobalTransform((float*)&((GameObject*)SelectedSceneObject->GetPTR())->GetParent()->GetWorldTransformation().m);
					}
					else {
						gizmo->SetLocalTransform((float*)&SelectedSceneObject->LocalTransform.m);
						gizmo->SetGlobalTransform((float*)Matrix().m);
					}
					gizmo->SetEditMatrix((float*)&SelectedSceneObject->LocalTransform.m);
				}

				Vec3 trans = SelectedSceneObject->LocalTransform.GetTranslation();
				Matrix m = SelectedSceneObject->globalRotation*SelectedSceneObject->LocalTransform;
				m.Translate(trans);

				// Fill Object Properties
				if (_leftMouse) {
					Vec3 actualScale = m.GetScale();
					// Ensure scale components are not zero to avoid division by zero
					if (fabs(actualScale.x) < 0.0001f) actualScale.x = 1.0f;
					if (fabs(actualScale.y) < 0.0001f) actualScale.y = 1.0f;
					if (fabs(actualScale.z) < 0.0001f) actualScale.z = 1.0f;
					SetObjectProperties(m.GetTranslation(), m.GetRotation(actualScale).GetEulerFromRotationMatrix(), SelectedSceneObject->ScaleTransform.GetScale());
				}
				else {
					SelectedSceneObject->LocalTransform.identity();
					SelectedSceneObject->LocalTransform.Translate(_translation);
					SelectedSceneObject->LocalTransform.SetRotationFromEuler(_rotation);
					SelectedSceneObject->ScaleTransform.ForceScale(_scale);
				}

				// Set From Fields
				if (SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT)
				{
					((GameObject*)SelectedSceneObject->GetPTR())->SetPosition(_translation);
					((GameObject*)SelectedSceneObject->GetPTR())->SetRotation(_rotation);
					((GameObject*)SelectedSceneObject->GetPTR())->SetScale(_scale);
				}

				gizmo->SetCameraMatrix(Camera->GetWorldTransformation().Inverse().m, (isPerspective ? projection : projectionOrtho).GetProjectionMatrix().m);
			}

			// Lights Selected
			if (SelectedSceneObject->GetType() == SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT)
			{
				((DirectionalLight*)SelectedSceneObject->GetPTR())->SetLightDirection(PropertiesLightDirection);
				((DirectionalLight*)SelectedSceneObject->GetPTR())->SetLightColor(PropertiesLightColor);
			}
			if (SelectedSceneObject->GetType() == SceneObjectTypes::POINTLIGHT_COMPONENT)
			{
				((PointLight*)SelectedSceneObject->GetPTR())->SetLightRadius(PropertiesLightRadius);
				((PointLight*)SelectedSceneObject->GetPTR())->SetLightColor(PropertiesLightColor);
			}
			if (SelectedSceneObject->GetType() == SceneObjectTypes::SPOTLIGHT_COMPONENT)
			{
				((SpotLight*)SelectedSceneObject->GetPTR())->SetLightRadius(PropertiesLightRadius);
				((SpotLight*)SelectedSceneObject->GetPTR())->SetLightDirection(PropertiesLightDirection);
				((SpotLight*)SelectedSceneObject->GetPTR())->SetLightColor(PropertiesLightColor);
				((SpotLight*)SelectedSceneObject->GetPTR())->SetLightInnerCone(PropertiesLightInnerCone);
				((SpotLight*)SelectedSceneObject->GetPTR())->SetLightOutterCone(PropertiesLightOutterCone);
			}
		}
		// Debug Draw - accumulates into DebugRenderer, so it is gated the same
		// way as the flush in Show().
#if defined(PYROS_EDITOR_HAS_DEBUG_DRAW)
		{
			DrawBoundings(SelectedSceneObject);
		}
#endif
	}

	void SceneEditor::DrawBoundings(SceneObject* obj)
	{
		if (obj != NULL)
		{
			switch (obj->GetType())
			{
			case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
			{
				DirectionalLight* d = (DirectionalLight*)obj->GetPTR();
				Matrix mRot, mTrans, mDirection;
				mRot.RotationX(-PI*.5f);
				mTrans.TranslateY(-5.f);
				// Matrix * Vec3 applies m[12..14], i.e. it treats the vector as
				// a *point* - so passing a direction through it added the
				// owner's translation, and the cylinder pointed somewhere
				// arbitrary as soon as the light was not at the origin. The
				// renderer gets this right with Vec4(dir, 0.f) (IRenderer.cpp's
				// PreRender); mirror it exactly. LookAt() also wants a target
				// position, not a direction, hence position + direction.
				const Vec3 dPos = d->GetOwner()->GetWorldPosition();
				const Vec3 dDir = (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f)).xyz().normalize();
				mDirection.LookAt(dPos, dPos + dDir.negate());
				DrawBoundingCylinder(2, 5, mDirection.Inverse()*mRot*mTrans);
			}
			break;
			case SceneObjectTypes::POINTLIGHT_COMPONENT:
			{
				PointLight* p = (PointLight*)obj->GetPTR();
				Matrix m; m.Translate(p->GetOwner()->GetWorldPosition());
				DrawBoundingSphere(p->GetLightRadius(), m);
			}
			break;
			case SceneObjectTypes::SPOTLIGHT_COMPONENT:
			{
				SpotLight* s = (SpotLight*)obj->GetPTR();
				Matrix mRot, mTrans, mDirection;
				mRot.RotationX(-PI*.5f);
				mTrans.TranslateY(-s->GetLightRadius()*.5f);
				mDirection.LookAt(Vec3::ZERO, s->GetLightDirection().normalize().negate());
				DrawBoundingCone(tanf(DEGTORAD(s->GetLightOutterCone()))*s->GetLightRadius(), s->GetLightRadius()*.5f, s->GetOwner()->GetWorldTransformation()*mDirection.Inverse()*mRot*mTrans);
			}
			break;
			case SceneObjectTypes::RENDERING_COMPONENT:
			{
				Vec3 minBounds = ((RenderingComponent*)obj->GetPTR())->GetOwner()->GetBoundingMinValue();
				Vec3 maxBounds = ((RenderingComponent*)obj->GetPTR())->GetOwner()->GetBoundingMaxValue();
				DrawBoundingBox(minBounds, maxBounds, ((RenderingComponent*)obj->GetPTR())->GetOwner()->GetWorldTransformation());
			}
			break;
			case SceneObjectTypes::GAMEOBJECT:
			default:
			{
				GameObject *go = ((GameObject*)obj->GetPTR());
				for (std::vector<std::shared_ptr<IComponent>>::const_iterator i = go->GetComponents().begin(); i != go->GetComponents().end(); i++)
					DrawBoundings(sceneObjects->GetSceneObject(sceneObjects->GetSceneObjectID((*i).get())));
			}
			break;
			};
		}
	}

	void SceneEditor::DrawBoundingBox(const Vec3 &vec1, const Vec3 &vec2, const Matrix &transform)
	{
		debugRenderer->pushMatrix(transform);
		Vec3 min = vec1;
		Vec3 max = vec2;
		debugRenderer->drawLine(Vec3(min.x, min.y, min.z), Vec3(max.x, min.y, min.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, min.y, min.z), Vec3(max.x, min.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, min.y, max.z), Vec3(min.x, min.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(min.x, min.y, max.z), Vec3(min.x, min.y, min.z), Vec4(1, 0, 0, 0.5));

		debugRenderer->drawLine(Vec3(min.x, max.y, min.z), Vec3(max.x, max.y, min.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, max.y, min.z), Vec3(max.x, max.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, max.y, max.z), Vec3(min.x, max.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(min.x, max.y, max.z), Vec3(min.x, max.y, min.z), Vec4(1, 0, 0, 0.5));

		debugRenderer->drawLine(Vec3(min.x, max.y, min.z), Vec3(min.x, min.y, min.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, max.y, max.z), Vec3(max.x, min.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(max.x, max.y, min.z), Vec3(max.x, min.y, min.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->drawLine(Vec3(min.x, max.y, max.z), Vec3(min.x, min.y, max.z), Vec4(1, 0, 0, 0.5));
		debugRenderer->popMatrix();
	}

	void SceneEditor::DrawBoundingSphere(const f32 radius, const Matrix &transform)
	{
		debugRenderer->pushMatrix(transform);
		debugRenderer->drawSphere(Vec3::ZERO, radius, Vec4(1, 1, 0, 0.5));
		debugRenderer->popMatrix();
	}

	void SceneEditor::DrawBoundingCone(const f32 radius, const f32 height, const Matrix &transform)
	{
		debugRenderer->pushMatrix(transform);
		debugRenderer->drawCone(radius, height, Vec4(1, 1, 0, 0.5));
		debugRenderer->popMatrix();
	}

	void SceneEditor::DrawBoundingCylinder(const f32 radius, const f32 height, const Matrix &transform)
	{
		debugRenderer->pushMatrix(transform);
		debugRenderer->drawCylinder(radius, height, Vec4(1, 1, 0, 0.5));
		debugRenderer->popMatrix();
	}

    	void SceneEditor::SelectSceneObject(SceneObject* go)
	{
        if (go == NULL) {
            return;
        }
        SelectedSceneObject = go;
		switch (go->GetType())
		{
			case SceneObjectTypes::GAMEOBJECT:
			{
                Vec3 trans = SelectedSceneObject->LocalTransform.GetTranslation();
                Matrix m = SelectedSceneObject->globalRotation * SelectedSceneObject->LocalTransform;
				m.Translate(trans);

				Vec3 actualScale = m.GetScale();
				// Ensure scale components are not zero to avoid division by zero
				if (fabs(actualScale.x) < 0.0001f) actualScale.x = 1.0f;
				if (fabs(actualScale.y) < 0.0001f) actualScale.y = 1.0f;
				if (fabs(actualScale.z) < 0.0001f) actualScale.z = 1.0f;
				SetObjectProperties(m.GetTranslation(), m.GetRotation(actualScale).GetEulerFromRotationMatrix(), SelectedSceneObject->ScaleTransform.GetScale());
			}
			break;
			case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
			{
				PropertiesLightDirection = ((DirectionalLight*)SelectedSceneObject->GetPTR())->GetLightDirection();
				PropertiesLightColor = ((DirectionalLight*)SelectedSceneObject->GetPTR())->GetLightColor();
				AddForm_cs = ((DirectionalLight*)SelectedSceneObject->GetPTR())->IsCastingShadows();
				SeedShadowProperties((DirectionalLight*)SelectedSceneObject->GetPTR());
				PropertiesShadowCascades = (int32)((DirectionalLight*)SelectedSceneObject->GetPTR())->GetNumberCascades();
			}
			break;
			case SceneObjectTypes::POINTLIGHT_COMPONENT:
			{
				PropertiesLightRadius = ((PointLight*)SelectedSceneObject->GetPTR())->GetLightRadius();
				PropertiesLightColor = ((PointLight*)SelectedSceneObject->GetPTR())->GetLightColor();
				AddForm_cs = ((PointLight*)SelectedSceneObject->GetPTR())->IsCastingShadows();
				SeedShadowProperties((PointLight*)SelectedSceneObject->GetPTR());
			}
			break;
			case SceneObjectTypes::SPOTLIGHT_COMPONENT:
			{
				PropertiesLightRadius = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightRadius();
				PropertiesLightDirection = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightDirection();
				PropertiesLightColor = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightColor();
				PropertiesLightInnerCone = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightInnerCone();
				PropertiesLightOutterCone = ((SpotLight*)SelectedSceneObject->GetPTR())->GetLightOutterCone();
				AddForm_cs = ((SpotLight*)SelectedSceneObject->GetPTR())->IsCastingShadows();
				SeedShadowProperties((SpotLight*)SelectedSceneObject->GetPTR());
			}
			break;
		};

	}

	// One modal for Open Scene and Save Scene As. A typed path rather than a
	// browser: the editor has no file-listing widget (the original one was
	// never finished), and a path field is honest about that.
	void SceneEditor::DrawSceneFileDialog()
	{
		if (!showingSceneDialog) return;

		const char* title = sceneDialogIsSave ? "Save Scene" : "Open Scene";
		ImGui::OpenPopup(title);
		if (ImGui::BeginPopupModal(title, &showingSceneDialog,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
		{
			editorDisabled = true;
			ImGui::Text("Scene file (.json):");
			ImGui::SetNextItemWidth(300.f);
			ImGui::InputText("##scenepath", &sceneDialogPath);
			ImGui::SameLine();
			// Browsing only makes sense for Open - Save needs a name that
			// does not exist yet, which a file listing cannot offer.
			if (!sceneDialogIsSave)
			{
				if (ImGui::Button("Browse..."))
					ImGui::_priv::OpenLocation("", "json", &sceneDialogBrowse);
			}
			else ImGui::TextDisabled("(type a name)");

			if (sceneDialogBrowse)
			{
				std::string picked;
				if (ImGui::FilePath("##browse", "", "json", &picked, 1024, &sceneDialogBrowse))
					if (picked.size() > 0) sceneDialogPath = picked;
			}

			if (sceneDialogError.size() > 0)
				ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "%s", sceneDialogError.c_str());

			ImGui::Spacing();
			if (ImGui::Button(sceneDialogIsSave ? "Save" : "Open"))
			{
				bool ok = sceneDialogIsSave ? SaveSceneToFile(sceneDialogPath)
										    : LoadSceneFromFile(sceneDialogPath);
				if (ok)
				{
					showingSceneDialog = false;
					editorDisabled = false;
					ImGui::CloseCurrentPopup();
				}
				else
					sceneDialogError = sceneDialogIsSave ? "Could not write that file." : "Could not read that file.";
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				showingSceneDialog = false;
				editorDisabled = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void SceneEditor::DetachEditorObjects(std::vector<std::shared_ptr<GameObject>> &out)
	{
		out.clear();
		// Helper icons first - they are per-SceneObject and must come back
		// attached to the same registry entries.
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
			if ((*i).second != NULL && (*i).second->Helper)
				out.push_back((*i).second->Helper);

		out.push_back(grid);
		out.push_back(Camera);
		out.push_back(CameraPivot);

		for (std::vector<std::shared_ptr<GameObject>>::iterator i = out.begin(); i != out.end(); i++)
			scene->Remove(*i);
	}

	void SceneEditor::AttachEditorObjects(std::vector<std::shared_ptr<GameObject>> &saved)
	{
		for (std::vector<std::shared_ptr<GameObject>>::iterator i = saved.begin(); i != saved.end(); i++)
			scene->Add(*i);
		saved.clear();
	}

	// Gives every adopted GameObject and light its viewport icon, the way the
	// Add form does for objects created through the UI.
	void SceneEditor::RebuildHelpers()
	{
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
		{
			SceneObject* obj = (*i).second;
			if (obj == NULL || obj->Helper) continue;

			if (obj->GetType() == SceneObjectTypes::GAMEOBJECT)
			{
				std::shared_ptr<GameObjectHelper> h = std::make_shared<GameObjectHelper>((GameObject*)obj->GetPTR());
				obj->Helper = h;
				scene->Add(h);
			}
			else if (obj->GetType() == SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT ||
					 obj->GetType() == SceneObjectTypes::POINTLIGHT_COMPONENT ||
					 obj->GetType() == SceneObjectTypes::SPOTLIGHT_COMPONENT)
			{
				IComponent* c = (IComponent*)obj->GetPTR();
				if (c == NULL || c->GetOwner() == NULL) continue;
				std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(c->GetOwner());
				obj->Helper = h;
				scene->Add(h);
			}
		}
	}

	void SceneEditor::NewScene()
	{
		DeselectMesh();
		DeselectSceneObject();
		selection.clear();
		node_clicked = -1;

		// Drops every user GameObject/component (and its helper) - the
		// SceneGraph holds the only strong references, so this frees them.
		sceneObjects->DestroyAll();
		scenePath.clear();
	}

	bool SceneEditor::SaveSceneToFile(const std::string &path)
	{
		if (path.size() == 0) return false;

		std::vector<std::shared_ptr<GameObject>> furniture;
		DetachEditorObjects(furniture);
		bool ok = false;
		try { ok = SceneSerializer::SaveScene(scene, path); }
		catch (const std::exception &e) { echo(std::string("ERROR: scene save threw: ") + e.what()); ok = false; }
		AttachEditorObjects(furniture);

		if (ok) scenePath = path;
		else echo("ERROR: failed to save scene to " + path);
		return ok;
	}

	bool SceneEditor::LoadSceneFromFile(const std::string &path)
	{
		if (path.size() == 0) return false;

		NewScene();

		std::vector<std::shared_ptr<GameObject>> furniture;
		DetachEditorObjects(furniture);

		// outAssets deliberately NULL: the SceneGraph holds the only strong
		// references to what LoadScene builds, so removing the roots (which
		// DestroyAll does) frees everything. Tracking them separately and
		// calling UnloadScene as well would free the same objects twice.
		// Defence in depth alongside the serializer's own shape check: the
		// user can point this at any file on disk, and the loader walks a lot
		// of nested JSON that a structurally-valid-but-wrong document could
		// still trip over. A bad pick should be a dialog error, never a
		// terminate.
		bool ok = false;
		try { ok = SceneSerializer::LoadScene(scene, path, physics); }
		catch (const std::exception &e) { echo(std::string("ERROR: scene load threw: ") + e.what()); ok = false; }

		if (ok)
		{
			std::vector<std::shared_ptr<GameObject>> roots = scene->GetAllGameObjectList();
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = roots.begin(); i != roots.end(); i++)
				sceneObjects->Adopt((*i).get());
			scenePath = path;
		}
		else echo("ERROR: failed to load scene from " + path);

		AttachEditorObjects(furniture);
		if (ok) RebuildHelpers();
		return ok;
	}

	void SceneEditor::DeselectSceneObject()
	{
		SelectedSceneObject = NULL;
	}

	void SceneEditor::Shutdown()
	{
		// All your Shutdown Code Here
		grid->Remove(rGrid);
		scene->Remove(grid);
		scene->Remove(Camera);
		scene->Remove(CameraPivot);

		rGrid.reset();
		grid.reset();
		gridhandle.reset();
		GridMaterial.reset();
		Camera.reset();
		CameraPivot.reset();
		SelectedRenderingComponent.reset();
		SelectedMeshMaterial.reset();
		tempMaterial.reset();

		// Ordering matters here, and it used to be wrong. ForwardRenderer
		// owns the active IRenderDevice; everything below *borrows* it -
		// PostEffectsManager explicitly so (ResolvePostEffectsDevice() hands
		// it a non-owning pointer when a device is already active), and the
		// first statement of ~PostEffectsManager is device->WaitIdle().
		// Deleting Renderer first therefore turned every clean exit into a
		// call through a freed device, i.e. closing the editor always
		// segfaulted. Destroy the consumers, then the scene and its GPU
		// resources, and let the renderer that owns the device go last.
		delete sceneObjects;
		delete EffectsManager;
		delete Picking;
		delete axisHelper;
		delete debugRenderer;
		delete icons;
		delete scene;
		delete physics;
		delete Renderer;

		InputManager::RemoveEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &SceneEditor::MouseMove);
		InputManager::RemoveEvent(Event::Type::OnMove, Event::Input::Mouse::Wheel, this, &SceneEditor::MouseWheel);
		InputManager::RemoveEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &SceneEditor::MouseLeftPress);
		InputManager::RemoveEvent(Event::Type::OnRelease, Event::Input::Mouse::Left, this, &SceneEditor::MouseLeftRelease);
		InputManager::RemoveEvent(Event::Type::OnPress, Event::Input::Mouse::Middle, this, &SceneEditor::MouseMiddlePress);
		InputManager::RemoveEvent(Event::Type::OnRelease, Event::Input::Mouse::Middle, this, &SceneEditor::MouseMiddleRelease);
		InputManager::RemoveEvent(Event::Type::OnPress, Event::Input::Mouse::Right, this, &SceneEditor::MouseRightPress);
		InputManager::RemoveEvent(Event::Type::OnRelease, Event::Input::Mouse::Right, this, &SceneEditor::MouseRightRelease);

		if (gizmo != NULL) delete gizmo;

	}

	SceneEditor::~SceneEditor()
	{
		Shutdown();
	}

	void SceneEditor::MouseWheel(Event::Input::Info e)
	{
		if ((mPos.x > 0 && mPos.x < dim.x) &&
			(mPos.y > 0 && mPos.y < dim.y) &&
			!editorDisabled
			)
		{

			Vec2 tempMouse = mousePosition;
			if (tempMouse.x > 0 && tempMouse.x < Width)
			{
				if (isPerspective)
				{
					// zoomOrtho In and Out
					Vec3 finalPosition;
					Vec3 direction = Vec3(Camera->GetLocalTransformation().m[8], Camera->GetLocalTransformation().m[9], Camera->GetLocalTransformation().m[10]);
					finalPosition -= direction * f32(e.Value);
					Camera->SetPosition(Camera->GetPosition() + finalPosition);
				}
				else zoomOrtho -= 0.1 * f32(e.Value);
			}
		}
	}

	void SceneEditor::MouseLeftPress(Event::Input::Info e)
	{
		if ((mPos.x > 0 && mPos.x < dim.x) &&
			(mPos.y > 0 && mPos.y < dim.y) &&
			!editorDisabled)
		{
			switch (axisHelper->MouseClick())
			{
				case AXIS_HELPER_AXIS::CENTER:
					UseCamera0();
					break;
				case AXIS_HELPER_AXIS::NEGATIVE_X:
					UseCamera2(true);
					break;
				case AXIS_HELPER_AXIS::POSITIVE_X:
					UseCamera2();
					break;
				case AXIS_HELPER_AXIS::NEGATIVE_Y:
					UseCamera3(true);
					break;
				case AXIS_HELPER_AXIS::POSITIVE_Y:
					UseCamera3();
					break;
				case AXIS_HELPER_AXIS::NEGATIVE_Z:
					UseCamera1(true);
					break;
				case AXIS_HELPER_AXIS::POSITIVE_Z:
					UseCamera1();
					break;
				case -1:
				default:
				{
				}
				Vec2 tempMouse = mousePosition;
				if (!_rightMouse && !_middleMouse && (tempMouse.x > 0 && tempMouse.x < Width))
				{
					_leftMouse = true;
				}
				if ((gizmo != NULL && !gizmo->OnMouseDown(mousePosition.x, mousePosition.y)) || gizmo == NULL)
				{
					// Mouse Picking
					Picking->Resize(dim.x, dim.y);
					Picking->ResetViewPort();
					Picking->SetViewPort(0, 0, dim.x, dim.y);
					RenderingMesh* rm = Picking->PickObject(mousePosition.x, mousePosition.y, (isPerspective?projection:projectionOrtho), Camera.get(), scene);

					if (rm != NULL && rm->renderingComponent != rGrid.get())
					{
						// Check if is Helper
						bool helper = false;
						for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
						{
							if ((*i).second->Helper)
							{
								if ((*i).second->Helper.get() == rm->renderingComponent->GetOwner())
								{
									node_clicked = sceneObjects->GetSceneObjectID(((IHelper*)(*i).second->Helper.get())->owner);
									helper = true;
									break;
								}
							}
						}

						// if Not ...*/
						if (!helper)
							node_clicked = sceneObjects->GetSceneObjectID(rm->renderingComponent->GetOwner());

						DeselectMesh();
						SelectSceneObject(sceneObjects->GetSceneObject(node_clicked));
					}
				}
			}
		}
	}

	void SceneEditor::MouseLeftRelease(Event::Input::Info e)
	{
		_leftMouse = false;
		if (gizmo != NULL && SelectedSceneObject != NULL)
		{
			gizmo->OnMouseUp(mousePosition.x, mousePosition.y);
			if (GizmoInUse == GizmoFunction::ROTATION && !localTransform)
			{
				Vec3 trans = SelectedSceneObject->LocalTransform.GetTranslation();
				SelectedSceneObject->LocalTransform.Translate(Vec3(0, 0, 0));
				SelectedSceneObject->LocalTransform = SelectedSceneObject->globalRotation*SelectedSceneObject->LocalTransform;
				SelectedSceneObject->LocalTransform.Translate(trans);
				SelectedSceneObject->globalRotation.identity();
			}
		}
	}

	void SceneEditor::MouseMiddlePress(Event::Input::Info e)
	{
		if ((mPos.x > 0 && mPos.x < dim.x) &&
			(mPos.y > 0 && mPos.y < dim.y) &&
			!editorDisabled)
		{
			Vec2 tempMouse = mousePosition;
			if (!_rightMouse && !_leftMouse && (tempMouse.x > 0 && tempMouse.x < Width))
			{
				_middleMouse = true;
				mouse = mousePosition;
			}
		}
	}

	void SceneEditor::MouseMiddleRelease(Event::Input::Info e)
	{
		if (_middleMouse)
		{
			_middleMouse = false;
			rotX = rotX * qX;
			rotY = rotY * qY;
			rotation = Quaternion();
		}
	}

	void SceneEditor::MouseRightPress(Event::Input::Info e)
	{
		if ((mPos.x > 0 && mPos.x < dim.x) &&
			(mPos.y > 0 && mPos.y < dim.y) &&
			!editorDisabled)
		{
			Vec2 tempMouse = mousePosition;
			if (!_middleMouse && !_leftMouse && (tempMouse.x > 0 && tempMouse.x < Width))
			{
				_mousePanned = false;
				_rightMouse = true;
				mouse = mousePosition;
				pos = (rotY*rotX).ConvertToMatrix().Inverse()*CameraPivot->GetPosition();
			}
		}
	}

	void SceneEditor::MouseRightRelease(Event::Input::Info e)
	{
		if (!_mousePanned) // show right click menu
			showRightMenu = true;

		_rightMouse = false;
	}

	void SceneEditor::MouseMove(Event::Input::Info e)
	{
		Vec2 m = e.Value;
		if (gizmo != NULL) gizmo->OnMouseMove(mPos.x, mPos.y);
		mousePosition = mPos;
	}

	void SceneEditor::SetObjectProperties(const Vec3 &Translation, const Vec3 &Rotation, const Vec3 &Scale)
	{
		_translation = Translation;
		_rotation = Rotation;
		_scale = Scale;
	}

	void SceneEditor::KeyPressed(Event::Input::Info e)
	{

	}

	void SceneEditor::KeyReleased(Event::Input::Info e)
	{
		if (e.Input == Event::Input::Keyboard::Numpad0) UseCamera0();
		if (e.Input == Event::Input::Keyboard::Numpad1) UseCamera1();
		if (e.Input == Event::Input::Keyboard::Numpad2) UseCamera2();
		if (e.Input == Event::Input::Keyboard::Numpad3) UseCamera3();
	}

	void SceneEditor::UseCamera0() // Default View
	{
	/*	Camera->SetPosition(Vec3(0, 10, 20));
		Camera->SetRotation(Vec3(-0.464, 0, 0));
		CameraPivot->SetTransformationMatrix(Matrix());
		qX = qY = Quaternion();
		rotX = rotY = Quaternion();*/
		isPerspective = !isPerspective;
	}

	void SceneEditor::UseCamera1(bool invert) // Z Axis
	{
		if (!invert)
		{
			Camera->SetPosition(Vec3(0, 0, 20));
			Camera->SetRotation(Vec3(0, 0, 0));
		}
		else
		{
			Camera->SetPosition(Vec3(0, 0, -20));
			Camera->SetRotation(Vec3(0, 3.14, 0));
		}
		CameraPivot->SetTransformationMatrix(Matrix());
		qX = qY = Quaternion();
		rotX = rotY = Quaternion();
	}

	void SceneEditor::UseCamera2(bool invert) // X Axis
	{
		Camera->SetPosition(Vec3(0, 0, 20));
		Camera->SetRotation(Vec3(0, 0, 0));
		qX = rotX = rotY = Quaternion();

		if (!invert)
			qY = Quaternion(0.700909, 0, 0.71325, 0);
		else
			qY = Quaternion(0.700909, 0, -0.71325, 0);

		rotation = (rotY * qY) * (rotX * qX);
		Matrix m = rotation.ConvertToMatrix();
		CameraPivot->SetTransformationMatrix(m);
		rotX = rotX * qX;
		rotY = rotY * qY;
		rotation = Quaternion();
	}

	void SceneEditor::UseCamera3(bool invert) // Y Axis
	{
		Camera->SetRotation(Vec3(0, 0, 0));
		qY = rotX = rotY = Quaternion();
		if (!invert)
		{
			Camera->SetPosition(Vec3(0, 0, 20));
			qX = Quaternion(0.700909, -0.71325, 0, 0);
		}
		else
		{
			Camera->SetPosition(Vec3(0, 0, 20));
			qX = Quaternion(0.700909, 0.71325, 0, 0);
		}
		rotation = (rotY * qY) * (rotX * qX);
		Matrix m = rotation.ConvertToMatrix();
		CameraPivot->SetTransformationMatrix(m);
		rotX = rotX * qX;
		rotY = rotY * qY;
		rotation = Quaternion();
	}

	void SceneEditor::CreateGameObject(const std::string &name)
	{
		std::string NewName = name;
		if (NewName.size() == 0) NewName = "GameObject";
		
		SceneObject* s = sceneObjects->CreateGameObject(NewName);
		if (s == NULL) {
			fprintf(stderr, "[ERROR] Failed to create GameObject '%s'\n", NewName.c_str());
			return;
		}
		
		SelectSceneObject(s);

		// Re-enabled: what used to crash here was GameObjectHelper's
		// constructor calling LoadTexture("assets/gameobject.dds",
		// ShaderUsage::Diffuse) - an unloadable format passed with the wrong
		// enum, which threw std::length_error out of the texture loader.
		// Same bug that killed the directional light; fixed in
		// GameObjectHelper.cpp. SceneEditor::Update() only draws this icon
		// while the GameObject has no components of its own.
		std::shared_ptr<GameObjectHelper> h = std::make_shared<GameObjectHelper>((GameObject*)SelectedSceneObject->GetPTR());
		s->Helper = h;
		scene->Add(h);

		node_clicked = SelectedSceneObject->GetID();
	}

	// Reads a light's actual shadow setup into the Properties fields, so the
	// panel shows what the scene contains rather than a stale UI value.
	void SceneEditor::SeedShadowProperties(ILightComponent* light)
	{
		if (light == NULL || !light->IsCastingShadows()) return;
		PropertiesShadowBiasFactor = light->GetShadowBiasFactor();
		PropertiesShadowBiasUnits = light->GetShadowBiasUnits();
		PropertiesShadowMapSize = (int32)light->GetShadowWidth();
		PropertiesShadowNear = light->GetShadowNear();
		PropertiesShadowFar = light->GetShadowFar();
	}

	// Shadow acne is a function of bias, map resolution and how much depth
	// range the map has to cover, so all of it belongs in the UI rather than
	// baked into the Cast Shadows checkbox. Bias feeds glPolygonOffset and
	// takes effect on the next shadow pass; everything else changes the map
	// itself, so those return true and the caller re-runs EnableCastShadows()
	// (whose signature differs per light type).
	bool SceneEditor::ShowShadowProperties(ILightComponent* light, bool directional)
	{
		if (light == NULL || !light->IsCastingShadows()) return false;

		bool rebuild = false;

		f32 bias[2] = { PropertiesShadowBiasFactor, PropertiesShadowBiasUnits };
		if (ImGui::DragFloat2("Shadow Bias", bias, 0.05f, -32.f, 32.f, "%.2f"))
		{
			PropertiesShadowBiasFactor = bias[0];
			PropertiesShadowBiasUnits = bias[1];
			light->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("glPolygonOffset factor / units for the shadow map pass.\nRaise to remove acne; too much detaches contact shadows.");

		// Power-of-two only - a shadow map is a render target, and the sizes
		// worth offering are few enough that a combo beats a free-form int.
		static const int32 sizes[] = { 512, 1024, 2048, 4096 };
		int32 sizeIndex = 2;
		for (int32 i = 0; i < 4; i++)
			if (sizes[i] == PropertiesShadowMapSize) sizeIndex = i;
		if (ImGui::Combo("Map Size", &sizeIndex, "512\0" "1024\0" "2048\0" "4096\0"))
		{
			PropertiesShadowMapSize = sizes[sizeIndex];
			rebuild = true;
		}

		if (directional)
		{
			f32 range[2] = { PropertiesShadowNear, PropertiesShadowFar };
			if (ImGui::DragFloat2("Range", range, 0.05f, 0.001f, 10000.f, "%.2f"))
			{
				PropertiesShadowNear = range[0];
				PropertiesShadowFar = range[1];
				rebuild = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Near / far depth range the shadow map covers.\nA range far larger than the scene wastes precision and causes acne.");

			// PyrosShader.glsl reads uDirectionalDepthsMVP[0..3] /
			// uDirectionalShadowFar[0].xyzw - four cascades is the shader's
			// hard limit, not a taste one.
			if (ImGui::SliderInt("Cascades", &PropertiesShadowCascades, 1, 4))
				rebuild = true;
		}
		else
		{
			if (ImGui::DragFloat("Near", &PropertiesShadowNear, 0.01f, 0.001f, 1000.f, "%.3f"))
				rebuild = true;
		}

		return rebuild;
	}

	void SceneEditor::ShowProperties()
	{
		if (SelectedSceneObject != NULL)
		{
			ImGui::Spacing();
			ImGui::Indent(5.f);

			switch (SelectedSceneObject->GetType())
			{
				case SceneObjectTypes::GAMEOBJECT:
				{
                    ImGui::InputText("Name", &SelectedSceneObject->Name);
					sceneObjects->SetName(SelectedSceneObject->GetID(), SelectedSceneObject->Name);
					ImGui::DragFloat3("Position", (float *)&_translation, 0.1f, 0.0f, 0.0f);
					ImGui::DragFloat3("Rotation", (float *)&_rotation, 0.1f, 0.0f, 0.0f);
					ImGui::DragFloat3("Scale", (float *)&_scale, 0.1f, 0.0f, 0.0f);
				}
				break;
				case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
				{
					ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor);
					ImGui::DragFloat3("Direction", (float *)&PropertiesLightDirection, 0.01f, -1.0f, 1.0f);
					if (ImGui::Checkbox("Cast Shadows", &AddForm_cs))
						if (AddForm_cs)
						{
							DirectionalLight* l = (DirectionalLight*)SelectedSceneObject->GetPTR();
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, projection,
								PropertiesShadowNear, PropertiesShadowFar, PropertiesShadowCascades);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
						else {
							DirectionalLight* l = (DirectionalLight*)SelectedSceneObject->GetPTR();
							l->DisableCastShadows();
						}
					{
						DirectionalLight* l = (DirectionalLight*)SelectedSceneObject->GetPTR();
						if (ShowShadowProperties(l, true))
						{
							// EnableCastShadows() reconfigures a live shadow
							// map, releasing the old FBO/texture itself.
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, projection,
								PropertiesShadowNear, PropertiesShadowFar, PropertiesShadowCascades);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
					}
				}
				break;
				case SceneObjectTypes::POINTLIGHT_COMPONENT:
				{
					ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor);
					ImGui::DragFloat("Radius", (float *)&PropertiesLightRadius, 0.01f, 0.001f, 0.0f);
					if (ImGui::Checkbox("Cast Shadows", &AddForm_cs))
						if (AddForm_cs)
						{
							PointLight* l = (PointLight*)SelectedSceneObject->GetPTR();
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, PropertiesShadowNear);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
						else {
							PointLight* l = (PointLight*)SelectedSceneObject->GetPTR();
							l->DisableCastShadows();
						}
					{
						PointLight* l = (PointLight*)SelectedSceneObject->GetPTR();
						if (ShowShadowProperties(l, false))
						{
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, PropertiesShadowNear);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
					}
				}
				break;
				case SceneObjectTypes::SPOTLIGHT_COMPONENT:
				{
					ImGui::ColorEdit4("Color", (float*)&PropertiesLightColor);
					ImGui::DragFloat("Radius", (float *)&PropertiesLightRadius, 0.01f, 0.001f, 0.0f);
					ImGui::DragFloat3("Direction", (float *)&PropertiesLightDirection, 0.01f, -1.0f, 1.0f);
					ImGui::DragFloat("Outter Cone", (float *)&PropertiesLightOutterCone, 0.01f, 0.002f, 0.0f);
					ImGui::DragFloat("Inner Cone", (float *)&PropertiesLightInnerCone, 0.01f, 0.001f, 0.0f);
					if (ImGui::Checkbox("Cast Shadows", &AddForm_cs))
						if (AddForm_cs)
						{
							SpotLight* l = (SpotLight*)SelectedSceneObject->GetPTR();
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, PropertiesShadowNear);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
						else {
							SpotLight* l = (SpotLight*)SelectedSceneObject->GetPTR();
							l->DisableCastShadows();
						}
					{
						SpotLight* l = (SpotLight*)SelectedSceneObject->GetPTR();
						if (ShowShadowProperties(l, false))
						{
							l->EnableCastShadows(PropertiesShadowMapSize, PropertiesShadowMapSize, PropertiesShadowNear);
							l->SetShadowBias(PropertiesShadowBiasFactor, PropertiesShadowBiasUnits);
						}
					}
				}
				break;
			default:

				break;
			}
		}

	}

	#define MAX_INT32 2147483647
	#define MAX_F32 3.40282e+38
	void SceneEditor::ShowAddForm()
	{
		ImGui::OpenPopup("Add");
		if (ImGui::BeginPopupModal("Add", &showingAddFrom, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			editorDisabled = true;

			switch (showingAddFormType)
			{
			case 0:
				// Game Object
				break;
			case 1:
				// Cube
				ImGui::Text("Cube");
				ImGui::DragFloat("Width", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Depth", &AddForm_d, 0.01f, 0.001f, MAX_F32);
				break;
			case 2:
				// Sphere
				ImGui::Text("Sphere");
				ImGui::DragFloat("Radius", &AddForm_w, 0.001f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				ImGui::Checkbox("Half Sphere", &AddForm_hs);
				break;
			case 3:
				// Capsule
				ImGui::Text("Capsule");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Rings", &AddForm_r, 1, 1, 512);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				break;
			case 4:
				// Plane
				ImGui::Text("Plane");
				ImGui::DragFloat("Width", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				break;
			case 5:
				// Cone
				ImGui::Text("Cone");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				ImGui::Checkbox("Open Ended", &AddForm_oe);
				break;
			case 6:
				// Cylinder
				ImGui::Text("Cylinder");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Height", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				ImGui::Checkbox("Open Ended", &AddForm_oe);
				break;
			case 7:
				// Torus
				ImGui::Text("Torus");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Tube", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				break;
			case 8:
				// Torus
				ImGui::Text("Torus Knot");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Tube", &AddForm_h, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Segments W", &AddForm_sw, 1, 1, 512);
				ImGui::DragInt("Segments H", &AddForm_sh, 1, 1, 512);
				ImGui::DragFloat("P", &AddForm_p, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Q", &AddForm_q, 0.01f, 0.001f, MAX_F32);
				ImGui::DragInt("Height Scale", &AddForm_hscale, 1, 1, 512);
				break;
			case 9:
				// Model
				ImGui::Text("Import Model");
				// No resize(1024) here: it filled the string with 1024 NULs, so
				// the "did you pick a path" guard on the Create button below
				// (modelPath.size() != 0) passed even when nothing had been
				// entered, and CreateRenderingModel() was handed that garbage
				// as a filename. InputText (imgui_stdlib) grows the string to
				// fit what is actually typed, so an untouched field stays
				// empty and Create correctly refuses.
				// The browser this originally called - restored. Read-only
				// field plus a "..." button that opens a real directory
				// listing filtered to the extension.
				ImGui::FilePath("Path", "", "p3dm", &AddForm_modelPath, 1024, &showDir);
				break;
			case 10:
				// Directional Light
				ImGui::Text("Directional Light");
				ImGui::DragFloat3("Direction", (float *)&AddForm_dir, 0.01f, -1.0f, 1.0f);
				ImGui::ColorEdit4("Color", (float*)&AddForm_color);
				break;
			case 11:
				// Point Light
				ImGui::Text("Point Light");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::ColorEdit4("Color", (float*)&AddForm_color);
				break;
			case 12:
				// Spot Light
				ImGui::Text("Spot Light");
				ImGui::DragFloat("Radius", &AddForm_w, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat3("Direction", (float *)&AddForm_dir, 0.01f, -1.0f, 1.0f);
				ImGui::ColorEdit4("Color", (float*)&AddForm_color);
				ImGui::DragFloat("Outter Cone", &AddForm_oc, 0.01f, 0.001f, MAX_F32);
				ImGui::DragFloat("Inner Cone", &AddForm_ic, 0.01f, 0.001f, MAX_F32);
				break;
			default:
				break;
			}

			if (showingAddFormType < 10 && showingAddFormType > 0)
			{
				ImGui::Checkbox("Smooth Normals", &AddForm_sn);
				ImGui::Checkbox("Flip Normals", &AddForm_fn);
			}

			// The selection has to be a GameObject, not merely non-NULL:
			// AddFormSubmit() casts SelectedSceneObject->GetPTR() straight to
			// GameObject* and calls Add() on it, so offering "use the current
			// selection as the parent" while a light or a rendering component
			// is selected handed that cast a component pointer. Forcing a new
			// GameObject in every other case is what the NULL branch already
			// did.
			if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::GAMEOBJECT && showingAddFormType > 0)
			{
				ImGui::Checkbox("Create GameObject", &AddForm_cgo);
			}
			else AddForm_cgo = true;

			if (showingAddFormType == 0)
				AddForm_cgo = true;

            if (AddForm_cgo)
            {
                ImGui::InputText("GO Name", &AddForm_go);
            }

            if (ImGui::Button("Create"))
			{
				if (showingAddFormType != 9 || (showingAddFormType == 9 && AddForm_modelPath.size() != 0))
				{
                    if (AddForm_go.empty() && AddForm_cgo)
                        AddForm_go = "GameObject";
										AddFormSubmit();
					showingAddFrom = false;
					ImGui::CloseCurrentPopup();
					editorDisabled = false;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) { showingAddFrom = false; ImGui::CloseCurrentPopup(); editorDisabled = false; }
			ImGui::Spacing();

			ImGui::EndPopup();
		}
	}

	void SceneEditor::AddFormSubmit()
	{
		if (AddForm_cgo)
			CreateGameObject(AddForm_go);

		switch (showingAddFormType)
		{
		case 0:
			// Game Object

			break;
		case 1:
			// Cube
			sceneObjects->CreateRenderingCube((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_h, AddForm_d, AddForm_sn, AddForm_fn);
			break;
		case 2:
			// Sphere
			sceneObjects->CreateRenderingSphere((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_sw, AddForm_sh, AddForm_sn, AddForm_hs, AddForm_fn);
			break;
		case 3:
			// Capsule
			sceneObjects->CreateRenderingCapsule((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_h, AddForm_r, AddForm_sw, AddForm_sh, AddForm_sn, AddForm_fn);
			break;
		case 4:
			// Plane
			sceneObjects->CreateRenderingPlane((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_h, AddForm_sn, AddForm_fn);
			break;
		case 5:
			// Cone
			sceneObjects->CreateRenderingCone((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_h, AddForm_sw, AddForm_sh, AddForm_oe, AddForm_sn, AddForm_fn);
			break;
		case 6:
			// Cylinder
			sceneObjects->CreateRenderingCylinder((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_h, AddForm_sw, AddForm_sh, AddForm_oe, AddForm_sn, AddForm_fn);
			break;
		case 7:
			// Torus
			sceneObjects->CreateRenderingTorus((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_h, AddForm_sw, AddForm_sh, AddForm_sn, AddForm_fn);
			break;
		case 8:
			// Torus Knot
			sceneObjects->CreateRenderingTorusKnot((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_h, AddForm_sw, AddForm_sh, AddForm_p, AddForm_q, AddForm_sn, AddForm_fn);
			break;
		case 9:
			// Model
			sceneObjects->CreateRenderingModel((GameObject*)SelectedSceneObject->GetPTR(), AddForm_modelPath);
			break;
		case 10:
		{
			// Directional Light
			SceneObject* s = sceneObjects->CreateDirectionalLight((GameObject*)SelectedSceneObject->GetPTR(), AddForm_dir, AddForm_color);
			// Create Light Helper
			std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>((GameObject*)SelectedSceneObject->GetPTR());
			s->Helper = h;
			scene->Add(h);
		}
			break;
		case 11:
		{
			// Point Light
			SceneObject* s = sceneObjects->CreatePointLight((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_color);
			// Create Light Helper
			std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>((GameObject*)SelectedSceneObject->GetPTR());
			s->Helper = h;
			scene->Add(h);
		}
			break;
		case 12:
		{
			// Spot Light
			SceneObject* s = sceneObjects->CreateSpotLight((GameObject*)SelectedSceneObject->GetPTR(), AddForm_w, AddForm_dir, AddForm_oc, AddForm_ic, AddForm_color);
			// Create Light Helper
			std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>((GameObject*)SelectedSceneObject->GetPTR());
			s->Helper = h;
			scene->Add(h);
		}
			break;
		default:
			break;
		}
	}

	void SceneEditor::ShowMenubarOptions()
	{
		if (ImGui::BeginMenu("Scene", ""))
		{
			if (ImGui::MenuItem("New Scene", ""))
				NewScene();

			if (ImGui::MenuItem("Open Scene", ""))
			{
				showingSceneDialog = true;
				sceneDialogIsSave = false;
				sceneDialogPath = scenePath;
				sceneDialogError.clear();
			}

			// Saves straight over the current file once there is one; the
			// first save has nowhere to go, so it falls through to Save As.
			if (ImGui::MenuItem("Save Scene", ""))
			{
				if (scenePath.size() > 0)
					SaveSceneToFile(scenePath);
				else
				{
					showingSceneDialog = true;
					sceneDialogIsSave = true;
					sceneDialogPath = "scene.json";
					sceneDialogError.clear();
				}
			}

			if (ImGui::MenuItem("Save Scene As...", ""))
			{
				showingSceneDialog = true;
				sceneDialogIsSave = true;
				sceneDialogPath = scenePath.size() > 0 ? scenePath : std::string("scene.json");
				sceneDialogError.clear();
			}
			ImGui::Separator();

			ShowRightMenu();

			ImGui::EndMenu();
		}
	}

	void SceneEditor::ShowRightMenu()
	{
		if (ImGui::BeginMenu("Add", ""))
		{
            if (ImGui::MenuItem("Game Object", "")) { showingAddFrom = true; showingAddFormType = 0; AddForm_go = "GameObject"; }
			ImGui::Separator();
			if (ImGui::BeginMenu("Mesh", ""))
			{
				if (ImGui::BeginMenu("Primitives"))
				{
                    if (ImGui::MenuItem("Cube", "")) { showingAddFrom = true; showingAddFormType = 1; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_d = 1.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Sphere", "")) { showingAddFrom = true; showingAddFormType = 2; AddForm_w = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_hs = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Capsule", "")) { showingAddFrom = true; showingAddFormType = 3; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_r = 8.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Plane", "")) { showingAddFrom = true; showingAddFormType = 4; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Cone", "")) { showingAddFrom = true; showingAddFormType = 5; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_oe = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Cylinder", "")) { showingAddFrom = true; showingAddFormType = 6; AddForm_w = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_oe = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                    if (ImGui::MenuItem("Torus", "")) { showingAddFrom = true; showingAddFormType = 7; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
					if (ImGui::MenuItem("Torus Knot", "")) { showingAddFrom = true; showingAddFormType = 8; AddForm_w = 1.0; AddForm_h = 1.0; AddForm_sw = 8.0; AddForm_sh = 6.0; AddForm_p = 1.0; AddForm_q = 1.0; AddForm_hscale = 1.0; AddForm_cgo = false; AddForm_go.clear(); AddForm_sn = false; AddForm_fn = false; }
					ImGui::EndMenu();
				}
				ImGui::Separator();
                if (ImGui::MenuItem("Import Model")) { showingAddFrom = true; showingAddFormType = 9; AddForm_modelPath.clear(); AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; AddForm_cgo = false; }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Lights", ""))
			{
                if (ImGui::MenuItem("Directional", "")) { showingAddFrom = true; showingAddFormType = 10; AddForm_color = Vec4(1, 1, 1, 1); AddForm_dir = Vec3(0, -1, 0); AddForm_cs = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                if (ImGui::MenuItem("Point", "")) { showingAddFrom = true; showingAddFormType = 11; AddForm_w = 10.0; AddForm_color = Vec4(1, 1, 1, 1); AddForm_cs = false; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
                if (ImGui::MenuItem("Spot", "")) { showingAddFrom = true; showingAddFormType = 12; AddForm_w = 10.0; AddForm_color = Vec4(1, 1, 1, 1); AddForm_dir = Vec3(0, -1, 0); AddForm_cs = false; AddForm_oc = 45.f; AddForm_ic = 30.f; AddForm_cgo = false; AddForm_go = ""; AddForm_sn = false; AddForm_fn = false; }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Physics", ""))
			{
				if (ImGui::MenuItem("Box", "")) {}
				if (ImGui::MenuItem("Capsule", "")) {}
				if (ImGui::MenuItem("Cone", "")) {}
				if (ImGui::MenuItem("Convex Hull", "")) {}
				if (ImGui::MenuItem("Convex Triangle Mesh", "")) {}
				if (ImGui::MenuItem("Cylinder", "")) {}
				if (ImGui::MenuItem("Multiple Sphere", "")) {}
				if (ImGui::MenuItem("Sphere", "")) {}
				if (ImGui::MenuItem("Static Plane", "")) {}
				if (ImGui::MenuItem("Triangle Mesh", "")) {}
				if (ImGui::MenuItem("Vehicle", "")) {}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
	}

	// The icon sheet as an ImTextureID. Every ImageButton below used to pass
	// icons->GetBindID() straight through - a raw GL texture name. ImGui's
	// Vulkan backend reinterprets ImTextureID as a VkDescriptorSet, so a small
	// integer became a garbage descriptor and MoltenVK crashed inside
	// vkQueueSubmit -> bindDescriptorSets. Only reachable with something
	// selected, which is why the Vulkan editor looked fine until then.
	ImTextureID SceneEditor::IconsTextureID() const
	{
		if (icons == NULL) return (ImTextureID)0;
		return (ImTextureID)GetActiveRenderDevice().GetImGuiTextureID(icons->GetBindID(), icons->GetTextureType());
	}

	void SceneEditor::ShowTools()
	{
		if (SelectedSceneObject != NULL)
		{
			// A backend that cannot hand ImGui a texture handle returns NULL;
			// drawing the buttons anyway would submit an invalid descriptor.
			const ImTextureID iconsTex = IconsTextureID();
			if (iconsTex == (ImTextureID)0)
			{
				ImGui::TextDisabled("(tool icons unavailable on this backend)");
				return;
			}
			ImGui::Spacing();
			switch (SelectedSceneObject->GetType())
			{
			case SceneObjectTypes::GAMEOBJECT:
			{
				f32 nrItems = 9;
				f32 bid = 0;
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    (GizmoInUse == GizmoFunction::TRANSLATION ? ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0) : ImVec2(bid * 16.f / (nrItems*16.f), 0)),
                    (GizmoInUse == GizmoFunction::TRANSLATION ? ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1) : ImVec2((bid + 1) * 16.f / (nrItems*16.f), 1)))
                    )
				{
					UseTranslationManipulator();
				}
				ImGui::PopID();
				bid = 4;
				ImGui::SameLine();
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    (GizmoInUse == GizmoFunction::ROTATION ? ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0) : ImVec2(bid * 16.f / (nrItems*16.f), 0)),
                    (GizmoInUse == GizmoFunction::ROTATION ? ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1) : ImVec2((bid + 1) * 16.f / (nrItems*16.f), 1)))
                    )
				{
					UseRotationManipulator();
				}
				ImGui::PopID();
				bid = 2;
				ImGui::SameLine();
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    (GizmoInUse == GizmoFunction::SCALE ? ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0) : ImVec2(bid * 16.f / (nrItems*16.f), 0)),
                    (GizmoInUse == GizmoFunction::SCALE ? ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1) : ImVec2((bid + 1) * 16.f / (nrItems*16.f), 1)))
                    )
				{
					UseScaleManipulator();
				}
				ImGui::PopID();
				bid = 6;
				ImGui::SameLine();
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    (localTransform || GizmoInUse == GizmoFunction::SCALE ? ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0) : ImVec2(bid * 16.f / (nrItems*16.f), 0)),
                    (localTransform || GizmoInUse == GizmoFunction::SCALE ? ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1) : ImVec2((bid + 1) * 16.f / (nrItems*16.f), 1)))
                    )
				{
					if (localTransform) UseGlobalManipulator();
					else UseLocalManipulator();
				}
				ImGui::PopID();
				bid = 7;
				ImGui::SameLine();
				ImGui::PushID(bid);
                if (ImGui::ImageButton(
                    "##img_btn",
                    iconsTex,
                    ImVec2(16, 16),
                    ImVec2((bid + 1) * 16.f / (nrItems*16.f), 0),ImVec2((bid + 2) * 16.f / (nrItems*16.f), 1))
                    )
				{
					// Drop the highlight component first: it is attached to
					// the GameObject about to be destroyed, so DeselectMesh()
					// afterwards would call GetOwner()->Remove() through a
					// freed owner.
					DeselectMesh();
					sceneObjects->DestroySceneObject(SelectedSceneObject->GetID());
					DeselectSceneObject();
					// The tree's multi-selection holds ids, not pointers, and
					// Show() looks them up with map::at() - a stale id there
					// throws std::out_of_range on the next ctrl-click.
					selection.clear();
					node_clicked = -1;
				}
				ImGui::PopID();
			}
			break;
			default:

				break;
			}
		}
	}
