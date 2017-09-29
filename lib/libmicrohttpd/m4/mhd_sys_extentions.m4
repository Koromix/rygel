# SYNOPSIS
#
#   MHD_SYS_EXT([VAR-ADD-CPPFLAGS])
#
# DESCRIPTION
#
#   This macro checks system headers and add defines that enable maximum
#   number of exposed system interfaces. Macro verifies that added defines
#   will not break basic headers, some defines are also checked against
#   real recognition by headers.
#   If VAR-ADD-CPPFLAGS is specified, defines will be added to this variable
#   in form suitable for CPPFLAGS. Otherwise defines will be added to
#   configuration header (usually 'config.h').
#
#   Example usage:
#
#     MHD_SYS_EXT
#
#   or
#
#     MHD_SYS_EXT([CPPFLAGS])
#
#   Macro is not enforced to be called before AC_COMPILE_IFELSE, but it
#   advisable to call macro before any compile and header tests since
#   additional defines can change results of those tests.
#
#   Defined in command line macros are always honored and cache variables
#   used in all checks so if any test will not work correctly on some
#   platform, user may simply fix it by giving correct defines in CPPFLAGS
#   or by giving cache variable in configure parameters, for example:
#
#     ./configure CPPFLAGS='-D_XOPEN_SOURCE=1 -D_XOPEN_SOURCE_EXTENDED'
#
#   or
#
#     ./configure mhd_cv_define__xopen_source_sevenh_works=no
#
#   This simplify building from source on exotic platforms as patching
#   of configure.ac is not required to change results of tests.
#
# LICENSE
#
#   Copyright (c) 2016 Karlson2k (Evgeny Grin) <k2k@narod.ru>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 1

