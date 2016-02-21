//============================================================================
// Name        : Bullet Physics.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Bullet Physics Wrapper
//============================================================================

#include <Pyros3D/Physics/PhysicsEngines/BulletPhysics/BulletPhysics.h>
#include <Pyros3D/Physics/Components/Box/PhysicsBox.h>
#include <Pyros3D/Physics/Components/Sphere/PhysicsSphere.h>
#include <Pyros3D/Physics/Components/MultipleSphere/PhysicsMultipleSphere.h>
#include <Pyros3D/Physics/Components/Capsule/PhysicsCapsule.h>
#include <Pyros3D/Physics/Components/Cone/PhysicsCone.h>
#include <Pyros3D/Physics/Components/Cylinder/PhysicsCylinder.h>
#include <Pyros3D/Physics/Components/StaticPlane/PhysicsStaticPlane.h>
#include <Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.h>
#include <Pyros3D/Physics/Components/ConvexTriangleMesh/PhysicsConvexTriangleMesh.h>
#include <Pyros3D/Physics/Components/ConvexHull/PhysicsConvexHull.h>
#include <Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.h>

namespace p3d {
    
    BulletPhysics::BulletPhysics() {}
    
    BulletPhysics::~BulletPhysics()
    {
        delete m_dynamicsWorld;
        delete m_solver;
        delete m_broadphase;
        delete m_dispatcher;
        delete m_collisionConfiguration;
    }
    
    void BulletPhysics::InitPhysics()
    {
        // collision configuration contains default setup for memory, collision setup
    	m_collisionConfiguration = new btDefaultCollisionConfiguration();

    	// use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
    	m_dispatcher = new btCollisionDispatcher(m_collisionConfiguration);

    	m_broadphase = new btDbvtBroadphase();

    	// the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
    	m_solver = new btSequentialImpulseConstraintSolver();

    	m_dynamicsWorld = new btDiscreteDynamicsWorld(m_dispatcher,m_broadphase,m_solver,m_collisionConfiguration);
    	
        // set world gravity
    	m_dynamicsWorld->setGravity(btVector3(0,-9.8f,0));
        
        // Set Flag
        physicsInitialized = true;

        nowTime = 0, lastTime = 0, timeInterval = 0;
    }
    
    void BulletPhysics::Update(const f64& time, const uint32 steps)
    {
        //lastTime = time;
        if (lastTime==0) lastTime = time;

        timeInterval += time - lastTime;

        while( timeInterval >= 1.f/60.f )
        {
            m_dynamicsWorld->stepSimulation(1.f/60.f,steps);
            timeInterval -= 1.f/60.f;
        }

        lastTime = time;
    }
    
    void BulletPhysics::EnableDebugDraw()
    {
        m_debugDraw = new PhysicsDebugDraw();
        m_dynamicsWorld->setDebugDrawer(m_debugDraw);
        m_debugDraw->setDebugMode(btIDebugDraw::DBG_MAX_DEBUG_DRAW_MODE);
    }
    void BulletPhysics::RenderDebugDraw(Projection projection, GameObject* Camera)
    {
        m_debugDraw->StartDebugRendering();
        m_debugDraw->SetProjectionMatrix(projection.m);
        m_debugDraw->SetCameraMatrix(Camera->GetWorldTransformation().Inverse());
        m_dynamicsWorld->debugDrawWorld();
        m_debugDraw->EndDebugRendering();
    }
    void BulletPhysics::DisableDebugDraw()
    {
        m_dynamicsWorld->setDebugDrawer(NULL);
        delete m_debugDraw;
    }
    void BulletPhysics::EndPhysics()
    {
        delete m_dynamicsWorld;
    	delete m_solver;
    	delete m_broadphase;
    	delete m_dispatcher;
    	delete m_collisionConfiguration;
    }
    
