//============================================================================
// Name        : MaterialEditor.cpp
// Description : Material editor window implementation
//============================================================================

#include "MaterialEditor.h"
#include "../CodeEditorDocument.h"
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <fstream>

using namespace p3d;

static std::string ResolveTexturePath(const std::string& relOrAbs) {
    if (relOrAbs.empty()) return "";
    if (!relOrAbs[0] == '/' && !relOrAbs[1] == ':') {
        return "assets/textures/" + relOrAbs;
    }
    return relOrAbs;
}

MaterialEditor::MaterialEditor(bool* open) : Open(open), dirty(false) {}

MaterialEditor::~MaterialEditor() {
    Shutdown();
}

void MaterialEditor::Shutdown() {
    if (codeDoc) {
        delete codeDoc;
        codeDoc = nullptr;
    }

    // Clean up preview textures in nodes
    for (auto& node : nodes) {
        delete node.previewTex;
        node.previewTex = nullptr;
    }
}

std::shared_ptr<IMaterial> MaterialEditor::CreateNewMaterial(MaterialEditKind kind) {
    // Clean up existing preview textures
    for (auto& node : nodes) {
        delete node.previewTex;
        node.previewTex = nullptr;
    }

    currentMaterial.reset();
    editKind = kind;
    dirty = true;

    if (kind == MaterialEditKind::Generic) {
        auto mat = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color | ShaderUsage::Diffuse);
        mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
        currentMaterial = mat;
    } else {
        auto mat = std::make_shared<CustomShaderMaterial>("");
        currentMaterial = mat;
    }

    materialName = "NewMaterial";
    nodes.clear();
    connections.clear();
    if (kind == MaterialEditKind::Generic) {
        CreateNode(MaterialNode::Color, "Base Color", ImVec2(100.f, 150.f));
        CreateNode(MaterialNode::Float, "Metallic", ImVec2(100.f, 300.f));
        CreateNode(MaterialNode::Float, "Roughness", ImVec2(100.f, 450.f));
        CreateNode(MaterialNode::Output, "PBR Output", ImVec2(600.f, 300.f));
    }

    return currentMaterial;
}

std::shared_ptr<IMaterial> MaterialEditor::LoadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return nullptr;

    json j;
    try {
        f >> j;
    } catch (...) {
        return nullptr;
    }

    currentMaterial = DeserializeMaterial(j);
    if (!currentMaterial) return nullptr;

    materialName = j.value("name", path.substr(path.find_last_of("/\\") + 1));
    dirty = false;

    editKind = dynamic_cast<GenericShaderMaterial*>(currentMaterial.get()) ? MaterialEditKind::Generic : MaterialEditKind::Custom;

    nodes.clear();
    connections.clear();
    if (editKind == MaterialEditKind::Generic) {
        CreateNode(MaterialNode::Color, "Base Color", ImVec2(100.f, 150.f));
        CreateNode(MaterialNode::Float, "Metallic", ImVec2(100.f, 300.f));
        CreateNode(MaterialNode::Float, "Roughness", ImVec2(100.f, 450.f));
        CreateNode(MaterialNode::Output, "PBR Output", ImVec2(600.f, 300.f));
    }

    return currentMaterial;
}

bool MaterialEditor::SaveToFile(const std::string& path) {
    if (!currentMaterial) return false;

    json j = SerializeMaterial(currentMaterial.get());
    j["name"] = materialName;

    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << j.dump(2);
    dirty = false;
    return true;
}

void MaterialEditor::EditMaterial(std::shared_ptr<IMaterial> mat) {
    currentMaterial = mat;
    editKind = dynamic_cast<GenericShaderMaterial*>(mat.get()) ? MaterialEditKind::Generic : MaterialEditKind::Custom;
    dirty = false;
    materialName = "SceneMaterial";

    nodes.clear();
    connections.clear();
    if (editKind == MaterialEditKind::Generic) {
        CreateNode(MaterialNode::Color, "Base Color", ImVec2(100.f, 150.f));
        CreateNode(MaterialNode::Float, "Metallic", ImVec2(100.f, 300.f));
        CreateNode(MaterialNode::Float, "Roughness", ImVec2(100.f, 450.f));
        CreateNode(MaterialNode::Output, "PBR Output", ImVec2(600.f, 300.f));
    }
}

void MaterialEditor::SetProjectRoot(const std::string& root) {
    projectRoot = root;
}

void MaterialEditor::Show() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    if (dirty) flags |= ImGuiWindowFlags_UnsavedDocument;

    if (!ImGui::Begin(Name.c_str(), Open, flags)) {
        ImGui::End();
        return;
    }

    DrawToolbar();
    ImGui::Separator();

    if (!currentMaterial) {
        ImGui::TextDisabled("No material loaded.");
        ImGui::Spacing();
        if (ImGui::Button("New Generic Material")) {
            CreateNewMaterial(MaterialEditKind::Generic);
        }
        ImGui::SameLine();
        if (ImGui::Button("New Custom Shader Material")) {
            CreateNewMaterial(MaterialEditKind::Custom);
        }
    } else {
        switch (editMode) {
        case MaterialEditMode::Inspector:
            DrawInspectorTab();
            break;
        case MaterialEditMode::Text:
            DrawTextEditorTab();
            break;
        case MaterialEditMode::NodeGraph:
            DrawNodeGraphTab();
            break;
        }
    }

    ImGui::End();
}

