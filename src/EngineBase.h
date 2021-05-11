/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern Kind kindEnginePdf;
extern Kind kindEngineMulti;
extern Kind kindEngineXps;
extern Kind kindEngineDjVu;
extern Kind kindEngineImage;
extern Kind kindEngineImageDir;
extern Kind kindEngineComicBooks;
extern Kind kindEnginePostScript;
extern Kind kindEngineEpub;
extern Kind kindEngineFb2;
extern Kind kindEngineMobi;
extern Kind kindEnginePdb;
extern Kind kindEngineChm;
extern Kind kindEngineHtml;
extern Kind kindEngineTxt;

/* certain OCGs will only be rendered for some of these (e.g. watermarks) */
enum class RenderTarget { View, Print, Export };

enum PageLayoutType {
    Layout_Single = 0,
    Layout_Facing = 1,
    Layout_Book = 2,
    Layout_R2L = 16,
    Layout_NonContinuous = 32
};

enum class DocumentProperty {
    Title,
    Author,
    Copyright,
    Subject,
    CreationDate,
    ModificationDate,
    CreatorApp,
    UnsupportedFeatures,
    FontList,
    PdfVersion,
    PdfProducer,
    PdfFileStructure,
};

class RenderedBitmap {
  public:
    HBITMAP hbmp = nullptr;
    Size size = {};
    AutoCloseHandle hMap = {};

    RenderedBitmap(HBITMAP hbmp, Size size, HANDLE hMap = nullptr) : hbmp(hbmp), size(size), hMap(hMap) {
    }
    ~RenderedBitmap();
    RenderedBitmap* Clone() const;
    HBITMAP GetBitmap() const;
    Size Size() const;
    bool StretchDIBits(HDC hdc, Rect target) const;
};

extern Kind kindDestinationNone;
extern Kind kindDestinationScrollTo;
extern Kind kindDestinationLaunchURL;
extern Kind kindDestinationLaunchEmbedded;
extern Kind kindDestinationLaunchFile;
extern Kind kindDestinationNextPage;
extern Kind kindDestinationPrevPage;
extern Kind kindDestinationFirstPage;
extern Kind kindDestinationLastPage;
extern Kind kindDestinationFindDialog;
extern Kind kindDestinationFullScreen;
extern Kind kindDestinationGoBack;
extern Kind kindDestinationGoForward;
extern Kind kindDestinationGoToPageDialog;
extern Kind kindDestinationPrintDialog;
extern Kind kindDestinationSaveAsDialog;
extern Kind kindDestinationZoomToDialog;

Kind resolveDestKind(char* s);

// text on a page
// a character and its bounding box in page coordinates
struct PageText {
    WCHAR* text{nullptr};
    Rect* coords{nullptr};
    int len{0}; // number of chars in text and bounding boxes in coords
};

void FreePageText(PageText*);

// a link destination
struct PageDestination {
    Kind kind = nullptr;
    int pageNo = 0;
    RectF rect{};
    WCHAR* value = nullptr;
    WCHAR* name = nullptr;

    PageDestination() = default;

    ~PageDestination();
    Kind Kind() const;
    // page the destination points to (0 for external destinations such as URLs)
    int GetPageNo() const;
    // rectangle of the destination on the above returned page
    RectF GetRect() const;
    // string value associated with the destination (e.g. a path or a URL)
    WCHAR* GetValue() const;
    // the name of this destination (reverses EngineBase::GetNamedDest) or nullptr
    // (mainly applicable for links of type "LaunchFile" to PDF documents)
    WCHAR* GetName() const;
};

PageDestination* newSimpleDest(int pageNo, RectF rect, const WCHAR* value = nullptr);
PageDestination* clonePageDestination(PageDestination* dest);

// use in PageDestination::GetDestRect for values that don't matter
#define DEST_USE_DEFAULT -999.9f

extern Kind kindPageElementDest;
extern Kind kindPageElementImage;
extern Kind kindPageElementComment;

struct IPageElement {
    virtual ~IPageElement(){};

    // the type of this page element
    bool Is(Kind expectedKind);

    virtual Kind GetKind() = 0;
    // page this element lives on (0 for elements in a ToC)
    virtual int GetPageNo() = 0;
    // rectangle that can be interacted with
    virtual RectF GetRect() = 0;
    // string value associated with this element (e.g. displayed in an infotip)
    // caller must free() the result
    virtual WCHAR* GetValue() = 0;
    // if this element is a link, this returns information about the link's destination
    // (the result is owned by the PageElement and MUST NOT be deleted)
    virtual PageDestination* AsLink() = 0;
    virtual IPageElement* Clone() = 0;
};

