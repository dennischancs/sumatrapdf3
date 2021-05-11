/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/LogDbg.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EnginePdf.h"
#include "EngineXps.h"
#include "mui/MiniMui.h"
#include "EngineEbook.h"
#include "EngineImages.h"
#include "PdfPreview.h"
#include "PdfPreviewBase.h"

IFACEMETHODIMP PreviewBase::GetThumbnail(uint cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    EngineBase* engine = GetEngine();
    if (!engine) {
        return E_FAIL;
    }

    dbglogf("PdfPreview: PreviewBase::GetThumbnail(cx=%d)\n", (int)cx);

    RectF page = engine->Transform(engine->PageMediabox(1), 1, 1.0, 0);
    float zoom = std::min(cx / (float)page.dx, cx / (float)page.dy) - 0.001f;
    Rect thumb = RectF(0, 0, page.dx * zoom, page.dy * zoom).Round();

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biHeight = thumb.dy;
    bmi.bmiHeader.biWidth = thumb.dx;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    u8* bmpData = nullptr;
    HBITMAP hthumb = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, (void**)&bmpData, nullptr, 0);
    if (!hthumb) {
        return E_OUTOFMEMORY;
    }

    page = engine->Transform(ToRectFl(thumb), 1, zoom, 0, true);
    RenderPageArgs args(1, zoom, 0, &page);
    RenderedBitmap* bmp = engine->RenderPage(args);

    HDC hdc = GetDC(nullptr);
    if (bmp && GetDIBits(hdc, bmp->GetBitmap(), 0, thumb.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        // cf. http://msdn.microsoft.com/en-us/library/bb774612(v=VS.85).aspx
        for (int i = 0; i < thumb.dx * thumb.dy; i++) {
            bmpData[4 * i + 3] = 0xFF;
        }

        *phbmp = hthumb;
        if (pdwAlpha) {
            *pdwAlpha = WTSAT_RGB;
        }
    } else {
        DeleteObject(hthumb);
        hthumb = nullptr;
    }

    ReleaseDC(nullptr, hdc);
    delete bmp;

    return hthumb ? S_OK : E_NOTIMPL;
}

#define COL_WINDOW_BG RGB(0x99, 0x99, 0x99)
#define PREVIEW_MARGIN 2
#define UWM_PAINT_AGAIN (WM_USER + 101)

class PageRenderer {
    EngineBase* engine;
    HWND hwnd;

    int currPage;
    RenderedBitmap* currBmp;
    // due to rounding differences, currBmp->Size() and currSize can differ slightly
    Size currSize;
    int reqPage;
    float reqZoom;
    Size reqSize;
    bool reqAbort;
    AbortCookie* abortCookie;

    CRITICAL_SECTION currAccess;
    HANDLE thread;

    // seeking inside an IStream spins an inner event loop
    // which can cause reentrance in OnPaint and leave an
    // engine semi-initialized when it's called recursively
    // (this only applies for the UI thread where the critical
    // sections can't prevent recursion without the risk of deadlock)
    bool preventRecursion;

  public:
    PageRenderer(EngineBase* engine, HWND hwnd)
        : engine(engine),
          hwnd(hwnd),
          currPage(0),
          currBmp(nullptr),
          reqPage(0),
          reqZoom(0),
          reqAbort(false),
          abortCookie(nullptr),
          thread(nullptr),
          preventRecursion(false) {
        InitializeCriticalSection(&currAccess);
    }
    ~PageRenderer() {
        if (thread) {
            WaitForSingleObject(thread, INFINITE);
        }
        delete currBmp;
        DeleteCriticalSection(&currAccess);
    }

    RectF GetPageRect(int pageNo) {
        if (preventRecursion) {
            return RectF();
        }

        preventRecursion = true;
        // assume that any engine methods could lead to a seek
        RectF bbox = engine->PageMediabox(pageNo);
        bbox = engine->Transform(bbox, pageNo, 1.0, 0);
        preventRecursion = false;
        return bbox;
    }