void MaterialEditor::DrawToolbar() {
    if (!currentMaterial) return;

    ImGui::SetNextItemWidth(180.f);
    ImGui::InputText("Name", &materialName);
    dirty = true;
    ImGui::SameLine();

    static const char* kindLabels[] = { "Generic Shader", "Custom Shader" };
    int currentKind = (int)editKind;
    if (ImGui::Combo("Type", &currentKind, kindLabels, IM_ARRAYSIZE(kindLabels))) {
        MaterialEditKind newKind = (MaterialEditKind)currentKind;
        if (newKind != editKind) {
            CreateNewMaterial(newKind);
        }
    }

    ImGui::SameLine();
    static const char* modeLabels[] = { "Inspector", "Text", "Node Graph" };
    int currentMode = (int)editMode;
    if (ImGui::Combo("Edit Mode", &currentMode, modeLabels, IM_ARRAYSIZE(modeLabels))) {
        editMode = (MaterialEditMode)currentMode;
        if (editMode == MaterialEditMode::Text && !codeDoc) {
            codeDoc = new CodeEditorDocument();
            codeDoc->SetupForLua();
            codeDoc->editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
        }
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 200.f);
    if (ImGui::Button("Save...")) {
        dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load...")) {}
}

void MaterialEditor::DrawInspectorTab() {
    if (!currentMaterial) return;

    if (editKind == MaterialEditKind::Generic) {
        DrawGenericMaterialInspector();
    } else {
        DrawCustomMaterialInspector();
    }

    ImGui::Separator();
    DrawCommonMaterialSettings(currentMaterial.get());
}

void MaterialEditor::DrawGenericMaterialInspector() {
    auto* gm = dynamic_cast<GenericShaderMaterial*>(currentMaterial.get());
    if (!gm) return;

    ImGui::Text("Shader Options");
    uint32 opts = gm->GetOptions();

    #define CHK(flag, label) do { bool v = (opts & ShaderUsage::flag); \
        if (ImGui::Checkbox(label, &v)) { \
            dirty = true; \
            if (v) opts |= ShaderUsage::flag; else opts &= ~ShaderUsage::flag; \
        } ImGui::SameLine(); } while(0)

    CHK(Color, "Color");
    CHK(Texture, "Texture");
    CHK(PBR, "PBR");
    CHK(PBRMap, "PBR Map");
    ImGui::Separator();
    CHK(BumpMapping, "Normal Map");
    CHK(SpecularMap, "Specular Map");
    CHK(EnvMap, "Env Map");
    CHK(Refraction, "Refract");
    ImGui::Separator();
    CHK(CastShadows, "Cast Shadows");
    CHK(DirectionalShadow, "Dir Shadow");
    CHK(PointShadow, "Point Shadow");
    CHK(SpotShadow, "Spot Shadow");
    ImGui::Separator();
    CHK(AlphaTest, "Alpha Cutout");
    CHK(VertexWind, "Vertex Wind");

    #undef CHK

    if (opts != gm->GetOptions()) {
        dirty = true;
    }

    ImGui::Spacing();
    ImGui::Text("Colors");

    Vec4 color = gm->GetColor();
    float c[4] = { color.x, color.y, color.z, color.w };
    if (ImGui::ColorEdit4("Base Color", c, ImGuiColorEditFlags_AlphaBar)) {
        gm->SetColor(Vec4(c[0], c[1], c[2], c[3]));
        dirty = true;
    }

    Vec4 spec = gm->GetSpecular();
    float s[4] = { spec.x, spec.y, spec.z, spec.w };
    if (ImGui::ColorEdit4("Specular Color", s, ImGuiColorEditFlags_AlphaBar)) {
        gm->SetSpecular(Vec4(s[0], s[1], s[2], s[3]));
        dirty = true;
    }

    ImGui::Spacing();
    ImGui::Text("Textures");

    DrawTextureSlot("Color Map", gm->GetColorMap());
    DrawTextureSlot("Normal Map", gm->GetNormalMap());
    DrawTextureSlot("Specular Map", gm->GetSpecularMap());
    DrawTextureSlot("Env Map", gm->GetEnvMap());
    DrawTextureSlot("Metallic/Roughness Map", gm->GetMetallicRoughnessMap());

    ImGui::Spacing();
    ImGui::Text("PBR");

    float metallic = gm->GetMetallic();
    if (ImGui::SliderFloat("Metallic", &metallic, 0.f, 1.f)) {
        gm->SetMetallic(metallic);
        dirty = true;
    }

    float roughness = gm->GetRoughness();
    if (ImGui::SliderFloat("Roughness", &roughness, 0.f, 1.f)) {
        gm->SetRoughness(roughness);
        dirty = true;
    }

    bool ssr = gm->IsSSREnabled();
    if (ImGui::Checkbox("Screen Space Reflections", &ssr)) {
        gm->SetSSREnabled(ssr);
        dirty = true;
    }

    float alphaCutoff = gm->GetAlphaCutoff();
    if (ImGui::SliderFloat("Alpha Cutoff", &alphaCutoff, 0.f, 1.f)) {
        gm->SetAlphaCutoff(alphaCutoff);
        dirty = true;
    }

    ImGui::Spacing();
    ImGui::Text("Lighting");

    float shininess = gm->GetShininess();
    if (ImGui::SliderFloat("Shininess", &shininess, 0.f, 128.f)) {
        gm->SetShininess(shininess);
        dirty = true;
    }

    float reflectivity = gm->GetReflectivity();
    if (ImGui::SliderFloat("Reflectivity", &reflectivity, 0.f, 1.f)) {
        gm->SetReflectivity(reflectivity);
        dirty = true;
    }

    float dispHeight = gm->GetDisplacementHeight();
    if (ImGui::DragFloat("Displacement Height", &dispHeight, 0.01f, -1.f, 5.f)) {
        gm->SetDisplacementHeight(dispHeight);
        dirty = true;
    }

    const Vec4& wind = gm->GetWind();
    float wStr = wind.x;
    if (ImGui::DragFloat("Wind Strength", &wStr, 0.1f, 0.f, 5.f)) {
        dirty = true;
    }
}

