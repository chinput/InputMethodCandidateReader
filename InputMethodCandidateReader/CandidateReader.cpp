#include "CandidateReader.h"

HWND g_hwndMain = NULL;
HWND g_hInfoEdit = NULL;
static bool                     g_bChineseIME;
static bool                     g_bUILessMode = false;
static bool                     g_bCandList = false;
Candidate                       g_Candidate = { 0 };

#include <sstream>
static void InvalidatMainWindow()
{
    std::wstringstream wss;

    wchar_t szt[512];
    swprintf_s(szt, L"转换候选列表:        %s", g_Candidate.dwState == IMEUI_STATE_ON ? L"开启" : L"关闭");
    wss << szt << L"\r\n";

    swprintf_s(szt, L"输入编码:            %s", g_Candidate.szComposing);
    wss << szt << L"\r\n";

    swprintf_s(szt, L"总候选个数:          %d", g_Candidate.uCount);
    wss << szt << L"\r\n";

    swprintf_s(szt, L"总候选页数:          %d", g_Candidate.uPageCnt);
    wss << szt << L"\r\n";

    swprintf_s(szt, L"当前候选页序号:      %d", g_Candidate.uCurrentPage);
    wss << szt << L"\r\n";

    swprintf_s(szt, L"当前页首候选序号:    %d", g_Candidate.dwPageStart);
    wss << szt << L"\r\n";

    swprintf_s(szt, L"当前页候选个数:      %d", g_Candidate.dwPageSize);
    wss << szt << L"\r\n\r\n";

    swprintf_s(szt, L"选中候选序号:        %d", g_Candidate.uIndex);
    wss << szt << L"\r\n";

    swprintf_s(szt, L"选中候选:            %s", g_Candidate.szCandidate[g_Candidate.uIndex - g_Candidate.dwPageStart]);
    wss << szt << L"\r\n\r\n";

    for (UINT i = 0; i < g_Candidate.dwPageSize && i < ARRAYSIZE(g_Candidate.szCandidate); i++)
    {
        wss << i + 1 << L"."
            << g_Candidate.szCandidate[i] << L"    ";
    }

    SetWindowText(g_hInfoEdit, wss.str().c_str());
}


static void CloseCandidateList()
{
    g_bCandList = false;
    DWORD t = g_Candidate.dwState;
    memset(&g_Candidate, 0, sizeof(g_Candidate));
    g_Candidate.dwState = t;
    InvalidatMainWindow();
}


bool ImeUi_Initialize(HWND hwnd)
{
    g_hwndMain = hwnd;
    g_bUILessMode = CandidateReader::SetupSinks() != FALSE;
    return true;
}

void ImeUi_Uninitialize()
{
    CandidateReader::ReleaseSinks();
}

