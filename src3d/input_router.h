#pragma once
namespace rs3d {
enum class InputOwner { none, controls, viewport };
// A timeline drag owns input until release, even outside the slider. A short
// press+release within one rendered frame must still perform a seek.
inline bool updateTimelineCapture(bool& captured,bool focused,bool enabled,bool pressed,bool down,bool inside) {
    if(!focused||!enabled){captured=false;return false;}
    if(pressed&&inside)captured=true;
    bool seek=captured&&(pressed||down);
    if(!down)captured=false;
    return seek;
}
// Capture belongs to the surface where the press began until every button is released.
// Losing focus releases capture; an active editor/modal blocks viewport shortcuts too.
class InputRouter {
    InputOwner owner_ = InputOwner::none;
public:
    InputOwner update(bool focused,bool pressed,bool down,bool insideViewport,bool editor,bool modal) {
        if(!focused) return owner_=InputOwner::none;
        if(pressed && owner_==InputOwner::none) owner_=(editor||modal||!insideViewport)?InputOwner::controls:InputOwner::viewport;
        if(!down) owner_=InputOwner::none;
        return owner_;
    }
    bool cameraAllowed(bool focused,bool down,bool insideViewport,bool editor,bool modal) const {
        return focused&&!editor&&!modal&&(owner_==InputOwner::viewport||(!down&&insideViewport));
    }
    InputOwner owner() const { return owner_; }
};
}