void MaterialEditor::DrawCustomMaterialInspector() {
    auto* cm = dynamic_cast<CustomShaderMaterial*>(currentMaterial.get());
    if (!cm) return;

    ImGui::Text("Shader Source");
    static std::string shaderPath;
    shaderPath = cm->GetShaderFile();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##shaderPath", &shaderPath)) {
        dirty = true;
    }

    ImGui::Spacing();
    ImGui::BulletText("Use Text or Node Graph mode to edit shader code.");
}

void MaterialEditor::DrawCommonMaterialSettings(IMaterial* mat) {
    if (!mat) return;

    ImGui::Text("Rendering");

    float opacity = mat->GetOpacity();
    if (ImGui::SliderFloat("Opacity", &opacity, 0.f, 1.f)) {
        mat->SetOpacity(opacity);
        dirty = true;
    }

    bool transparent = mat->IsTransparent();
    if (ImGui::Checkbox("Transparent", &transparent)) {
        mat->SetTransparencyFlag(transparent);
        dirty = true;
    }

    bool blending = mat->IsBlendingEnabled();
    if (ImGui::Checkbox("Blending", &blending)) {
        if (blending) mat->EnableBlending(); else mat->DisableBlending();
        dirty = true;
    }

    bool depthTest = mat->IsDepthTesting();
    if (ImGui::Checkbox("Depth Test", &depthTest)) {
        if (depthTest) mat->EnableDepthTest(); else mat->DisableDepthTest();
        dirty = true;
    }

    bool depthWrite = mat->IsDepthWritting();
    if (ImGui::Checkbox("Depth Write", &depthWrite)) {
        if (depthWrite) mat->EnableDepthWrite(); else mat->DisableDepthWrite();
        dirty = true;
    }

    uint32 cullFace = mat->GetCullFace();
    static const char* cullLabels[] = { "None", "Front", "Back" };
    int cullIdx = (int)cullFace;
    if (ImGui::Combo("Cull Face", &cullIdx, cullLabels, IM_ARRAYSIZE(cullLabels))) {
        mat->SetCullFace((uint32)cullIdx);
        dirty = true;
    }

    bool wireframe = mat->IsWireFrame();
    if (ImGui::Checkbox("Wireframe", &wireframe)) {
        if (wireframe) mat->StartRenderWireFrame(); else mat->StopRenderWireFrame();
        dirty = true;
    }

    bool castShadows = mat->IsCastingShadows();
    if (ImGui::Checkbox("Cast Shadows", &castShadows)) {
        if (castShadows) mat->EnableCastingShadows(); else mat->DisableCastingShadows();
        dirty = true;
    }
}

void MaterialEditor::DrawTextureSlot(const char* label, Texture* tex) {
    ImGui::Text("%s", label);
    if (ImGui::Button("Browse...")) {}
    ImGui::SameLine();
    if (tex) {
        ImGui::TextDisabled("(loaded)");
        ImVec2 slotSize = ImVec2(ImGui::GetContentRegionAvail().x, 64.f);
        void* tid = GetActiveRenderDevice().GetImGuiTextureID(tex->GetBindID(), tex->GetTextureType());
        if (tid) {
            ImGui::Image((ImTextureID)tid, slotSize);
        } else {
            ImGui::TextDisabled("[texture preview unavailable]");
        }
    } else {
        ImGui::TextDisabled("(none)");
    }
}

