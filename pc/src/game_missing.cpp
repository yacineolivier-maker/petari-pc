// Game and JSystem functions the PC build needs but the decomp doesn't define.
//
// Virtual functions declared without a body in the decomp's headers: they are pure virtual in the
// original game (no symbol in the DOL), but the decomp declares them as plain virtuals, so MSVC's
// vtables reference them. They get harmless default bodies here.

#include "Game/AreaObj/AreaForm.hpp"
#include "Game/MapObj/SimpleBreakableObj.hpp"
#include "Game/System/StationedArchiveLoader.hpp"
#include "JSystem/J3DGraphBase/J3DPacket.hpp"
#include "JSystem/JAudio2/JAISe.hpp"
#include "JSystem/JAudio2/JAISound.hpp"
#include "JSystem/JAudio2/JASInstEffect.hpp"

void AreaForm::init(const JMapInfoIter&) {}
bool AreaForm::isInVolume(const TVec3f&) const { return false; }

bool StationedArchiveLoader::Condition::isExecute(const MR::StationedFileInfo*) const { return false; }

void J3DPacket::draw() {}

void JASInstEffect::effect(int, int, JASInstParam*) const {}

template <>
void JAISoundStrategy< JAISe >::calc(JAISe*) {}
template <>
void JAISoundStrategy< JAISe >::mix(JAISe*, JASSoundParams*) {}

void SimpleBreakableObjBase::control() {}
