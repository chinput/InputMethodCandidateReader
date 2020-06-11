#pragma once

/* ======================================================================================

��ʾ����Ҫ���ܣ���ʵ��TSF���뷨���롢��ѡ����/Ӣ״̬ʶ��

�ﵱ������Ҫ�Լ��������뷨��ѡ���棬�����������뷨ԭʼ����ʱ����Ҫ���๦��
  ������ȫ����Ϸ�����У���Ҫ���ض��Ĺ淶��ȡTSF��ܵ����뷨��ѡ�б�

�ﱾ��Ϊ�˹淶�ṩָ���Է���

�ﱾʾ���󲿷ִ��븴����΢��ٷ��ĵ���ͬʱ�����˹ٷ��ĵ���Ӣ��ע�͡�

��Ϊ�˸��õĲ�����ʾ��Ҫ˵�������⣬��Ҫ�ط�ʹ�á����ġ�ע�ͣ������ע��

�����ԭ��
  1. ��������������TSF��ܹ淶����ʼ����Ҫ��sink
  2. ���뷨�������ڹؼ��¼��ص�sink
  3. ��ȷ���Ĺ����ڻص��л�ȡ���뷨����

��ϵ��: begin@vninv.com

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
    DWORD dwState;                 // ��ǰ״̬

    UINT uIndex;                   // ��ǰ��ѡ���
    UINT uCount;                   // �ܺ�ѡ����
    UINT uCurrentPage;             // ��ǰ��ѡҳ���
    UINT uPageCnt;                 // ��ѡҳ��
    DWORD dwPageStart;             // ��ǰҳ��һ����ѡ�����к�ѡ�е����
    DWORD dwPageSize;              // ��ǰҳ��ѡ����

    wchar_t szComposing[256];      // ����
    wchar_t szCandidate[128][256];  // ��ǰҳ��ѡ
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