void MaterialEditor::DrawTextEditorTab() {
    if (!codeDoc) {
        codeDoc = new CodeEditorDocument();
        codeDoc->SetupForLua();
        codeDoc->editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    }

    if (ImGui::Button("Save Shader")) {
        dirty = true;
    }

    ImGui::Separator();

    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    codeDoc->editor.SetHandleKeyboardInputs(focused);
    ImVec2 avail = ImGui::GetContentRegionAvail();
    codeDoc->editor.Render("##material_shader_editor", avail, true);
}

void MaterialEditor::DrawNodeGraphTab() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 10 || avail.y < 10) return;

    bool canvasHovered = false;
    ImGuiIO& io = ImGui::GetIO();
    static const float pinRadius = 5.f;
    static const float pinSpacing = 24.f;

    // Draw background grid first
    ImDrawList* bgDrawList = ImGui::GetWindowDrawList();
    ImVec2 canvasScreenPos = ImGui::GetCursorScreenPos();
    ImVec2 gridSize(40.f * graphZoom, 40.f * graphZoom);
    int startX = (int)((-graphOffset.x / graphZoom));
    int startY = (int)((-graphOffset.y / graphZoom));
    int endX = startX + (int)(avail.x / gridSize.x) + 2;
    int endY = startY + (int)(avail.y / gridSize.y) + 2;

    for (int x = startX; x <= endX; x++) {
        float sx = canvasScreenPos.x + graphOffset.x + x * gridSize.x;
        bgDrawList->AddLine(ImVec2(sx, canvasScreenPos.y), ImVec2(sx, canvasScreenPos.y + avail.y),
            ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.15f, 0.8f)), 1.f);
    }
    for (int y = startY; y <= endY; y++) {
        float sy = canvasScreenPos.y + graphOffset.y + y * gridSize.y;
        bgDrawList->AddLine(ImVec2(canvasScreenPos.x, sy), ImVec2(canvasScreenPos.x + avail.x, sy),
            ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.15f, 0.8f)), 1.f);
    }

    // Collect pin positions during node rendering
    std::vector<PinPosition> allPins;

    // Draw nodes as child windows positioned in screen space
    for (auto& node : nodes) {
        ImVec2 screenPos(node.pos.x * graphZoom + canvasScreenPos.x + graphOffset.x,
                         node.pos.y * graphZoom + canvasScreenPos.y + graphOffset.y);

        char title[128];
        snprintf(title, sizeof(title), "%s##node_%u", node.name.c_str(), node.id);

        ImVec4 bgColor(0.15f, 0.15f, 0.18f, 0.95f);
        switch (node.type) {
            case MaterialNode::Color: bgColor = ImVec4(0.3f, 0.2f, 0.4f, 0.95f); break;
            case MaterialNode::Texture: bgColor = ImVec4(0.2f, 0.35f, 0.2f, 0.95f); break;
            case MaterialNode::Float: bgColor = ImVec4(0.2f, 0.25f, 0.4f, 0.95f); break;
            case MaterialNode::Output: bgColor = ImVec4(0.4f, 0.3f, 0.15f, 0.95f); break;
            default: break;
        }

        ImGui::SetNextWindowPos(screenPos);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(bgColor.x * 0.7f, bgColor.y * 0.7f, bgColor.z * 0.7f, 1.f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking;

        if (!ImGui::Begin(title, nullptr, flags)) {
            ImGui::End();
            ImGui::PopStyleColor(2);
            continue;
        }

        bool nodeHovered = ImGui::IsWindowHovered();
        if (nodeHovered) canvasHovered = true;

        switch (node.type) {
            case MaterialNode::Color: {
                float color[4] = {1, 1, 1, 1};
                if (!node.userData.empty()) {
                    sscanf(node.userData.c_str(), "%f,%f,%f,%f", &color[0], &color[1], &color[2], &color[3]);
                }
                ImGui::ColorEdit4("Value", color);
                node.userData = std::to_string(color[0]) + "," + std::to_string(color[1]) + "," +
                                std::to_string(color[2]) + "," + std::to_string(color[3]);

                // Color preview swatch below editor - height matches pin count spacing
                ImVec4 colVec(color[0], color[1], color[2], color[3]);
                float swatchWidth = ImGui::GetContentRegionAvail().x;
                int outPins = MaterialNode::GetOutputPinCount(MaterialNode::Color);
                float swatchHeight = (outPins - 1) * pinSpacing + 20.f;
                ImVec2 swatchPos = ImGui::GetCursorScreenPos();
                bgDrawList->AddRectFilled(swatchPos,
                    ImVec2(swatchPos.x + swatchWidth, swatchPos.y + swatchHeight),
                    ImGui::GetColorU32(colVec));
                break;
            }
            case MaterialNode::Float: {
                float val = 0.5f;
                if (!node.userData.empty()) {
                    sscanf(node.userData.c_str(), "%f", &val);
                }
                ImGui::SliderFloat("Value", &val, 0.f, 1.f);
                node.userData = std::to_string(val);

                // Visual bar preview
                float barWidth = ImGui::GetContentRegionAvail().x;
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                bgDrawList->AddRectFilled(cursor,
                    ImVec2(cursor.x + barWidth * val, cursor.y + 8.f),
                    ImGui::GetColorU32(ImVec4(0.5f, 0.6f, 1.f, 1.f)));
                break;
            }
            case MaterialNode::Texture: {
                if (ImGui::Button("Load...")) {
                    node.texturePath = projectRoot + "assets/textures/";
                }

                // Show texture preview if loaded
                if (!node.texturePath.empty()) {
                    ImGui::TextDisabled("%s", node.texturePath.c_str());

                    if (!node.previewTex) {
                        node.previewTex = new p3d::Texture();
                        node.previewTex->LoadTexture(node.texturePath, TextureType::Texture);
                    }

                    ImVec2 slotSize(ImGui::GetContentRegionAvail().x, 64.f);
                    void* tid = GetActiveRenderDevice().GetImGuiTextureID(node.previewTex->GetBindID(), node.previewTex->GetTextureType());
                    if (tid) {
                        ImGui::Image((ImTextureID)tid, slotSize);
                    } else {
                        ImGui::TextDisabled("[preview unavailable]");
                    }
                }

                // Accept drag-and-drop of texture files onto this node
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_FILE")) {
                        const char* path = reinterpret_cast<const char*>(payload->Data);
                        node.texturePath = std::string(path);
                        delete node.previewTex;
                        node.previewTex = new p3d::Texture();
                        node.previewTex->LoadTexture(node.texturePath, TextureType::Texture);
                    }
                    ImGui::EndDragDropTarget();
                }
                break;
            }
            case MaterialNode::Output: {
                // Output node shows final material preview (simplified)
                ImGui::Text("Material Preview");

                // Compute albedo from connected input pin 0, or default to white
                Vec4 albedo(1.f, 1.f, 1.f, 1.f);
                for (const auto& conn : connections) {
                    if (conn.toNode == node.id && conn.toPinIndex == 0) { // Albedo pin
                        for (const auto& srcNode : nodes) {
                            if (srcNode.id == conn.fromNode) {
                                albedo = srcNode.ComputePreviewValue(srcNode, nodes, connections);
                                break;
                            }
                        }
                    }
                }

                // Preview box showing computed albedo color - sized for 6 input pins, indented to not overlap labels
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float previewSize = fmin(avail.x, 80.f);
                int inPins = MaterialNode::GetInputPinCount(MaterialNode::Output);
                float previewHeight = (inPins - 1) * pinSpacing + 20.f;
                ImVec2 basePos = ImGui::GetCursorScreenPos();
                ImVec2 previewPos(basePos.x + 10.f, basePos.y);
                bgDrawList->AddRectFilled(previewPos,
                    ImVec2(previewPos.x + previewSize, previewPos.y + previewHeight),
                    ImGui::GetColorU32(ImVec4(albedo.x, albedo.y, albedo.z, albedo.w)));
                break;
            }
            default: {
                // Math nodes show computed preview based on connections
                Vec4 previewVal = node.ComputePreviewValue(node, nodes, connections);

                // Show operation name and result
                const char* opName = node.GetOpName();
                if (opName && opName[0] != '\0') {
                    ImGui::Text("%s", opName);
                } else {
                    ImGui::TextDisabled(node.name.c_str());
                }

                // Preview rectangle sized for pin count - shows computed color/value
                float previewWidth = ImGui::GetContentRegionAvail().x;
                int totalPins = MaterialNode::GetInputPinCount(node.type) + MaterialNode::GetOutputPinCount(node.type);
                if (totalPins == 0) totalPins = 1;
                float previewHeight = fmaxf(16.f, (totalPins - 1) * pinSpacing + 20.f);
                ImVec2 previewPos = ImGui::GetCursorScreenPos();
                bgDrawList->AddRectFilled(previewPos,
                    ImVec2(previewPos.x + previewWidth, previewPos.y + previewHeight),
                    ImGui::GetColorU32(ImVec4(previewVal.x, previewVal.y, previewVal.z, previewVal.w)));

                // Show numeric value as text overlay
                char valStr[64];
                snprintf(valStr, sizeof(valStr), "%.2f", previewVal.x);
                ImVec2 textSize = ImGui::CalcTextSize(valStr);
                bgDrawList->AddText(ImVec2(previewPos.x + 4.f, previewPos.y + 3.f),
                    ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.9f)), valStr);
                break;
            }
        }

        // Get window size AFTER content is drawn to know actual height for pin placement
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        float centerY = winPos.y + winSize.y * 0.5f;

        int numInputs = MaterialNode::GetInputPinCount(node.type);
        int numOutputs = MaterialNode::GetOutputPinCount(node.type);

        ImDrawList* fgDrawList = ImGui::GetForegroundDrawList();

        // Draw input pins (left side) with labels - positioned outside node window to left
        for (int i = 0; i < numInputs; i++) {
            float y = centerY + (i - (numInputs - 1) * 0.5f) * pinSpacing;
            ImVec2 pinPos(winPos.x, y);

            // Draw label to the LEFT of input pins (outside node window) using foreground list so it appears on top
            const char* label = MaterialNode::GetInputPinLabel(node.type, i);
            if (!label) {
                static char lbl[8];
                snprintf(lbl, sizeof(lbl), "In%d", i + 1);
                label = lbl;
            }
            ImVec2 textSize = ImGui::CalcTextSize(label);
            fgDrawList->AddText(ImVec2(pinPos.x - pinRadius - 4.f - textSize.x, y - 6.f),
                ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.f)), label);

            bgDrawList->AddCircleFilled(pinPos, pinRadius, ImGui::GetColorU32(ImVec4(0.5f, 0.6f, 1.f, 1.f)));

            PinPosition pp;
            pp.nodeId = node.id; pp.pinIndex = i; pp.isOutput = false; pp.screenPos = pinPos;
            allPins.push_back(pp);
        }

        // Draw output pins (right side) with labels - skip for Output node type
        if (numOutputs > 0) {
            for (int i = 0; i < numOutputs; i++) {
                float y = centerY + (i - (numOutputs - 1) * 0.5f) * pinSpacing;
                ImVec4 pinColor(0.5f, 0.8f, 0.5f, 1.f);
                ImVec2 pinPos(winPos.x + winSize.x, y);
                bgDrawList->AddCircleFilled(pinPos, pinRadius, ImGui::GetColorU32(pinColor));

                // Draw label next to output pin using proper labels (R,G,B,A for Color etc) - foreground list
                const char* label = MaterialNode::GetOutputPinLabel(node.type, i);
                if (!label) {
                    static char lbl[8];
                    snprintf(lbl, sizeof(lbl), "Out%d", i + 1);
                    label = lbl;
                }
                fgDrawList->AddText(ImVec2(pinPos.x + pinRadius + 4.f, y - 6.f),
                    ImGui::GetColorU32(ImVec4(0.7f, 0.8f, 0.5f, 1.f)), label);

                PinPosition pp;
                pp.nodeId = node.id; pp.pinIndex = i; pp.isOutput = true; pp.screenPos = pinPos;
                allPins.push_back(pp);

                // Start connection drag from output pin
                if (IsPinHovered(pinPos, pinRadius + 2.f) && ImGui::IsMouseClicked(0)) {
                    isDraggingConnection = true;
                    dragFromNode = node.id;
                    dragFromPinIndex = i;
                    dragStartPos = pinPos;
                }
            }
        }

        // Right-click on node for context menu
        if (nodeHovered && ImGui::IsMouseClicked(1)) {
            ImGui::OpenPopup("NodeContextMenu");
        }

        if (ImGui::BeginPopup("NodeContextMenu")) {
            if (ImGui::MenuItem("Delete Node")) {
                // Clean up preview texture before deleting node
                for (auto& n : nodes) {
                    if (n.id == node.id && n.previewTex) {
                        delete n.previewTex;
                        n.previewTex = nullptr;
                        break;
                    }
                }

                nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&node](const MaterialNode& n) { return n.id == node.id; }), nodes.end());
                connections.erase(std::remove_if(connections.begin(), connections.end(), [&node](const MaterialConnection& c) {
                    return c.fromNode == node.id || c.toNode == node.id;
                }), connections.end());
            }
            ImGui::EndPopup();
        }

        // Drag node: track mouse delta when left-click on title bar (not during connection drag)
        if (!isDraggingConnection && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            ImVec2 delta(io.MouseDelta.x / graphZoom, io.MouseDelta.y / graphZoom);
            node.pos.x += delta.x;
            node.pos.y += delta.y;
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
    }

    // Draw connections using stored pin positions
    for (const auto& conn : connections) {
        ImVec2 fromPos, toPos;
        bool foundFrom = false, foundTo = false;

        for (const auto& p : allPins) {
            if (!foundFrom && p.nodeId == conn.fromNode && p.pinIndex == conn.fromPinIndex && p.isOutput) {
                fromPos = p.screenPos; foundFrom = true;
            }
            if (!foundTo && p.nodeId == conn.toNode && p.pinIndex == conn.toPinIndex && !p.isOutput) {
                toPos = p.screenPos; foundTo = true;
            }
        }

        if (foundFrom && foundTo) {
            float dx = (toPos.x - fromPos.x) * 0.5f;
            bgDrawList->AddBezierCubic(fromPos, ImVec2(fromPos.x + dx, fromPos.y),
                ImVec2(toPos.x - dx, toPos.y), toPos,
                ImGui::GetColorU32(ImVec4(0.4f, 0.6f, 0.9f, 0.7f)), 2.f);
        }
    }

    // Canvas interactions (only when not hovering a node and not dragging connection)
    if (!canvasHovered && !isDraggingConnection && ImGui::IsWindowHovered()) {
        if (io.MouseWheel != 0) {
            graphZoom *= 1.f + io.MouseWheel * 0.1f;
            graphZoom = fmaxf(0.25f, fminf(graphZoom, 4.f));
        }

        if (ImGui::IsMouseDragging(1)) {
            graphOffset.x += io.MouseDelta.x;
            graphOffset.y += io.MouseDelta.y;
        }

        // Right-click on empty canvas for add node menu
        if (ImGui::IsMouseClicked(1) && !canvasHovered) {
            ImGui::OpenPopup("AddNodeMenu");
        }
    }

    // Handle connection dragging on top of everything
    HandleConnectionDrag(bgDrawList, allPins);

    if (ImGui::BeginPopup("AddNodeMenu")) {
        ImVec2 mouseGraphPos = ImVec2(
            (io.MousePos.x - canvasScreenPos.x - graphOffset.x) / graphZoom,
            (io.MousePos.y - canvasScreenPos.y - graphOffset.y) / graphZoom);

        if (ImGui::MenuItem("Color Node")) {
            CreateNode(MaterialNode::Color, "Color", mouseGraphPos);
        }
        if (ImGui::MenuItem("Float Node")) {
            CreateNode(MaterialNode::Float, "Float", mouseGraphPos);
        }
        if (ImGui::MenuItem("Texture Node")) {
            CreateNode(MaterialNode::Texture, "Texture", mouseGraphPos);
        }
        if (ImGui::MenuItem("Output Node")) {
            CreateNode(MaterialNode::Output, "Output", mouseGraphPos);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Add: Add")) {
            CreateNode(MaterialNode::Add, "+", mouseGraphPos);
        }
        if (ImGui::MenuItem("Add: Multiply")) {
            CreateNode(MaterialNode::Multiply, "*", mouseGraphPos);
        }
        if (ImGui::MenuItem("Add: Lerp")) {
            CreateNode(MaterialNode::Lerp, "lerp", mouseGraphPos);
        }

        ImGui::EndPopup();
    }
}