    btCollisionShape* BulletPhysics::GetCollisionShape(IPhysicsComponent* pcomp)
    {
        btCollisionShape* collShape;
        switch(pcomp->GetShape())
        {
            case CollisionShapes::Box:
            {
                // Get Shape Informations And Create Bullet Shape Equivalent
                PhysicsBox* box = static_cast<PhysicsBox*> (pcomp);
                collShape = new btBoxShape(btVector3(box->GetWidth(),box->GetHeight(),box->GetDepth()));
            }
            break;
            case CollisionShapes::Sphere:
            {   
                // Get Shape Informations And Create Bullet Shape Equivalent
                PhysicsSphere* sphere = static_cast<PhysicsSphere*> (pcomp);
                collShape = new btSphereShape(sphere->GetRadius());
            }
            break;
            case CollisionShapes::Capsule:
            {   
                // Get Shape Informations And Create Bullet Shape Equivalent
                PhysicsCapsule* capsule = static_cast<PhysicsCapsule*> (pcomp);
                collShape = new btCapsuleShape(capsule->GetRadius(), capsule->GetHeight());
            }
            break;
            case CollisionShapes::Cone:
            {   
                // Get Shape Informations And Create Bullet Shape Equivalent
                PhysicsCone* Cone = static_cast<PhysicsCone*> (pcomp);
                collShape = new btConeShape(Cone->GetRadius(), Cone->GetHeight());
            }
            break;
            case CollisionShapes::Cylinder:
            {   
                // Get Shape Informations And Create Bullet Shape Equivalent
                PhysicsCylinder* Cylinder = static_cast<PhysicsCylinder*> (pcomp);
                collShape = new btCylinderShape(btVector3(Cylinder->GetHeight(), Cylinder->GetRadius(),Cylinder->GetRadius()));
            }
            break;
            case CollisionShapes::StaticPlane:
            {   
                // Get Shape Informations And Create Bullet Shape Equivalent
                PhysicsStaticPlane* StaticPlane = static_cast<PhysicsStaticPlane*> (pcomp);
                collShape = new btStaticPlaneShape(btVector3(StaticPlane->GetNormal().x,StaticPlane->GetNormal().y,StaticPlane->GetNormal().z),StaticPlane->GetConstant());
            }
            break;
            case CollisionShapes::ConvexHull:
            {
                // Get Shape Informations And Create Bullet Shape Equivalent
                btConvexHullShape* convexHull = new btConvexHullShape();
                PhysicsConvexHull* ConvexHull = static_cast<PhysicsConvexHull*> (pcomp);
                for (uint32 i=0;i<ConvexHull->GetPoints().size();i++)
                {
                    convexHull->addPoint(btVector3(ConvexHull->GetPoints()[i].x,ConvexHull->GetPoints()[i].y,ConvexHull->GetPoints()[i].z));
                }
                collShape = convexHull;
            }
            break;
            case CollisionShapes::ConvexTriangleMesh:
            {
                // Get Shape Informations And Create Bullet Shape Equivalent
                btTriangleMesh *trimesh = new btTriangleMesh();
                // Static cast to Triangle Mesh
                PhysicsConvexTriangleMesh* p = (PhysicsConvexTriangleMesh*) pcomp;
                for (uint32 i=0;i<p->GetVertexData().size();i+=3)
                {
                    trimesh->addTriangle(
                                            btVector3(p->GetVertexData()[i].x, p->GetVertexData()[i].y, p->GetVertexData()[i].z), 
                                            btVector3(p->GetVertexData()[i + 1].x, p->GetVertexData()[i + 1].y, p->GetVertexData()[i + 1].z),
                                            btVector3(p->GetVertexData()[i + 2].x, p->GetVertexData()[i + 2].y, p->GetVertexData()[i + 2].z)
                                        );
                }
                for (uint32 i=0;i<p->GetIndexData().size();i++)
                {
                    trimesh->addIndex(p->GetIndexData()[i]);
                }
                // Add Trimesh to Shape
                collShape = new btConvexTriangleMeshShape(trimesh,true);
                
            }
            break;
            case CollisionShapes::TriangleMesh:   
            {
                // Get Shape Informations And Create Bullet Shape Equivalent
                btTriangleMesh *trimesh = new btTriangleMesh();
                // Static cast to Triangle Mesh
                PhysicsTriangleMesh* p = (PhysicsTriangleMesh*) pcomp;
                for (uint32 i=0;i<p->GetVertexData().size();i+=3)
                {
                    trimesh->addTriangle(
                                            btVector3(p->GetVertexData()[i].x, p->GetVertexData()[i].y, p->GetVertexData()[i].z), 
                                            btVector3(p->GetVertexData()[i + 1].x, p->GetVertexData()[i + 1].y, p->GetVertexData()[i + 1].z),
                                            btVector3(p->GetVertexData()[i + 2].x, p->GetVertexData()[i + 2].y, p->GetVertexData()[i + 2].z)
                                        );
                }
                for (uint32 i=0;i<p->GetIndexData().size();i++)
                {
                    trimesh->addIndex(p->GetIndexData()[i]);
                }
                // Add Trimesh to Shape
                collShape = new btBvhTriangleMeshShape(trimesh,true);
                
            }
            break;
            case CollisionShapes::MultipleSphere:
            {
                // Get Shape Informations And Create Bullet Shape Equivalent
            }
            break;
            case CollisionShapes::HeightFieldTerrain:
            {
                // Get Shape Informations And Create Bullet Shape Equivalent
            }
            break;
        };
        
        return collShape;
    }
    
