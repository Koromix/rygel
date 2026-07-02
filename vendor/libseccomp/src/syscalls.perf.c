/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf -m 100 --null-strings --pic -tCEG -T -S1 syscalls.perf  */
/* Computed positions: -k'1-2,4-9,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "syscalls.perf"

/**
 * Copyright (c) 2012 Red Hat <pmoore@redhat.com>
 * Copyright (c) 2020 Red Hat <gscrivan@redhat.com>
 * Copyright (c) 2022 Microsoft Corporation. <paulmoore@microsoft.com>
 *
 * Authors: Paul Moore <paul@paul-moore.com>
 *          Giuseppe Scrivano <gscrivan@redhat.com>
 */

/*
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses>.
 */

#include <seccomp.h>
#include <string.h>
#include "syscalls.h"

enum
  {
    TOTAL_KEYWORDS = 508,
    MIN_WORD_LENGTH = 3,
    MAX_WORD_LENGTH = 28,
    MIN_HASH_VALUE = 21,
    MAX_HASH_VALUE = 2025
  };

/* maximum key range = 2005, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register size_t len)
{
  static const unsigned short asso_values[] =
    {
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,   29,
       388,  164,  500, 2026,   45,    6, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026,    4,  280,  122,  436,   15,
        10,    6,    6,    5,  385,    9,   60,  283,   66,   56,
       209,   33,   74,  314,   16,    4,    6,  299,  422,  599,
       237,  383,   27, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2026,
      2026, 2026, 2026, 2026, 2026, 2026, 2026
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[8]];
      /*FALLTHROUGH*/
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
        hval += asso_values[(unsigned char)str[3]+1];
      /*FALLTHROUGH*/
      case 3:
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
  {
    char stringpool_str0[sizeof("tee")];
    char stringpool_str1[sizeof("send")];
    char stringpool_str2[sizeof("time")];
    char stringpool_str3[sizeof("times")];
    char stringpool_str4[sizeof("idle")];
    char stringpool_str5[sizeof("rtas")];
    char stringpool_str6[sizeof("read")];
    char stringpool_str7[sizeof("select")];
    char stringpool_str8[sizeof("setsid")];
    char stringpool_str9[sizeof("getsid")];
    char stringpool_str10[sizeof("setns")];
    char stringpool_str11[sizeof("getegid")];
    char stringpool_str12[sizeof("setfsgid")];
    char stringpool_str13[sizeof("setregid")];
    char stringpool_str14[sizeof("setresgid")];
    char stringpool_str15[sizeof("getresgid")];
    char stringpool_str16[sizeof("timerfd")];
    char stringpool_str17[sizeof("fchdir")];
    char stringpool_str18[sizeof("timer_settime")];
    char stringpool_str19[sizeof("timer_gettime")];
    char stringpool_str20[sizeof("fsync")];
    char stringpool_str21[sizeof("timerfd_settime")];
    char stringpool_str22[sizeof("timerfd_gettime")];
    char stringpool_str23[sizeof("sched_setattr")];
    char stringpool_str24[sizeof("sched_getattr")];
    char stringpool_str25[sizeof("readdir")];
    char stringpool_str26[sizeof("sched_setscheduler")];
    char stringpool_str27[sizeof("sched_getscheduler")];
    char stringpool_str28[sizeof("timerfd_create")];
    char stringpool_str29[sizeof("sendmsg")];
    char stringpool_str30[sizeof("sendto")];
    char stringpool_str31[sizeof("timer_create")];
    char stringpool_str32[sizeof("pipe")];
    char stringpool_str33[sizeof("ipc")];
    char stringpool_str34[sizeof("close")];
    char stringpool_str35[sizeof("prof")];
    char stringpool_str36[sizeof("sendfile")];
    char stringpool_str37[sizeof("ioprio_set")];
    char stringpool_str38[sizeof("ioprio_get")];
    char stringpool_str39[sizeof("connect")];
    char stringpool_str40[sizeof("memfd_secret")];
    char stringpool_str41[sizeof("sched_setparam")];
    char stringpool_str42[sizeof("sched_getparam")];
    char stringpool_str43[sizeof("socket")];
    char stringpool_str44[sizeof("msync")];
    char stringpool_str45[sizeof("clone")];
    char stringpool_str46[sizeof("rt_sigtimedwait")];
    char stringpool_str47[sizeof("memfd_create")];
    char stringpool_str48[sizeof("mount")];
    char stringpool_str49[sizeof("pidfd_getfd")];
    char stringpool_str50[sizeof("timer_delete")];
    char stringpool_str51[sizeof("mincore")];
    char stringpool_str52[sizeof("delete_module")];
    char stringpool_str53[sizeof("reboot")];
    char stringpool_str54[sizeof("sendmmsg")];
    char stringpool_str55[sizeof("rseq_slice_yield")];
    char stringpool_str56[sizeof("access")];
    char stringpool_str57[sizeof("sched_rr_get_interval")];
    char stringpool_str58[sizeof("semctl")];
    char stringpool_str59[sizeof("capset")];
    char stringpool_str60[sizeof("iopl")];
    char stringpool_str61[sizeof("rmdir")];
    char stringpool_str62[sizeof("splice")];
    char stringpool_str63[sizeof("_sysctl")];
    char stringpool_str64[sizeof("setrlimit")];
    char stringpool_str65[sizeof("getrlimit")];
    char stringpool_str66[sizeof("mount_setattr")];
    char stringpool_str67[sizeof("ioperm")];
    char stringpool_str68[sizeof("copy_file_range")];
    char stringpool_str69[sizeof("setitimer")];
    char stringpool_str70[sizeof("getitimer")];
    char stringpool_str71[sizeof("file_setattr")];
    char stringpool_str72[sizeof("file_getattr")];
    char stringpool_str73[sizeof("open_tree")];
    char stringpool_str74[sizeof("process_madvise")];
    char stringpool_str75[sizeof("process_mrelease")];
    char stringpool_str76[sizeof("finit_module")];
    char stringpool_str77[sizeof("open_tree_attr")];
    char stringpool_str78[sizeof("msgctl")];
    char stringpool_str79[sizeof("truncate")];
    char stringpool_str80[sizeof("pause")];
    char stringpool_str81[sizeof("poll")];
    char stringpool_str82[sizeof("nice")];
    char stringpool_str83[sizeof("accept")];
    char stringpool_str84[sizeof("stime")];
    char stringpool_str85[sizeof("semop")];
    char stringpool_str86[sizeof("ftime")];
    char stringpool_str87[sizeof("profil")];
    char stringpool_str88[sizeof("mprotect")];
    char stringpool_str89[sizeof("oldstat")];
    char stringpool_str90[sizeof("getdents")];
    char stringpool_str91[sizeof("oldfstat")];
    char stringpool_str92[sizeof("pselect6")];
    char stringpool_str93[sizeof("epoll_create")];
    char stringpool_str94[sizeof("seccomp")];
    char stringpool_str95[sizeof("pivot_root")];
    char stringpool_str96[sizeof("signalfd")];
    char stringpool_str97[sizeof("membarrier")];
    char stringpool_str98[sizeof("tgkill")];
    char stringpool_str99[sizeof("openat")];
    char stringpool_str100[sizeof("linkat")];
    char stringpool_str101[sizeof("timer_getoverrun")];
    char stringpool_str102[sizeof("epoll_create1")];
    char stringpool_str103[sizeof("sched_get_priority_min")];
    char stringpool_str104[sizeof("fchmod")];
    char stringpool_str105[sizeof("rt_sigreturn")];
    char stringpool_str106[sizeof("faccessat")];
    char stringpool_str107[sizeof("migrate_pages")];
    char stringpool_str108[sizeof("msgsnd")];
    char stringpool_str109[sizeof("cachestat")];
    char stringpool_str110[sizeof("signal")];
    char stringpool_str111[sizeof("oldlstat")];
    char stringpool_str112[sizeof("alarm")];
    char stringpool_str113[sizeof("cachectl")];
    char stringpool_str114[sizeof("sched_get_priority_max")];
    char stringpool_str115[sizeof("epoll_ctl_old")];
    char stringpool_str116[sizeof("stat")];
    char stringpool_str117[sizeof("move_pages")];
    char stringpool_str118[sizeof("fsconfig")];
    char stringpool_str119[sizeof("s390_pci_mmio_write")];
    char stringpool_str120[sizeof("statfs")];
    char stringpool_str121[sizeof("s390_pci_mmio_read")];
    char stringpool_str122[sizeof("arch_prctl")];
    char stringpool_str123[sizeof("ppoll")];
    char stringpool_str124[sizeof("gettid")];
    char stringpool_str125[sizeof("socketpair")];
    char stringpool_str126[sizeof("rt_sigpending")];
    char stringpool_str127[sizeof("geteuid")];
    char stringpool_str128[sizeof("open")];
    char stringpool_str129[sizeof("rseq")];
    char stringpool_str130[sizeof("setfsuid")];
    char stringpool_str131[sizeof("setreuid")];
    char stringpool_str132[sizeof("getpid")];
    char stringpool_str133[sizeof("setresuid")];
    char stringpool_str134[sizeof("getresuid")];
    char stringpool_str135[sizeof("setpgid")];
    char stringpool_str136[sizeof("getpgid")];
    char stringpool_str137[sizeof("epoll_ctl")];
    char stringpool_str138[sizeof("mpx")];
    char stringpool_str139[sizeof("rt_sigsuspend")];
    char stringpool_str140[sizeof("set_tls")];
    char stringpool_str141[sizeof("get_tls")];
    char stringpool_str142[sizeof("fallocate")];
    char stringpool_str143[sizeof("pciconfig_write")];
    char stringpool_str144[sizeof("pciconfig_iobase")];
    char stringpool_str145[sizeof("pciconfig_read")];
    char stringpool_str146[sizeof("dup")];
    char stringpool_str147[sizeof("fork")];
    char stringpool_str148[sizeof("socketcall")];
    char stringpool_str149[sizeof("getpmsg")];
    char stringpool_str150[sizeof("pidfd_send_signal")];
    char stringpool_str151[sizeof("sysfs")];
    char stringpool_str152[sizeof("rt_sigaction")];
    char stringpool_str153[sizeof("lsm_set_self_attr")];
    char stringpool_str154[sizeof("lsm_get_self_attr")];
    char stringpool_str155[sizeof("sethostname")];
    char stringpool_str156[sizeof("clock_getres")];
    char stringpool_str157[sizeof("shmdt")];
    char stringpool_str158[sizeof("clock_settime")];
    char stringpool_str159[sizeof("clock_gettime")];
    char stringpool_str160[sizeof("fchmodat")];
    char stringpool_str161[sizeof("sync")];
    char stringpool_str162[sizeof("syncfs")];
    char stringpool_str163[sizeof("kill")];
    char stringpool_str164[sizeof("semget")];
    char stringpool_str165[sizeof("kexec_file_load")];
    char stringpool_str166[sizeof("stty")];
    char stringpool_str167[sizeof("gtty")];
    char stringpool_str168[sizeof("link")];
    char stringpool_str169[sizeof("setgid")];
    char stringpool_str170[sizeof("getgid")];
    char stringpool_str171[sizeof("pidfd_open")];
    char stringpool_str172[sizeof("getppid")];
    char stringpool_str173[sizeof("mkdir")];
    char stringpool_str174[sizeof("pkey_free")];
    char stringpool_str175[sizeof("mknod")];
    char stringpool_str176[sizeof("keyctl")];
    char stringpool_str177[sizeof("acct")];
    char stringpool_str178[sizeof("setresgid32")];
    char stringpool_str179[sizeof("getresgid32")];
    char stringpool_str180[sizeof("lock")];
    char stringpool_str181[sizeof("clone3")];
    char stringpool_str182[sizeof("sched_setaffinity")];
    char stringpool_str183[sizeof("sched_getaffinity")];
    char stringpool_str184[sizeof("fcntl")];
    char stringpool_str185[sizeof("sched_yield")];
    char stringpool_str186[sizeof("lsm_list_modules")];
    char stringpool_str187[sizeof("set_tid_address")];
    char stringpool_str188[sizeof("close_range")];
    char stringpool_str189[sizeof("rt_sigprocmask")];
    char stringpool_str190[sizeof("fstat")];
    char stringpool_str191[sizeof("setdomainname")];
    char stringpool_str192[sizeof("bind")];
    char stringpool_str193[sizeof("getrusage")];
    char stringpool_str194[sizeof("setuid")];
    char stringpool_str195[sizeof("getuid")];
    char stringpool_str196[sizeof("msgget")];
    char stringpool_str197[sizeof("setsockopt")];
    char stringpool_str198[sizeof("getsockopt")];
    char stringpool_str199[sizeof("fstatfs")];
    char stringpool_str200[sizeof("ioctl")];
    char stringpool_str201[sizeof("flistxattr")];
    char stringpool_str202[sizeof("tkill")];
    char stringpool_str203[sizeof("creat")];
    char stringpool_str204[sizeof("getpagesize")];
    char stringpool_str205[sizeof("syslog")];
    char stringpool_str206[sizeof("newfstatat")];
    char stringpool_str207[sizeof("chdir")];
    char stringpool_str208[sizeof("chmod")];
    char stringpool_str209[sizeof("eventfd")];
    char stringpool_str210[sizeof("getpgrp")];
    char stringpool_str211[sizeof("mmap")];
    char stringpool_str212[sizeof("io_destroy")];
    char stringpool_str213[sizeof("semtimedop")];
    char stringpool_str214[sizeof("sync_file_range")];
    char stringpool_str215[sizeof("getrandom")];
    char stringpool_str216[sizeof("io_setup")];
    char stringpool_str217[sizeof("bpf")];
    char stringpool_str218[sizeof("lstat")];
    char stringpool_str219[sizeof("chroot")];
    char stringpool_str220[sizeof("prctl")];
    char stringpool_str221[sizeof("utime")];
    char stringpool_str222[sizeof("rename")];
    char stringpool_str223[sizeof("vm86")];
    char stringpool_str224[sizeof("utimes")];
    char stringpool_str225[sizeof("spu_create")];
    char stringpool_str226[sizeof("llistxattr")];
    char stringpool_str227[sizeof("shmctl")];
    char stringpool_str228[sizeof("set_mempolicy_home_node")];
    char stringpool_str229[sizeof("io_cancel")];
    char stringpool_str230[sizeof("capget")];
    char stringpool_str231[sizeof("ptrace")];
    char stringpool_str232[sizeof("mbind")];
    char stringpool_str233[sizeof("exit")];
    char stringpool_str234[sizeof("restart_syscall")];
    char stringpool_str235[sizeof("kexec_load")];
    char stringpool_str236[sizeof("mkdirat")];
    char stringpool_str237[sizeof("mremap")];
    char stringpool_str238[sizeof("usr26")];
    char stringpool_str239[sizeof("_newselect")];
    char stringpool_str240[sizeof("mknodat")];
    char stringpool_str241[sizeof("setxattr")];
    char stringpool_str242[sizeof("getxattr")];
    char stringpool_str243[sizeof("timer_settime64")];
    char stringpool_str244[sizeof("timer_gettime64")];
    char stringpool_str245[sizeof("futimesat")];
    char stringpool_str246[sizeof("timerfd_settime64")];
    char stringpool_str247[sizeof("timerfd_gettime64")];
    char stringpool_str248[sizeof("pkey_mprotect")];
    char stringpool_str249[sizeof("lookup_dcookie")];
    char stringpool_str250[sizeof("clock_adjtime")];
    char stringpool_str251[sizeof("create_module")];
    char stringpool_str252[sizeof("listns")];
    char stringpool_str253[sizeof("ulimit")];
    char stringpool_str254[sizeof("sched_rr_get_interval_time64")];
    char stringpool_str255[sizeof("setfsgid32")];
    char stringpool_str256[sizeof("setregid32")];
    char stringpool_str257[sizeof("vm86old")];
    char stringpool_str258[sizeof("fsmount")];
    char stringpool_str259[sizeof("landlock_add_rule")];
    char stringpool_str260[sizeof("landlock_restrict_self")];
    char stringpool_str261[sizeof("landlock_create_ruleset")];
    char stringpool_str262[sizeof("fanotify_init")];
    char stringpool_str263[sizeof("sigsuspend")];
    char stringpool_str264[sizeof("mseal")];
    char stringpool_str265[sizeof("modify_ldt")];
    char stringpool_str266[sizeof("rt_sigtimedwait_time64")];
    char stringpool_str267[sizeof("remap_file_pages")];
    char stringpool_str268[sizeof("tuxcall")];
    char stringpool_str269[sizeof("nanosleep")];
    char stringpool_str270[sizeof("getcwd")];
    char stringpool_str271[sizeof("lseek")];
    char stringpool_str272[sizeof("flock")];
    char stringpool_str273[sizeof("fspick")];
    char stringpool_str274[sizeof("sendfile64")];
    char stringpool_str275[sizeof("_llseek")];
    char stringpool_str276[sizeof("renameat")];
    char stringpool_str277[sizeof("s390_guarded_storage")];
    char stringpool_str278[sizeof("unshare")];
    char stringpool_str279[sizeof("faccessat2")];
    char stringpool_str280[sizeof("setxattrat")];
    char stringpool_str281[sizeof("getxattrat")];
    char stringpool_str282[sizeof("kcmp")];
    char stringpool_str283[sizeof("readahead")];
    char stringpool_str284[sizeof("execve")];
    char stringpool_str285[sizeof("sysmips")];
    char stringpool_str286[sizeof("getsockname")];
    char stringpool_str287[sizeof("recvmsg")];
    char stringpool_str288[sizeof("getcpu")];
    char stringpool_str289[sizeof("pkey_alloc")];
    char stringpool_str290[sizeof("getpeername")];
    char stringpool_str291[sizeof("mlock")];
    char stringpool_str292[sizeof("rt_tgsigqueueinfo")];
    char stringpool_str293[sizeof("rt_sigqueueinfo")];
    char stringpool_str294[sizeof("move_mount")];
    char stringpool_str295[sizeof("fsetxattr")];
    char stringpool_str296[sizeof("fgetxattr")];
    char stringpool_str297[sizeof("syscall")];
    char stringpool_str298[sizeof("pselect6_time64")];
    char stringpool_str299[sizeof("readlinkat")];
    char stringpool_str300[sizeof("uname")];
    char stringpool_str301[sizeof("sysinfo")];
    char stringpool_str302[sizeof("io_uring_enter")];
    char stringpool_str303[sizeof("brk")];
    char stringpool_str304[sizeof("io_uring_register")];
    char stringpool_str305[sizeof("setresuid32")];
    char stringpool_str306[sizeof("getresuid32")];
    char stringpool_str307[sizeof("mlockall")];
    char stringpool_str308[sizeof("fsopen")];
    char stringpool_str309[sizeof("recvmmsg")];
    char stringpool_str310[sizeof("ustat")];
    char stringpool_str311[sizeof("truncate64")];
    char stringpool_str312[sizeof("nfsservctl")];
    char stringpool_str313[sizeof("putpmsg")];
    char stringpool_str314[sizeof("s390_runtime_instr")];
    char stringpool_str315[sizeof("sigreturn")];
    char stringpool_str316[sizeof("ugetrlimit")];
    char stringpool_str317[sizeof("mq_timedsend")];
    char stringpool_str318[sizeof("listxattrat")];
    char stringpool_str319[sizeof("lsetxattr")];
    char stringpool_str320[sizeof("lgetxattr")];
    char stringpool_str321[sizeof("sigpending")];
    char stringpool_str322[sizeof("listxattr")];
    char stringpool_str323[sizeof("mq_timedreceive")];
    char stringpool_str324[sizeof("statx")];
    char stringpool_str325[sizeof("futex")];
    char stringpool_str326[sizeof("name_to_handle_at")];
    char stringpool_str327[sizeof("io_uring_setup")];
    char stringpool_str328[sizeof("recvfrom")];
    char stringpool_str329[sizeof("getdents64")];
    char stringpool_str330[sizeof("shmget")];
    char stringpool_str331[sizeof("setpriority")];
    char stringpool_str332[sizeof("getpriority")];
    char stringpool_str333[sizeof("listen")];
    char stringpool_str334[sizeof("ftruncate")];
    char stringpool_str335[sizeof("madvise")];
    char stringpool_str336[sizeof("mq_getsetattr")];
    char stringpool_str337[sizeof("settimeofday")];
    char stringpool_str338[sizeof("gettimeofday")];
    char stringpool_str339[sizeof("io_pgetevents")];
    char stringpool_str340[sizeof("execveat")];
    char stringpool_str341[sizeof("userfaultfd")];
    char stringpool_str342[sizeof("get_kernel_syms")];
    char stringpool_str343[sizeof("setgroups")];
    char stringpool_str344[sizeof("getgroups")];
    char stringpool_str345[sizeof("munmap")];
    char stringpool_str346[sizeof("atomic_cmpxchg_32")];
    char stringpool_str347[sizeof("shmat")];
    char stringpool_str348[sizeof("subpage_prot")];
    char stringpool_str349[sizeof("riscv_flush_icache")];
    char stringpool_str350[sizeof("ppoll_time64")];
    char stringpool_str351[sizeof("sigprocmask")];
    char stringpool_str352[sizeof("dup2")];
    char stringpool_str353[sizeof("io_submit")];
    char stringpool_str354[sizeof("pipe2")];
    char stringpool_str355[sizeof("utimensat")];
    char stringpool_str356[sizeof("readv")];
    char stringpool_str357[sizeof("readlink")];
    char stringpool_str358[sizeof("io_getevents")];
    char stringpool_str359[sizeof("sync_file_range2")];
    char stringpool_str360[sizeof("oldolduname")];
    char stringpool_str361[sizeof("vserver")];
    char stringpool_str362[sizeof("setfsuid32")];
    char stringpool_str363[sizeof("setreuid32")];
    char stringpool_str364[sizeof("uprobe")];
    char stringpool_str365[sizeof("vmsplice")];
    char stringpool_str366[sizeof("fanotify_mark")];
    char stringpool_str367[sizeof("futex_requeue")];
    char stringpool_str368[sizeof("removexattrat")];
    char stringpool_str369[sizeof("clock_settime64")];
    char stringpool_str370[sizeof("clock_gettime64")];
    char stringpool_str371[sizeof("set_mempolicy")];
    char stringpool_str372[sizeof("get_mempolicy")];
    char stringpool_str373[sizeof("clock_getres_time64")];
    char stringpool_str374[sizeof("removexattr")];
    char stringpool_str375[sizeof("personality")];
    char stringpool_str376[sizeof("cacheflush")];
    char stringpool_str377[sizeof("statmount")];
    char stringpool_str378[sizeof("olduname")];
    char stringpool_str379[sizeof("msgrcv")];
    char stringpool_str380[sizeof("write")];
    char stringpool_str381[sizeof("umask")];
    char stringpool_str382[sizeof("init_module")];
    char stringpool_str383[sizeof("sigaction")];
    char stringpool_str384[sizeof("semtimedop_time64")];
    char stringpool_str385[sizeof("inotify_init")];
    char stringpool_str386[sizeof("sigaltstack")];
    char stringpool_str387[sizeof("mq_open")];
    char stringpool_str388[sizeof("epoll_wait")];
    char stringpool_str389[sizeof("atomic_barrier")];
    char stringpool_str390[sizeof("perf_event_open")];
    char stringpool_str391[sizeof("epoll_wait_old")];
    char stringpool_str392[sizeof("set_thread_area")];
    char stringpool_str393[sizeof("get_thread_area")];
    char stringpool_str394[sizeof("inotify_init1")];
    char stringpool_str395[sizeof("swapoff")];
    char stringpool_str396[sizeof("dup3")];
    char stringpool_str397[sizeof("process_vm_readv")];
    char stringpool_str398[sizeof("process_vm_writev")];
    char stringpool_str399[sizeof("exit_group")];
    char stringpool_str400[sizeof("adjtimex")];
    char stringpool_str401[sizeof("getegid32")];
    char stringpool_str402[sizeof("listmount")];
    char stringpool_str403[sizeof("s390_sthyi")];
    char stringpool_str404[sizeof("vfork")];
    char stringpool_str405[sizeof("umount")];
    char stringpool_str406[sizeof("munlockall")];
    char stringpool_str407[sizeof("clock_nanosleep")];
    char stringpool_str408[sizeof("afs_syscall")];
    char stringpool_str409[sizeof("epoll_pwait")];
    char stringpool_str410[sizeof("munlock")];
    char stringpool_str411[sizeof("fchownat")];
    char stringpool_str412[sizeof("recv")];
    char stringpool_str413[sizeof("openat2")];
    char stringpool_str414[sizeof("waitid")];
    char stringpool_str415[sizeof("arm_sync_file_range")];
    char stringpool_str416[sizeof("chown")];
    char stringpool_str417[sizeof("ssetmask")];
    char stringpool_str418[sizeof("sgetmask")];
    char stringpool_str419[sizeof("fdatasync")];
    char stringpool_str420[sizeof("multiplexer")];
    char stringpool_str421[sizeof("fremovexattr")];
    char stringpool_str422[sizeof("clock_adjtime64")];
    char stringpool_str423[sizeof("symlinkat")];
    char stringpool_str424[sizeof("spu_run")];
    char stringpool_str425[sizeof("set_robust_list")];
    char stringpool_str426[sizeof("get_robust_list")];
    char stringpool_str427[sizeof("fchown")];
    char stringpool_str428[sizeof("query_module")];
    char stringpool_str429[sizeof("waitpid")];
    char stringpool_str430[sizeof("futex_time64")];
    char stringpool_str431[sizeof("lremovexattr")];
    char stringpool_str432[sizeof("request_key")];
    char stringpool_str433[sizeof("quotactl_fd")];
    char stringpool_str434[sizeof("lchown")];
    char stringpool_str435[sizeof("fchmodat2")];
    char stringpool_str436[sizeof("uretprobe")];
    char stringpool_str437[sizeof("quotactl")];
    char stringpool_str438[sizeof("mlock2")];
    char stringpool_str439[sizeof("unlinkat")];
    char stringpool_str440[sizeof("swapcontext")];
    char stringpool_str441[sizeof("mmap2")];
    char stringpool_str442[sizeof("setgroups32")];
    char stringpool_str443[sizeof("getgroups32")];
    char stringpool_str444[sizeof("accept4")];
    char stringpool_str445[sizeof("mq_notify")];
    char stringpool_str446[sizeof("symlink")];
    char stringpool_str447[sizeof("security")];
    char stringpool_str448[sizeof("uselib")];
    char stringpool_str449[sizeof("signalfd4")];
    char stringpool_str450[sizeof("recvmmsg_time64")];
    char stringpool_str451[sizeof("eventfd2")];
    char stringpool_str452[sizeof("mq_timedsend_time64")];
    char stringpool_str453[sizeof("prlimit64")];
    char stringpool_str454[sizeof("geteuid32")];
    char stringpool_str455[sizeof("mq_timedreceive_time64")];
    char stringpool_str456[sizeof("futex_wait")];
    char stringpool_str457[sizeof("vhangup")];
    char stringpool_str458[sizeof("ftruncate64")];
    char stringpool_str459[sizeof("map_shadow_stack")];
    char stringpool_str460[sizeof("io_pgetevents_time64")];
    char stringpool_str461[sizeof("inotify_rm_watch")];
    char stringpool_str462[sizeof("unlink")];
    char stringpool_str463[sizeof("stat64")];
    char stringpool_str464[sizeof("setgid32")];
    char stringpool_str465[sizeof("getgid32")];
    char stringpool_str466[sizeof("statfs64")];
    char stringpool_str467[sizeof("swapon")];
    char stringpool_str468[sizeof("open_by_handle_at")];
    char stringpool_str469[sizeof("utimensat_time64")];
    char stringpool_str470[sizeof("preadv")];
    char stringpool_str471[sizeof("setuid32")];
    char stringpool_str472[sizeof("getuid32")];
    char stringpool_str473[sizeof("epoll_pwait2")];
    char stringpool_str474[sizeof("renameat2")];
    char stringpool_str475[sizeof("fcntl64")];
    char stringpool_str476[sizeof("clock_nanosleep_time64")];
    char stringpool_str477[sizeof("inotify_add_watch")];
    char stringpool_str478[sizeof("sys_debug_setcontext")];
    char stringpool_str479[sizeof("break")];
    char stringpool_str480[sizeof("add_key")];
    char stringpool_str481[sizeof("arm_fadvise64_64")];
    char stringpool_str482[sizeof("fstat64")];
    char stringpool_str483[sizeof("breakpoint")];
    char stringpool_str484[sizeof("fstatfs64")];
    char stringpool_str485[sizeof("riscv_hwprobe")];
    char stringpool_str486[sizeof("lstat64")];
    char stringpool_str487[sizeof("futex_wake")];
    char stringpool_str488[sizeof("bdflush")];
    char stringpool_str489[sizeof("usr32")];
    char stringpool_str490[sizeof("pread64")];
    char stringpool_str491[sizeof("pwritev")];
    char stringpool_str492[sizeof("fstatat64")];
    char stringpool_str493[sizeof("futex_waitv")];
    char stringpool_str494[sizeof("preadv2")];
    char stringpool_str495[sizeof("switch_endian")];
    char stringpool_str496[sizeof("shutdown")];
    char stringpool_str497[sizeof("writev")];
    char stringpool_str498[sizeof("umount2")];
    char stringpool_str499[sizeof("chown32")];
    char stringpool_str500[sizeof("pwrite64")];
    char stringpool_str501[sizeof("fadvise64")];
    char stringpool_str502[sizeof("fadvise64_64")];
    char stringpool_str503[sizeof("fchown32")];
    char stringpool_str504[sizeof("mq_unlink")];
    char stringpool_str505[sizeof("lchown32")];
    char stringpool_str506[sizeof("pwritev2")];
    char stringpool_str507[sizeof("wait4")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "tee",
    "send",
    "time",
    "times",
    "idle",
    "rtas",
    "read",
    "select",
    "setsid",
    "getsid",
    "setns",
    "getegid",
    "setfsgid",
    "setregid",
    "setresgid",
    "getresgid",
    "timerfd",
    "fchdir",
    "timer_settime",
    "timer_gettime",
    "fsync",
    "timerfd_settime",
    "timerfd_gettime",
    "sched_setattr",
    "sched_getattr",
    "readdir",
    "sched_setscheduler",
    "sched_getscheduler",
    "timerfd_create",
    "sendmsg",
    "sendto",
    "timer_create",
    "pipe",
    "ipc",
    "close",
    "prof",
    "sendfile",
    "ioprio_set",
    "ioprio_get",
    "connect",
    "memfd_secret",
    "sched_setparam",
    "sched_getparam",
    "socket",
    "msync",
    "clone",
    "rt_sigtimedwait",
    "memfd_create",
    "mount",
    "pidfd_getfd",
    "timer_delete",
    "mincore",
    "delete_module",
    "reboot",
    "sendmmsg",
    "rseq_slice_yield",
    "access",
    "sched_rr_get_interval",
    "semctl",
    "capset",
    "iopl",
    "rmdir",
    "splice",
    "_sysctl",
    "setrlimit",
    "getrlimit",
    "mount_setattr",
    "ioperm",
    "copy_file_range",
    "setitimer",
    "getitimer",
    "file_setattr",
    "file_getattr",
    "open_tree",
    "process_madvise",
    "process_mrelease",
    "finit_module",
    "open_tree_attr",
    "msgctl",
    "truncate",
    "pause",
    "poll",
    "nice",
    "accept",
    "stime",
    "semop",
    "ftime",
    "profil",
    "mprotect",
    "oldstat",
    "getdents",
    "oldfstat",
    "pselect6",
    "epoll_create",
    "seccomp",
    "pivot_root",
    "signalfd",
    "membarrier",
    "tgkill",
    "openat",
    "linkat",
    "timer_getoverrun",
    "epoll_create1",
    "sched_get_priority_min",
    "fchmod",
    "rt_sigreturn",
    "faccessat",
    "migrate_pages",
    "msgsnd",
    "cachestat",
    "signal",
    "oldlstat",
    "alarm",
    "cachectl",
    "sched_get_priority_max",
    "epoll_ctl_old",
    "stat",
    "move_pages",
    "fsconfig",
    "s390_pci_mmio_write",
    "statfs",
    "s390_pci_mmio_read",
    "arch_prctl",
    "ppoll",
    "gettid",
    "socketpair",
    "rt_sigpending",
    "geteuid",
    "open",
    "rseq",
    "setfsuid",
    "setreuid",
    "getpid",
    "setresuid",
    "getresuid",
    "setpgid",
    "getpgid",
    "epoll_ctl",
    "mpx",
    "rt_sigsuspend",
    "set_tls",
    "get_tls",
    "fallocate",
    "pciconfig_write",
    "pciconfig_iobase",
    "pciconfig_read",
    "dup",
    "fork",
    "socketcall",
    "getpmsg",
    "pidfd_send_signal",
    "sysfs",
    "rt_sigaction",
    "lsm_set_self_attr",
    "lsm_get_self_attr",
    "sethostname",
    "clock_getres",
    "shmdt",
    "clock_settime",
    "clock_gettime",
    "fchmodat",
    "sync",
    "syncfs",
    "kill",
    "semget",
    "kexec_file_load",
    "stty",
    "gtty",
    "link",
    "setgid",
    "getgid",
    "pidfd_open",
    "getppid",
    "mkdir",
    "pkey_free",
    "mknod",
    "keyctl",
    "acct",
    "setresgid32",
    "getresgid32",
    "lock",
    "clone3",
    "sched_setaffinity",
    "sched_getaffinity",
    "fcntl",
    "sched_yield",
    "lsm_list_modules",
    "set_tid_address",
    "close_range",
    "rt_sigprocmask",
    "fstat",
    "setdomainname",
    "bind",
    "getrusage",
    "setuid",
    "getuid",
    "msgget",
    "setsockopt",
    "getsockopt",
    "fstatfs",
    "ioctl",
    "flistxattr",
    "tkill",
    "creat",
    "getpagesize",
    "syslog",
    "newfstatat",
    "chdir",
    "chmod",
    "eventfd",
    "getpgrp",
    "mmap",
    "io_destroy",
    "semtimedop",
    "sync_file_range",
    "getrandom",
    "io_setup",
    "bpf",
    "lstat",
    "chroot",
    "prctl",
    "utime",
    "rename",
    "vm86",
    "utimes",
    "spu_create",
    "llistxattr",
    "shmctl",
    "set_mempolicy_home_node",
    "io_cancel",
    "capget",
    "ptrace",
    "mbind",
    "exit",
    "restart_syscall",
    "kexec_load",
    "mkdirat",
    "mremap",
    "usr26",
    "_newselect",
    "mknodat",
    "setxattr",
    "getxattr",
    "timer_settime64",
    "timer_gettime64",
    "futimesat",
    "timerfd_settime64",
    "timerfd_gettime64",
    "pkey_mprotect",
    "lookup_dcookie",
    "clock_adjtime",
    "create_module",
    "listns",
    "ulimit",
    "sched_rr_get_interval_time64",
    "setfsgid32",
    "setregid32",
    "vm86old",
    "fsmount",
    "landlock_add_rule",
    "landlock_restrict_self",
    "landlock_create_ruleset",
    "fanotify_init",
    "sigsuspend",
    "mseal",
    "modify_ldt",
    "rt_sigtimedwait_time64",
    "remap_file_pages",
    "tuxcall",
    "nanosleep",
    "getcwd",
    "lseek",
    "flock",
    "fspick",
    "sendfile64",
    "_llseek",
    "renameat",
    "s390_guarded_storage",
    "unshare",
    "faccessat2",
    "setxattrat",
    "getxattrat",
    "kcmp",
    "readahead",
    "execve",
    "sysmips",
    "getsockname",
    "recvmsg",
    "getcpu",
    "pkey_alloc",
    "getpeername",
    "mlock",
    "rt_tgsigqueueinfo",
    "rt_sigqueueinfo",
    "move_mount",
    "fsetxattr",
    "fgetxattr",
    "syscall",
    "pselect6_time64",
    "readlinkat",
    "uname",
    "sysinfo",
    "io_uring_enter",
    "brk",
    "io_uring_register",
    "setresuid32",
    "getresuid32",
    "mlockall",
    "fsopen",
    "recvmmsg",
    "ustat",
    "truncate64",
    "nfsservctl",
    "putpmsg",
    "s390_runtime_instr",
    "sigreturn",
    "ugetrlimit",
    "mq_timedsend",
    "listxattrat",
    "lsetxattr",
    "lgetxattr",
    "sigpending",
    "listxattr",
    "mq_timedreceive",
    "statx",
    "futex",
    "name_to_handle_at",
    "io_uring_setup",
    "recvfrom",
    "getdents64",
    "shmget",
    "setpriority",
    "getpriority",
    "listen",
    "ftruncate",
    "madvise",
    "mq_getsetattr",
    "settimeofday",
    "gettimeofday",
    "io_pgetevents",
    "execveat",
    "userfaultfd",
    "get_kernel_syms",
    "setgroups",
    "getgroups",
    "munmap",
    "atomic_cmpxchg_32",
    "shmat",
    "subpage_prot",
    "riscv_flush_icache",
    "ppoll_time64",
    "sigprocmask",
    "dup2",
    "io_submit",
    "pipe2",
    "utimensat",
    "readv",
    "readlink",
    "io_getevents",
    "sync_file_range2",
    "oldolduname",
    "vserver",
    "setfsuid32",
    "setreuid32",
    "uprobe",
    "vmsplice",
    "fanotify_mark",
    "futex_requeue",
    "removexattrat",
    "clock_settime64",
    "clock_gettime64",
    "set_mempolicy",
    "get_mempolicy",
    "clock_getres_time64",
    "removexattr",
    "personality",
    "cacheflush",
    "statmount",
    "olduname",
    "msgrcv",
    "write",
    "umask",
    "init_module",
    "sigaction",
    "semtimedop_time64",
    "inotify_init",
    "sigaltstack",
    "mq_open",
    "epoll_wait",
    "atomic_barrier",
    "perf_event_open",
    "epoll_wait_old",
    "set_thread_area",
    "get_thread_area",
    "inotify_init1",
    "swapoff",
    "dup3",
    "process_vm_readv",
    "process_vm_writev",
    "exit_group",
    "adjtimex",
    "getegid32",
    "listmount",
    "s390_sthyi",
    "vfork",
    "umount",
    "munlockall",
    "clock_nanosleep",
    "afs_syscall",
    "epoll_pwait",
    "munlock",
    "fchownat",
    "recv",
    "openat2",
    "waitid",
    "arm_sync_file_range",
    "chown",
    "ssetmask",
    "sgetmask",
    "fdatasync",
    "multiplexer",
    "fremovexattr",
    "clock_adjtime64",
    "symlinkat",
    "spu_run",
    "set_robust_list",
    "get_robust_list",
    "fchown",
    "query_module",
    "waitpid",
    "futex_time64",
    "lremovexattr",
    "request_key",
    "quotactl_fd",
    "lchown",
    "fchmodat2",
    "uretprobe",
    "quotactl",
    "mlock2",
    "unlinkat",
    "swapcontext",
    "mmap2",
    "setgroups32",
    "getgroups32",
    "accept4",
    "mq_notify",
    "symlink",
    "security",
    "uselib",
    "signalfd4",
    "recvmmsg_time64",
    "eventfd2",
    "mq_timedsend_time64",
    "prlimit64",
    "geteuid32",
    "mq_timedreceive_time64",
    "futex_wait",
    "vhangup",
    "ftruncate64",
    "map_shadow_stack",
    "io_pgetevents_time64",
    "inotify_rm_watch",
    "unlink",
    "stat64",
    "setgid32",
    "getgid32",
    "statfs64",
    "swapon",
    "open_by_handle_at",
    "utimensat_time64",
    "preadv",
    "setuid32",
    "getuid32",
    "epoll_pwait2",
    "renameat2",
    "fcntl64",
    "clock_nanosleep_time64",
    "inotify_add_watch",
    "sys_debug_setcontext",
    "break",
    "add_key",
    "arm_fadvise64_64",
    "fstat64",
    "breakpoint",
    "fstatfs64",
    "riscv_hwprobe",
    "lstat64",
    "futex_wake",
    "bdflush",
    "usr32",
    "pread64",
    "pwritev",
    "fstatat64",
    "futex_waitv",
    "preadv2",
    "switch_endian",
    "shutdown",
    "writev",
    "umount2",
    "chown32",
    "pwrite64",
    "fadvise64",
    "fadvise64_64",
    "fchown32",
    "mq_unlink",
    "lchown32",
    "pwritev2",
    "wait4"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct arch_syscall_table wordlist[] =
  {
#line 489 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str0,456,315,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF},
#line 399 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str1,366,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF},
#line 491 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str2,458,13,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF},
#line 505 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str3,472,43,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF},
#line 187 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str4,154,112,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF},
#line 363 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str5,330,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF},
#line 338 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str6,305,3,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF},
#line 393 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str7,360,82,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF},
#line 431 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str8,398,66,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF},
#line 175 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str9,142,147,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF},
#line 418 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str10,385,346,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF},
#line 148 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str11,115,50,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF},
#line 406 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str12,373,139,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF},
#line 421 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str13,388,71,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF},
#line 423 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str14,390,170,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF},
#line 168 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str15,135,171,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF},
#line 494 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str16,461,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF},
#line 105 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str17,72,133,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF},
#line 503 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str18,470,260,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF},
#line 501 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str19,468,261,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF},
#line 133 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str20,100,118,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF},
#line 498 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str21,465,325,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF},
#line 496 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str22,463,326,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF},
#line 387 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str23,354,351,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF},
#line 379 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str24,346,352,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF},
#line 340 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str25,307,89,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF},
#line 389 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str26,356,156,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF},
#line 383 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str27,350,157,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF},
#line 495 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str28,462,322,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF},
#line 403 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str29,370,370,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,518,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF},
#line 404 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str30,371,369,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF},
#line 492 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str31,459,259,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,526,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF},
#line 308 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str32,275,42,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,__PNR_pipe,SCMP_KV_UNDEF,__PNR_pipe,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,__PNR_pipe,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF},
#line 208 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str33,175,117,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF},
#line 74 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str34,41,6,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF},
#line 326 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str35,293,44,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF},
#line 400 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str36,367,187,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF},
#line 202 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str37,169,289,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF},
#line 201 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str38,168,290,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF},
#line 76 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str39,43,362,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF},
#line 244 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str40,211,447,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF},
#line 388 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str41,355,154,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF},
#line 380 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str42,347,155,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF},
#line 456 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str43,423,359,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF},
#line 277 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str44,244,144,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF},
#line 72 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str45,39,120,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF},
#line 370 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str46,337,177,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,523,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF},
#line 243 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str47,210,356,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF},
#line 257 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str48,224,21,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF},
#line 305 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str49,272,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF},
#line 493 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str50,460,263,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF},
#line 246 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str51,213,218,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF},
#line 80 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str52,47,129,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF},
#line 344 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str53,311,88,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF},
#line 402 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str54,369,345,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,538,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF},
#line 362 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str55,329,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,__PNR_rseq_slice_yield,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF,471,SCMP_KV_UNDEF},
#line 35 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str56,2,33,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,__PNR_access,SCMP_KV_UNDEF,__PNR_access,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,__PNR_access,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF},
#line 384 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str57,351,161,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF},
#line 394 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str58,361,394,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF},
#line 56 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str59,23,185,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF},
#line 200 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str60,167,110,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF},
#line 360 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str61,327,40,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,__PNR_rmdir,SCMP_KV_UNDEF,__PNR_rmdir,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,__PNR_rmdir,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF},
#line 459 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str62,426,313,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF},
#line 483 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str63,450,149,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,__PNR__sysctl,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,__PNR__sysctl,SCMP_KV_UNDEF,__PNR__sysctl,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,__PNR__sysctl,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF},
#line 429 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str64,396,75,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,__PNR_setrlimit,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF},
#line 172 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str65,139,76,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,__PNR_getrlimit,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,__PNR_getrlimit,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF},
#line 258 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str66,225,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF},
#line 197 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str67,164,101,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF},
#line 77 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str68,44,377,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,391,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF},
#line 415 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str69,382,104,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF},
#line 156 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str70,123,105,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF},
#line 117 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str71,84,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF,469,SCMP_KV_UNDEF},
#line 116 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str72,83,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF,468,SCMP_KV_UNDEF},
#line 297 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str73,264,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF},
#line 322 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str74,289,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF},
#line 323 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str75,290,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF},
#line 118 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str76,85,350,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF},
#line 298 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str77,265,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF,467,SCMP_KV_UNDEF},
#line 273 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str78,240,402,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF},
#line 507 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str79,474,92,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF},
#line 299 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str80,266,29,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,__PNR_pause,SCMP_KV_UNDEF,__PNR_pause,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,__PNR_pause,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF},
#line 314 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str81,281,168,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,__PNR_poll,SCMP_KV_UNDEF,__PNR_poll,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,__PNR_poll,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF},
#line 287 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str82,254,34,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF},
#line 33 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str83,0,__PNR_accept,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_accept,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_accept,SCMP_KV_UNDEF,__PNR_accept,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF},
#line 469 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str84,436,25,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF},
#line 396 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str85,363,__PNR_semop,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF},
#line 134 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str86,101,35,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF},
#line 327 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str87,294,98,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF},
#line 261 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str88,228,125,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF},
#line 291 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str89,258,18,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF},
#line 146 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str90,113,141,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,__PNR_getdents,SCMP_KV_UNDEF,__PNR_getdents,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,__PNR_getdents,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF},
#line 288 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str91,255,28,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF},
#line 328 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str92,295,308,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF},
#line 84 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str93,51,254,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,__PNR_epoll_create,SCMP_KV_UNDEF,__PNR_epoll_create,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,__PNR_epoll_create,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF},
#line 391 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str94,358,354,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF},
#line 310 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str95,277,217,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF},
#line 450 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str96,417,321,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,__PNR_signalfd,SCMP_KV_UNDEF,__PNR_signalfd,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,__PNR_signalfd,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF},
#line 242 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str97,209,375,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,389,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF},
#line 490 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str98,457,270,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF},
#line 294 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str99,261,295,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF},
#line 221 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str100,188,303,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF},
#line 500 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str101,467,262,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF},
#line 85 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str102,52,329,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF},
#line 382 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str103,349,160,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF},
#line 106 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str104,73,94,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF},
#line 368 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str105,335,173,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,513,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF},
#line 98 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str106,65,307,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF},
#line 245 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str107,212,294,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF},
#line 276 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str108,243,400,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF},
#line 54 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str109,21,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF},
#line 449 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str110,416,48,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF},
#line 289 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str111,256,84,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF},
#line 40 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str112,7,27,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,__PNR_alarm,SCMP_KV_UNDEF,__PNR_alarm,SCMP_KV_UNDEF,__PNR_alarm,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,__PNR_alarm,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF},
#line 52 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str113,19,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF},
#line 381 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str114,348,159,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF},
#line 87 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str115,54,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF},
#line 463 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str116,430,106,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,__PNR_stat,SCMP_KV_UNDEF,__PNR_stat,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,__PNR_stat,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF},
#line 260 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str117,227,317,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,533,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF},
#line 123 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str118,90,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF},
#line 375 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str119,342,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF},
#line 465 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str120,432,99,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF},
#line 374 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str121,341,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF},
#line 41 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str122,8,384,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF},
#line 315 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str123,282,309,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF},
#line 179 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str124,146,224,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF},
#line 458 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str125,425,360,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF},
#line 365 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str126,332,176,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,522,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF},
#line 150 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str127,117,49,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF},
#line 293 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str128,260,5,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,__PNR_open,SCMP_KV_UNDEF,__PNR_open,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,__PNR_open,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF},
#line 361 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str129,328,386,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,387,SCMP_KV_UNDEF,387,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,387,SCMP_KV_UNDEF},
#line 408 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str130,375,138,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF},
#line 427 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str131,394,70,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF},
#line 163 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str132,130,20,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF},
#line 425 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str133,392,164,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF},
#line 170 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str134,137,165,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF},
#line 419 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str135,386,57,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF},
#line 161 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str136,128,132,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF},
#line 86 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str137,53,255,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF},
#line 262 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str138,229,56,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF},
#line 369 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str139,336,179,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF},
#line 436 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str140,403,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,983045,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF},
#line 181 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str141,148,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,983046,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF},
#line 102 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str142,69,324,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF},
#line 302 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str143,269,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF},
#line 300 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str144,267,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF},
#line 301 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str145,268,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF},
#line 81 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str146,48,41,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF},
#line 121 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str147,88,2,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,__PNR_fork,SCMP_KV_UNDEF,__PNR_fork,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,__PNR_fork,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF},
#line 457 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str148,424,102,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF},
#line 164 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str149,131,188,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF},
#line 307 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str150,274,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF},
#line 485 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str151,452,135,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,__PNR_sysfs,SCMP_KV_UNDEF,__PNR_sysfs,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,__PNR_sysfs,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF},
#line 364 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str152,331,174,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,512,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF},
#line 236 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str153,203,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF},
#line 234 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str154,201,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF},
#line 414 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str155,381,74,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF},
#line 64 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str156,31,266,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF},
#line 444 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str157,411,398,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF},
#line 70 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str158,37,264,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF},
#line 66 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str159,33,265,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF},
#line 107 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str160,74,306,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF},
#line 478 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str161,445,36,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF},
#line 481 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str162,448,344,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF},
#line 213 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str163,180,37,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF},
#line 395 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str164,362,393,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF},
#line 210 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str165,177,__PNR_kexec_file_load,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF},
#line 470 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str166,437,31,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF},
#line 186 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str167,153,32,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF},
#line 220 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str168,187,9,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,__PNR_link,SCMP_KV_UNDEF,__PNR_link,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,__PNR_link,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF},
#line 410 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str169,377,46,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF},
#line 152 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str170,119,47,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF},
#line 306 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str171,273,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF},
#line 165 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str172,132,64,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF},
#line 247 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str173,214,39,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,__PNR_mkdir,SCMP_KV_UNDEF,__PNR_mkdir,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,__PNR_mkdir,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF},
#line 312 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str174,279,382,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF},
#line 249 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str175,216,14,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,__PNR_mknod,SCMP_KV_UNDEF,__PNR_mknod,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,__PNR_mknod,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF},
#line 212 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str176,179,288,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF},
#line 36 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str177,3,51,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF},
#line 424 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str178,391,210,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF},
#line 169 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str179,136,211,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF},
#line 229 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str180,196,53,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF},
#line 73 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str181,40,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,__PNR_clone3,SCMP_KV_UNDEF},
#line 386 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str182,353,241,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF},
#line 378 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str183,345,242,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF},
#line 112 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str184,79,55,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF},
#line 390 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str185,357,158,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF},
#line 235 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str186,202,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF},
#line 434 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str187,401,258,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF},
#line 75 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str188,42,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF},
#line 366 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str189,333,175,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF},
#line 128 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str190,95,108,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF},
#line 405 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str191,372,121,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF},
#line 47 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str192,14,361,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF},
#line 174 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str193,141,77,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF},
#line 437 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str194,404,23,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF},
#line 182 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str195,149,24,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF},
#line 274 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str196,241,399,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF},
#line 432 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str197,399,366,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,541,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF},
#line 177 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str198,144,365,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,542,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF},
#line 131 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str199,98,100,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF},
#line 194 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str200,161,54,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,514,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF},
#line 119 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str201,86,234,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF},
#line 506 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str202,473,238,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF},
#line 78 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str203,45,8,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,__PNR_creat,SCMP_KV_UNDEF,__PNR_creat,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,__PNR_creat,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF},
#line 159 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str204,126,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF},
#line 487 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str205,454,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF},
#line 284 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str206,251,__PNR_newfstatat,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF},
#line 57 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str207,24,12,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF},
#line 58 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str208,25,15,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,__PNR_chmod,SCMP_KV_UNDEF,__PNR_chmod,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,__PNR_chmod,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF},
#line 92 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str209,59,323,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,__PNR_eventfd,SCMP_KV_UNDEF,__PNR_eventfd,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,__PNR_eventfd,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF},
#line 162 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str210,129,65,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,__PNR_getpgrp,SCMP_KV_UNDEF,__PNR_getpgrp,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,__PNR_getpgrp,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF},
#line 254 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str211,221,90,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,__PNR_mmap,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF},
#line 195 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str212,162,246,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF},
#line 397 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str213,364,__PNR_semtimedop,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF,392,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF,392,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF},
#line 479 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str214,446,314,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,__PNR_sync_file_range,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,__PNR_sync_file_range,SCMP_KV_UNDEF,__PNR_sync_file_range,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF},
#line 167 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str215,134,355,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF},
#line 203 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str216,170,245,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,543,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF},
#line 48 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str217,15,357,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF},
#line 237 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str218,204,107,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,__PNR_lstat,SCMP_KV_UNDEF,__PNR_lstat,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,__PNR_lstat,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF},
#line 61 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str219,28,61,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF},
#line 317 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str220,284,172,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF},
#line 526 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str221,493,30,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,__PNR_utime,SCMP_KV_UNDEF,__PNR_utime,SCMP_KV_UNDEF,__PNR_utime,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,__PNR_utime,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF},
#line 353 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str222,320,38,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,__PNR_rename,SCMP_KV_UNDEF,__PNR_rename,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,__PNR_rename,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF},
#line 532 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str223,499,166,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF},
#line 529 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str224,496,271,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,__PNR_utimes,SCMP_KV_UNDEF,__PNR_utimes,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,__PNR_utimes,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF},
#line 460 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str225,427,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF},
#line 227 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str226,194,233,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF},
#line 443 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str227,410,396,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF},
#line 417 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str228,384,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF},
#line 193 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str229,160,249,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF},
#line 55 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str230,22,184,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF},
#line 330 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str231,297,26,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,521,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF},
#line 241 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str232,208,274,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF},
#line 96 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str233,63,1,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF},
#line 357 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str234,324,0,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF},
#line 211 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str235,178,283,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,528,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF},
#line 248 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str236,215,296,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF},
#line 271 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str237,238,163,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF},
#line 523 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str238,490,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,983043,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF},
#line 285 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str239,252,142,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF},
#line 250 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str240,217,297,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF},
#line 439 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str241,406,226,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF},
#line 184 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str242,151,229,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF},
#line 504 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str243,471,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF},
#line 502 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str244,469,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF},
#line 143 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str245,110,299,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,__PNR_futimesat,SCMP_KV_UNDEF,__PNR_futimesat,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,__PNR_futimesat,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF},
#line 499 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str246,466,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF},
#line 497 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str247,464,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF},
#line 313 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str248,280,380,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF},
#line 230 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str249,197,253,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF},
#line 62 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str250,29,343,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF},
#line 79 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str251,46,127,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF},
#line 224 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str252,191,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,__PNR_listns,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF,470,SCMP_KV_UNDEF},
#line 511 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str253,478,58,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF},
#line 385 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str254,352,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF},
#line 407 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str255,374,216,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF},
#line 422 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str256,389,204,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF},
#line 533 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str257,500,113,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF},
#line 125 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str258,92,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF},
#line 214 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str259,181,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF},
#line 216 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str260,183,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF},
#line 215 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str261,182,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF},
#line 103 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str262,70,338,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF},
#line 455 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str263,422,72,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF},
#line 272 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str264,239,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF},
#line 256 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str265,223,123,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF},
#line 371 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str266,338,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF},
#line 350 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str267,317,257,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF},
#line 509 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str268,476,__PNR_tuxcall,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF},
#line 283 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str269,250,162,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF},
#line 145 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str270,112,183,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF},
#line 232 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str271,199,19,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF},
#line 120 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str272,87,143,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF},
#line 127 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str273,94,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF},
#line 401 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str274,368,239,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF},
#line 228 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str275,195,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF},
#line 354 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str276,321,302,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,__PNR_renameat,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,__PNR_renameat,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF},
#line 373 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str277,340,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF},
#line 518 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str278,485,310,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF},
#line 99 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str279,66,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF},
#line 440 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str280,407,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF},
#line 185 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str281,152,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF},
#line 209 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str282,176,349,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF},
#line 339 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str283,306,225,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF},
#line 94 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str284,61,11,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,520,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF},
#line 488 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str285,455,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF},
#line 176 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str286,143,367,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF},
#line 349 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str287,316,372,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,519,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF},
#line 144 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str288,111,318,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF},
#line 311 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str289,278,381,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF},
#line 160 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str290,127,368,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF},
#line 251 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str291,218,150,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF},
#line 372 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str292,339,335,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,536,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF},
#line 367 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str293,334,178,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,524,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF},
#line 259 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str294,226,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF},
#line 124 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str295,91,228,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF},
#line 115 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str296,82,231,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF},
#line 482 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str297,449,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF},
#line 329 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str298,296,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF},
#line 342 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str299,309,305,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF},
#line 515 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str300,482,122,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF},
#line 486 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str301,453,116,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF},
#line 205 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str302,172,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF},
#line 51 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str303,18,45,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF},
#line 206 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str304,173,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF},
#line 426 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str305,393,208,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF},
#line 171 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str306,138,209,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF},
#line 253 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str307,220,152,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF},
#line 126 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str308,93,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF},
#line 347 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str309,314,337,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,537,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF},
#line 525 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str310,492,62,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,__PNR_ustat,SCMP_KV_UNDEF,__PNR_ustat,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,__PNR_ustat,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF},
#line 508 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str311,475,193,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF},
#line 286 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str312,253,169,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,__PNR_nfsservctl,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,__PNR_nfsservctl,SCMP_KV_UNDEF,__PNR_nfsservctl,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF},
#line 331 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str313,298,189,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF},
#line 376 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str314,343,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF},
#line 454 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str315,421,119,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF},
#line 510 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str316,477,191,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF},
#line 268 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str317,235,279,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF},
#line 226 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str318,193,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF},
#line 233 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str319,200,227,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF},
#line 219 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str320,186,230,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF},
#line 452 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str321,419,73,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF},
#line 225 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str322,192,232,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF},
#line 266 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str323,233,280,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF},
#line 468 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str324,435,383,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF},
#line 137 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str325,104,240,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF},
#line 282 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str326,249,341,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF},
#line 207 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str327,174,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF},
#line 346 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str328,313,371,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,517,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF},
#line 147 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str329,114,220,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF},
#line 445 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str330,412,395,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF},
#line 420 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str331,387,97,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF},
#line 166 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str332,133,96,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF},
#line 222 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str333,189,363,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF},
#line 135 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str334,102,93,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF},
#line 239 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str335,206,219,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF},
#line 263 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str336,230,282,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF},
#line 435 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str337,402,79,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF},
#line 180 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str338,147,78,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF},
#line 198 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str339,165,385,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,__PNR_io_pgetevents,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,388,SCMP_KV_UNDEF,388,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,__PNR_io_pgetevents,SCMP_KV_UNDEF},
#line 95 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str340,62,358,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,545,SCMP_KV_UNDEF,387,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF},
#line 522 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str341,489,374,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,388,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF},
#line 157 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str342,124,130,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF},
#line 412 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str343,379,81,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF},
#line 154 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str344,121,80,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF},
#line 281 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str345,248,91,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF},
#line 45 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str346,12,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF},
#line 442 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str347,409,397,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF},
#line 471 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str348,438,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF},
#line 358 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str349,325,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF},
#line 316 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str350,283,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF},
#line 453 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str351,420,126,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF},
#line 82 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str352,49,63,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,__PNR_dup2,SCMP_KV_UNDEF,__PNR_dup2,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,__PNR_dup2,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF},
#line 204 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str353,171,248,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,544,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF},
#line 309 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str354,276,331,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF},
#line 527 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str355,494,320,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF},
#line 343 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str356,310,145,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,515,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF},
#line 341 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str357,308,85,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,__PNR_readlink,SCMP_KV_UNDEF,__PNR_readlink,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,__PNR_readlink,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF},
#line 196 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str358,163,247,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF},
#line 480 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str359,447,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,388,SCMP_KV_UNDEF},
#line 290 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str360,257,59,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF},
#line 535 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str361,502,273,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF},
#line 409 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str362,376,215,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF},
#line 428 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str363,395,203,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF},
#line 519 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str364,486,__PNR_uprobe,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF,__PNR_uprobe,SCMP_KV_UNDEF},
#line 534 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str365,501,316,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,532,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF},
#line 104 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str366,71,339,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF},
#line 138 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str367,105,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF},
#line 352 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str368,319,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF},
#line 71 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str369,38,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF},
#line 67 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str370,34,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF},
#line 416 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str371,383,276,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF},
#line 158 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str372,125,275,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF},
#line 65 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str373,32,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF},
#line 351 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str374,318,235,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF},
#line 304 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str375,271,136,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF},
#line 53 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str376,20,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,983042,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF},
#line 467 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str377,434,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF},
#line 292 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str378,259,109,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF},
#line 275 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str379,242,401,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF},
#line 539 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str380,506,4,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF},
#line 512 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str381,479,60,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF},
#line 188 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str382,155,128,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF},
#line 447 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str383,414,67,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF},
#line 398 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str384,365,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF},
#line 190 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str385,157,291,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,__PNR_inotify_init,SCMP_KV_UNDEF,__PNR_inotify_init,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,__PNR_inotify_init,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF},
#line 448 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str386,415,186,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,525,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF},
#line 265 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str387,232,277,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF},
#line 90 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str388,57,256,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,__PNR_epoll_wait,SCMP_KV_UNDEF,__PNR_epoll_wait,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,__PNR_epoll_wait,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF},
#line 44 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str389,11,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF},
#line 303 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str390,270,336,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF},
#line 91 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str391,58,__PNR_epoll_wait_old,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF},
#line 433 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str392,400,243,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF},
#line 178 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str393,145,244,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF},
#line 191 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str394,158,332,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF},
#line 473 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str395,440,115,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF},
#line 83 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str396,50,330,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF},
#line 324 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str397,291,347,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,539,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF},
#line 325 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str398,292,348,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,540,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF},
#line 97 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str399,64,252,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF},
#line 38 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str400,5,124,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF},
#line 149 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str401,116,202,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF},
#line 223 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str402,190,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF},
#line 377 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str403,344,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF},
#line 530 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str404,497,190,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF},
#line 513 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str405,480,22,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF},
#line 280 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str406,247,153,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF},
#line 68 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str407,35,267,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF},
#line 39 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str408,6,137,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF},
#line 88 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str409,55,319,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF},
#line 279 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str410,246,151,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF},
#line 111 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str411,78,298,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF},
#line 345 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str412,312,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF},
#line 295 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str413,262,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF},
#line 537 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str414,504,284,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,529,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF},
#line 43 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str415,10,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF},
#line 59 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str416,26,182,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,__PNR_chown,SCMP_KV_UNDEF,__PNR_chown,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,__PNR_chown,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF},
#line 462 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str417,429,69,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF},
#line 441 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str418,408,68,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF},
#line 114 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str419,81,148,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF},
#line 278 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str420,245,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF},
#line 122 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str421,89,237,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF},
#line 63 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str422,30,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF},
#line 477 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str423,444,304,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF},
#line 461 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str424,428,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF},
#line 430 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str425,397,311,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,530,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF},
#line 173 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str426,140,312,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,531,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF},
#line 109 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str427,76,95,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF},
#line 335 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str428,302,167,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF},
#line 538 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str429,505,7,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF},
#line 139 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str430,106,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF},
#line 231 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str431,198,236,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF},
#line 356 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str432,323,287,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF},
#line 337 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str433,304,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF},
#line 217 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str434,184,16,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,__PNR_lchown,SCMP_KV_UNDEF,__PNR_lchown,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,__PNR_lchown,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF},
#line 108 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str435,75,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF},
#line 520 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str436,487,__PNR_uretprobe,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF},
#line 336 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str437,303,131,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF},
#line 252 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str438,219,376,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,390,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF},
#line 517 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str439,484,301,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF},
#line 472 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str440,439,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF},
#line 255 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str441,222,192,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF},
#line 413 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str442,380,206,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF},
#line 155 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str443,122,205,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF},
#line 34 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str444,1,364,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF},
#line 264 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str445,231,281,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,527,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF},
#line 476 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str446,443,83,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,__PNR_symlink,SCMP_KV_UNDEF,__PNR_symlink,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,__PNR_symlink,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF},
#line 392 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str447,359,__PNR_security,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF},
#line 521 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str448,488,86,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF},
#line 451 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str449,418,327,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF},
#line 348 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str450,315,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF},
#line 93 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str451,60,328,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF},
#line 269 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str452,236,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF},
#line 321 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str453,288,340,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF},
#line 151 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str454,118,201,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF},
#line 267 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str455,234,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF},
#line 140 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str456,107,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF},
#line 531 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str457,498,111,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF},
#line 136 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str458,103,194,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF},
#line 240 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str459,207,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF},
#line 199 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str460,166,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF},
#line 192 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str461,159,293,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF},
#line 516 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str462,483,10,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,__PNR_unlink,SCMP_KV_UNDEF,__PNR_unlink,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,__PNR_unlink,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF},
#line 464 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str463,431,195,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF},
#line 411 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str464,378,214,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF},
#line 153 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str465,120,200,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF},
#line 466 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str466,433,268,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF},
#line 474 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str467,441,87,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF},
#line 296 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str468,263,342,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF},
#line 528 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str469,495,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF},
#line 319 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str470,286,333,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,534,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF},
#line 438 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str471,405,213,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF},
#line 183 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str472,150,199,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF},
#line 89 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str473,56,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF},
#line 355 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str474,322,353,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF},
#line 113 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str475,80,221,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF},
#line 69 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str476,36,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF},
#line 189 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str477,156,292,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF},
#line 484 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str478,451,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF},
#line 49 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str479,16,17,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF},
#line 37 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str480,4,286,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF},
#line 42 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str481,9,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF},
#line 129 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str482,96,197,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF},
#line 50 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str483,17,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,983041,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF},
#line 132 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str484,99,269,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF},
#line 359 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str485,326,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF},
#line 238 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str486,205,196,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF},
#line 142 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str487,109,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF},
#line 46 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str488,13,134,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF},
#line 524 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str489,491,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,983044,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF},
#line 318 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str490,285,180,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF},
#line 333 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str491,300,334,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,535,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF},
#line 130 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str492,97,300,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF},
#line 141 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str493,108,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF},
#line 320 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str494,287,378,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,546,SCMP_KV_UNDEF,392,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF},
#line 475 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str495,442,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF},
#line 446 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str496,413,373,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF},
#line 540 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str497,507,146,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,516,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF},
#line 514 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str498,481,52,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF},
#line 60 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str499,27,212,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF},
#line 332 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str500,299,181,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF},
#line 100 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str501,67,250,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,__PNR_fadvise64,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,__PNR_fadvise64,SCMP_KV_UNDEF,__PNR_fadvise64,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF},
#line 101 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str502,68,272,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF},
#line 110 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str503,77,207,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF},
#line 270 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str504,237,278,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF},
#line 218 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str505,185,198,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF},
#line 334 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str506,301,379,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,547,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF},
#line 536 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str507,503,114,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF}
  };

const struct arch_syscall_table *
in_word_set (register const char *str, register size_t len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const struct arch_syscall_table *resword;

          switch (key - 21)
            {
              case 0:
                resword = &wordlist[0];
                goto compare;
              case 9:
                resword = &wordlist[1];
                goto compare;
              case 10:
                resword = &wordlist[2];
                goto compare;
              case 13:
                resword = &wordlist[3];
                goto compare;
              case 14:
                resword = &wordlist[4];
                goto compare;
              case 15:
                resword = &wordlist[5];
                goto compare;
              case 21:
                resword = &wordlist[6];
                goto compare;
              case 28:
                resword = &wordlist[7];
                goto compare;
              case 30:
                resword = &wordlist[8];
                goto compare;
              case 31:
                resword = &wordlist[9];
                goto compare;
              case 35:
                resword = &wordlist[10];
                goto compare;
              case 37:
                resword = &wordlist[11];
                goto compare;
              case 40:
                resword = &wordlist[12];
                goto compare;
              case 41:
                resword = &wordlist[13];
                goto compare;
              case 46:
                resword = &wordlist[14];
                goto compare;
              case 47:
                resword = &wordlist[15];
                goto compare;
              case 49:
                resword = &wordlist[16];
                goto compare;
              case 53:
                resword = &wordlist[17];
                goto compare;
              case 55:
                resword = &wordlist[18];
                goto compare;
              case 56:
                resword = &wordlist[19];
                goto compare;
              case 57:
                resword = &wordlist[20];
                goto compare;
              case 61:
                resword = &wordlist[21];
                goto compare;
              case 62:
                resword = &wordlist[22];
                goto compare;
              case 63:
                resword = &wordlist[23];
                goto compare;
              case 64:
                resword = &wordlist[24];
                goto compare;
              case 65:
                resword = &wordlist[25];
                goto compare;
              case 68:
                resword = &wordlist[26];
                goto compare;
              case 69:
                resword = &wordlist[27];
                goto compare;
              case 71:
                resword = &wordlist[28];
                goto compare;
              case 72:
                resword = &wordlist[29];
                goto compare;
              case 73:
                resword = &wordlist[30];
                goto compare;
              case 75:
                resword = &wordlist[31];
                goto compare;
              case 78:
                resword = &wordlist[32];
                goto compare;
              case 80:
                resword = &wordlist[33];
                goto compare;
              case 83:
                resword = &wordlist[34];
                goto compare;
              case 84:
                resword = &wordlist[35];
                goto compare;
              case 96:
                resword = &wordlist[36];
                goto compare;
              case 97:
                resword = &wordlist[37];
                goto compare;
              case 98:
                resword = &wordlist[38];
                goto compare;
              case 100:
                resword = &wordlist[39];
                goto compare;
              case 103:
                resword = &wordlist[40];
                goto compare;
              case 104:
                resword = &wordlist[41];
                goto compare;
              case 105:
                resword = &wordlist[42];
                goto compare;
              case 106:
                resword = &wordlist[43];
                goto compare;
              case 107:
                resword = &wordlist[44];
                goto compare;
              case 110:
                resword = &wordlist[45];
                goto compare;
              case 113:
                resword = &wordlist[46];
                goto compare;
              case 115:
                resword = &wordlist[47];
                goto compare;
              case 118:
                resword = &wordlist[48];
                goto compare;
              case 119:
                resword = &wordlist[49];
                goto compare;
              case 120:
                resword = &wordlist[50];
                goto compare;
              case 122:
                resword = &wordlist[51];
                goto compare;
              case 125:
                resword = &wordlist[52];
                goto compare;
              case 126:
                resword = &wordlist[53];
                goto compare;
              case 129:
                resword = &wordlist[54];
                goto compare;
              case 139:
                resword = &wordlist[55];
                goto compare;
              case 140:
                resword = &wordlist[56];
                goto compare;
              case 141:
                resword = &wordlist[57];
                goto compare;
              case 143:
                resword = &wordlist[58];
                goto compare;
              case 146:
                resword = &wordlist[59];
                goto compare;
              case 147:
                resword = &wordlist[60];
                goto compare;
              case 148:
                resword = &wordlist[61];
                goto compare;
              case 150:
                resword = &wordlist[62];
                goto compare;
              case 153:
                resword = &wordlist[63];
                goto compare;
              case 154:
                resword = &wordlist[64];
                goto compare;
              case 155:
                resword = &wordlist[65];
                goto compare;
              case 156:
                resword = &wordlist[66];
                goto compare;
              case 161:
                resword = &wordlist[67];
                goto compare;
              case 166:
                resword = &wordlist[68];
                goto compare;
              case 167:
                resword = &wordlist[69];
                goto compare;
              case 168:
                resword = &wordlist[70];
                goto compare;
              case 170:
                resword = &wordlist[71];
                goto compare;
              case 171:
                resword = &wordlist[72];
                goto compare;
              case 172:
                resword = &wordlist[73];
                goto compare;
              case 174:
                resword = &wordlist[74];
                goto compare;
              case 175:
                resword = &wordlist[75];
                goto compare;
              case 181:
                resword = &wordlist[76];
                goto compare;
              case 187:
                resword = &wordlist[77];
                goto compare;
              case 193:
                resword = &wordlist[78];
                goto compare;
              case 197:
                resword = &wordlist[79];
                goto compare;
              case 198:
                resword = &wordlist[80];
                goto compare;
              case 212:
                resword = &wordlist[81];
                goto compare;
              case 213:
                resword = &wordlist[82];
                goto compare;
              case 214:
                resword = &wordlist[83];
                goto compare;
              case 215:
                resword = &wordlist[84];
                goto compare;
              case 216:
                resword = &wordlist[85];
                goto compare;
              case 217:
                resword = &wordlist[86];
                goto compare;
              case 221:
                resword = &wordlist[87];
                goto compare;
              case 230:
                resword = &wordlist[88];
                goto compare;
              case 231:
                resword = &wordlist[89];
                goto compare;
              case 233:
                resword = &wordlist[90];
                goto compare;
              case 235:
                resword = &wordlist[91];
                goto compare;
              case 238:
                resword = &wordlist[92];
                goto compare;
              case 240:
                resword = &wordlist[93];
                goto compare;
              case 243:
                resword = &wordlist[94];
                goto compare;
              case 244:
                resword = &wordlist[95];
                goto compare;
              case 247:
                resword = &wordlist[96];
                goto compare;
              case 251:
                resword = &wordlist[97];
                goto compare;
              case 254:
                resword = &wordlist[98];
                goto compare;
              case 259:
                resword = &wordlist[99];
                goto compare;
              case 260:
                resword = &wordlist[100];
                goto compare;
              case 262:
                resword = &wordlist[101];
                goto compare;
              case 264:
                resword = &wordlist[102];
                goto compare;
              case 266:
                resword = &wordlist[103];
                goto compare;
              case 268:
                resword = &wordlist[104];
                goto compare;
              case 270:
                resword = &wordlist[105];
                goto compare;
              case 274:
                resword = &wordlist[106];
                goto compare;
              case 277:
                resword = &wordlist[107];
                goto compare;
              case 280:
                resword = &wordlist[108];
                goto compare;
              case 284:
                resword = &wordlist[109];
                goto compare;
              case 285:
                resword = &wordlist[110];
                goto compare;
              case 286:
                resword = &wordlist[111];
                goto compare;
              case 288:
                resword = &wordlist[112];
                goto compare;
              case 292:
                resword = &wordlist[113];
                goto compare;
              case 294:
                resword = &wordlist[114];
                goto compare;
              case 295:
                resword = &wordlist[115];
                goto compare;
              case 298:
                resword = &wordlist[116];
                goto compare;
              case 299:
                resword = &wordlist[117];
                goto compare;
              case 305:
                resword = &wordlist[118];
                goto compare;
              case 307:
                resword = &wordlist[119];
                goto compare;
              case 308:
                resword = &wordlist[120];
                goto compare;
              case 310:
                resword = &wordlist[121];
                goto compare;
              case 317:
                resword = &wordlist[122];
                goto compare;
              case 320:
                resword = &wordlist[123];
                goto compare;
              case 324:
                resword = &wordlist[124];
                goto compare;
              case 325:
                resword = &wordlist[125];
                goto compare;
              case 328:
                resword = &wordlist[126];
                goto compare;
              case 331:
                resword = &wordlist[127];
                goto compare;
              case 332:
                resword = &wordlist[128];
                goto compare;
              case 333:
                resword = &wordlist[129];
                goto compare;
              case 334:
                resword = &wordlist[130];
                goto compare;
              case 335:
                resword = &wordlist[131];
                goto compare;
              case 339:
                resword = &wordlist[132];
                goto compare;
              case 340:
                resword = &wordlist[133];
                goto compare;
              case 341:
                resword = &wordlist[134];
                goto compare;
              case 344:
                resword = &wordlist[135];
                goto compare;
              case 345:
                resword = &wordlist[136];
                goto compare;
              case 347:
                resword = &wordlist[137];
                goto compare;
              case 349:
                resword = &wordlist[138];
                goto compare;
              case 351:
                resword = &wordlist[139];
                goto compare;
              case 356:
                resword = &wordlist[140];
                goto compare;
              case 357:
                resword = &wordlist[141];
                goto compare;
              case 360:
                resword = &wordlist[142];
                goto compare;
              case 361:
                resword = &wordlist[143];
                goto compare;
              case 362:
                resword = &wordlist[144];
                goto compare;
              case 364:
                resword = &wordlist[145];
                goto compare;
              case 365:
                resword = &wordlist[146];
                goto compare;
              case 371:
                resword = &wordlist[147];
                goto compare;
              case 373:
                resword = &wordlist[148];
                goto compare;
              case 381:
                resword = &wordlist[149];
                goto compare;
              case 383:
                resword = &wordlist[150];
                goto compare;
              case 384:
                resword = &wordlist[151];
                goto compare;
              case 385:
                resword = &wordlist[152];
                goto compare;
              case 386:
                resword = &wordlist[153];
                goto compare;
              case 387:
                resword = &wordlist[154];
                goto compare;
              case 389:
                resword = &wordlist[155];
                goto compare;
              case 390:
                resword = &wordlist[156];
                goto compare;
              case 391:
                resword = &wordlist[157];
                goto compare;
              case 392:
                resword = &wordlist[158];
                goto compare;
              case 393:
                resword = &wordlist[159];
                goto compare;
              case 394:
                resword = &wordlist[160];
                goto compare;
              case 395:
                resword = &wordlist[161];
                goto compare;
              case 396:
                resword = &wordlist[162];
                goto compare;
              case 397:
                resword = &wordlist[163];
                goto compare;
              case 398:
                resword = &wordlist[164];
                goto compare;
              case 399:
                resword = &wordlist[165];
                goto compare;
              case 403:
                resword = &wordlist[166];
                goto compare;
              case 404:
                resword = &wordlist[167];
                goto compare;
              case 407:
                resword = &wordlist[168];
                goto compare;
              case 409:
                resword = &wordlist[169];
                goto compare;
              case 410:
                resword = &wordlist[170];
                goto compare;
              case 413:
                resword = &wordlist[171];
                goto compare;
              case 414:
                resword = &wordlist[172];
                goto compare;
              case 415:
                resword = &wordlist[173];
                goto compare;
              case 416:
                resword = &wordlist[174];
                goto compare;
              case 417:
                resword = &wordlist[175];
                goto compare;
              case 422:
                resword = &wordlist[176];
                goto compare;
              case 425:
                resword = &wordlist[177];
                goto compare;
              case 426:
                resword = &wordlist[178];
                goto compare;
              case 427:
                resword = &wordlist[179];
                goto compare;
              case 431:
                resword = &wordlist[180];
                goto compare;
              case 433:
                resword = &wordlist[181];
                goto compare;
              case 434:
                resword = &wordlist[182];
                goto compare;
              case 435:
                resword = &wordlist[183];
                goto compare;
              case 436:
                resword = &wordlist[184];
                goto compare;
              case 437:
                resword = &wordlist[185];
                goto compare;
              case 438:
                resword = &wordlist[186];
                goto compare;
              case 439:
                resword = &wordlist[187];
                goto compare;
              case 440:
                resword = &wordlist[188];
                goto compare;
              case 441:
                resword = &wordlist[189];
                goto compare;
              case 442:
                resword = &wordlist[190];
                goto compare;
              case 443:
                resword = &wordlist[191];
                goto compare;
              case 444:
                resword = &wordlist[192];
                goto compare;
              case 445:
                resword = &wordlist[193];
                goto compare;
              case 446:
                resword = &wordlist[194];
                goto compare;
              case 447:
                resword = &wordlist[195];
                goto compare;
              case 448:
                resword = &wordlist[196];
                goto compare;
              case 449:
                resword = &wordlist[197];
                goto compare;
              case 450:
                resword = &wordlist[198];
                goto compare;
              case 452:
                resword = &wordlist[199];
                goto compare;
              case 457:
                resword = &wordlist[200];
                goto compare;
              case 460:
                resword = &wordlist[201];
                goto compare;
              case 461:
                resword = &wordlist[202];
                goto compare;
              case 463:
                resword = &wordlist[203];
                goto compare;
              case 467:
                resword = &wordlist[204];
                goto compare;
              case 471:
                resword = &wordlist[205];
                goto compare;
              case 475:
                resword = &wordlist[206];
                goto compare;
              case 476:
                resword = &wordlist[207];
                goto compare;
              case 478:
                resword = &wordlist[208];
                goto compare;
              case 479:
                resword = &wordlist[209];
                goto compare;
              case 480:
                resword = &wordlist[210];
                goto compare;
              case 483:
                resword = &wordlist[211];
                goto compare;
              case 485:
                resword = &wordlist[212];
                goto compare;
              case 486:
                resword = &wordlist[213];
                goto compare;
              case 488:
                resword = &wordlist[214];
                goto compare;
              case 489:
                resword = &wordlist[215];
                goto compare;
              case 494:
                resword = &wordlist[216];
                goto compare;
              case 498:
                resword = &wordlist[217];
                goto compare;
              case 502:
                resword = &wordlist[218];
                goto compare;
              case 504:
                resword = &wordlist[219];
                goto compare;
              case 505:
                resword = &wordlist[220];
                goto compare;
              case 510:
                resword = &wordlist[221];
                goto compare;
              case 511:
                resword = &wordlist[222];
                goto compare;
              case 512:
                resword = &wordlist[223];
                goto compare;
              case 513:
                resword = &wordlist[224];
                goto compare;
              case 518:
                resword = &wordlist[225];
                goto compare;
              case 520:
                resword = &wordlist[226];
                goto compare;
              case 522:
                resword = &wordlist[227];
                goto compare;
              case 523:
                resword = &wordlist[228];
                goto compare;
              case 524:
                resword = &wordlist[229];
                goto compare;
              case 525:
                resword = &wordlist[230];
                goto compare;
              case 528:
                resword = &wordlist[231];
                goto compare;
              case 529:
                resword = &wordlist[232];
                goto compare;
              case 531:
                resword = &wordlist[233];
                goto compare;
              case 533:
                resword = &wordlist[234];
                goto compare;
              case 534:
                resword = &wordlist[235];
                goto compare;
              case 535:
                resword = &wordlist[236];
                goto compare;
              case 536:
                resword = &wordlist[237];
                goto compare;
              case 541:
                resword = &wordlist[238];
                goto compare;
              case 542:
                resword = &wordlist[239];
                goto compare;
              case 543:
                resword = &wordlist[240];
                goto compare;
              case 546:
                resword = &wordlist[241];
                goto compare;
              case 547:
                resword = &wordlist[242];
                goto compare;
              case 551:
                resword = &wordlist[243];
                goto compare;
              case 552:
                resword = &wordlist[244];
                goto compare;
              case 553:
                resword = &wordlist[245];
                goto compare;
              case 557:
                resword = &wordlist[246];
                goto compare;
              case 558:
                resword = &wordlist[247];
                goto compare;
              case 565:
                resword = &wordlist[248];
                goto compare;
              case 566:
                resword = &wordlist[249];
                goto compare;
              case 568:
                resword = &wordlist[250];
                goto compare;
              case 570:
                resword = &wordlist[251];
                goto compare;
              case 576:
                resword = &wordlist[252];
                goto compare;
              case 580:
                resword = &wordlist[253];
                goto compare;
              case 582:
                resword = &wordlist[254];
                goto compare;
              case 584:
                resword = &wordlist[255];
                goto compare;
              case 585:
                resword = &wordlist[256];
                goto compare;
              case 589:
                resword = &wordlist[257];
                goto compare;
              case 590:
                resword = &wordlist[258];
                goto compare;
              case 597:
                resword = &wordlist[259];
                goto compare;
              case 602:
                resword = &wordlist[260];
                goto compare;
              case 603:
                resword = &wordlist[261];
                goto compare;
              case 608:
                resword = &wordlist[262];
                goto compare;
              case 610:
                resword = &wordlist[263];
                goto compare;
              case 612:
                resword = &wordlist[264];
                goto compare;
              case 613:
                resword = &wordlist[265];
                goto compare;
              case 614:
                resword = &wordlist[266];
                goto compare;
              case 616:
                resword = &wordlist[267];
                goto compare;
              case 621:
                resword = &wordlist[268];
                goto compare;
              case 623:
                resword = &wordlist[269];
                goto compare;
              case 625:
                resword = &wordlist[270];
                goto compare;
              case 626:
                resword = &wordlist[271];
                goto compare;
              case 632:
                resword = &wordlist[272];
                goto compare;
              case 636:
                resword = &wordlist[273];
                goto compare;
              case 637:
                resword = &wordlist[274];
                goto compare;
              case 640:
                resword = &wordlist[275];
                goto compare;
              case 641:
                resword = &wordlist[276];
                goto compare;
              case 648:
                resword = &wordlist[277];
                goto compare;
              case 653:
                resword = &wordlist[278];
                goto compare;
              case 657:
                resword = &wordlist[279];
                goto compare;
              case 660:
                resword = &wordlist[280];
                goto compare;
              case 661:
                resword = &wordlist[281];
                goto compare;
              case 669:
                resword = &wordlist[282];
                goto compare;
              case 671:
                resword = &wordlist[283];
                goto compare;
              case 672:
                resword = &wordlist[284];
                goto compare;
              case 673:
                resword = &wordlist[285];
                goto compare;
              case 675:
                resword = &wordlist[286];
                goto compare;
              case 677:
                resword = &wordlist[287];
                goto compare;
              case 678:
                resword = &wordlist[288];
                goto compare;
              case 679:
                resword = &wordlist[289];
                goto compare;
              case 680:
                resword = &wordlist[290];
                goto compare;
              case 682:
                resword = &wordlist[291];
                goto compare;
              case 687:
                resword = &wordlist[292];
                goto compare;
              case 688:
                resword = &wordlist[293];
                goto compare;
              case 691:
                resword = &wordlist[294];
                goto compare;
              case 700:
                resword = &wordlist[295];
                goto compare;
              case 701:
                resword = &wordlist[296];
                goto compare;
              case 703:
                resword = &wordlist[297];
                goto compare;
              case 704:
                resword = &wordlist[298];
                goto compare;
              case 712:
                resword = &wordlist[299];
                goto compare;
              case 713:
                resword = &wordlist[300];
                goto compare;
              case 714:
                resword = &wordlist[301];
                goto compare;
              case 716:
                resword = &wordlist[302];
                goto compare;
              case 717:
                resword = &wordlist[303];
                goto compare;
              case 719:
                resword = &wordlist[304];
                goto compare;
              case 720:
                resword = &wordlist[305];
                goto compare;
              case 721:
                resword = &wordlist[306];
                goto compare;
              case 722:
                resword = &wordlist[307];
                goto compare;
              case 733:
                resword = &wordlist[308];
                goto compare;
              case 734:
                resword = &wordlist[309];
                goto compare;
              case 735:
                resword = &wordlist[310];
                goto compare;
              case 738:
                resword = &wordlist[311];
                goto compare;
              case 741:
                resword = &wordlist[312];
                goto compare;
              case 743:
                resword = &wordlist[313];
                goto compare;
              case 744:
                resword = &wordlist[314];
                goto compare;
              case 750:
                resword = &wordlist[315];
                goto compare;
              case 754:
                resword = &wordlist[316];
                goto compare;
              case 755:
                resword = &wordlist[317];
                goto compare;
              case 757:
                resword = &wordlist[318];
                goto compare;
              case 760:
                resword = &wordlist[319];
                goto compare;
              case 761:
                resword = &wordlist[320];
                goto compare;
              case 764:
                resword = &wordlist[321];
                goto compare;
              case 765:
                resword = &wordlist[322];
                goto compare;
              case 766:
                resword = &wordlist[323];
                goto compare;
              case 767:
                resword = &wordlist[324];
                goto compare;
              case 769:
                resword = &wordlist[325];
                goto compare;
              case 771:
                resword = &wordlist[326];
                goto compare;
              case 774:
                resword = &wordlist[327];
                goto compare;
              case 775:
                resword = &wordlist[328];
                goto compare;
              case 776:
                resword = &wordlist[329];
                goto compare;
              case 777:
                resword = &wordlist[330];
                goto compare;
              case 780:
                resword = &wordlist[331];
                goto compare;
              case 781:
                resword = &wordlist[332];
                goto compare;
              case 783:
                resword = &wordlist[333];
                goto compare;
              case 786:
                resword = &wordlist[334];
                goto compare;
              case 788:
                resword = &wordlist[335];
                goto compare;
              case 791:
                resword = &wordlist[336];
                goto compare;
              case 793:
                resword = &wordlist[337];
                goto compare;
              case 794:
                resword = &wordlist[338];
                goto compare;
              case 797:
                resword = &wordlist[339];
                goto compare;
              case 802:
                resword = &wordlist[340];
                goto compare;
              case 806:
                resword = &wordlist[341];
                goto compare;
              case 809:
                resword = &wordlist[342];
                goto compare;
              case 813:
                resword = &wordlist[343];
                goto compare;
              case 814:
                resword = &wordlist[344];
                goto compare;
              case 819:
                resword = &wordlist[345];
                goto compare;
              case 820:
                resword = &wordlist[346];
                goto compare;
              case 821:
                resword = &wordlist[347];
                goto compare;
              case 825:
                resword = &wordlist[348];
                goto compare;
              case 835:
                resword = &wordlist[349];
                goto compare;
              case 836:
                resword = &wordlist[350];
                goto compare;
              case 842:
                resword = &wordlist[351];
                goto compare;
              case 844:
                resword = &wordlist[352];
                goto compare;
              case 848:
                resword = &wordlist[353];
                goto compare;
              case 849:
                resword = &wordlist[354];
                goto compare;
              case 855:
                resword = &wordlist[355];
                goto compare;
              case 856:
                resword = &wordlist[356];
                goto compare;
              case 865:
                resword = &wordlist[357];
                goto compare;
              case 868:
                resword = &wordlist[358];
                goto compare;
              case 871:
                resword = &wordlist[359];
                goto compare;
              case 875:
                resword = &wordlist[360];
                goto compare;
              case 876:
                resword = &wordlist[361];
                goto compare;
              case 878:
                resword = &wordlist[362];
                goto compare;
              case 879:
                resword = &wordlist[363];
                goto compare;
              case 880:
                resword = &wordlist[364];
                goto compare;
              case 881:
                resword = &wordlist[365];
                goto compare;
              case 885:
                resword = &wordlist[366];
                goto compare;
              case 886:
                resword = &wordlist[367];
                goto compare;
              case 887:
                resword = &wordlist[368];
                goto compare;
              case 888:
                resword = &wordlist[369];
                goto compare;
              case 889:
                resword = &wordlist[370];
                goto compare;
              case 890:
                resword = &wordlist[371];
                goto compare;
              case 891:
                resword = &wordlist[372];
                goto compare;
              case 893:
                resword = &wordlist[373];
                goto compare;
              case 895:
                resword = &wordlist[374];
                goto compare;
              case 898:
                resword = &wordlist[375];
                goto compare;
              case 901:
                resword = &wordlist[376];
                goto compare;
              case 906:
                resword = &wordlist[377];
                goto compare;
              case 907:
                resword = &wordlist[378];
                goto compare;
              case 908:
                resword = &wordlist[379];
                goto compare;
              case 910:
                resword = &wordlist[380];
                goto compare;
              case 911:
                resword = &wordlist[381];
                goto compare;
              case 915:
                resword = &wordlist[382];
                goto compare;
              case 918:
                resword = &wordlist[383];
                goto compare;
              case 919:
                resword = &wordlist[384];
                goto compare;
              case 925:
                resword = &wordlist[385];
                goto compare;
              case 926:
                resword = &wordlist[386];
                goto compare;
              case 928:
                resword = &wordlist[387];
                goto compare;
              case 931:
                resword = &wordlist[388];
                goto compare;
              case 932:
                resword = &wordlist[389];
                goto compare;
              case 935:
                resword = &wordlist[390];
                goto compare;
              case 939:
                resword = &wordlist[391];
                goto compare;
              case 941:
                resword = &wordlist[392];
                goto compare;
              case 942:
                resword = &wordlist[393];
                goto compare;
              case 949:
                resword = &wordlist[394];
                goto compare;
              case 954:
                resword = &wordlist[395];
                goto compare;
              case 956:
                resword = &wordlist[396];
                goto compare;
              case 957:
                resword = &wordlist[397];
                goto compare;
              case 958:
                resword = &wordlist[398];
                goto compare;
              case 962:
                resword = &wordlist[399];
                goto compare;
              case 963:
                resword = &wordlist[400];
                goto compare;
              case 969:
                resword = &wordlist[401];
                goto compare;
              case 971:
                resword = &wordlist[402];
                goto compare;
              case 977:
                resword = &wordlist[403];
                goto compare;
              case 982:
                resword = &wordlist[404];
                goto compare;
              case 983:
                resword = &wordlist[405];
                goto compare;
              case 985:
                resword = &wordlist[406];
                goto compare;
              case 986:
                resword = &wordlist[407];
                goto compare;
              case 992:
                resword = &wordlist[408];
                goto compare;
              case 997:
                resword = &wordlist[409];
                goto compare;
              case 1011:
                resword = &wordlist[410];
                goto compare;
              case 1024:
                resword = &wordlist[411];
                goto compare;
              case 1026:
                resword = &wordlist[412];
                goto compare;
              case 1030:
                resword = &wordlist[413];
                goto compare;
              case 1034:
                resword = &wordlist[414];
                goto compare;
              case 1037:
                resword = &wordlist[415];
                goto compare;
              case 1039:
                resword = &wordlist[416];
                goto compare;
              case 1042:
                resword = &wordlist[417];
                goto compare;
              case 1043:
                resword = &wordlist[418];
                goto compare;
              case 1051:
                resword = &wordlist[419];
                goto compare;
              case 1052:
                resword = &wordlist[420];
                goto compare;
              case 1058:
                resword = &wordlist[421];
                goto compare;
              case 1064:
                resword = &wordlist[422];
                goto compare;
              case 1066:
                resword = &wordlist[423];
                goto compare;
              case 1077:
                resword = &wordlist[424];
                goto compare;
              case 1078:
                resword = &wordlist[425];
                goto compare;
              case 1079:
                resword = &wordlist[426];
                goto compare;
              case 1097:
                resword = &wordlist[427];
                goto compare;
              case 1100:
                resword = &wordlist[428];
                goto compare;
              case 1109:
                resword = &wordlist[429];
                goto compare;
              case 1114:
                resword = &wordlist[430];
                goto compare;
              case 1118:
                resword = &wordlist[431];
                goto compare;
              case 1120:
                resword = &wordlist[432];
                goto compare;
              case 1125:
                resword = &wordlist[433];
                goto compare;
              case 1157:
                resword = &wordlist[434];
                goto compare;
              case 1165:
                resword = &wordlist[435];
                goto compare;
              case 1173:
                resword = &wordlist[436];
                goto compare;
              case 1174:
                resword = &wordlist[437];
                goto compare;
              case 1176:
                resword = &wordlist[438];
                goto compare;
              case 1181:
                resword = &wordlist[439];
                goto compare;
              case 1182:
                resword = &wordlist[440];
                goto compare;
              case 1186:
                resword = &wordlist[441];
                goto compare;
              case 1199:
                resword = &wordlist[442];
                goto compare;
              case 1200:
                resword = &wordlist[443];
                goto compare;
              case 1209:
                resword = &wordlist[444];
                goto compare;
              case 1211:
                resword = &wordlist[445];
                goto compare;
              case 1213:
                resword = &wordlist[446];
                goto compare;
              case 1216:
                resword = &wordlist[447];
                goto compare;
              case 1225:
                resword = &wordlist[448];
                goto compare;
              case 1238:
                resword = &wordlist[449];
                goto compare;
              case 1240:
                resword = &wordlist[450];
                goto compare;
              case 1246:
                resword = &wordlist[451];
                goto compare;
              case 1252:
                resword = &wordlist[452];
                goto compare;
              case 1254:
                resword = &wordlist[453];
                goto compare;
              case 1263:
                resword = &wordlist[454];
                goto compare;
              case 1267:
                resword = &wordlist[455];
                goto compare;
              case 1277:
                resword = &wordlist[456];
                goto compare;
              case 1278:
                resword = &wordlist[457];
                goto compare;
              case 1282:
                resword = &wordlist[458];
                goto compare;
              case 1290:
                resword = &wordlist[459];
                goto compare;
              case 1300:
                resword = &wordlist[460];
                goto compare;
              case 1315:
                resword = &wordlist[461];
                goto compare;
              case 1328:
                resword = &wordlist[462];
                goto compare;
              case 1339:
                resword = &wordlist[463];
                goto compare;
              case 1341:
                resword = &wordlist[464];
                goto compare;
              case 1342:
                resword = &wordlist[465];
                goto compare;
              case 1351:
                resword = &wordlist[466];
                goto compare;
              case 1353:
                resword = &wordlist[467];
                goto compare;
              case 1354:
                resword = &wordlist[468];
                goto compare;
              case 1356:
                resword = &wordlist[469];
                goto compare;
              case 1365:
                resword = &wordlist[470];
                goto compare;
              case 1378:
                resword = &wordlist[471];
                goto compare;
              case 1379:
                resword = &wordlist[472];
                goto compare;
              case 1380:
                resword = &wordlist[473];
                goto compare;
              case 1412:
                resword = &wordlist[474];
                goto compare;
              case 1417:
                resword = &wordlist[475];
                goto compare;
              case 1419:
                resword = &wordlist[476];
                goto compare;
              case 1422:
                resword = &wordlist[477];
                goto compare;
              case 1428:
                resword = &wordlist[478];
                goto compare;
              case 1438:
                resword = &wordlist[479];
                goto compare;
              case 1453:
                resword = &wordlist[480];
                goto compare;
              case 1482:
                resword = &wordlist[481];
                goto compare;
              case 1483:
                resword = &wordlist[482];
                goto compare;
              case 1491:
                resword = &wordlist[483];
                goto compare;
              case 1495:
                resword = &wordlist[484];
                goto compare;
              case 1517:
                resword = &wordlist[485];
                goto compare;
              case 1543:
                resword = &wordlist[486];
                goto compare;
              case 1551:
                resword = &wordlist[487];
                goto compare;
              case 1561:
                resword = &wordlist[488];
                goto compare;
              case 1563:
                resword = &wordlist[489];
                goto compare;
              case 1567:
                resword = &wordlist[490];
                goto compare;
              case 1575:
                resword = &wordlist[491];
                goto compare;
              case 1613:
                resword = &wordlist[492];
                goto compare;
              case 1694:
                resword = &wordlist[493];
                goto compare;
              case 1720:
                resword = &wordlist[494];
                goto compare;
              case 1722:
                resword = &wordlist[495];
                goto compare;
              case 1735:
                resword = &wordlist[496];
                goto compare;
              case 1749:
                resword = &wordlist[497];
                goto compare;
              case 1754:
                resword = &wordlist[498];
                goto compare;
              case 1772:
                resword = &wordlist[499];
                goto compare;
              case 1777:
                resword = &wordlist[500];
                goto compare;
              case 1779:
                resword = &wordlist[501];
                goto compare;
              case 1782:
                resword = &wordlist[502];
                goto compare;
              case 1830:
                resword = &wordlist[503];
                goto compare;
              case 1839:
                resword = &wordlist[504];
                goto compare;
              case 1890:
                resword = &wordlist[505];
                goto compare;
              case 1930:
                resword = &wordlist[506];
                goto compare;
              case 2004:
                resword = &wordlist[507];
                goto compare;
            }
          return 0;
        compare:
          {
            register const char *s = resword->name + stringpool;

            if (*str == *s && !strcmp (str + 1, s + 1))
              return resword;
          }
        }
    }
  return 0;
}
#line 541 "syscalls.perf"


