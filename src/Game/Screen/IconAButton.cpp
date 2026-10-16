#include "Game/Screen/IconAButton.hpp"
#include "Game/LiveActor/Nerve.hpp"
#include "Game/Util/LayoutUtil.hpp"
#include "Game/Util/ObjUtil.hpp"
#include <cstdio>
#if TARGET_PC
#include <cstdlib>
#include <nw4r/lyt/material.h>
#include <nw4r/lyt/pane.h>
#endif

namespace NrvIconAButton {
    NEW_NERVE(IconAButtonNrvOpen, IconAButton, Open);
    NEW_NERVE(IconAButtonNrvWait, IconAButton, Wait);
    NEW_NERVE(IconAButtonNrvTerm, IconAButton, Term);
};  // namespace NrvIconAButton

IconAButton::IconAButton(bool connectToScene, bool connectToPause) : LayoutActor("Aボタンアイコン", true), mFollowPos(0.0f, 0.0f), mFollowActor() {
    if (connectToScene) {
        if (connectToPause) {
            MR::connectToSceneLayoutOnPause(this);
        } else {
            MR::connectToSceneTalkLayout(this);
        }
    }

    mFollowPaneName[0] = '\0';
}

void IconAButton::init(const JMapInfoIter& rIter) {
    initLayoutManager("IconAButton", 1);
    initNerve(GET_NERVE(IconAButton, IconAButtonNrvOpen));
    MR::startAnim(this, "Appear", 0);
    MR::setAnimFrameAndStop(this, 0.0f, 0);
#if TARGET_PC
    // Xbox look: the A disc (white in the grey-scale IA4 textures, tinted by the material's
    // TEV colors) turns Xbox green; the letter stays dark. SMGPC_WII_ICONS keeps the Wii one.
    if (getenv("SMGPC_WII_ICONS") == nullptr) {
        const char* names[] = {"PicIconA", "PicIconA1"};
        for (const char* name : names) {
            nw4r::lyt::Pane* pane = MR::getPane(this, name);
            if (nw4r::lyt::Material* mat = pane != nullptr ? pane->GetMaterial() : nullptr) {
                const GXColorS10 green = {110, 200, 40, 255};
                mat->SetTevColor(1, green);
            }
        }
    }
#endif
    kill();
}

void IconAButton::setFollowActorPane(LayoutActor* pActor, const char* pName) {
    mFollowActor = pActor;

    snprintf(mFollowPaneName, sizeof(mFollowPaneName), "%s", pName);
    MR::setFollowPos(&mFollowPos, this, nullptr);
}

bool IconAButton::isOpen() {
    return !MR::isDead(this);
}

bool IconAButton::isWait() {
    return isNerve(GET_NERVE(IconAButton, IconAButtonNrvWait));
}

void IconAButton::open() {
    appear();
    updateFollowPos();
    MR::showPane(this, "PicPlate");
    setNerve(GET_NERVE(IconAButton, IconAButtonNrvOpen));
}

void IconAButton::openWithTalk() {
    open();
    MR::setTextBoxLayoutMessageRecursive(this, "IconAButton", "Layout_SystemTalk");
}

void IconAButton::openWithRead() {
    open();
    MR::setTextBoxLayoutMessageRecursive(this, "IconAButton", "Layout_SystemRead");
}

void IconAButton::openWithNext() {
    open();
    MR::setTextBoxLayoutMessageRecursive(this, "IconAButton", "Layout_SystemNext");
}

void IconAButton::openWithTerm() {
    open();
    MR::setTextBoxLayoutMessageRecursive(this, "IconAButton", "Layout_SystemTerm");
}

void IconAButton::openWithDecide() {
    open();
    MR::setTextBoxLayoutMessageRecursive(this, "IconAButton", "Layout_SystemDecide");
}

void IconAButton::openWithTurn() {
    open();
    MR::setTextBoxLayoutMessageRecursive(this, "IconAButton", "Layout_SystemTurn");
}

void IconAButton::openWithoutMessage() {
    open();
    MR::hidePane(this, "PicPlate");
    MR::clearTextBoxMessageRecursive(this, "IconAButton");
}

void IconAButton::term() {
    if (isOpen() && (isNerve(GET_NERVE(IconAButton, IconAButtonNrvWait)) || isNerve(GET_NERVE(IconAButton, IconAButtonNrvOpen)))) {
        setNerve(GET_NERVE(IconAButton, IconAButtonNrvTerm));
    }
}

void IconAButton::exeOpen() {
    if (MR::isFirstStep(this)) {
        MR::startAnim(this, "Appear", 0);
    }

    if (MR::isAnimStopped(this, 0)) {
        setNerve(GET_NERVE(IconAButton, IconAButtonNrvWait));
    }
}

void IconAButton::exeWait() {
    if (MR::isFirstStep(this)) {
        MR::startAnim(this, "Press", 0);
    }
}

void IconAButton::exeTerm() {
    if (MR::isFirstStep(this)) {
        MR::startAnim(this, "End", 0);
    }

    if (MR::isAnimStopped(this, 0)) {
        kill();
    }
}

void IconAButton::control() {
    updateFollowPos();
}

void IconAButton::updateFollowPos() {
    if (mFollowActor != nullptr) {
        MR::copyPaneTrans(&mFollowPos, mFollowActor, mFollowPaneName);
    }
}
