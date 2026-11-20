#include "padcheck.h"
#include <cstdio>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

namespace {

long intProp(IOHIDDeviceRef d, CFStringRef key) {
    CFTypeRef v = IOHIDDeviceGetProperty(d, key);
    long n = 0;
    if (v && CFGetTypeID(v) == CFNumberGetTypeID()) CFNumberGetValue((CFNumberRef)v, kCFNumberLongType, &n);
    return n;
}

struct Pad { std::string name, guid; long vendor, product, version, page, usage; bool glfwSees; };

// Every HID game controller -- a generic-desktop joystick, gamepad or multi-axis controller
// (usage 1:4, 1:5, 1:8), which is what GLFW takes -- and, `all`, anything from Microsoft too.
std::vector<Pad> listPads(bool all) {
    std::vector<Pad> out;
    IOHIDManagerRef m = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    IOHIDManagerSetDeviceMatching(m, nullptr);
    CFSetRef set = IOHIDManagerCopyDevices(m);
    if (set) {
        std::vector<const void*> devs((size_t)CFSetGetCount(set));
        CFSetGetValues(set, devs.data());
        for (const void* p : devs) {
            IOHIDDeviceRef d = (IOHIDDeviceRef)p;
            Pad pad;
            pad.page = intProp(d, CFSTR(kIOHIDPrimaryUsagePageKey));
            pad.usage = intProp(d, CFSTR(kIOHIDPrimaryUsageKey));
            pad.vendor = intProp(d, CFSTR(kIOHIDVendorIDKey));
            pad.product = intProp(d, CFSTR(kIOHIDProductIDKey));
            pad.version = intProp(d, CFSTR(kIOHIDVersionNumberKey));
            pad.glfwSees = pad.page == 1 && (pad.usage == 4 || pad.usage == 5 || pad.usage == 8);
            if (!pad.glfwSees && !(all && pad.vendor == 0x045e)) continue;
            char name[256] = "?";
            CFTypeRef nm = IOHIDDeviceGetProperty(d, CFSTR(kIOHIDProductKey));
            if (nm && CFGetTypeID(nm) == CFStringGetTypeID()) CFStringGetCString((CFStringRef)nm, name, sizeof name, kCFStringEncodingUTF8);
            pad.name = name;
            char g[40];   // as GLFW's Cocoa joystick code builds it
            snprintf(g, sizeof g, "03000000%02lx%02lx0000%02lx%02lx0000%02lx%02lx0000", pad.vendor & 0xff, (pad.vendor >> 8) & 0xff,
                     pad.product & 0xff, (pad.product >> 8) & 0xff, pad.version & 0xff, (pad.version >> 8) & 0xff);
            pad.guid = g;
            out.push_back(pad);
        }
        CFRelease(set);
    }
    CFRelease(m);
    return out;
}

}  // namespace

std::vector<std::pair<std::string, std::string>> systemPads() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const Pad& p : listPads(false)) out.push_back({p.name, p.guid});
    return out;
}

namespace {
IOHIDManagerRef gRaw = nullptr;
int gRawCount = 0;

std::string TextHex(uint8_t b) {
    static const char* d = "0123456789abcdef";
    return std::string{d[b >> 4], d[b & 15], ' '};
}

void rawValue(void*, IOReturn, void*, IOHIDValueRef v) {
    IOHIDElementRef e = IOHIDValueGetElement(v);
    long page = IOHIDElementGetUsagePage(e), usage = IOHIDElementGetUsage(e), val = IOHIDValueGetIntegerValue(v);
    long lo = IOHIDElementGetLogicalMin(e), hi = IOHIDElementGetLogicalMax(e);
    IOHIDDeviceRef d = IOHIDElementGetDevice(e);
    char name[128] = "?";
    CFTypeRef nm = d ? IOHIDDeviceGetProperty(d, CFSTR(kIOHIDProductKey)) : nullptr;
    if (nm && CFGetTypeID(nm) == CFStringGetTypeID()) CFStringGetCString((CFStringRef)nm, name, sizeof name, kCFStringEncodingUTF8);
    long cookie = (long)IOHIDElementGetCookie(e);
    CFIndex len = IOHIDValueGetLength(v);
    if (len > 8) {                                   // a whole packet (a vendor's own protocol): in hex,
        static std::vector<uint8_t> lastPkt;         // when it says something new (its byte 1, a counter, aside)
        const uint8_t* b = IOHIDValueGetBytePtr(v);
        std::vector<uint8_t> pkt(b, b + len);
        std::vector<uint8_t> cmp = pkt;
        if (cmp.size() > 1) cmp[1] = 0;
        if (cmp == lastPkt) return;
        lastPkt = cmp;
        std::string hex;
        for (CFIndex k = 0; k < len && k < 48; k++) hex += TextHex(b[k]);
        printf("PADCHECK raw: \"%s\" element %ld (usage %02lx:%02lx) packet of %ld bytes: %s\n", name, cookie, page, usage, (long)len, hex.c_str());
        fflush(stdout);
        gRawCount++;
        return;
    }
    // A stick wobbles at rest: an axis is told only past a tenth of its range from what it last said.
    static std::vector<std::pair<long, long>> said;   // (cookie, value)
    bool axis = hi - lo > 1;
    for (auto& s : said)
        if (s.first == cookie) {
            if (axis && labs(val - s.second) * 10 < hi - lo) return;
            if (!axis && val == s.second) return;
            s.second = val;
            goto print;
        }
    said.push_back({cookie, val});
print:
    printf("PADCHECK raw: \"%s\" element %ld (usage %02lx:%02lx) = %ld   (range %ld..%ld)\n", name, cookie, page, usage, val, lo, hi);
    fflush(stdout);
    gRawCount++;
}
}  // namespace

