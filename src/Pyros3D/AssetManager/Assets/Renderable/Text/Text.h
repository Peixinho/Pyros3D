//============================================================================
// Name        : Text.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text
//============================================================================

#ifndef TEXT_H
#define	TEXT_H

#include "../Renderables.h"
#include "../../../../Core/Buffers/GeometryBuffer.h"
#include "../../Font/Font.h"
#include "../../../AssetManager.h"
#include <vector>

namespace p3d {
    
    class TextGeometry : public IGeometry {

        public:
            std::vector<Vec3> tVertex, tNormal;
            std::vector<Vec2> tTexcoord;

            TextGeometry(bool DynamicText) : IGeometry((DynamicText?GeometryType::ARRAY:GeometryType::BUFFER)) {}
            
            void CreateBuffers();

            virtual std::vector<uint32> &GetIndexData() {return index;}
            virtual std::vector<Vec3> &GetVertexData() {return tVertex;}

        protected:

            void CalculateBounding();
            
    };

    class Text : public Renderable {
        public:

            // Geometry
            TextGeometry* geometry;

            Text(Font* font, const std::string& text, const f32 &charWidth, const f32 &charHeight, const Vec4 &color = Vec4(1,1,1,1), bool DynamicText = false);
            Text(Font* font, const std::string& text, const f32 &charWidth, const f32 &charHeight, const std::vector<Vec4> &color, bool DynamicText = false);

            void Build();

            void UpdateText(const std::string &text, const Vec4 &color = Vec4(1,1,1,1));
            void UpdateText(const std::string &text, const std::vector<Vec4> &color);

        private:

            // Char Dimensions
            f32 charWidth, charHeight;
            
            // Font
            Font* font;

            // String
            std::string text;
            
            // Dynamic Text Optimization
            bool DynamicText;
            
            // Is Initialized
            bool Initialized;
    };
};

#endif	/* TEXT_H */