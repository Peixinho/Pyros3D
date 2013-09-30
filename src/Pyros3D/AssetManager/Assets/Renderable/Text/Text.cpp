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

    Text::Text(Font* font, const std::string& text, const f32 &charWidth, const f32 &charHeight, const Vec4 &color)
    {
        this->charWidth = charWidth;
        this->charHeight = charHeight;
        this->font = font;

        geometry = new TextGeometry();

        // Generate Font
        font->CreateText(text);

        UpdateText(text, color);
    }

    Text::Text(Font* font, const std::string& text, const f32 &charWidth, const f32 &charHeight, const std::vector<Vec4> &colors)
    {
        this->charWidth = charWidth;
        this->charHeight = charHeight;
        this->font = font;

        geometry = new TextGeometry();

        // Generate Font
        font->CreateText(text);

        UpdateText(text, colors);
    }        

    void Text::UpdateText(const std::string &text,const Vec4 &color)
    {

        if (this->text!=text)
        {
            this->text = text;

            if (geometry->index.size()>0)
            {
                geometry->Dispose();
                geometry->index.clear();
                geometry->tVertex.clear();
                geometry->tNormal.clear();
                geometry->tTexcoord.clear();
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

            // Build and Send Buffers
            Build();
        }
    }

    void Text::UpdateText(const std::string &text,const std::vector<Vec4> &colors)
    {
        if (this->text!=text)
        {
            this->text = text;

            if (geometry->index.size()>0)
            {
                geometry->Dispose();
                geometry->index.clear();
                geometry->tVertex.clear();
                geometry->tNormal.clear();
                geometry->tTexcoord.clear();
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

            // Build and Send Buffers
            Build();
        }
    }        

    Text::~Text()
    {



    }
};