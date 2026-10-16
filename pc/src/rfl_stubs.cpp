// Mii library (RVLFaceLib) for the PC port. There is no Mii on PC: the library initializes fine
// (like a Wii whose Mii Channel is empty, otherwise the file select shows a "can't read the Mii
// Channel" error) and the database has no Mii, so the file select offers its default icons.

#include <RVLFaceLib.h>

extern "C" {

void RFLExit(void) {}
RFLErrcode RFLInitResAsync(void*, void*, u32, BOOL) { return RFLErrcode_Success; }
RFLErrcode RFLGetAsyncStatus(void) { return RFLErrcode_Success; }
s32 RFLGetLastReason(void) { return 0; }
u32 RFLGetWorkSize(BOOL) { return 0; }
u32 RFLGetModelBufferSize(RFLResolution, u32) { return 0; }

BOOL RFLIsAvailableOfficialData(u16) { return FALSE; }
BOOL RFLSearchOfficialData(const RFLCreateID*, u16*) { return FALSE; }
RFLErrcode RFLGetAdditionalInfo(RFLAdditionalInfo*, RFLDataSource, struct RFLMiddleDB*, u16) {
    return RFLErrcode_NotAvailable;
}

RFLErrcode RFLInitCharModel(RFLCharModel*, RFLDataSource, struct RFLMiddleDB*, u16, void*, RFLResolution, u32) {
    return RFLErrcode_NotAvailable;
}
void RFLSetMtx(RFLCharModel*, const Mtx) {}
void RFLSetExpression(RFLCharModel*, RFLExpression) {}
void RFLLoadVertexSetting(const RFLDrawCoreSetting*) {}
void RFLLoadMaterialSetting(const RFLDrawCoreSetting*) {}
void RFLDrawOpa(const RFLCharModel*) {}
void RFLDrawXlu(const RFLCharModel*) {}
void RFLDrawOpaCore(const RFLCharModel*, const RFLDrawCoreSetting*) {}
void RFLDrawXluCore(const RFLCharModel*, const RFLDrawCoreSetting*) {}
RFLErrcode RFLMakeIcon(void*, RFLDataSource, RFLMiddleDB*, u16, RFLExpression, const RFLIconSetting*) {
    return RFLErrcode_NotAvailable;
}

}  // extern "C"