AC_DEFUN([MHD_SYS_EXT],[dnl
  AC_PREREQ([2.64])dnl for AS_VAR_IF, m4_ifnblank
  AC_LANG_PUSH([C])dnl Use C language for simplicity
  mhd_mse_added_exts_flags=""
  mhd_mse_added_prolog=""
  MHD_CHECK_DEFINED([[_XOPEN_SOURCE]], [], [dnl
    AC_CACHE_CHECK([[whether predefined value of _XOPEN_SOURCE is more or equal 500]],
      [[mhd_cv_macro__xopen_source_def_fiveh]], [dnl
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if _XOPEN_SOURCE+0 < 500
#error Value of _XOPEN_SOURCE is less than 500
choke me now;
#endif
          ]],[[int i = 0; i++]])],dnl
        [[mhd_cv_macro__xopen_source_def_fiveh="yes"]],
        [[mhd_cv_macro__xopen_source_def_fiveh="no"]]
      )
    ])
    AS_VAR_IF([mhd_cv_macro__xopen_source_def_fiveh], [["no"]], [dnl
      _MHD_XOPEN_ADD([])
    ])
  ],
  [
    dnl Some platforms (namely: Solaris) use '==' checks instead of '>='
    dnl for _XOPEN_SOURCE, resulting that unknown for platform values are
    dnl interpreted as oldest and platform expose reduced number of
    dnl interfaces. Next checks will ensure that platform recognise
    dnl requested mode instead of blindly define high number that can
    dnl be simply ignored by platform.
    MHD_CHECK_ACCEPT_DEFINE([[_XOPEN_SOURCE]], [[700]], [], [dnl
      AC_CACHE_CHECK([[whether _XOPEN_SOURCE with value 700 really enable POSIX.1-2008/SUSv4 features]],
        [[mhd_cv_define__xopen_source_sevenh_works]], [dnl
        _MHD_CHECK_XOPEN_ENABLE([[700]], [
_MHD_BASIC_INCLUDES
[
/* Check will be passed if ALL features are avalable 
 * and failed if ANY feature is not avalable. */
int main()
{

#ifndef stpncpy
  (void) stpncpy;
#endif
#ifndef strnlen
  (void) strnlen;
#endif

#if !defined(__NetBSD__) && !defined(__OpenBSD__)
/* NetBSD and OpenBSD didn't implement wcsnlen() for some reason. */
#ifndef wcsnlen
  (void) wcsnlen;
#endif
#endif

#ifdef __CYGWIN__
/* The only depend function on Cygwin, but missing on some other platforms */
#ifndef strndup
  (void) strndup;
#endif
#endif

#ifndef __sun
/* illumos forget to uncomment some _XPG7 macros. */
#ifndef renameat
  (void) renameat;
#endif

#ifndef getline
  (void) getline;
#endif
#endif /* ! __sun */

/* gmtime_r() becomes mandatory only in POSIX.1-2008. */
#ifndef gmtime_r
  (void) gmtime_r;
#endif

/* unsetenv() actually defined in POSIX.1-2001 so it
 * must be present with _XOPEN_SOURCE == 700 too. */
#ifndef unsetenv
  (void) unsetenv;
#endif

  return 0;
}
          ]],
          [[mhd_cv_define__xopen_source_sevenh_works="yes"]],
          [[mhd_cv_define__xopen_source_sevenh_works="no"]]
        )dnl
      ])dnl
    ])
    AS_IF([[test "x$mhd_cv_define__xopen_source_accepted_700" = "xyes" &&
      test "x$mhd_cv_define__xopen_source_sevenh_works" = "xyes"]], [dnl
      _MHD_SYS_EXT_ADD_FLAG([[_XOPEN_SOURCE]], [[700]])
    ], [dnl
      MHD_CHECK_ACCEPT_DEFINE([[_XOPEN_SOURCE]], [[600]], [], [dnl
        AC_CACHE_CHECK([[whether _XOPEN_SOURCE with value 600 really enable POSIX.1-2001/SUSv3 features]],
          [[mhd_cv_define__xopen_source_sixh_works]], [dnl
          _MHD_CHECK_XOPEN_ENABLE([[600]], [
_MHD_BASIC_INCLUDES
[
/* Check will be passed if ALL features are available
 * and failed if ANY feature is not available. */
int main()
{

#ifndef setenv
  (void) setenv;
#endif

#ifndef __NetBSD__
#ifndef vsscanf
  (void) vsscanf;
#endif
#endif

/* Availability of next features varies, but they all must be present
 * on platform with support for _XOPEN_SOURCE = 600. */

/* vsnprintf() should be available with _XOPEN_SOURCE >= 500, but some platforms
 * provide it only with _POSIX_C_SOURCE >= 200112 (autodefined when
 * _XOPEN_SOURCE >= 600) where specification of vsnprintf() is aligned with
 * ISO C99 while others platforms defined it with even earlier standards. */
#ifndef vsnprintf
  (void) vsnprintf;
#endif

/* On platforms that prefer POSIX over X/Open, fseeko() is available
 * with _POSIX_C_SOURCE >= 200112 (autodefined when _XOPEN_SOURCE >= 600).
 * On other platforms it should be available with _XOPEN_SOURCE >= 500. */
#ifndef fseeko
  (void) fseeko;
#endif

/* F_GETOWN must be defined with _XOPEN_SOURCE >= 600, but some platforms
 * define it with _XOPEN_SOURCE >= 500. */
#ifndef F_GETOWN
#error F_GETOWN is not defined
choke me now;
#endif
  return 0;
}
            ]],
            [[mhd_cv_define__xopen_source_sixh_works="yes"]],
            [[mhd_cv_define__xopen_source_sixh_works="no"]]
          )dnl
        ])dnl
      ])
      AS_IF([[test "x$mhd_cv_define__xopen_source_accepted_600" = "xyes" &&
        test "x$mhd_cv_define__xopen_source_sixh_works" = "xyes"]], [dnl
        _MHD_SYS_EXT_ADD_FLAG([[_XOPEN_SOURCE]], [[600]])
      ], [dnl
        MHD_CHECK_ACCEPT_DEFINE([[_XOPEN_SOURCE]], [[500]], [], [dnl
          AC_CACHE_CHECK([[whether _XOPEN_SOURCE with value 500 really enable SUSv2/XPG5 features]],
            [mhd_cv_define__xopen_source_fiveh_works], [dnl
            _MHD_CHECK_XOPEN_ENABLE([[500]], [
_MHD_BASIC_INCLUDES
[
/* Check will be passed if ALL features are available
 * and failed if ANY feature is not available. */
int main()
{
/* It's not easy to write reliable test for _XOPEN_SOURCE = 500 as
 * platforms not always precisely follow this standard and some
 * functions are already deprecated in later standards. */

/* Availability of next features varies, but they all must be present
 * on platform with correct support for _XOPEN_SOURCE = 500. */

/* Mandatory with _XOPEN_SOURCE >= 500 but as XSI extension available
 * with much older standards. */
#ifndef ftruncate
  (void) ftruncate;
#endif

/* Added with _XOPEN_SOURCE >= 500 but was available in some standards
 * before. XSI extension. */
#ifndef pread
  (void) pread;
#endif

#ifndef __APPLE__
/* Actually comes from XPG4v2 and must be available
 * with _XOPEN_SOURCE >= 500 as well. */
#ifndef symlink
  (void) symlink;
#endif

/* Actually comes from XPG4v2 and must be available
 * with _XOPEN_SOURCE >= 500 as well. XSI extension. */
#ifndef strdup
  (void) strdup;
#endif
#endif /* ! __APPLE__ */
  return 0;
}
              ]],
              [[mhd_cv_define__xopen_source_fiveh_works="yes"]],
              [[mhd_cv_define__xopen_source_fiveh_works="no"]]
            )dnl
          ])dnl
        ])
        AS_IF([[test "x$mhd_cv_define__xopen_source_accepted_500" = "xyes" && ]dnl
          [test "x$mhd_cv_define__xopen_source_fiveh_works" = "xyes"]], [dnl
          _MHD_SYS_EXT_ADD_FLAG([[_XOPEN_SOURCE]], [[500]])
        ],
        [
          [#] Earlier standards are widely supported, so just define macros to maximum value
          [#] which do not break headers.
          _MHD_XOPEN_ADD([[#define _XOPEN_SOURCE 1]])
          AC_CACHE_CHECK([[whether headers accept _XOPEN_SOURCE with value 1]],
            [mhd_cv_define__xopen_source_accepted_1], [dnl
            AS_IF([[test "x$mhd_cv_define__xopen_source_extended_accepted" = "xyes" || ]dnl
                   [test "x$mhd_cv_define__xopen_version_accepted" = "xyes"]],
              [[mhd_cv_define__xopen_source_accepted_1="yes"]],
            [
              MHD_CHECK_BASIC_HEADERS([[#define _XOPEN_SOURCE 1]],
                [[mhd_cv_define__xopen_source_accepted_1="yes"]],
                [[mhd_cv_define__xopen_source_accepted_1="no"]])
            ])
          ])
          AS_VAR_IF([[mhd_cv_define__xopen_source_accepted_1]], [["yes"]], [dnl
            _MHD_SYS_EXT_ADD_FLAG([[_XOPEN_SOURCE]], [[1]])
          ])
        ])
      ])
    ])
  ])
  dnl Add other extensions.
  dnl Use compiler-based test for determinig target.

  dnl Always add _GNU_SOURCE if headers allow.
  MHD_CHECK_DEF_AND_ACCEPT([[_GNU_SOURCE]], [],
    [[${mhd_mse_added_prolog}]], [],
    [_MHD_SYS_EXT_ADD_FLAG([[_GNU_SOURCE]])])

  dnl __BSD_VISIBLE is actually a small hack for FreeBSD.
  dnl Funny that it's used in Android headers too.
  AC_CACHE_CHECK([[whether to try __BSD_VISIBLE macro]],
    [[mhd_cv_macro_try___bsd_visible]], [dnl
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if !defined(__FreeBSD__) && !defined (__ANDROID__)
#error Target is not FreeBSD or Android
choke me now;
#endif
        ]],[])],
      [[mhd_cv_macro_try___bsd_visible="yes"]],
      [[mhd_cv_macro_try___bsd_visible="no"]]
    )
  ])
  AS_VAR_IF([[mhd_cv_macro_try___bsd_visible]], [["yes"]],
  [dnl
    AC_CACHE_CHECK([[whether __BSD_VISIBLE is already defined]],
      [[mhd_cv_macro___bsd_visible_defined]], [dnl
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
${mhd_mse_added_prolog}
/* Warning: test with inverted logic! */
#ifdef __BSD_VISIBLE
#error __BSD_VISIBLE is defined
choke me now;
#endif
#ifdef __ANDROID__
/* if __BSD_VISIBLE is not defined, Android usually defines it to 1 */
#include <stdio.h>
#if defined(__BSD_VISIBLE) && __BSD_VISIBLE == 1
#error __BSD_VISIBLE is autodefined by headers to value 1
choke me now;
#endif
#endif
          ]],[])
      ],
        [[mhd_cv_macro___bsd_visible_defined="no"]],
        [[mhd_cv_macro___bsd_visible_defined="yes"]]
      )
    ])
    AS_VAR_IF([[mhd_cv_macro___bsd_visible_defined]], [["yes"]], [[:]],
    [dnl
      MHD_CHECK_ACCEPT_DEFINE(
        [[__BSD_VISIBLE]], [], [[${mhd_mse_added_prolog}]],
        [_MHD_SYS_EXT_ADD_FLAG([[__BSD_VISIBLE]])]
      )dnl
    ])
  ])


  dnl _DARWIN_C_SOURCE enables additional functionality on Darwin.
  MHD_CHECK_DEFINED_MSG([[__APPLE__]], [[${mhd_mse_added_prolog}]],
    [[whether to try _DARWIN_C_SOURCE macro]],
  [dnl
    MHD_CHECK_DEF_AND_ACCEPT(
      [[_DARWIN_C_SOURCE]], [], [[${mhd_mse_added_prolog}]], [],
      [_MHD_SYS_EXT_ADD_FLAG([[_DARWIN_C_SOURCE]])]
    )dnl
  ])

  dnl __EXTENSIONS__ unlocks almost all interfaces on Solaris.
  MHD_CHECK_DEFINED_MSG([[__sun]], [[${mhd_mse_added_prolog}]],
    [[whether to try __EXTENSIONS__ macro]],
  [dnl
    MHD_CHECK_DEF_AND_ACCEPT(
      [[__EXTENSIONS__]], [], [[${mhd_mse_added_prolog}]], [],
      [_MHD_SYS_EXT_ADD_FLAG([[__EXTENSIONS__]])]
    )dnl
  ])

  dnl _NETBSD_SOURCE switch on almost all headers definitions on NetBSD.
  MHD_CHECK_DEFINED_MSG([[__NetBSD__]], [[${mhd_mse_added_prolog}]],
    [[whether to try _NETBSD_SOURCE macro]],
  [dnl
    MHD_CHECK_DEF_AND_ACCEPT(
      [[_NETBSD_SOURCE]], [], [[${mhd_mse_added_prolog}]], [],
      [_MHD_SYS_EXT_ADD_FLAG([[_NETBSD_SOURCE]])]
    )dnl
  ])

  dnl _BSD_SOURCE currently used only on OpenBSD to unhide functions.
  MHD_CHECK_DEFINED_MSG([[__OpenBSD__]], [[${mhd_mse_added_prolog}]],
    [[whether to try _BSD_SOURCE macro]],
  [dnl
    MHD_CHECK_DEF_AND_ACCEPT(
      [[_BSD_SOURCE]], [], [[${mhd_mse_added_prolog}]], [],
      [_MHD_SYS_EXT_ADD_FLAG([[_BSD_SOURCE]])]
    )dnl
  ])

  dnl _TANDEM_SOURCE unhides most functions on NonStop OS
  dnl (which comes from Tandem Computers decades ago).
  MHD_CHECK_DEFINED_MSG([[__TANDEM]], [[${mhd_mse_added_prolog}]],
    [[whether to try _TANDEM_SOURCE macro]],
  [dnl
    MHD_CHECK_DEF_AND_ACCEPT(
      [[_TANDEM_SOURCE]], [], [[${mhd_mse_added_prolog}]], [],
      [_MHD_SYS_EXT_ADD_FLAG([[_TANDEM_SOURCE]])]
    )dnl
  ])

  dnl _ALL_SOURCE makes visible POSIX and non-POSIX symbols
  dnl on z/OS, AIX and Interix.
  AC_CACHE_CHECK([[whether to try _ALL_SOURCE macro]],
    [[mhd_cv_macro_try__all_source]], [dnl
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if !defined(__TOS_MVS__) && !defined (__INTERIX)
#error Target is not z/OS, AIX or Interix
choke me now;
#endif
        ]],[])],
      [[mhd_cv_macro_try__all_source="yes"]],
      [[mhd_cv_macro_try__all_source="no"]]
    )
  ])
  AS_VAR_IF([[mhd_cv_macro_try__all_source]], [["yes"]],
  [dnl
    MHD_CHECK_DEF_AND_ACCEPT(
      [[_ALL_SOURCE]], [], [[${mhd_mse_added_prolog}]], [],
      [_MHD_SYS_EXT_ADD_FLAG([[_TANDEM_SOURCE]])]
    )dnl
  ])

  dnl Discard temporal prolog with set of defines.
  AS_UNSET([[mhd_mse_added_prolog]])
  dnl Determined all required defines.
  AC_MSG_CHECKING([[for final set of defined symbols]])
  m4_ifblank([$1], [dnl
    AH_TEMPLATE([[_XOPEN_SOURCE]], [Define to maximum value supported by system headers])dnl
    AH_TEMPLATE([[_XOPEN_SOURCE_EXTENDED]], [Define to 1 if _XOPEN_SOURCE is defined to value less than 500 ]dnl
      [and system headers requre this symbol])dnl
    AH_TEMPLATE([[_XOPEN_VERSION]], [Define to maximum value supported by system headers if _XOPEN_SOURCE ]dnl
      [is defined to value less than 500 and headers do not support _XOPEN_SOURCE_EXTENDED])dnl
    AH_TEMPLATE([[_GNU_SOURCE]], [Define to 1 to enable GNU-related header features])dnl
    AH_TEMPLATE([[__BSD_VISIBLE]], [Define to 1 if it is required by headers to expose additional symbols])dnl
    AH_TEMPLATE([[_DARWIN_C_SOURCE]], [Define to 1 if it is required by headers to expose additional symbols])dnl
    AH_TEMPLATE([[__EXTENSIONS__]], [Define to 1 if it is required by headers to expose additional symbols])dnl
    AH_TEMPLATE([[_NETBSD_SOURCE]], [Define to 1 if it is required by headers to expose additional symbols])dnl
    AH_TEMPLATE([[_BSD_SOURCE]], [Define to 1 if it is required by headers to expose additional symbols])dnl
    AH_TEMPLATE([[_TANDEM_SOURCE]], [Define to 1 if it is required by headers to expose additional symbols])dnl
    AH_TEMPLATE([[_ALL_SOURCE]], [Define to 1 if it is required by headers to expose additional symbols])dnl
  ])
  for mhd_mse_Flag in $mhd_mse_added_exts_flags
  do
    m4_ifnblank([$1], [dnl
      AS_VAR_APPEND([$1],[[" -D$mhd_mse_Flag"]])
    ], [dnl
      AS_CASE([[$mhd_mse_Flag]], [[*=*]],
      [dnl
        AC_DEFINE_UNQUOTED([[`echo $mhd_mse_Flag | cut -f 1 -d =`]],
          [[`echo $mhd_mse_Flag | cut -f 2 -d = -s`]])
      ], [dnl
        AC_DEFINE_UNQUOTED([[$mhd_mse_Flag]])
      ])
    ])
  done
  dnl Trim whitespaces
  mhd_mse_result=`echo $mhd_mse_added_exts_flags`
  AC_MSG_RESULT([[$mhd_mse_result]])
  AS_UNSET([[mhd_mse_result]])

  AS_UNSET([[mhd_mse_added_exts_flags]])
  AC_LANG_POP([C])
])