// hoverable (and maybe interactable) element on a single page
struct PageElement : IPageElement {
    Kind kind_ = nullptr;
    int pageNo = 0;
    RectF rect{};
    WCHAR* value = nullptr;
    // only set if kindPageElementDest
    PageDestination* dest = nullptr;

    int imageID = 0;

    ~PageElement() override;

    Kind GetKind() override;
    int GetPageNo() override;
    RectF GetRect() override;
    WCHAR* GetValue() override;
    PageDestination* AsLink() override;
    IPageElement* Clone() override;
};

IPageElement* clonePageElement(IPageElement*);

// those are the same as F font bitmask in PDF docs
// for TocItem::fontFlags
// https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf page 369
constexpr int fontBitItalic = 0;
constexpr int fontBitBold = 1;

extern Kind kindTocFzOutline;
extern Kind kindTocFzLink;
extern Kind kindTocFzOutlineAttachment;
extern Kind kindTocDjvu;

// an item in a document's Table of Content
struct TocItem : TreeItem {
    // each engine has a raw representation of the toc item which
    // we want to access. Not (yet) supported by all engines
    // other values come from parsing this value
    Kind kindRaw = nullptr;
    char* rawVal1 = nullptr;
    char* rawVal2 = nullptr;

    TocItem* parent = nullptr;

    // the item's visible label
    WCHAR* title = nullptr;

    // in some formats, the document can specify the tree item
    // is expanded by default. We keep track if user toggled
    // expansion state of the tree item
    bool isOpenDefault = false;
    bool isOpenToggled = false;

    bool isUnchecked = false;

    // page this item points to (0 for non-page destinations)
    // if GetLink() returns a destination to a page, the two should match
    int pageNo = 0;

    // arbitrary number allowing to distinguish this TocItem
    // from any other of the same ToC tree (must be constant
    // between runs so that it can be persisted in FileState::tocState)
    int id = 0;

    int fontFlags = 0; // fontBitBold, fontBitItalic
    COLORREF color = ColorUnset;

    PageDestination* dest = nullptr;

    // first child item
    TocItem* child = nullptr;
    // next sibling
    TocItem* next = nullptr;

    // -- only for .vbkm usage (EngineMulti, TocEditor) --
    // marks a node that represents a file
    char* engineFilePath = nullptr;
    int nPages = 0;
    // auto-calculated page number that tells us a span from
    // pageNo => endPageNo
    int endPageNo = 0;

    TocItem() = default;

    explicit TocItem(TocItem* parent, const WCHAR* title, int pageNo);

    ~TocItem() override;

    void AddSibling(TocItem* sibling);
    void AddSiblingAtEnd(TocItem* sibling);
    void AddChild(TocItem* child);

    void OpenSingleNode();
    void DeleteJustSelf();

    PageDestination* GetPageDestination();

    // TreeItem
    TreeItem* Parent() override;
    int ChildCount() override;
    TreeItem* ChildAt(int n) override;
    bool IsExpanded() override;
    bool IsChecked() override;
    WCHAR* Text() override;

    bool PageNumbersMatch() const;
};

TocItem* CloneTocItemRecur(TocItem*, bool);

struct TocTree : TreeModel {
    TocItem* root = nullptr;

    TocTree() = default;
    TocTree(TocItem* root);
    ~TocTree() override;

    // TreeModel
    int RootCount() override;
    TreeItem* RootAt(int n) override;
};

TocTree* CloneTocTree(TocTree*, bool removeUnchecked);
bool VisitTocTree(TocItem* ti, const std::function<bool(TocItem*)>& f);
bool VisitTocTreeWithParent(TocItem* ti, const std::function<bool(TocItem* ti, TocItem* parent)>& f);
void SetTocTreeParents(TocItem* treeRoot);

// a helper that allows for rendering interruptions in an engine-agnostic way
class AbortCookie {
  public:
    virtual ~AbortCookie() {
    }
    // aborts a rendering request (as far as possible)
    // note: must be thread-safe
    virtual void Abort() = 0;
};

struct RenderPageArgs {
    int pageNo = 0;
    float zoom = 0;
    int rotation = 0;
    /* if nullptr: defaults to the page's mediabox */
    RectF* pageRect = nullptr;
    RenderTarget target = RenderTarget::View;
    AbortCookie** cookie_out = nullptr;

    RenderPageArgs(int pageNo, float zoom, int rotation, RectF* pageRect = nullptr,
                   RenderTarget target = RenderTarget::View, AbortCookie** cookie_out = nullptr);
};

class EngineBase {
  public:
    Kind kind = nullptr;
    // the default file extension for a document like
    // the currently loaded one (e.g. L".pdf")
    const WCHAR* defaultFileExt = nullptr;
    PageLayoutType preferredLayout = Layout_Single;
    float fileDPI = 96.0f;
    bool isImageCollection = false;
    bool allowsPrinting = true;
    bool allowsCopyingText = true;
    bool isPasswordProtected = false;
    char* decryptionKey = nullptr;
    bool hasPageLabels = false;
    int pageCount = -1;