// Returns open mode compartments and compartment manager.
// Function fails if it fails to acquire any of the objects to be returned.
BOOL CandidateReader::GetCompartments(ITfCompartmentMgr** ppcm, ITfCompartment** ppTfOpenMode,
    ITfCompartment** ppTfConvMode)
{
    ITfCompartmentMgr* pcm = NULL;
    ITfCompartment* pTfOpenMode = NULL;
    ITfCompartment* pTfConvMode = NULL;

    static GUID _GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION =
    {
        0xCCF05DD8, 0x4A87, 0x11D7, 0xA6, 0xE2, 0x00, 0x06, 0x5B, 0x84, 0x43, 0x5C
    };

    HRESULT hr;
    if (SUCCEEDED(hr = m_tm->QueryInterface(IID_ITfCompartmentMgr, (void**)&pcm)))
    {
        if (SUCCEEDED(hr = pcm->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pTfOpenMode)))
        {
            if (SUCCEEDED(hr = pcm->GetCompartment(_GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                &pTfConvMode)))
            {
                *ppcm = pcm;
                *ppTfOpenMode = pTfOpenMode;
                *ppTfConvMode = pTfConvMode;
                return TRUE;
            }
            pTfOpenMode->Release();
        }
        pcm->Release();
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
//
//	CandidateReader methods
//
///////////////////////////////////////////////////////////////////////////////

ITfThreadMgrEx*                 CandidateReader::m_tm;
DWORD                           CandidateReader::m_dwUIElementSinkCookie = TF_INVALID_COOKIE;
DWORD                           CandidateReader::m_dwThreadMgrCookie = TF_INVALID_COOKIE;
DWORD                           CandidateReader::m_dwTextEditCookie = TF_INVALID_COOKIE;
DWORD                           CandidateReader::m_dwAlpnSinkCookie = TF_INVALID_COOKIE;
DWORD                           CandidateReader::m_dwOpenModeSinkCookie = TF_INVALID_COOKIE;
DWORD                           CandidateReader::m_dwConvModeSinkCookie = TF_INVALID_COOKIE;
CandidateReader::CUIElementSink* CandidateReader::m_TsfSink = NULL;
int                             CandidateReader::m_nCandidateRefCount = NULL;

//
//	SetupSinks()
//	Set up sinks. A sink is used to receive a Text Service Framework event.
//  CUIElementSink implements multiple sink interfaces to receive few different TSF events.
//
BOOL CandidateReader::SetupSinks()
{
    // ITfThreadMgrEx is available on Vista or later.
    HRESULT hr;
    hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITfThreadMgrEx), (void**)&m_tm);
    if (hr != S_OK)
    {
        return FALSE;
    }

    // Setup sinks
    BOOL bRc = FALSE;
    m_TsfSink = new CUIElementSink();
    if (m_TsfSink)
    {
        ITfSource* srcTm;
        if (SUCCEEDED(hr = m_tm->QueryInterface(__uuidof(ITfSource), (void**)&srcTm)))
        {
            hr = srcTm->AdviseSink(IID_ITfThreadMgrEventSink, (ITfThreadMgrEventSink*)m_TsfSink, &m_dwThreadMgrCookie);

            // Sink for reading window change
            if (SUCCEEDED(hr = srcTm->AdviseSink(__uuidof(ITfUIElementSink), (ITfUIElementSink*)m_TsfSink,
                &m_dwUIElementSinkCookie)))
            {
                // Sink for input locale change
                if (SUCCEEDED(hr = srcTm->AdviseSink(__uuidof(ITfInputProcessorProfileActivationSink),
                    (ITfInputProcessorProfileActivationSink*)m_TsfSink,
                    &m_dwAlpnSinkCookie)))
                {
                    if (SetupCompartmentSinks())	// Setup compartment sinks for the first time
                    {
                        bRc = TRUE;
                    }
                }
            }

            ITfDocumentMgr* doc_mgr = NULL;
            m_tm->GetFocus(&doc_mgr);
            if (doc_mgr) {
                m_TsfSink->UpdateTextEditSink(doc_mgr);
                doc_mgr->Release();
            }

            srcTm->Release();
        }
    }
    return bRc;
}

void CandidateReader::ReleaseSinks()
{
    HRESULT hr;
    ITfSource* source;

    // Remove all sinks
    if (m_tm && SUCCEEDED(m_tm->QueryInterface(__uuidof(ITfSource), (void**)&source)))
    {
        hr = source->UnadviseSink(m_dwThreadMgrCookie);
        hr = source->UnadviseSink(m_dwTextEditCookie);
        hr = source->UnadviseSink(m_dwUIElementSinkCookie);
        hr = source->UnadviseSink(m_dwAlpnSinkCookie);
        source->Release();
        SetupCompartmentSinks(TRUE);	// Remove all compartment sinks
        //m_tm->Deactivate();
        SAFE_RELEASE(m_tm);
        SAFE_RELEASE(m_TsfSink);
    }
}

CandidateReader::CUIElementSink::CUIElementSink()
{
    _cRef = 1;
}


CandidateReader::CUIElementSink::~CUIElementSink()
{
}

