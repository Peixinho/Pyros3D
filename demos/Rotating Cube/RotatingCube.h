#include "Fishy3D/Utils/Interfaces/SFMLInterface.h"
#include "Fishy3D/Renderer/ForwardRenderer/ForwardRenderer.h"
#include "Fishy3D/Materials/IMaterial.h"
#include "Fishy3D/Core/GameObjects/GameObject.h"
#include "Fishy3D/Components/IComponent.h"
#include "Fishy3D/Core/InputManager/InputManager.h"
#include "Fishy3D/Utils/Colors/Colors.h"
#include "Fishy3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h"

namespace Fishy3D {
    
    class Game : public SFMLInterface
    {

        public:
            Game(const int &Width, const int &Height, const std::string &title, const unsigned int &type);
            ~Game();

            virtual void Init();
            virtual void Update();
            virtual void Shutdown();
            virtual void Resize(const int &Width, const int &Height);

        private:

            // set camera, renderer, scenegraph
            SuperSmartPointer<SceneGraph> scene;
            SuperSmartPointer<IRenderer> renderer;
			SuperSmartPointer<Projection> projection;
            SuperSmartPointer<GameObject> camera;
            
            // Cube
            SuperSmartPointer<GameObject> CubeObject;
            SuperSmartPointer<IComponent> Cube;
			SuperSmartPointer<GameObject> FloorObject;
            SuperSmartPointer<IComponent> Floor;
			SuperSmartPointer<GenericShaderMaterial> cubeMaterial;
			SuperSmartPointer<GenericShaderMaterial> floorMaterial;
            
            // Light
            SuperSmartPointer<GameObject> LightObject;
            SuperSmartPointer<IComponent> Light;            
        
    };
}