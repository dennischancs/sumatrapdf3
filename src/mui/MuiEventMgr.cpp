/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/Timer.h"
#include "utils/HtmlParserLookup.h"
#include "utils/Log.h"
#include "utils/GdiPlusUtil.h"
#include "Mui.h"
#include "wingui/FrameRateWnd.h"
#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

namespace mui {

EventMgr::EventMgr(HwndWrapper* wndRoot) : wndRoot(wndRoot), currOver(nullptr), inSizeMove(false) {
    CrashIf(!wndRoot);
    // CrashIf(wndRoot->hwnd);
}

EventMgr::~EventMgr() {
    // unsubscribe event handlers for all controls
    for (EventHandler& h : eventHandlers) {
        delete h.events;
    }
    for (const NamedEventHandler& nh : namedEventHandlers) {
        str::Free(nh.name);
        delete nh.namedEvents;
    }
}

// Set minimum size that will be enforced by handling WM_GETMINMAXINFO
// Default is (0,0), which is unlimited
void EventMgr::SetMinSize(Size s) {
    // TODO: need to figure out a way to force resizing
    // respecting those constraints. Could just size manually.
    // Without doing sth., the constraints will only apply
    // after next resize operation
    minSize = s;
}

// Set maximum size that will be enforced by handling WM_GETMINMAXINFO
// Default is (0,0), which is unlimited
void EventMgr::SetMaxSize(Size s) {
    // TODO: need to figure out a way to force resizing
    // respecting those constraints. Could just size manually.
    // Without doing sth., the constraints will only apply
    // after next resize operation
    maxSize = s;
}

void EventMgr::RemoveEventsForControl(Control* c) {
    for (size_t i = 0; i < eventHandlers.size(); i++) {
        EventHandler h = eventHandlers.at(i);
        if (h.ctrlSource == c) {
            ControlEvents* events = eventHandlers.at(i).events;
            eventHandlers.RemoveAtFast(i);
            delete events;
            return;
        }
    }
}

ControlEvents* EventMgr::EventsForControl(Control* c) {
    for (EventHandler& h : eventHandlers) {
        if (h.ctrlSource == c) {
            return h.events;
        }
    }
    ControlEvents* events = new ControlEvents();
    EventHandler eh = {c, events};
    eventHandlers.Append(eh);
    return events;
}

NamedEvents* EventMgr::EventsForName(const char* name) {
    for (NamedEventHandler& h : namedEventHandlers) {
        if (str::EqI(h.name, name)) {
            return h.namedEvents;
        }
    }
    NamedEvents* namedEvents = new NamedEvents();
    NamedEventHandler eh = {str::Dup(name), namedEvents};
    namedEventHandlers.Append(eh);
    return namedEvents;
}

void EventMgr::NotifyNamedEventClicked(Control* c, int x, int y) {
    const char* name = c->namedEventClick;
    if (!name) {
        return;
    }
    for (NamedEventHandler& h : namedEventHandlers) {
        if (str::EqI(h.name, name)) {
            h.namedEvents->Clicked(c, x, y);
            return;
        }
    }
}

void EventMgr::NotifyClicked(Control* c, int x, int y) {
    for (EventHandler& h : eventHandlers) {
        if (h.ctrlSource == c) {
            h.events->Clicked(c, x, y);
            return;
        }
    }
}

void EventMgr::NotifySizeChanged(Control* c, int dx, int dy) {
    for (EventHandler& h : eventHandlers) {
        if (h.ctrlSource == c && h.events->SizeChanged) {
            h.events->SizeChanged(c, dx, dy);
        }
    }
}

// TODO: optimize by getting both mouse over and mouse move windows in one call
// x, y is a position in the root window
LRESULT EventMgr::OnMouseMove([[maybe_unused]] WPARAM keys, int x, int y, [[maybe_unused]] bool& wasHandled) {
    Vec<CtrlAndOffset> windows;
    Control* c;

    u16 wantedInputMask = bit::FromBit<u16>(Control::WantsMouseOverBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count) {
        if (currOver) {
            currOver->SetIsMouseOver(false);
            currOver->NotifyMouseLeave();
            currOver = nullptr;
        }
    } else {
        // TODO: should this take z-order into account ?
        c = windows.Last().c;
        if (c != currOver) {
            if (currOver) {
                currOver->SetIsMouseOver(false);
                currOver->NotifyMouseLeave();
            }
            currOver = c;
            currOver->SetIsMouseOver(true);
            currOver->NotifyMouseEnter();
        }
    }

    wantedInputMask = bit::FromBit<u16>(Control::WantsMouseMoveBit);
    count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count) {
        return 0;
    }
    c = windows.Last().c;
    c->MapRootToMyPos(x, y);
    c->NotifyMouseMove(x, y);
    return 0;
}