STDAPI CandidateReader::CUIElementSink::QueryInterface(REFIID riid, void** ppvObj)
{
    if (ppvObj == NULL)
        return E_INVALIDARG;

    *ppvObj = NULL;

    if (IsEqualIID(riid, IID_IUnknown))
    {
        *ppvObj = reinterpret_cast<IUnknown*>(this);
    }
    else if (IsEqualIID(riid, __uuidof(ITfUIElementSink)))
    {
        // 输入法候选列表更新时回调
        *ppvObj = (ITfUIElementSink*)this;
    }
    else if (IsEqualIID(riid, __uuidof(ITfInputProcessorProfileActivationSink)))
    {
        // 输入法激活时回调
        *ppvObj = (ITfInputProcessorProfileActivationSink*)this;
    }
    else if (IsEqualIID(riid, __uuidof(ITfCompartmentEventSink)))
    {
        // 输入法中/英状态更改时回调
        *ppvObj = (ITfCompartmentEventSink*)this;
    }
    else if (IsEqualIID(riid, IID_ITfTextEditSink)) {
        *ppvObj = (ITfTextEditSink*)this;
    }
    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDAPI_(ULONG) CandidateReader::CUIElementSink::AddRef()
{
    return ++_cRef;
}

STDAPI_(ULONG) CandidateReader::CUIElementSink::Release()
{
    LONG cr = --_cRef;

    if (_cRef == 0)
    {
        delete this;
    }

    return cr;
}

STDAPI CandidateReader::CUIElementSink::BeginUIElement(DWORD dwUIElementId, BOOL* pbShow)
{
    ITfUIElement* pElement = GetUIElement(dwUIElementId);
    if (!pElement)
        return E_INVALIDARG;

    ITfReadingInformationUIElement* preading = NULL;
    ITfCandidateListUIElement* pcandidate = NULL;
    //*pbShow = FALSE;
    *pbShow = TRUE;
    if (!g_bCandList && SUCCEEDED(pElement->QueryInterface(__uuidof(ITfReadingInformationUIElement),
        (void**)&preading)))
    {
        MakeReadingInformationString(preading);
        preading->Release();
    }
    else if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement),
        (void**)&pcandidate)))
    {
        m_nCandidateRefCount++;
        MakeCandidateStrings(pcandidate);
        pcandidate->Release();
    }

    pElement->Release();
    return S_OK;
}

STDAPI CandidateReader::CUIElementSink::UpdateUIElement(DWORD dwUIElementId)
{
    ITfUIElement* pElement = GetUIElement(dwUIElementId);
    if (!pElement)
        return E_INVALIDARG;

    ITfReadingInformationUIElement* preading = NULL;
    ITfCandidateListUIElement* pcandidate = NULL;
    if (!g_bCandList && SUCCEEDED(pElement->QueryInterface(__uuidof(ITfReadingInformationUIElement),
        (void**)&preading)))
    {
        MakeReadingInformationString(preading);
        preading->Release();
    }
    else if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement),
        (void**)&pcandidate)))
    {
        MakeCandidateStrings(pcandidate);
        pcandidate->Release();
    }

    pElement->Release();
    return S_OK;
}

STDAPI CandidateReader::CUIElementSink::EndUIElement(DWORD dwUIElementId)
{
    ITfUIElement* pElement = GetUIElement(dwUIElementId);
    if (!pElement)
        return E_INVALIDARG;

    ITfReadingInformationUIElement* preading = NULL;
    if (!g_bCandList && SUCCEEDED(pElement->QueryInterface(__uuidof(ITfReadingInformationUIElement),
        (void**)&preading)))
    {
        preading->Release();
    }

    ITfCandidateListUIElement* pcandidate = NULL;
    if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement),
        (void**)&pcandidate)))
    {
        m_nCandidateRefCount--;
        if (m_nCandidateRefCount == 0)
            CloseCandidateList();
        pcandidate->Release();
    }

    pElement->Release();
    return S_OK;
}