uint32_t MaterialEditor::CreateNode(MaterialNode::Type type, const std::string& name, ImVec2 pos) {
    MaterialNode node;
    node.id = nextNodeId++;
    node.name = name;
    node.pos = pos; // store in graph space directly
    node.type = type;
    nodes.push_back(node);
    return node.id;
}

Vec4 MaterialNode::ComputePreviewValue(const MaterialNode& self, const std::vector<MaterialNode>& nodes,
                                       const std::vector<MaterialConnection>& connections) const {
    auto GetInputValue = [&](int pinIndex) -> Vec4 {
        for (const auto& conn : connections) {
            if (conn.toNode == self.id && conn.toPinIndex == pinIndex) {
                for (const auto& n : nodes) {
                    if (n.id == conn.fromNode) {
                        return n.ComputePreviewValue(n, nodes, connections);
                    }
                }
            }
        }
        return Vec4(0.f, 0.f, 0.f, 0.f); // default disconnected value
    };

    switch (type) {
        case Color: {
            float c[4] = {1, 1, 1, 1};
            if (!self.userData.empty()) {
                sscanf(self.userData.c_str(), "%f,%f,%f,%f", &c[0], &c[1], &c[2], &c[3]);
            }
            return Vec4(c[0], c[1], c[2], c[3]);
        }
        case Float: {
            float val = 0.5f;
            if (!self.userData.empty()) sscanf(self.userData.c_str(), "%f", &val);
            return Vec4(val, val, val, 1.f);
        }
        case Add: return GetInputValue(0) + GetInputValue(1);
        case Subtract: return GetInputValue(0) - GetInputValue(1);
        case Multiply: {
            Vec4 a = GetInputValue(0), b = GetInputValue(1);
            return Vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
        }
        case Divide: {
            Vec4 a = GetInputValue(0), b = GetInputValue(1);
            return Vec4(a.x / (b.x + 0.001f), a.y / (b.y + 0.001f),
                       a.z / (b.z + 0.001f), a.w / (b.w + 0.001f));
        }
        case Lerp: {
            Vec4 a = GetInputValue(0), b = GetInputValue(1), t = GetInputValue(2);
            return a + (b - a) * t.x;
        }
        default: return Vec4(0.5f, 0.5f, 0.5f, 1.f);
    }
}

