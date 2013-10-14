//============================================================================
// Name        : Text.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text
//============================================================================

#include <string>

#include "Text.h"

namespace p3d {

    Text::Text(Font* font, const std::string& text, const f32 &charWidth, const f32 &charHeight, const Vec4 &color, bool DynamicText)
    {
        this->charWidth = charWidth;
        this->charHeight = charHeight;
        this->font = font;
        this->DynamicText = DynamicText;
        this->Initialized = false;
        
        geometry = new TextGeometry(DynamicText);

        // Generate Font
        font->CreateText(text);

        UpdateText(text, color);
    }

    Text::Text(Font* font, const std::string& text, const f32 &charWidth, const f32 &charHeight, const std::vector<Vec4> &colors, bool DynamicText)
    {
        this->charWidth = charWidth;
        this->charHeight = charHeight;
        this->font = font;
        this->DynamicText = DynamicText;
        this->Initialized = false;
        
        geometry = new TextGeometry(DynamicText);

        // Generate Font
        font->CreateText(text);

        UpdateText(text, colors);
    }        

    void Text::UpdateText(const std::string &text,const Vec4 &color)
    {
        if (this->text!=text)
        {
            this->text = text;

            if (Initialized)
            {
                delete geometry;
                geometry = new TextGeometry(DynamicText);
            }
            // Generate Font
            font->CreateText(text);

            f32 width = 0.0f;
            f32 height = 0.0f;

            f32 offsetX = 0;
            f32 offsetY = 0;

            uint32 quads = 0;
            f32 lineSize = 0.0f;

            for (uint32 i = 0;i<text.size();i++)
            {
                switch(text[i])
                {
                    case '\n':
                        // Build Quads in the bottom
                        offsetY-=lineSize*1.5;
                        offsetX = 0.0f;
                    break;
                    case ' ':
                        offsetX+=font->GetFontSize()/2;
                    break;
                    default:

                        glyph_properties glp = font->GetGlyphs()[text[i]];
                        width = glp.size.x;
                        height = glp.size.y;
                        // Build Quads to the right
                        f32 w2 = width; f32 h2 = height;

                        if (height + glp.offset.y >lineSize) lineSize = height + glp.offset.y;

                        Vec3 a = Vec3(offsetX,      offsetY-glp.offset.y        ,0);
                        Vec3 b = Vec3(w2+offsetX,   offsetY-glp.offset.y        ,0);
                        Vec3 c = Vec3(w2+offsetX,   h2+offsetY-glp.offset.y     ,0);
                        Vec3 d = Vec3(offsetX,      h2+offsetY-glp.offset.y     ,0);

                        // Apply Dimensions
                        a.x = charWidth * a.x / font->GetFontSize();
                        a.y = charHeight * a.y / font->GetFontSize();

                        b.x = charWidth * b.x / font->GetFontSize();
                        b.y = charHeight * b.y / font->GetFontSize();

                        c.x = charWidth * c.x / font->GetFontSize();
                        c.y = charHeight * c.y / font->GetFontSize();

                        d.x = charWidth * d.x / font->GetFontSize();
                        d.y = charHeight * d.y / font->GetFontSize();

                        Vec3 normal = Vec3(color.x,color.y,color.z);

                        geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);
                        geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);
                        geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);
                        geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);

