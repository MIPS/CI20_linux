ccflags-y := -Iinclude/linux -Iinclude/drm
ifeq (, $(findstring -W,$(EXTRA_CFLAGS)))

endif

jz4780-y := \
	jz4780_crtc.o \
	dwc_hdmi.o \
	jz4780_drv.o

obj-$(CONFIG_DRM_JZ4780)	+= jz4780.o
obj-$(CONFIG_DRM_JZ4780_HDMI_AUDIO) += dw-hdmi-audio.o