    void Render(HDC hdc, Rect target, int pageNo, float zoom) {
        dbglog("PdfPreview: PageRenderer::Render()\n");

        ScopedCritSec scope(&currAccess);
        if (currBmp && currPage == pageNo && currSize == target.Size()) {
            currBmp->StretchDIBits(hdc, target);
        } else if (!thread) {
            reqPage = pageNo;
            reqZoom = zoom;
            reqSize = target.Size();
            reqAbort = false;
            thread = CreateThread(nullptr, 0, RenderThread, this, 0, 0);
        } else if (reqPage != pageNo || reqSize != target.Size()) {
            if (abortCookie) {
                abortCookie->Abort();
            }
            reqAbort = true;
        }
    }

  protected:
    static DWORD WINAPI RenderThread(LPVOID data) {
        ScopedCom comScope; // because the engine reads data from a COM IStream

        PageRenderer* pr = (PageRenderer*)data;
        RenderPageArgs args(pr->reqPage, pr->reqZoom, 0, nullptr, RenderTarget::View, &pr->abortCookie);
        RenderedBitmap* bmp = pr->engine->RenderPage(args);

        ScopedCritSec scope(&pr->currAccess);

        if (!pr->reqAbort) {
            delete pr->currBmp;
            pr->currBmp = bmp;
            pr->currPage = pr->reqPage;
            pr->currSize = pr->reqSize;
        } else {
            delete bmp;
        }
        delete pr->abortCookie;
        pr->abortCookie = nullptr;

        HANDLE th = pr->thread;
        pr->thread = nullptr;
        PostMessageW(pr->hwnd, UWM_PAINT_AGAIN, 0, 0);

        CloseHandle(th);
        return 0;
    }
};

static LRESULT OnPaint(HWND hwnd) {
    Rect rect = ClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rect);
    HDC hdc = buffer.GetDC();
    HBRUSH brushBg = CreateSolidBrush(COL_WINDOW_BG);
    HBRUSH brushWhite = GetStockBrush(WHITE_BRUSH);
    RECT rcClient = ToRECT(rect);
    FillRect(hdc, &rcClient, brushBg);

    PreviewBase* preview = (PreviewBase*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview && preview->renderer) {
        int pageNo = GetScrollPos(hwnd, SB_VERT);
        RectF page = preview->renderer->GetPageRect(pageNo);
        if (!page.IsEmpty()) {
            rect.Inflate(-PREVIEW_MARGIN, -PREVIEW_MARGIN);
            float zoom = (float)std::min(rect.dx / page.dx, rect.dy / page.dy) - 0.001f;
            Rect onScreen = RectF((float)rect.x, (float)rect.y, (float)page.dx * zoom, (float)page.dy * zoom).Round();
            onScreen.Offset((rect.dx - onScreen.dx) / 2, (rect.dy - onScreen.dy) / 2);

            RECT rcPage = ToRECT(onScreen);
            FillRect(hdc, &rcPage, brushWhite);
            preview->renderer->Render(hdc, onScreen, pageNo, zoom);
        }
    }

    DeleteObject(brushBg);
    DeleteObject(brushWhite);

    PAINTSTRUCT ps;
    buffer.Flush(BeginPaint(hwnd, &ps));
    EndPaint(hwnd, &ps);
    return 0;
}

static LRESULT OnVScroll(HWND hwnd, WPARAM wp) {
    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    switch (LOWORD(wp)) {
        case SB_TOP:
            si.nPos = si.nMin;
            break;
        case SB_BOTTOM:
            si.nPos = si.nMax;
            break;
        case SB_LINEUP:
            si.nPos--;
            break;
        case SB_LINEDOWN:
            si.nPos++;
            break;
        case SB_PAGEUP:
            si.nPos--;
            break;
        case SB_PAGEDOWN:
            si.nPos++;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
    }
    si.fMask = SIF_POS;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
    return 0;
}