    void BulletPhysics::CreatePhysicsComponent(IPhysicsComponent* pcomp)
    {
        btCollisionShape* collShape;
        switch(pcomp->GetShape())
        {
            case CollisionShapes::Vehicle:
            {
                
                #define CUBE_HALF_EXTENTS 1
                PhysicsVehicle* vehicle = (PhysicsVehicle*) pcomp;

                btCollisionShape* chassisShape = GetCollisionShape(vehicle->GetChassis());

                btCompoundShape* compound = new btCompoundShape();
                btTransform localTrans;
                localTrans.setIdentity();
                //localTrans effectively shifts the center of mass with respect to the chassis
                localTrans.setOrigin(btVector3(0,1,0));

                compound->addChildShape(localTrans,chassisShape);
                btTransform tr;
                tr.setIdentity();
                tr.setOrigin(btVector3(pcomp->GetOwner()->GetPosition().x, pcomp->GetOwner()->GetPosition().y, pcomp->GetOwner()->GetPosition().z));
                btRigidBody* m_carChassis = LocalCreateRigidBody(vehicle->GetChassis()->GetMass(),tr,compound);//chassisShape;
                //m_carChassis->setDamping(0.2,0.2);

                btRaycastVehicle::btVehicleTuning m_tuning;
                // create vehicle
                btVehicleRaycaster* m_vehicleRayCaster = new btDefaultVehicleRaycaster(m_dynamicsWorld);
                btRaycastVehicle* m_vehicle = new btRaycastVehicle(m_tuning,m_carChassis,m_vehicleRayCaster);
                
                ///never deactivate the vehicle
                m_carChassis->setActivationState(DISABLE_DEACTIVATION);
                
                m_dynamicsWorld->addVehicle(m_vehicle);

                //choose coordinate system
                m_vehicle->setCoordinateSystem(vehicle->GetRightIndex(),vehicle->GetUpIndex(),vehicle->GetForwardIndex());
                
                // Save Pointer
                pcomp->SaveRigidBodyPTR(m_vehicle);
                //vehicle->RaycastVehicle = m_vehicle;
                //vehicle->VehicleRaycaster = m_vehicleRayCaster;
                
                std::vector<VehicleWheel> Wheels = vehicle->GetWheels();
                for (uint32 i=0;i<Wheels.size();i++)
                {
                    AddWheel(pcomp,Wheels[i].Direction,Wheels[i].Axle,Wheels[i].Radius,Wheels[i].Width,Wheels[i].Friction,Wheels[i].RollInfluence,Wheels[i].Position,Wheels[i].IsFrontWheel);
                }
                
            }
            break;
            default:
                collShape = GetCollisionShape(pcomp);
                CreateRigidBody(collShape,pcomp);
                break;
        };
        
    }
    