void CandidateReader::UpdateImeState(BOOL bResetCompartmentEventSink)
{
    ITfCompartmentMgr* pcm;
    ITfCompartment* pTfOpenMode = NULL;
    ITfCompartment* pTfConvMode = NULL;
    if (GetCompartments(&pcm, &pTfOpenMode, &pTfConvMode))
    {
        VARIANT valOpenMode;
        VARIANT valConvMode;
        pTfOpenMode->GetValue(&valOpenMode);
        pTfConvMode->GetValue(&valConvMode);
        if (valOpenMode.vt == VT_I4)
        {
            if (g_bChineseIME)
            {
                g_Candidate.dwState = valOpenMode.lVal != 0 && valConvMode.lVal != 0 ? IMEUI_STATE_ON : IMEUI_STATE_ENGLISH;
            }
            else
            {
                g_Candidate.dwState = valOpenMode.lVal != 0 ? IMEUI_STATE_ON : IMEUI_STATE_OFF;
            }
        }
        VariantClear(&valOpenMode);
        VariantClear(&valConvMode);

        if (bResetCompartmentEventSink)
        {
            SetupCompartmentSinks(FALSE, pTfOpenMode, pTfConvMode);	// Reset compartment sinks
        }
        pTfOpenMode->Release();
        pTfConvMode->Release();
        pcm->Release();

        // 通知界面刷新
        InvalidatMainWindow();
    }
}

STDAPI CandidateReader::CUIElementSink::OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags)
{
    if (IsEqualIID(catid, GUID_TFCAT_TIP_KEYBOARD) && (dwFlags & TF_IPSINK_FLAG_ACTIVE))
    {
#define LANG_CHS MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)
#define LANG_CHT MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL)
        g_bChineseIME = (dwProfileType & TF_PROFILETYPE_INPUTPROCESSOR) && langid == LANG_CHT;
        if (dwProfileType & TF_PROFILETYPE_INPUTPROCESSOR)
        {
            UpdateImeState(TRUE);
        }
        else
            g_Candidate.dwState = IMEUI_STATE_OFF;
    }
    return S_OK;
}

STDAPI CandidateReader::CUIElementSink::OnChange(REFGUID rguid)
{
    UpdateImeState();
    return S_OK;
}

IEnumITfCompositionView* GetCompViewEnum(ITfContext* pCtx)
{
    // Make sure there is a composition context
    ITfContextComposition* pctxcomp = NULL;
    pCtx->QueryInterface(IID_ITfContextComposition, (void**)&pctxcomp);
    if (!pctxcomp)  return NULL;

    // Obtain composition view enumerator
    IEnumITfCompositionView* enum_view = NULL;
    pctxcomp->EnumCompositions(&enum_view);
    pctxcomp->Release();
    return enum_view;
}

ITfRange* CombineCompRange(ITfContext* pCtx, TfEditCookie cookie)
{
    // Make sure there is a composition view enumerator
    IEnumITfCompositionView* pEnumview = GetCompViewEnum(pCtx);
    if (!pEnumview)  return NULL;

    // Combine composition ranges from all views
    ITfRange*           range = NULL;
    ITfCompositionView* view = NULL;
    while (pEnumview->Next(1, &view, NULL) == S_OK)
    {
        ITfRange *prange = NULL;
        if (view->GetRange(&prange) == S_OK)
        {
            if (!range)
            {
                prange->Clone(&range);
            }
            else
            {
                range->ShiftEndToRange(cookie, prange, TF_ANCHOR_END);
            }
            prange->Release();
        }
        view->Release();
    }
    pEnumview->Release();
    return range;
}

bool fetchRangeExtent(ITfRange* pRange, long* start, ULONG* length)
{
    HRESULT res = S_OK;
    if (!pRange) return false;
    ITfRangeACP* pRangeACP = NULL;
    res = pRange->QueryInterface(IID_ITfRangeACP, (void**)&pRangeACP);
    if (res != S_OK || !pRangeACP) return false;
    res = pRangeACP->GetExtent(start, (long*)length);
    pRangeACP->Release();
    return true ? (res == S_OK) : false;
}


