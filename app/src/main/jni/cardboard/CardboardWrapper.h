//
// Created by FuYoucheng on 13/2/2021.
//

#ifndef FVISIONA_CARDBOARDWRAPPER_H
#define FVISIONA_CARDBOARDWRAPPER_H

#include <jni.h>
#include "cardboard.h"
#include "util.h"

namespace fvision {

class CardboardWrapper {
public:
    CardboardWrapper(JavaVM* vm, jobject obj);
    virtual ~CardboardWrapper();

    void pause();
    void resume();

    ndk_hello_cardboard::Matrix4x4 getPos();

private:
    CardboardHeadTracker* headTrackerPtr;

};

}

#endif //FVISIONA_CARDBOARDWRAPPER_H