json MaterialEditor::SerializeMaterial(const IMaterial* mat) {
    json j;

    if (auto* gm = dynamic_cast<const GenericShaderMaterial*>(mat)) {
        j["kind"] = "generic";
        j["options"] = gm->GetOptions();
        const auto& c = gm->GetColor();
        j["color"] = {c.x, c.y, c.z, c.w};
        const auto& s = gm->GetSpecular();
        j["specular"] = {s.x, s.y, s.z, s.w};
        j["metallic"] = gm->GetMetallic();
        j["roughness"] = gm->GetRoughness();
        j["ssrEnabled"] = gm->IsSSREnabled();
        j["alphaCutoff"] = gm->GetAlphaCutoff();
        j["shininess"] = gm->GetShininess();
        j["reflectivity"] = gm->GetReflectivity();
        j["displacementHeight"] = gm->GetDisplacementHeight();
    } else if (auto* cm = dynamic_cast<const CustomShaderMaterial*>(mat)) {
        j["kind"] = "custom";
        if (!cm->GetShaderFile().empty()) {
            j["shaderFile"] = cm->GetShaderFile();
        }
    }

    IMaterial* nonConstMat = const_cast<IMaterial*>(mat);
    j["opacity"] = mat->GetOpacity();
    j["transparent"] = mat->IsTransparent();
    j["cullFace"] = mat->GetCullFace();
    j["wireframe"] = mat->IsWireFrame();
    j["castingShadows"] = nonConstMat->IsCastingShadows();
    j["depthTest"] = nonConstMat->IsDepthTesting();
    j["depthWrite"] = nonConstMat->IsDepthWritting();
    j["blending"] = mat->IsBlendingEnabled();

    return j;
}

