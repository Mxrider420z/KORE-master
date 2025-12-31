package=zeromq
$(package)_version=4.3.5
$(package)_download_path=https://github.com/zeromq/libzmq/releases/download/v$($(package)_version)
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=6653ef5910f17954861fe72332e68b03ca6e4d9c7160eb3a8de5a5a913bfab43

define $(package)_set_vars
$(package)_config_opts=--without-docs --disable-shared --enable-static --disable-dependency-tracking --without-libsodium --enable-curve=no --disable-libunwind --disable-perf --disable-Werror
$(package)_config_opts_linux=--with-pic
endef

define $(package)_config_cmds
	./configure $(host) $($(package)_config_opts)
endef

define $(package)_build_cmds
	$(MAKE)
endef

define $(package)_stage_cmds
	$(MAKE) DESTDIR=$($(package)_staging_dir) install
	mkdir -p $($(package)_staging_dir)$(host_prefix)/lib
	# FIX: Search for libzmq.a and force-install it to the correct staging path
	find . -name libzmq.a -exec install -D -m 644 {} $($(package)_staging_dir)$(host_prefix)/lib/libzmq.a \;
endef
