#include "input_router.h"
#include <iostream>
#include <stdexcept>
using namespace rs3d;
void check(bool b,const char* what){if(!b)throw std::runtime_error(what);}
int main(){try{
    InputRouter r;
    check(r.update(true,true,true,false,false,false)==InputOwner::controls,"panel press");
    check(!r.cameraAllowed(true,true,true,false,false),"drag out of panel must not rotate");
    check(r.update(true,true,true,true,false,false)==InputOwner::controls,"second button must not steal capture");
    r.update(true,false,false,true,false,false);
    check(r.cameraAllowed(true,false,true,false,false),"hover zoom after release");
    r.update(true,true,true,true,true,false);
    check(!r.cameraAllowed(true,true,true,true,false),"editor owns viewport click");
    r.update(false,false,true,true,true,false);
    check(r.owner()==InputOwner::none&&!r.cameraAllowed(false,false,true,false,false),"focus loss");
    r.update(true,true,true,true,false,true);
    check(!r.cameraAllowed(true,true,true,false,true),"modal priority");
    r.update(true,false,false,true,false,false);
    r.update(true,true,true,true,false,false);
    check(r.cameraAllowed(true,true,false,false,false),"viewport drag retains capture outside viewport");
    r.update(false,false,false,false,false,false);
    check(r.owner()==InputOwner::none,"focus cleanup");
    std::cout<<"PASS input ownership: panel/editor/modal/viewport, multi-button capture, drag-out, focus-loss\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
