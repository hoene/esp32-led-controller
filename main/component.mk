#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# COMPONENT_ADD_LDFLAGS := -u ld_include_xt_highint5
COMPONENT_ADD_LDFLAGS := ${COMPONENT_ADD_LDFLAGS} -u ld_include_xt_highint5