std::shared_ptr<IMaterial> MaterialEditor::DeserializeMaterial(const json& j) {
    std::string kind = j.value("kind", "generic");

    if (kind == "generic") {
        auto mat = std::make_shared<GenericShaderMaterial>(j.value("options", 0u));

        if (j.find("color") != j.end() && j["color"].size() >= 4) {
            mat->SetColor(Vec4(j["color"][0], j["color"][1], j["color"][2], j["color"][3]));
        }
        if (j.find("specular") != j.end() && j["specular"].size() >= 4) {
            mat->SetSpecular(Vec4(j["specular"][0], j["specular"][1], j["specular"][2], j["specular"][3]));
        }

        if (j.find("metallic") != j.end()) mat->SetMetallic(j["metallic"]);
        if (j.find("roughness") != j.end()) mat->SetRoughness(j["roughness"]);
        if (j.value("ssrEnabled", false)) mat->SetSSREnabled(true);
        if (j.find("alphaCutoff") != j.end()) mat->SetAlphaCutoff(j["alphaCutoff"]);
        if (j.find("shininess") != j.end()) mat->SetShininess(j["shininess"]);
        if (j.find("reflectivity") != j.end()) mat->SetReflectivity(j["reflectivity"]);
        if (j.find("displacementHeight") != j.end()) mat->SetDisplacementHeight(j["displacementHeight"]);

        return mat;
    } else if (kind == "custom") {
        std::string shaderFile = j.value("shaderFile", "");
        if (!shaderFile.empty()) {
            return std::make_shared<CustomShaderMaterial>(ResolveTexturePath(shaderFile));
        }
    }

    return nullptr;
}

