#include <Pyros3D/Physics/PhysicsEngines/BulletPhysics/DebugDraw/PhysicsDebugDraw.h>
#if defined(ANDROID) || defined(EMSCRIPTEN)
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    #include "GL/glew.h"
#endif

#include <iostream>

namespace p3d {
    
    PhysicsDebugDraw::PhysicsDebugDraw():m_debugMode(0) 
    {
        physicsDebugMaterial = new GenericShaderMaterial(ShaderUsage::PhysicsDebug);
    }

    PhysicsDebugDraw::~PhysicsDebugDraw() 
    {
        delete physicsDebugMaterial;
    }

    void PhysicsDebugDraw::SetProjectionMatrix(const Matrix projection)
    {
        projectionMatrix = projection;
    }

    void PhysicsDebugDraw::SetCameraMatrix(const Matrix camera)
    {
        viewMatrix = camera;
    }

    void PhysicsDebugDraw::StartDebugRendering()
    {
        // clean values
        vertexLines.clear();
        colorLines.clear();
        vertexTriangles.clear();
        colorTriangles.clear();
        vertexQuadStrip.clear();
        colorQuadStrip.clear();
    }
    
    void PhysicsDebugDraw::EndDebugRendering()
    {
        glUseProgram(physicsDebugMaterial->GetShader());
        
        uint32 vertexHandle = Shader::GetAttributeLocation(physicsDebugMaterial->GetShader(),"aPosition");
        uint32 colorHandle = Shader::GetAttributeLocation(physicsDebugMaterial->GetShader(),"aColor");

        uint32 projectionHandle = Shader::GetUniformLocation(physicsDebugMaterial->GetShader(),"uProjectionMatrix");
        uint32 viewMatrixHandle = Shader::GetUniformLocation(physicsDebugMaterial->GetShader(),"uViewMatrix");
        uint32 modelMatrixHandle = Shader::GetUniformLocation(physicsDebugMaterial->GetShader(),"uModelMatrix");
        uint32 opacityHandle = Shader::GetUniformLocation(physicsDebugMaterial->GetShader(),"uOpacity");
        
        Shader::SendUniform(Uniform("uProjectionMatrix",DataType::Matrix,&projectionMatrix.m[0]),projectionHandle);
        Shader::SendUniform(Uniform("uViewMatrix",DataType::Matrix,&viewMatrix.m[0]),viewMatrixHandle);
        Shader::SendUniform(Uniform("uModelMatrix",DataType::Matrix,&modelMatrix.m[0]),modelMatrixHandle);
        f32 opacity = 1.f;
        Shader::SendUniform(Uniform("uOpacity",DataType::Float,&opacity),opacityHandle);
        
        // Send Attributes
        glEnableVertexAttribArray(vertexHandle);
        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0, &vertexLines[0]);
        
        glEnableVertexAttribArray(colorHandle);
        glVertexAttribPointer(colorHandle, 4, GL_FLOAT, GL_FALSE, 0, &colorLines[0]);
        
        // Draw Quad
        glDrawArrays(GL_LINES, 0, vertexLines.size());

        // Disable Attributes
        glDisableVertexAttribArray(colorHandle);
        glDisableVertexAttribArray(vertexHandle);
        