    // Add Wheel to Vehicle
    void BulletPhysics::AddWheel(IPhysicsComponent* pcomp, const Vec3& WheelDirection, const Vec3& WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3& Position, bool isFrontWheel)
    {
        PhysicsVehicle* vehicle = (PhysicsVehicle*) pcomp;
        btRaycastVehicle* m_vehicle = (btRaycastVehicle*) pcomp->GetRigidBodyPTR();

        btRaycastVehicle::btVehicleTuning m_tuning;
        
        btVector3 connectionPointCS0(Position.x,Position.y,Position.z);
        m_vehicle->addWheel(connectionPointCS0,btVector3(WheelDirection.x,WheelDirection.y,WheelDirection.z),btVector3(WheelAxle.x,WheelAxle.y,WheelAxle.z),vehicle->GetSuspensionRestLength(),WheelRadius,m_tuning,isFrontWheel);

        btWheelInfo& wheel = m_vehicle->getWheelInfo(m_vehicle->getNumWheels()-1);
        wheel.m_suspensionStiffness = vehicle->GetSuspensionStiffness();
        wheel.m_wheelsDampingRelaxation = vehicle->GetSuspensionDamping();
        wheel.m_wheelsDampingCompression = vehicle->GetSuspensionCompression();
        wheel.m_frictionSlip = WheelFriction;
        wheel.m_rollInfluence = WheelRollInfluence;
    }
    
