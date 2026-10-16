#include "Game/System/MessageHolder.hpp"
#include "Game/NPC/TalkMessageInfo.hpp"
#include "Game/NPC/TalkNodeCtrl.hpp"
#include "Game/System/GameSystemObjHolder.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/SystemUtil.hpp"
#include <JSystem/JKernel/JKRArchive.hpp>

#define SYSTEMMESSAGE_ARC "/Memory/SystemMessage.arc"
#define MESSAGE_ARC "/MessageData/Message.arc"

namespace {
#if TARGET_PC
    // BMG data is big-endian.
    inline u32 readU32(const u8* p) {
        return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
    }
    inline u16 readU16(const u8* p) {
        return (u16)((p[0] << 8) | p[1]);
    }
#define BMG_U32(p) readU32(p)
#define BMG_U16(p) readU16(p)
#else
#define BMG_U32(p) (*reinterpret_cast< u32* >(p))
#define BMG_U16(p) (*reinterpret_cast< u16* >(p))
#endif

    u8* getBlock(u32 magic, u8* pData) {
        u32 numBlocks = BMG_U32(pData + 0xc);
        pData += 0x20;

        for (u32 i = 0; i < numBlocks; i++) {
            u32 blockMagic = BMG_U32(pData);

            if (blockMagic == magic) {
                return pData;
            }

            u32 blockSize = BMG_U32(pData + 4);
            pData += blockSize;
        }

        return nullptr;
    }
};  // namespace

bool MessageData::getMessageDirect(TalkMessageInfo* pMessageInfo, const char* pMessage) const {
    s32 messageIndex = findMessageIndex(pMessage);

    if (messageIndex >= 0 && messageIndex < mInfoBlock->mItemCount) {
        getMessage(pMessageInfo, 0, messageIndex);

        return true;
    }

    return false;
}

bool MessageData::getMessage(TalkMessageInfo* pMessageInfo, u16, u16 infoToolIndex) const {
    u8* pInfoTool = getMessageInfoTool(infoToolIndex);
    pMessageInfo->_0 = reinterpret_cast< u8* >(mDataBlock + 1) + BMG_U32(pInfoTool);
    pMessageInfo->mCameraSetID = BMG_U16(pInfoTool + 4);
    pMessageInfo->_6 = *(pInfoTool + 6);
    pMessageInfo->mCameraType = *(pInfoTool + 7);
    pMessageInfo->mTalkType = *(pInfoTool + 8);
    pMessageInfo->_A = *(pInfoTool + 0xa);
    pMessageInfo->_B = *(pInfoTool + 0xb);
    pMessageInfo->mBalloonType = *(pInfoTool + 9);

    return true;
}

TalkNode* MessageData::findNode(const char* pMessage) const {
    s32 messageIndex = findMessageIndex(pMessage);

    for (int i = 0; i < mFlowBlock->mNodeCount; i++) {
        TalkNode* pNode = reinterpret_cast< TalkNode* >(mFlowBlock + 1) + i;

        if (pNode->mNodeType == 1 && pNode->mIndex == messageIndex) {
            return pNode;
        }
    }

    return nullptr;
}

TalkNode* MessageData::getNode(u32 index) const {
    return reinterpret_cast< TalkNode* >(mFlowBlock + 1) + index;
}

TalkNode* MessageData::getBranchNode(u32 index) const {
    return reinterpret_cast< TalkNode* >(mFlowBlock + 1) + BMG_U16(reinterpret_cast< u8* >(&_14[index]));
}

bool MessageData::isValidBranchNode(u32 index) const {
    return BMG_U16(reinterpret_cast< u8* >(&_14[index])) != 0xffff;
}

u8* MessageData::getMessageInfoTool(int index) const {
    return reinterpret_cast< u8* >(mInfoBlock + 1) + mInfoBlock->mItemSize * index;
}

MessageHolder::MessageHolder() : mSystemMessageData(nullptr), mGameMessageData(nullptr), mSceneMessageData(nullptr) {
}

void MessageHolder::initSceneData() {
    mSceneMessageData = mGameMessageData;
}

void MessageHolder::destroySceneData() {
    mSceneMessageData = nullptr;
}