    // TODO: migrate other engines to use this
    AutoFreeWstr fileNameBase;

    virtual ~EngineBase();
    // creates a clone of this engine (e.g. for printing on a different thread)
    virtual EngineBase* Clone() = 0;

    // number of pages the loaded document contains
    int PageCount() const;

    // the box containing the visible page content (usually RectF(0, 0, pageWidth, pageHeight))
    virtual RectF PageMediabox(int pageNo) = 0;
    // the box inside PageMediabox that actually contains any relevant content
    // (used for auto-cropping in Fit Content mode, can be PageMediabox)
    virtual RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View);

    // renders a page into a cacheable RenderedBitmap
    // (*cookie_out must be deleted after the call returns)
    virtual RenderedBitmap* RenderPage(RenderPageArgs& args) = 0;

    // applies zoom and rotation to a point in user/page space converting
    // it into device/screen space - or in the inverse direction
    PointF Transform(PointF pt, int pageNo, float zoom, int rotation, bool inverse = false);
    virtual RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) = 0;

    // returns the binary data for the current file
    // (e.g. for saving again when the file has already been deleted)
    // caller needs to free() the result
    virtual std::span<u8> GetFileData() = 0;

    // saves a copy of the current file under a different name (overwriting an existing file)
    // (includeUserAnnots only has an effect if SupportsAnnotation(true) returns true)
    virtual bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) = 0;
    // converts the current file to a PDF file and saves it (overwriting an existing file),
    // (includeUserAnnots should always have an effect)
    virtual bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false);

    // extracts all text found in the given page (and optionally also the
    // coordinates of the individual glyphs)
    // caller needs to free() the result and *coordsOut (if coordsOut is non-nullptr)
    virtual PageText ExtractPageText(int pageNo) = 0;
    // pages where clipping doesn't help are rendered in larger tiles
    virtual bool HasClipOptimizations(int pageNo) = 0;

    // the layout type this document's author suggests (if the user doesn't care)
    // whether the content should be displayed as images instead of as document pages
    // (e.g. with a black background and less padding in between and without search UI)
    bool IsImageCollection() const;

    // access to various document properties (such as Author, Title, etc.)
    virtual WCHAR* GetProperty(DocumentProperty prop) = 0;

    // TODO: needs a more general interface
    // whether it is allowed to print the current document
    bool AllowsPrinting() const;

    // whether it is allowed to extract text from the current document
    // (except for searching an accessibility reasons)
    bool AllowsCopyingText() const;

    // the DPI for a file is needed when converting internal measures to physical ones
    float GetFileDPI() const;

    // returns a list of all available elements for this page
    // caller must delete the result (including all elements contained in the Vec)
    virtual Vec<IPageElement*>* GetElements(int pageNo) = 0;
    // returns the element at a given point or nullptr if there's none
    // caller must delete the result
    virtual IPageElement* GetElementAtPos(int pageNo, PointF pt) = 0;

    // creates a PageDestination from a name (or nullptr for invalid names)
    // caller must delete the result
    virtual PageDestination* GetNamedDest(const WCHAR* name);

    // checks whether this document has an associated Table of Contents
    bool HacToc();

    // returns the root element for the loaded document's Table of Contents
    // caller must delete the result (when no longer needed)
    virtual TocTree* GetToc();

    // checks whether this document has explicit labels for pages (such as
    // roman numerals) instead of the default plain arabic numbering
    bool HasPageLabels() const;

    // returns a label to be displayed instead of the page number
    // caller must free() the result
    virtual WCHAR* GetPageLabel(int pageNo) const;

    // reverts GetPageLabel by returning the first page number having the given label
    virtual int GetPageByLabel(const WCHAR* label) const;

    // whether this document required a password in order to be loaded
    bool IsPasswordProtected() const;

    // returns a string to remember when the user wants to save a document's password
    // (don't implement for document types that don't support password protection)
    // caller must free() the result
    char* GetDecryptionKey() const;

    // loads the given page so that the time required can be measured
    // without also measuring rendering times
    virtual bool BenchLoadPage(int pageNo) = 0;

    // the name of the file this engine handles
    const WCHAR* FileName() const;

    virtual RenderedBitmap* GetImageForPageElement(IPageElement*);

    // protected:
    void SetFileName(const WCHAR* s);
};

class PasswordUI {
  public:
    virtual WCHAR* GetPassword(const WCHAR* fileName, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) = 0;
    virtual ~PasswordUI() {
    }
};

const WCHAR* SkipFileProtocol(const WCHAR*);