    // Create Rigid Body
    btRigidBody* BulletPhysics::LocalCreateRigidBody(f32 mass, const btTransform& startTransform,btCollisionShape* shape)
    {
        btAssert((!shape || shape->getShapeType() != INVALID_SHAPE_PROXYTYPE));

        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (mass != 0.f);
		
        // Local Inertia
        btVector3 localInertia(0,0,0);

        // Local Inertia
        if (isDynamic)
            shape->calculateLocalInertia(mass,localInertia);
            
        //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
		/*btTransform startTransform1;
        startTransform1.setIdentity();
        startTransform1.setOrigin(btVector3(0,0,0));*/

        btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), startTransform.getOrigin()));

        btRigidBody::btRigidBodyConstructionInfo cInfo(mass,motionState,shape,localInertia);

        btRigidBody* body = new btRigidBody(cInfo);

        m_dynamicsWorld->addRigidBody(body);

        return body;
    }
    
    void BulletPhysics::CreateRigidBody(btCollisionShape* shape, IPhysicsComponent* pcomp)
    {
                        
        // Static or Dynamic
        bool isDynamic = (pcomp->GetMass() != 0.f);

        // Local Inertia
        btVector3 localInertia(0,0,0);

        // Calculate Inertia
        if (isDynamic)
            shape->calculateLocalInertia(pcomp->GetMass(),localInertia);

        // Initial Transformation
        btTransform startTransform;
        startTransform.setIdentity();
        // Local is Enough because we can't use Parent/Child Relationships, it wouldn't work right
        startTransform.setOrigin(btVector3(pcomp->GetOwner()->GetPosition().x,pcomp->GetOwner()->GetPosition().y,pcomp->GetOwner()->GetPosition().z));
		std::cout << pcomp->GetOwner()->GetPosition().toString() << std::endl;
        Quaternion q;
        q.SetRotationFromEuler(pcomp->GetOwner()->GetRotation());
        startTransform.setRotation(btQuaternion(q.x,q.y,q.z,q.w));
        // Create Motion State
        btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(0,0,0)));
        // Rigid Body Info
        btRigidBody::btRigidBodyConstructionInfo rbInfo(pcomp->GetMass(),motionState, shape,localInertia);
        rbInfo.m_additionalDamping = true;
        
        // Create Rigid Body
        btRigidBody* body = new btRigidBody(rbInfo);
        body->setWorldTransform(startTransform);
        // Add Rigid Body to World
        m_dynamicsWorld->addRigidBody(body);

        // Save RigidBody Pointer
        pcomp->SaveRigidBodyPTR(body);
    }
    
    void BulletPhysics::UpdateTransformations(IPhysicsComponent* pcomp)
    {
        if (pcomp->RigidBodyRegistered())
        {
            btTransform trans;
            if (pcomp->GetShape()==CollisionShapes::Vehicle)
            {
                PhysicsVehicle* vcomp = static_cast<PhysicsVehicle*>(pcomp);
                btRaycastVehicle* vehicle = static_cast<btRaycastVehicle*> (pcomp->GetRigidBodyPTR());
                trans = vehicle->getChassisWorldTransform();
                // Wheels Transformation
                {
                    for (uint32 i=0;i<vehicle->getNumWheels();i++)
                    {
                        Matrix m;
                        vehicle->updateWheelTransform(i,true);
                        vehicle->getWheelInfo(i).m_worldTransform.getOpenGLMatrix(&m.m[0]);
                        vcomp->GetWheels()[i].Transformation = m;
                    }
                }
            } else {
                btRigidBody* body = static_cast<btRigidBody*> (pcomp->GetRigidBodyPTR());
                trans = body->getWorldTransform();
            }

            // Apply to Rendering
            pcomp->GetOwner()->SetPosition(Vec3(trans.getOrigin().x(),trans.getOrigin().y(),trans.getOrigin().z()));
            Quaternion q = Quaternion(trans.getRotation().w(),trans.getRotation().x(),trans.getRotation().y(),trans.getRotation().z());
            pcomp->GetOwner()->SetRotation(q.GetEulerFromQuaternion());
        }
    }
    
    void BulletPhysics::RemovePhysicsComponent(IPhysicsComponent* pcomp)
    {
        //m_dynamicsWorld->removeRigidBody(static_cast<btRigidBody*> (pcomp->GetRigidBodyPTR()));

	 }
	
    // Rigid Bodys Methods
    void BulletPhysics::UpdatePosition(IPhysicsComponent *pcomp, const Vec3 &position)
    {
        btRigidBody* body = static_cast<btRigidBody*> (pcomp->GetRigidBodyPTR());
        btTransform startTransform;
        startTransform = body->getWorldTransform();
        startTransform.setOrigin(btVector3(position.x,position.y,position.z));
        
        body->setWorldTransform(startTransform);
    }
    void BulletPhysics::UpdateRotation(IPhysicsComponent *pcomp, const Vec3 &rotation)
    {
        btRigidBody* body = static_cast<btRigidBody*> (pcomp->GetRigidBodyPTR());
        btTransform startTransform;
        startTransform = body->getWorldTransform();
        Quaternion q;
        q.SetRotationFromEuler(rotation);
        startTransform.setRotation(btQuaternion(q.x,q.y,q.z,q.w));
        body->setWorldTransform(startTransform);
    }
    void BulletPhysics::CleanForces(IPhysicsComponent *pcomp)
    {
        btRigidBody* body = static_cast<btRigidBody*> (pcomp->GetRigidBodyPTR());
        body->clearForces();
    }
    void BulletPhysics::SetAngularVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity)
    {
        btRigidBody* body = static_cast<btRigidBody*> (pcomp->GetRigidBodyPTR());
        body->setAngularVelocity(btVector3(velocity.x,velocity.y,velocity.z));
    }
    void BulletPhysics::SetLinearVelocity(IPhysicsComponent *pcomp, const Vec3 &velocity)
    {
        btRigidBody* body = static_cast<btRigidBody*> (pcomp->GetRigidBodyPTR());
        body->setLinearVelocity(btVector3(velocity.x,velocity.y,velocity.z));
    }
    void BulletPhysics::Activate(IPhysicsComponent *pcomp)
    {
        btRigidBody* body = static_cast<btRigidBody*> (pcomp->GetRigidBodyPTR());
        body->activate();
    }

    // Create Physics Components
    IPhysicsComponent* BulletPhysics::CreateBox(const f32 width, const f32 height, const f32 depth, const f32 mass)
    {
        PhysicsBox* box = new PhysicsBox(this,width,height,depth,mass);
        return box;
    }
    IPhysicsComponent* BulletPhysics::CreateCapsule(const f32 radius, const f32 height, const f32 mass)
    {
        PhysicsCapsule* capsule = new PhysicsCapsule(this,radius,height,mass);
        return capsule;
    }
    IPhysicsComponent* BulletPhysics::CreateCone(const f32 radius, const f32 height, const f32 mass)
    {
        PhysicsCone* cone = new PhysicsCone(this,radius,height,mass);
        return cone;
    }
    IPhysicsComponent* BulletPhysics::CreateConvexHull(const std::vector<Vec3> &points, const f32 mass)
    {
        PhysicsConvexHull* convexHull = new PhysicsConvexHull(this,points,mass);
        return convexHull;
    }
    IPhysicsComponent* BulletPhysics::CreateConvexTriangleMesh(RenderingComponent* rcomp, const f32 mass)
    {
        PhysicsConvexTriangleMesh* convexTriangleMesh = new PhysicsConvexTriangleMesh(this,rcomp,mass);
        return convexTriangleMesh;
    }
    IPhysicsComponent* BulletPhysics::CreateConvexTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass)
    {
        PhysicsConvexTriangleMesh* convexTriangleMesh = new PhysicsConvexTriangleMesh(this,index,vertex,mass);
        return convexTriangleMesh;
    }
    IPhysicsComponent* BulletPhysics::CreateCylinder(const f32 radius, const f32 height, const f32 mass)
    {
        PhysicsCylinder* cylinder = new PhysicsCylinder(this,radius,height,mass);
        return cylinder;
    }
    IPhysicsComponent* BulletPhysics::CreateMultipleSphere(const std::vector<Vec3> &positions, const std::vector<f32> &radius, const f32 mass)
    {
        PhysicsMultipleSphere* multipleSphere = new PhysicsMultipleSphere(this,positions,radius,mass);
        return multipleSphere;
    }
    IPhysicsComponent* BulletPhysics::CreateSphere(const f32 radius, const f32 mass)
    {
        PhysicsSphere* sphere = new PhysicsSphere(this,radius,mass);
        return sphere;
    }
    IPhysicsComponent* BulletPhysics::CreateStaticPlane(const Vec3 &Normal, const f32 Constant, const f32 mass)
    {
        PhysicsStaticPlane* plane = new PhysicsStaticPlane(this,Normal,Constant,mass);
        return plane;
    }
    IPhysicsComponent* BulletPhysics::CreateTriangleMesh(RenderingComponent* rcomp, const f32 mass)
    {
        PhysicsTriangleMesh* triangleMesh = new PhysicsTriangleMesh(this,rcomp,mass);
        return triangleMesh;
    }
    IPhysicsComponent* BulletPhysics::CreateTriangleMesh(const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass)
    {
        PhysicsTriangleMesh* triangleMesh = new PhysicsTriangleMesh(this,index,vertex,mass);
        return triangleMesh;
    }
    IPhysicsComponent* BulletPhysics::CreateVehicle(IPhysicsComponent* ChassisShape)
    {
        PhysicsVehicle* vehicle = new PhysicsVehicle(this,ChassisShape);
        return vehicle;
    }
}
