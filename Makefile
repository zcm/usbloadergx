#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules
#---------------------------------------------------------------------------------
# USE is the type of build to perform (debug, release)
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
USE		?=	debug
TARGET		:=	boot
BUILD		:=	build
SOURCES		:=	source \
				source/GUI \
				source/Controls \
				source/system \
				source/libs/libfat-frag \
				source/libs/libwbfs \
				source/libs/libruntimeiospatch \
				source/language \
				source/mload \
				source/mload/modules \
				source/patches \
				source/usbloader \
				source/xml \
				source/network \
				source/settings \
				source/settings/menus \
				source/prompts \
				source/wad \
				source/banner \
				source/Channels \
				source/BoxCover \
				source/GameCube \
				source/cheats \
				source/homebrewboot \
				source/themes \
				source/menu \
				source/memory \
				source/FileOperations \
				source/ImageOperations \
				source/SoundOperations \
				source/SystemMenu \
				source/utils \
				source/utils/minizip \
				source/usbloader/wbfs \
				source/cache
DATA		:=	data \
				data/images \
				data/fonts \
				data/sounds \
				data/binary
INCLUDES	:=	source

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
COMMON		=	-pipe -O2

ifeq ($(USE), debug)
	COMMON += -g -ggdb
else ifeq ($(USE), release)
	COMMON += -flto=auto -Werror=odr -Werror=lto-type-mismatch -Werror=strict-aliasing
else
$(error Invalid USE flag: $(USE))
endif

CFLAGS		=	$(COMMON) -Wall -Wno-multichar -Wno-unused-parameter -Wextra $(MACHDEP) $(INCLUDE) -D_GNU_SOURCE -DNO_OLD_WC_NAMES
CXXFLAGS	=	$(CFLAGS)
LDFLAGS		=	$(COMMON) $(MACHDEP) -Wl,-Map,$(notdir $@).map,--section-start,.init=0x80B00000,-wrap,malloc,-wrap,free,-wrap,memalign,-wrap,calloc,-wrap,realloc,-wrap,malloc_usable_size,-wrap,time

ifeq ($(USE), release)
	CFLAGS += -DNO_DEBUG
	LDFLAGS := $(LDFLAGS),--strip-all,-flto=auto
endif

ifeq ($(BUILDMODE),channel)
CFLAGS += -DFULLCHANNEL
CXXFLAGS += -DFULLCHANNEL
endif

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS := -lwolfssl -lcustomntfs -lcustomext2fs -lvorbisidec -logg \
		-lmad -lfreetype -lgd -ljpeg -lpng -lm -lz -lwiiuse -lwiidrc \
		-lbte -lasnd -logc
#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CURDIR)/portlibs

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
export PROJECTDIR := $(CURDIR)
export OUTPUT	:=	$(CURDIR)/$(TARGETDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(CURDIR)/source/libs \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
SVNREV		:=	$(shell bash ./svnrev.sh)
GITVER		:=	$(shell bash ./gitver.sh)
IMPORTFILES	:=  $(shell bash ./filelist.sh)
export CFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
ELFFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.elf)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))
TTFFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.ttf)))
PNGFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.png)))
OGGFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.ogg)))
PCMFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.pcm)))
WAVFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.wav)))
DOLFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.dol)))
MP3FILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.mp3)))
BNRFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bnr)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o) \
					$(TTFFILES:.ttf=.ttf.o) $(PNGFILES:.png=.png.o) $(addsuffix .o,$(DOLFILES)) \
					$(OGGFILES:.ogg=.ogg.o) $(PCMFILES:.pcm=.pcm.o) $(MP3FILES:.mp3=.mp3.o) \
					$(WAVFILES:.wav=.wav.o) $(addsuffix .o,$(ELFFILES)) $(addsuffix .o,$(BINFILES)) \
					$(BNRFILES:.bnr=.bnr.o) $(CURDIR)/data/magic_patcher.o

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(CURDIR)/source/libs/libdrc/ \
					-L$(CURDIR)/source/libs/libext2fs \
					-L$(CURDIR)/source/libs/libntfs \
					-L$(CURDIR)/source/libs/libwolfssl -L$(LIBOGC_LIB)

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) lang all clean sublibs

#---------------------------------------------------------------------------------
$(BUILD):
	$(MAKE) sublibs
	$(SILENTCMD)[ -d $@ ] || mkdir -p $@
	$(SILENTCMD)$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
define sublib_patch_and_make =
CFILES_$(1) := $(subst source/libs/,,$(shell find source/libs/$(1)/$(2) -name '*.c'))
OFILES_$(1) := $$(patsubst $(1)/$(2)/%.c,$(1)/$(3)/%.o,$$(CFILES_$(1)))

PATCHES_$(1) := $(shell find source/libs/_patches/$(1) -name '*.sed')

OFILES += $$(OFILES_$(1))

INCLUDE_$(1) := -I$(CURDIR)/source/libs/$(1)/$(4)
INCLUDE += $$(INCLUDE_$(1))

$$(OFILES_$(1)) &: $$(CFILES_$(1)) $$(subst source/libs/,,$$(PATCHES_$(1)))
	$(SILENTCMD)for p in $$(PATCHES_$(1)); do \
		sed -Ei -f $$$$p `echo $$$$p | sed -E -e 's/_patches\/([^/]+)\/[^/]+/\1/' -e 's/\.sed$$$$//'`; \
	done
	$(MAKE) -C source/libs/$(1) CFLAGS="$(CFLAGS) $$(INCLUDE_$(1))" LDFLAGS="$(LDFLAGS)" $(5)

