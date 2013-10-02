#include "Game.h"
#include "Fishy3D/Components/RenderingComponents/RenderingPrimitiveComponents/Cube/RenderingCubeComponent.h"
#include "Fishy3D/Components/RenderingComponents/RenderingPrimitiveComponents/Plane/RenderingPlaneComponent.h"
#include "Fishy3D/Components/LightComponents/DirectionalLight/DirectionalLightComponent.h"

namespace Fishy3D {    
    
    Game::Game(const int &Width, const int &Height, const std::string &title, const unsigned int &type) : SFMLInterface(Width,Height,title,type) {}

    Game::~Game() {}

    void Game::Resize(const int& width, const int& height)
    {
        
        // Use Base Resize
        SFMLInterface::Resize(Width,Height);
        
        // Set Projection and Update Camera
        projection->MakePerspectiveRH(65.f,(float)Width/(float)Height,1.f,100.f);
        
        // Update Renderer Dimensions
        renderer->Resize(Width, Height);
        
    }
    
    void Game::Init()
    {
        
        // Set Renderer and SceneGraph
        renderer = SuperSmartPointer<IRenderer> (new ForwardRenderer(Width, Height));
        scene = SuperSmartPointer<SceneGraph> (new SceneGraph());
        
        // Set Projection
		projection = SuperSmartPointer<Projection> (new Projection());
        projection->MakePerspectiveRH(65.f,(float)Width/(float)Height,1.f,100.f);
        
        // Set Camera GameObject and Camera Component
        camera = SuperSmartPointer<GameObject> (new GameObject("Camera"));       
            
        // Add Camera to SceneGraph
        scene->AddChild(camera);
        
        // Set Camera Position
		camera->transformation.SetZ(30);
		camera->transformation.SetY(10);
        
        // Camera looking at Origin
        camera->LookAt(vec3::ZERO);
        
		// Materials
		// Cube Material
		cubeMaterial = SuperSmartPointer<IMaterial> (new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse));
		cubeMaterial->SetColor(Colors::Brick);
		cubeMaterial->SetCullFace(CullFace::BackFace);

		// Material's Lighting Properties
        vec4 Ke = vec4(0.2,0.2,0.2,0.2);
        vec4 Ka = vec4(0.2,0.2,0.2,0.2);
        vec4 Kd = Colors::Brick;
        vec4 Ks = vec4(1,1,1,1);
        float Shininess = 50;
		// Apply those Properties
		cubeMaterial->ChangeLightingProperties(Ke,Ka,Kd,Ks,Shininess);

		// Floor Material
		floorMaterial = SuperSmartPointer<IMaterial> (new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::ShadowMaterial));
		floorMaterial ->SetColor(Colors::White);
		floorMaterial ->SetCullFace(CullFace::BackFace);

        // Create Cube Game Object
        CubeObject = SuperSmartPointer<GameObject> (new GameObject("CubeObject"));
        // Create Cube Rendering ComponentLightingModel::None
        Cube = SuperSmartPointer<IComponent> (new RenderingCubeComponent("Cube",10,10,10,cubeMaterial));
        // Add Cube Component to Game Object
        CubeObject->AddComponent(Cube);
        // Add Cube to Scene
        scene->AddChild(CubeObject);
        
		// Create Floor Game Object
        FloorObject = SuperSmartPointer<GameObject> (new GameObject("FlootObject"));
        // Create Cube Rendering ComponentLightingModel::None
        Floor = SuperSmartPointer<IComponent> (new RenderingPlaneComponent("Floor",50,50,floorMaterial));
        // Add Cube Component to Game Object
        FloorObject->AddComponent(Floor);
        // Add Cube to Scene
        scene->AddChild(FloorObject);

		// Floor Position
		FloorObject->transformation.SetY(-13.f);
		FloorObject->transformation.RotationX(-1.5f);

        // Create Light Game Object
        LightObject = SuperSmartPointer<GameObject> (new GameObject("LightObject"));
        // Create Directional Light Component
        Light = SuperSmartPointer<IComponent> (new DirectionalLightComponent("Light", vec4(1.f,1.f,1.f,1.f)));
		DirectionalLightComponent* l = (DirectionalLightComponent*) Light.Get();
		l->EnableCascadeShadows(1,*projection.Get(),1.f,50.f,2048,2048);
        // Add Light Component to Game Object
        LightObject->AddComponent(Light);
        // Add Light to Scene
        scene->AddChild(LightObject);
        // Set Light Position
        LightObject->transformation.SetY(100.f);
        LightObject->transformation.SetX(100.f);

    }

    void Game::Update()
    {
        
        // Rotate Cube
        CubeObject->transformation.RotationY(GetTime());
        
        // Update Scene
        scene->Update(GetTime());
        
        // Render Scene
		renderer->RenderScene(scene.Get(), camera.Get(), projection.Get());
        
        // Show Debug Text
        // Get Game FPS
        std::ostringstream x; x << fps.getFPS();
        // Show Text and Game Engine Logo
        DisplayInfo("Fishy3D v1.0\nFPS:" + x.str(), vec2(0.4f,0.4f),Colors::White);
    }

    void Game::Shutdown() 
    {

        // Set Flag to ShutDown App
        Initialized = false;
        
        // Destroy
        scene->RemoveAllChilds();
        scene.Dispose();
        renderer.Dispose();
        camera.Dispose();
        CubeObject->RemoveAllComponents();
        CubeObject.Dispose();
        Cube.Dispose();
        LightObject->RemoveAllComponents();
        LightObject.Dispose();
        Light.Dispose();
        
    }
       
}