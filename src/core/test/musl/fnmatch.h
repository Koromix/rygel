#ifndef	_MUSL_FNMATCH_H
#define	_MUSL_FNMATCH_H

#if defined(__cplusplus)
extern "C" {
#endif

#define	MUSL_FNM_PATHNAME 0x1
#define	MUSL_FNM_NOESCAPE 0x2
#define	MUSL_FNM_PERIOD   0x4
#define	MUSL_FNM_LEADING_DIR	0x8
#define	MUSL_FNM_CASEFOLD	0x10
#define	MUSL_FNM_FILE_NAME	MUSL_FNM_PATHNAME

#define	MUSL_FNM_NOMATCH 1
#define MUSL_FNM_NOSYS   (-1)

int musl_fnmatch(const char *, const char *, int);

#if defined(__cplusplus)
}
#endif

#endif
