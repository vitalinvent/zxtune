library_name := core_plugins_players
dirs.root := ../../../..
source_dirs := . asap ay dac gme multi music saa sid tfm vgm xmp xsf

po_files := core_players

includes.dirs.sid_base.cpp = $(dirs.root)/3rdparty/sidplayfp
includes.dirs.ffmpeg_decoder.cpp = $(dirs.root)/3rdparty/ffmpeg

jumbo.name := $(library_name)
jumbo.excludes := asap/asap_base.cpp \
                  gme/spc_supp.cpp gme/gme_base.cpp \
                  sid/sid_base.cpp \
                  music/ffmpeg_decoder.cpp music/flac_supp.cpp music/mp3_supp.cpp music/ogg_supp.cpp \
                  vgm/vgm_base.cpp \
                  xmp/xmp_base.cpp

include $(dirs.root)/makefile.mak

ifdef tools.python
sid/songlengths_db.inc: sid/Songlengths.txt sid/mksonglength.py
	$(tools.python) sid/mksonglength.py < $< > $@
endif

sid/songlengths.cpp: sid/songlengths_db.inc