#
# _MHD_SYS_EXT_ADD_FLAG(FLAG, [FLAG-VALUE = 1])
#
# Internal macro, only to be used from MHD_SYS_EXT, _MHD_XOPEN_ADD

m4_define([_MHD_SYS_EXT_ADD_FLAG], [dnl
  m4_ifnblank([$2],[dnl
    mhd_mse_added_exts_flags="$mhd_mse_added_exts_flags m4_normalize($1)=m4_normalize($2)"
    mhd_mse_added_prolog="${mhd_mse_added_prolog}[#define ]m4_normalize($1) m4_normalize($2)
"
  ], [dnl
    mhd_mse_added_exts_flags="$mhd_mse_added_exts_flags m4_normalize($1)"
    mhd_mse_added_prolog="${mhd_mse_added_prolog}[#define ]m4_normalize($1) 1
"
  ])dnl
])

#
# _MHD_VAR_IF(VAR, VALUE, [IF-EQ], [IF-NOT-EQ])
#
# Same as AS_VAR_IF, except that it expands to nothing if
# both IF-EQ and IF-NOT-EQ are empty.

m4_define([_MHD_VAR_IF],[dnl
m4_ifnblank([$3][$4],[dnl
m4_ifblank([$4],[AS_VAR_IF([$1],[$2],[$3])],[dnl
AS_VAR_IF([$1],[$2],[$3],[$4])])])])

