LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := gb-loopback
LOCAL_SRC_FILES := gb-loopback.c
LOCAL_SYSTEM_SHARED_LIBRARIES := libc libudev
LOCAL_C_INCLUDES := $(LOCAL_PATH) external/udev/dist/src/libudev/
LOCAL_CFLAGS :=
include $(BUILD_EXECUTABLE)
