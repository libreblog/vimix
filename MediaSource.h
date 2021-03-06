#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H

#include "Source.h"

class MediaSource : public Source
{
public:
    MediaSource();
    ~MediaSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    void render() override;
    bool failed() const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    // Media specific interface
    void setPath(const std::string &p);
    std::string path() const;
    MediaPlayer *mediaplayer() const;

protected:

    void init() override;
    void replaceRenderingShader() override;

    Surface *mediasurface_;
    std::string path_;
    MediaPlayer *mediaplayer_;
};

#endif // MEDIASOURCE_H
