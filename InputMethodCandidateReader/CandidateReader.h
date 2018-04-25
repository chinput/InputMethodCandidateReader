#pragma once

/* ======================================================================================

本示例主要功能：类实现TSF输入法编码、候选、中/英状态识别

★当程序需要自己绘制输入法候选界面，并且隐藏输入法原始界面时，需要此类功能
  尤其在全屏游戏程序中，需要按特定的规范获取TSF框架的输入法候选列表

★本类为此规范提供指导性方法

★本示例大部分代码复制于微软官方文档，同时保留了官方文档的英文注释。

★为了更好的阐述本示例要说明的问题，重要地方使用“中文”注释，请读者注意

★基本原理：
  1. 程序启动后，依据TSF框架规范，初始化必要的sink
  2. 输入法启动后，在关键事件回调sink
  3. 按确定的规则在回调中获取输入法数据

联系我: begin@vninv.com

========================================================================================== */

#include <windows.h>
#include <msctf.h>
#include <wchar.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

// IME States
#define IMEUI_STATE_OFF		0
#define IMEUI_STATE_ON		1
#define IMEUI_STATE_ENGLISH	2

struct Candidate 
{
    DWORD dwState;                 // 当前状态

    UINT uIndex;                   // 当前候选序号
    UINT uCount;                   // 总候选个数
    UINT uCurrentPage;             // 当前候选页序号
    UINT uPageCnt;                 // 候选页数
    DWORD dwPageStart;             // 当前页第一个候选在所有候选中的序号
    DWORD dwPageSize;              // 当前页候选个数

    wchar_t szComposing[256];      // 编码
    wchar_t szCandidate[10][256];  // 当前页候选
};

class CandidateReader
{
protected:
    // Sink receives event notifications
    class CUIElementSink : public ITfUIElementSink,
        public ITfInputProcessorProfileActivationSink,
        public ITfCompartmentEventSink,
        public ITfTextEditSink,
        public ITfThreadMgrEventSink
    {
    public:
        CUIElementSink();
        ~CUIElementSink();

        // IUnknown
        STDMETHODIMP    QueryInterface(REFIID riid, void** ppvObj);
        STDMETHODIMP_(ULONG)
            AddRef(void);
        STDMETHODIMP_(ULONG)
            Release(void);

        // ITfUIElementSink
        //   Notifications for Reading Window events. We could process candidate as well, but we'll use IMM for simplicity sake.
        STDMETHODIMP    BeginUIElement(DWORD dwUIElementId, BOOL* pbShow);
        STDMETHODIMP    UpdateUIElement(DWORD dwUIElementId);
        STDMETHODIMP    EndUIElement(DWORD dwUIElementId);

        // ITfInputProcessorProfileActivationSink
        //   Notification for keyboard input locale change
        STDMETHODIMP    OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid,
            REFGUID guidProfile, HKL hkl, DWORD dwFlags);

        // ITfCompartmentEventSink
        //    Notification for open mode (toggle state) change
        STDMETHODIMP    OnChange(REFGUID rguid);

        // ITfThreadMgrEventSink methods
        STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*);
        STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*);
        STDMETHODIMP OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*);
        STDMETHODIMP OnPushContext(ITfContext*);
        STDMETHODIMP OnPopContext(ITfContext*);

        // ITfTextEditSink methods
        STDMETHODIMP OnEndEdit(ITfContext*, TfEditCookie, ITfEditRecord*);


    public:
        void UpdateTextEditSink(ITfDocumentMgr* docMgr);
    private:
        LONG _cRef;
    };

    static void MakeReadingInformationString(ITfReadingInformationUIElement* preading);
    static void MakeCandidateStrings(ITfCandidateListUIElement* pcandidate);
    static ITfUIElement* GetUIElement(DWORD dwUIElementId);
    static BOOL GetCompartments(ITfCompartmentMgr** ppcm, ITfCompartment** ppTfOpenMode,
        ITfCompartment** ppTfConvMode);
    static BOOL SetupCompartmentSinks(BOOL bResetOnly = FALSE, ITfCompartment* pTfOpenMode = NULL,
        ITfCompartment* ppTfConvMode = NULL);

    static ITfThreadMgrEx* m_tm;
    static DWORD m_dwUIElementSinkCookie;
    static DWORD m_dwThreadMgrCookie;
    static DWORD m_dwTextEditCookie;
    static DWORD m_dwAlpnSinkCookie;
    static DWORD m_dwOpenModeSinkCookie;
    static DWORD m_dwConvModeSinkCookie;
    static CUIElementSink* m_TsfSink;
    static int m_nCandidateRefCount;	// Some IME shows multiple candidate lists but the Library doesn't support multiple candidate list. 
                                        // So track open / close events to make sure the candidate list opened last is shown.
    CandidateReader()
    {
    }	// this class can't be instanciated

public:
    static BOOL SetupSinks();
    static void ReleaseSinks();
    static BOOL CurrentInputLocaleIsIme();
    static void UpdateImeState(BOOL bResetCompartmentEventSink = FALSE);
    static void EnableUiUpdates(bool bEnable);
};


bool ImeUi_Initialize(HWND hwnd);
void ImeUi_Uninitialize();

extern Candidate g_Candidate;
