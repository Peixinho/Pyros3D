//============================================================================
// Name        : MaterialEditor.h
// Description : Material editor window (generic + custom shader, node/text modes)
//============================================================================

#ifndef MATERIALEDITOR_H
#define	MATERIALEDITOR_H

#include <imgui.h>
#include <Pyros3D/Utils/Json/json.hpp>
using json = nlohmann::json;
#include <misc/cpp/imgui_stdlib.h>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Assets/Texture/Texture.h>

struct CodeEditorDocument;

enum class MaterialEditKind { Generic, Custom };
enum class MaterialEditMode { Inspector, Text, NodeGraph };

enum class MaterialInputPin { Albedo, Normal, Metallic, Roughness, Emissive, Occlusion, Height };

struct MaterialNode;
struct MaterialConnection;

struct MaterialNode {
    uint32_t id = 0;
    std::string name;
    ImVec2 pos = ImVec2(100.f, 100.f);

    enum Type {
        // Constants
        Color, Float, Texture, Int, Bool, Vec2Type, Vec3Type, Vec4Type,
        // Math operations
        Add, Subtract, Multiply, Divide, Power, Modulo, Negate, Abs, Sqrt, Sin, Cos, Tan,
        Min, Max, Clamp, Lerp, DotProduct, CrossProduct, Length, Normalize, Distance,
        // Comparison/Logic
        Equal, NotEqual, GreaterThan, LessThan, And, Or, Not, Step, SmoothStep,
        // Vector ops
        SplitVec2, SplitVec3, SplitVec4, CombineVec2, CombineVec3, CombineVec4,
        // Material outputs
        Output_Albedo, Output_Normal, Output_Metallic, Output_Roughness, Output_Emissive,
        Output_Occlusion, Output_Height, Output_AO, Output_FinalColor, Output,
        // Special
        ObjectPosition, CameraPosition, UVCoordinate, NormalVector, TimeValue
    } type = Float;

    std::string boundProperty;
    std::string userData;      // stores node-specific values (color floats, etc)
    std::string texturePath;   // for Texture nodes: path to loaded texture
    p3d::Texture* previewTex = nullptr;  // cached preview texture for display

    static int GetInputPinCount(Type t) {
        switch (t) {
            case Add: case Subtract: case Multiply: case Divide: case Power: case Modulo:
            case Min: case Max: case Equal: case NotEqual: case GreaterThan: case LessThan:
            case And: case Or: return 2;
            case Clamp: case Lerp: case SmoothStep: return 3;
            case DotProduct: case CrossProduct: case Distance: return 2;
            case CombineVec2: return 2; case CombineVec3: return 3; case CombineVec4: return 4;
            case Step: return 2;
            case Output: return 6; // Albedo, Normal, Metallic, Roughness, Emissive, Occlusion
            default: return 0;
        }
    }

    static int GetOutputPinCount(Type t) {
        switch (t) {
            case Color: return 5; // R, G, B, A, RGBA
            case SplitVec2: return 2; case SplitVec3: return 3; case SplitVec4: return 4;
            case Output: return 0; // Output node only has inputs, no outputs
            default: return 1;
        }
    }

    static const char* GetInputPinLabel(Type t, int index) {
        if (t == Output) {
            switch (index) {
                case 0: return "Albedo"; case 1: return "Normal"; case 2: return "Metallic";
                case 3: return "Roughness"; case 4: return "Emissive"; case 5: return "Occlusion";
            }
        }
        return nullptr;
    }

    static const char* GetOutputPinLabel(Type t, int index) {
        if (t == Color) {
            switch (index) {
                case 0: return "R"; case 1: return "G"; case 2: return "B";
                case 3: return "A"; case 4: return "RGBA";
            }
        } else if (t == SplitVec2) {
            switch (index) { case 0: return "X"; case 1: return "Y"; }
        } else if (t == SplitVec3) {
            switch (index) { case 0: return "X"; case 1: return "Y"; case 2: return "Z"; }
        } else if (t == SplitVec4) {
            switch (index) { case 0: return "X"; case 1: return "Y"; case 2: return "Z"; case 3: return "W"; }
        }
        return nullptr;
    }