# SYNOPSIS
#
# _MHD_CHECK_XOPEN_ENABLE(_XOPEN_SOURCE-VALUE, FEATURES_TEST,
#                         [ACTION-IF-ENABLED-BY-XOPEN_SOURCE],
#                         [ACTION-IF-NOT],
#                         [ACTION-IF-FEATURES-AVALABLE],
#                         [ACTION-IF-FEATURES-NOT-AVALABLE],
#                         [ACTION-IF-ONLY-WITH-EXTENSIONS]
#                         [ACTION-IF-WITHOUT-ALL])
#
# DESCRIPTION
#
#   This macro determines whether the _XOPEN_SOURCE with
#   _XOPEN_SOURCE-VALUE really enable some header features. FEATURES_TEST
#   must contains required includes and main function.
#   One of ACTION-IF-ENABLED-BY-XOPEN_SOURCE and ACTION-IF-NOT
#   is always executed depending on test results.
#   One of ACTION-IF-FEATURES-AVALABLE and is ACTION-IF-FEATURES-NOT-AVALABLE
#   is executed if features can be enabled by _XOPEN_SOURCE, by currently
#   defined (by compiler flags or by predefined macros) extensions or
#   all checked combinations are failed to enable features.
#   ACTION-IF-ONLY-WITH-EXTENSIONS is executed if features can be
#   enabled with not undefined extension.
#   ACTION-IF-WITHOUT-ALL is executed if features work with all
#   disabled extensions (including _XOPEN_SOURCE).

