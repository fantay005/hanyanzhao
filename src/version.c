#ifndef __TARGET_STRING__
#  error "You must define __TARGET_STRING__"
#endif

const char *__target_build = __TARGET_STRING__ ;


const char *Version(void) {
	return __target_build;
}
