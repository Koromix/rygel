// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#if defined(__APPLE__)

#include "src/core/base/base.hh"
#include "tray.hh"
#include "vendor/stb/stb_image.h"
#include "vendor/stb/stb_image_resize2.h"

#include <mach/port.h>
#include <mach/mach.h>
#include <sys/event.h>
#include <sys/time.h>
#include <objc/objc-runtime.h>
#include <dispatch/dispatch.h>
#include <limits.h>
#include <poll.h>

namespace K {

extern "C" mach_port_t _dispatch_get_main_queue_port_4CF(void);
extern "C" void _dispatch_main_queue_callback_4CF(void);

struct IconSet {
    LocalArray<Span<const uint8_t>, 8> pixmaps;
    BlockAllocator allocator;
};

struct MenuItem {
    const char *label;
    int check;

    std::function<void()> func;
};

class MacTray: public gui_TrayIcon {
    int kfd = -1;
    mach_port_t ports = 0;
    bool waiting = false;

    id app;
    id pool;
    id status_bar;
    id status_item;
    id status_btn;

    IconSet icons;
    std::function<void()> activate;
    std::function<void()> context;
    BucketArray<MenuItem> items;

public:
    ~MacTray();

    bool Init();

    bool SetIcon(Span<const uint8_t> png) override;

    void OnActivation(std::function<void()> func) override;
    void OnContext(std::function<void()> func) override;

    void AddAction(const char *label, int check, std::function<void()> func) override;
    void AddSeparator() override;
    void ClearMenu() override;

    WaitSource GetWaitSource() override;
    bool ProcessEvents() override;
};

template <typename T = void, typename U>
T ObjCSendMsg(id obj, U arg1)
{
    return ((T (*)(U))objc_msgSend)(obj, arg1);
}
template <typename T = void, typename U, typename V>
T ObjCSendMsg(id obj, U arg1, V arg2)
{
    return ((T (*)(U, V))objc_msgSend)(obj, arg1, arg2);
}
template <typename T = void, typename U, typename V, typename W>
T ObjCSendMsg(id obj, U arg1, V arg2, W arg3)
{
    return ((T (*)(U, V, W))objc_msgSend)(obj, arg1, arg2, arg3);
}
template <typename T = void, typename U, typename V, typename W, typename X>
T ObjCSendMsg(id obj, U arg1, V arg2, W arg3, X arg4)
{
    return ((T (*)(U, V, W, X))objc_msgSend)(obj, arg1, arg2, arg3, arg4);
}

static id _tray_menu(struct tray_menu *m)
{
    id menu = ObjCSendMsg<id>((id)objc_getClass("NSMenu"), sel_registerName("new"));

    ObjCSendMsg<void>(menu, sel_registerName("autorelease"));
    ObjCSendMsg<void>(menu, sel_registerName("setAutoenablesItems:"), false);

    return menu;
}

static void menu_callback(id self, SEL cmd, id sender)
{
    // XXX
}

/* static int tray_loop(int blocking) {
    id until = (blocking ? 
      ObjCSendMsg<id>((id)objc_getClass("NSDate"), sel_registerName("distantFuture")) : 
      ObjCSendMsg<id>((id)objc_getClass("NSDate"), sel_registerName("distantPast")));
  
    id event = objc_msgSend(app, sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"), 
                ULONG_MAX, 
                until, 
                objc_msgSend((id)objc_getClass("NSString"), 
                  sel_registerName("stringWithUTF8String:"), 
                  "kCFRunLoopDefaultMode"), 
                true);
    if (event) {
      objc_msgSend(app, sel_registerName("sendEvent:"), event);
    }
    return 0;
} */

static void tray_update(struct tray *tray) {
    objc_msgSend(statusBarButton, sel_registerName("setImage:"), 
                 objc_msgSend((id)objc_getClass("NSImage"), sel_registerName("imageNamed:"), 
                              objc_msgSend((id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), tray->icon)));

    objc_msgSend(statusItem, sel_registerName("setMenu:"), _tray_menu(tray->menu));
}

static void tray_exit() { objc_msgSend(app, sel_registerName("terminate:"), app); }*/

MacTray::~MacTray()
{
    CloseDescriptor(kfd);

    if (ports) {
        mach_port_deallocate(mach_task_self(), ports);
    }
}

bool MacTray::Init()
{
    K_ASSERT(!kfd);

    // Init the kqueue descriptor
    {
        kfd = kqueue();
        if (kfd < 0) {
            LogError("Failed to init kqueue(): %1", strerror(errno));
            return false;
        }
        fcntl(kfd, F_SETFD, FD_CLOEXEC);

        if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &ports) != KERN_SUCCESS) {
            LogError("Failed to allocate mach port set: %1", strerror(errno));
            return false;
        }

        struct kevent kev;
        struct timespec ts = {};

        EV_SET(&kev, ports, EVFILT_MACHPORT, EV_ADD, 0, 0, nullptr);

        if (kevent(kfd, &kev, 1, nullptr, 0, &ts) < 0) {
            LogError("Failed to use kevent with mach port set: %1", strerror(errno));
            return false;
        }
    }

    // Create the tray icon
    {
        pool = ObjCSendMsg<id>((id)objc_getClass("NSAutoreleasePool"), sel_registerName("new"));

        ObjCSendMsg((id)objc_getClass("NSApplication"), sel_registerName("sharedApplication"));

        Class cls = objc_allocateClassPair(objc_getClass("NSObject"), "Tray", 0);
        class_addProtocol(cls, objc_getProtocol("NSApplicationDelegate"));
        class_addMethod(cls, sel_registerName("menuCallback:"), (IMP)menu_callback, "v@:@");
        objc_registerClassPair(cls);

        id tray_delegate = ObjCSendMsg<id>((id)cls, sel_registerName("new"));
        id app = ObjCSendMsg<id>((id)objc_getClass("NSApplication"), sel_registerName("sharedApplication"));

        ObjCSendMsg(app, sel_registerName("setDelegate:"), tray_delegate);

        status_bar = ObjCSendMsg<id>((id)objc_getClass("NSStatusBar"), sel_registerName("systemStatusBar"));
        status_item = ObjCSendMsg<id>(status_bar, sel_registerName("statusItemWithLength:"), -1.0);

        ObjCSendMsg(status_item, sel_registerName("retain"));
        ObjCSendMsg(status_item, sel_registerName("setHighlightMode:"), true);
        status_btn = ObjCSendMsg<id>(status_item, sel_registerName("button"));
        tray_update(tray);

        ObjCSendMsg(app, sel_registerName("activateIgnoringOtherApps:"), true);
    }

    return true;
}

bool gui_TrayIcon::SetIcon()
{
    objc_msgSend(statusBarButton, sel_registerName("setImage:"), 
                 objc_msgSend((id)objc_getClass("NSImage"), sel_registerName("imageNamed:"), 
                              objc_msgSend((id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), tray->icon)));

    objc_msgSend(statusItem, sel_registerName("setMenu:"), _tray_menu(tray->menu));
}

WaitSource MacTray::GetWaitSource()
{
    if (!waiting) {
        mach_port_t queue = _dispatch_get_main_queue_port_4CF();

        kern_return_t kret = mach_port_insert_member(mach_task_self(), queue, ports);
        K_CRITICAL(kret == KERN_SUCCESS, "Failed to wait on event loop port: %1", strerror(errno));

        waiting = true;
    }

    WaitSource src = { kfd, POLLIN, -1 };
    return src;
}

bool MacTray::ProcessEvents()
{
    _dispatch_main_queue_callback_4CF();
    return true;
}

}

#endif
