#ifndef DECORATIONS_H
#define DECORATIONS_H


#include <string>
#include <glm/glm.hpp>

#include "Primitives.h"
#include "Mesh.h"

class Frame : public Node
{
public:

    typedef enum { ROUND = 0, SHARP } CornerType;
    typedef enum { THIN = 0, LARGE } BorderType;
    typedef enum { NONE = 0, GLOW, DROP, PERSPECTIVE } ShadowType;

    Frame(CornerType corner, BorderType border, ShadowType shadow);
    ~Frame();

    void update (float dt) override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Mesh *border() const { return side_; }
    glm::vec4 color;

protected:
    Mesh *side_;
    Mesh *top_;
    Mesh *shadow_;
    LineSquare *square_;
};

class Handles : public Node
{
public:
    typedef enum { RESIZE = 0, RESIZE_H, RESIZE_V, ROTATE } Type;
    Handles(Type type);
    ~Handles();

    void update (float dt) override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Type type() const { return type_; }
    Primitive *handle() const { return handle_; }
    glm::vec4 color;

protected:
    Primitive *handle_;
    Type type_;

};

class Symbol : public Node
{
public:
    typedef enum { POINT = 0, IMAGE, VIDEO, SESSION, CLONE, RENDER, DOTS, CIRCLES, EMPTY } Type;
    Symbol(Type t = POINT, glm::vec3 pos = glm::vec3(0.f));
    ~Symbol();

    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Type type() const { return type_; }
    glm::vec4 color;

protected:
    Mesh *symbol_;
    Type type_;
};

class Disk : public Node
{
public:
    Disk();
    ~Disk();

    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    glm::vec4 color;

protected:
    static Mesh *disk_;
};

//class SelectionBox : public Group
//{
//public:
//    SelectionBox();

//    void draw (glm::mat4 modelview, glm::mat4 projection) override;

//    glm::vec4 color;

//protected:
//    LineSquare *square_;
//    GlmToolkit::AxisAlignedBoundingBox bbox_;

//};


#endif // DECORATIONS_H
