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
    TOTAL_KEYWORDS = 502,
    MIN_WORD_LENGTH = 3,
    MAX_WORD_LENGTH = 28,
    MIN_HASH_VALUE = 7,
    MAX_HASH_VALUE = 1916
  };

/* maximum key range = 1910, duplicates = 0 */

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
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,   27,
       424,  101,  306, 1917,  195,    1, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917,    0,  522,   79,  321,    4,
         5,    2,    5,    1,  479,    5,   64,  282,   61,   39,
       186,   28,   73,  184,   21,    0,    0,  240,  374,  399,
       291,  283,    5, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917, 1917,
      1917, 1917, 1917, 1917, 1917, 1917, 1917
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
    char stringpool_str2[sizeof("times")];
    char stringpool_str3[sizeof("time")];
    char stringpool_str4[sizeof("select")];
    char stringpool_str5[sizeof("idle")];
    char stringpool_str6[sizeof("setsid")];
    char stringpool_str7[sizeof("getsid")];
    char stringpool_str8[sizeof("rtas")];
    char stringpool_str9[sizeof("setfsgid")];
    char stringpool_str10[sizeof("setregid")];
    char stringpool_str11[sizeof("setresgid")];
    char stringpool_str12[sizeof("getresgid")];
    char stringpool_str13[sizeof("getegid")];
    char stringpool_str14[sizeof("read")];
    char stringpool_str15[sizeof("setns")];
    char stringpool_str16[sizeof("fsync")];
    char stringpool_str17[sizeof("timer_settime")];
    char stringpool_str18[sizeof("timer_gettime")];
    char stringpool_str19[sizeof("sched_setattr")];
    char stringpool_str20[sizeof("sched_getattr")];
    char stringpool_str21[sizeof("sendmsg")];
    char stringpool_str22[sizeof("timerfd")];
    char stringpool_str23[sizeof("sched_setscheduler")];
    char stringpool_str24[sizeof("sched_getscheduler")];
    char stringpool_str25[sizeof("timerfd_settime")];
    char stringpool_str26[sizeof("timerfd_gettime")];
    char stringpool_str27[sizeof("timerfd_create")];
    char stringpool_str28[sizeof("fchdir")];
    char stringpool_str29[sizeof("memfd_secret")];
    char stringpool_str30[sizeof("sendto")];
    char stringpool_str31[sizeof("sched_setparam")];
    char stringpool_str32[sizeof("sched_getparam")];
    char stringpool_str33[sizeof("timer_create")];
    char stringpool_str34[sizeof("connect")];
    char stringpool_str35[sizeof("close")];
    char stringpool_str36[sizeof("ioprio_set")];
    char stringpool_str37[sizeof("ioprio_get")];
    char stringpool_str38[sizeof("msync")];
    char stringpool_str39[sizeof("readdir")];
    char stringpool_str40[sizeof("ipc")];
    char stringpool_str41[sizeof("rt_sigtimedwait")];
    char stringpool_str42[sizeof("sendfile")];
    char stringpool_str43[sizeof("memfd_create")];
    char stringpool_str44[sizeof("pipe")];
    char stringpool_str45[sizeof("capset")];
    char stringpool_str46[sizeof("sendmmsg")];
    char stringpool_str47[sizeof("access")];
    char stringpool_str48[sizeof("delete_module")];
    char stringpool_str49[sizeof("socket")];
    char stringpool_str50[sizeof("mount")];
    char stringpool_str51[sizeof("clone")];
    char stringpool_str52[sizeof("pidfd_getfd")];
    char stringpool_str53[sizeof("prof")];
    char stringpool_str54[sizeof("mincore")];
    char stringpool_str55[sizeof("timer_delete")];
    char stringpool_str56[sizeof("setrlimit")];
    char stringpool_str57[sizeof("getrlimit")];
    char stringpool_str58[sizeof("copy_file_range")];
    char stringpool_str59[sizeof("reboot")];
    char stringpool_str60[sizeof("mount_setattr")];
    char stringpool_str61[sizeof("_sysctl")];
    char stringpool_str62[sizeof("semctl")];
    char stringpool_str63[sizeof("iopl")];
    char stringpool_str64[sizeof("sched_rr_get_interval")];
    char stringpool_str65[sizeof("ioperm")];
    char stringpool_str66[sizeof("truncate")];
    char stringpool_str67[sizeof("splice")];
    char stringpool_str68[sizeof("process_madvise")];
    char stringpool_str69[sizeof("process_mrelease")];
    char stringpool_str70[sizeof("finit_module")];
    char stringpool_str71[sizeof("pause")];
    char stringpool_str72[sizeof("setitimer")];
    char stringpool_str73[sizeof("getitimer")];
    char stringpool_str74[sizeof("open_tree")];
    char stringpool_str75[sizeof("accept")];
    char stringpool_str76[sizeof("rmdir")];
    char stringpool_str77[sizeof("msgctl")];
    char stringpool_str78[sizeof("oldstat")];
    char stringpool_str79[sizeof("oldfstat")];
    char stringpool_str80[sizeof("cachestat")];
    char stringpool_str81[sizeof("faccessat")];
    char stringpool_str82[sizeof("stime")];
    char stringpool_str83[sizeof("signalfd")];
    char stringpool_str84[sizeof("mprotect")];
    char stringpool_str85[sizeof("ftime")];
    char stringpool_str86[sizeof("getdents")];
    char stringpool_str87[sizeof("nice")];
    char stringpool_str88[sizeof("membarrier")];
    char stringpool_str89[sizeof("poll")];
    char stringpool_str90[sizeof("getpid")];
    char stringpool_str91[sizeof("setpgid")];
    char stringpool_str92[sizeof("getpgid")];
    char stringpool_str93[sizeof("migrate_pages")];
    char stringpool_str94[sizeof("linkat")];
    char stringpool_str95[sizeof("openat")];
    char stringpool_str96[sizeof("oldlstat")];
    char stringpool_str97[sizeof("epoll_create")];
    char stringpool_str98[sizeof("alarm")];
    char stringpool_str99[sizeof("cachectl")];
    char stringpool_str100[sizeof("sched_get_priority_min")];
    char stringpool_str101[sizeof("semop")];
    char stringpool_str102[sizeof("seccomp")];
    char stringpool_str103[sizeof("profil")];
    char stringpool_str104[sizeof("rseq")];
    char stringpool_str105[sizeof("s390_pci_mmio_write")];
    char stringpool_str106[sizeof("s390_pci_mmio_read")];
    char stringpool_str107[sizeof("getpmsg")];
    char stringpool_str108[sizeof("timer_getoverrun")];
    char stringpool_str109[sizeof("move_pages")];
    char stringpool_str110[sizeof("pivot_root")];
    char stringpool_str111[sizeof("fchmod")];
    char stringpool_str112[sizeof("signal")];
    char stringpool_str113[sizeof("msgsnd")];
    char stringpool_str114[sizeof("epoll_create1")];
    char stringpool_str115[sizeof("stat")];
    char stringpool_str116[sizeof("fallocate")];
    char stringpool_str117[sizeof("rt_sigreturn")];
    char stringpool_str118[sizeof("statfs")];
    char stringpool_str119[sizeof("tgkill")];
    char stringpool_str120[sizeof("epoll_ctl_old")];
    char stringpool_str121[sizeof("gettid")];
    char stringpool_str122[sizeof("setfsuid")];
    char stringpool_str123[sizeof("setreuid")];
    char stringpool_str124[sizeof("setresuid")];
    char stringpool_str125[sizeof("getresuid")];
    char stringpool_str126[sizeof("geteuid")];
    char stringpool_str127[sizeof("arch_prctl")];
    char stringpool_str128[sizeof("socketpair")];
    char stringpool_str129[sizeof("getppid")];
    char stringpool_str130[sizeof("fsconfig")];
    char stringpool_str131[sizeof("rt_sigsuspend")];
    char stringpool_str132[sizeof("getpagesize")];
    char stringpool_str133[sizeof("sysfs")];
    char stringpool_str134[sizeof("stty")];
    char stringpool_str135[sizeof("gtty")];
    char stringpool_str136[sizeof("sync")];
    char stringpool_str137[sizeof("syncfs")];
    char stringpool_str138[sizeof("rt_sigpending")];
    char stringpool_str139[sizeof("clone3")];
    char stringpool_str140[sizeof("socketcall")];
    char stringpool_str141[sizeof("rt_sigaction")];
    char stringpool_str142[sizeof("epoll_ctl")];
    char stringpool_str143[sizeof("ppoll")];
    char stringpool_str144[sizeof("sethostname")];
    char stringpool_str145[sizeof("fchmodat")];
    char stringpool_str146[sizeof("sched_setaffinity")];
    char stringpool_str147[sizeof("sched_getaffinity")];
    char stringpool_str148[sizeof("open")];
    char stringpool_str149[sizeof("sched_yield")];
    char stringpool_str150[sizeof("dup")];
    char stringpool_str151[sizeof("pciconfig_write")];
    char stringpool_str152[sizeof("pciconfig_iobase")];
    char stringpool_str153[sizeof("pciconfig_read")];
    char stringpool_str154[sizeof("acct")];
    char stringpool_str155[sizeof("sched_get_priority_max")];
    char stringpool_str156[sizeof("fstat")];
    char stringpool_str157[sizeof("getrusage")];
    char stringpool_str158[sizeof("bind")];
    char stringpool_str159[sizeof("fstatfs")];
    char stringpool_str160[sizeof("mmap")];
    char stringpool_str161[sizeof("pidfd_send_signal")];
    char stringpool_str162[sizeof("creat")];
    char stringpool_str163[sizeof("timer_settime64")];
    char stringpool_str164[sizeof("timer_gettime64")];
    char stringpool_str165[sizeof("setdomainname")];
    char stringpool_str166[sizeof("newfstatat")];
    char stringpool_str167[sizeof("syslog")];
    char stringpool_str168[sizeof("getpgrp")];
    char stringpool_str169[sizeof("timerfd_settime64")];
    char stringpool_str170[sizeof("timerfd_gettime64")];
    char stringpool_str171[sizeof("close_range")];
    char stringpool_str172[sizeof("clock_getres")];
    char stringpool_str173[sizeof("clock_settime")];
    char stringpool_str174[sizeof("clock_gettime")];
    char stringpool_str175[sizeof("fcntl")];
    char stringpool_str176[sizeof("sync_file_range")];
    char stringpool_str177[sizeof("io_destroy")];
    char stringpool_str178[sizeof("fork")];
    char stringpool_str179[sizeof("pidfd_open")];
    char stringpool_str180[sizeof("kexec_file_load")];
    char stringpool_str181[sizeof("lstat")];
    char stringpool_str182[sizeof("getrandom")];
    char stringpool_str183[sizeof("sched_rr_get_interval_time64")];
    char stringpool_str184[sizeof("kill")];
    char stringpool_str185[sizeof("rename")];
    char stringpool_str186[sizeof("setuid")];
    char stringpool_str187[sizeof("getuid")];
    char stringpool_str188[sizeof("rt_sigtimedwait_time64")];
    char stringpool_str189[sizeof("ioctl")];
    char stringpool_str190[sizeof("pkey_free")];
    char stringpool_str191[sizeof("bpf")];
    char stringpool_str192[sizeof("mbind")];
    char stringpool_str193[sizeof("semtimedop")];
    char stringpool_str194[sizeof("mpx")];
    char stringpool_str195[sizeof("ptrace")];
    char stringpool_str196[sizeof("mknod")];
    char stringpool_str197[sizeof("link")];
    char stringpool_str198[sizeof("setxattr")];
    char stringpool_str199[sizeof("getxattr")];
    char stringpool_str200[sizeof("keyctl")];
    char stringpool_str201[sizeof("getcwd")];
    char stringpool_str202[sizeof("eventfd")];
    char stringpool_str203[sizeof("setsockopt")];
    char stringpool_str204[sizeof("getsockopt")];
    char stringpool_str205[sizeof("io_setup")];
    char stringpool_str206[sizeof("create_module")];
    char stringpool_str207[sizeof("mkdir")];
    char stringpool_str208[sizeof("utimes")];
    char stringpool_str209[sizeof("utime")];
    char stringpool_str210[sizeof("lock")];
    char stringpool_str211[sizeof("futimesat")];
    char stringpool_str212[sizeof("restart_syscall")];
    char stringpool_str213[sizeof("io_cancel")];
    char stringpool_str214[sizeof("rt_sigprocmask")];
    char stringpool_str215[sizeof("tkill")];
    char stringpool_str216[sizeof("setresgid32")];
    char stringpool_str217[sizeof("getresgid32")];
    char stringpool_str218[sizeof("prctl")];
    char stringpool_str219[sizeof("fanotify_init")];
    char stringpool_str220[sizeof("flistxattr")];
    char stringpool_str221[sizeof("recvmsg")];
    char stringpool_str222[sizeof("renameat")];
    char stringpool_str223[sizeof("setxattrat")];
    char stringpool_str224[sizeof("getxattrat")];
    char stringpool_str225[sizeof("kexec_load")];
    char stringpool_str226[sizeof("mremap")];
    char stringpool_str227[sizeof("mknodat")];
    char stringpool_str228[sizeof("shmdt")];
    char stringpool_str229[sizeof("mseal")];
    char stringpool_str230[sizeof("lookup_dcookie")];
    char stringpool_str231[sizeof("semget")];
    char stringpool_str232[sizeof("getpeername")];
    char stringpool_str233[sizeof("s390_guarded_storage")];
    char stringpool_str234[sizeof("mkdirat")];
    char stringpool_str235[sizeof("modify_ldt")];
    char stringpool_str236[sizeof("rt_sigqueueinfo")];
    char stringpool_str237[sizeof("rt_tgsigqueueinfo")];
    char stringpool_str238[sizeof("ulimit")];
    char stringpool_str239[sizeof("setgid")];
    char stringpool_str240[sizeof("getgid")];
    char stringpool_str241[sizeof("remap_file_pages")];
    char stringpool_str242[sizeof("recvmmsg")];
    char stringpool_str243[sizeof("fsmount")];
    char stringpool_str244[sizeof("tuxcall")];
    char stringpool_str245[sizeof("clock_adjtime")];
    char stringpool_str246[sizeof("pselect6")];
    char stringpool_str247[sizeof("vm86old")];
    char stringpool_str248[sizeof("sigsuspend")];
    char stringpool_str249[sizeof("llistxattr")];
    char stringpool_str250[sizeof("msgget")];
    char stringpool_str251[sizeof("mq_timedsend")];
    char stringpool_str252[sizeof("madvise")];
    char stringpool_str253[sizeof("pkey_mprotect")];
    char stringpool_str254[sizeof("landlock_add_rule")];
    char stringpool_str255[sizeof("exit")];
    char stringpool_str256[sizeof("landlock_create_ruleset")];
    char stringpool_str257[sizeof("unshare")];
    char stringpool_str258[sizeof("landlock_restrict_self")];
    char stringpool_str259[sizeof("putpmsg")];
    char stringpool_str260[sizeof("kcmp")];
    char stringpool_str261[sizeof("setfsgid32")];
    char stringpool_str262[sizeof("setregid32")];
    char stringpool_str263[sizeof("mq_timedreceive")];
    char stringpool_str264[sizeof("sysmips")];
    char stringpool_str265[sizeof("_newselect")];
    char stringpool_str266[sizeof("syscall")];
    char stringpool_str267[sizeof("nanosleep")];
    char stringpool_str268[sizeof("setpriority")];
    char stringpool_str269[sizeof("getpriority")];
    char stringpool_str270[sizeof("recvfrom")];
    char stringpool_str271[sizeof("ustat")];
    char stringpool_str272[sizeof("getcpu")];
    char stringpool_str273[sizeof("fsopen")];
    char stringpool_str274[sizeof("capget")];
    char stringpool_str275[sizeof("chmod")];
    char stringpool_str276[sizeof("move_mount")];
    char stringpool_str277[sizeof("sigpending")];
    char stringpool_str278[sizeof("sendfile64")];
    char stringpool_str279[sizeof("chroot")];
    char stringpool_str280[sizeof("subpage_prot")];
    char stringpool_str281[sizeof("set_tls")];
    char stringpool_str282[sizeof("get_tls")];
    char stringpool_str283[sizeof("chdir")];
    char stringpool_str284[sizeof("getsockname")];
    char stringpool_str285[sizeof("sysinfo")];
    char stringpool_str286[sizeof("pkey_alloc")];
    char stringpool_str287[sizeof("faccessat2")];
    char stringpool_str288[sizeof("ppoll_time64")];
    char stringpool_str289[sizeof("io_pgetevents")];
    char stringpool_str290[sizeof("shmctl")];
    char stringpool_str291[sizeof("vm86")];
    char stringpool_str292[sizeof("s390_runtime_instr")];
    char stringpool_str293[sizeof("settimeofday")];
    char stringpool_str294[sizeof("gettimeofday")];
    char stringpool_str295[sizeof("uname")];
    char stringpool_str296[sizeof("ugetrlimit")];
    char stringpool_str297[sizeof("lsm_set_self_attr")];
    char stringpool_str298[sizeof("lsm_get_self_attr")];
    char stringpool_str299[sizeof("brk")];
    char stringpool_str300[sizeof("set_tid_address")];
    char stringpool_str301[sizeof("swapoff")];
    char stringpool_str302[sizeof("pselect6_time64")];
    char stringpool_str303[sizeof("lseek")];
    char stringpool_str304[sizeof("_llseek")];
    char stringpool_str305[sizeof("flock")];
    char stringpool_str306[sizeof("userfaultfd")];
    char stringpool_str307[sizeof("fspick")];
    char stringpool_str308[sizeof("semtimedop_time64")];
    char stringpool_str309[sizeof("truncate64")];
    char stringpool_str310[sizeof("io_submit")];
    char stringpool_str311[sizeof("readlinkat")];
    char stringpool_str312[sizeof("sigreturn")];
    char stringpool_str313[sizeof("sigprocmask")];
    char stringpool_str314[sizeof("io_uring_enter")];
    char stringpool_str315[sizeof("dup3")];
    char stringpool_str316[sizeof("mlockall")];
    char stringpool_str317[sizeof("io_uring_register")];
    char stringpool_str318[sizeof("ftruncate")];
    char stringpool_str319[sizeof("nfsservctl")];
    char stringpool_str320[sizeof("lsm_list_modules")];
    char stringpool_str321[sizeof("fsetxattr")];
    char stringpool_str322[sizeof("fgetxattr")];
    char stringpool_str323[sizeof("epoll_wait")];
    char stringpool_str324[sizeof("write")];
    char stringpool_str325[sizeof("mlock")];
    char stringpool_str326[sizeof("clock_settime64")];
    char stringpool_str327[sizeof("clock_gettime64")];
    char stringpool_str328[sizeof("epoll_wait_old")];
    char stringpool_str329[sizeof("vmsplice")];
    char stringpool_str330[sizeof("clock_getres_time64")];
    char stringpool_str331[sizeof("execve")];
    char stringpool_str332[sizeof("readahead")];
    char stringpool_str333[sizeof("listen")];
    char stringpool_str334[sizeof("setresuid32")];
    char stringpool_str335[sizeof("getresuid32")];
    char stringpool_str336[sizeof("munmap")];
    char stringpool_str337[sizeof("utimensat")];
    char stringpool_str338[sizeof("getdents64")];
    char stringpool_str339[sizeof("io_uring_setup")];
    char stringpool_str340[sizeof("listxattrat")];
    char stringpool_str341[sizeof("atomic_barrier")];
    char stringpool_str342[sizeof("spu_create")];
    char stringpool_str343[sizeof("lsetxattr")];
    char stringpool_str344[sizeof("lgetxattr")];
    char stringpool_str345[sizeof("listxattr")];
    char stringpool_str346[sizeof("personality")];
    char stringpool_str347[sizeof("set_mempolicy_home_node")];
    char stringpool_str348[sizeof("riscv_flush_icache")];
    char stringpool_str349[sizeof("usr26")];
    char stringpool_str350[sizeof("epoll_pwait")];
    char stringpool_str351[sizeof("waitid")];
    char stringpool_str352[sizeof("mq_getsetattr")];
    char stringpool_str353[sizeof("inotify_init")];
    char stringpool_str354[sizeof("statmount")];
    char stringpool_str355[sizeof("sigaction")];
    char stringpool_str356[sizeof("fanotify_mark")];
    char stringpool_str357[sizeof("oldolduname")];
    char stringpool_str358[sizeof("mq_open")];
    char stringpool_str359[sizeof("fchownat")];
    char stringpool_str360[sizeof("init_module")];
    char stringpool_str361[sizeof("atomic_cmpxchg_32")];
    char stringpool_str362[sizeof("sigaltstack")];
    char stringpool_str363[sizeof("execveat")];
    char stringpool_str364[sizeof("futex_requeue")];
    char stringpool_str365[sizeof("inotify_init1")];
    char stringpool_str366[sizeof("dup2")];
    char stringpool_str367[sizeof("readv")];
    char stringpool_str368[sizeof("olduname")];
    char stringpool_str369[sizeof("accept4")];
    char stringpool_str370[sizeof("setfsuid32")];
    char stringpool_str371[sizeof("setreuid32")];
    char stringpool_str372[sizeof("query_module")];
    char stringpool_str373[sizeof("name_to_handle_at")];
    char stringpool_str374[sizeof("msgrcv")];
    char stringpool_str375[sizeof("vserver")];
    char stringpool_str376[sizeof("recv")];
    char stringpool_str377[sizeof("sync_file_range2")];
    char stringpool_str378[sizeof("signalfd4")];
    char stringpool_str379[sizeof("shmat")];
    char stringpool_str380[sizeof("listmount")];
    char stringpool_str381[sizeof("waitpid")];
    char stringpool_str382[sizeof("swapcontext")];
    char stringpool_str383[sizeof("fdatasync")];
    char stringpool_str384[sizeof("clock_adjtime64")];
    char stringpool_str385[sizeof("recvmmsg_time64")];
    char stringpool_str386[sizeof("quotactl_fd")];
    char stringpool_str387[sizeof("statx")];
    char stringpool_str388[sizeof("futex")];
    char stringpool_str389[sizeof("mq_timedsend_time64")];
    char stringpool_str390[sizeof("perf_event_open")];
    char stringpool_str391[sizeof("munlockall")];
    char stringpool_str392[sizeof("umount")];
    char stringpool_str393[sizeof("umask")];
    char stringpool_str394[sizeof("readlink")];
    char stringpool_str395[sizeof("setgroups")];
    char stringpool_str396[sizeof("getgroups")];
    char stringpool_str397[sizeof("removexattrat")];
    char stringpool_str398[sizeof("fchown")];
    char stringpool_str399[sizeof("mq_timedreceive_time64")];
    char stringpool_str400[sizeof("mq_notify")];
    char stringpool_str401[sizeof("process_vm_readv")];
    char stringpool_str402[sizeof("process_vm_writev")];
    char stringpool_str403[sizeof("removexattr")];
    char stringpool_str404[sizeof("quotactl")];
    char stringpool_str405[sizeof("symlinkat")];
    char stringpool_str406[sizeof("cacheflush")];
    char stringpool_str407[sizeof("clock_nanosleep")];
    char stringpool_str408[sizeof("futex_time64")];
    char stringpool_str409[sizeof("io_getevents")];
    char stringpool_str410[sizeof("s390_sthyi")];
    char stringpool_str411[sizeof("exit_group")];
    char stringpool_str412[sizeof("lchown")];
    char stringpool_str413[sizeof("munlock")];
    char stringpool_str414[sizeof("io_pgetevents_time64")];
    char stringpool_str415[sizeof("ssetmask")];
    char stringpool_str416[sizeof("sgetmask")];
    char stringpool_str417[sizeof("uselib")];
    char stringpool_str418[sizeof("pipe2")];
    char stringpool_str419[sizeof("vfork")];
    char stringpool_str420[sizeof("uretprobe")];
    char stringpool_str421[sizeof("adjtimex")];
    char stringpool_str422[sizeof("shmget")];
    char stringpool_str423[sizeof("ftruncate64")];
    char stringpool_str424[sizeof("request_key")];
    char stringpool_str425[sizeof("security")];
    char stringpool_str426[sizeof("getegid32")];
    char stringpool_str427[sizeof("multiplexer")];
    char stringpool_str428[sizeof("swapon")];
    char stringpool_str429[sizeof("set_mempolicy")];
    char stringpool_str430[sizeof("get_mempolicy")];
    char stringpool_str431[sizeof("utimensat_time64")];
    char stringpool_str432[sizeof("prlimit64")];
    char stringpool_str433[sizeof("fremovexattr")];
    char stringpool_str434[sizeof("get_kernel_syms")];
    char stringpool_str435[sizeof("futex_wait")];
    char stringpool_str436[sizeof("afs_syscall")];
    char stringpool_str437[sizeof("unlinkat")];
    char stringpool_str438[sizeof("stat64")];
    char stringpool_str439[sizeof("statfs64")];
    char stringpool_str440[sizeof("openat2")];
    char stringpool_str441[sizeof("lremovexattr")];
    char stringpool_str442[sizeof("symlink")];
    char stringpool_str443[sizeof("mmap2")];
    char stringpool_str444[sizeof("arm_sync_file_range")];
    char stringpool_str445[sizeof("fcntl64")];
    char stringpool_str446[sizeof("clock_nanosleep_time64")];
    char stringpool_str447[sizeof("fstat64")];
    char stringpool_str448[sizeof("fstatfs64")];
    char stringpool_str449[sizeof("set_robust_list")];
    char stringpool_str450[sizeof("get_robust_list")];
    char stringpool_str451[sizeof("chown")];
    char stringpool_str452[sizeof("epoll_pwait2")];
    char stringpool_str453[sizeof("fchmodat2")];
    char stringpool_str454[sizeof("preadv")];
    char stringpool_str455[sizeof("lstat64")];
    char stringpool_str456[sizeof("set_thread_area")];
    char stringpool_str457[sizeof("get_thread_area")];
    char stringpool_str458[sizeof("geteuid32")];
    char stringpool_str459[sizeof("fstatat64")];
    char stringpool_str460[sizeof("open_by_handle_at")];
    char stringpool_str461[sizeof("break")];
    char stringpool_str462[sizeof("pread64")];
    char stringpool_str463[sizeof("spu_run")];
    char stringpool_str464[sizeof("inotify_rm_watch")];
    char stringpool_str465[sizeof("mlock2")];
    char stringpool_str466[sizeof("unlink")];
    char stringpool_str467[sizeof("breakpoint")];
    char stringpool_str468[sizeof("eventfd2")];
    char stringpool_str469[sizeof("vhangup")];
    char stringpool_str470[sizeof("setgroups32")];
    char stringpool_str471[sizeof("getgroups32")];
    char stringpool_str472[sizeof("pwritev")];
    char stringpool_str473[sizeof("inotify_add_watch")];
    char stringpool_str474[sizeof("fadvise64")];
    char stringpool_str475[sizeof("fadvise64_64")];
    char stringpool_str476[sizeof("futex_wake")];
    char stringpool_str477[sizeof("renameat2")];
    char stringpool_str478[sizeof("wait4")];
    char stringpool_str479[sizeof("setuid32")];
    char stringpool_str480[sizeof("getuid32")];
    char stringpool_str481[sizeof("pwrite64")];
    char stringpool_str482[sizeof("riscv_hwprobe")];
    char stringpool_str483[sizeof("sys_debug_setcontext")];
    char stringpool_str484[sizeof("usr32")];
    char stringpool_str485[sizeof("futex_waitv")];
    char stringpool_str486[sizeof("arm_fadvise64_64")];
    char stringpool_str487[sizeof("writev")];
    char stringpool_str488[sizeof("setgid32")];
    char stringpool_str489[sizeof("getgid32")];
    char stringpool_str490[sizeof("add_key")];
    char stringpool_str491[sizeof("switch_endian")];
    char stringpool_str492[sizeof("map_shadow_stack")];
    char stringpool_str493[sizeof("shutdown")];
    char stringpool_str494[sizeof("bdflush")];
    char stringpool_str495[sizeof("mq_unlink")];
    char stringpool_str496[sizeof("fchown32")];
    char stringpool_str497[sizeof("preadv2")];
    char stringpool_str498[sizeof("lchown32")];
    char stringpool_str499[sizeof("umount2")];
    char stringpool_str500[sizeof("pwritev2")];
    char stringpool_str501[sizeof("chown32")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "tee",
    "send",
    "times",
    "time",
    "select",
    "idle",
    "setsid",
    "getsid",
    "rtas",
    "setfsgid",
    "setregid",
    "setresgid",
    "getresgid",
    "getegid",
    "read",
    "setns",
    "fsync",
    "timer_settime",
    "timer_gettime",
    "sched_setattr",
    "sched_getattr",
    "sendmsg",
    "timerfd",
    "sched_setscheduler",
    "sched_getscheduler",
    "timerfd_settime",
    "timerfd_gettime",
    "timerfd_create",
    "fchdir",
    "memfd_secret",
    "sendto",
    "sched_setparam",
    "sched_getparam",
    "timer_create",
    "connect",
    "close",
    "ioprio_set",
    "ioprio_get",
    "msync",
    "readdir",
    "ipc",
    "rt_sigtimedwait",
    "sendfile",
    "memfd_create",
    "pipe",
    "capset",
    "sendmmsg",
    "access",
    "delete_module",
    "socket",
    "mount",
    "clone",
    "pidfd_getfd",
    "prof",
    "mincore",
    "timer_delete",
    "setrlimit",
    "getrlimit",
    "copy_file_range",
    "reboot",
    "mount_setattr",
    "_sysctl",
    "semctl",
    "iopl",
    "sched_rr_get_interval",
    "ioperm",
    "truncate",
    "splice",
    "process_madvise",
    "process_mrelease",
    "finit_module",
    "pause",
    "setitimer",
    "getitimer",
    "open_tree",
    "accept",
    "rmdir",
    "msgctl",
    "oldstat",
    "oldfstat",
    "cachestat",
    "faccessat",
    "stime",
    "signalfd",
    "mprotect",
    "ftime",
    "getdents",
    "nice",
    "membarrier",
    "poll",
    "getpid",
    "setpgid",
    "getpgid",
    "migrate_pages",
    "linkat",
    "openat",
    "oldlstat",
    "epoll_create",
    "alarm",
    "cachectl",
    "sched_get_priority_min",
    "semop",
    "seccomp",
    "profil",
    "rseq",
    "s390_pci_mmio_write",
    "s390_pci_mmio_read",
    "getpmsg",
    "timer_getoverrun",
    "move_pages",
    "pivot_root",
    "fchmod",
    "signal",
    "msgsnd",
    "epoll_create1",
    "stat",
    "fallocate",
    "rt_sigreturn",
    "statfs",
    "tgkill",
    "epoll_ctl_old",
    "gettid",
    "setfsuid",
    "setreuid",
    "setresuid",
    "getresuid",
    "geteuid",
    "arch_prctl",
    "socketpair",
    "getppid",
    "fsconfig",
    "rt_sigsuspend",
    "getpagesize",
    "sysfs",
    "stty",
    "gtty",
    "sync",
    "syncfs",
    "rt_sigpending",
    "clone3",
    "socketcall",
    "rt_sigaction",
    "epoll_ctl",
    "ppoll",
    "sethostname",
    "fchmodat",
    "sched_setaffinity",
    "sched_getaffinity",
    "open",
    "sched_yield",
    "dup",
    "pciconfig_write",
    "pciconfig_iobase",
    "pciconfig_read",
    "acct",
    "sched_get_priority_max",
    "fstat",
    "getrusage",
    "bind",
    "fstatfs",
    "mmap",
    "pidfd_send_signal",
    "creat",
    "timer_settime64",
    "timer_gettime64",
    "setdomainname",
    "newfstatat",
    "syslog",
    "getpgrp",
    "timerfd_settime64",
    "timerfd_gettime64",
    "close_range",
    "clock_getres",
    "clock_settime",
    "clock_gettime",
    "fcntl",
    "sync_file_range",
    "io_destroy",
    "fork",
    "pidfd_open",
    "kexec_file_load",
    "lstat",
    "getrandom",
    "sched_rr_get_interval_time64",
    "kill",
    "rename",
    "setuid",
    "getuid",
    "rt_sigtimedwait_time64",
    "ioctl",
    "pkey_free",
    "bpf",
    "mbind",
    "semtimedop",
    "mpx",
    "ptrace",
    "mknod",
    "link",
    "setxattr",
    "getxattr",
    "keyctl",
    "getcwd",
    "eventfd",
    "setsockopt",
    "getsockopt",
    "io_setup",
    "create_module",
    "mkdir",
    "utimes",
    "utime",
    "lock",
    "futimesat",
    "restart_syscall",
    "io_cancel",
    "rt_sigprocmask",
    "tkill",
    "setresgid32",
    "getresgid32",
    "prctl",
    "fanotify_init",
    "flistxattr",
    "recvmsg",
    "renameat",
    "setxattrat",
    "getxattrat",
    "kexec_load",
    "mremap",
    "mknodat",
    "shmdt",
    "mseal",
    "lookup_dcookie",
    "semget",
    "getpeername",
    "s390_guarded_storage",
    "mkdirat",
    "modify_ldt",
    "rt_sigqueueinfo",
    "rt_tgsigqueueinfo",
    "ulimit",
    "setgid",
    "getgid",
    "remap_file_pages",
    "recvmmsg",
    "fsmount",
    "tuxcall",
    "clock_adjtime",
    "pselect6",
    "vm86old",
    "sigsuspend",
    "llistxattr",
    "msgget",
    "mq_timedsend",
    "madvise",
    "pkey_mprotect",
    "landlock_add_rule",
    "exit",
    "landlock_create_ruleset",
    "unshare",
    "landlock_restrict_self",
    "putpmsg",
    "kcmp",
    "setfsgid32",
    "setregid32",
    "mq_timedreceive",
    "sysmips",
    "_newselect",
    "syscall",
    "nanosleep",
    "setpriority",
    "getpriority",
    "recvfrom",
    "ustat",
    "getcpu",
    "fsopen",
    "capget",
    "chmod",
    "move_mount",
    "sigpending",
    "sendfile64",
    "chroot",
    "subpage_prot",
    "set_tls",
    "get_tls",
    "chdir",
    "getsockname",
    "sysinfo",
    "pkey_alloc",
    "faccessat2",
    "ppoll_time64",
    "io_pgetevents",
    "shmctl",
    "vm86",
    "s390_runtime_instr",
    "settimeofday",
    "gettimeofday",
    "uname",
    "ugetrlimit",
    "lsm_set_self_attr",
    "lsm_get_self_attr",
    "brk",
    "set_tid_address",
    "swapoff",
    "pselect6_time64",
    "lseek",
    "_llseek",
    "flock",
    "userfaultfd",
    "fspick",
    "semtimedop_time64",
    "truncate64",
    "io_submit",
    "readlinkat",
    "sigreturn",
    "sigprocmask",
    "io_uring_enter",
    "dup3",
    "mlockall",
    "io_uring_register",
    "ftruncate",
    "nfsservctl",
    "lsm_list_modules",
    "fsetxattr",
    "fgetxattr",
    "epoll_wait",
    "write",
    "mlock",
    "clock_settime64",
    "clock_gettime64",
    "epoll_wait_old",
    "vmsplice",
    "clock_getres_time64",
    "execve",
    "readahead",
    "listen",
    "setresuid32",
    "getresuid32",
    "munmap",
    "utimensat",
    "getdents64",
    "io_uring_setup",
    "listxattrat",
    "atomic_barrier",
    "spu_create",
    "lsetxattr",
    "lgetxattr",
    "listxattr",
    "personality",
    "set_mempolicy_home_node",
    "riscv_flush_icache",
    "usr26",
    "epoll_pwait",
    "waitid",
    "mq_getsetattr",
    "inotify_init",
    "statmount",
    "sigaction",
    "fanotify_mark",
    "oldolduname",
    "mq_open",
    "fchownat",
    "init_module",
    "atomic_cmpxchg_32",
    "sigaltstack",
    "execveat",
    "futex_requeue",
    "inotify_init1",
    "dup2",
    "readv",
    "olduname",
    "accept4",
    "setfsuid32",
    "setreuid32",
    "query_module",
    "name_to_handle_at",
    "msgrcv",
    "vserver",
    "recv",
    "sync_file_range2",
    "signalfd4",
    "shmat",
    "listmount",
    "waitpid",
    "swapcontext",
    "fdatasync",
    "clock_adjtime64",
    "recvmmsg_time64",
    "quotactl_fd",
    "statx",
    "futex",
    "mq_timedsend_time64",
    "perf_event_open",
    "munlockall",
    "umount",
    "umask",
    "readlink",
    "setgroups",
    "getgroups",
    "removexattrat",
    "fchown",
    "mq_timedreceive_time64",
    "mq_notify",
    "process_vm_readv",
    "process_vm_writev",
    "removexattr",
    "quotactl",
    "symlinkat",
    "cacheflush",
    "clock_nanosleep",
    "futex_time64",
    "io_getevents",
    "s390_sthyi",
    "exit_group",
    "lchown",
    "munlock",
    "io_pgetevents_time64",
    "ssetmask",
    "sgetmask",
    "uselib",
    "pipe2",
    "vfork",
    "uretprobe",
    "adjtimex",
    "shmget",
    "ftruncate64",
    "request_key",
    "security",
    "getegid32",
    "multiplexer",
    "swapon",
    "set_mempolicy",
    "get_mempolicy",
    "utimensat_time64",
    "prlimit64",
    "fremovexattr",
    "get_kernel_syms",
    "futex_wait",
    "afs_syscall",
    "unlinkat",
    "stat64",
    "statfs64",
    "openat2",
    "lremovexattr",
    "symlink",
    "mmap2",
    "arm_sync_file_range",
    "fcntl64",
    "clock_nanosleep_time64",
    "fstat64",
    "fstatfs64",
    "set_robust_list",
    "get_robust_list",
    "chown",
    "epoll_pwait2",
    "fchmodat2",
    "preadv",
    "lstat64",
    "set_thread_area",
    "get_thread_area",
    "geteuid32",
    "fstatat64",
    "open_by_handle_at",
    "break",
    "pread64",
    "spu_run",
    "inotify_rm_watch",
    "mlock2",
    "unlink",
    "breakpoint",
    "eventfd2",
    "vhangup",
    "setgroups32",
    "getgroups32",
    "pwritev",
    "inotify_add_watch",
    "fadvise64",
    "fadvise64_64",
    "futex_wake",
    "renameat2",
    "wait4",
    "setuid32",
    "getuid32",
    "pwrite64",
    "riscv_hwprobe",
    "sys_debug_setcontext",
    "usr32",
    "futex_waitv",
    "arm_fadvise64_64",
    "writev",
    "setgid32",
    "getgid32",
    "add_key",
    "switch_endian",
    "map_shadow_stack",
    "shutdown",
    "bdflush",
    "mq_unlink",
    "fchown32",
    "preadv2",
    "lchown32",
    "umount2",
    "pwritev2",
    "chown32"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct arch_syscall_table wordlist[] =
  {
#line 484 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str0,451,315,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF},
#line 394 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str1,361,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,__PNR_send,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF},
#line 500 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str2,467,43,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF},
#line 486 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str3,453,13,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,__PNR_time,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF},
#line 388 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str4,355,82,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,__PNR_select,SCMP_KV_UNDEF},
#line 185 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str5,152,112,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,__PNR_idle,SCMP_KV_UNDEF},
#line 426 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str6,393,66,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF},
#line 173 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str7,140,147,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF},
#line 358 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str8,325,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF,__PNR_rtas,SCMP_KV_UNDEF},
#line 401 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str9,368,139,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF},
#line 416 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str10,383,71,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF},
#line 418 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str11,385,170,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF},
#line 166 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str12,133,171,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF},
#line 146 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str13,113,50,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF},
#line 334 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str14,301,3,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF},
#line 413 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str15,380,346,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF},
#line 131 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str16,98,118,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF},
#line 498 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str17,465,260,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF},
#line 496 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str18,463,261,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF},
#line 382 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str19,349,351,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF},
#line 374 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str20,341,352,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF},
#line 398 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str21,365,370,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,518,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF},
#line 489 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str22,456,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,__PNR_timerfd,SCMP_KV_UNDEF},
#line 384 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str23,351,156,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF},
#line 378 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str24,345,157,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF},
#line 493 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str25,460,325,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF},
#line 491 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str26,458,326,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF},
#line 490 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str27,457,322,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF},
#line 105 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str28,72,133,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF},
#line 241 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str29,208,447,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,447,SCMP_KV_UNDEF,__PNR_memfd_secret,SCMP_KV_UNDEF},
#line 399 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str30,366,369,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF},
#line 383 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str31,350,154,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF},
#line 375 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str32,342,155,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF},
#line 487 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str33,454,259,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,526,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF},
#line 76 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str34,43,362,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF},
#line 74 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str35,41,6,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF},
#line 200 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str36,167,289,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF},
#line 199 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str37,166,290,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF},
#line 274 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str38,241,144,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF},
#line 336 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str39,303,89,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,__PNR_readdir,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF},
#line 206 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str40,173,117,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,__PNR_ipc,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF},
#line 365 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str41,332,177,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,523,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF},
#line 395 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str42,362,187,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF},
#line 240 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str43,207,356,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF},
#line 304 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str44,271,42,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,__PNR_pipe,SCMP_KV_UNDEF,__PNR_pipe,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,__PNR_pipe,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF},
#line 56 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str45,23,185,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF},
#line 397 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str46,364,345,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,538,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF},
#line 35 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str47,2,33,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,__PNR_access,SCMP_KV_UNDEF,__PNR_access,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,__PNR_access,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF},
#line 80 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str48,47,129,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF},
#line 451 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str49,418,359,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF},
#line 254 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str50,221,21,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF},
#line 72 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str51,39,120,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF},
#line 301 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str52,268,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF,438,SCMP_KV_UNDEF},
#line 322 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str53,289,44,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF,__PNR_prof,SCMP_KV_UNDEF},
#line 243 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str54,210,218,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF},
#line 488 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str55,455,263,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF},
#line 424 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str56,391,75,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,__PNR_setrlimit,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF},
#line 170 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str57,137,76,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,__PNR_getrlimit,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,__PNR_getrlimit,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF},
#line 77 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str58,44,377,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,391,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF},
#line 340 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str59,307,88,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF},
#line 255 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str60,222,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF,442,SCMP_KV_UNDEF},
#line 478 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str61,445,149,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,__PNR__sysctl,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,__PNR__sysctl,SCMP_KV_UNDEF,__PNR__sysctl,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,__PNR__sysctl,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF},
#line 389 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str62,356,394,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF},
#line 198 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str63,165,110,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF,__PNR_iopl,SCMP_KV_UNDEF},
#line 379 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str64,346,161,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF},
#line 195 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str65,162,101,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF,__PNR_ioperm,SCMP_KV_UNDEF},
#line 502 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str66,469,92,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF},
#line 454 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str67,421,313,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF},
#line 318 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str68,285,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF,440,SCMP_KV_UNDEF},
#line 319 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str69,286,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF,448,SCMP_KV_UNDEF},
#line 116 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str70,83,350,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF},
#line 295 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str71,262,29,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,__PNR_pause,SCMP_KV_UNDEF,__PNR_pause,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,__PNR_pause,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF},
#line 410 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str72,377,104,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF},
#line 154 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str73,121,105,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF},
#line 294 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str74,261,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF,428,SCMP_KV_UNDEF},
#line 33 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str75,0,__PNR_accept,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_accept,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_accept,SCMP_KV_UNDEF,__PNR_accept,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF},
#line 356 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str76,323,40,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,__PNR_rmdir,SCMP_KV_UNDEF,__PNR_rmdir,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,__PNR_rmdir,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF,40,SCMP_KV_UNDEF},
#line 270 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str77,237,402,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF,402,SCMP_KV_UNDEF},
#line 288 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str78,255,18,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,__PNR_oldstat,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF},
#line 285 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str79,252,28,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,__PNR_oldfstat,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF},
#line 54 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str80,21,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF,451,SCMP_KV_UNDEF},
#line 98 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str81,65,307,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF},
#line 464 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str82,431,25,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,__PNR_stime,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF},
#line 445 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str83,412,321,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,__PNR_signalfd,SCMP_KV_UNDEF,__PNR_signalfd,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,__PNR_signalfd,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF},
#line 258 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str84,225,125,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF},
#line 132 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str85,99,35,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF,__PNR_ftime,SCMP_KV_UNDEF},
#line 144 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str86,111,141,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,__PNR_getdents,SCMP_KV_UNDEF,__PNR_getdents,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,76,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,__PNR_getdents,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF},
#line 284 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str87,251,34,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,__PNR_nice,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF},
#line 239 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str88,206,375,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,389,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF},
#line 310 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str89,277,168,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,__PNR_poll,SCMP_KV_UNDEF,__PNR_poll,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,__PNR_poll,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF},
#line 161 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str90,128,20,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF},
#line 414 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str91,381,57,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF},
#line 159 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str92,126,132,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF},
#line 242 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str93,209,294,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF},
#line 219 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str94,186,303,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF},
#line 291 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str95,258,295,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF},
#line 286 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str96,253,84,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,__PNR_oldlstat,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF},
#line 84 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str97,51,254,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,__PNR_epoll_create,SCMP_KV_UNDEF,__PNR_epoll_create,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,__PNR_epoll_create,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF},
#line 40 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str98,7,27,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,__PNR_alarm,SCMP_KV_UNDEF,__PNR_alarm,SCMP_KV_UNDEF,__PNR_alarm,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,__PNR_alarm,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF},
#line 52 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str99,19,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF,__PNR_cachectl,SCMP_KV_UNDEF},
#line 377 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str100,344,160,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF},
#line 391 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str101,358,__PNR_semop,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF,__PNR_semop,SCMP_KV_UNDEF},
#line 386 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str102,353,354,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF},
#line 323 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str103,290,98,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF,__PNR_profil,SCMP_KV_UNDEF},
#line 357 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str104,324,386,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,387,SCMP_KV_UNDEF,387,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,387,SCMP_KV_UNDEF},
#line 370 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str105,337,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_write,SCMP_KV_UNDEF},
#line 369 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str106,336,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,__PNR_s390_pci_mmio_read,SCMP_KV_UNDEF},
#line 162 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str107,129,188,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,__PNR_getpmsg,SCMP_KV_UNDEF},
#line 495 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str108,462,262,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF},
#line 257 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str109,224,317,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,533,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF},
#line 306 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str110,273,217,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF},
#line 106 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str111,73,94,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF},
#line 444 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str112,411,48,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,__PNR_signal,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF},
#line 273 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str113,240,400,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF,400,SCMP_KV_UNDEF},
#line 85 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str114,52,329,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF},
#line 458 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str115,425,106,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,__PNR_stat,SCMP_KV_UNDEF,__PNR_stat,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,__PNR_stat,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF},
#line 102 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str116,69,324,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF},
#line 363 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str117,330,173,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,513,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF},
#line 460 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str118,427,99,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,43,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF},
#line 485 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str119,452,270,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF},
#line 87 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str120,54,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF,__PNR_epoll_ctl_old,SCMP_KV_UNDEF},
#line 177 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str121,144,224,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF},
#line 403 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str122,370,138,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,120,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF},
#line 422 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str123,389,70,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF},
#line 420 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str124,387,164,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF},
#line 168 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str125,135,165,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,118,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF},
#line 148 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str126,115,49,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF},
#line 41 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str127,8,384,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF,__PNR_arch_prctl,SCMP_KV_UNDEF},
#line 453 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str128,420,360,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF},
#line 163 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str129,130,64,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF},
#line 121 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str130,88,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF,431,SCMP_KV_UNDEF},
#line 364 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str131,331,179,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF},
#line 157 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str132,124,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF,__PNR_getpagesize,SCMP_KV_UNDEF},
#line 480 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str133,447,135,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,139,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,__PNR_sysfs,SCMP_KV_UNDEF,__PNR_sysfs,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,__PNR_sysfs,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF},
#line 465 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str134,432,31,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF,__PNR_stty,SCMP_KV_UNDEF},
#line 184 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str135,151,32,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF,__PNR_gtty,SCMP_KV_UNDEF},
#line 473 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str136,440,36,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF},
#line 476 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str137,443,344,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF},
#line 360 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str138,327,176,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,522,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF},
#line 73 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str139,40,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,435,SCMP_KV_UNDEF,__PNR_clone3,SCMP_KV_UNDEF},
#line 452 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str140,419,102,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,__PNR_socketcall,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF},
#line 359 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str141,326,174,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,512,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF},
#line 86 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str142,53,255,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,21,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF},
#line 311 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str143,278,309,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF},
#line 409 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str144,376,74,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF},
#line 107 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str145,74,306,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF},
#line 381 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str146,348,241,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF},
#line 373 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str147,340,242,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF},
#line 290 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str148,257,5,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,__PNR_open,SCMP_KV_UNDEF,__PNR_open,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,__PNR_open,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF},
#line 385 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str149,352,158,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF},
#line 81 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str150,48,41,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF,41,SCMP_KV_UNDEF},
#line 298 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str151,265,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF,__PNR_pciconfig_write,SCMP_KV_UNDEF},
#line 296 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str152,263,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF,__PNR_pciconfig_iobase,SCMP_KV_UNDEF},
#line 297 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str153,264,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF,__PNR_pciconfig_read,SCMP_KV_UNDEF},
#line 36 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str154,3,51,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF},
#line 376 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str155,343,159,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF},
#line 126 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str156,93,108,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF},
#line 172 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str157,139,77,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,165,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF},
#line 47 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str158,14,361,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF},
#line 129 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str159,96,100,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF},
#line 251 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str160,218,90,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,__PNR_mmap,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF},
#line 303 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str161,270,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF,424,SCMP_KV_UNDEF},
#line 78 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str162,45,8,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,__PNR_creat,SCMP_KV_UNDEF,__PNR_creat,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,__PNR_creat,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF},
#line 499 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str163,466,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF,__PNR_timer_settime64,SCMP_KV_UNDEF,409,SCMP_KV_UNDEF},
#line 497 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str164,464,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF,__PNR_timer_gettime64,SCMP_KV_UNDEF,408,SCMP_KV_UNDEF},
#line 400 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str165,367,121,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF,121,SCMP_KV_UNDEF},
#line 281 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str166,248,__PNR_newfstatat,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,__PNR_newfstatat,SCMP_KV_UNDEF},
#line 482 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str167,449,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF},
#line 160 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str168,127,65,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,__PNR_getpgrp,SCMP_KV_UNDEF,__PNR_getpgrp,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,__PNR_getpgrp,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF},
#line 494 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str169,461,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF,__PNR_timerfd_settime64,SCMP_KV_UNDEF,411,SCMP_KV_UNDEF},
#line 492 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str170,459,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF,__PNR_timerfd_gettime64,SCMP_KV_UNDEF,410,SCMP_KV_UNDEF},
#line 75 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str171,42,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF,436,SCMP_KV_UNDEF},
#line 64 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str172,31,266,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF},
#line 70 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str173,37,264,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF},
#line 66 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str174,33,265,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF},
#line 112 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str175,79,55,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF},
#line 474 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str176,441,314,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,__PNR_sync_file_range,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,__PNR_sync_file_range,SCMP_KV_UNDEF,__PNR_sync_file_range,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF},
#line 193 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str177,160,246,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF},
#line 119 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str178,86,2,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,__PNR_fork,SCMP_KV_UNDEF,__PNR_fork,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,__PNR_fork,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF},
#line 302 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str179,269,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF,434,SCMP_KV_UNDEF},
#line 208 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str180,175,__PNR_kexec_file_load,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,__PNR_kexec_file_load,SCMP_KV_UNDEF},
#line 234 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str181,201,107,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,__PNR_lstat,SCMP_KV_UNDEF,__PNR_lstat,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,__PNR_lstat,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF,107,SCMP_KV_UNDEF},
#line 165 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str182,132,355,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF},
#line 380 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str183,347,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF,__PNR_sched_rr_get_interval_time64,SCMP_KV_UNDEF,423,SCMP_KV_UNDEF},
#line 211 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str184,178,37,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF,37,SCMP_KV_UNDEF},
#line 349 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str185,316,38,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,82,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,__PNR_rename,SCMP_KV_UNDEF,__PNR_rename,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,__PNR_rename,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF},
#line 432 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str186,399,23,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,103,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,23,SCMP_KV_UNDEF},
#line 180 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str187,147,24,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF},
#line 366 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str188,333,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF,__PNR_rt_sigtimedwait_time64,SCMP_KV_UNDEF,421,SCMP_KV_UNDEF},
#line 192 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str189,159,54,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,514,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF},
#line 308 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str190,275,382,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF},
#line 48 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str191,15,357,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF},
#line 238 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str192,205,274,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF},
#line 392 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str193,359,__PNR_semtimedop,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF,392,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF,392,SCMP_KV_UNDEF,__PNR_semtimedop,SCMP_KV_UNDEF},
#line 259 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str194,226,56,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,56,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF,__PNR_mpx,SCMP_KV_UNDEF},
#line 326 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str195,293,26,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,521,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF},
#line 246 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str196,213,14,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,__PNR_mknod,SCMP_KV_UNDEF,__PNR_mknod,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,__PNR_mknod,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF},
#line 218 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str197,185,9,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,__PNR_link,SCMP_KV_UNDEF,__PNR_link,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,84,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,__PNR_link,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF},
#line 434 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str198,401,226,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,5,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF},
#line 182 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str199,149,229,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF},
#line 210 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str200,177,288,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF},
#line 143 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str201,110,183,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF},
#line 92 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str202,59,323,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,__PNR_eventfd,SCMP_KV_UNDEF,__PNR_eventfd,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,__PNR_eventfd,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF},
#line 427 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str203,394,366,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,541,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,353,SCMP_KV_UNDEF},
#line 175 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str204,142,365,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,542,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF},
#line 201 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str205,168,245,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,543,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF},
#line 79 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str206,46,127,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,__PNR_create_module,SCMP_KV_UNDEF},
#line 244 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str207,211,39,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,__PNR_mkdir,SCMP_KV_UNDEF,__PNR_mkdir,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,__PNR_mkdir,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF},
#line 523 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str208,490,271,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,__PNR_utimes,SCMP_KV_UNDEF,__PNR_utimes,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,__PNR_utimes,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF},
#line 520 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str209,487,30,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,__PNR_utime,SCMP_KV_UNDEF,__PNR_utime,SCMP_KV_UNDEF,__PNR_utime,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,__PNR_utime,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF},
#line 226 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str210,193,53,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF,__PNR_lock,SCMP_KV_UNDEF},
#line 141 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str211,108,299,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,__PNR_futimesat,SCMP_KV_UNDEF,__PNR_futimesat,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,__PNR_futimesat,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF},
#line 353 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str212,320,0,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF},
#line 191 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str213,158,249,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,3,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF},
#line 361 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str214,328,175,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF},
#line 501 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str215,468,238,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF},
#line 419 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str216,386,210,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,__PNR_setresgid32,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF},
#line 167 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str217,134,211,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_getresgid32,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF},
#line 313 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str218,280,172,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF},
#line 103 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str219,70,338,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF},
#line 117 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str220,84,234,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,13,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF},
#line 345 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str221,312,372,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,519,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF},
#line 350 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str222,317,302,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,38,SCMP_KV_UNDEF,__PNR_renameat,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,__PNR_renameat,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF},
#line 435 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str223,402,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF,463,SCMP_KV_UNDEF},
#line 183 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str224,150,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF,464,SCMP_KV_UNDEF},
#line 209 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str225,176,283,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,528,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF},
#line 268 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str226,235,163,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,25,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF},
#line 247 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str227,214,297,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF},
#line 439 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str228,406,398,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF,398,SCMP_KV_UNDEF},
#line 269 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str229,236,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF,462,SCMP_KV_UNDEF},
#line 227 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str230,194,253,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,110,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF},
#line 390 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str231,357,393,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF},
#line 158 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str232,125,368,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,53,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF},
#line 368 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str233,335,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,__PNR_s390_guarded_storage,SCMP_KV_UNDEF},
#line 245 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str234,212,296,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF},
#line 253 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str235,220,123,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF,__PNR_modify_ldt,SCMP_KV_UNDEF},
#line 362 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str236,329,178,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,524,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,127,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF},
#line 367 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str237,334,335,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,536,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF},
#line 506 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str238,473,58,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF,__PNR_ulimit,SCMP_KV_UNDEF},
#line 405 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str239,372,46,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,144,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF},
#line 150 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str240,117,47,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,104,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,102,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF},
#line 346 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str241,313,257,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF},
#line 343 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str242,310,337,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,537,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF},
#line 123 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str243,90,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF,432,SCMP_KV_UNDEF},
#line 504 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str244,471,__PNR_tuxcall,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF,__PNR_tuxcall,SCMP_KV_UNDEF},
#line 62 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str245,29,343,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,372,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF},
#line 324 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str246,291,308,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF},
#line 527 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str247,494,113,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF,__PNR_vm86old,SCMP_KV_UNDEF},
#line 450 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str248,417,72,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,__PNR_sigsuspend,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF,72,SCMP_KV_UNDEF},
#line 224 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str249,191,233,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF},
#line 271 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str250,238,399,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF},
#line 265 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str251,232,279,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF},
#line 236 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str252,203,219,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF},
#line 309 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str253,276,380,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,394,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,386,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF},
#line 212 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str254,179,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF,445,SCMP_KV_UNDEF},
#line 96 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str255,63,1,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF},
#line 213 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str256,180,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF,444,SCMP_KV_UNDEF},
#line 513 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str257,480,310,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF},
#line 214 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str258,181,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF,446,SCMP_KV_UNDEF},
#line 327 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str259,294,189,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,__PNR_putpmsg,SCMP_KV_UNDEF},
#line 207 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str260,174,349,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,306,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF},
#line 402 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str261,369,216,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,__PNR_setfsgid32,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF},
#line 417 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str262,384,204,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,__PNR_setregid32,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF},
#line 263 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str263,230,280,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF},
#line 483 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str264,450,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF,__PNR_sysmips,SCMP_KV_UNDEF},
#line 282 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str265,249,142,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF,__PNR__newselect,SCMP_KV_UNDEF,142,SCMP_KV_UNDEF},
#line 477 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str266,444,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,0,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF,__PNR_syscall,SCMP_KV_UNDEF},
#line 280 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str267,247,162,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,34,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF},
#line 415 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str268,382,97,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,138,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF},
#line 164 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str269,131,96,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,141,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF},
#line 342 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str270,309,371,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,517,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF},
#line 519 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str271,486,62,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,__PNR_ustat,SCMP_KV_UNDEF,__PNR_ustat,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,133,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,__PNR_ustat,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF},
#line 142 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str272,109,318,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF},
#line 124 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str273,91,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF,430,SCMP_KV_UNDEF},
#line 55 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str274,22,184,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,125,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,106,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF},
#line 58 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str275,25,15,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,__PNR_chmod,SCMP_KV_UNDEF,__PNR_chmod,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,__PNR_chmod,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF},
#line 256 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str276,223,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF,429,SCMP_KV_UNDEF},
#line 447 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str277,414,73,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,__PNR_sigpending,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF},
#line 396 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str278,363,239,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,__PNR_sendfile64,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF},
#line 61 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str279,28,61,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF},
#line 466 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str280,433,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF,__PNR_subpage_prot,SCMP_KV_UNDEF},
#line 431 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str281,398,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,983045,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF,__PNR_set_tls,SCMP_KV_UNDEF},
#line 179 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str282,146,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,983046,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF,__PNR_get_tls,SCMP_KV_UNDEF},
#line 57 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str283,24,12,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF},
#line 174 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str284,141,367,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,51,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,44,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,367,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF},
#line 481 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str285,448,116,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,97,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF},
#line 307 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str286,274,381,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,384,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF,385,SCMP_KV_UNDEF},
#line 99 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str287,66,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF,439,SCMP_KV_UNDEF},
#line 312 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str288,279,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF,__PNR_ppoll_time64,SCMP_KV_UNDEF,414,SCMP_KV_UNDEF},
#line 196 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str289,163,385,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,399,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,__PNR_io_pgetevents,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF,388,SCMP_KV_UNDEF,388,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,__PNR_io_pgetevents,SCMP_KV_UNDEF},
#line 438 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str290,405,396,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,31,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF,396,SCMP_KV_UNDEF},
#line 526 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str291,493,166,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF,__PNR_vm86,SCMP_KV_UNDEF},
#line 371 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str292,338,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,__PNR_s390_runtime_instr,SCMP_KV_UNDEF},
#line 430 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str293,397,79,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,164,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF,79,SCMP_KV_UNDEF},
#line 178 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str294,145,78,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF},
#line 510 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str295,477,122,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,160,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF,122,SCMP_KV_UNDEF},
#line 505 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str296,472,191,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,__PNR_ugetrlimit,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF},
#line 233 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str297,200,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF,460,SCMP_KV_UNDEF},
#line 231 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str298,198,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF,459,SCMP_KV_UNDEF},
#line 51 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str299,18,45,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,12,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF,45,SCMP_KV_UNDEF},
#line 429 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str300,396,258,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,96,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF},
#line 468 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str301,435,115,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,163,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF},
#line 325 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str302,292,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF,__PNR_pselect6_time64,SCMP_KV_UNDEF,413,SCMP_KV_UNDEF},
#line 229 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str303,196,19,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,8,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,62,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF},
#line 225 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str304,192,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF,__PNR__llseek,SCMP_KV_UNDEF,140,SCMP_KV_UNDEF},
#line 118 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str305,85,143,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,71,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF,143,SCMP_KV_UNDEF},
#line 516 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str306,483,374,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,388,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF},
#line 125 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str307,92,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF,433,SCMP_KV_UNDEF},
#line 393 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str308,360,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF,__PNR_semtimedop_time64,SCMP_KV_UNDEF,420,SCMP_KV_UNDEF},
#line 503 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str309,470,193,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,__PNR_truncate64,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF},
#line 202 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str310,169,248,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,544,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,2,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF},
#line 338 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str311,305,305,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,78,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF},
#line 449 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str312,416,119,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,__PNR_sigreturn,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF,119,SCMP_KV_UNDEF},
#line 448 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str313,415,126,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,__PNR_sigprocmask,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF,126,SCMP_KV_UNDEF},
#line 203 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str314,170,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF,426,SCMP_KV_UNDEF},
#line 83 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str315,50,330,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,24,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF},
#line 250 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str316,217,152,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,156,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF},
#line 204 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str317,171,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF,427,SCMP_KV_UNDEF},
#line 133 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str318,100,93,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,77,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,46,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF},
#line 283 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str319,250,169,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,__PNR_nfsservctl,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,173,SCMP_KV_UNDEF,__PNR_nfsservctl,SCMP_KV_UNDEF,__PNR_nfsservctl,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,42,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF,169,SCMP_KV_UNDEF},
#line 232 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str320,199,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF,461,SCMP_KV_UNDEF},
#line 122 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str321,89,228,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF},
#line 115 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str322,82,231,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,193,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF},
#line 90 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str323,57,256,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,__PNR_epoll_wait,SCMP_KV_UNDEF,__PNR_epoll_wait,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,226,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,__PNR_epoll_wait,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,251,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF},
#line 533 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str324,500,4,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,1,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,64,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF},
#line 248 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str325,215,150,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF},
#line 71 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str326,38,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF,__PNR_clock_settime64,SCMP_KV_UNDEF,404,SCMP_KV_UNDEF},
#line 67 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str327,34,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF,__PNR_clock_gettime64,SCMP_KV_UNDEF,403,SCMP_KV_UNDEF},
#line 91 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str328,58,__PNR_epoll_wait_old,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF,__PNR_epoll_wait_old,SCMP_KV_UNDEF},
#line 528 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str329,495,316,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,532,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF},
#line 65 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str330,32,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF,__PNR_clock_getres_time64,SCMP_KV_UNDEF,406,SCMP_KV_UNDEF},
#line 94 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str331,61,11,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,520,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,57,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF},
#line 335 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str332,302,225,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF},
#line 220 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str333,187,363,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,50,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,174,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,49,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,343,SCMP_KV_UNDEF},
#line 421 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str334,388,208,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,__PNR_setresuid32,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF},
#line 169 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str335,136,209,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF,__PNR_getresuid32,SCMP_KV_UNDEF,209,SCMP_KV_UNDEF},
#line 278 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str336,245,91,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF},
#line 521 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str337,488,320,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF},
#line 145 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str338,112,220,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF},
#line 205 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str339,172,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF,425,SCMP_KV_UNDEF},
#line 223 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str340,190,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF,465,SCMP_KV_UNDEF},
#line 44 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str341,11,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF,__PNR_atomic_barrier,SCMP_KV_UNDEF},
#line 455 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str342,422,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF,__PNR_spu_create,SCMP_KV_UNDEF},
#line 230 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str343,197,227,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,6,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,225,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF},
#line 217 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str344,184,230,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,227,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,9,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF},
#line 222 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str345,189,232,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,11,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF},
#line 300 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str346,267,136,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,135,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF,136,SCMP_KV_UNDEF},
#line 412 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str347,379,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF,450,SCMP_KV_UNDEF},
#line 354 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str348,321,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,259,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF,__PNR_riscv_flush_icache,SCMP_KV_UNDEF},
#line 517 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str349,484,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,983043,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF,__PNR_usr26,SCMP_KV_UNDEF},
#line 88 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str350,55,319,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF},
#line 531 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str351,498,284,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,529,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF},
#line 260 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str352,227,282,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,282,SCMP_KV_UNDEF},
#line 188 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str353,155,291,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,__PNR_inotify_init,SCMP_KV_UNDEF,__PNR_inotify_init,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,__PNR_inotify_init,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF},
#line 462 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str354,429,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF,457,SCMP_KV_UNDEF},
#line 442 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str355,409,67,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,__PNR_sigaction,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF},
#line 104 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str356,71,339,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,368,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF},
#line 287 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str357,254,59,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF,__PNR_oldolduname,SCMP_KV_UNDEF},
#line 262 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str358,229,277,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF},
#line 111 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str359,78,298,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,54,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF},
#line 186 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str360,153,128,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,168,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,105,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF,128,SCMP_KV_UNDEF},
#line 45 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str361,12,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF,__PNR_atomic_cmpxchg_32,SCMP_KV_UNDEF},
#line 443 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str362,410,186,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,525,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,129,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,132,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF,186,SCMP_KV_UNDEF},
#line 95 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str363,62,358,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,545,SCMP_KV_UNDEF,387,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,342,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,354,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF},
#line 136 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str364,103,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF,456,SCMP_KV_UNDEF},
#line 189 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str365,156,332,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,26,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF},
#line 82 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str366,49,63,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,33,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,__PNR_dup2,SCMP_KV_UNDEF,__PNR_dup2,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,32,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,__PNR_dup2,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF,63,SCMP_KV_UNDEF},
#line 339 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str367,306,145,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,515,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,65,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF,145,SCMP_KV_UNDEF},
#line 289 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str368,256,109,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,__PNR_olduname,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF},
#line 34 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str369,1,364,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,344,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,358,SCMP_KV_UNDEF},
#line 404 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str370,371,215,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_setfsuid32,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF},
#line 423 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str371,390,203,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF,__PNR_setreuid32,SCMP_KV_UNDEF,203,SCMP_KV_UNDEF},
#line 331 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str372,298,167,SCMP_KV_UNDEF,178,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,187,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,__PNR_query_module,SCMP_KV_UNDEF},
#line 279 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str373,246,341,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,303,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF},
#line 272 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str374,239,401,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,188,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF,401,SCMP_KV_UNDEF},
#line 529 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str375,496,273,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF,__PNR_vserver,SCMP_KV_UNDEF},
#line 341 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str376,308,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,175,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,__PNR_recv,SCMP_KV_UNDEF,350,SCMP_KV_UNDEF},
#line 475 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str377,442,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,308,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,__PNR_sync_file_range2,SCMP_KV_UNDEF,388,SCMP_KV_UNDEF},
#line 446 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str378,413,327,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,355,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,74,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF},
#line 437 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str379,404,397,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,30,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF},
#line 221 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str380,188,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF,458,SCMP_KV_UNDEF},
#line 532 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str381,499,7,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,__PNR_waitpid,SCMP_KV_UNDEF,7,SCMP_KV_UNDEF},
#line 467 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str382,434,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF,__PNR_swapcontext,SCMP_KV_UNDEF},
#line 114 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str383,81,148,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,75,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,73,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF,148,SCMP_KV_UNDEF},
#line 63 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str384,30,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF,__PNR_clock_adjtime64,SCMP_KV_UNDEF,405,SCMP_KV_UNDEF},
#line 344 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str385,311,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF,__PNR_recvmmsg_time64,SCMP_KV_UNDEF,417,SCMP_KV_UNDEF},
#line 333 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str386,300,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF,443,SCMP_KV_UNDEF},
#line 463 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str387,430,383,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,397,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,349,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF,383,SCMP_KV_UNDEF},
#line 135 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str388,102,240,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,98,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF},
#line 266 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str389,233,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF,__PNR_mq_timedsend_time64,SCMP_KV_UNDEF,418,SCMP_KV_UNDEF},
#line 299 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str390,266,336,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,364,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,332,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF},
#line 277 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str391,244,153,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,152,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,157,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,149,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF},
#line 508 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str392,475,22,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,__PNR_umount,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF,22,SCMP_KV_UNDEF},
#line 507 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str393,474,60,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF},
#line 337 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str394,304,85,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,__PNR_readlink,SCMP_KV_UNDEF,__PNR_readlink,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,__PNR_readlink,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF},
#line 407 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str395,374,81,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,116,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,81,SCMP_KV_UNDEF},
#line 152 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str396,119,80,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,158,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,80,SCMP_KV_UNDEF},
#line 348 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str397,315,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF,466,SCMP_KV_UNDEF},
#line 109 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str398,76,95,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,93,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,91,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,55,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,95,SCMP_KV_UNDEF},
#line 264 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str399,231,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF,__PNR_mq_timedreceive_time64,SCMP_KV_UNDEF,419,SCMP_KV_UNDEF},
#line 261 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str400,228,281,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,527,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,184,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF},
#line 320 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str401,287,347,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,539,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,365,SCMP_KV_UNDEF},
#line 321 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str402,288,348,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,540,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,366,SCMP_KV_UNDEF},
#line 347 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str403,314,235,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,14,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF},
#line 332 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str404,299,131,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,172,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,60,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF,131,SCMP_KV_UNDEF},
#line 472 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str405,439,304,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,36,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF},
#line 53 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str406,20,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,983042,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,__PNR_cacheflush,SCMP_KV_UNDEF,123,SCMP_KV_UNDEF},
#line 68 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str407,35,267,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,115,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF},
#line 137 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str408,104,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF,__PNR_futex_time64,SCMP_KV_UNDEF,422,SCMP_KV_UNDEF},
#line 194 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str409,161,247,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,208,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,4,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF},
#line 372 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str410,339,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,__PNR_s390_sthyi,SCMP_KV_UNDEF},
#line 97 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str411,64,252,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,247,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,222,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF},
#line 215 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str412,182,16,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,94,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,__PNR_lchown,SCMP_KV_UNDEF,__PNR_lchown,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,__PNR_lchown,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF},
#line 276 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str413,243,151,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,155,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,147,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF,151,SCMP_KV_UNDEF},
#line 197 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str414,164,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF,__PNR_io_pgetevents_time64,SCMP_KV_UNDEF,416,SCMP_KV_UNDEF},
#line 457 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str415,424,69,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,__PNR_ssetmask,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF},
#line 436 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str416,403,68,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,__PNR_sgetmask,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF},
#line 515 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str417,482,86,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,__PNR_uselib,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF},
#line 305 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str418,272,331,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,313,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF},
#line 524 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str419,491,190,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,113,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,189,SCMP_KV_UNDEF,__PNR_vfork,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF},
#line 514 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str420,481,__PNR_uretprobe,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,335,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF,__PNR_uretprobe,SCMP_KV_UNDEF},
#line 38 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str421,5,124,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,159,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,154,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,171,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF,124,SCMP_KV_UNDEF},
#line 440 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str422,407,395,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,29,SCMP_KV_UNDEF,307,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF,395,SCMP_KV_UNDEF},
#line 134 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str423,101,194,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF,__PNR_ftruncate64,SCMP_KV_UNDEF,194,SCMP_KV_UNDEF},
#line 352 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str424,319,287,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,240,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF},
#line 387 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str425,354,__PNR_security,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,185,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF,__PNR_security,SCMP_KV_UNDEF},
#line 147 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str426,114,202,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,__PNR_getegid32,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF},
#line 275 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str427,242,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF,__PNR_multiplexer,SCMP_KV_UNDEF},
#line 469 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str428,436,87,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,167,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,162,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,224,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF},
#line 411 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str429,378,276,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,238,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,229,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,262,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF},
#line 156 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str430,123,275,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,228,SCMP_KV_UNDEF,232,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF},
#line 522 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str431,489,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF,__PNR_utimensat_time64,SCMP_KV_UNDEF,412,SCMP_KV_UNDEF},
#line 317 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str432,284,340,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,369,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,297,SCMP_KV_UNDEF,302,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,261,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF},
#line 120 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str433,87,237,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,191,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,237,SCMP_KV_UNDEF},
#line 155 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str434,122,130,SCMP_KV_UNDEF,177,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,170,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,130,SCMP_KV_UNDEF,__PNR_get_kernel_syms,SCMP_KV_UNDEF},
#line 138 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str435,105,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF,455,SCMP_KV_UNDEF},
#line 39 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str436,6,137,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,183,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,176,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,137,SCMP_KV_UNDEF,__PNR_afs_syscall,SCMP_KV_UNDEF},
#line 512 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str437,479,301,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,257,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,281,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF,35,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,301,SCMP_KV_UNDEF},
#line 459 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str438,426,195,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,101,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF,__PNR_stat64,SCMP_KV_UNDEF,195,SCMP_KV_UNDEF},
#line 461 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str439,428,268,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,298,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,252,SCMP_KV_UNDEF,__PNR_statfs64,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF},
#line 292 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str440,259,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF,437,SCMP_KV_UNDEF},
#line 228 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str441,195,236,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,190,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,219,SCMP_KV_UNDEF,15,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,234,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF},
#line 471 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str442,438,83,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,88,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,__PNR_symlink,SCMP_KV_UNDEF,__PNR_symlink,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,86,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,__PNR_symlink,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF,83,SCMP_KV_UNDEF},
#line 252 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str443,219,192,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,89,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF,__PNR_mmap2,SCMP_KV_UNDEF,192,SCMP_KV_UNDEF},
#line 43 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str444,10,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF,__PNR_arm_sync_file_range,SCMP_KV_UNDEF},
#line 113 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str445,80,221,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,220,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,204,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,__PNR_fcntl64,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF},
#line 69 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str446,36,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF,__PNR_clock_nanosleep_time64,SCMP_KV_UNDEF,407,SCMP_KV_UNDEF},
#line 127 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str447,94,197,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,112,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF,__PNR_fstat64,SCMP_KV_UNDEF,197,SCMP_KV_UNDEF},
#line 130 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str448,97,269,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,218,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,__PNR_fstatfs64,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,266,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF},
#line 425 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str449,392,311,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,530,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,268,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF,99,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF},
#line 171 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str450,138,312,SCMP_KV_UNDEF,274,SCMP_KV_UNDEF,531,SCMP_KV_UNDEF,339,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,273,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,100,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,305,SCMP_KV_UNDEF,312,SCMP_KV_UNDEF},
#line 59 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str451,26,182,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,92,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,__PNR_chown,SCMP_KV_UNDEF,__PNR_chown,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,202,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,90,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,__PNR_chown,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF},
#line 89 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str452,56,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF,441,SCMP_KV_UNDEF},
#line 108 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str453,75,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF,452,SCMP_KV_UNDEF},
#line 315 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str454,282,333,SCMP_KV_UNDEF,295,SCMP_KV_UNDEF,534,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,289,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,320,SCMP_KV_UNDEF,69,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF},
#line 235 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str455,202,196,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF,__PNR_lstat64,SCMP_KV_UNDEF,196,SCMP_KV_UNDEF},
#line 428 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str456,395,243,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF,283,SCMP_KV_UNDEF,242,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF,__PNR_set_thread_area,SCMP_KV_UNDEF},
#line 176 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str457,143,244,SCMP_KV_UNDEF,211,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,333,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF,__PNR_get_thread_area,SCMP_KV_UNDEF},
#line 149 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str458,116,201,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,__PNR_geteuid32,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF},
#line 128 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str459,95,300,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,__PNR_fstatat64,SCMP_KV_UNDEF,300,SCMP_KV_UNDEF},
#line 293 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str460,260,342,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,341,SCMP_KV_UNDEF,340,SCMP_KV_UNDEF,299,SCMP_KV_UNDEF,304,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,346,SCMP_KV_UNDEF,265,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,336,SCMP_KV_UNDEF,360,SCMP_KV_UNDEF},
#line 49 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str461,16,17,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF,__PNR_break,SCMP_KV_UNDEF},
#line 314 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str462,281,180,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,16,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,108,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,179,SCMP_KV_UNDEF,67,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF},
#line 456 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str463,423,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF,__PNR_spu_run,SCMP_KV_UNDEF},
#line 190 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str464,157,293,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,255,SCMP_KV_UNDEF,318,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,245,SCMP_KV_UNDEF,249,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,271,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,277,SCMP_KV_UNDEF,28,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,292,SCMP_KV_UNDEF},
#line 249 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str465,216,376,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,390,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,375,SCMP_KV_UNDEF,359,SCMP_KV_UNDEF,319,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,345,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF,374,SCMP_KV_UNDEF,379,SCMP_KV_UNDEF},
#line 511 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str466,478,10,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,87,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,__PNR_unlink,SCMP_KV_UNDEF,__PNR_unlink,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,85,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,__PNR_unlink,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF,10,SCMP_KV_UNDEF},
#line 50 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str467,17,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,983041,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF,__PNR_breakpoint,SCMP_KV_UNDEF},
#line 93 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str468,60,328,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,356,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,324,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,284,SCMP_KV_UNDEF,288,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,310,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,314,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,323,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF},
#line 525 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str469,492,111,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,153,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,150,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,58,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF,111,SCMP_KV_UNDEF},
#line 408 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str470,375,206,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF,__PNR_setgroups32,SCMP_KV_UNDEF,206,SCMP_KV_UNDEF},
#line 153 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str471,120,205,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF,__PNR_getgroups32,SCMP_KV_UNDEF,205,SCMP_KV_UNDEF},
#line 329 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str472,296,334,SCMP_KV_UNDEF,296,SCMP_KV_UNDEF,535,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,330,SCMP_KV_UNDEF,331,SCMP_KV_UNDEF,290,SCMP_KV_UNDEF,294,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,70,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,329,SCMP_KV_UNDEF,334,SCMP_KV_UNDEF},
#line 187 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str473,154,292,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,317,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,244,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,27,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF,291,SCMP_KV_UNDEF},
#line 100 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str474,67,250,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,221,SCMP_KV_UNDEF,__PNR_fadvise64,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,246,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,215,SCMP_KV_UNDEF,216,SCMP_KV_UNDEF,__PNR_fadvise64,SCMP_KV_UNDEF,__PNR_fadvise64,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,233,SCMP_KV_UNDEF,223,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,253,SCMP_KV_UNDEF,250,SCMP_KV_UNDEF},
#line 101 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str475,68,272,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,267,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,236,SCMP_KV_UNDEF,254,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,__PNR_fadvise64_64,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF},
#line 140 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str476,107,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF,454,SCMP_KV_UNDEF},
#line 351 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str477,318,353,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,316,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,351,SCMP_KV_UNDEF,311,SCMP_KV_UNDEF,315,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,337,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,357,SCMP_KV_UNDEF,276,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,371,SCMP_KV_UNDEF},
#line 530 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str478,497,114,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,61,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,59,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,260,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF,114,SCMP_KV_UNDEF},
#line 433 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str479,400,213,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF,__PNR_setuid32,SCMP_KV_UNDEF,213,SCMP_KV_UNDEF},
#line 181 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str480,148,199,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF,__PNR_getuid32,SCMP_KV_UNDEF,199,SCMP_KV_UNDEF},
#line 328 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str481,295,181,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,18,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,201,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,17,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,109,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,180,SCMP_KV_UNDEF,68,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF},
#line 355 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str482,322,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,258,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF,__PNR_riscv_hwprobe,SCMP_KV_UNDEF},
#line 479 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str483,446,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,256,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF,__PNR_sys_debug_setcontext,SCMP_KV_UNDEF},
#line 518 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str484,485,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,983044,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF,__PNR_usr32,SCMP_KV_UNDEF},
#line 139 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str485,106,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF,449,SCMP_KV_UNDEF},
#line 42 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str486,9,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,270,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF,__PNR_arm_fadvise64_64,SCMP_KV_UNDEF},
#line 534 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str487,501,146,SCMP_KV_UNDEF,20,SCMP_KV_UNDEF,516,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,19,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,66,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF,146,SCMP_KV_UNDEF},
#line 406 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str488,373,214,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF,__PNR_setgid32,SCMP_KV_UNDEF,214,SCMP_KV_UNDEF},
#line 151 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str489,118,200,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF,__PNR_getgid32,SCMP_KV_UNDEF,200,SCMP_KV_UNDEF},
#line 37 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str490,4,286,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,248,SCMP_KV_UNDEF,309,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,279,SCMP_KV_UNDEF,280,SCMP_KV_UNDEF,239,SCMP_KV_UNDEF,243,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,264,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,269,SCMP_KV_UNDEF,217,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF,285,SCMP_KV_UNDEF},
#line 470 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str491,437,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,363,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF,__PNR_switch_endian,SCMP_KV_UNDEF},
#line 237 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str492,204,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF,453,SCMP_KV_UNDEF},
#line 441 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str493,408,373,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,48,SCMP_KV_UNDEF,293,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,370,SCMP_KV_UNDEF,182,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,47,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,117,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,338,SCMP_KV_UNDEF,210,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF,373,SCMP_KV_UNDEF,352,SCMP_KV_UNDEF},
#line 46 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str494,13,134,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,__PNR_bdflush,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF,134,SCMP_KV_UNDEF},
#line 267 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str495,234,278,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,241,SCMP_KV_UNDEF,275,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,231,SCMP_KV_UNDEF,235,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,230,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,263,SCMP_KV_UNDEF,181,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,272,SCMP_KV_UNDEF,278,SCMP_KV_UNDEF},
#line 110 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str496,77,207,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF,__PNR_fchown32,SCMP_KV_UNDEF,207,SCMP_KV_UNDEF},
#line 316 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str497,283,378,SCMP_KV_UNDEF,327,SCMP_KV_UNDEF,546,SCMP_KV_UNDEF,392,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF,361,SCMP_KV_UNDEF,321,SCMP_KV_UNDEF,325,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,347,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,380,SCMP_KV_UNDEF,286,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF,376,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF},
#line 216 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str498,183,198,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_lchown32,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF},
#line 509 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str499,476,52,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,166,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,161,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,39,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF,52,SCMP_KV_UNDEF},
#line 330 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str500,297,379,SCMP_KV_UNDEF,328,SCMP_KV_UNDEF,547,SCMP_KV_UNDEF,393,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,378,SCMP_KV_UNDEF,362,SCMP_KV_UNDEF,322,SCMP_KV_UNDEF,326,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,348,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,381,SCMP_KV_UNDEF,287,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF,377,SCMP_KV_UNDEF,382,SCMP_KV_UNDEF},
#line 60 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str501,27,212,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,198,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF,__PNR_chown32,SCMP_KV_UNDEF,212,SCMP_KV_UNDEF}
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

          switch (key - 7)
            {
              case 0:
                resword = &wordlist[0];
                goto compare;
              case 6:
                resword = &wordlist[1];
                goto compare;
              case 8:
                resword = &wordlist[2];
                goto compare;
              case 9:
                resword = &wordlist[3];
                goto compare;
              case 10:
                resword = &wordlist[4];
                goto compare;
              case 14:
                resword = &wordlist[5];
                goto compare;
              case 16:
                resword = &wordlist[6];
                goto compare;
              case 17:
                resword = &wordlist[7];
                goto compare;
              case 18:
                resword = &wordlist[8];
                goto compare;
              case 20:
                resword = &wordlist[9];
                goto compare;
              case 21:
                resword = &wordlist[10];
                goto compare;
              case 22:
                resword = &wordlist[11];
                goto compare;
              case 23:
                resword = &wordlist[12];
                goto compare;
              case 24:
                resword = &wordlist[13];
                goto compare;
              case 27:
                resword = &wordlist[14];
                goto compare;
              case 28:
                resword = &wordlist[15];
                goto compare;
              case 39:
                resword = &wordlist[16];
                goto compare;
              case 41:
                resword = &wordlist[17];
                goto compare;
              case 42:
                resword = &wordlist[18];
                goto compare;
              case 43:
                resword = &wordlist[19];
                goto compare;
              case 44:
                resword = &wordlist[20];
                goto compare;
              case 45:
                resword = &wordlist[21];
                goto compare;
              case 46:
                resword = &wordlist[22];
                goto compare;
              case 48:
                resword = &wordlist[23];
                goto compare;
              case 49:
                resword = &wordlist[24];
                goto compare;
              case 51:
                resword = &wordlist[25];
                goto compare;
              case 52:
                resword = &wordlist[26];
                goto compare;
              case 54:
                resword = &wordlist[27];
                goto compare;
              case 57:
                resword = &wordlist[28];
                goto compare;
              case 58:
                resword = &wordlist[29];
                goto compare;
              case 59:
                resword = &wordlist[30];
                goto compare;
              case 62:
                resword = &wordlist[31];
                goto compare;
              case 63:
                resword = &wordlist[32];
                goto compare;
              case 65:
                resword = &wordlist[33];
                goto compare;
              case 66:
                resword = &wordlist[34];
                goto compare;
              case 67:
                resword = &wordlist[35];
                goto compare;
              case 71:
                resword = &wordlist[36];
                goto compare;
              case 72:
                resword = &wordlist[37];
                goto compare;
              case 73:
                resword = &wordlist[38];
                goto compare;
              case 77:
                resword = &wordlist[39];
                goto compare;
              case 78:
                resword = &wordlist[40];
                goto compare;
              case 79:
                resword = &wordlist[41];
                goto compare;
              case 80:
                resword = &wordlist[42];
                goto compare;
              case 81:
                resword = &wordlist[43];
                goto compare;
              case 82:
                resword = &wordlist[44];
                goto compare;
              case 84:
                resword = &wordlist[45];
                goto compare;
              case 85:
                resword = &wordlist[46];
                goto compare;
              case 87:
                resword = &wordlist[47];
                goto compare;
              case 89:
                resword = &wordlist[48];
                goto compare;
              case 90:
                resword = &wordlist[49];
                goto compare;
              case 93:
                resword = &wordlist[50];
                goto compare;
              case 95:
                resword = &wordlist[51];
                goto compare;
              case 96:
                resword = &wordlist[52];
                goto compare;
              case 97:
                resword = &wordlist[53];
                goto compare;
              case 102:
                resword = &wordlist[54];
                goto compare;
              case 106:
                resword = &wordlist[55];
                goto compare;
              case 114:
                resword = &wordlist[56];
                goto compare;
              case 115:
                resword = &wordlist[57];
                goto compare;
              case 120:
                resword = &wordlist[58];
                goto compare;
              case 123:
                resword = &wordlist[59];
                goto compare;
              case 124:
                resword = &wordlist[60];
                goto compare;
              case 126:
                resword = &wordlist[61];
                goto compare;
              case 128:
                resword = &wordlist[62];
                goto compare;
              case 130:
                resword = &wordlist[63];
                goto compare;
              case 131:
                resword = &wordlist[64];
                goto compare;
              case 136:
                resword = &wordlist[65];
                goto compare;
              case 137:
                resword = &wordlist[66];
                goto compare;
              case 144:
                resword = &wordlist[67];
                goto compare;
              case 150:
                resword = &wordlist[68];
                goto compare;
              case 151:
                resword = &wordlist[69];
                goto compare;
              case 153:
                resword = &wordlist[70];
                goto compare;
              case 154:
                resword = &wordlist[71];
                goto compare;
              case 156:
                resword = &wordlist[72];
                goto compare;
              case 157:
                resword = &wordlist[73];
                goto compare;
              case 158:
                resword = &wordlist[74];
                goto compare;
              case 160:
                resword = &wordlist[75];
                goto compare;
              case 164:
                resword = &wordlist[76];
                goto compare;
              case 165:
                resword = &wordlist[77];
                goto compare;
              case 168:
                resword = &wordlist[78];
                goto compare;
              case 170:
                resword = &wordlist[79];
                goto compare;
              case 171:
                resword = &wordlist[80];
                goto compare;
              case 172:
                resword = &wordlist[81];
                goto compare;
              case 188:
                resword = &wordlist[82];
                goto compare;
              case 189:
                resword = &wordlist[83];
                goto compare;
              case 192:
                resword = &wordlist[84];
                goto compare;
              case 193:
                resword = &wordlist[85];
                goto compare;
              case 194:
                resword = &wordlist[86];
                goto compare;
              case 195:
                resword = &wordlist[87];
                goto compare;
              case 197:
                resword = &wordlist[88];
                goto compare;
              case 198:
                resword = &wordlist[89];
                goto compare;
              case 201:
                resword = &wordlist[90];
                goto compare;
              case 202:
                resword = &wordlist[91];
                goto compare;
              case 203:
                resword = &wordlist[92];
                goto compare;
              case 204:
                resword = &wordlist[93];
                goto compare;
              case 205:
                resword = &wordlist[94];
                goto compare;
              case 207:
                resword = &wordlist[95];
                goto compare;
              case 208:
                resword = &wordlist[96];
                goto compare;
              case 209:
                resword = &wordlist[97];
                goto compare;
              case 216:
                resword = &wordlist[98];
                goto compare;
              case 217:
                resword = &wordlist[99];
                goto compare;
              case 218:
                resword = &wordlist[100];
                goto compare;
              case 219:
                resword = &wordlist[101];
                goto compare;
              case 220:
                resword = &wordlist[102];
                goto compare;
              case 221:
                resword = &wordlist[103];
                goto compare;
              case 223:
                resword = &wordlist[104];
                goto compare;
              case 224:
                resword = &wordlist[105];
                goto compare;
              case 226:
                resword = &wordlist[106];
                goto compare;
              case 228:
                resword = &wordlist[107];
                goto compare;
              case 229:
                resword = &wordlist[108];
                goto compare;
              case 230:
                resword = &wordlist[109];
                goto compare;
              case 231:
                resword = &wordlist[110];
                goto compare;
              case 232:
                resword = &wordlist[111];
                goto compare;
              case 233:
                resword = &wordlist[112];
                goto compare;
              case 234:
                resword = &wordlist[113];
                goto compare;
              case 235:
                resword = &wordlist[114];
                goto compare;
              case 237:
                resword = &wordlist[115];
                goto compare;
              case 240:
                resword = &wordlist[116];
                goto compare;
              case 241:
                resword = &wordlist[117];
                goto compare;
              case 244:
                resword = &wordlist[118];
                goto compare;
              case 247:
                resword = &wordlist[119];
                goto compare;
              case 251:
                resword = &wordlist[120];
                goto compare;
              case 257:
                resword = &wordlist[121];
                goto compare;
              case 259:
                resword = &wordlist[122];
                goto compare;
              case 260:
                resword = &wordlist[123];
                goto compare;
              case 261:
                resword = &wordlist[124];
                goto compare;
              case 262:
                resword = &wordlist[125];
                goto compare;
              case 263:
                resword = &wordlist[126];
                goto compare;
              case 267:
                resword = &wordlist[127];
                goto compare;
              case 272:
                resword = &wordlist[128];
                goto compare;
              case 275:
                resword = &wordlist[129];
                goto compare;
              case 277:
                resword = &wordlist[130];
                goto compare;
              case 278:
                resword = &wordlist[131];
                goto compare;
              case 280:
                resword = &wordlist[132];
                goto compare;
              case 282:
                resword = &wordlist[133];
                goto compare;
              case 285:
                resword = &wordlist[134];
                goto compare;
              case 286:
                resword = &wordlist[135];
                goto compare;
              case 289:
                resword = &wordlist[136];
                goto compare;
              case 292:
                resword = &wordlist[137];
                goto compare;
              case 295:
                resword = &wordlist[138];
                goto compare;
              case 296:
                resword = &wordlist[139];
                goto compare;
              case 299:
                resword = &wordlist[140];
                goto compare;
              case 301:
                resword = &wordlist[141];
                goto compare;
              case 303:
                resword = &wordlist[142];
                goto compare;
              case 305:
                resword = &wordlist[143];
                goto compare;
              case 306:
                resword = &wordlist[144];
                goto compare;
              case 308:
                resword = &wordlist[145];
                goto compare;
              case 309:
                resword = &wordlist[146];
                goto compare;
              case 310:
                resword = &wordlist[147];
                goto compare;
              case 312:
                resword = &wordlist[148];
                goto compare;
              case 313:
                resword = &wordlist[149];
                goto compare;
              case 314:
                resword = &wordlist[150];
                goto compare;
              case 317:
                resword = &wordlist[151];
                goto compare;
              case 318:
                resword = &wordlist[152];
                goto compare;
              case 319:
                resword = &wordlist[153];
                goto compare;
              case 320:
                resword = &wordlist[154];
                goto compare;
              case 323:
                resword = &wordlist[155];
                goto compare;
              case 324:
                resword = &wordlist[156];
                goto compare;
              case 329:
                resword = &wordlist[157];
                goto compare;
              case 330:
                resword = &wordlist[158];
                goto compare;
              case 331:
                resword = &wordlist[159];
                goto compare;
              case 332:
                resword = &wordlist[160];
                goto compare;
              case 343:
                resword = &wordlist[161];
                goto compare;
              case 344:
                resword = &wordlist[162];
                goto compare;
              case 347:
                resword = &wordlist[163];
                goto compare;
              case 348:
                resword = &wordlist[164];
                goto compare;
              case 349:
                resword = &wordlist[165];
                goto compare;
              case 350:
                resword = &wordlist[166];
                goto compare;
              case 351:
                resword = &wordlist[167];
                goto compare;
              case 355:
                resword = &wordlist[168];
                goto compare;
              case 357:
                resword = &wordlist[169];
                goto compare;
              case 358:
                resword = &wordlist[170];
                goto compare;
              case 359:
                resword = &wordlist[171];
                goto compare;
              case 360:
                resword = &wordlist[172];
                goto compare;
              case 362:
                resword = &wordlist[173];
                goto compare;
              case 363:
                resword = &wordlist[174];
                goto compare;
              case 369:
                resword = &wordlist[175];
                goto compare;
              case 371:
                resword = &wordlist[176];
                goto compare;
              case 372:
                resword = &wordlist[177];
                goto compare;
              case 373:
                resword = &wordlist[178];
                goto compare;
              case 376:
                resword = &wordlist[179];
                goto compare;
              case 377:
                resword = &wordlist[180];
                goto compare;
              case 380:
                resword = &wordlist[181];
                goto compare;
              case 381:
                resword = &wordlist[182];
                goto compare;
              case 383:
                resword = &wordlist[183];
                goto compare;
              case 384:
                resword = &wordlist[184];
                goto compare;
              case 386:
                resword = &wordlist[185];
                goto compare;
              case 390:
                resword = &wordlist[186];
                goto compare;
              case 391:
                resword = &wordlist[187];
                goto compare;
              case 392:
                resword = &wordlist[188];
                goto compare;
              case 393:
                resword = &wordlist[189];
                goto compare;
              case 394:
                resword = &wordlist[190];
                goto compare;
              case 395:
                resword = &wordlist[191];
                goto compare;
              case 396:
                resword = &wordlist[192];
                goto compare;
              case 397:
                resword = &wordlist[193];
                goto compare;
              case 399:
                resword = &wordlist[194];
                goto compare;
              case 401:
                resword = &wordlist[195];
                goto compare;
              case 402:
                resword = &wordlist[196];
                goto compare;
              case 406:
                resword = &wordlist[197];
                goto compare;
              case 407:
                resword = &wordlist[198];
                goto compare;
              case 408:
                resword = &wordlist[199];
                goto compare;
              case 410:
                resword = &wordlist[200];
                goto compare;
              case 416:
                resword = &wordlist[201];
                goto compare;
              case 419:
                resword = &wordlist[202];
                goto compare;
              case 420:
                resword = &wordlist[203];
                goto compare;
              case 421:
                resword = &wordlist[204];
                goto compare;
              case 422:
                resword = &wordlist[205];
                goto compare;
              case 423:
                resword = &wordlist[206];
                goto compare;
              case 425:
                resword = &wordlist[207];
                goto compare;
              case 427:
                resword = &wordlist[208];
                goto compare;
              case 428:
                resword = &wordlist[209];
                goto compare;
              case 429:
                resword = &wordlist[210];
                goto compare;
              case 431:
                resword = &wordlist[211];
                goto compare;
              case 432:
                resword = &wordlist[212];
                goto compare;
              case 433:
                resword = &wordlist[213];
                goto compare;
              case 438:
                resword = &wordlist[214];
                goto compare;
              case 441:
                resword = &wordlist[215];
                goto compare;
              case 443:
                resword = &wordlist[216];
                goto compare;
              case 444:
                resword = &wordlist[217];
                goto compare;
              case 454:
                resword = &wordlist[218];
                goto compare;
              case 456:
                resword = &wordlist[219];
                goto compare;
              case 460:
                resword = &wordlist[220];
                goto compare;
              case 463:
                resword = &wordlist[221];
                goto compare;
              case 465:
                resword = &wordlist[222];
                goto compare;
              case 467:
                resword = &wordlist[223];
                goto compare;
              case 468:
                resword = &wordlist[224];
                goto compare;
              case 469:
                resword = &wordlist[225];
                goto compare;
              case 470:
                resword = &wordlist[226];
                goto compare;
              case 478:
                resword = &wordlist[227];
                goto compare;
              case 479:
                resword = &wordlist[228];
                goto compare;
              case 480:
                resword = &wordlist[229];
                goto compare;
              case 481:
                resword = &wordlist[230];
                goto compare;
              case 482:
                resword = &wordlist[231];
                goto compare;
              case 483:
                resword = &wordlist[232];
                goto compare;
              case 484:
                resword = &wordlist[233];
                goto compare;
              case 485:
                resword = &wordlist[234];
                goto compare;
              case 488:
                resword = &wordlist[235];
                goto compare;
              case 489:
                resword = &wordlist[236];
                goto compare;
              case 490:
                resword = &wordlist[237];
                goto compare;
              case 491:
                resword = &wordlist[238];
                goto compare;
              case 495:
                resword = &wordlist[239];
                goto compare;
              case 496:
                resword = &wordlist[240];
                goto compare;
              case 497:
                resword = &wordlist[241];
                goto compare;
              case 503:
                resword = &wordlist[242];
                goto compare;
              case 504:
                resword = &wordlist[243];
                goto compare;
              case 507:
                resword = &wordlist[244];
                goto compare;
              case 508:
                resword = &wordlist[245];
                goto compare;
              case 509:
                resword = &wordlist[246];
                goto compare;
              case 513:
                resword = &wordlist[247];
                goto compare;
              case 514:
                resword = &wordlist[248];
                goto compare;
              case 516:
                resword = &wordlist[249];
                goto compare;
              case 519:
                resword = &wordlist[250];
                goto compare;
              case 524:
                resword = &wordlist[251];
                goto compare;
              case 526:
                resword = &wordlist[252];
                goto compare;
              case 527:
                resword = &wordlist[253];
                goto compare;
              case 529:
                resword = &wordlist[254];
                goto compare;
              case 530:
                resword = &wordlist[255];
                goto compare;
              case 533:
                resword = &wordlist[256];
                goto compare;
              case 535:
                resword = &wordlist[257];
                goto compare;
              case 537:
                resword = &wordlist[258];
                goto compare;
              case 538:
                resword = &wordlist[259];
                goto compare;
              case 540:
                resword = &wordlist[260];
                goto compare;
              case 542:
                resword = &wordlist[261];
                goto compare;
              case 543:
                resword = &wordlist[262];
                goto compare;
              case 545:
                resword = &wordlist[263];
                goto compare;
              case 547:
                resword = &wordlist[264];
                goto compare;
              case 549:
                resword = &wordlist[265];
                goto compare;
              case 550:
                resword = &wordlist[266];
                goto compare;
              case 551:
                resword = &wordlist[267];
                goto compare;
              case 553:
                resword = &wordlist[268];
                goto compare;
              case 554:
                resword = &wordlist[269];
                goto compare;
              case 555:
                resword = &wordlist[270];
                goto compare;
              case 559:
                resword = &wordlist[271];
                goto compare;
              case 560:
                resword = &wordlist[272];
                goto compare;
              case 562:
                resword = &wordlist[273];
                goto compare;
              case 563:
                resword = &wordlist[274];
                goto compare;
              case 564:
                resword = &wordlist[275];
                goto compare;
              case 568:
                resword = &wordlist[276];
                goto compare;
              case 577:
                resword = &wordlist[277];
                goto compare;
              case 581:
                resword = &wordlist[278];
                goto compare;
              case 583:
                resword = &wordlist[279];
                goto compare;
              case 584:
                resword = &wordlist[280];
                goto compare;
              case 585:
                resword = &wordlist[281];
                goto compare;
              case 586:
                resword = &wordlist[282];
                goto compare;
              case 587:
                resword = &wordlist[283];
                goto compare;
              case 588:
                resword = &wordlist[284];
                goto compare;
              case 594:
                resword = &wordlist[285];
                goto compare;
              case 596:
                resword = &wordlist[286];
                goto compare;
              case 597:
                resword = &wordlist[287];
                goto compare;
              case 601:
                resword = &wordlist[288];
                goto compare;
              case 602:
                resword = &wordlist[289];
                goto compare;
              case 605:
                resword = &wordlist[290];
                goto compare;
              case 606:
                resword = &wordlist[291];
                goto compare;
              case 607:
                resword = &wordlist[292];
                goto compare;
              case 609:
                resword = &wordlist[293];
                goto compare;
              case 610:
                resword = &wordlist[294];
                goto compare;
              case 614:
                resword = &wordlist[295];
                goto compare;
              case 615:
                resword = &wordlist[296];
                goto compare;
              case 616:
                resword = &wordlist[297];
                goto compare;
              case 617:
                resword = &wordlist[298];
                goto compare;
              case 620:
                resword = &wordlist[299];
                goto compare;
              case 621:
                resword = &wordlist[300];
                goto compare;
              case 626:
                resword = &wordlist[301];
                goto compare;
              case 627:
                resword = &wordlist[302];
                goto compare;
              case 628:
                resword = &wordlist[303];
                goto compare;
              case 629:
                resword = &wordlist[304];
                goto compare;
              case 633:
                resword = &wordlist[305];
                goto compare;
              case 634:
                resword = &wordlist[306];
                goto compare;
              case 636:
                resword = &wordlist[307];
                goto compare;
              case 637:
                resword = &wordlist[308];
                goto compare;
              case 638:
                resword = &wordlist[309];
                goto compare;
              case 640:
                resword = &wordlist[310];
                goto compare;
              case 641:
                resword = &wordlist[311];
                goto compare;
              case 642:
                resword = &wordlist[312];
                goto compare;
              case 646:
                resword = &wordlist[313];
                goto compare;
              case 648:
                resword = &wordlist[314];
                goto compare;
              case 649:
                resword = &wordlist[315];
                goto compare;
              case 650:
                resword = &wordlist[316];
                goto compare;
              case 651:
                resword = &wordlist[317];
                goto compare;
              case 654:
                resword = &wordlist[318];
                goto compare;
              case 656:
                resword = &wordlist[319];
                goto compare;
              case 658:
                resword = &wordlist[320];
                goto compare;
              case 659:
                resword = &wordlist[321];
                goto compare;
              case 660:
                resword = &wordlist[322];
                goto compare;
              case 661:
                resword = &wordlist[323];
                goto compare;
              case 662:
                resword = &wordlist[324];
                goto compare;
              case 667:
                resword = &wordlist[325];
                goto compare;
              case 668:
                resword = &wordlist[326];
                goto compare;
              case 669:
                resword = &wordlist[327];
                goto compare;
              case 670:
                resword = &wordlist[328];
                goto compare;
              case 672:
                resword = &wordlist[329];
                goto compare;
              case 673:
                resword = &wordlist[330];
                goto compare;
              case 675:
                resword = &wordlist[331];
                goto compare;
              case 676:
                resword = &wordlist[332];
                goto compare;
              case 679:
                resword = &wordlist[333];
                goto compare;
              case 682:
                resword = &wordlist[334];
                goto compare;
              case 683:
                resword = &wordlist[335];
                goto compare;
              case 689:
                resword = &wordlist[336];
                goto compare;
              case 695:
                resword = &wordlist[337];
                goto compare;
              case 697:
                resword = &wordlist[338];
                goto compare;
              case 700:
                resword = &wordlist[339];
                goto compare;
              case 701:
                resword = &wordlist[340];
                goto compare;
              case 702:
                resword = &wordlist[341];
                goto compare;
              case 706:
                resword = &wordlist[342];
                goto compare;
              case 715:
                resword = &wordlist[343];
                goto compare;
              case 716:
                resword = &wordlist[344];
                goto compare;
              case 720:
                resword = &wordlist[345];
                goto compare;
              case 721:
                resword = &wordlist[346];
                goto compare;
              case 723:
                resword = &wordlist[347];
                goto compare;
              case 724:
                resword = &wordlist[348];
                goto compare;
              case 729:
                resword = &wordlist[349];
                goto compare;
              case 730:
                resword = &wordlist[350];
                goto compare;
              case 732:
                resword = &wordlist[351];
                goto compare;
              case 733:
                resword = &wordlist[352];
                goto compare;
              case 734:
                resword = &wordlist[353];
                goto compare;
              case 735:
                resword = &wordlist[354];
                goto compare;
              case 737:
                resword = &wordlist[355];
                goto compare;
              case 738:
                resword = &wordlist[356];
                goto compare;
              case 739:
                resword = &wordlist[357];
                goto compare;
              case 743:
                resword = &wordlist[358];
                goto compare;
              case 747:
                resword = &wordlist[359];
                goto compare;
              case 749:
                resword = &wordlist[360];
                goto compare;
              case 751:
                resword = &wordlist[361];
                goto compare;
              case 752:
                resword = &wordlist[362];
                goto compare;
              case 754:
                resword = &wordlist[363];
                goto compare;
              case 756:
                resword = &wordlist[364];
                goto compare;
              case 762:
                resword = &wordlist[365];
                goto compare;
              case 767:
                resword = &wordlist[366];
                goto compare;
              case 771:
                resword = &wordlist[367];
                goto compare;
              case 772:
                resword = &wordlist[368];
                goto compare;
              case 773:
                resword = &wordlist[369];
                goto compare;
              case 781:
                resword = &wordlist[370];
                goto compare;
              case 782:
                resword = &wordlist[371];
                goto compare;
              case 786:
                resword = &wordlist[372];
                goto compare;
              case 787:
                resword = &wordlist[373];
                goto compare;
              case 790:
                resword = &wordlist[374];
                goto compare;
              case 792:
                resword = &wordlist[375];
                goto compare;
              case 793:
                resword = &wordlist[376];
                goto compare;
              case 794:
                resword = &wordlist[377];
                goto compare;
              case 797:
                resword = &wordlist[378];
                goto compare;
              case 798:
                resword = &wordlist[379];
                goto compare;
              case 801:
                resword = &wordlist[380];
                goto compare;
              case 806:
                resword = &wordlist[381];
                goto compare;
              case 807:
                resword = &wordlist[382];
                goto compare;
              case 808:
                resword = &wordlist[383];
                goto compare;
              case 814:
                resword = &wordlist[384];
                goto compare;
              case 815:
                resword = &wordlist[385];
                goto compare;
              case 817:
                resword = &wordlist[386];
                goto compare;
              case 820:
                resword = &wordlist[387];
                goto compare;
              case 830:
                resword = &wordlist[388];
                goto compare;
              case 832:
                resword = &wordlist[389];
                goto compare;
              case 834:
                resword = &wordlist[390];
                goto compare;
              case 836:
                resword = &wordlist[391];
                goto compare;
              case 838:
                resword = &wordlist[392];
                goto compare;
              case 841:
                resword = &wordlist[393];
                goto compare;
              case 842:
                resword = &wordlist[394];
                goto compare;
              case 845:
                resword = &wordlist[395];
                goto compare;
              case 846:
                resword = &wordlist[396];
                goto compare;
              case 848:
                resword = &wordlist[397];
                goto compare;
              case 852:
                resword = &wordlist[398];
                goto compare;
              case 856:
                resword = &wordlist[399];
                goto compare;
              case 857:
                resword = &wordlist[400];
                goto compare;
              case 858:
                resword = &wordlist[401];
                goto compare;
              case 859:
                resword = &wordlist[402];
                goto compare;
              case 867:
                resword = &wordlist[403];
                goto compare;
              case 870:
                resword = &wordlist[404];
                goto compare;
              case 876:
                resword = &wordlist[405];
                goto compare;
              case 878:
                resword = &wordlist[406];
                goto compare;
              case 884:
                resword = &wordlist[407];
                goto compare;
              case 896:
                resword = &wordlist[408];
                goto compare;
              case 897:
                resword = &wordlist[409];
                goto compare;
              case 898:
                resword = &wordlist[410];
                goto compare;
              case 899:
                resword = &wordlist[411];
                goto compare;
              case 908:
                resword = &wordlist[412];
                goto compare;
              case 914:
                resword = &wordlist[413];
                goto compare;
              case 915:
                resword = &wordlist[414];
                goto compare;
              case 923:
                resword = &wordlist[415];
                goto compare;
              case 924:
                resword = &wordlist[416];
                goto compare;
              case 925:
                resword = &wordlist[417];
                goto compare;
              case 929:
                resword = &wordlist[418];
                goto compare;
              case 941:
                resword = &wordlist[419];
                goto compare;
              case 950:
                resword = &wordlist[420];
                goto compare;
              case 953:
                resword = &wordlist[421];
                goto compare;
              case 959:
                resword = &wordlist[422];
                goto compare;
              case 960:
                resword = &wordlist[423];
                goto compare;
              case 968:
                resword = &wordlist[424];
                goto compare;
              case 969:
                resword = &wordlist[425];
                goto compare;
              case 970:
                resword = &wordlist[426];
                goto compare;
              case 976:
                resword = &wordlist[427];
                goto compare;
              case 982:
                resword = &wordlist[428];
                goto compare;
              case 994:
                resword = &wordlist[429];
                goto compare;
              case 995:
                resword = &wordlist[430];
                goto compare;
              case 1008:
                resword = &wordlist[431];
                goto compare;
              case 1011:
                resword = &wordlist[432];
                goto compare;
              case 1012:
                resword = &wordlist[433];
                goto compare;
              case 1026:
                resword = &wordlist[434];
                goto compare;
              case 1027:
                resword = &wordlist[435];
                goto compare;
              case 1037:
                resword = &wordlist[436];
                goto compare;
              case 1038:
                resword = &wordlist[437];
                goto compare;
              case 1046:
                resword = &wordlist[438];
                goto compare;
              case 1053:
                resword = &wordlist[439];
                goto compare;
              case 1056:
                resword = &wordlist[440];
                goto compare;
              case 1068:
                resword = &wordlist[441];
                goto compare;
              case 1077:
                resword = &wordlist[442];
                goto compare;
              case 1108:
                resword = &wordlist[443];
                goto compare;
              case 1109:
                resword = &wordlist[444];
                goto compare;
              case 1117:
                resword = &wordlist[445];
                goto compare;
              case 1124:
                resword = &wordlist[446];
                goto compare;
              case 1133:
                resword = &wordlist[447];
                goto compare;
              case 1140:
                resword = &wordlist[448];
                goto compare;
              case 1142:
                resword = &wordlist[449];
                goto compare;
              case 1143:
                resword = &wordlist[450];
                goto compare;
              case 1144:
                resword = &wordlist[451];
                goto compare;
              case 1155:
                resword = &wordlist[452];
                goto compare;
              case 1157:
                resword = &wordlist[453];
                goto compare;
              case 1167:
                resword = &wordlist[454];
                goto compare;
              case 1189:
                resword = &wordlist[455];
                goto compare;
              case 1192:
                resword = &wordlist[456];
                goto compare;
              case 1193:
                resword = &wordlist[457];
                goto compare;
              case 1209:
                resword = &wordlist[458];
                goto compare;
              case 1214:
                resword = &wordlist[459];
                goto compare;
              case 1222:
                resword = &wordlist[460];
                goto compare;
              case 1225:
                resword = &wordlist[461];
                goto compare;
              case 1227:
                resword = &wordlist[462];
                goto compare;
              case 1228:
                resword = &wordlist[463];
                goto compare;
              case 1233:
                resword = &wordlist[464];
                goto compare;
              case 1234:
                resword = &wordlist[465];
                goto compare;
              case 1239:
                resword = &wordlist[466];
                goto compare;
              case 1240:
                resword = &wordlist[467];
                goto compare;
              case 1263:
                resword = &wordlist[468];
                goto compare;
              case 1268:
                resword = &wordlist[469];
                goto compare;
              case 1271:
                resword = &wordlist[470];
                goto compare;
              case 1272:
                resword = &wordlist[471];
                goto compare;
              case 1286:
                resword = &wordlist[472];
                goto compare;
              case 1292:
                resword = &wordlist[473];
                goto compare;
              case 1299:
                resword = &wordlist[474];
                goto compare;
              case 1302:
                resword = &wordlist[475];
                goto compare;
              case 1306:
                resword = &wordlist[476];
                goto compare;
              case 1314:
                resword = &wordlist[477];
                goto compare;
              case 1328:
                resword = &wordlist[478];
                goto compare;
              case 1336:
                resword = &wordlist[479];
                goto compare;
              case 1337:
                resword = &wordlist[480];
                goto compare;
              case 1346:
                resword = &wordlist[481];
                goto compare;
              case 1364:
                resword = &wordlist[482];
                goto compare;
              case 1387:
                resword = &wordlist[483];
                goto compare;
              case 1392:
                resword = &wordlist[484];
                goto compare;
              case 1402:
                resword = &wordlist[485];
                goto compare;
              case 1405:
                resword = &wordlist[486];
                goto compare;
              case 1409:
                resword = &wordlist[487];
                goto compare;
              case 1441:
                resword = &wordlist[488];
                goto compare;
              case 1442:
                resword = &wordlist[489];
                goto compare;
              case 1456:
                resword = &wordlist[490];
                goto compare;
              case 1502:
                resword = &wordlist[491];
                goto compare;
              case 1522:
                resword = &wordlist[492];
                goto compare;
              case 1524:
                resword = &wordlist[493];
                goto compare;
              case 1563:
                resword = &wordlist[494];
                goto compare;
              case 1601:
                resword = &wordlist[495];
                goto compare;
              case 1617:
                resword = &wordlist[496];
                goto compare;
              case 1642:
                resword = &wordlist[497];
                goto compare;
              case 1673:
                resword = &wordlist[498];
                goto compare;
              case 1687:
                resword = &wordlist[499];
                goto compare;
              case 1761:
                resword = &wordlist[500];
                goto compare;
              case 1909:
                resword = &wordlist[501];
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
#line 535 "syscalls.perf"


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
