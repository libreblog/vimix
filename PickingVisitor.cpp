#include "PickingVisitor.h"

#include "Log.h"
#include "Primitives.h"
#include "Decorations.h"

#include "GlmToolkit.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/vector_angle.hpp>


PickingVisitor::PickingVisitor(glm::vec3 coordinates) : Visitor()
{
    modelview_ = glm::mat4(1.f);
    points_.push_back( coordinates );
}

PickingVisitor::PickingVisitor(glm::vec3 selectionstart, glm::vec3 selection_end) : Visitor()
{
    modelview_ = glm::mat4(1.f);
    points_.push_back( selectionstart );
    points_.push_back( selection_end );
}

void PickingVisitor::visit(Node &n)
{
    // use the transform modified during update
    modelview_ *= n.transform_;

//      modelview_ *= transform(n.translation_, n.rotation_, n.scale_);
//    Log::Info("Node %d", n.id());
//    Log::Info("%s", glm::to_string(modelview_).c_str());
}

void PickingVisitor::visit(Group &n)
{
    if (!n.visible_)
        return;

    glm::mat4 mv = modelview_;
    for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
        if ( (*node)->visible_ )
            (*node)->accept(*this);
        modelview_ = mv;
    }
}

void PickingVisitor::visit(Switch &n)
{
    if (!n.visible_ || n.numChildren()<1)
        return;

    glm::mat4 mv = modelview_;
    n.activeChild()->accept(*this);
    modelview_ = mv;
}

void PickingVisitor::visit(Primitive &)
{
    // by default, a Primitive is not interactive
}

void PickingVisitor::visit(Surface &n)
{
    if (!n.visible_)
        return;

    // if more than one point given for testing: test overlap
    if (points_.size() > 1) {
        // create bounding box for those points (2 in practice)
        GlmToolkit::AxisAlignedBoundingBox bb_points;
        bb_points.extend(points_);

        // apply inverse transform
        bb_points = bb_points.transformed(glm::inverse(modelview_)) ;

        // test bounding box for overlap with inverse transform bbox
        if ( bb_points.intersect( n.bbox() ) )
//            if ( n.bbox().contains( bb_points ) )
            // add this surface to the nodes picked
            nodes_.push_back( std::pair(&n, glm::vec2(0.f)) );
    }
    // only one point
    else if (points_.size() > 0) {

        // apply inverse transform to the point of interest
        glm::vec4 P = glm::inverse(modelview_) * glm::vec4( points_[0], 1.f );

        // test bounding box for picking from a single point
        if ( n.bbox().contains( glm::vec3(P)) )
            // add this surface to the nodes picked
            nodes_.push_back( std::pair(&n, glm::vec2(P)) );
    }
}

void PickingVisitor::visit(Disk &n)
{
    // discard if not visible or if not exactly one point given for picking
    if (!n.visible_ || points_.size() != 1)
        return;

    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( points_[0], 1.f );

    // test distance for picking from a single point
    if ( glm::length(glm::vec2(P)) < 1.f )
        // add this surface to the nodes picked
        nodes_.push_back( std::pair(&n, glm::vec2(P)) );

}

void PickingVisitor::visit(Handles &n)
{
    // discard if not visible or if not exactly one point given for picking
    if (!n.visible_ || points_.size() != 1)
        return;

    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( points_[0], 1.f );

    // inverse transform to check the scale
    glm::vec4 S = glm::inverse(modelview_) * glm::vec4( 0.05f, 0.05f, 0.f, 0.f );
    float scale = glm::length( glm::vec2(S) );

    bool picked = false;
    if ( n.type() == Handles::RESIZE ) {
        // 4 corners
        picked = ( glm::length(glm::vec2(+1.f, +1.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(+1.f, -1.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(-1.f, +1.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(-1.f, -1.f)- glm::vec2(P)) < scale );
    }
    else if ( n.type() == Handles::RESIZE_H ){
        // left & right
        picked = ( glm::length(glm::vec2(+1.f, 0.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(-1.f, 0.f)- glm::vec2(P)) < scale );
    }
    else if ( n.type() == Handles::RESIZE_V ){
        // top & bottom
        picked = ( glm::length(glm::vec2(0.f, +1.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(0.f, -1.f)- glm::vec2(P)) < scale );
    }
    else if ( n.type() == Handles::ROTATE ){
        // the icon for rotation is on the right top corner at (0.12, 0.12) in scene coordinates
        glm::vec4 vec = glm::inverse(modelview_) * glm::vec4( 0.1f, 0.1f, 0.f, 0.f );
        float l = glm::length( glm::vec2(vec) );
        picked  = glm::length( glm::vec2( 1.f + l, 1.f + l) - glm::vec2(P) ) < 1.5f * scale;
    }

    if ( picked )
        // add this to the nodes picked
        nodes_.push_back( std::pair(&n, glm::vec2(P)) );

}


void PickingVisitor::visit(Scene &n)
{
    n.root()->accept(*this);
}