bool MaterialEditor::IsPinHovered(ImVec2 pinPos, float radius) const {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 delta(io.MousePos.x - pinPos.x, io.MousePos.y - pinPos.y);
    return (delta.x * delta.x + delta.y * delta.y) <= (radius * radius);
}

void MaterialEditor::DrawAddNodePalette() {
    if (!ImGui::Begin("Add Node Palette", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::End();
        return;
    }

    auto AddCategory = [&](const char* label) {
        if (ImGui::TreeNode(label)) {
            ImGui::TreePop();
        }
    };

    AddCategory("Constants");
    AddCategory("Math");
    AddCategory("Vector");
    AddCategory("Comparison");
    AddCategory("Special");

    ImGui::End();
}

void MaterialEditor::HandleConnectionDrag(ImDrawList* drawList, const std::vector<PinPosition>& allPins) {
    if (!isDraggingConnection) return;

    ImGuiIO& io = ImGui::GetIO();

    // Use stored pin screen position from drag start
    ImVec2 outPinPos = dragStartPos;

    drawList->AddLine(outPinPos, io.MousePos, ImGui::GetColorU32(ImVec4(0.6f, 0.8f, 1.f, 0.9f)), 2.f);

    if (!ImGui::IsMouseDown(0)) {
        // Check all input pins for snap target
        for (const auto& p : allPins) {
            if (!p.isOutput && p.nodeId != dragFromNode && IsPinHovered(p.screenPos, 8.f)) {
                MaterialConnection conn;
                conn.fromNode = dragFromNode;
                conn.fromPinIndex = dragFromPinIndex;
                conn.toNode = p.nodeId;
                conn.toPinIndex = p.pinIndex;
                connections.push_back(conn);
            }
        }

        isDraggingConnection = false;
    }
}
