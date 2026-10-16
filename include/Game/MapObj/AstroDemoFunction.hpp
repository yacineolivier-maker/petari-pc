#pragma once

#if TARGET_PC
// Same functions as Game/Demo/AstroDemoFunction.hpp, which matches their definitions (MSVC
// mangles return types and class vs namespace, so this older declaration can't link).
#include "Game/Demo/AstroDemoFunction.hpp"
#else

#include <revolution/types.h>

class JMapInfoIter;
class LiveActor;

namespace MR {
    class FunctorBase;
};  // namespace MR

class AstroDemoFunction {
public:
    static s32 getOpenedAstroDomeNum();
    static const char* getGrandStarReturnDemoName(int);

    static bool tryRegisterDemo(LiveActor*, const char*, const JMapInfoIter&);
    static bool tryRegisterGrandStarReturnAndSimpleCast(LiveActor*, const JMapInfoIter&);
    static bool tryRegisterGrandStarReturnWithFunctionAndSimpleCast(LiveActor*, const JMapInfoIter&, const MR::FunctorBase&);
};
#endif