                        Texture* t = font->GetTexture();
                        geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x,glp.startingPoint.y + glp.size.y/(f32)t->GetHeight()));
                        geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x + glp.size.x/(f32)t->GetWidth(),glp.startingPoint.y + glp.size.y/(f32)t->GetHeight()));
                        geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x + glp.size.x/(f32)t->GetWidth(),glp.startingPoint.y));
                        geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x,glp.startingPoint.y));

                        geometry->index.push_back(quads*4+0);
                        geometry->index.push_back(quads*4+1);
                        geometry->index.push_back(quads*4+2);
                        geometry->index.push_back(quads*4+2);
                        geometry->index.push_back(quads*4+3);
                        geometry->index.push_back(quads*4+0);

                        offsetX+=width + glp.offset.x;
                        quads++;

                    break;
                }
            }
            Build();
        }
    }

    void Text::UpdateText(const std::string &text,const std::vector<Vec4> &colors)
    {
        if (this->text!=text)
        {
            this->text = text;

            if (Initialized)
            {
                delete geometry;
                geometry = new TextGeometry(DynamicText);
            }
            // Generate Font
            font->CreateText(text);

            f32 width = 0.0f;
            f32 height = 0.0f;

            f32 offsetX = 0;
            f32 offsetY = 0;

            uint32 quads = 0;
            f32 lineSize = 0.0f;

            for (uint32 i = 0;i<text.size();i++)
            {
                switch(text[i])
                {
                    case '\n':
                        // Build Quads in the bottom
                        offsetY-=lineSize*1.5;
                        offsetX = 0.0f;
                    break;
                    case ' ':
                        offsetX+=font->GetFontSize()/2;
                    break;
                    default:

                        glyph_properties glp = font->GetGlyphs()[text[i]];
                        width = glp.size.x;
                        height = glp.size.y;
                        // Build Quads to the right
                        f32 w2 = width; f32 h2 = height;

                        if (height + glp.offset.y >lineSize) lineSize = height + glp.offset.y;

                        Vec3 a = Vec3(offsetX,      offsetY-glp.offset.y        ,0);
                        Vec3 b = Vec3(w2+offsetX,   offsetY-glp.offset.y        ,0);
                        Vec3 c = Vec3(w2+offsetX,   h2+offsetY-glp.offset.y     ,0);
                        Vec3 d = Vec3(offsetX,      h2+offsetY-glp.offset.y     ,0);

                        // Apply Dimensions
                        a.x = charWidth * a.x / font->GetFontSize();
                        a.y = charHeight * a.y / font->GetFontSize();

                        b.x = charWidth * b.x / font->GetFontSize();
                        b.y = charHeight * b.y / font->GetFontSize();

                        c.x = charWidth * c.x / font->GetFontSize();
                        c.y = charHeight * c.y / font->GetFontSize();

                        d.x = charWidth * d.x / font->GetFontSize();
                        d.y = charHeight * d.y / font->GetFontSize();

                        // Set Color
                        Vec3 normal = Vec3(colors[i].x,colors[i].y,colors[i].z);

                        geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);
                        geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);
                        geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);
                        geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);

                        Texture* t = font->GetTexture();
                        geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x,glp.startingPoint.y + glp.size.y/(f32)t->GetHeight()));
                        geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x + glp.size.x/(f32)t->GetWidth(),glp.startingPoint.y + glp.size.y/(f32)t->GetHeight()));
                        geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x + glp.size.x/(f32)t->GetWidth(),glp.startingPoint.y));
                        geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x,glp.startingPoint.y));

                        geometry->index.push_back(quads*4+0);
                        geometry->index.push_back(quads*4+1);
                        geometry->index.push_back(quads*4+2);
                        geometry->index.push_back(quads*4+2);
                        geometry->index.push_back(quads*4+3);
                        geometry->index.push_back(quads*4+0);

                        offsetX+=width + glp.offset.x;
                        quads++;

                    break;
                }
            }
            Build();
        }
    }
    
    void Text::Build()
    {
        // Create Attributes Buffers
        geometry->CreateBuffers();

        // Clean Geometries List
        Geometries.clear();

        // Add To Geometry List
        Geometries.push_back(geometry);

        // Calculate Mesh Bounding Box
        CalculateBounding();

        if (!Initialized)
        {
            // Execute Parent Build
            Renderable::Build();

            // Set Flag
            Initialized = true;
        }
    }
    
    void TextGeometry::CreateBuffers()
    {
        if (Type==GeometryType::BUFFER)
        {

            // Create and Set Attribute Buffer
            AttributeBuffer* Vertex = new AttributeBuffer(Buffer::Type::Attribute,Buffer::Draw::Static);
            Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tVertex[0],tVertex.size());
            Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3,&tNormal[0],tNormal.size());
            Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2,&tTexcoord[0],tTexcoord.size());
            // Add Buffer to Attributes Buffer List
            Vertex->SendBuffer();
            Attributes.push_back(Vertex);

            // create and send index buffer
            IndexBuffer = new GeometryBuffer(Buffer::Type::Index, Buffer::Draw::Static);
            IndexBuffer->Init( &index[0], sizeof(uint32)*index.size());

        } else {

            // Create and Set Attribute Buffer
            AttributeArray* Vertex = new AttributeArray();
            Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tVertex[0],tVertex.size());
            Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3,&tNormal[0],tNormal.size());
            Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2,&tTexcoord[0],tTexcoord.size());
            // Add Buffer to Attributes Buffer List
            Attributes.push_back(Vertex);

        }
        
        // Calculate Bounding Sphere Radius
        CalculateBounding();
    }
    
    void TextGeometry::CalculateBounding()
    {

        // Bounding Box
        for (uint32 i=0;i<tVertex.size();i++)
        {
            if (i==0) {
                minBounds = tVertex[i];
                maxBounds = tVertex[i];
            } else {
                if (tVertex[i].x<minBounds.x) minBounds.x = tVertex[i].x;
                if (tVertex[i].y<minBounds.y) minBounds.y = tVertex[i].y;
                if (tVertex[i].z<minBounds.z) minBounds.z = tVertex[i].z;
                if (tVertex[i].x>maxBounds.x) maxBounds.x = tVertex[i].x;
                if (tVertex[i].y>maxBounds.y) maxBounds.y = tVertex[i].y;
                if (tVertex[i].z>maxBounds.z) maxBounds.z = tVertex[i].z;
            }
        }
        // Bounding Sphere
        BoundingSphereCenter = maxBounds-minBounds;
        f32 a = maxBounds.distance(BoundingSphereCenter);
        f32 b = minBounds.distance(BoundingSphereCenter);        
        BoundingSphereRadius = (a>b?a:b);
            
    }
    
};