STDMETHODIMP CandidateReader::CUIElementSink::OnInitDocumentMgr(ITfDocumentMgr* pDIM)
{
    return S_OK;
}

STDMETHODIMP CandidateReader::CUIElementSink::OnUninitDocumentMgr(ITfDocumentMgr* pDIM)
{
    return S_OK;
}

STDMETHODIMP CandidateReader::CUIElementSink::OnSetFocus(
    ITfDocumentMgr* pDIM, ITfDocumentMgr* pPrevDIM)
{
    UpdateTextEditSink(pDIM);
    return S_OK;
}

STDMETHODIMP CandidateReader::CUIElementSink::OnPushContext(ITfContext* pCtx)
{
    return S_OK;
}

STDMETHODIMP CandidateReader::CUIElementSink::OnPopContext(ITfContext* pCtx)
{
    return S_OK;
}

STDMETHODIMP CandidateReader::CUIElementSink::OnEndEdit(
    ITfContext* pCtx, TfEditCookie cookie, ITfEditRecord* pEditRec)
{
    // TSF input processor performing composition
    ITfRange* pRange = CombineCompRange(pCtx, cookie);
    if (!pRange)
    {
        return S_OK;
    }
    ULONG len = ARRAYSIZE(g_Candidate.szComposing) - 1;
    pRange->GetText(cookie, 0, g_Candidate.szComposing, len, &len);
    g_Candidate.szComposing[min(len, 255)] = L'\0';
    long compStart = 0;
    fetchRangeExtent(pRange, &compStart, &len);
    long selStart = compStart;
    long selEnd = compStart;
    TF_SELECTION tfSelection = { 0 };
    if (pCtx->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &tfSelection, &len) == S_OK && tfSelection.range)
    {
        if (fetchRangeExtent(tfSelection.range, &selStart, &len))
        {
            selEnd = selStart + len;
        }
        tfSelection.range->Release();
    }
    selStart = max(0, selStart - compStart);
    selEnd = max(0, selEnd - compStart);

    // 通知界面刷新
    InvalidatMainWindow();
    return S_OK;
}

void CandidateReader::CUIElementSink::UpdateTextEditSink(ITfDocumentMgr* docMgr)
{
    if (!docMgr)  return;
    ITfContext* ctx = NULL;
    HRESULT hr = docMgr->GetBase(&ctx);
    if (hr == S_OK)
    {
        ITfSource* src = NULL;
        if (SUCCEEDED(hr = ctx->QueryInterface(IID_ITfSource, (void**)&src)))
        {
            hr = src->AdviseSink(IID_ITfTextEditSink, (ITfTextEditSink*)this, &m_dwTextEditCookie);
            src->Release();
        }
        ctx->Release();
    }
}

void CandidateReader::MakeReadingInformationString(ITfReadingInformationUIElement* preading)
{
    UINT cchMax;
    UINT uErrorIndex = 0;
    BOOL fVertical;
    DWORD dwFlags;

    preading->GetUpdatedFlags(&dwFlags);
    preading->GetMaxReadingStringLength(&cchMax);
    preading->GetErrorIndex(&uErrorIndex);	// errorIndex is zero-based
    preading->IsVerticalOrderPreferred(&fVertical);

    BSTR bstr;
    if (SUCCEEDED(preading->GetString(&bstr)))
    {
        if (bstr)
        {
            wcscpy_s(g_Candidate.szComposing, bstr);

            // 应注意TSF框架要求输入内部必须使用SysAlloc() 分配候选列表字符串保存空间
            //在调用pcandidate->GetString 之后，必须使用SysFreeString(bstr)释放
            SysFreeString(bstr);

            // 通知界面刷新
            InvalidatMainWindow();
        }
    }
}

