package=tor
$(package)_version=0.4.8.10
$(package)_download_path=https://dist.torproject.org/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=e628b4fab70edb4727715b23cf2931375a9f7685ac08f2c59ea498a178463a86
$(package)_dependencies=libevent openssl zlib

define $(package)_set_vars
$(package)_config_opts=--prefix=$(host_prefix) --enable-static --disable-shared --disable-asciidoc --disable-system-torrc --disable-lzma --disable-zstd --with-openssl-dir=$(host_prefix) --with-libevent-dir=$(host_prefix) --with-zlib-dir=$(host_prefix) --enable-pic --disable-module-relay --disable-module-dirauth --disable-tool-name-check --disable-unittests
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
	
	# FIX: Use 'find' to locate libtor.a anywhere and copy it to the verified staging path
	find . -name libtor.a -exec cp {} $($(package)_staging_dir)$(host_prefix)/lib/ \;
endef
