#include <algorithm>

#include "defines.h"
#include "Settings.h"
#include "FrameBuffer.h"
#include "Session.h"
#include "GarbageVisitor.h"
#include "Recorder.h"
#include "SessionCreator.h"

#include "Log.h"

Session::Session() : failedSource_(nullptr), active_(true), fading_target_(0.f)
{
    filename_ = "";

    config_[View::RENDERING] = new Group;
    config_[View::RENDERING]->scale_ = FrameBuffer::getResolutionFromParameters(Settings::application.render.ratio, Settings::application.render.res);

    config_[View::GEOMETRY] = new Group;
    config_[View::GEOMETRY]->scale_ = Settings::application.views[View::GEOMETRY].default_scale;
    config_[View::GEOMETRY]->translation_ = Settings::application.views[View::GEOMETRY].default_translation;

    config_[View::LAYER] = new Group;
    config_[View::LAYER]->scale_ = Settings::application.views[View::LAYER].default_scale;
    config_[View::LAYER]->translation_ = Settings::application.views[View::LAYER].default_translation;

    config_[View::MIXING] = new Group;
    config_[View::MIXING]->scale_ = Settings::application.views[View::MIXING].default_scale;
    config_[View::MIXING]->translation_ = Settings::application.views[View::MIXING].default_translation;
}


Session::~Session()
{
    // delete all recorders
    clearRecorders();

    // delete all sources
    for(auto it = sources_.begin(); it != sources_.end(); ) {
        // erase this source from the list
        it = deleteSource(*it);
    }

    delete config_[View::RENDERING];
    delete config_[View::GEOMETRY];
    delete config_[View::LAYER];
    delete config_[View::MIXING];
}

void Session::setActive (bool on)
{
    if (active_ != on) {
        active_ = on;
        for(auto it = sources_.begin(); it != sources_.end(); it++) {
            (*it)->setActive(active_);
        }
    }
}

// update all sources
void Session::update(float dt)
{
    failedSource_ = nullptr;

    // pre-render of all sources
    for( SourceList::iterator it = sources_.begin(); it != sources_.end(); it++){

        if ( (*it)->failed() ) {
            failedSource_ = (*it);
        }
        else {
            // render the source
            (*it)->render();
            // update the source
            (*it)->update(dt);
        }
    }

    // apply fading (smooth dicotomic reaching)
    float f = render_.fading();
    if ( ABS_DIFF(f, fading_target_) > EPSILON) {
        render_.setFading( f + ( fading_target_ - f ) / 2.f);
    }

    // update the scene tree
    render_.update(dt);

    // draw render view in Frame Buffer
    render_.draw();

    // send frame to recorders
    std::list<Recorder *>::iterator iter;
    for (iter=recorders_.begin(); iter != recorders_.end(); )
    {
        Recorder *rec = *iter;

        rec->addFrame(render_.frame(), dt);

        if (rec->finished()) {
            iter = recorders_.erase(iter);
            delete rec;
        }
        else {
            iter++;
        }
    }
}


SourceList::iterator Session::addSource(Source *s)
{
    // lock before change
    access_.lock();

    // insert the source in the rendering
    render_.scene.ws()->attach(s->group(View::RENDERING));
    // insert the source to the beginning of the list
    sources_.push_front(s);

    // unlock access
    access_.unlock();

    // return the iterator to the source created at the beginning
    return sources_.begin();
}

SourceList::iterator Session::deleteSource(Source *s)
{
    // lock before change
    access_.lock();

    // find the source
    SourceList::iterator its = find(s);
    // ok, its in the list !
    if (its != sources_.end()) {

        // remove Node from the rendering scene
        render_.scene.ws()->detatch( s->group(View::RENDERING) );

        // erase the source from the update list & get next element
        its = sources_.erase(its);

        // delete the source : safe now
        delete s;
    }

    // unlock access
    access_.unlock();

    // return end of next element
    return its;
}

Source *Session::popSource()
{
    Source *s = nullptr;

    SourceList::iterator its = sources_.begin();
    if (its != sources_.end())
    {
        s = *its;

        // remove Node from the rendering scene
        render_.scene.ws()->detatch( s->group(View::RENDERING) );

        // erase the source from the update list & get next element
        sources_.erase(its);
    }

    return s;
}

void Session::setResolution(glm::vec3 resolution)
{
    render_.setResolution(resolution);
    config_[View::RENDERING]->scale_ = resolution;
}

void Session::setFading(float f, bool forcenow)
{
    if (forcenow)
        render_.setFading( f );

    fading_target_ = CLAMP(f, 0.f, 1.f);
}

SourceList::iterator Session::begin()
{
    return sources_.begin();
}

SourceList::iterator Session::end()
{
    return sources_.end();
}

SourceList::iterator Session::find(int index)
{
    if (index<0)
        return sources_.end();

    int i = 0;
    SourceList::iterator it = sources_.begin();
    while ( i < index && it != sources_.end() ){
        i++;
        it++;
    }
    return it;
}

SourceList::iterator Session::find(Source *s)
{
    return std::find(sources_.begin(), sources_.end(), s);
}

SourceList::iterator Session::find(std::string namesource)
{
    return std::find_if(sources_.begin(), sources_.end(), Source::hasName(namesource));
}

SourceList::iterator Session::find(Node *node)
{
    return std::find_if(sources_.begin(), sources_.end(), Source::hasNode(node));
}

uint Session::numSource() const
{
    return sources_.size();
}

bool Session::empty() const
{
    return sources_.empty();
}

int Session::index(SourceList::iterator it) const
{
    int index = -1;
    int count = 0;
    for(auto i = sources_.begin(); i != sources_.end(); i++, count++) {
        if ( i == it ) {
            index = count;
            break;
        }
    }
    return index;
}

void Session::addRecorder(Recorder *rec)
{
    recorders_.push_back(rec);
}


Recorder *Session::frontRecorder()
{
    if (recorders_.empty())
        return nullptr;
    else
        return recorders_.front();
}

void Session::stopRecorders()
{
    std::list<Recorder *>::iterator iter;
    for (iter=recorders_.begin(); iter != recorders_.end(); )
        (*iter)->stop();
}

void Session::clearRecorders()
{
    std::list<Recorder *>::iterator iter;
    for (iter=recorders_.begin(); iter != recorders_.end(); )
    {
        Recorder *rec = *iter;
        rec->stop();
        iter = recorders_.erase(iter);
        delete rec;
    }
}

void Session::transferRecorders(Session *dest)
{
    if (dest == nullptr)
        return;

    std::list<Recorder *>::iterator iter;
    for (iter=recorders_.begin(); iter != recorders_.end(); )
    {
        dest->recorders_.push_back(*iter);
        iter = recorders_.erase(iter);
    }
}


void Session::lock()
{
    access_.lock();
}

void Session::unlock()
{
    access_.unlock();
}


Session *loadSession_(const std::string& filename)
{
    Session *s = new Session;

    if (s) {
        // actual loading of xml file
        SessionCreator creator( s );

        if (creator.load(filename)) {
            // loaded ok
            s->setFilename(filename);
        }
        else {
            // error loading
            delete s;
            s = nullptr;
        }
    }

    return s;
}

