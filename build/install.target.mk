# This file is generated by gyp; do not edit.

TOOLSET := target
TARGET := install
### Generated for copy rule.
/usr/local/lib/libjam.a: TOOLSET := $(TOOLSET)
/usr/local/lib/libjam.a: $(builddir)/libjam.a FORCE_DO_CMD
	$(call do_cmd,copy)

all_deps += /usr/local/lib/libjam.a
binding_gyp_install_target_copies = /usr/local/lib/libjam.a

### Rules for final target.
# Build our special outputs first.
$(obj).target/install.stamp: | $(binding_gyp_install_target_copies)

# Preserve order dependency of special output on deps.
$(binding_gyp_install_target_copies): | $(builddir)/libjam.a

$(obj).target/install.stamp: TOOLSET := $(TOOLSET)
$(obj).target/install.stamp: $(builddir)/libjam.a FORCE_DO_CMD
	$(call do_cmd,touch)

all_deps += $(obj).target/install.stamp
# Add target alias
.PHONY: install
install: $(obj).target/install.stamp

# Add target alias to "all" target.
.PHONY: all
all: install