        // Send Attributes
        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0, &vertexTriangles[0]);
        glVertexAttribPointer(colorHandle, 4, GL_FLOAT, GL_FALSE, 0, &colorTriangles[0]);
        
        // Draw Quad
        glDrawArrays(GL_TRIANGLES, 0, vertexTriangles.size());

        // Disable Attributes
        glDisableVertexAttribArray(colorHandle);
        glDisableVertexAttribArray(vertexHandle);
        
        #if !defined(ANDROID) && !defined(EMSCRIPTEN)
        // Send Attributes
        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0, &vertexQuadStrip[0]);
        glVertexAttribPointer(colorHandle, 4, GL_FLOAT, GL_FALSE, 0, &colorQuadStrip[0]);
        
        // Draw Quad
        glDrawArrays(GL_QUAD_STRIP, 0, vertexQuadStrip.size());

        // Disable Attributes
        glDisableVertexAttribArray(colorHandle);
        glDisableVertexAttribArray(vertexHandle);        

        #endif
        
        glUseProgram(0);
    }
    
    void PhysicsDebugDraw::drawLine(const btVector3& from,const btVector3& to,const btVector3& fromColor, const btVector3& toColor)
    {
        vertexLines.push_back(Vec3(from.getX(), from.getY(), from.getZ()));
        vertexLines.push_back(Vec3(to.getX(), to.getY(), to.getZ()));
        colorLines.push_back(Vec4(fromColor.getX(), fromColor.getY(), fromColor.getZ(), 1.f));
        colorLines.push_back(Vec4(toColor.getX(), toColor.getY(), toColor.getZ(), 1.f));
    }

    void PhysicsDebugDraw::drawLine(const btVector3& from,const btVector3& to,const btVector3& color)
    {
        drawLine(from,to,color,color);
    }

    void PhysicsDebugDraw::drawSphere(const btVector3& p, btScalar radius, const btVector3& color)
    {

        #if !defined(ANDROID) && !defined(EMSCRIPTEN)
        Vec3 pos = Vec3(p.getX(), p.getY(), p.getZ());

        int lats = 5;
        int longs = 5;

        int i, j;
        for(i = 0; i <= lats; i++) 
        {
            btScalar lat0 = SIMD_PI * (-btScalar(0.5) + (btScalar) (i - 1) / lats);
            btScalar z0  = radius*sin(lat0);
            btScalar zr0 =  radius*cos(lat0);

            btScalar lat1 = SIMD_PI * (-btScalar(0.5) + (btScalar) i / lats);
            btScalar z1 = radius*sin(lat1);
            btScalar zr1 = radius*cos(lat1);

            glBegin(GL_QUAD_STRIP);
            for(j = 0; j <= longs; j++) 
            {
                btScalar lng = 2 * SIMD_PI * (btScalar) (j - 1) / longs;
                btScalar x = cos(lng);
                btScalar y = sin(lng);

                vertexQuadStrip.push_back(pos+Vec3(x * zr0, y * zr0, z0));
                vertexQuadStrip.push_back(pos+Vec3(x * zr1, y * zr1, z1));
                colorQuadStrip.push_back(Vec4(color.getX(), color.getY(), color.getZ(), btScalar(1.0f)));
                colorQuadStrip.push_back(Vec4(color.getX(), color.getY(), color.getZ(), btScalar(1.0f)));
                
//                glNormal3f(x * zr0, y * zr0, z0);
//                glNormal3f(x * zr1, y * zr1, z1);
            }
        }
        #endif
    }

    void PhysicsDebugDraw::drawTriangle(const btVector3& a,const btVector3& b,const btVector3& c,const btVector3& color,btScalar alpha)
    {
    //	if (m_debugMode > 0)
        {
            const btVector3 n=btCross(b-a,c-a).normalized();
            
            colorTriangles.push_back(Vec4(color.getX(), color.getY(), color.getZ(),1.f));
            colorTriangles.push_back(Vec4(color.getX(), color.getY(), color.getZ(),1.f));
            colorTriangles.push_back(Vec4(color.getX(), color.getY(), color.getZ(),1.f));
            
//            glNormal3d(n.getX(),n.getY(),n.getZ());
            vertexTriangles.push_back(Vec3(a.getX(),a.getY(),a.getZ()));
            vertexTriangles.push_back(Vec3(b.getX(),b.getY(),b.getZ()));
            vertexTriangles.push_back(Vec3(c.getX(),c.getY(),c.getZ()));
        }
    }

    void PhysicsDebugDraw::setDebugMode(int debugMode)
    {
        m_debugMode = debugMode;
    }

    void PhysicsDebugDraw::draw3dText(const btVector3& location,const char* textString)
    {
        std::cout << textString << std::endl;
    }

    void PhysicsDebugDraw::reportErrorWarning(const char* warningString)
    {
        std::cout << warningString << std::endl;
    }

    void PhysicsDebugDraw::drawContactPoint(const btVector3& pointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color)
    {
        btVector3 to=pointOnB+normalOnB*1;//distance;
        const btVector3&from = pointOnB;
        vertexLines.push_back(Vec3(from.getX(), from.getY(), from.getZ()));
        vertexLines.push_back(Vec3(to.getX(), to.getY(), to.getZ()));
        colorLines.push_back(Vec4(color.getX(), color.getY(), color.getZ(),1.f));
        colorLines.push_back(Vec4(color.getX(), color.getY(), color.getZ(),1.f));
    }

};