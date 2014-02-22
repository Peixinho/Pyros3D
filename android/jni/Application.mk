APP_OPTIM := release
APP_PLATFORM := android-8
APP_STL := stlport_static
APP_CPPFLAGS += -Wno-error=format-security
APP_CPPFLAGS += -frtti 
APP_CPPFLAGS += -fexceptions
APP_CPPFLAGS += -DANDROID -DLOG_TO_CONSOLE
APP_ABI := all