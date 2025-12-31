package=qrencode
$(package)_version=4.1.1
$(package)_download_path=https://github.com/fukuchi/libqrencode/releases/download/v$($(package)_version)
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=0019dfc4b32d63c1392aa264aed2253c1e0c2fb09216f8e2cc269bbfb8bb49b5

define $(package)_set_vars
$(package)_config_opts=--disable-shared -without-tools --disable-sdltest
$(package)_config_opts_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
