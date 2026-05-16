APP_ABI      := arm64-v8a
APP_PLATFORM := android-24
APP_STL      := c++_static
APP_OPTIM    := release
APP_CFLAGS   := -Os -ffunction-sections -fdata-sections -fvisibility=hidden -fvisibility-inlines-hidden -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-rtti
APP_CPPFLAGS := -Os -ffunction-sections -fdata-sections -fvisibility=hidden -fvisibility-inlines-hidden -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-rtti
APP_LDFLAGS  := -Wl,--gc-sections -Wl,--icf=all -Wl,-s -flto