bool MessageSystem::getSystemMessageDirect(TalkMessageInfo* pMessageInfo, const char* pMessageId) {
    return MR::getGameSystemObjHolder()->mMessageHolder->mSystemMessageData->getMessageDirect(pMessageInfo, pMessageId);
}

bool MessageSystem::getGameMessageDirect(TalkMessageInfo* pMessageInfo, const char* pMessageId) {
    return MR::getGameSystemObjHolder()->mMessageHolder->mGameMessageData->getMessageDirect(pMessageInfo, pMessageId);
}

bool MessageSystem::getLayoutMessageDirect(TalkMessageInfo* pMessageInfo, const char* pMessageId) {
    return MR::getGameSystemObjHolder()->mMessageHolder->mGameMessageData->getMessageDirect(pMessageInfo, pMessageId);
}

MessageData* MessageSystem::getSceneMessageData() {
    return MR::getGameSystemObjHolder()->mMessageHolder->mSceneMessageData;
}

MessageData::MessageData(const char* pArchiveName)
    : mIDTable(nullptr), mInfoBlock(nullptr), mDataBlock(nullptr), _C(0), mFlowBlock(nullptr), _14(nullptr), _18(nullptr), mFLI1Block(nullptr) {
    JKRArchive* pArchive = nullptr;
    JKRHeap* pHeap = nullptr;
    MR::getMountedArchiveAndHeap(pArchiveName, &pArchive, &pHeap);

    u8* msgData = (u8*)pArchive->getResource("Message.bmg");
    u8* mapData = (u8*)pArchive->getResource(QUESTIONMARK_MAGIC, "MessageId.tbl");

    mIDTable = new JMapInfo();
    mIDTable->attach(mapData);

    mInfoBlock = (MessageInfoBlock*)::getBlock('INF1', msgData);
    mDataBlock = (MessageDataBlock*)::getBlock('DAT1', msgData);
#if TARGET_PC
    // The text is big-endian UTF-16 and the game reads it as wchar_t (16-bit on Windows): convert
    // the characters to host order once. Message tags (0x1A, size, group, tag id, parameters) keep
    // their payload bytes as they are; MessageEditorMessageTag reads them big-endian. The block
    // magic is changed afterwards so a reused archive buffer isn't converted twice.
    if (mDataBlock != nullptr && mDataBlock->mMagic == 'DAT1') {
        u8* pText = reinterpret_cast< u8* >(mDataBlock + 1);
        u8* pEnd = reinterpret_cast< u8* >(mDataBlock) + mDataBlock->mBlockSize;
        while (pText + 1 < pEnd) {
            u16 code = (u16)((pText[0] << 8) | pText[1]);
            *reinterpret_cast< u16* >(pText) = code;
            if (code == 0x1A && pText + 2 < pEnd) {
                u8 tagSize = pText[2];
                pText += tagSize >= 2 ? tagSize : 2;
            } else {
                pText += 2;
            }
        }
        mDataBlock->mMagic = 'DAT!';
    }
#endif
    mFlowBlock = (MessageFlowBlock*)::getBlock('FLW1', msgData);

    if (mFlowBlock != nullptr) {
        _14 = reinterpret_cast< u16* >(reinterpret_cast< TalkNode* >(mFlowBlock + 1) + mFlowBlock->mNodeCount);
        _18 = reinterpret_cast< u8* >(_14 + mFlowBlock->_A);
    }

    mFLI1Block = (MessageFLI1Block*)::getBlock('FLI1', msgData);
}

inline JMapInfoIter end(const JMapInfo* pInfo) {
    return JMapInfoIter(pInfo, pInfo->getNumEntries());
}

s32 MessageData::findMessageIndex(const char* pMessage) const {
    JMapInfoIter iter = mIDTable->findElementBinary("MessageId", pMessage);

    // This *should* be mIDTable->end(), but I have had trouble
    // getting the compiler to inline that (see comment in JMapInfo.hpp)
    if (iter == end(mIDTable)) {
        return -1;
    }

    s32 messageIndex = -1;
    iter.getValue("Index", &messageIndex);

    return messageIndex;
}

void MessageHolder::initSystemData() {
    mSystemMessageData = new MessageData(SYSTEMMESSAGE_ARC);
}

void MessageHolder::initGameData() {
    MR::mountArchive(MESSAGE_ARC, nullptr);

    mGameMessageData = new MessageData(MESSAGE_ARC);
}
