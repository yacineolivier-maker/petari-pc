#include "Game/System/MessageHolder.hpp"
#include "Game/NPC/TalkMessageInfo.hpp"
#include "Game/NPC/TalkNodeCtrl.hpp"
#include "Game/System/GameSystemObjHolder.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/SystemUtil.hpp"
#include <JSystem/JKernel/JKRArchive.hpp>
#if TARGET_PC
#include "Game/Screen/MessageTagSkipTagProcessor.hpp"
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#endif

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

#if TARGET_PC
extern "C" unsigned char pc_settings_language();  // SC_LANG_*: 3 French, 1 English, 4 Spanish

namespace {
    bool pcIsLetter(wchar_t c) {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || c >= 0xC0;
    }

    // Length (in wchar_t) of the control tag at p (p[0] == 0x1A).
    size_t pcTagLength(const wchar_t* p) {
        MessageEditorMessageTag tag(p + 1);
        return tag.getSkipLength() + 1;
    }

    bool pcIsPictureTag(const wchar_t* p) {
        return *p == 0x1A && MessageEditorMessageTag(p + 1).getGroup() == 3;
    }

    // Skips spaces and control tags other than pictures (colors, sizes...).
    const wchar_t* pcSkipTagsAndSpaces(const wchar_t* p) {
        while ((*p == 0x1A && !pcIsPictureTag(p)) || *p == L' ') {
            p += *p == 0x1A ? pcTagLength(p) : 1;
        }
        return p;
    }

    // The French texts tell to shake the Wii Remote with its icon: "Secoue la [Wii Remote]". On PC
    // the icon is the gamepad's spin button (see pc/src/ui/xbox_glyphs_gdi.cpp), so the sentence
    // becomes "Appuie sur [B]" (same for "Secouez"/"secouer"). Control tags are kept as they are.
    const wchar_t* pcRewriteShakeMessage(const wchar_t* pOriginal) {
        static std::mutex sMutex;
        static std::unordered_map< const wchar_t*, std::wstring > sCache;
        std::lock_guard< std::mutex > lock(sMutex);
        PcHostNewScope hostNew;  // the cache lives as long as the game
        auto it = sCache.find(pOriginal);
        if (it != sCache.end()) {
            return it->second.empty() ? pOriginal : it->second.c_str();
        }

        static const struct {
            const wchar_t* from;
            const wchar_t* to;
        } kVerbs[] = {
            {L"Secouez", L"Appuyez"}, {L"secouez", L"appuyez"}, {L"Secouer", L"Appuyer"},
            {L"secouer", L"appuyer"}, {L"Secoue", L"Appuie"},   {L"secoue", L"appuie"},
        };

        std::wstring out;
        bool changed = false;
        const wchar_t* p = pOriginal;
        while (*p != 0) {
            if (*p == 0x1A) {
                const size_t len = pcTagLength(p);
                out.append(p, len);
                p += len;
                continue;
            }
            bool replaced = false;
            if (p == pOriginal || !pcIsLetter(p[-1])) {
                for (const auto& verb : kVerbs) {
                    const size_t n = wcslen(verb.from);
                    if (wcsncmp(p, verb.from, n) != 0 || pcIsLetter(p[n])) {
                        continue;
                    }
                    // "<verb> [tags] la [tags] <icon tag>"
                    const wchar_t* q = pcSkipTagsAndSpaces(p + n);
                    if (q[0] != L'l' || q[1] != L'a' || pcIsLetter(q[2])) {
                        break;
                    }
                    const wchar_t* icon = pcSkipTagsAndSpaces(q + 2);
                    if (!pcIsPictureTag(icon)) {
                        break;
                    }
                    out += verb.to;
                    out.append(p + n, q - (p + n));
                    out += L"sur";
                    out.append(q + 2, icon - (q + 2));
                    p = icon;
                    replaced = changed = true;
                    break;
                }
            }
            if (!replaced) {
                out += *p++;
            }
        }
        std::wstring& entry = sCache[pOriginal];
        if (changed) {
            entry = std::move(out);
            return entry.c_str();
        }
        return pOriginal;
    }

    // Texts changed by the PC port (the controls differ from the Wii's). Built once from the
    // original message so its control tags (button icons) are kept.
    const wchar_t* pcOverrideMessage(const char* pId, const wchar_t* pOriginal) {
        // Title screen: A alone starts on PC ("Appuyez en même temps sur [A] et [B]." on the Wii).
        if (std::strcmp(pId, "Layout_PressStartPressAB") == 0) {
            static std::wstring sText;
            if (sText.empty()) {
                PcHostNewScope hostNew;  // lives as long as the game
                const wchar_t* pTag = pOriginal;
                while (*pTag != 0 && *pTag != 0x1A) {
                    pTag++;
                }
                if (*pTag == 0) {
                    return pOriginal;
                }
                MessageEditorMessageTag tag(pTag + 1);
                const unsigned char language = pc_settings_language();
                sText = language == 1 ? L"Press " : language == 4 ? L"Presiona " : L"Appuyez sur ";
                sText.append(pTag, tag.getSkipLength() + 1);
                sText += L".";
            }
            return sText.c_str();
        }
        return pcRewriteShakeMessage(pOriginal);
    }
}  // namespace
#endif

bool MessageData::getMessageDirect(TalkMessageInfo* pMessageInfo, const char* pMessage) const {
    s32 messageIndex = findMessageIndex(pMessage);

    if (messageIndex >= 0 && messageIndex < mInfoBlock->mItemCount) {
        getMessage(pMessageInfo, 0, messageIndex);
#if TARGET_PC
        pMessageInfo->_0 = (u8*)pcOverrideMessage(pMessage, reinterpret_cast< const wchar_t* >(pMessageInfo->_0));
#endif

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
#if TARGET_PC
    pMessageInfo->_0 = (u8*)pcRewriteShakeMessage(reinterpret_cast< const wchar_t* >(pMessageInfo->_0));
#endif

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
#if TARGET_PC
    // SMGPC_TEST_MESSAGES=1: print every message the PC port rewrites (tags shown as [..]).
    if (getenv("SMGPC_TEST_MESSAGES") != nullptr && mInfoBlock != nullptr) {
        for (u16 i = 0; i < mInfoBlock->mItemCount; i++) {
            const wchar_t* pOriginal = reinterpret_cast< const wchar_t* >(reinterpret_cast< u8* >(mDataBlock + 1) + BMG_U32(getMessageInfoTool(i)));
            const wchar_t* pText = pcRewriteShakeMessage(pOriginal);
            if (pText == pOriginal) {
                continue;
            }
            std::string utf8;
            for (const wchar_t* p = pText; *p != 0;) {
                if (*p == 0x1A) {
                    utf8 += "[..]";
                    p += pcTagLength(p);
                    continue;
                }
                const wchar_t c = *p++;
                if (c < 0x80) {
                    utf8 += char(c);
                } else if (c < 0x800) {
                    utf8 += char(0xC0 | (c >> 6));
                    utf8 += char(0x80 | (c & 0x3F));
                } else {
                    utf8 += char(0xE0 | (c >> 12));
                    utf8 += char(0x80 | ((c >> 6) & 0x3F));
                    utf8 += char(0x80 | (c & 0x3F));
                }
            }
            OSReport("[pc] message %u rewritten: %s\n", i, utf8.c_str());
        }
    }
#endif
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