void padRawStart() {
    gRaw = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    // The game controllers only: generic desktop joystick, gamepad, multi-axis controller.
    CFMutableArrayRef match = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    for (int u : {4, 5, 8}) {
        CFMutableDictionaryRef dct = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        int page = 1;
        CFNumberRef p = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page), us = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &u);
        CFDictionarySetValue(dct, CFSTR(kIOHIDDeviceUsagePageKey), p);
        CFDictionarySetValue(dct, CFSTR(kIOHIDDeviceUsageKey), us);
        CFArrayAppendValue(match, dct);
        CFRelease(p); CFRelease(us); CFRelease(dct);
    }
    IOHIDManagerSetDeviceMatchingMultiple(gRaw, match);
    CFRelease(match);
    IOHIDManagerRegisterInputValueCallback(gRaw, rawValue, nullptr);
    IOHIDManagerScheduleWithRunLoop(gRaw, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    IOReturn r = IOHIDManagerOpen(gRaw, kIOHIDOptionsTypeNone);
    printf("PADCHECK raw: listening to the game controllers directly (open: %s)\n",
           r == kIOReturnSuccess ? "ok" : r == kIOReturnNotPermitted ? "NOT PERMITTED -- Input Monitoring?" : "failed");
}

int padRawStop() {
    if (gRaw) {
        IOHIDManagerUnscheduleFromRunLoop(gRaw, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        IOHIDManagerClose(gRaw, kIOHIDOptionsTypeNone);
        CFRelease(gRaw);
        gRaw = nullptr;
    }
    return gRawCount;
}

namespace {
IOHIDManagerRef gGip = nullptr;
GipPad gGipNow;

void gipReport(void*, IOReturn, void*, IOHIDReportType, uint32_t id, uint8_t* r, CFIndex len) {
    // The report ID is the packet's first byte (its GIP command): kept in the buffer, or left out.
    int base = (len > 0 && r[0] == id) ? 0 : -1;
    auto at = [&](int i) -> int { int k = i + base; return (k >= 0 && k < len) ? r[k] : 0; };
    auto u16 = [&](int i) { return at(i) | (at(i + 1) << 8); };
    auto s16 = [&](int i) { return (int)(short)(unsigned short)u16(i); };
    if (id == 0x07) {                                // the Xbox button, on its own
        gGipNow.xbox = (at(4) & 3) != 0;
    } else if (id == 0x20 && len >= 17) {            // buttons, triggers, sticks
        gGipNow.face = (unsigned char)at(4);
        gGipNow.more = (unsigned char)at(5);
        gGipNow.lt = u16(6); gGipNow.rt = u16(8);
        gGipNow.lx = s16(10); gGipNow.ly = s16(12);
        gGipNow.rx = s16(14); gGipNow.ry = s16(16);
    } else {
        return;
    }
    gGipNow.seen = true;
}
}  // namespace

void gipStart() {
    if (gGip) return;
    gGip = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    // Microsoft's game controllers only (a pad over Bluetooth sends ordinary HID reports, never
    // these: GLFW reads that one, and this leaves it be).
    CFMutableDictionaryRef dct = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int vendor = 0x045e, page = 1, usage = 5;
    CFNumberRef v = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendor);
    CFNumberRef p = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
    CFNumberRef u = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
    CFDictionarySetValue(dct, CFSTR(kIOHIDVendorIDKey), v);
    CFDictionarySetValue(dct, CFSTR(kIOHIDDeviceUsagePageKey), p);
    CFDictionarySetValue(dct, CFSTR(kIOHIDDeviceUsageKey), u);
    IOHIDManagerSetDeviceMatching(gGip, dct);
    CFRelease(v); CFRelease(p); CFRelease(u); CFRelease(dct);
    IOHIDManagerRegisterInputReportCallback(gGip, gipReport, nullptr);
    IOHIDManagerScheduleWithRunLoop(gGip, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(gGip, kIOHIDOptionsTypeNone);
}

GipPad gipState() { return gGipNow; }

void padCheckSystem() {
    std::vector<Pad> pads = listPads(true);
    for (const Pad& p : pads)
        printf("PADCHECK system: \"%s\" vendor %04lx product %04lx version %04lx usage %ld:%ld -- %s, GUID %s\n", p.name.c_str(),
               p.vendor, p.product, p.version, p.page, p.usage, p.glfwSees ? "a game controller to GLFW" : "NOT a game controller to GLFW",
               p.guid.c_str());
    if (pads.empty()) printf("PADCHECK system: no game controller connected\n");
}
#else
std::vector<std::pair<std::string, std::string>> systemPads() { return {}; }
void padCheckSystem() { printf("PADCHECK system: (listed on macOS only)\n"); }
void padRawStart() {}
int padRawStop() { return 0; }
void gipStart() {}
GipPad gipState() { return {}; }
#endif
