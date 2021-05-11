/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// a "unset" state for COLORREF value. technically all colors are valid
// this one is hopefully not used in practice
// can't use constexpr because they'll end up in multiple .lib and conflict
#define ColorUnset ((COLORREF)(0xfeffffff))
// ColorNoChange indicates that we shouldn't change the color
#define ColorNoChange ((COLORREF)(0xfdffffff))

COLORREF MkRgb(u8 r, u8 g, u8 b);
COLORREF MkGray(u8 x);
COLORREF MkRgba(u8 r, u8 g, u8 b, u8 a);
void UnpackRgb(COLORREF, u8& r, u8& g, u8& b);
void UnpackRgba(COLORREF, u8& r, u8& g, u8& b, u8& a);

COLORREF ColorSetRed(COLORREF c, u8 red);
COLORREF ColorSetGreen(COLORREF c, u8 green);
COLORREF ColorSetBlue(COLORREF c, u8 blue);
COLORREF ColorSetAlpha(COLORREF c, u8 alpha);

bool ParseColor(COLORREF* destColor, const WCHAR* txt);
bool ParseColor(COLORREF* destColor, const char* txt);
bool ParseColor(COLORREF* destColor, std::string_view sv);
void SerializeColorRgb(COLORREF, str::Str&);
void SerializeColor(COLORREF, str::Str&);

COLORREF AdjustLightness(COLORREF c, float factor);
COLORREF AdjustLightness2(COLORREF c, float units);
float GetLightness(COLORREF c);

// TODO: use AdjustLightness instead to compensate for the alpha?
Gdiplus::Color Unblend(COLORREF c, u8 alpha);
Gdiplus::Color FromColor(COLORREF c);
Gdiplus::Color GdiRgbFromCOLORREF(COLORREF c);
Gdiplus::Color GdiRgbaFromCOLORREF(COLORREF c);

/* In debug mode, VS 2010 instrumentations complains about GetRValue() etc.
This adds equivalent functions that don't have this problem and ugly
substitutions to make sure we don't use Get*Value() in the future */
u8 GetRed(COLORREF rgb);
u8 GetGreen(COLORREF rgb);
u8 GetBlue(COLORREF rgb);
u8 GetAlpha(COLORREF rgb);

// disabled because not compatible with wdl / lice
#if 0
#undef GetRValue
#define GetRValue UseGetRValueSafeInstead
#undef GetGValue
#define GetGValue UseGetGValueSafeInstead
#undef GetBValue
#define GetBValue UseGetBValueSafeInstead
#endif