SUBLIB_OFILES += $$(OFILES_$(1))
endef

# arguments: 1. source/libs dir, 2. input dir, 3. output dir, 4. include dir, 5. submake target
$(eval $(call sublib_patch_and_make,libfat,source,libogc2/wii_release,include,wii-release))

sublibs: $(SUBLIB_OFILES)

#---------------------------------------------------------------------------------
channel:
	$(SILENTCMD)[ -d build ] || mkdir -p build
	$(SILENTCMD)$(MAKE) BUILDMODE=channel --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
lang:
	$(SILENTCMD)[ -d build ] || mkdir -p build
	$(SILENTCMD)$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile language

#---------------------------------------------------------------------------------
theme:
	$(SILENTCMD)[ -d build ] || mkdir -p build
	$(SILENTCMD)$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile language

#---------------------------------------------------------------------------------
all:
	$(SILENTCMD)$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	$(SILENTCMD)$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile lang

#---------------------------------------------------------------------------------
clean:
	$(SILENTCMD)echo Cleaning...
	$(SILENTCMD)rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol usbloader_gx.zip usbloader_gx
	$(foreach sub,$(shell find source/libs -maxdepth 2 -name Makefile -exec dirname {} \;),-$(MAKE) -C $(sub) clean)

#---------------------------------------------------------------------------------
package:
	$(SILENTCMD)echo "\nBuilding with `$(DEVKITPPC)/bin/*gcc --version | head -n1`\n"
	$(MAKE)
	$(SILENTCMD)echo Packaging...
	$(SILENTCMD)[ -d $(PROJECTDIR)/usbloader_gx ] || mkdir -p $(PROJECTDIR)/usbloader_gx
	$(SILENTCMD)cp $(TARGET).dol $(PROJECTDIR)/usbloader_gx/
	$(SILENTCMD)cp $(PROJECTDIR)/HBC/icon.png $(PROJECTDIR)/usbloader_gx/
	$(SILENTCMD)cp $(PROJECTDIR)/HBC/meta.xml $(PROJECTDIR)/usbloader_gx/

#---------------------------------------------------------------------------------
dist:
	$(MAKE) package
	$(SILENTCMD)mkdir -p $(PROJECTDIR)/dist/apps
	$(SILENTCMD)cp -r $(PROJECTDIR)/usbloader_gx $(PROJECTDIR)/dist/apps/
	$(SILENTCMD)cd $(PROJECTDIR)/dist && zip "../usbloadergx_r`cat $(PROJECTDIR)/version.txt`" -r .

#---------------------------------------------------------------------------------

deploy:
	$(MAKE) package	
	$(SILENTCMD)echo Deploying...
	$(SILENTCMD)zip usbloader_gx.zip usbloader_gx/*
	wiiload usbloader_gx.zip

#---------------------------------------------------------------------------------
reload:
	wiiload -r $(OUTPUT).dol

#---------------------------------------------------------------------------------
release:
	$(MAKE)
	cp boot.dol ./hbc/boot.dol

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

language: $(wildcard $(PROJECTDIR)/Languages/*.lang) $(wildcard $(PROJECTDIR)/Themes/*.them)
#---------------------------------------------------------------------------------
# This rule links in binary data with .ttf, .png, and .mp3 extensions
#---------------------------------------------------------------------------------

%.elf.o : %.elf
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.dol.o : %.dol
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.ttf.o : %.ttf
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.png.o : %.png
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.ogg.o : %.ogg
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.pcm.o : %.pcm
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.wav.o : %.wav
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.mp3.o : %.mp3
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.certs.o	:	%.certs
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.dat.o	:	%.dat
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.bin.o	:	%.bin
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.tik.o	:	%.tik
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

%.tmd.o	:	%.tmd
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)
	
%.bnr.o	:	%.bnr
	$(SILENTCMD)echo $(notdir $<)
	$(SILENTCMD)bin2s -a 32 $< | sed '$$a\' | $(AS) -o $(@)

export PATH		:=	$(PROJECTDIR)/gettext-bin:$(PATH)

%.pot: $(CFILES) $(CPPFILES)
	$(SILENTCMD)echo Updating Languagefiles ...
	$(SILENTCMD)touch $(PROJECTDIR)/Languages/$(TARGET).pot
	$(SILENTCMD)xgettext -C -cTRANSLATORS --from-code=utf-8 --sort-output --no-wrap --no-location -ktr -ktrNOOP -o$(PROJECTDIR)/Languages/$(TARGET).pot -p $@ $^
	$(SILENTCMD)echo Updating Themefiles ...
	$(SILENTCMD)touch $(PROJECTDIR)/Themes/$(TARGET).pot
	$(SILENTCMD)xgettext -C -cTRANSLATORS --from-code=utf-8 -F --no-wrap --add-location -kthInt -kthFloat -kthColor -kthAlign -o$(PROJECTDIR)/Themes/$(TARGET).pot -p $@ $^

%.lang: $(PROJECTDIR)/Languages/$(TARGET).pot
	$(SILENTCMD)msgmerge -U -N --no-wrap --no-location --backup=none -q $@ $<
	$(SILENTCMD)touch $@

%.them: $(PROJECTDIR)/Themes/$(TARGET).pot
	$(SILENTCMD)msgmerge -U -N --no-wrap --no-location --backup=none -q $@ $<
	$(SILENTCMD)touch $@

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
