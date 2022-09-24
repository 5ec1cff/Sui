#include <cstdlib>
#include <dlfcn.h>
#include <logging.h>
#include <cstring>
#include <sys/system_properties.h>
#include <fcntl.h>
#include <unistd.h>

extern "C" {
[[gnu::constructor]] void constructor() {
    LOGD("preload constructor");

    auto ld_preload = getenv("SUI_LD_PRELOAD_BACKUP");
    if (ld_preload) {
        setenv("LD_PRELOAD", ld_preload, 1);
    } else {
        unsetenv("LD_PRELOAD");
    }
}

[[gnu::visibility("default")]] [[maybe_unused]]
int __android_log_is_debuggable() { // NOLINT(bugprone-reserved-identifier)
    return 1;
}

using property_get_t = int(const char *, char *, const char *);

[[gnu::visibility("default")]] [[maybe_unused]]
int property_get(const char *key, char *value, const char *default_value) { // NOLINT(bugprone-reserved-identifier)
    if (key && value && strcmp("ro.debuggable", key) == 0) {
        value[0] = '1';
        value[1] = '\0';
        return 1;
    }

    static property_get_t * original = nullptr;
    if (!original) {
        original = (property_get_t *) dlsym(RTLD_NEXT, "property_get");
    }
    if (original) {
        return original(key, value, default_value);
    }
    return -1;
}

using selinux_android_setcon_t = int(const char*);

constexpr char adbd_label[] = "u:r:adbd:s0";

[[gnu::visibility("default")]] [[maybe_unused]]
int selinux_android_setcon(const char *c) {
    static selinux_android_setcon_t * original = nullptr;
    if (!original) {
        original = (selinux_android_setcon_t *) dlsym(RTLD_NEXT, "selinux_android_setcon");
    }
    if (original) {
        int r = original(c);
        int fd = open("/proc/self/attr/sockcreate", O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            write(fd, adbd_label, sizeof(adbd_label));
            close(fd);
        }
        return r;
    }
    return -1;
}
}