AC_DEFUN([_MHD_CHECK_XOPEN_ENABLE], [dnl
  AS_VAR_PUSHDEF([src_Var], [[mhd_cxoe_tmp_src_variable]])dnl
  AS_VAR_SET([src_Var],["
$2
"])dnl Reduce 'configure' size

  dnl Some platforms enable most features when no
  dnl specific mode is requested by macro.
  dnl Check whether features test works without _XOPEN_SOURCE and
  dnl with disabled extensions (undefined most of
  dnl predefined macros for specific requested mode).
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
_MHD_UNDEF_ALL_EXT
$src_Var
    ])],
  [dnl
    _AS_ECHO_LOG([[Checked features work with undefined all extensions and without _XOPEN_SOURCE]])
    dnl Checked features is enabled in platform's "default" mode.
    dnl Try to disable features by requesting oldest X/Open mode.
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
_MHD_UNDEF_ALL_EXT
[#define _XOPEN_SOURCE 1]
$src_Var
      ])],
    [dnl
      _AS_ECHO_LOG([[Checked features work with undefined all extensions and with _XOPEN_SOURCE=1]])
      dnl Features still work in oldest X/Open mode.
      dnl Some platforms enable all XSI features for any _XOPEN_SOURCE value.
      dnl Apply some fuzzy logic, try to use _POSIX_C_SOURCE with oldest number.
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([
_MHD_UNDEF_ALL_EXT
[#define _POSIX_C_SOURCE 1]
$src_Var
        ])],
      [dnl
        _AS_ECHO_LOG([[Checked features work with undefined all extensions and with _POSIX_C_SOURCE=1]])
        dnl Features still work in oldest _POSIX_C_SOURCE mode.
        dnl Try to disable features by requesting strict ANSI C mode.
        AC_COMPILE_IFELSE([AC_LANG_SOURCE([
_MHD_UNDEF_ALL_EXT
[#define  _ANSI_SOURCE 1]
$src_Var
          ])],
        [dnl
          _AS_ECHO_LOG([[Checked features work with undefined all extensions and with _ANSI_SOURCE]])
          dnl Features still work in strict _ANSI_SOURCE mode.
          dnl Assume that _XOPEN_SOURCE, _POSIX_C_SOURCE and _ANSI_SOURCE has no influence on
          dnl enabling of features as features are enabled always unconditionally.
          m4_n([$4])dnl ACTION-IF-NOT-ENABLED-BY-XOPEN_SOURCE
        ], [dnl
          _AS_ECHO_LOG([[Checked features do not work with undefined all extensions and with _ANSI_SOURCE]])
          dnl Features do not work in strict _ANSI_SOURCE mode.
          dnl Try to enable features by _XOPEN_SOURCE with specified value.
          AC_COMPILE_IFELSE([AC_LANG_SOURCE([
_MHD_UNDEF_ALL_EXT
[#define  _ANSI_SOURCE 1]
[#define _XOPEN_SOURCE] $1
$src_Var
            ])],
          [dnl
            _AS_ECHO_LOG([[Checked features work with undefined all extensions and with _ANSI_SOURCE and _XOPEN_SOURCE=]$1])
            dnl Finally, features were  disabled by strict ANSI mode and enabled by adding _XOPEN_SOURCE.
            dnl Assume that _XOPEN_SOURCE can enable features.
            m4_n([$3])dnl ACTION-IF-ENABLED-BY-XOPEN_SOURCE
          ], [dnl
            _AS_ECHO_LOG([[Checked features do not work with undefined all extensions and with _ANSI_SOURCE and _XOPEN_SOURCE=]$1])
            dnl Features are not enabled in strict ANSI mode with _XOPEN_SOURCE.
            dnl Actually this is not correct documented situation and _ANSI_SOURCE may have
            dnl priority over _XOPEN_SOURCE or headers are not controlled by _XOPEN_SOURCE at all.
            dnl As features work in all mode except strict ANSI regardless of _XOPEN_SOURCE,
            dnl assume that _XOPEN_SOURCE do not control visibility of features.
            m4_n([$4])dnl ACTION-IF-NOT-ENABLED-BY-XOPEN_SOURCE
          ])
        ])
      ], [dnl
        _AS_ECHO_LOG([[Checked features do not work with undefined all extensions and with _POSIX_C_SOURCE=1]])
        dnl Features do not work in oldest _POSIX_C_SOURCE mode.
        dnl OK, features were disabled by _POSIX_C_SOURCE.
        dnl Check whether headers controlled by _XOPEN_SOURCE too.
        AC_COMPILE_IFELSE([AC_LANG_SOURCE([
_MHD_UNDEF_ALL_EXT
[#define _POSIX_C_SOURCE 1]
[#define _XOPEN_SOURCE] $1
$src_Var
          ])],
        [dnl
          _AS_ECHO_LOG([[Checked features work with undefined all extensions and with _POSIX_C_SOURCE=1 and _XOPEN_SOURCE=]$1])
          dnl Features were enabled again after adding _XOPEN_SOURCE with value.
          dnl Assume that headers can be controlled by _XOPEN_SOURCE with specified value.
          m4_n([$3])dnl ACTION-IF-ENABLED-BY-XOPEN_SOURCE
        ], [dnl
          _AS_ECHO_LOG([[Checked features do not work with undefined all extensions and with _POSIX_C_SOURCE=1 and _XOPEN_SOURCE=]$1])
          dnl Features still work after adding _XOPEN_SOURCE with value.
          dnl It's unclear whether headers know only about _POSIX_C_SOURCE or
          dnl _POSIX_C_SOURCE have priority over _XOPEN_SOURCE (standards are
          dnl silent about priorities).
          dnl Assume that it's unknown whether _XOPEN_SOURCE can turn on features.
          m4_n([$4])dnl ACTION-IF-NOT-ENABLED-BY-XOPEN_SOURCE
        ])
      ])
    ], [dnl
      _AS_ECHO_LOG([[Checked features does not work with undefined all extensions and with _XOPEN_SOURCE=1]])
      dnl Features disabled by oldest X/Open mode.
      dnl Check whether requested _XOPEN_SOURCE value will turn on features.
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([
_MHD_UNDEF_ALL_EXT
[#define _XOPEN_SOURCE] $1
$src_Var
        ])],
      [dnl
        _AS_ECHO_LOG([[Checked features work with undefined all extensions and with _XOPEN_SOURCE=]$1])
        dnl Features work with _XOPEN_SOURCE requested value and do not work
        dnl with value 1.
        dnl Assume that _XOPEN_SOURCE really turn on features.
        m4_n([$3])dnl ACTION-IF-ENABLED-BY-XOPEN_SOURCE
      ], [dnl
        _AS_ECHO_LOG([[Checked features do not work with undefined all extensions and with _XOPEN_SOURCE=]$1])
        dnl Features do not work with _XOPEN_SOURCE, but work in "default" mode.
        dnl Assume that features cannot be enabled by requested _XOPEN_SOURCE value.
        m4_n([$4])dnl ACTION-IF-NOT-ENABLED-BY-XOPEN_SOURCE
      ])
    ])
    m4_n([$5])dnl ACTION-IF-FEATURES-AVALABLE
    m4_n([$8])dnl ACTION-IF-WITHOUT-ALL
  ],
  [dnl
    _AS_ECHO_LOG([[Checked features do not work with undefined all extensions and without _XOPEN_SOURCE]])
    dnl Features do not work with turned off extensions.
    dnl Check whether they can be enabled by _XOPEN_SOURCE.
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
_MHD_UNDEF_ALL_EXT
[#define _XOPEN_SOURCE] $1
$src_Var
      ])],
    [dnl
      _AS_ECHO_LOG([[Checked features work with undefined all extensions and with _XOPEN_SOURCE=]$1])
      dnl Features work with _XOPEN_SOURCE and do not work without _XOPEN_SOURCE.
      dnl Assume that _XOPEN_SOURCE really turn on features.
      m4_n([$3])dnl ACTION-IF-ENABLED-BY-XOPEN_SOURCE
      m4_n([$5])dnl ACTION-IF-FEATURES-AVALABLE
    ],
    [dnl
      _AS_ECHO_LOG([[Checked features do not work with undefined all extensions and with _XOPEN_SOURCE=]$1])
      dnl Features do not work with _XOPEN_SOURCE and turned off extensions.
      dnl Retry without turning off known extensions.
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([
[#define _XOPEN_SOURCE] $1
$src_Var
        ])],
      [dnl
        _AS_ECHO_LOG([[Checked features work with current extensions and with _XOPEN_SOURCE=]$1])
        dnl Features work with _XOPEN_SOURCE and without turning off extensions.
        dnl Check whether features work with oldest _XOPEN_SOURCE or it was enabled only by extensions.
        AC_COMPILE_IFELSE([AC_LANG_SOURCE([
[#define _XOPEN_SOURCE 1]
$src_Var
          ])],
        [dnl
          _AS_ECHO_LOG([[Checked features work with current extensions and with _XOPEN_SOURCE=1]])
          dnl Features still work with oldest _XOPEN_SOURCE.
          dnl Assume that _XOPEN_SOURCE has no influence on enabling of features.
          m4_n([$4])dnl ACTION-IF-NOT-ENABLED-BY-XOPEN_SOURCE
        ], [dnl
          _AS_ECHO_LOG([[Checked features do not work with current extensions and with _XOPEN_SOURCE=1]])
          dnl Features do not work with oldest _XOPEN_SOURCE.
          dnl Assume that _XOPEN_SOURCE really turn on features.
          m4_n([$3])dnl ACTION-IF-ENABLED-BY-XOPEN_SOURCE
        ])
        m4_n([$5])dnl ACTION-IF-FEATURES-AVALABLE
        m4_n([$7])dnl ACTION-IF-ONLY-WITH-EXTENSIONS
      ], [dnl
        _AS_ECHO_LOG([[Checked features do not work with current extensions and with _XOPEN_SOURCE=]$1])
        dnl Features do not work in all checked conditions.
        dnl Assume that _XOPEN_SOURCE cannot enable feature.
        m4_n([$4])dnl ACTION-IF-NOT-ENABLED-BY-XOPEN_SOURCE
        m4_n([$6])dnl ACTION-IF-FEATURE-NOT-AVALABLE
      ])
    ])
  ])
  AS_UNSET([src_Var])
  AS_VAR_POPDEF([src_Var])dnl
])