// TODO: quite possibly the real logic for generating "click" events is
// more complicated
// (x, y) is in the coordinates of the root window
LRESULT EventMgr::OnLButtonUp([[maybe_unused]] WPARAM keys, int x, int y, [[maybe_unused]] bool& wasHandled) {
    Vec<CtrlAndOffset> controls;
    u16 wantedInputMask = bit::FromBit<u16>(Control::WantsMouseClickBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &controls);
    if (0 == count) {
        return 0;
    }
    // TODO: should this take z-order into account?
    Control* c = controls.Last().c;
    c->MapRootToMyPos(x, y);
    NotifyClicked(c, x, y);
    NotifyNamedEventClicked(c, x, y);
    return 0;
}

static void SetIfNotZero(LONG& l, int i, bool& didSet) {
    if (i != 0) {
        l = i;
        didSet = true;
    }
}

LRESULT EventMgr::OnGetMinMaxInfo(MINMAXINFO* info, bool& wasHandled) {
    SetIfNotZero(info->ptMinTrackSize.x, minSize.dx, wasHandled);
    SetIfNotZero(info->ptMinTrackSize.y, minSize.dy, wasHandled);
    SetIfNotZero(info->ptMaxTrackSize.x, maxSize.dx, wasHandled);
    SetIfNotZero(info->ptMaxTrackSize.y, maxSize.dy, wasHandled);
    return 0;
}

LRESULT EventMgr::OnSetCursor([[maybe_unused]] int x, [[maybe_unused]] int y, bool& wasHandled) {
    if (currOver && currOver->hCursor) {
        SetCursor(currOver->hCursor);
        wasHandled = true;
    }
    return TRUE;
}

LRESULT EventMgr::OnMessage(UINT msg, WPARAM wp, LPARAM lp, bool& wasHandled) {
    wasHandled = false;

    if (WM_ENTERSIZEMOVE == msg) {
        inSizeMove = true;
    }

    if (WM_EXITSIZEMOVE == msg) {
        inSizeMove = false;
    }

    if (WM_SIZE == msg) {
        // int dx = LOWORD(lp);
        // int dy = HIWORD(lp);
        wndRoot->RequestLayout();
        return 0;
    }

    wndRoot->LayoutIfRequested();

    if (WM_SETCURSOR == msg) {
        POINT pt;
        if (GetCursorPos(&pt) && ScreenToClient(wndRoot->hwndParent, &pt)) {
            return OnSetCursor(pt.x, pt.y, wasHandled);
        }
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);
        return OnMouseMove(wp, x, y, wasHandled);
    }

    if (WM_LBUTTONUP == msg) {
        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);
        return OnLButtonUp(wp, x, y, wasHandled);
    }

    if (WM_GETMINMAXINFO == msg) {
        return OnGetMinMaxInfo((MINMAXINFO*)lp, wasHandled);
    }

    if (WM_PAINT == msg) {
        wndRoot->OnPaint(wndRoot->hwndParent);
        wasHandled = true;
        return 0;
    }

    return 0;
}
} // namespace mui