void CandidateReader::MakeCandidateStrings(ITfCandidateListUIElement* pcandidate)
{
    if (pcandidate)
    {
        BSTR bstr;
        UINT *pIndexList = NULL;
        pcandidate->GetSelection(&g_Candidate.uIndex);            //获取当前选中状态的候选序号（可设置高亮显示，一般为第一候选）
        pcandidate->GetCount(&g_Candidate.uCount);                //当前候选列表总数
        pcandidate->GetCurrentPage(&g_Candidate.uCurrentPage);    //当前候选列表所在的页

        g_bCandList = true;

        pcandidate->GetPageIndex(NULL, 0, &g_Candidate.uPageCnt); //获取候选列表页每一页对应的起始序号
        if (g_Candidate.uPageCnt > 0)
        {
            pIndexList = (UINT *)malloc(sizeof(UINT)*g_Candidate.uPageCnt);
            if (pIndexList)
            {
                pcandidate->GetPageIndex(pIndexList, g_Candidate.uPageCnt, &g_Candidate.uPageCnt);
                g_Candidate.dwPageStart = pIndexList[g_Candidate.uCurrentPage];
                g_Candidate.dwPageSize = (g_Candidate.uCurrentPage < g_Candidate.uPageCnt - 1) ?
                    min(g_Candidate.uCount, pIndexList[g_Candidate.uCurrentPage + 1]) - g_Candidate.dwPageStart :
                    g_Candidate.uCount - g_Candidate.dwPageStart;
            }
        }

        UINT uCandPageSize = min(g_Candidate.dwPageSize, 10);  // 本示例的g_Candidate.szCandidate最大个数为10, 因此min处理
        for (UINT i = g_Candidate.dwPageStart, j = 0; (DWORD)i < g_Candidate.uCount && j < 128; i++, j++)
        {
            if (SUCCEEDED(pcandidate->GetString(i, &bstr)))  //获取候选列表的第i个候选串
            {
                if (bstr)
                {
                    wcscpy_s(g_Candidate.szCandidate[j], bstr);

                    // 应注意TSF框架要求输入内部必须使用SysAlloc() 分配候选列表字符串保存空间
                    //在调用pcandidate->GetString 之后，必须使用SysFreeString(bstr)释放
                    SysFreeString(bstr);
                }
            }
        }
        if (pIndexList)
        {
            free(pIndexList);
        }
        // 通知界面刷新
        InvalidatMainWindow();
    }
}

ITfUIElement* CandidateReader::GetUIElement(DWORD dwUIElementId)
{
    ITfUIElementMgr* puiem;
    ITfUIElement* pElement = NULL;

    if (SUCCEEDED(m_tm->QueryInterface(__uuidof(ITfUIElementMgr), (void**)&puiem)))
    {
        puiem->GetUIElement(dwUIElementId, &pElement);
        puiem->Release();
    }

    return pElement;
}

BOOL CandidateReader::CurrentInputLocaleIsIme()
{
    BOOL ret = FALSE;
    HRESULT hr;

    ITfInputProcessorProfiles* pProfiles;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITfInputProcessorProfiles), (LPVOID*)&pProfiles);
    if (SUCCEEDED(hr))
    {
        ITfInputProcessorProfileMgr* pProfileMgr;
        hr = pProfiles->QueryInterface(__uuidof(ITfInputProcessorProfileMgr), (LPVOID*)&pProfileMgr);
        if (SUCCEEDED(hr))
        {
            TF_INPUTPROCESSORPROFILE tip;
            hr = pProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &tip);
            if (SUCCEEDED(hr))
            {
                ret = (tip.dwProfileType & TF_PROFILETYPE_INPUTPROCESSOR) != 0;
            }
            pProfileMgr->Release();
        }
        pProfiles->Release();
    }
    return ret;
}