#
# MHD_CHECK_HEADER_PRESENCE(headername.h)
#
# Check only by preprocessor whether header file is present.

AC_DEFUN([MHD_CHECK_HEADER_PRESENCE], [dnl
  AC_PREREQ([2.64])dnl for AS_VAR_PUSHDEF, AS_VAR_SET
  AS_VAR_PUSHDEF([mhd_cache_Var],[mhd_cv_header_[]$1[]_present])dnl
  AC_CACHE_CHECK([for presence of $1], [mhd_cache_Var], [dnl
    dnl Hack autoconf to get pure result of only single header presence
    cat > conftest.$ac_ext <<_ACEOF
@%:@include <[]$1[]>
_ACEOF
    AC_PREPROC_IFELSE([],
      [AS_VAR_SET([mhd_cache_Var],[["yes"]])],
      [AS_VAR_SET([mhd_cache_Var],[["no"]])]
    )
    rm -f conftest.$ac_ext
  ])
  AS_VAR_POPDEF([mhd_cache_Var])dnl
])


#
# MHD_CHECK_HEADERS_PRESENCE(oneheader.h otherheader.h ...)
#
# Check each specified header in whitespace-separated list for presence.

AC_DEFUN([MHD_CHECK_HEADERS_PRESENCE], [dnl
  AC_PREREQ([2.60])dnl for m4_foreach_w
  m4_foreach_w([mhd_chk_Header], [$1],
    [MHD_CHECK_HEADER_PRESENCE(m4_defn([mhd_chk_Header]))]
  )dnl
])


