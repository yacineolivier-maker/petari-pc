// Mii library (RVLFaceLib) for the PC port. There is no Mii database on PC: the library reports
// "not available", which the game already handles (Miis are optional in Super Mario Galaxy: the
// file select shows its default icons instead).

#include <RVLFaceLib.h>

extern "C" {

void RFLExit(void) {}
RFLErrcode RFLInitResAsync(void*, void*, u32, BOOL) { return RFLErrcode_NotAvailable; }
RFLErrcode RFLGetAsyncStatus(void) { return RFLErrcode_NotAvailable; }
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