static LRESULT OnKeydown(HWND hwnd, WPARAM key) {
    switch (key) {
        case VK_DOWN:
        case VK_RIGHT:
        case VK_NEXT:
            return OnVScroll(hwnd, SB_PAGEDOWN);
        case VK_UP:
        case VK_LEFT:
        case VK_PRIOR:
            return OnVScroll(hwnd, SB_PAGEUP);
        case VK_HOME:
            return OnVScroll(hwnd, SB_TOP);
        case VK_END:
            return OnVScroll(hwnd, SB_BOTTOM);
        default:
            return 0;
    }
}

static LRESULT OnDestroy(HWND hwnd) {
    PreviewBase* preview = (PreviewBase*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview) {
        delete preview->renderer;
        preview->renderer = nullptr;
    }
    return 0;
}

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            return OnPaint(hwnd);
        case WM_VSCROLL:
            return OnVScroll(hwnd, wp);
        case WM_KEYDOWN:
            return OnKeydown(hwnd, wp);
        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            return 0;
        case WM_MOUSEWHEEL:
            return OnVScroll(hwnd, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? SB_LINEUP : SB_LINEDOWN);
        case WM_DESTROY:
            return OnDestroy(hwnd);
        case UWM_PAINT_AGAIN:
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

IFACEMETHODIMP PreviewBase::DoPreview() {
    dbglog("PdfPreview: PreviewBase::DoPreview()\n");

    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = PreviewWndProc;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"SumatraPDF_PreviewPane";
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wcex);

    m_hwnd = CreateWindow(wcex.lpszClassName, nullptr, WS_CHILD | WS_VSCROLL | WS_VISIBLE, m_rcParent.x, m_rcParent.x,
                          m_rcParent.dx, m_rcParent.dy, m_hwndParent, nullptr, nullptr, nullptr);
    if (!m_hwnd) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    this->renderer = nullptr;
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    EngineBase* engine = GetEngine();
    int pageCount = 1;
    if (engine) {
        pageCount = engine->PageCount();
        this->renderer = new PageRenderer(engine, m_hwnd);
        // don't use the engine afterwards directly (cf. PageRenderer::preventRecursion)
        engine = nullptr;
    }

    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nPos = 1;
    si.nMin = 1;
    si.nMax = pageCount;
    si.nPage = si.nMax > 1 ? 1 : 2;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

    ShowWindow(m_hwnd, SW_SHOW);
    return S_OK;
}

EngineBase* CPdfPreview::LoadEngine(IStream* stream) {
    dbglog("PdfPreview: CPdfPreview::LoadEngine()\n");
    return CreateEnginePdfFromStream(stream);
}

EngineBase* CXpsPreview::LoadEngine(IStream* stream) {
    return CreateXpsEngineFromStream(stream);
}

#include "EngineDjVu.h"

EngineBase* CDjVuPreview::LoadEngine(IStream* stream) {
    return CreateDjVuEngineFromStream(stream);
}

CEpubPreview::CEpubPreview(long* plRefCount) : PreviewBase(plRefCount, SZ_EPUB_PREVIEW_CLSID) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

CEpubPreview::~CEpubPreview() {
    mui::Destroy();
}

EngineBase* CEpubPreview::LoadEngine(IStream* stream) {
    return CreateEpubEngineFromStream(stream);
}

CFb2Preview::CFb2Preview(long* plRefCount) : PreviewBase(plRefCount, SZ_FB2_PREVIEW_CLSID) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

CFb2Preview::~CFb2Preview() {
    mui::Destroy();
}

EngineBase* CFb2Preview::LoadEngine(IStream* stream) {
    return CreateFb2EngineFromStream(stream);
}

CMobiPreview::CMobiPreview(long* plRefCount) : PreviewBase(plRefCount, SZ_MOBI_PREVIEW_CLSID) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

CMobiPreview::~CMobiPreview() {
    mui::Destroy();
}

EngineBase* CMobiPreview::LoadEngine(IStream* stream) {
    return CreateMobiEngineFromStream(stream);
}

EngineBase* CCbxPreview::LoadEngine(IStream* stream) {
    return CreateCbxEngineFromStream(stream);
}

EngineBase* CTgaPreview::LoadEngine(IStream* stream) {
    return CreateImageEngineFromStream(stream);
}