    // Compute preview value based on connected inputs and node type
    Vec4 ComputePreviewValue(const MaterialNode& self, const std::vector<MaterialNode>& nodes,
                             const std::vector<MaterialConnection>& connections) const;

    const char* GetOpName() const {
        switch (type) {
            case Add: return "+"; case Subtract: return "-"; case Multiply: return "*";
            case Divide: return "/"; case Power: return "^"; case Modulo: return "%";
            case Negate: return "neg"; case Abs: return "|x|"; case Sqrt: return "√";
            case Sin: return "sin"; case Cos: return "cos"; case Tan: return "tan";
            case Min: return "min"; case Max: return "max"; case Clamp: return "clamp";
            case Lerp: return "lerp"; case DotProduct: return "dot"; case CrossProduct: return "cross";
            case Length: return "|v|"; case Normalize: return "norm"; case Distance: return "dist";
            case Equal: return "=="; case NotEqual: return "!="; case GreaterThan: return ">";
            case LessThan: return "<"; case And: return "&"; case Or: return "|";
            case Not: return "!"; case Step: return "step"; case SmoothStep: return "smoothstep";
            default: return "";
        }
    }
};

struct PinPosition {
    uint32_t nodeId = 0;
    int pinIndex = 0;
    bool isOutput = false;
    ImVec2 screenPos;
};

struct MaterialConnection {
    uint32_t fromNode = 0;
    int fromPinIndex = 0; // which output pin
    uint32_t toNode = 0;
    int toPinIndex = 0;   // which input pin
};

class MaterialEditor {
public:
    MaterialEditor(bool* open);
    ~MaterialEditor();

    void Init(const p3d::uint32 width, const p3d::uint32 height) {}
    void OnResize(const p3d::uint32 width, const p3d::uint32 height) {}
    void Update(const p3d::f64 time) {}
    void Shutdown();

    void Show();
    std::shared_ptr<p3d::IMaterial> CreateNewMaterial(MaterialEditKind kind);
    std::shared_ptr<p3d::IMaterial> LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path);
    void EditMaterial(std::shared_ptr<p3d::IMaterial> mat);
    std::shared_ptr<p3d::IMaterial> GetCurrentMaterial() const { return currentMaterial; }
    void SetProjectRoot(const std::string& root);

private:
    bool* Open;
    std::string Name = "Material Editor";

    std::shared_ptr<p3d::IMaterial> currentMaterial;
    MaterialEditKind editKind = MaterialEditKind::Generic;
    MaterialEditMode editMode = MaterialEditMode::Inspector;
    std::string materialName = "NewMaterial";
    bool dirty = false;

    CodeEditorDocument* codeDoc = nullptr;
    uint32_t nextNodeId = 1;
    std::vector<MaterialNode> nodes;
    std::vector<MaterialConnection> connections;
    ImVec2 graphOffset = ImVec2(0.f, 0.f);
    float graphZoom = 1.0f;

    // Connection dragging state
    bool isDraggingConnection = false;
    uint32_t dragFromNode = 0;
    int dragFromPinIndex = -1;
    ImVec2 dragStartPos = ImVec2(0, 0);

    std::string projectRoot;

    void DrawToolbar();
    void DrawInspectorTab();
    void DrawTextEditorTab();
    void DrawNodeGraphTab();
    void DrawGenericMaterialInspector();
    void DrawCustomMaterialInspector();
    void DrawCommonMaterialSettings(p3d::IMaterial* mat);
    void DrawTextureSlot(const char* label, p3d::Texture* tex);

    uint32_t CreateNode(MaterialNode::Type type, const std::string& name, ImVec2 pos);
    
    // Node graph helpers
    bool IsPinHovered(ImVec2 pinPos, float radius) const;
    void DrawAddNodePalette();
    void HandleConnectionDrag(ImDrawList* drawList, const std::vector<PinPosition>& allPins);

    json SerializeMaterial(const p3d::IMaterial* mat);
    std::shared_ptr<p3d::IMaterial> DeserializeMaterial(const json& j);
};

#endif	/* MATERIALEDITOR_H */
