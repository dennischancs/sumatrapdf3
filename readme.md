![](https://github.com/sumatrapdfreader/sumatrapdf/workflows/Build/badge.svg)

## Custom SumatraPDF Reader
The original software contains annotation bad colors. This customised version has new pretty colors from Microsoft Edge PDF Reader and also includes a moon-star button on the toolbar which will toggle Dark Mode.

based version: [sumatrapdfreader/sumatrapdf@ad91707](https://github.com/sumatrapdfreader/sumatrapdf/commit/ad9170712fdf0bfb3dca808dcb155c05d7364f56)

## add ToggleDarkMode

1. add a new macro `#define IDC_TOGGLEDARKMODE  1035` in `.\src\resource.h`

2. add toolbar svg icon in `.\src\SvgIcons.cpp` and `.\src\Toolbar.cpp`

3. add function in `.\src\SumatraPDF.cpp`

ref: [ImmutableGlitch/SumatraPDF_Custom: Modified version of SumatraPDF](https://github.com/ImmutableGlitch/SumatraPDF_Custom)

![DarkMode](.\DarkMode.png)

## new pretty colors from Microsoft Edge PDF Reader

edit 3 files below:

`.\src\gl-annotate.c` and `.\mupdf\platform\gl\gl-annotate.c`

`.\src\EditAnnotations.cpp`

<video src=".\AnnotationColors.mp4"></video>

------

## SumatraPDF Reader

SumatraPDF is a multi-format (PDF, EPUB, MOBI, FB2, CHM, XPS, DjVu) reader
for Windows under (A)GPLv3 license, with some code under BSD license (see
AUTHORS).

More information:
* [main website](https://www.sumatrapdfreader.org) with downloads and documentation
* [manual](https://www.sumatrapdfreader.org/manual.html)
* [all other docs](https://www.sumatrapdfreader.org/docs/SumatraPDF-documentation-fed36a5624d443fe9f7be0e410ecd715.html)

To compile you need Visual Studio 2019 16.6 or later. [Free Community edition](https://www.visualstudio.com/vs/community/) works.

Open `vs2019/SumatraPDF.sln` and hit F5 to compile and run.

For best results use the latest release available as that's what I use and test with.
If things don't compile, first make sure you're using the latest version of Visual Studio.

Notes on targets:
* `x32_asan` target is for enabling address sanitizer, only works in 32-bit Release build and requires installing an optional "C++ AddressSanitizers" component

### Asan notes

Docs:
* https://devblogs.microsoft.com/cppblog/asan-for-windows-x64-and-debug-build-support/
* https://devblogs.microsoft.com/cppblog/addresssanitizer-asan-for-windows-with-msvc/


Flags:
* https://github.com/google/sanitizers/wiki/SanitizerCommonFlags
* https://github.com/google/sanitizers/wiki/AddressSanitizerFlags

Can be set with env variable:
* `ASAN_OPTIONS=halt_on_error=0:allocator_may_return_null=1:verbosity=2:check_malloc_usable_size=false:print_suppressions=true:suppressions="C:\Users\kjk\src\sumatrapdf\asan.supp"`

In Visual Studio, this is in  `Debugging`, `Environment` section.

Note:
* as of VS 16.6.2 `ASAN_OPTIONS=detect_leaks=1` (i.e. memory leaks) doesn't work.
  Unix version relies on tcmalloc so this might never work
  Suppressing issues: https://clang.llvm.org/docs/AddressSanitizer.html#issue-suppression
  Note: I couldn't get suppressing to work.