static int __syscall_offset_value(const struct arch_syscall_table *s,
				    int offset)
{
	return *(int *)((char *)s + offset);
}

static const struct arch_syscall_table *__syscall_lookup_name(const char *name)
{
	return in_word_set(name, strlen(name));
}

static const struct arch_syscall_table *__syscall_lookup_num(int num,
							     int offset_arch)
{
	unsigned int i;

	for (i = 0; i < sizeof(wordlist)/sizeof(wordlist[0]); i++) {
		if (__syscall_offset_value(&wordlist[i], offset_arch) == num)
			return &wordlist[i];
	}

	return NULL;
}

int syscall_resolve_name(const char *name, int offset_arch)
{
	const struct arch_syscall_table *entry;

	entry = __syscall_lookup_name(name);
	if (!entry)
		return __NR_SCMP_ERROR;

	return __syscall_offset_value(entry, offset_arch);
}

const char *syscall_resolve_num(int num, int offset_arch)
{
	const struct arch_syscall_table *entry;

	entry = __syscall_lookup_num(num, offset_arch);
	if (!entry)
		return NULL;

	return (stringpool + entry->name);
}

enum scmp_kver syscall_resolve_name_kver(const char *name, int offset_kver)
{
	const struct arch_syscall_table *entry;

	entry = __syscall_lookup_name(name);
	if (!entry)
		return __SCMP_KV_NULL;

	return __syscall_offset_value(entry, offset_kver);
}

enum scmp_kver syscall_resolve_num_kver(int num,
					int offset_arch, int offset_kver)
{
	const struct arch_syscall_table *entry;

	entry = __syscall_lookup_num(num, offset_arch);
	if (!entry)
		return __SCMP_KV_NULL;

	return __syscall_offset_value(entry, offset_kver);
}

/* DANGER: this is NOT THREAD-SAFE, use only for testing */
const struct arch_syscall_def *syscall_iterate(unsigned int spot, int offset)
{
	unsigned int iter;
	static struct arch_syscall_def arch_def;

	/* DANGER: see the note above, NOT THREAD-SAFE, use only for testing */

	arch_def.name = NULL;
	arch_def.num = __NR_SCMP_ERROR;

	for (iter = 0; iter < sizeof(wordlist)/sizeof(wordlist[0]); iter++) {
		if (wordlist[iter].index == spot) {
			arch_def.name = stringpool + wordlist[iter].name;
			arch_def.num = __syscall_offset_value(&wordlist[iter],
								offset);
			return &arch_def;
		}
	}

	return &arch_def;
}