// Sets up or removes sink for UI element. 
// UI element sink should be removed when IME is disabled,
// otherwise the sink can be triggered when a game has multiple instances of IME UI library.
void CandidateReader::EnableUiUpdates(bool bEnable)
{
    if (m_tm == NULL ||
        (bEnable && m_dwUIElementSinkCookie != TF_INVALID_COOKIE) ||
        (!bEnable && m_dwUIElementSinkCookie == TF_INVALID_COOKIE))
    {
        return;
    }
    ITfSource* srcTm = NULL;
    HRESULT hr = E_FAIL;
    if (SUCCEEDED(hr = m_tm->QueryInterface(__uuidof(ITfSource), (void**)&srcTm)))
    {
        if (bEnable)
        {
            hr = srcTm->AdviseSink(__uuidof(ITfUIElementSink), (ITfUIElementSink*)m_TsfSink,
                &m_dwUIElementSinkCookie);
        }
        else
        {
            hr = srcTm->UnadviseSink(m_dwUIElementSinkCookie);
            m_dwUIElementSinkCookie = TF_INVALID_COOKIE;
        }
        srcTm->Release();
    }
}

// There are three ways to call this function:
// SetupCompartmentSinks() : initialization
// SetupCompartmentSinks(FALSE, openmode, convmode) : Resetting sinks. This is necessary as DaYi and Array IME resets compartment on switching input locale
// SetupCompartmentSinks(TRUE) : clean up sinks
BOOL CandidateReader::SetupCompartmentSinks(BOOL bRemoveOnly, ITfCompartment* pTfOpenMode,
    ITfCompartment* pTfConvMode)
{
    bool bLocalCompartments = false;
    ITfCompartmentMgr* pcm = NULL;
    BOOL bRc = FALSE;
    HRESULT hr = E_FAIL;

    if (!pTfOpenMode && !pTfConvMode)
    {
        bLocalCompartments = true;
        GetCompartments(&pcm, &pTfOpenMode, &pTfConvMode);
    }
    if (!(pTfOpenMode && pTfConvMode))
    {
        // Invalid parameters or GetCompartments() has failed.
        return FALSE;
    }
    ITfSource* srcOpenMode = NULL;
    if (SUCCEEDED(hr = pTfOpenMode->QueryInterface(IID_ITfSource, (void**)&srcOpenMode)))
    {
        // Remove existing sink for open mode
        if (m_dwOpenModeSinkCookie != TF_INVALID_COOKIE)
        {
            srcOpenMode->UnadviseSink(m_dwOpenModeSinkCookie);
            m_dwOpenModeSinkCookie = TF_INVALID_COOKIE;
        }
        // Setup sink for open mode (toggle state) change
        if (bRemoveOnly || SUCCEEDED(hr = srcOpenMode->AdviseSink(IID_ITfCompartmentEventSink,
            (ITfCompartmentEventSink*)m_TsfSink,
            &m_dwOpenModeSinkCookie)))
        {
            ITfSource* srcConvMode = NULL;
            if (SUCCEEDED(hr = pTfConvMode->QueryInterface(IID_ITfSource, (void**)&srcConvMode)))
            {
                // Remove existing sink for open mode
                if (m_dwConvModeSinkCookie != TF_INVALID_COOKIE)
                {
                    srcConvMode->UnadviseSink(m_dwConvModeSinkCookie);
                    m_dwConvModeSinkCookie = TF_INVALID_COOKIE;
                }
                // Setup sink for open mode (toggle state) change
                if (bRemoveOnly || SUCCEEDED(hr = srcConvMode->AdviseSink(IID_ITfCompartmentEventSink,
                    (ITfCompartmentEventSink*)m_TsfSink,
                    &m_dwConvModeSinkCookie)))
                {
                    bRc = TRUE;
                }
                srcConvMode->Release();
            }
        }
        srcOpenMode->Release();
    }
    if (bLocalCompartments)
    {
        pTfOpenMode->Release();
        pTfConvMode->Release();
        pcm->Release();
    }
    return bRc;
}