#
# MHD_CHECK_HEADERS_PRESENCE_COMPACT(oneheader.h otherheader.h ...)
#
# Same as MHD_CHECK_HEADERS_PRESENCE, but a bit slower and produce more compact 'configure'.

AC_DEFUN([MHD_CHECK_HEADERS_PRESENCE_COMPACT], [dnl
  for mhd_chk_Header in $1 ; do
    MHD_CHECK_HEADER_PRESENCE([[${mhd_chk_Header}]])
  done
])


#
# MHD_CHECK_BASIC_HEADERS_PRESENCE
#
# Check basic headers for presence.

AC_DEFUN([MHD_CHECK_BASIC_HEADERS_PRESENCE], [dnl
  MHD_CHECK_HEADERS_PRESENCE([stdio.h wchar.h stdlib.h string.h strings.h stdint.h fcntl.h sys/types.h time.h unistd.h])
])


#
# _MHD_SET_BASIC_INCLUDES
#
# Internal preparatory macro.

AC_DEFUN([_MHD_SET_BASIC_INCLUDES], [dnl
  AC_REQUIRE([MHD_CHECK_BASIC_HEADERS_PRESENCE])dnl
  AS_IF([[test -z "$mhd_basic_headers_includes"]], [dnl
    AS_VAR_IF([mhd_cv_header_stdio_h_present], [["yes"]],
      [[mhd_basic_headers_includes="\
#include <stdio.h>
"     ]],[[mhd_basic_headers_includes=""]]
    )
    AS_VAR_IF([mhd_cv_header_sys_types_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <sys/types.h>
"     ]]
    )
    AS_VAR_IF([mhd_cv_header_wchar_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <wchar.h>
"     ]]
    )
    AS_VAR_IF([mhd_cv_header_stdlib_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <stdlib.h>
"     ]]
    )
    AS_VAR_IF([mhd_cv_header_string_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <string.h>
"     ]]
    )
    AS_VAR_IF([mhd_cv_header_strings_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <strings.h>
"     ]]
    )
    AS_VAR_IF([mhd_cv_header_stdint_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <stdint.h>
"     ]]
    )
    AS_VAR_IF([mhd_cv_header_fcntl_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <fcntl.h>
"     ]]
    )
    AS_VAR_IF([mhd_cv_header_time_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <time.h>
"     ]]
    )
    AS_VAR_IF([mhd_cv_header_unistd_h_present], [["yes"]],
      [[mhd_basic_headers_includes="${mhd_basic_headers_includes}\
#include <unistd.h>
"     ]]
    )
  ])dnl
])


#
# _MHD_BASIC_INCLUDES
#
# Internal macro. Output set of basic includes.

AC_DEFUN([_MHD_BASIC_INCLUDES], [AC_REQUIRE([_MHD_SET_BASIC_INCLUDES])dnl
[ /* Start of MHD basic test includes */
$mhd_basic_headers_includes /* End of MHD basic test includes */
]])


#
# MHD_CHECK_BASIC_HEADERS([PROLOG], [ACTION-IF-OK], [ACTION-IF-FAIL])
#
# Check whether basic headers can be compiled with specified prolog.

AC_DEFUN([MHD_CHECK_BASIC_HEADERS], [dnl
  AC_COMPILE_IFELSE([dnl
    AC_LANG_PROGRAM([m4_n([$1])dnl
_MHD_BASIC_INCLUDES
    ], [[int i = 1; i++]])
  ], [$2], [$3])
])


#
# _MHD_SET_UNDEF_ALL_EXT
#
# Internal preparatory macro.

AC_DEFUN([_MHD_SET_UNDEF_ALL_EXT], [m4_divert_text([INIT_PREPARE],[dnl
[mhd_undef_all_extensions="
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#endif
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#ifdef _XOPEN_SOURCE_EXTENDED
#undef _XOPEN_SOURCE_EXTENDED
#endif
#ifdef _XOPEN_VERSION
#undef _XOPEN_VERSION
#endif
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#ifdef _POSIX_SOURCE
#undef _POSIX_SOURCE
#endif
#ifdef _DEFAULT_SOURCE
#undef _DEFAULT_SOURCE
#endif
#ifdef _BSD_SOURCE
#undef _BSD_SOURCE
#endif
#ifdef _SVID_SOURCE
#undef _SVID_SOURCE
#endif
#ifdef __EXTENSIONS__
#undef __EXTENSIONS__
#endif
#ifdef _ALL_SOURCE
#undef _ALL_SOURCE
#endif
#ifdef _TANDEM_SOURCE
#undef _TANDEM_SOURCE
#endif
#ifdef _DARWIN_C_SOURCE
#undef _DARWIN_C_SOURCE
#endif
#ifdef __BSD_VISIBLE
#undef __BSD_VISIBLE
#endif
#ifdef _NETBSD_SOURCE
#undef _NETBSD_SOURCE
#endif
"
]])])


#
# _MHD_UNDEF_ALL_EXT
#
# Output prolog that undefine all known extension and visibility macros.

AC_DEFUN([_MHD_UNDEF_ALL_EXT], [dnl
AC_REQUIRE([_MHD_SET_UNDEF_ALL_EXT])dnl
$mhd_undef_all_extensions
])


#
# _MHD_CHECK_DEFINED(SYMBOL, [PROLOG],
#                    [ACTION-IF-DEFINED], [ACTION-IF-NOT-DEFINED])
#
# Silently checks for defined symbols.

AC_DEFUN([_MHD_CHECK_DEFINED], [dnl
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
m4_n([$2])dnl
[#ifndef ]$1[
#error ]$1[ is not defined
choke me now;
#endif
        ]],[])
    ], [$3], [$4]
  )
])


#
# MHD_CHECK_DEFINED(SYMBOL, [PROLOG],
#                   [ACTION-IF-DEFINED], [ACTION-IF-NOT-DEFINED],
#                   [MESSAGE])
#
# Cache-check for defined symbols with printing results.

AC_DEFUN([MHD_CHECK_DEFINED], [dnl
  AS_VAR_PUSHDEF([mhd_cache_Var],
    [mhd_cv_macro_[]m4_tolower($1)_defined])dnl
  AC_CACHE_CHECK([dnl
m4_ifnblank([$5], [$5], [whether $1 is already defined])],
    [mhd_cache_Var],
  [
    _MHD_CHECK_DEFINED([$1], [$2],
      [mhd_cache_Var="yes"],
      [mhd_cache_Var="no"]
    )
  ])
  _MHD_VAR_IF([mhd_cache_Var], [["yes"]], [$3], [$4])
  AS_VAR_POPDEF([mhd_cache_Var])dnl
])


#
# MHD_CHECK_DEFINED_MSG(SYMBOL, [PROLOG], [MESSAGE]
#                       [ACTION-IF-DEFINED], [ACTION-IF-NOT-DEFINED])
#
# Cache-check for defined symbols with printing results.
# Reordered arguments for better readability.

AC_DEFUN([MHD_CHECK_DEFINED_MSG],[dnl
MHD_CHECK_DEFINED([$1],[$2],[$4],[$5],[$3])])

#
# MHD_CHECK_ACCEPT_DEFINE(DEFINE-SYMBOL, [DEFINE-VALUE = 1], [PROLOG],
#                         [ACTION-IF-ACCEPTED], [ACTION-IF-NOT-ACCEPTED],
#                         [MESSAGE])
#
# Cache-check whether specific defined symbol do not break basic headers.

AC_DEFUN([MHD_CHECK_ACCEPT_DEFINE], [dnl
  AC_PREREQ([2.64])dnl for AS_VAR_PUSHDEF, AS_VAR_SET, m4_ifnblank
  AS_VAR_PUSHDEF([mhd_cache_Var],
    [mhd_cv_define_[]m4_tolower($1)_accepted[]m4_ifnblank([$2],[_[]$2])])dnl
  AC_CACHE_CHECK([dnl
m4_ifnblank([$6],[$6],[whether headers accept $1[]m4_ifnblank([$2],[ with value $2])])],
    [mhd_cache_Var], [dnl
    MHD_CHECK_BASIC_HEADERS([
m4_n([$3])[#define ]$1 m4_default_nblank([$2],[[1]])],
      [mhd_cache_Var="yes"], [mhd_cache_Var="no"]
    )
  ])
  _MHD_VAR_IF([mhd_cache_Var], [["yes"]], [$4], [$5])
  AS_VAR_POPDEF([mhd_cache_Var])dnl
])


#
# MHD_CHECK_DEF_AND_ACCEPT(DEFINE-SYMBOL, [DEFINE-VALUE = 1], [PROLOG],
#                         [ACTION-IF-DEFINED],
#                         [ACTION-IF-ACCEPTED], [ACTION-IF-NOT-ACCEPTED])
#
# Combination of MHD_CHECK_DEFINED_ECHO and MHD_CHECK_ACCEPT_DEFINE.
# First check whether symbol is already defined and, if not defined,
# checks whether it can be defined.

AC_DEFUN([MHD_CHECK_DEF_AND_ACCEPT], [dnl
  MHD_CHECK_DEFINED([$1], [$3], [$4], [dnl
    MHD_CHECK_ACCEPT_DEFINE([$1], [$2], [$3], [$5], [$6])dnl
  ])dnl
])


#
# _MHD_XOPEN_ADD([PROLOG])
#
# Internal macro. Only to be used in MHD_SYS_EXT.

AC_DEFUN([_MHD_XOPEN_ADD], [dnl
  MHD_CHECK_DEF_AND_ACCEPT([[_XOPEN_SOURCE_EXTENDED]], [],
    [[${mhd_mse_added_prolog}]m4_n([$1])], [],
  [dnl
    _MHD_SYS_EXT_ADD_FLAG([[_XOPEN_SOURCE_EXTENDED]])dnl
  ], [dnl
    MHD_CHECK_DEFINED([[_XOPEN_VERSION]],
      [[${mhd_mse_added_prolog}]m4_n([$1])], [],
    [dnl
      AC_CACHE_CHECK([[for value of _XOPEN_VERSION accepted by headers]],
        [mhd_cv_define__xopen_version_accepted], [dnl
        MHD_CHECK_BASIC_HEADERS([
[${mhd_mse_added_prolog}]m4_n([$1])
[#define _XOPEN_VERSION 4]],
          [[mhd_cv_define__xopen_version_accepted="4"]],
        [
          MHD_CHECK_BASIC_HEADERS([
[${mhd_mse_added_prolog}]m4_n([$1])
[#define _XOPEN_VERSION 3]],
            [[mhd_cv_define__xopen_version_accepted="3"]],
            [[mhd_cv_define__xopen_version_accepted="no"]]
          )
        ])
      ])
      AS_VAR_IF([mhd_cv_define__xopen_version_accepted], [["no"]],
        [[:]],
      [dnl
        _MHD_SYS_EXT_ADD_FLAG([[_XOPEN_VERSION]],
          [[${mhd_cv_define__xopen_version_accepted}]]dnl
        )
      ])
    ])
  ])
])

