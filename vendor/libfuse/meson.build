project('libfuse3', ['c'],
        version: '3.17.3',
        meson_version: '>= 0.60.0',
        default_options: [
            'buildtype=debugoptimized',
            'c_std=gnu11',
            'cpp_std=c++17',
            'warning_level=2',
        ])

# Would be better to create the version string
# from integers, i.e. concatenating strings instead
# of splitting a string, but 'project' needs to be
# the first meson.build keyword...

# split version into base and rc
version_parts = meson.project_version().split('-')
base_version = version_parts[0]

version_list = base_version.split('.')
FUSE_MAJOR_VERSION = version_list[0]
FUSE_MINOR_VERSION = version_list[1]
FUSE_HOTFIX_VERSION = version_list[2]
FUSE_RC_VERSION = version_parts.length() > 1 ? version_parts[1] : ''

platform = host_machine.system()
if platform == 'darwin'
  error('libfuse does not support OS-X.\n' +
        'Take a look at http://osxfuse.github.io/ or the more recent\n' +
        'https://www.fuse-t.org/ instead')
elif platform == 'cygwin' or platform == 'windows'
  error('libfuse does not support Windows.\n' +
        'Take a look at http://www.secfs.net/winfsp/ instead')       
endif

cc = meson.get_compiler('c')

#
# Feature detection, the resulting config file is installed
# with the package.
# Note: Symbols need to be care fully named, to avoid conflicts
#       with applications linking to libfuse and including
#       this config.
#
public_cfg = configuration_data()

public_cfg.set('FUSE_MAJOR_VERSION', FUSE_MAJOR_VERSION)
public_cfg.set('FUSE_MINOR_VERSION', FUSE_MINOR_VERSION)
public_cfg.set('FUSE_HOTFIX_VERSION', FUSE_HOTFIX_VERSION)
public_cfg.set('FUSE_RC_VERSION', FUSE_RC_VERSION)

# Default includes when checking for presence of functions and
# struct members
include_default = '''
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>     /* For static_assert */
#include <pthread.h>    /* For pthread_setname_np */
'''
args_default = [ '-D_GNU_SOURCE' ]

#
# Feature detection, only available at libfuse compilation time,
# but not for application linking to libfuse.
#
private_cfg = configuration_data()
private_cfg.set_quoted('PACKAGE_VERSION', meson.project_version())

# Test for presence of some functions
test_funcs = [ 'fork', 'fstatat', 'openat', 'readlinkat', 'pipe2',
               'splice', 'vmsplice', 'posix_fallocate', 'fdatasync',
               'utimensat', 'copy_file_range', 'fallocate' ]
foreach func : test_funcs
    private_cfg.set('HAVE_' + func.to_upper(),
        cc.has_function(func, prefix: include_default, args: args_default))
endforeach

# Special case checks that need custom code
special_funcs = {
    'static_assert': '''
        #include <assert.h>
        static_assert(1, "test");
        int main(void) { return 0; }
    ''',
    'pthread_setname_np': '''
        #include <pthread.h>
        int main(void) {
            pthread_t thread = pthread_self();
            pthread_setname_np(thread, "test");
            return 0;
        }
    ''',
    'close_range': '''
        #include <unistd.h>
        #include <fcntl.h>
        #include <linux/close_range.h>
        int main(void) {
            unsigned int flags = CLOSE_RANGE_UNSHARE;
            return close_range(3, ~0U, flags);
        }
    '''
}

foreach name, code : special_funcs
    private_cfg.set('HAVE_' + name.to_upper(),
        cc.links(code, args: args_default,
                 name: name + ' check'))
endforeach

# Regular function checks
private_cfg.set('HAVE_SETXATTR',
    cc.has_function('setxattr', prefix: '#include <sys/xattr.h>'))
private_cfg.set('HAVE_ICONV',
    cc.has_function('iconv', prefix: '#include <iconv.h>'))
private_cfg.set('HAVE_BACKTRACE',
    cc.has_function('backtrace', prefix: '#include <execinfo.h>'))

# Struct member checks
private_cfg.set('HAVE_STRUCT_STAT_ST_ATIM',
    cc.has_member('struct stat', 'st_atim',
                  prefix: include_default + '#include <sys/stat.h>',
                  args: args_default))
private_cfg.set('HAVE_STRUCT_STAT_ST_ATIMESPEC',
    cc.has_member('struct stat', 'st_atimespec',
                  prefix: include_default + '#include <sys/stat.h>',
                  args: args_default))

#
# Compiler configuration
#
add_project_arguments('-D_REENTRANT', '-DHAVE_LIBFUSE_PRIVATE_CONFIG_H', '-Wno-sign-compare', '-D_FILE_OFFSET_BITS=64',
                      '-Wstrict-prototypes', '-Wmissing-declarations', '-Wwrite-strings',
                      '-fno-strict-aliasing', language: 'c')
