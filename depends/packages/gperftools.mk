package=gperftools
$(package)_version=2.16
$(package)_name=$(package)-$($(package)_version)
$(package)_download_path=https://github.com/$(package)/$(package)/releases/download/$($(package)_name)
$(package)_file_name=$($(package)_name).tar.gz
$(package)_sha256_hash=f12624af5c5987f2cc830ee534f754c3c5961eec08004c26a8b80de015cf056f

define $(package)_set_vars
  $(package)_config_opts=
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