add_project_arguments('-D_REENTRANT', '-DHAVE_LIBFUSE_PRIVATE_CONFIG_H', '-D_GNU_SOURCE', '-D_FILE_OFFSET_BITS=64',
                     '-Wno-sign-compare', '-Wmissing-declarations',
                     '-Wwrite-strings', '-fno-strict-aliasing', language: 'cpp')

# Some (stupid) GCC versions warn about unused return values even when they are
# casted to void. This makes -Wunused-result pretty useless, since there is no
# way to suppress the warning when we really *want* to ignore the value.
code = '''
__attribute__((warn_unused_result)) int get_4() {
    return 4;
}
int main(void) {
    (void) get_4();
    return 0;
}'''
if not cc.compiles(code, args: [ '-O0', '-Werror=unused-result' ])
     message('Compiler warns about unused result even when casting to void')
     add_project_arguments('-Wno-unused-result', language: 'c')
endif

# It is hard to detect if the libc supports versioned symbols. Only gnu-libc
# seems to provide that, but then glibc is the main target for libfuse, so
# enable it by default
versioned_symbols = 1

# This is an attempt to detect if another libc is used.
code = '''
int main(void) {
#if (defined(__UCLIBC__) || defined(__APPLE__))
#error /* libc does not have versioned symbols */
#endif
    return 0;
}'''
if not cc.compiles(code, args: [ '-O0' ])
  versioned_symbols = 0
endif

# The detection can be overridden, which is useful for other (above unhandled)
# libcs and also especially useful for testing
if get_option('disable-libc-symbol-version')
     versioned_symbols = 0
endif

if versioned_symbols == 1
     message('Enabling versioned libc symbols')
     public_cfg.set('LIBFUSE_BUILT_WITH_VERSIONED_SYMBOLS', 1)

     # gcc-10 and newer support the symver attribute which we need to use if we
     # want to support LTO
     # recent clang and gcc both support __has_attribute (and if they are too old
     # to have __has_attribute, then they are too old to support symver)
     # other compilers might not have __has_attribute, but in those cases
     # it is safe for this check to fail and for us to fallback to the old _asm_
     # method for symver. Anyway the attributes not supported by __has_attribute()
     # unfortunately return true giving a false positive. So let's try to build
     # using __attribute__ ((symver )) and see the result.
     code = '''
     __attribute__ ((symver ("test@TEST")))
     void foo(void) {
     }

     int main(void) {
         return 0;
     }'''
     if cc.compiles(code, args: [ '-O0', '-c', '-Werror'])
          message('Compiler supports symver attribute')
          add_project_arguments('-DHAVE_SYMVER_ATTRIBUTE', language: 'c')
     else
          message('Compiler does not support symver attribute')
     endif
else
     message('Disabling versioned libc symbols')
endif

# Older versions of musl libc don't unescape entries in /etc/mtab
# Try to detect this behaviour, and work around, if necessary.
detect_getmntent_needs_unescape = '''
#define _GNU_SOURCE
#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define dir_space_tab "dir\\040space\\011tab"

int main()
{
    const char *fake_mtab = "name " dir_space_tab " type opts 0 0\n";
    FILE *f = fmemopen((void *)fake_mtab, strlen(fake_mtab) + 1, "r");
    struct mntent *entp = getmntent(f);
    fclose(f);
    if(NULL == entp)
        exit(EXIT_FAILURE);
    if (0 == strcmp(entp->mnt_dir, dir_space_tab))
        printf("needs escaping\n");
    else
        printf("no need to escape\n");
}
'''

if not meson.is_cross_build()
  result = cc.run(detect_getmntent_needs_unescape)
  if result.compiled() and result.returncode() == 0 and result.stdout().strip() == 'needs escaping'
    message('getmntent does not unescape')
    add_project_arguments('-DGETMNTENT_NEEDS_UNESCAPING', language: 'c')
  endif
endif

# Write private test results into fuse_config.h (stored in build directory)
configure_file(output: 'fuse_config.h', configuration : private_cfg)

# Write the test results, installed with the package,
# symbols need to be properly prefixed to avoid
# symbol (define) conflicts
configure_file(output: 'libfuse_config.h',
               configuration : public_cfg,
               install: true, install_dir: join_paths(get_option('includedir'), 'fuse3'))

# '.' will refer to current build directory, which contains config.h
include_dirs = include_directories('include', 'lib', '.')

# Common dependencies
thread_dep = dependency('threads') 

#
# Read build files from sub-directories
#
subdirs = [ 'lib', 'include']
if get_option('utils') and not platform.endswith('bsd') and platform != 'dragonfly'
  subdirs += [ 'util', 'doc' ]
endif

if get_option('examples')
  subdirs += 'example'
endif

if get_option('tests')
  subdirs += 'test'
endif

foreach n : subdirs
    subdir(n)
endforeach

