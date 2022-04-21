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
    TOTAL_KEYWORDS = 481,
    MIN_WORD_LENGTH = 3,
    MAX_WORD_LENGTH = 28,
    MIN_HASH_VALUE = 13,
    MAX_HASH_VALUE = 1680
  };

/* maximum key range = 1668, duplicates = 0 */

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
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,    7,
       110,  492,  323, 1681,   22,    3, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681,    5,   87,  101,  436,   18,
         3,    4,    4,    2,  420,    5,  587,  222,   91,   20,
       130,   38,   57,  397,    8,    1,    2,  320,  210,  275,
       393,  151,   72, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681, 1681,
      1681, 1681, 1681, 1681, 1681, 1681, 1681
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
    char stringpool_str2[sizeof("rtas")];
    char stringpool_str3[sizeof("times")];
    char stringpool_str4[sizeof("time")];
    char stringpool_str5[sizeof("idle")];
    char stringpool_str6[sizeof("read")];
    char stringpool_str7[sizeof("setsid")];
    char stringpool_str8[sizeof("getsid")];
    char stringpool_str9[sizeof("setfsgid")];
    char stringpool_str10[sizeof("getegid")];
    char stringpool_str11[sizeof("setregid")];
    char stringpool_str12[sizeof("setresgid")];
    char stringpool_str13[sizeof("getresgid")];
    char stringpool_str14[sizeof("timerfd")];
    char stringpool_str15[sizeof("select")];
    char stringpool_str16[sizeof("sendmsg")];
    char stringpool_str17[sizeof("readdir")];
    char stringpool_str18[sizeof("timer_settime")];
    char stringpool_str19[sizeof("timer_gettime")];
    char stringpool_str20[sizeof("setns")];
    char stringpool_str21[sizeof("timerfd_settime")];
    char stringpool_str22[sizeof("timerfd_gettime")];
    char stringpool_str23[sizeof("fchdir")];
    char stringpool_str24[sizeof("sched_setattr")];
    char stringpool_str25[sizeof("sched_getattr")];
    char stringpool_str26[sizeof("sendmmsg")];
    char stringpool_str27[sizeof("rt_sigtimedwait")];
    char stringpool_str28[sizeof("sched_setscheduler")];
    char stringpool_str29[sizeof("sched_getscheduler")];
    char stringpool_str30[sizeof("timerfd_create")];
    char stringpool_str31[sizeof("timer_create")];
    char stringpool_str32[sizeof("memfd_secret")];
    char stringpool_str33[sizeof("sched_setparam")];
    char stringpool_str34[sizeof("sched_getparam")];
    char stringpool_str35[sizeof("pipe")];
    char stringpool_str36[sizeof("prof")];
    char stringpool_str37[sizeof("memfd_create")];
    char stringpool_str38[sizeof("ipc")];
    char stringpool_str39[sizeof("fsync")];
    char stringpool_str40[sizeof("mincore")];
    char stringpool_str41[sizeof("sendto")];
    char stringpool_str42[sizeof("pidfd_getfd")];
    char stringpool_str43[sizeof("delete_module")];
    char stringpool_str44[sizeof("msync")];
    char stringpool_str45[sizeof("ioperm")];
    char stringpool_str46[sizeof("mount")];
    char stringpool_str47[sizeof("ioprio_set")];
    char stringpool_str48[sizeof("ioprio_get")];
    char stringpool_str49[sizeof("reboot")];
    char stringpool_str50[sizeof("process_madvise")];
    char stringpool_str51[sizeof("process_mrelease")];
    char stringpool_str52[sizeof("close")];
    char stringpool_str53[sizeof("sendfile")];
    char stringpool_str54[sizeof("connect")];
    char stringpool_str55[sizeof("mount_setattr")];
    char stringpool_str56[sizeof("access")];
    char stringpool_str57[sizeof("capset")];
    char stringpool_str58[sizeof("timer_delete")];
    char stringpool_str59[sizeof("setrlimit")];
    char stringpool_str60[sizeof("getrlimit")];
    char stringpool_str61[sizeof("setresgid32")];
    char stringpool_str62[sizeof("getresgid32")];
    char stringpool_str63[sizeof("socket")];
    char stringpool_str64[sizeof("stime")];
    char stringpool_str65[sizeof("nice")];
    char stringpool_str66[sizeof("ftime")];
    char stringpool_str67[sizeof("pselect6")];
    char stringpool_str68[sizeof("getdents")];
    char stringpool_str69[sizeof("iopl")];
    char stringpool_str70[sizeof("clone")];
    char stringpool_str71[sizeof("sysfs")];
    char stringpool_str72[sizeof("sched_rr_get_interval")];
    char stringpool_str73[sizeof("msgsnd")];
    char stringpool_str74[sizeof("syncfs")];
    char stringpool_str75[sizeof("open_tree")];
    char stringpool_str76[sizeof("mprotect")];
    char stringpool_str77[sizeof("pause")];
    char stringpool_str78[sizeof("rt_sigreturn")];
    char stringpool_str79[sizeof("sync")];
    char stringpool_str80[sizeof("timer_getoverrun")];
    char stringpool_str81[sizeof("semop")];
    char stringpool_str82[sizeof("truncate")];
    char stringpool_str83[sizeof("membarrier")];
    char stringpool_str84[sizeof("seccomp")];
    char stringpool_str85[sizeof("accept")];
    char stringpool_str86[sizeof("sched_get_priority_min")];
    char stringpool_str87[sizeof("set_tls")];
    char stringpool_str88[sizeof("get_tls")];
    char stringpool_str89[sizeof("semctl")];
    char stringpool_str90[sizeof("fchmod")];
    char stringpool_str91[sizeof("sched_yield")];
    char stringpool_str92[sizeof("sched_setaffinity")];
    char stringpool_str93[sizeof("sched_getaffinity")];
    char stringpool_str94[sizeof("migrate_pages")];
    char stringpool_str95[sizeof("poll")];
    char stringpool_str96[sizeof("fsconfig")];
    char stringpool_str97[sizeof("msgctl")];
    char stringpool_str98[sizeof("_sysctl")];
    char stringpool_str99[sizeof("syslog")];
    char stringpool_str100[sizeof("pivot_root")];
    char stringpool_str101[sizeof("epoll_create")];
    char stringpool_str102[sizeof("set_tid_address")];
    char stringpool_str103[sizeof("rt_sigpending")];
    char stringpool_str104[sizeof("epoll_create1")];
    char stringpool_str105[sizeof("faccessat")];
    char stringpool_str106[sizeof("stty")];
    char stringpool_str107[sizeof("gtty")];
    char stringpool_str108[sizeof("setuid")];
    char stringpool_str109[sizeof("getuid")];
    char stringpool_str110[sizeof("alarm")];
    char stringpool_str111[sizeof("move_pages")];
    char stringpool_str112[sizeof("openat")];
    char stringpool_str113[sizeof("oldstat")];
    char stringpool_str114[sizeof("oldfstat")];
    char stringpool_str115[sizeof("signalfd")];
    char stringpool_str116[sizeof("copy_file_range")];
    char stringpool_str117[sizeof("set_mempolicy_home_node")];
    char stringpool_str118[sizeof("vm86")];
    char stringpool_str119[sizeof("profil")];
    char stringpool_str120[sizeof("io_destroy")];
    char stringpool_str121[sizeof("oldlstat")];
    char stringpool_str122[sizeof("open")];
    char stringpool_str123[sizeof("eventfd")];
    char stringpool_str124[sizeof("pciconfig_read")];
    char stringpool_str125[sizeof("pciconfig_write")];
    char stringpool_str126[sizeof("pciconfig_iobase")];
    char stringpool_str127[sizeof("rt_sigaction")];
    char stringpool_str128[sizeof("sync_file_range")];
    char stringpool_str129[sizeof("setxattr")];
    char stringpool_str130[sizeof("getxattr")];
    char stringpool_str131[sizeof("pipe2")];
    char stringpool_str132[sizeof("spu_create")];
    char stringpool_str133[sizeof("getcwd")];
    char stringpool_str134[sizeof("sethostname")];
    char stringpool_str135[sizeof("linkat")];
    char stringpool_str136[sizeof("fallocate")];
    char stringpool_str137[sizeof("epoll_ctl_old")];
    char stringpool_str138[sizeof("arch_prctl")];
    char stringpool_str139[sizeof("fchmodat")];
    char stringpool_str140[sizeof("mknod")];
    char stringpool_str141[sizeof("pidfd_open")];
    char stringpool_str142[sizeof("pidfd_send_signal")];
    char stringpool_str143[sizeof("socketpair")];
    char stringpool_str144[sizeof("recvmsg")];
    char stringpool_str145[sizeof("setdomainname")];
    char stringpool_str146[sizeof("ppoll")];
    char stringpool_str147[sizeof("getrandom")];
    char stringpool_str148[sizeof("stat")];
    char stringpool_str149[sizeof("signal")];
    char stringpool_str150[sizeof("statfs")];
    char stringpool_str151[sizeof("faccessat2")];
    char stringpool_str152[sizeof("cachectl")];
    char stringpool_str153[sizeof("recvmmsg")];
    char stringpool_str154[sizeof("kill")];
    char stringpool_str155[sizeof("gettid")];
    char stringpool_str156[sizeof("fanotify_init")];
    char stringpool_str157[sizeof("setfsuid")];
    char stringpool_str158[sizeof("geteuid")];
    char stringpool_str159[sizeof("setreuid")];
    char stringpool_str160[sizeof("setresuid")];
    char stringpool_str161[sizeof("getresuid")];
    char stringpool_str162[sizeof("sysmips")];
    char stringpool_str163[sizeof("newfstatat")];
    char stringpool_str164[sizeof("rt_sigsuspend")];
    char stringpool_str165[sizeof("rt_sigprocmask")];
    char stringpool_str166[sizeof("fork")];
    char stringpool_str167[sizeof("clock_getres")];
    char stringpool_str168[sizeof("clock_settime")];
    char stringpool_str169[sizeof("clock_gettime")];
    char stringpool_str170[sizeof("timer_settime64")];
    char stringpool_str171[sizeof("timer_gettime64")];
    char stringpool_str172[sizeof("kexec_file_load")];
    char stringpool_str173[sizeof("timerfd_settime64")];
    char stringpool_str174[sizeof("timerfd_gettime64")];
    char stringpool_str175[sizeof("close_range")];
    char stringpool_str176[sizeof("vm86old")];
    char stringpool_str177[sizeof("mremap")];
    char stringpool_str178[sizeof("dup")];
    char stringpool_str179[sizeof("recvfrom")];
    char stringpool_str180[sizeof("epoll_ctl")];
    char stringpool_str181[sizeof("pkey_free")];
    char stringpool_str182[sizeof("sync_file_range2")];
    char stringpool_str183[sizeof("rt_sigtimedwait_time64")];
    char stringpool_str184[sizeof("setsockopt")];
    char stringpool_str185[sizeof("getsockopt")];
    char stringpool_str186[sizeof("set_mempolicy")];
    char stringpool_str187[sizeof("get_mempolicy")];
    char stringpool_str188[sizeof("sched_rr_get_interval_time64")];
    char stringpool_str189[sizeof("link")];
    char stringpool_str190[sizeof("mknodat")];
    char stringpool_str191[sizeof("madvise")];
    char stringpool_str192[sizeof("rseq")];
    char stringpool_str193[sizeof("keyctl")];
    char stringpool_str194[sizeof("getpid")];
    char stringpool_str195[sizeof("setpgid")];
    char stringpool_str196[sizeof("getpgid")];
    char stringpool_str197[sizeof("io_uring_enter")];
    char stringpool_str198[sizeof("io_uring_register")];
    char stringpool_str199[sizeof("tkill")];
    char stringpool_str200[sizeof("shmdt")];
    char stringpool_str201[sizeof("getpmsg")];
    char stringpool_str202[sizeof("semget")];
    char stringpool_str203[sizeof("readv")];
    char stringpool_str204[sizeof("setgid")];
    char stringpool_str205[sizeof("getgid")];
    char stringpool_str206[sizeof("acct")];
    char stringpool_str207[sizeof("lock")];
    char stringpool_str208[sizeof("socketcall")];
    char stringpool_str209[sizeof("getrusage")];
    char stringpool_str210[sizeof("vserver")];
    char stringpool_str211[sizeof("fstat")];
    char stringpool_str212[sizeof("bind")];
    char stringpool_str213[sizeof("sched_get_priority_max")];
    char stringpool_str214[sizeof("msgget")];
    char stringpool_str215[sizeof("fstatfs")];
    char stringpool_str216[sizeof("setresuid32")];
    char stringpool_str217[sizeof("getresuid32")];
    char stringpool_str218[sizeof("semtimedop")];
    char stringpool_str219[sizeof("openat2")];
    char stringpool_str220[sizeof("utimes")];
    char stringpool_str221[sizeof("utime")];
    char stringpool_str222[sizeof("msgrcv")];
    char stringpool_str223[sizeof("pselect6_time64")];
    char stringpool_str224[sizeof("sendfile64")];
    char stringpool_str225[sizeof("creat")];
    char stringpool_str226[sizeof("mpx")];
    char stringpool_str227[sizeof("io_uring_setup")];
    char stringpool_str228[sizeof("get_kernel_syms")];
    char stringpool_str229[sizeof("getppid")];
    char stringpool_str230[sizeof("nfsservctl")];
    char stringpool_str231[sizeof("rename")];
    char stringpool_str232[sizeof("ftruncate")];
    char stringpool_str233[sizeof("eventfd2")];
    char stringpool_str234[sizeof("io_cancel")];
    char stringpool_str235[sizeof("io_setup")];
    char stringpool_str236[sizeof("pkey_mprotect")];
    char stringpool_str237[sizeof("kexec_load")];
    char stringpool_str238[sizeof("mmap")];
    char stringpool_str239[sizeof("bpf")];
    char stringpool_str240[sizeof("recv")];
    char stringpool_str241[sizeof("getdents64")];
    char stringpool_str242[sizeof("mbind")];
    char stringpool_str243[sizeof("chmod")];
    char stringpool_str244[sizeof("nanosleep")];
    char stringpool_str245[sizeof("process_vm_readv")];
    char stringpool_str246[sizeof("process_vm_writev")];
    char stringpool_str247[sizeof("fsmount")];
    char stringpool_str248[sizeof("arm_sync_file_range")];
    char stringpool_str249[sizeof("ptrace")];
    char stringpool_str250[sizeof("truncate64")];
    char stringpool_str251[sizeof("fcntl")];
    char stringpool_str252[sizeof("getsockname")];
    char stringpool_str253[sizeof("sigsuspend")];
    char stringpool_str254[sizeof("getpgrp")];
    char stringpool_str255[sizeof("syscall")];
    char stringpool_str256[sizeof("lstat")];
    char stringpool_str257[sizeof("security")];
    char stringpool_str258[sizeof("chroot")];
    char stringpool_str259[sizeof("lseek")];
    char stringpool_str260[sizeof("flock")];
    char stringpool_str261[sizeof("create_module")];
    char stringpool_str262[sizeof("ioctl")];
    char stringpool_str263[sizeof("capget")];
    char stringpool_str264[sizeof("restart_syscall")];
    char stringpool_str265[sizeof("ulimit")];
    char stringpool_str266[sizeof("_llseek")];
    char stringpool_str267[sizeof("settimeofday")];
    char stringpool_str268[sizeof("gettimeofday")];
    char stringpool_str269[sizeof("perf_event_open")];
    char stringpool_str270[sizeof("mlock2")];
    char stringpool_str271[sizeof("mlock")];
    char stringpool_str272[sizeof("fanotify_mark")];
    char stringpool_str273[sizeof("afs_syscall")];
    char stringpool_str274[sizeof("epoll_wait")];
    char stringpool_str275[sizeof("prctl")];
    char stringpool_str276[sizeof("epoll_wait_old")];
    char stringpool_str277[sizeof("readlinkat")];
    char stringpool_str278[sizeof("unshare")];
    char stringpool_str279[sizeof("renameat")];
    char stringpool_str280[sizeof("move_mount")];
    char stringpool_str281[sizeof("personality")];
    char stringpool_str282[sizeof("landlock_add_rule")];
    char stringpool_str283[sizeof("ppoll_time64")];
    char stringpool_str284[sizeof("uname")];
    char stringpool_str285[sizeof("landlock_create_ruleset")];
    char stringpool_str286[sizeof("landlock_restrict_self")];
    char stringpool_str287[sizeof("fchownat")];
    char stringpool_str288[sizeof("olduname")];
    char stringpool_str289[sizeof("sigreturn")];
    char stringpool_str290[sizeof("s390_pci_mmio_read")];
    char stringpool_str291[sizeof("s390_pci_mmio_write")];
    char stringpool_str292[sizeof("shmctl")];
    char stringpool_str293[sizeof("flistxattr")];
    char stringpool_str294[sizeof("write")];
    char stringpool_str295[sizeof("request_key")];
    char stringpool_str296[sizeof("fchown")];
    char stringpool_str297[sizeof("epoll_pwait")];
    char stringpool_str298[sizeof("execve")];
    char stringpool_str299[sizeof("remap_file_pages")];
    char stringpool_str300[sizeof("setpriority")];
    char stringpool_str301[sizeof("getpriority")];
    char stringpool_str302[sizeof("setfsgid32")];
    char stringpool_str303[sizeof("setregid32")];
    char stringpool_str304[sizeof("rmdir")];
    char stringpool_str305[sizeof("inotify_init")];
    char stringpool_str306[sizeof("lookup_dcookie")];
    char stringpool_str307[sizeof("symlinkat")];
    char stringpool_str308[sizeof("inotify_init1")];
    char stringpool_str309[sizeof("setitimer")];
    char stringpool_str310[sizeof("getitimer")];
    char stringpool_str311[sizeof("readahead")];
    char stringpool_str312[sizeof("_newselect")];
    char stringpool_str313[sizeof("mmap2")];
    char stringpool_str314[sizeof("vfork")];
    char stringpool_str315[sizeof("getpeername")];
    char stringpool_str316[sizeof("riscv_flush_icache")];
    char stringpool_str317[sizeof("brk")];
    char stringpool_str318[sizeof("fsopen")];
    char stringpool_str319[sizeof("recvmmsg_time64")];
    char stringpool_str320[sizeof("io_pgetevents")];
    char stringpool_str321[sizeof("splice")];
    char stringpool_str322[sizeof("finit_module")];
    char stringpool_str323[sizeof("clock_settime64")];
    char stringpool_str324[sizeof("clock_gettime64")];
    char stringpool_str325[sizeof("listen")];
    char stringpool_str326[sizeof("sigpending")];
    char stringpool_str327[sizeof("clock_getres_time64")];
    char stringpool_str328[sizeof("umount")];
    char stringpool_str329[sizeof("munmap")];
    char stringpool_str330[sizeof("readlink")];
    char stringpool_str331[sizeof("kcmp")];
    char stringpool_str332[sizeof("io_getevents")];
    char stringpool_str333[sizeof("utimensat")];
    char stringpool_str334[sizeof("llistxattr")];
    char stringpool_str335[sizeof("pkey_alloc")];
    char stringpool_str336[sizeof("tuxcall")];
    char stringpool_str337[sizeof("lchown")];
    char stringpool_str338[sizeof("getcpu")];
    char stringpool_str339[sizeof("waitid")];
    char stringpool_str340[sizeof("mlockall")];
    char stringpool_str341[sizeof("exit")];
    char stringpool_str342[sizeof("name_to_handle_at")];
    char stringpool_str343[sizeof("add_key")];
    char stringpool_str344[sizeof("execveat")];
    char stringpool_str345[sizeof("swapoff")];
    char stringpool_str346[sizeof("epoll_pwait2")];
    char stringpool_str347[sizeof("semtimedop_time64")];
    char stringpool_str348[sizeof("spu_run")];
    char stringpool_str349[sizeof("getegid32")];
    char stringpool_str350[sizeof("set_thread_area")];
    char stringpool_str351[sizeof("get_thread_area")];
    char stringpool_str352[sizeof("fdatasync")];
    char stringpool_str353[sizeof("vmsplice")];
    char stringpool_str354[sizeof("symlink")];
    char stringpool_str355[sizeof("ustat")];
    char stringpool_str356[sizeof("waitpid")];
    char stringpool_str357[sizeof("clock_nanosleep")];
    char stringpool_str358[sizeof("sigaction")];
    char stringpool_str359[sizeof("ugetrlimit")];
    char stringpool_str360[sizeof("mq_timedsend")];
    char stringpool_str361[sizeof("umask")];
    char stringpool_str362[sizeof("rt_tgsigqueueinfo")];
    char stringpool_str363[sizeof("rt_sigqueueinfo")];
    char stringpool_str364[sizeof("mq_timedreceive")];
    char stringpool_str365[sizeof("removexattr")];
    char stringpool_str366[sizeof("mq_open")];
    char stringpool_str367[sizeof("renameat2")];
    char stringpool_str368[sizeof("ftruncate64")];
    char stringpool_str369[sizeof("putpmsg")];
    char stringpool_str370[sizeof("mq_notify")];
    char stringpool_str371[sizeof("sigprocmask")];
    char stringpool_str372[sizeof("accept4")];
    char stringpool_str373[sizeof("io_submit")];
    char stringpool_str374[sizeof("oldolduname")];
    char stringpool_str375[sizeof("fsetxattr")];
    char stringpool_str376[sizeof("fgetxattr")];
    char stringpool_str377[sizeof("mkdir")];
    char stringpool_str378[sizeof("userfaultfd")];
    char stringpool_str379[sizeof("shmget")];
    char stringpool_str380[sizeof("init_module")];
    char stringpool_str381[sizeof("arm_fadvise64_64")];
    char stringpool_str382[sizeof("setgroups")];
    char stringpool_str383[sizeof("getgroups")];
    char stringpool_str384[sizeof("usr26")];
    char stringpool_str385[sizeof("shmat")];
    char stringpool_str386[sizeof("munlock")];
    char stringpool_str387[sizeof("usr32")];
    char stringpool_str388[sizeof("tgkill")];
    char stringpool_str389[sizeof("mq_getsetattr")];
    char stringpool_str390[sizeof("sigaltstack")];
    char stringpool_str391[sizeof("swapcontext")];
    char stringpool_str392[sizeof("ssetmask")];
    char stringpool_str393[sizeof("sgetmask")];
    char stringpool_str394[sizeof("signalfd4")];
    char stringpool_str395[sizeof("subpage_prot")];
    char stringpool_str396[sizeof("fremovexattr")];
    char stringpool_str397[sizeof("umount2")];
    char stringpool_str398[sizeof("modify_ldt")];
    char stringpool_str399[sizeof("set_robust_list")];
    char stringpool_str400[sizeof("get_robust_list")];
    char stringpool_str401[sizeof("dup2")];
    char stringpool_str402[sizeof("preadv")];
    char stringpool_str403[sizeof("munlockall")];
    char stringpool_str404[sizeof("lsetxattr")];
    char stringpool_str405[sizeof("lgetxattr")];
    char stringpool_str406[sizeof("listxattr")];
    char stringpool_str407[sizeof("preadv2")];
    char stringpool_str408[sizeof("setuid32")];
    char stringpool_str409[sizeof("getuid32")];
    char stringpool_str410[sizeof("setfsuid32")];
    char stringpool_str411[sizeof("mkdirat")];
    char stringpool_str412[sizeof("setreuid32")];
    char stringpool_str413[sizeof("query_module")];
    char stringpool_str414[sizeof("sysinfo")];
    char stringpool_str415[sizeof("s390_guarded_storage")];
    char stringpool_str416[sizeof("setgroups32")];
    char stringpool_str417[sizeof("getgroups32")];
    char stringpool_str418[sizeof("cacheflush")];
    char stringpool_str419[sizeof("swapon")];
    char stringpool_str420[sizeof("s390_runtime_instr")];
    char stringpool_str421[sizeof("lremovexattr")];
    char stringpool_str422[sizeof("stat64")];
    char stringpool_str423[sizeof("statfs64")];
    char stringpool_str424[sizeof("io_pgetevents_time64")];
    char stringpool_str425[sizeof("sys_debug_setcontext")];
    char stringpool_str426[sizeof("utimensat_time64")];
    char stringpool_str427[sizeof("writev")];
    char stringpool_str428[sizeof("clock_nanosleep_time64")];
    char stringpool_str429[sizeof("chdir")];
    char stringpool_str430[sizeof("clock_adjtime")];
    char stringpool_str431[sizeof("futimesat")];
    char stringpool_str432[sizeof("geteuid32")];
    char stringpool_str433[sizeof("fspick")];
    char stringpool_str434[sizeof("inotify_rm_watch")];
    char stringpool_str435[sizeof("fadvise64")];
    char stringpool_str436[sizeof("fadvise64_64")];
    char stringpool_str437[sizeof("futex_time64")];
    char stringpool_str438[sizeof("s390_sthyi")];
    char stringpool_str439[sizeof("chown")];
    char stringpool_str440[sizeof("fcntl64")];
    char stringpool_str441[sizeof("vhangup")];
    char stringpool_str442[sizeof("mq_timedsend_time64")];
    char stringpool_str443[sizeof("statx")];
    char stringpool_str444[sizeof("fstat64")];
    char stringpool_str445[sizeof("futex")];
    char stringpool_str446[sizeof("mq_timedreceive_time64")];
    char stringpool_str447[sizeof("fstatfs64")];
    char stringpool_str448[sizeof("clone3")];
    char stringpool_str449[sizeof("dup3")];
    char stringpool_str450[sizeof("setgid32")];
    char stringpool_str451[sizeof("getgid32")];
    char stringpool_str452[sizeof("exit_group")];
    char stringpool_str453[sizeof("inotify_add_watch")];
    char stringpool_str454[sizeof("open_by_handle_at")];
    char stringpool_str455[sizeof("pread64")];
    char stringpool_str456[sizeof("fchown32")];
    char stringpool_str457[sizeof("lstat64")];
    char stringpool_str458[sizeof("fstatat64")];
    char stringpool_str459[sizeof("uselib")];
    char stringpool_str460[sizeof("multiplexer")];
    char stringpool_str461[sizeof("adjtimex")];
    char stringpool_str462[sizeof("quotactl_fd")];
    char stringpool_str463[sizeof("lchown32")];
    char stringpool_str464[sizeof("switch_endian")];
    char stringpool_str465[sizeof("shutdown")];
    char stringpool_str466[sizeof("futex_waitv")];
    char stringpool_str467[sizeof("break")];
    char stringpool_str468[sizeof("breakpoint")];
    char stringpool_str469[sizeof("wait4")];
    char stringpool_str470[sizeof("quotactl")];
    char stringpool_str471[sizeof("pwritev")];
    char stringpool_str472[sizeof("prlimit64")];
    char stringpool_str473[sizeof("pwritev2")];
    char stringpool_str474[sizeof("clock_adjtime64")];
    char stringpool_str475[sizeof("mq_unlink")];
    char stringpool_str476[sizeof("unlinkat")];
    char stringpool_str477[sizeof("pwrite64")];
    char stringpool_str478[sizeof("unlink")];
    char stringpool_str479[sizeof("bdflush")];
    char stringpool_str480[sizeof("chown32")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "tee",
    "send",
    "rtas",
    "times",
    "time",
    "idle",
    "read",
    "setsid",
    "getsid",
    "setfsgid",
    "getegid",
    "setregid",
    "setresgid",
    "getresgid",
    "timerfd",
    "select",
    "sendmsg",
    "readdir",
    "timer_settime",
    "timer_gettime",
    "setns",
    "timerfd_settime",
    "timerfd_gettime",
    "fchdir",
    "sched_setattr",
    "sched_getattr",
    "sendmmsg",
    "rt_sigtimedwait",
    "sched_setscheduler",
    "sched_getscheduler",
    "timerfd_create",
    "timer_create",
    "memfd_secret",
    "sched_setparam",
    "sched_getparam",
    "pipe",
    "prof",
    "memfd_create",
    "ipc",
    "fsync",
    "mincore",
    "sendto",
    "pidfd_getfd",
    "delete_module",
    "msync",
    "ioperm",
    "mount",
    "ioprio_set",
    "ioprio_get",
    "reboot",
    "process_madvise",
    "process_mrelease",
    "close",
    "sendfile",
    "connect",
    "mount_setattr",
    "access",
    "capset",
    "timer_delete",
    "setrlimit",
    "getrlimit",
    "setresgid32",
    "getresgid32",
    "socket",
    "stime",
    "nice",
    "ftime",
    "pselect6",
    "getdents",
    "iopl",
    "clone",
    "sysfs",
    "sched_rr_get_interval",
    "msgsnd",
    "syncfs",
    "open_tree",
    "mprotect",
    "pause",
    "rt_sigreturn",
    "sync",
    "timer_getoverrun",
    "semop",
    "truncate",
    "membarrier",
    "seccomp",
    "accept",
    "sched_get_priority_min",
    "set_tls",
    "get_tls",
    "semctl",
    "fchmod",
    "sched_yield",
    "sched_setaffinity",
    "sched_getaffinity",
    "migrate_pages",
    "poll",
    "fsconfig",
    "msgctl",
    "_sysctl",
    "syslog",
    "pivot_root",
    "epoll_create",
    "set_tid_address",
    "rt_sigpending",
    "epoll_create1",
    "faccessat",
    "stty",
    "gtty",
    "setuid",
    "getuid",
    "alarm",
    "move_pages",
    "openat",
    "oldstat",
    "oldfstat",
    "signalfd",
    "copy_file_range",
    "set_mempolicy_home_node",
    "vm86",
    "profil",
    "io_destroy",
    "oldlstat",
    "open",
    "eventfd",
    "pciconfig_read",
    "pciconfig_write",
    "pciconfig_iobase",
    "rt_sigaction",
    "sync_file_range",
    "setxattr",
    "getxattr",
    "pipe2",
    "spu_create",
    "getcwd",
    "sethostname",
    "linkat",
    "fallocate",
    "epoll_ctl_old",
    "arch_prctl",
    "fchmodat",
    "mknod",
    "pidfd_open",
    "pidfd_send_signal",
    "socketpair",
    "recvmsg",
    "setdomainname",
    "ppoll",
    "getrandom",
    "stat",
    "signal",
    "statfs",
    "faccessat2",
    "cachectl",
    "recvmmsg",
    "kill",
    "gettid",
    "fanotify_init",
    "setfsuid",
    "geteuid",
    "setreuid",
    "setresuid",
    "getresuid",
    "sysmips",
    "newfstatat",
    "rt_sigsuspend",
    "rt_sigprocmask",
    "fork",
    "clock_getres",
    "clock_settime",
    "clock_gettime",
    "timer_settime64",
    "timer_gettime64",
    "kexec_file_load",
    "timerfd_settime64",
    "timerfd_gettime64",
    "close_range",
    "vm86old",
    "mremap",
    "dup",
    "recvfrom",
    "epoll_ctl",
    "pkey_free",
    "sync_file_range2",
    "rt_sigtimedwait_time64",
    "setsockopt",
    "getsockopt",
    "set_mempolicy",
    "get_mempolicy",
    "sched_rr_get_interval_time64",
    "link",
    "mknodat",
    "madvise",
    "rseq",
    "keyctl",
    "getpid",
    "setpgid",
    "getpgid",
    "io_uring_enter",
    "io_uring_register",
    "tkill",
    "shmdt",
    "getpmsg",
    "semget",
    "readv",
    "setgid",
    "getgid",
    "acct",
    "lock",
    "socketcall",
    "getrusage",
    "vserver",
    "fstat",
    "bind",
    "sched_get_priority_max",
    "msgget",
    "fstatfs",
    "setresuid32",
    "getresuid32",
    "semtimedop",
    "openat2",
    "utimes",
    "utime",
    "msgrcv",
    "pselect6_time64",
    "sendfile64",
    "creat",
    "mpx",
    "io_uring_setup",
    "get_kernel_syms",
    "getppid",
    "nfsservctl",
    "rename",
    "ftruncate",
    "eventfd2",
    "io_cancel",
    "io_setup",
    "pkey_mprotect",
    "kexec_load",
    "mmap",
    "bpf",
    "recv",
    "getdents64",
    "mbind",
    "chmod",
    "nanosleep",
    "process_vm_readv",
    "process_vm_writev",
    "fsmount",
    "arm_sync_file_range",
    "ptrace",
    "truncate64",
    "fcntl",
    "getsockname",
    "sigsuspend",
    "getpgrp",
    "syscall",
    "lstat",
    "security",
    "chroot",
    "lseek",
    "flock",
    "create_module",
    "ioctl",
    "capget",
    "restart_syscall",
    "ulimit",
    "_llseek",
    "settimeofday",
    "gettimeofday",
    "perf_event_open",
    "mlock2",
    "mlock",
    "fanotify_mark",
    "afs_syscall",
    "epoll_wait",
    "prctl",
    "epoll_wait_old",
    "readlinkat",
    "unshare",
    "renameat",
    "move_mount",
    "personality",
    "landlock_add_rule",
    "ppoll_time64",
    "uname",
    "landlock_create_ruleset",
    "landlock_restrict_self",
    "fchownat",
    "olduname",
    "sigreturn",
    "s390_pci_mmio_read",
    "s390_pci_mmio_write",
    "shmctl",
    "flistxattr",
    "write",
    "request_key",
    "fchown",
    "epoll_pwait",
    "execve",
    "remap_file_pages",
    "setpriority",
    "getpriority",
    "setfsgid32",
    "setregid32",
    "rmdir",
    "inotify_init",
    "lookup_dcookie",
    "symlinkat",
    "inotify_init1",
    "setitimer",
    "getitimer",
    "readahead",
    "_newselect",
    "mmap2",
    "vfork",
    "getpeername",
    "riscv_flush_icache",
    "brk",
    "fsopen",
    "recvmmsg_time64",
    "io_pgetevents",
    "splice",
    "finit_module",
    "clock_settime64",
    "clock_gettime64",
    "listen",
    "sigpending",
    "clock_getres_time64",
    "umount",
    "munmap",
    "readlink",
    "kcmp",
    "io_getevents",
    "utimensat",
    "llistxattr",
    "pkey_alloc",
    "tuxcall",
    "lchown",
    "getcpu",
    "waitid",
    "mlockall",
    "exit",
    "name_to_handle_at",
    "add_key",
    "execveat",
    "swapoff",
    "epoll_pwait2",
    "semtimedop_time64",
    "spu_run",
    "getegid32",
    "set_thread_area",
    "get_thread_area",
    "fdatasync",
    "vmsplice",
    "symlink",
    "ustat",
    "waitpid",
    "clock_nanosleep",
    "sigaction",
    "ugetrlimit",
    "mq_timedsend",
    "umask",
    "rt_tgsigqueueinfo",
    "rt_sigqueueinfo",
    "mq_timedreceive",
    "removexattr",
    "mq_open",
    "renameat2",
    "ftruncate64",
    "putpmsg",
    "mq_notify",
    "sigprocmask",
    "accept4",
    "io_submit",
    "oldolduname",
    "fsetxattr",
    "fgetxattr",
    "mkdir",
    "userfaultfd",
    "shmget",
    "init_module",
    "arm_fadvise64_64",
    "setgroups",
    "getgroups",
    "usr26",
    "shmat",
    "munlock",
    "usr32",
    "tgkill",
    "mq_getsetattr",
    "sigaltstack",
    "swapcontext",
    "ssetmask",
    "sgetmask",
    "signalfd4",
    "subpage_prot",
    "fremovexattr",
    "umount2",
    "modify_ldt",
    "set_robust_list",
    "get_robust_list",
    "dup2",
    "preadv",
    "munlockall",
    "lsetxattr",
    "lgetxattr",
    "listxattr",
    "preadv2",
    "setuid32",
    "getuid32",
    "setfsuid32",
    "mkdirat",
    "setreuid32",
    "query_module",
    "sysinfo",
    "s390_guarded_storage",
    "setgroups32",
    "getgroups32",
    "cacheflush",
    "swapon",
    "s390_runtime_instr",
    "lremovexattr",
    "stat64",
    "statfs64",
    "io_pgetevents_time64",
    "sys_debug_setcontext",
    "utimensat_time64",
    "writev",
    "clock_nanosleep_time64",
    "chdir",
    "clock_adjtime",
    "futimesat",
    "geteuid32",
    "fspick",
    "inotify_rm_watch",
    "fadvise64",
    "fadvise64_64",
    "futex_time64",
    "s390_sthyi",
    "chown",
    "fcntl64",
    "vhangup",
    "mq_timedsend_time64",
    "statx",
    "fstat64",
    "futex",
    "mq_timedreceive_time64",
    "fstatfs64",
    "clone3",
    "dup3",
    "setgid32",
    "getgid32",
    "exit_group",
    "inotify_add_watch",
    "open_by_handle_at",
    "pread64",
    "fchown32",
    "lstat64",
    "fstatat64",
    "uselib",
    "multiplexer",
    "adjtimex",
    "quotactl_fd",
    "lchown32",
    "switch_endian",
    "shutdown",
    "futex_waitv",
    "break",
    "breakpoint",
    "wait4",
    "quotactl",
    "pwritev",
    "prlimit64",
    "pwritev2",
    "clock_adjtime64",
    "mq_unlink",
    "unlinkat",
    "pwrite64",
    "unlink",
    "bdflush",
    "chown32"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct arch_syscall_table wordlist[] =
  {
#line 462 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str0,431,315,276,276,342,77,306,265,269,293,293,284,284,77,308,308},
#line 374 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str1,343,__PNR_send,__PNR_send,__PNR_send,289,__PNR_send,178,__PNR_send,__PNR_send,58,58,334,334,__PNR_send,__PNR_send,__PNR_send},
#line 338 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str2,307,__PNR_rtas,__PNR_rtas,__PNR_rtas,__PNR_rtas,__PNR_rtas,__PNR_rtas,__PNR_rtas,__PNR_rtas,__PNR_rtas,__PNR_rtas,255,255,__PNR_rtas,__PNR_rtas,__PNR_rtas},
#line 478 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str3,447,43,100,100,43,153,43,98,98,43,43,43,43,153,43,43},
#line 464 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str4,433,13,201,201,__PNR_time,__PNR_time,13,__PNR_time,__PNR_time,13,13,13,13,__PNR_time,13,__PNR_time},
#line 174 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str5,143,112,__PNR_idle,__PNR_idle,__PNR_idle,__PNR_idle,112,__PNR_idle,__PNR_idle,__PNR_idle,__PNR_idle,112,112,__PNR_idle,112,112},
#line 316 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str6,285,3,0,0,3,63,3,0,0,3,3,3,3,63,3,3},
#line 406 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str7,375,66,112,112,66,157,66,110,110,66,66,66,66,157,66,66},
#line 163 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str8,132,147,124,124,147,156,151,122,122,147,147,147,147,156,147,147},
#line 381 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str9,350,139,123,123,139,152,139,121,121,139,139,139,139,152,139,216},
#line 137 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str10,106,50,108,108,50,177,50,106,106,50,50,50,50,177,50,202},
#line 396 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str11,365,71,114,114,71,143,71,112,112,71,71,71,71,143,71,204},
#line 398 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str12,367,170,119,119,170,149,190,117,117,170,170,169,169,149,170,210},
#line 156 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str13,125,171,120,120,171,150,191,118,118,171,171,170,170,150,171,211},
#line 467 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str14,436,__PNR_timerfd,__PNR_timerfd,__PNR_timerfd,__PNR_timerfd,__PNR_timerfd,318,277,281,__PNR_timerfd,__PNR_timerfd,__PNR_timerfd,__PNR_timerfd,__PNR_timerfd,317,317},
#line 368 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str15,337,82,23,23,__PNR_select,__PNR_select,__PNR_select,__PNR_select,__PNR_select,__PNR_select,__PNR_select,82,82,__PNR_select,__PNR_select,142},
#line 378 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str16,347,370,46,518,296,211,179,45,45,183,183,341,341,211,370,370},
#line 318 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str17,287,89,__PNR_readdir,__PNR_readdir,__PNR_readdir,__PNR_readdir,89,__PNR_readdir,__PNR_readdir,__PNR_readdir,__PNR_readdir,89,89,__PNR_readdir,89,89},
#line 476 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str18,445,260,223,223,258,110,258,217,221,251,251,241,241,110,255,255},
#line 474 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str19,443,261,224,224,259,108,259,218,222,252,252,242,242,108,256,256},
#line 393 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str20,362,346,308,308,375,268,344,303,308,328,328,350,350,268,339,339},
#line 471 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str21,440,325,286,286,353,86,323,282,286,307,307,311,311,86,320,320},
#line 469 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str22,438,326,287,287,354,87,322,281,285,308,308,312,312,87,321,321},
#line 100 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str23,69,133,81,81,133,50,133,79,79,133,133,133,133,50,133,133},
#line 362 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str24,331,351,314,314,380,274,349,309,313,334,334,355,355,274,345,345},
#line 354 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str25,323,352,315,315,381,275,350,310,314,335,335,356,356,275,346,346},
#line 377 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str26,346,345,307,538,374,269,343,302,307,329,329,349,349,269,358,358},
#line 345 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str27,314,177,128,523,177,137,197,126,126,177,177,176,176,137,177,177},
#line 364 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str28,333,156,144,144,156,119,160,141,141,156,156,156,156,119,156,156},
#line 358 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str29,327,157,145,145,157,120,161,142,142,157,157,157,157,120,157,157},
#line 468 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str30,437,322,283,283,350,85,321,280,284,306,306,306,306,85,319,319},
#line 465 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str31,434,259,222,526,257,107,257,216,220,250,250,240,240,107,254,254},
#line 224 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str32,193,447,447,447,__PNR_memfd_secret,447,__PNR_memfd_secret,__PNR_memfd_secret,__PNR_memfd_secret,__PNR_memfd_secret,__PNR_memfd_secret,__PNR_memfd_secret,__PNR_memfd_secret,__PNR_memfd_secret,__PNR_memfd_secret,__PNR_memfd_secret},
#line 363 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str33,332,154,142,142,154,118,158,139,139,154,154,154,154,118,154,154},
#line 355 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str34,324,155,143,143,155,121,159,140,140,155,155,155,155,121,155,155},
#line 286 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str35,255,42,22,22,42,__PNR_pipe,42,21,21,42,42,42,42,__PNR_pipe,42,42},
#line 304 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str36,273,44,__PNR_prof,__PNR_prof,__PNR_prof,__PNR_prof,44,__PNR_prof,__PNR_prof,__PNR_prof,__PNR_prof,44,44,__PNR_prof,__PNR_prof,__PNR_prof},
#line 223 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str37,192,356,319,319,385,279,354,314,318,340,340,360,360,279,350,350},
#line 195 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str38,164,117,__PNR_ipc,__PNR_ipc,__PNR_ipc,__PNR_ipc,117,__PNR_ipc,__PNR_ipc,__PNR_ipc,__PNR_ipc,117,117,__PNR_ipc,117,117},
#line 125 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str39,94,118,74,74,118,82,118,72,72,118,118,118,118,82,118,118},
#line 226 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str40,195,218,27,27,219,232,217,26,26,72,72,206,206,232,218,218},
#line 379 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str41,348,369,44,44,290,206,180,43,43,82,82,335,335,206,369,369},
#line 283 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str42,252,438,438,438,438,438,438,438,438,438,438,438,438,438,438,438},
#line 75 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str43,44,129,176,176,129,106,129,169,169,129,129,129,129,106,129,129},
#line 256 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str44,225,144,26,26,144,227,144,25,25,144,144,144,144,227,144,144},
#line 184 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str45,153,101,173,173,__PNR_ioperm,__PNR_ioperm,101,__PNR_ioperm,__PNR_ioperm,__PNR_ioperm,__PNR_ioperm,101,101,__PNR_ioperm,101,__PNR_ioperm},
#line 237 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str46,206,21,165,165,21,40,21,160,160,21,21,21,21,40,21,21},
#line 189 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str47,158,289,251,251,314,30,314,273,277,267,267,273,273,30,282,282},
#line 188 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str48,157,290,252,252,315,31,315,274,278,268,268,274,274,31,283,283},
#line 322 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str49,291,88,169,169,88,142,88,164,164,88,88,88,88,142,88,88},
#line 300 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str50,269,440,440,440,440,440,440,440,440,440,440,440,440,440,440,440},
#line 301 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str51,270,448,448,448,448,448,448,448,448,448,448,448,448,448,448,448},
#line 69 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str52,38,6,3,3,6,57,6,3,3,6,6,6,6,57,6,6},
#line 375 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str53,344,187,40,40,187,71,207,39,39,122,122,186,186,71,187,187},
#line 71 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str54,40,362,42,42,283,203,170,41,41,31,31,328,328,203,362,362},
#line 238 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str55,207,442,442,442,442,442,442,442,442,442,442,442,442,442,442,442},
#line 33 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str56,2,33,21,21,33,__PNR_access,33,20,20,33,33,33,33,__PNR_access,33,33},
#line 51 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str57,20,185,126,126,185,91,205,124,124,107,107,184,184,91,185,185},
#line 466 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str58,435,263,226,226,261,111,261,220,224,254,254,244,244,111,258,258},
#line 404 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str59,373,75,160,160,75,164,75,155,155,75,75,75,75,164,75,75},
#line 160 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str60,129,76,97,97,__PNR_getrlimit,163,76,95,95,76,76,76,76,163,76,191},
#line 399 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str61,368,210,__PNR_setresgid32,__PNR_setresgid32,210,__PNR_setresgid32,__PNR_setresgid32,__PNR_setresgid32,__PNR_setresgid32,__PNR_setresgid32,__PNR_setresgid32,__PNR_setresgid32,__PNR_setresgid32,__PNR_setresgid32,210,__PNR_setresgid32},
#line 157 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str62,126,211,__PNR_getresgid32,__PNR_getresgid32,211,__PNR_getresgid32,__PNR_getresgid32,__PNR_getresgid32,__PNR_getresgid32,__PNR_getresgid32,__PNR_getresgid32,__PNR_getresgid32,__PNR_getresgid32,__PNR_getresgid32,211,__PNR_getresgid32},
#line 430 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str63,399,359,41,41,281,198,183,40,40,17,17,326,326,198,359,359},
#line 442 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str64,411,25,__PNR_stime,__PNR_stime,__PNR_stime,__PNR_stime,25,__PNR_stime,__PNR_stime,25,25,25,25,__PNR_stime,25,__PNR_stime},
#line 266 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str65,235,34,__PNR_nice,__PNR_nice,34,__PNR_nice,34,__PNR_nice,__PNR_nice,34,34,34,34,__PNR_nice,34,34},
#line 126 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str66,95,35,__PNR_ftime,__PNR_ftime,__PNR_ftime,__PNR_ftime,35,__PNR_ftime,__PNR_ftime,__PNR_ftime,__PNR_ftime,35,35,__PNR_ftime,__PNR_ftime,__PNR_ftime},
#line 306 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str67,275,308,270,270,335,72,301,260,264,273,273,280,280,72,301,301},
#line 135 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str68,104,141,78,78,141,__PNR_getdents,141,76,76,141,141,141,141,__PNR_getdents,141,141},
#line 187 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str69,156,110,172,172,__PNR_iopl,__PNR_iopl,110,__PNR_iopl,__PNR_iopl,__PNR_iopl,__PNR_iopl,110,110,__PNR_iopl,__PNR_iopl,__PNR_iopl},
#line 67 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str70,36,120,56,56,120,220,120,55,55,120,120,120,120,220,120,120},
#line 458 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str71,427,135,139,139,135,__PNR_sysfs,135,136,136,135,135,135,135,__PNR_sysfs,135,135},
#line 359 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str72,328,161,148,148,161,127,165,145,145,161,161,161,161,127,161,161},
#line 255 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str73,224,400,69,69,301,189,400,67,67,188,188,400,400,189,400,400},
#line 454 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str74,423,344,306,306,373,267,342,301,306,327,327,348,348,267,338,338},
#line 276 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str75,245,428,428,428,428,428,428,428,428,428,428,428,428,428,428,428},
#line 241 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str76,210,125,10,10,125,226,125,10,10,125,125,125,125,226,125,125},
#line 277 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str77,246,29,34,34,29,__PNR_pause,29,33,33,29,29,29,29,__PNR_pause,29,29},
#line 343 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str78,312,173,15,513,173,139,193,211,211,173,173,172,172,139,173,173},
#line 451 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str79,420,36,162,162,36,81,36,157,157,36,36,36,36,81,36,36},
#line 473 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str80,442,262,225,225,260,109,260,219,223,253,253,243,243,109,257,257},
#line 371 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str81,340,__PNR_semop,65,65,298,193,__PNR_semop,63,63,185,185,__PNR_semop,__PNR_semop,193,__PNR_semop,__PNR_semop},
#line 480 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str82,449,92,76,76,92,45,92,74,74,92,92,92,92,45,92,92},
#line 222 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str83,191,375,324,324,389,283,358,318,322,343,343,365,365,283,356,356},
#line 366 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str84,335,354,317,317,383,277,352,312,316,338,338,358,358,277,348,348},
#line 31 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str85,0,__PNR_accept,43,43,285,202,168,42,42,35,35,330,330,202,__PNR_accept,__PNR_accept},
#line 357 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str86,326,160,147,147,160,126,164,144,144,160,160,160,160,126,160,160},
#line 411 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str87,380,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,983045,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls,__PNR_set_tls},
#line 169 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str88,138,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,983046,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls,__PNR_get_tls},
#line 369 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str89,338,394,66,66,300,191,394,64,64,187,187,394,394,191,394,394},
#line 101 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str90,70,94,91,91,94,52,94,89,89,94,94,94,94,52,94,94},
#line 365 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str91,334,158,24,24,158,124,162,23,23,158,158,158,158,124,158,158},
#line 361 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str92,330,241,203,203,241,122,239,195,195,211,211,222,222,122,239,239},
#line 353 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str93,322,242,204,204,242,123,240,196,196,212,212,223,223,123,240,240},
#line 225 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str94,194,294,256,256,400,238,287,246,250,272,272,258,258,238,287,287},
#line 292 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str95,261,168,7,7,168,__PNR_poll,188,7,7,168,168,167,167,__PNR_poll,168,168},
#line 115 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str96,84,431,431,431,431,431,431,431,431,431,431,431,431,431,431,431},
#line 252 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str97,221,402,71,71,304,187,402,69,69,191,191,402,402,187,402,402},
#line 456 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str98,425,149,156,__PNR__sysctl,149,__PNR__sysctl,153,152,152,149,149,149,149,__PNR__sysctl,149,149},
#line 460 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str99,429,103,103,103,103,116,103,101,101,103,103,103,103,116,103,103},
#line 288 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str100,257,217,155,155,218,41,216,151,151,67,67,203,203,41,217,217},
#line 79 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str101,48,254,213,213,250,__PNR_epoll_create,248,207,207,224,224,236,236,__PNR_epoll_create,249,249},
#line 409 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str102,378,258,218,218,256,96,252,212,213,237,237,232,232,96,252,252},
#line 340 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str103,309,176,127,522,176,136,196,125,125,176,176,175,175,136,176,176},
#line 80 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str104,49,329,291,291,357,20,326,285,289,311,311,315,315,20,327,327},
#line 93 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str105,62,307,269,269,334,48,300,259,263,287,287,298,298,48,300,300},
#line 443 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str106,412,31,__PNR_stty,__PNR_stty,__PNR_stty,__PNR_stty,31,__PNR_stty,__PNR_stty,__PNR_stty,__PNR_stty,31,31,__PNR_stty,__PNR_stty,__PNR_stty},
#line 173 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str107,142,32,__PNR_gtty,__PNR_gtty,__PNR_gtty,__PNR_gtty,32,__PNR_gtty,__PNR_gtty,__PNR_gtty,__PNR_gtty,32,32,__PNR_gtty,__PNR_gtty,__PNR_gtty},
#line 412 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str108,381,23,105,105,23,146,23,103,103,23,23,23,23,146,23,213},
#line 170 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str109,139,24,102,102,24,174,24,100,100,24,24,24,24,174,24,199},
#line 38 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str110,7,27,37,37,__PNR_alarm,__PNR_alarm,27,37,37,27,27,27,27,__PNR_alarm,27,27},
#line 240 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str111,209,317,279,533,344,239,308,267,271,295,295,301,301,239,310,310},
#line 273 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str112,242,295,257,257,322,56,288,247,251,275,275,286,286,56,288,288},
#line 270 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str113,239,18,__PNR_oldstat,__PNR_oldstat,__PNR_oldstat,__PNR_oldstat,__PNR_oldstat,__PNR_oldstat,__PNR_oldstat,__PNR_oldstat,__PNR_oldstat,18,18,__PNR_oldstat,__PNR_oldstat,__PNR_oldstat},
#line 267 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str114,236,28,__PNR_oldfstat,__PNR_oldfstat,__PNR_oldfstat,__PNR_oldfstat,__PNR_oldfstat,__PNR_oldfstat,__PNR_oldfstat,__PNR_oldfstat,__PNR_oldfstat,28,28,__PNR_oldfstat,__PNR_oldfstat,__PNR_oldfstat},
#line 424 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str115,393,321,282,282,349,__PNR_signalfd,317,276,280,302,302,305,305,__PNR_signalfd,316,316},
#line 72 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str116,41,377,326,326,391,285,360,320,324,346,346,379,379,285,375,375},
#line 392 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str117,361,450,450,450,450,450,450,450,450,450,450,450,450,450,450,450},
#line 503 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str118,472,166,__PNR_vm86,__PNR_vm86,__PNR_vm86,__PNR_vm86,113,__PNR_vm86,__PNR_vm86,__PNR_vm86,__PNR_vm86,113,113,__PNR_vm86,__PNR_vm86,__PNR_vm86},
#line 305 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str119,274,98,__PNR_profil,__PNR_profil,__PNR_profil,__PNR_profil,98,__PNR_profil,__PNR_profil,__PNR_profil,__PNR_profil,98,98,__PNR_profil,__PNR_profil,__PNR_profil},
#line 182 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str120,151,246,207,207,244,1,242,201,201,216,216,228,228,1,244,244},
#line 268 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str121,237,84,__PNR_oldlstat,__PNR_oldlstat,__PNR_oldlstat,__PNR_oldlstat,__PNR_oldlstat,__PNR_oldlstat,__PNR_oldlstat,__PNR_oldlstat,__PNR_oldlstat,84,84,__PNR_oldlstat,__PNR_oldlstat,__PNR_oldlstat},
#line 272 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str122,241,5,2,2,5,__PNR_open,5,2,2,5,5,5,5,__PNR_open,5,5},
#line 87 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str123,56,323,284,284,351,__PNR_eventfd,319,278,282,304,304,307,307,__PNR_eventfd,318,318},
#line 279 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str124,248,__PNR_pciconfig_read,__PNR_pciconfig_read,__PNR_pciconfig_read,272,__PNR_pciconfig_read,__PNR_pciconfig_read,__PNR_pciconfig_read,__PNR_pciconfig_read,__PNR_pciconfig_read,__PNR_pciconfig_read,198,198,__PNR_pciconfig_read,__PNR_pciconfig_read,__PNR_pciconfig_read},
#line 280 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str125,249,__PNR_pciconfig_write,__PNR_pciconfig_write,__PNR_pciconfig_write,273,__PNR_pciconfig_write,__PNR_pciconfig_write,__PNR_pciconfig_write,__PNR_pciconfig_write,__PNR_pciconfig_write,__PNR_pciconfig_write,199,199,__PNR_pciconfig_write,__PNR_pciconfig_write,__PNR_pciconfig_write},
#line 278 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str126,247,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase,271,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase,200,200,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase,__PNR_pciconfig_iobase},
#line 339 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str127,308,174,13,512,174,134,194,13,13,174,174,173,173,134,174,174},
#line 452 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str128,421,314,277,277,__PNR_sync_file_range,84,305,264,268,292,292,__PNR_sync_file_range,__PNR_sync_file_range,84,307,307},
#line 414 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str129,383,226,188,188,226,5,224,180,180,238,238,209,209,5,224,224},
#line 172 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str130,141,229,191,191,229,8,227,183,183,241,241,212,212,8,227,227},
#line 287 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str131,256,331,293,293,359,59,328,287,291,313,313,317,317,59,325,325},
#line 434 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str132,403,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create,279,279,__PNR_spu_create,__PNR_spu_create,__PNR_spu_create},
#line 134 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str133,103,183,79,79,183,17,203,77,77,110,110,182,182,17,183,183},
#line 389 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str134,358,74,170,170,74,161,74,165,165,74,74,74,74,161,74,74},
#line 208 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str135,177,303,265,265,330,37,296,255,259,283,283,294,294,37,296,296},
#line 97 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str136,66,324,285,285,352,47,320,279,283,305,305,309,309,47,314,314},
#line 82 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str137,51,__PNR_epoll_ctl_old,214,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old,__PNR_epoll_ctl_old},
#line 39 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str138,8,384,158,158,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl,__PNR_arch_prctl},
#line 102 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str139,71,306,268,268,333,53,299,258,262,286,286,297,297,53,299,299},
#line 229 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str140,198,14,133,133,14,__PNR_mknod,14,131,131,14,14,14,14,__PNR_mknod,14,14},
#line 284 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str141,253,434,434,434,434,434,434,434,434,434,434,434,434,434,434,434},
#line 285 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str142,254,424,424,424,424,424,424,424,424,424,424,424,424,424,424,424},
#line 432 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str143,401,360,53,53,288,199,184,52,52,56,56,333,333,199,360,360},
#line 327 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str144,296,372,47,519,297,212,177,46,46,184,184,342,342,212,372,372},
#line 380 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str145,349,121,171,171,121,162,121,166,166,121,121,121,121,162,121,121},
#line 293 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str146,262,309,271,271,336,73,302,261,265,274,274,281,281,73,302,302},
#line 155 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str147,124,355,318,318,384,278,353,313,317,339,339,359,359,278,349,349},
#line 437 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str148,406,106,4,4,106,__PNR_stat,106,4,4,18,18,106,106,__PNR_stat,106,106},
#line 423 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str149,392,48,__PNR_signal,__PNR_signal,__PNR_signal,__PNR_signal,48,__PNR_signal,__PNR_signal,48,48,48,48,__PNR_signal,48,48},
#line 439 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str150,408,99,137,137,99,43,99,134,134,99,99,99,99,43,99,99},
#line 94 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str151,63,439,439,439,439,439,439,439,439,439,439,439,439,439,439,439},
#line 48 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str152,17,__PNR_cachectl,__PNR_cachectl,__PNR_cachectl,__PNR_cachectl,__PNR_cachectl,148,198,198,__PNR_cachectl,__PNR_cachectl,__PNR_cachectl,__PNR_cachectl,__PNR_cachectl,__PNR_cachectl,__PNR_cachectl},
#line 325 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str153,294,337,299,537,365,243,335,294,298,319,319,343,343,243,357,357},
#line 200 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str154,169,37,62,62,37,129,37,60,60,37,37,37,37,129,37,37},
#line 167 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str155,136,224,186,186,224,178,222,178,178,206,206,207,207,178,236,236},
#line 98 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str156,67,338,300,300,367,262,336,295,300,322,322,323,323,262,332,332},
#line 383 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str157,352,138,122,122,138,151,138,120,120,138,138,138,138,151,138,215},
#line 139 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str158,108,49,107,107,49,175,49,105,105,49,49,49,49,175,49,201},
#line 402 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str159,371,70,113,113,70,145,70,111,111,70,70,70,70,145,70,203},
#line 400 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str160,369,164,117,117,164,147,185,115,115,164,164,164,164,147,164,208},
#line 158 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str161,127,165,118,118,165,148,186,116,116,165,165,165,165,148,165,209},
#line 461 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str162,430,__PNR_sysmips,__PNR_sysmips,__PNR_sysmips,__PNR_sysmips,__PNR_sysmips,149,199,199,__PNR_sysmips,__PNR_sysmips,__PNR_sysmips,__PNR_sysmips,__PNR_sysmips,__PNR_sysmips,__PNR_sysmips},
#line 263 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str163,232,__PNR_newfstatat,262,262,__PNR_newfstatat,79,__PNR_newfstatat,252,256,__PNR_newfstatat,__PNR_newfstatat,__PNR_newfstatat,291,79,__PNR_newfstatat,293},
#line 344 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str164,313,179,130,130,179,133,199,128,128,179,179,178,178,133,179,179},
#line 341 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str165,310,175,14,14,175,135,195,14,14,175,175,174,174,135,175,175},
#line 113 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str166,82,2,57,57,2,__PNR_fork,2,56,56,2,2,2,2,__PNR_fork,2,2},
#line 59 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str167,28,266,229,229,264,114,264,223,227,257,257,247,247,114,261,261},
#line 65 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str168,34,264,227,227,262,112,262,221,225,255,255,245,245,112,259,259},
#line 61 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str169,30,265,228,228,263,113,263,222,226,256,256,246,246,113,260,260},
#line 477 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str170,446,409,__PNR_timer_settime64,__PNR_timer_settime64,409,__PNR_timer_settime64,409,__PNR_timer_settime64,409,409,__PNR_timer_settime64,409,__PNR_timer_settime64,__PNR_timer_settime64,409,__PNR_timer_settime64},
#line 475 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str171,444,408,__PNR_timer_gettime64,__PNR_timer_gettime64,408,__PNR_timer_gettime64,408,__PNR_timer_gettime64,408,408,__PNR_timer_gettime64,408,__PNR_timer_gettime64,__PNR_timer_gettime64,408,__PNR_timer_gettime64},
#line 197 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str172,166,__PNR_kexec_file_load,320,320,401,294,__PNR_kexec_file_load,__PNR_kexec_file_load,__PNR_kexec_file_load,355,355,382,382,294,381,381},
#line 472 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str173,441,411,__PNR_timerfd_settime64,__PNR_timerfd_settime64,411,__PNR_timerfd_settime64,411,__PNR_timerfd_settime64,411,411,__PNR_timerfd_settime64,411,__PNR_timerfd_settime64,__PNR_timerfd_settime64,411,__PNR_timerfd_settime64},
#line 470 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str174,439,410,__PNR_timerfd_gettime64,__PNR_timerfd_gettime64,410,__PNR_timerfd_gettime64,410,__PNR_timerfd_gettime64,410,410,__PNR_timerfd_gettime64,410,__PNR_timerfd_gettime64,__PNR_timerfd_gettime64,410,__PNR_timerfd_gettime64},
#line 70 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str175,39,436,436,436,436,436,436,436,436,436,436,436,436,436,436,436},
#line 504 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str176,473,113,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old,__PNR_vm86old},
#line 251 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str177,220,163,25,25,163,216,167,24,24,163,163,163,163,216,163,163},
#line 76 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str178,45,41,32,32,41,23,41,31,31,41,41,41,41,23,41,41},
#line 324 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str179,293,371,45,517,292,207,176,44,44,123,123,337,337,207,371,371},
#line 81 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str180,50,255,233,233,251,21,249,208,208,225,225,237,237,21,250,250},
#line 290 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str181,259,382,331,331,396,290,365,325,329,353,353,385,385,290,386,386},
#line 453 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str182,422,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2,308,308,__PNR_sync_file_range2,__PNR_sync_file_range2,__PNR_sync_file_range2},
#line 346 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str183,315,421,__PNR_rt_sigtimedwait_time64,__PNR_rt_sigtimedwait_time64,421,__PNR_rt_sigtimedwait_time64,421,__PNR_rt_sigtimedwait_time64,421,421,__PNR_rt_sigtimedwait_time64,421,__PNR_rt_sigtimedwait_time64,__PNR_rt_sigtimedwait_time64,421,__PNR_rt_sigtimedwait_time64},
#line 407 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str184,376,366,54,541,294,208,181,53,53,181,181,339,339,208,366,366},
#line 165 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str185,134,365,55,542,295,209,173,54,54,182,182,340,340,209,365,365},
#line 391 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str186,360,276,238,238,321,237,270,229,233,262,262,261,261,237,270,270},
#line 147 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str187,116,275,239,239,320,236,269,228,232,261,261,260,260,236,269,269},
#line 360 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str188,329,423,__PNR_sched_rr_get_interval_time64,__PNR_sched_rr_get_interval_time64,423,__PNR_sched_rr_get_interval_time64,423,__PNR_sched_rr_get_interval_time64,423,423,__PNR_sched_rr_get_interval_time64,423,__PNR_sched_rr_get_interval_time64,__PNR_sched_rr_get_interval_time64,423,__PNR_sched_rr_get_interval_time64},
#line 207 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str189,176,9,86,86,9,__PNR_link,9,84,84,9,9,9,9,__PNR_link,9,9},
#line 230 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str190,199,297,259,259,324,33,290,249,253,277,277,288,288,33,290,290},
#line 220 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str191,189,219,28,28,220,233,218,27,27,119,119,205,205,233,219,219},
#line 337 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str192,306,386,334,334,398,293,367,327,331,354,354,387,387,293,383,383},
#line 199 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str193,168,288,250,250,311,219,282,241,245,266,266,271,271,219,280,280},
#line 151 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str194,120,20,39,39,20,172,20,38,38,20,20,20,20,172,20,20},
#line 394 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str195,363,57,109,109,57,154,57,107,107,57,57,57,57,154,57,57},
#line 149 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str196,118,132,121,121,132,155,132,119,119,132,132,132,132,155,132,132},
#line 192 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str197,161,426,426,426,426,426,426,426,426,426,426,426,426,426,426,426},
#line 193 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str198,162,427,427,427,427,427,427,427,427,427,427,427,427,427,427,427},
#line 479 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str199,448,238,200,200,238,130,236,192,192,208,208,208,208,130,237,237},
#line 418 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str200,387,398,67,67,306,197,398,65,65,193,193,398,398,197,398,398},
#line 152 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str201,121,188,181,181,__PNR_getpmsg,__PNR_getpmsg,208,174,174,__PNR_getpmsg,__PNR_getpmsg,187,187,__PNR_getpmsg,188,188},
#line 370 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str202,339,393,64,64,299,190,393,62,62,186,186,393,393,190,393,393},
#line 321 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str203,290,145,19,515,145,65,145,18,18,145,145,145,145,65,145,145},
#line 385 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str204,354,46,106,106,46,144,46,104,104,46,46,46,46,144,46,214},
#line 141 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str205,110,47,104,104,47,176,47,102,102,47,47,47,47,176,47,200},
#line 34 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str206,3,51,163,163,51,89,51,158,158,51,51,51,51,89,51,51},
#line 213 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str207,182,53,__PNR_lock,__PNR_lock,__PNR_lock,__PNR_lock,53,__PNR_lock,__PNR_lock,__PNR_lock,__PNR_lock,53,53,__PNR_lock,__PNR_lock,__PNR_lock},
#line 431 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str208,400,102,__PNR_socketcall,__PNR_socketcall,__PNR_socketcall,__PNR_socketcall,102,__PNR_socketcall,__PNR_socketcall,__PNR_socketcall,__PNR_socketcall,102,102,__PNR_socketcall,102,102},
#line 162 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str209,131,77,98,98,77,165,77,96,96,77,77,77,77,165,77,77},
#line 506 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str210,475,273,236,__PNR_vserver,313,__PNR_vserver,277,236,240,__PNR_vserver,__PNR_vserver,__PNR_vserver,__PNR_vserver,__PNR_vserver,__PNR_vserver,__PNR_vserver},
#line 120 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str211,89,108,5,5,108,80,108,5,5,28,28,108,108,80,108,108},
#line 43 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str212,12,361,49,49,282,200,169,48,48,22,22,327,327,200,361,361},
#line 356 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str213,325,159,146,146,159,125,163,143,143,159,159,159,159,125,159,159},
#line 253 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str214,222,399,68,68,303,186,399,66,66,190,190,399,399,186,399,399},
#line 123 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str215,92,100,138,138,100,44,100,135,135,100,100,100,100,44,100,100},
#line 401 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str216,370,208,__PNR_setresuid32,__PNR_setresuid32,208,__PNR_setresuid32,__PNR_setresuid32,__PNR_setresuid32,__PNR_setresuid32,__PNR_setresuid32,__PNR_setresuid32,__PNR_setresuid32,__PNR_setresuid32,__PNR_setresuid32,208,__PNR_setresuid32},
#line 159 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str217,128,209,__PNR_getresuid32,__PNR_getresuid32,209,__PNR_getresuid32,__PNR_getresuid32,__PNR_getresuid32,__PNR_getresuid32,__PNR_getresuid32,__PNR_getresuid32,__PNR_getresuid32,__PNR_getresuid32,__PNR_getresuid32,209,__PNR_getresuid32},
#line 372 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str218,341,__PNR_semtimedop,220,220,312,192,__PNR_semtimedop,214,215,228,228,__PNR_semtimedop,392,192,__PNR_semtimedop,392},
#line 274 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str219,243,437,437,437,437,437,437,437,437,437,437,437,437,437,437,437},
#line 500 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str220,469,271,235,235,269,__PNR_utimes,267,226,230,336,336,251,251,__PNR_utimes,313,313},
#line 497 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str221,466,30,132,132,__PNR_utime,__PNR_utime,30,130,130,30,30,30,30,__PNR_utime,30,30},
#line 254 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str222,223,401,70,70,302,188,401,68,68,189,189,401,401,188,401,401},
#line 307 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str223,276,413,__PNR_pselect6_time64,__PNR_pselect6_time64,413,__PNR_pselect6_time64,413,__PNR_pselect6_time64,413,413,__PNR_pselect6_time64,413,__PNR_pselect6_time64,__PNR_pselect6_time64,413,__PNR_pselect6_time64},
#line 376 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str224,345,239,__PNR_sendfile64,__PNR_sendfile64,239,__PNR_sendfile64,237,__PNR_sendfile64,219,209,209,226,__PNR_sendfile64,__PNR_sendfile64,223,__PNR_sendfile64},
#line 73 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str225,42,8,85,85,8,__PNR_creat,8,83,83,8,8,8,8,__PNR_creat,8,8},
#line 242 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str226,211,56,__PNR_mpx,__PNR_mpx,__PNR_mpx,__PNR_mpx,56,__PNR_mpx,__PNR_mpx,__PNR_mpx,__PNR_mpx,56,56,__PNR_mpx,__PNR_mpx,__PNR_mpx},
#line 194 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str227,163,425,425,425,425,425,425,425,425,425,425,425,425,425,425,425},
#line 146 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str228,115,130,177,__PNR_get_kernel_syms,__PNR_get_kernel_syms,__PNR_get_kernel_syms,130,170,170,__PNR_get_kernel_syms,__PNR_get_kernel_syms,130,130,__PNR_get_kernel_syms,130,130},
#line 153 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str229,122,64,110,110,64,173,64,108,108,64,64,64,64,173,64,64},
#line 265 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str230,234,169,180,__PNR_nfsservctl,169,42,189,173,173,__PNR_nfsservctl,__PNR_nfsservctl,168,168,42,169,169},
#line 330 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str231,299,38,82,82,38,__PNR_rename,38,80,80,38,38,38,38,__PNR_rename,38,38},
#line 127 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str232,96,93,77,77,93,46,93,75,75,93,93,93,93,46,93,93},
#line 88 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str233,57,328,290,290,356,19,325,284,288,310,310,314,314,19,323,323},
#line 180 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str234,149,249,210,210,247,3,245,204,204,219,219,231,231,3,247,247},
#line 190 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str235,159,245,206,543,243,0,241,200,200,215,215,227,227,0,243,243},
#line 291 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str236,260,380,329,329,394,288,363,323,327,351,351,386,386,288,384,384},
#line 198 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str237,167,283,246,528,347,104,311,270,274,300,300,268,268,104,277,277},
#line 234 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str238,203,90,9,9,__PNR_mmap,222,90,9,9,90,90,90,90,222,90,90},
#line 44 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str239,13,357,321,321,386,280,355,315,319,341,341,361,361,280,351,351},
#line 323 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str240,292,__PNR_recv,__PNR_recv,__PNR_recv,291,__PNR_recv,175,__PNR_recv,__PNR_recv,98,98,336,336,__PNR_recv,__PNR_recv,__PNR_recv},
#line 136 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str241,105,220,217,217,217,61,219,308,299,201,201,202,202,61,220,220},
#line 221 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str242,190,274,237,237,319,235,268,227,231,260,260,259,259,235,268,268},
#line 53 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str243,22,15,90,90,15,__PNR_chmod,15,88,88,15,15,15,15,__PNR_chmod,15,15},
#line 262 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str244,231,162,35,35,162,101,166,34,34,162,162,162,162,101,162,162},
#line 302 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str245,271,347,310,539,376,270,345,304,309,330,330,351,351,270,340,340},
#line 303 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str246,272,348,311,540,377,271,346,305,310,331,331,352,352,271,341,341},
#line 117 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str247,86,432,432,432,432,432,432,432,432,432,432,432,432,432,432,432},
#line 41 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str248,10,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,341,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range,__PNR_arm_sync_file_range},
#line 308 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str249,277,26,101,521,26,117,26,99,99,26,26,26,26,117,26,26},
#line 481 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str250,450,193,__PNR_truncate64,__PNR_truncate64,193,__PNR_truncate64,211,__PNR_truncate64,__PNR_truncate64,199,199,193,__PNR_truncate64,__PNR_truncate64,193,__PNR_truncate64},
#line 106 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str251,75,55,72,72,55,25,55,70,70,55,55,55,55,25,55,55},
#line 164 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str252,133,367,51,51,286,204,172,50,50,44,44,331,331,204,367,367},
#line 429 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str253,398,72,__PNR_sigsuspend,__PNR_sigsuspend,72,__PNR_sigsuspend,72,__PNR_sigsuspend,__PNR_sigsuspend,__PNR_sigsuspend,__PNR_sigsuspend,72,72,__PNR_sigsuspend,72,72},
#line 150 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str254,119,65,111,111,65,__PNR_getpgrp,65,109,109,65,65,65,65,__PNR_getpgrp,65,65},
#line 455 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str255,424,__PNR_syscall,__PNR_syscall,__PNR_syscall,__PNR_syscall,__PNR_syscall,0,__PNR_syscall,__PNR_syscall,__PNR_syscall,__PNR_syscall,__PNR_syscall,__PNR_syscall,__PNR_syscall,__PNR_syscall,__PNR_syscall},
#line 218 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str256,187,107,6,6,107,__PNR_lstat,107,6,6,84,84,107,107,__PNR_lstat,107,107},
#line 367 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str257,336,__PNR_security,185,185,__PNR_security,__PNR_security,__PNR_security,__PNR_security,__PNR_security,__PNR_security,__PNR_security,__PNR_security,__PNR_security,__PNR_security,__PNR_security,__PNR_security},
#line 56 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str258,25,61,161,161,61,51,61,156,156,61,61,61,61,51,61,61},
#line 216 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str259,185,19,8,8,19,62,19,8,8,19,19,19,19,62,19,19},
#line 112 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str260,81,143,73,73,143,32,143,71,71,143,143,143,143,32,143,143},
#line 74 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str261,43,127,174,__PNR_create_module,__PNR_create_module,__PNR_create_module,127,167,167,__PNR_create_module,__PNR_create_module,127,127,__PNR_create_module,127,127},
#line 181 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str262,150,54,16,514,54,29,54,15,15,54,54,54,54,29,54,54},
#line 50 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str263,19,184,125,125,184,90,204,123,123,106,106,183,183,90,184,184},
#line 334 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str264,303,0,219,219,0,128,253,213,214,0,0,0,0,128,7,7},
#line 484 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str265,453,58,__PNR_ulimit,__PNR_ulimit,__PNR_ulimit,__PNR_ulimit,58,__PNR_ulimit,__PNR_ulimit,__PNR_ulimit,__PNR_ulimit,58,58,__PNR_ulimit,__PNR_ulimit,__PNR_ulimit},
#line 212 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str266,181,140,__PNR__llseek,__PNR__llseek,140,__PNR__llseek,140,__PNR__llseek,__PNR__llseek,140,140,140,140,__PNR__llseek,140,__PNR__llseek},
#line 410 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str267,379,79,164,164,79,170,79,159,159,79,79,79,79,170,79,79},
#line 168 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str268,137,78,96,96,78,169,78,94,94,78,78,78,78,169,78,78},
#line 281 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str269,250,336,298,298,364,241,333,292,296,318,318,319,319,241,331,331},
#line 232 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str270,201,376,325,325,390,284,359,319,323,345,345,378,378,284,374,374},
#line 231 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str271,200,150,149,149,150,228,154,146,146,150,150,150,150,228,150,150},
#line 99 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str272,68,339,301,301,368,263,337,296,301,323,323,324,324,263,333,333},
#line 37 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str273,6,137,183,183,__PNR_afs_syscall,__PNR_afs_syscall,137,176,176,__PNR_afs_syscall,__PNR_afs_syscall,137,137,__PNR_afs_syscall,137,137},
#line 85 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str274,54,256,232,232,252,__PNR_epoll_wait,250,209,209,226,226,238,238,__PNR_epoll_wait,251,251},
#line 295 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str275,264,172,157,157,172,167,192,153,153,172,172,171,171,167,172,172},
#line 86 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str276,55,__PNR_epoll_wait_old,215,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old,__PNR_epoll_wait_old},
#line 320 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str277,289,305,267,267,332,78,298,257,261,285,285,296,296,78,298,298},
#line 491 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str278,460,310,272,272,337,97,303,262,266,288,288,282,282,97,303,303},
#line 331 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str279,300,302,264,264,329,38,295,254,258,282,282,293,293,__PNR_renameat,295,295},
#line 239 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str280,208,429,429,429,429,429,429,429,429,429,429,429,429,429,429,429},
#line 282 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str281,251,136,135,135,136,92,136,132,132,136,136,136,136,92,136,136},
#line 201 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str282,170,445,445,445,445,445,445,445,445,445,445,445,445,445,445,445},
#line 294 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str283,263,414,__PNR_ppoll_time64,__PNR_ppoll_time64,414,__PNR_ppoll_time64,414,__PNR_ppoll_time64,414,414,__PNR_ppoll_time64,414,__PNR_ppoll_time64,__PNR_ppoll_time64,414,__PNR_ppoll_time64},
#line 488 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str284,457,122,63,63,122,160,122,61,61,59,59,122,122,160,122,122},
#line 202 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str285,171,444,444,444,444,444,444,444,444,444,444,444,444,444,444,444},
#line 203 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str286,172,446,446,446,446,446,446,446,446,446,446,446,446,446,446,446},
#line 105 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str287,74,298,260,260,325,54,291,250,254,278,278,289,289,54,291,291},
#line 271 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str288,240,109,__PNR_olduname,__PNR_olduname,__PNR_olduname,__PNR_olduname,__PNR_olduname,__PNR_olduname,__PNR_olduname,__PNR_olduname,__PNR_olduname,109,109,__PNR_olduname,__PNR_olduname,__PNR_olduname},
#line 428 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str289,397,119,__PNR_sigreturn,__PNR_sigreturn,119,__PNR_sigreturn,119,__PNR_sigreturn,__PNR_sigreturn,__PNR_sigreturn,__PNR_sigreturn,119,119,__PNR_sigreturn,119,119},
#line 349 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str290,318,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,__PNR_s390_pci_mmio_read,353,353},
#line 350 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str291,319,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,__PNR_s390_pci_mmio_write,352,352},
#line 417 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str292,386,396,31,31,308,195,396,30,30,195,195,396,396,195,396,396},
#line 111 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str293,80,234,196,196,234,13,232,188,188,246,246,217,217,13,232,232},
#line 510 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str294,479,4,1,1,4,64,4,1,1,4,4,4,4,64,4,4},
#line 333 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str295,302,287,249,249,310,218,281,240,244,265,265,270,270,218,279,279},
#line 103 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str296,72,95,93,93,95,55,95,91,91,95,95,95,95,55,95,207},
#line 83 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str297,52,319,281,281,346,22,313,272,276,297,297,303,303,22,312,312},
#line 89 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str298,58,11,59,520,11,221,11,57,57,11,11,11,11,221,11,11},
#line 328 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str299,297,257,216,216,253,234,251,210,210,227,227,239,239,234,267,267},
#line 395 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str300,364,97,141,141,97,140,97,138,138,97,97,97,97,140,97,97},
#line 154 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str301,123,96,140,140,96,141,96,137,137,96,96,96,96,141,96,96},
#line 382 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str302,351,216,__PNR_setfsgid32,__PNR_setfsgid32,216,__PNR_setfsgid32,__PNR_setfsgid32,__PNR_setfsgid32,__PNR_setfsgid32,__PNR_setfsgid32,__PNR_setfsgid32,__PNR_setfsgid32,__PNR_setfsgid32,__PNR_setfsgid32,216,__PNR_setfsgid32},
#line 397 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str303,366,204,__PNR_setregid32,__PNR_setregid32,204,__PNR_setregid32,__PNR_setregid32,__PNR_setregid32,__PNR_setregid32,__PNR_setregid32,__PNR_setregid32,__PNR_setregid32,__PNR_setregid32,__PNR_setregid32,204,__PNR_setregid32},
#line 336 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str304,305,40,84,84,40,__PNR_rmdir,40,82,82,40,40,40,40,__PNR_rmdir,40,40},
#line 177 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str305,146,291,253,253,316,__PNR_inotify_init,284,243,247,269,269,275,275,__PNR_inotify_init,284,284},
#line 214 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str306,183,253,212,212,249,18,247,206,206,223,223,235,235,18,110,110},
#line 450 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str307,419,304,266,266,331,36,297,256,260,284,284,295,295,36,297,297},
#line 178 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str308,147,332,294,294,360,26,329,288,292,314,314,318,318,26,324,324},
#line 390 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str309,359,104,38,38,104,103,104,36,36,104,104,104,104,103,104,104},
#line 145 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str310,114,105,36,36,105,102,105,35,35,105,105,105,105,102,105,105},
#line 317 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str311,286,225,187,187,225,213,223,179,179,207,207,191,191,213,222,222},
#line 264 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str312,233,142,__PNR__newselect,__PNR__newselect,142,__PNR__newselect,142,22,22,142,142,142,142,__PNR__newselect,142,__PNR__newselect},
#line 235 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str313,204,192,__PNR_mmap2,__PNR_mmap2,192,__PNR_mmap2,210,__PNR_mmap2,__PNR_mmap2,89,89,192,__PNR_mmap2,__PNR_mmap2,192,__PNR_mmap2},
#line 501 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str314,470,190,58,58,190,__PNR_vfork,__PNR_vfork,__PNR_vfork,__PNR_vfork,113,113,189,189,__PNR_vfork,190,190},
#line 148 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str315,117,368,52,52,287,205,171,51,51,53,53,332,332,205,368,368},
#line 335 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str316,304,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache,259,__PNR_riscv_flush_icache,__PNR_riscv_flush_icache},
#line 47 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str317,16,45,12,12,45,214,45,12,12,45,45,45,45,214,45,45},
#line 118 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str318,87,430,430,430,430,430,430,430,430,430,430,430,430,430,430,430},
#line 326 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str319,295,417,__PNR_recvmmsg_time64,__PNR_recvmmsg_time64,417,__PNR_recvmmsg_time64,417,__PNR_recvmmsg_time64,417,417,__PNR_recvmmsg_time64,417,__PNR_recvmmsg_time64,__PNR_recvmmsg_time64,417,__PNR_recvmmsg_time64},
#line 185 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str320,154,385,333,333,399,292,368,328,332,350,350,388,388,292,382,382},
#line 433 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str321,402,313,275,275,340,76,304,263,267,291,291,283,283,76,306,306},
#line 110 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str322,79,350,313,313,379,273,348,307,312,333,333,353,353,273,344,344},
#line 66 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str323,35,404,__PNR_clock_settime64,__PNR_clock_settime64,404,__PNR_clock_settime64,404,__PNR_clock_settime64,404,404,__PNR_clock_settime64,404,__PNR_clock_settime64,__PNR_clock_settime64,404,__PNR_clock_settime64},
#line 62 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str324,31,403,__PNR_clock_gettime64,__PNR_clock_gettime64,403,__PNR_clock_gettime64,403,__PNR_clock_gettime64,403,403,__PNR_clock_gettime64,403,__PNR_clock_gettime64,__PNR_clock_gettime64,403,__PNR_clock_gettime64},
#line 209 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str325,178,363,50,50,284,201,174,49,49,32,32,329,329,201,363,363},
#line 426 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str326,395,73,__PNR_sigpending,__PNR_sigpending,73,__PNR_sigpending,73,__PNR_sigpending,__PNR_sigpending,73,73,73,73,__PNR_sigpending,73,73},
#line 60 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str327,29,406,__PNR_clock_getres_time64,__PNR_clock_getres_time64,406,__PNR_clock_getres_time64,406,__PNR_clock_getres_time64,406,406,__PNR_clock_getres_time64,406,__PNR_clock_getres_time64,__PNR_clock_getres_time64,406,__PNR_clock_getres_time64},
#line 486 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str328,455,22,__PNR_umount,__PNR_umount,__PNR_umount,__PNR_umount,22,__PNR_umount,__PNR_umount,__PNR_umount,__PNR_umount,22,22,__PNR_umount,22,22},
#line 260 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str329,229,91,11,11,91,215,91,11,11,91,91,91,91,215,91,91},
#line 319 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str330,288,85,89,89,85,__PNR_readlink,85,87,87,85,85,85,85,__PNR_readlink,85,85},
#line 196 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str331,165,349,312,312,378,272,347,306,311,332,332,354,354,272,343,343},
#line 183 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str332,152,247,208,208,245,4,243,202,202,217,217,229,229,4,245,245},
#line 498 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str333,467,320,280,280,348,88,316,275,279,301,301,304,304,88,315,315},
#line 211 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str334,180,233,195,195,233,12,231,187,187,245,245,216,216,12,231,231},
#line 289 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str335,258,381,330,330,395,289,364,324,328,352,352,384,384,289,385,385},
#line 482 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str336,451,__PNR_tuxcall,184,184,__PNR_tuxcall,__PNR_tuxcall,__PNR_tuxcall,__PNR_tuxcall,__PNR_tuxcall,__PNR_tuxcall,__PNR_tuxcall,225,225,__PNR_tuxcall,__PNR_tuxcall,__PNR_tuxcall},
#line 204 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str337,173,16,94,94,16,__PNR_lchown,16,92,92,16,16,16,16,__PNR_lchown,16,198},
#line 133 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str338,102,318,309,309,345,168,312,271,275,296,296,302,302,168,311,311},
#line 508 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str339,477,284,247,529,280,95,278,237,241,235,235,272,272,95,281,281},
#line 233 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str340,202,152,151,151,152,230,156,148,148,152,152,152,152,230,152,152},
#line 91 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str341,60,1,60,60,1,93,1,58,58,1,1,1,1,93,1,1},
#line 261 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str342,230,341,303,303,370,264,339,298,303,325,325,345,345,264,335,335},
#line 35 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str343,4,286,248,248,309,217,280,239,243,264,264,269,269,217,278,278},
#line 90 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str344,59,358,322,545,387,281,356,316,320,342,342,362,362,281,354,354},
#line 446 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str345,415,115,168,168,115,225,115,163,163,115,115,115,115,225,115,115},
#line 84 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str346,53,441,441,441,441,441,441,441,441,441,441,441,441,441,441,441},
#line 373 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str347,342,420,__PNR_semtimedop_time64,__PNR_semtimedop_time64,420,__PNR_semtimedop_time64,420,__PNR_semtimedop_time64,420,420,__PNR_semtimedop_time64,420,__PNR_semtimedop_time64,__PNR_semtimedop_time64,420,__PNR_semtimedop_time64},
#line 435 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str348,404,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run,278,278,__PNR_spu_run,__PNR_spu_run,__PNR_spu_run},
#line 138 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str349,107,202,__PNR_getegid32,__PNR_getegid32,202,__PNR_getegid32,__PNR_getegid32,__PNR_getegid32,__PNR_getegid32,__PNR_getegid32,__PNR_getegid32,__PNR_getegid32,__PNR_getegid32,__PNR_getegid32,202,__PNR_getegid32},
#line 408 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str350,377,243,205,__PNR_set_thread_area,__PNR_set_thread_area,__PNR_set_thread_area,283,242,246,__PNR_set_thread_area,__PNR_set_thread_area,__PNR_set_thread_area,__PNR_set_thread_area,__PNR_set_thread_area,__PNR_set_thread_area,__PNR_set_thread_area},
#line 166 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str351,135,244,211,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area,__PNR_get_thread_area},
#line 108 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str352,77,148,75,75,148,83,152,73,73,148,148,148,148,83,148,148},
#line 505 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str353,474,316,278,532,343,75,307,266,270,294,294,285,285,75,309,309},
#line 449 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str354,418,83,88,88,83,__PNR_symlink,83,86,86,83,83,83,83,__PNR_symlink,83,83},
#line 496 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str355,465,62,136,136,62,__PNR_ustat,62,133,133,62,62,62,62,__PNR_ustat,62,62},
#line 509 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str356,478,7,__PNR_waitpid,__PNR_waitpid,__PNR_waitpid,__PNR_waitpid,7,__PNR_waitpid,__PNR_waitpid,7,7,7,7,__PNR_waitpid,__PNR_waitpid,__PNR_waitpid},
#line 63 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str357,32,267,230,230,265,115,265,224,228,258,258,248,248,115,262,262},
#line 421 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str358,390,67,__PNR_sigaction,__PNR_sigaction,67,__PNR_sigaction,67,__PNR_sigaction,__PNR_sigaction,__PNR_sigaction,__PNR_sigaction,67,67,__PNR_sigaction,67,67},
#line 483 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str359,452,191,__PNR_ugetrlimit,__PNR_ugetrlimit,191,__PNR_ugetrlimit,__PNR_ugetrlimit,__PNR_ugetrlimit,__PNR_ugetrlimit,__PNR_ugetrlimit,__PNR_ugetrlimit,190,190,__PNR_ugetrlimit,191,__PNR_ugetrlimit},
#line 248 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str360,217,279,242,242,276,182,273,232,236,231,231,264,264,182,273,273},
#line 485 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str361,454,60,95,95,60,166,60,93,93,60,60,60,60,166,60,60},
#line 347 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str362,316,335,297,536,363,240,332,291,295,317,317,322,322,240,330,330},
#line 342 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str363,311,178,129,524,178,138,198,127,127,178,178,177,177,138,178,178},
#line 246 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str364,215,280,243,243,277,183,274,233,237,232,232,265,265,183,274,274},
#line 329 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str365,298,235,197,197,235,14,233,189,189,247,247,218,218,14,233,233},
#line 245 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str366,214,277,240,240,274,180,271,230,234,229,229,262,262,180,271,271},
#line 332 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str367,301,353,316,316,382,276,351,311,315,337,337,357,357,276,347,347},
#line 128 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str368,97,194,__PNR_ftruncate64,__PNR_ftruncate64,194,__PNR_ftruncate64,212,__PNR_ftruncate64,__PNR_ftruncate64,200,200,194,__PNR_ftruncate64,__PNR_ftruncate64,194,__PNR_ftruncate64},
#line 309 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str369,278,189,182,182,__PNR_putpmsg,__PNR_putpmsg,209,175,175,__PNR_putpmsg,__PNR_putpmsg,188,188,__PNR_putpmsg,189,189},
#line 244 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str370,213,281,244,527,278,184,275,234,238,233,233,266,266,184,275,275},
#line 427 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str371,396,126,__PNR_sigprocmask,__PNR_sigprocmask,126,__PNR_sigprocmask,126,__PNR_sigprocmask,__PNR_sigprocmask,126,126,126,126,__PNR_sigprocmask,126,126},
#line 32 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str372,1,364,288,288,366,242,334,293,297,320,320,344,344,242,364,364},
#line 191 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str373,160,248,209,544,246,2,244,203,203,218,218,230,230,2,246,246},
#line 269 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str374,238,59,__PNR_oldolduname,__PNR_oldolduname,__PNR_oldolduname,__PNR_oldolduname,__PNR_oldolduname,__PNR_oldolduname,__PNR_oldolduname,__PNR_oldolduname,__PNR_oldolduname,59,59,__PNR_oldolduname,__PNR_oldolduname,__PNR_oldolduname},
#line 116 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str375,85,228,190,190,228,7,226,182,182,240,240,211,211,7,226,226},
#line 109 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str376,78,231,193,193,231,10,229,185,185,243,243,214,214,10,229,229},
#line 227 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str377,196,39,83,83,39,__PNR_mkdir,39,81,81,39,39,39,39,__PNR_mkdir,39,39},
#line 493 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str378,462,374,323,323,388,282,357,317,321,344,344,364,364,282,355,355},
#line 419 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str379,388,395,29,29,307,194,395,28,28,194,194,395,395,194,395,395},
#line 175 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str380,144,128,175,175,128,105,128,168,168,128,128,128,128,105,128,128},
#line 40 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str381,9,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,270,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64,__PNR_arm_fadvise64_64},
#line 387 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str382,356,81,116,116,81,159,81,114,114,81,81,81,81,159,81,206},
#line 143 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str383,112,80,115,115,80,158,80,113,113,80,80,80,80,158,80,205},
#line 494 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str384,463,__PNR_usr26,__PNR_usr26,__PNR_usr26,983043,__PNR_usr26,__PNR_usr26,__PNR_usr26,__PNR_usr26,__PNR_usr26,__PNR_usr26,__PNR_usr26,__PNR_usr26,__PNR_usr26,__PNR_usr26,__PNR_usr26},
#line 416 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str385,385,397,30,30,305,196,397,29,29,192,192,397,397,196,397,397},
#line 258 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str386,227,151,150,150,151,229,155,147,147,151,151,151,151,229,151,151},
#line 495 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str387,464,__PNR_usr32,__PNR_usr32,__PNR_usr32,983044,__PNR_usr32,__PNR_usr32,__PNR_usr32,__PNR_usr32,__PNR_usr32,__PNR_usr32,__PNR_usr32,__PNR_usr32,__PNR_usr32,__PNR_usr32,__PNR_usr32},
#line 463 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str388,432,270,234,234,268,131,266,225,229,259,259,250,250,131,241,241},
#line 243 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str389,212,282,245,245,279,185,276,235,239,234,234,267,267,185,276,276},
#line 422 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str390,391,186,131,525,186,132,206,129,129,166,166,185,185,132,186,186},
#line 445 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str391,414,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext,249,249,__PNR_swapcontext,__PNR_swapcontext,__PNR_swapcontext},
#line 436 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str392,405,69,__PNR_ssetmask,__PNR_ssetmask,__PNR_ssetmask,__PNR_ssetmask,69,__PNR_ssetmask,__PNR_ssetmask,69,69,69,69,__PNR_ssetmask,__PNR_ssetmask,__PNR_ssetmask},
#line 415 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str393,384,68,__PNR_sgetmask,__PNR_sgetmask,__PNR_sgetmask,__PNR_sgetmask,68,__PNR_sgetmask,__PNR_sgetmask,68,68,68,68,__PNR_sgetmask,__PNR_sgetmask,__PNR_sgetmask},
#line 425 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str394,394,327,289,289,355,74,324,283,287,309,309,313,313,74,322,322},
#line 444 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str395,413,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot,310,310,__PNR_subpage_prot,__PNR_subpage_prot,__PNR_subpage_prot},
#line 114 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str396,83,237,199,199,237,16,235,191,191,249,249,220,220,16,235,235},
#line 487 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str397,456,52,166,166,52,39,52,161,161,52,52,52,52,39,52,52},
#line 236 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str398,205,123,154,154,__PNR_modify_ldt,__PNR_modify_ldt,123,__PNR_modify_ldt,__PNR_modify_ldt,__PNR_modify_ldt,__PNR_modify_ldt,123,123,__PNR_modify_ldt,__PNR_modify_ldt,__PNR_modify_ldt},
#line 405 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str399,374,311,273,530,338,99,309,268,272,289,289,300,300,99,304,304},
#line 161 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str400,130,312,274,531,339,100,310,269,273,290,290,299,299,100,305,305},
#line 77 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str401,46,63,33,33,63,__PNR_dup2,63,32,32,63,63,63,63,__PNR_dup2,63,63},
#line 297 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str402,266,333,295,534,361,69,330,289,293,315,315,320,320,69,328,328},
#line 259 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str403,228,153,152,152,153,231,157,149,149,153,153,153,153,231,153,153},
#line 217 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str404,186,227,189,189,227,6,225,181,181,239,239,210,210,6,225,225},
#line 206 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str405,175,230,192,192,230,9,228,184,184,242,242,213,213,9,228,228},
#line 210 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str406,179,232,194,194,232,11,230,186,186,244,244,215,215,11,230,230},
#line 298 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str407,267,378,327,546,392,286,361,321,325,347,347,380,380,286,376,376},
#line 413 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str408,382,213,__PNR_setuid32,__PNR_setuid32,213,__PNR_setuid32,__PNR_setuid32,__PNR_setuid32,__PNR_setuid32,__PNR_setuid32,__PNR_setuid32,__PNR_setuid32,__PNR_setuid32,__PNR_setuid32,213,__PNR_setuid32},
#line 171 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str409,140,199,__PNR_getuid32,__PNR_getuid32,199,__PNR_getuid32,__PNR_getuid32,__PNR_getuid32,__PNR_getuid32,__PNR_getuid32,__PNR_getuid32,__PNR_getuid32,__PNR_getuid32,__PNR_getuid32,199,__PNR_getuid32},
#line 384 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str410,353,215,__PNR_setfsuid32,__PNR_setfsuid32,215,__PNR_setfsuid32,__PNR_setfsuid32,__PNR_setfsuid32,__PNR_setfsuid32,__PNR_setfsuid32,__PNR_setfsuid32,__PNR_setfsuid32,__PNR_setfsuid32,__PNR_setfsuid32,215,__PNR_setfsuid32},
#line 228 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str411,197,296,258,258,323,34,289,248,252,276,276,287,287,34,289,289},
#line 403 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str412,372,203,__PNR_setreuid32,__PNR_setreuid32,203,__PNR_setreuid32,__PNR_setreuid32,__PNR_setreuid32,__PNR_setreuid32,__PNR_setreuid32,__PNR_setreuid32,__PNR_setreuid32,__PNR_setreuid32,__PNR_setreuid32,203,__PNR_setreuid32},
#line 313 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str413,282,167,178,__PNR_query_module,__PNR_query_module,__PNR_query_module,187,171,171,__PNR_query_module,__PNR_query_module,166,166,__PNR_query_module,167,167},
#line 459 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str414,428,116,99,99,116,179,116,97,97,116,116,116,116,179,116,116},
#line 348 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str415,317,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,__PNR_s390_guarded_storage,378,378},
#line 388 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str416,357,206,__PNR_setgroups32,__PNR_setgroups32,206,__PNR_setgroups32,__PNR_setgroups32,__PNR_setgroups32,__PNR_setgroups32,__PNR_setgroups32,__PNR_setgroups32,__PNR_setgroups32,__PNR_setgroups32,__PNR_setgroups32,206,__PNR_setgroups32},
#line 144 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str417,113,205,__PNR_getgroups32,__PNR_getgroups32,205,__PNR_getgroups32,__PNR_getgroups32,__PNR_getgroups32,__PNR_getgroups32,__PNR_getgroups32,__PNR_getgroups32,__PNR_getgroups32,__PNR_getgroups32,__PNR_getgroups32,205,__PNR_getgroups32},
#line 49 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str418,18,__PNR_cacheflush,__PNR_cacheflush,__PNR_cacheflush,983042,__PNR_cacheflush,147,197,197,__PNR_cacheflush,__PNR_cacheflush,__PNR_cacheflush,__PNR_cacheflush,__PNR_cacheflush,__PNR_cacheflush,__PNR_cacheflush},
#line 447 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str419,416,87,167,167,87,224,87,162,162,87,87,87,87,224,87,87},
#line 351 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str420,320,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,__PNR_s390_runtime_instr,342,342},
#line 215 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str421,184,236,198,198,236,15,234,190,190,248,248,219,219,15,234,234},
#line 438 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str422,407,195,__PNR_stat64,__PNR_stat64,195,__PNR_stat64,213,__PNR_stat64,__PNR_stat64,101,101,195,__PNR_stat64,__PNR_stat64,195,__PNR_stat64},
#line 440 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str423,409,268,__PNR_statfs64,__PNR_statfs64,266,__PNR_statfs64,255,__PNR_statfs64,217,298,298,252,252,__PNR_statfs64,265,265},
#line 186 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str424,155,416,__PNR_io_pgetevents_time64,__PNR_io_pgetevents_time64,416,__PNR_io_pgetevents_time64,416,__PNR_io_pgetevents_time64,416,416,__PNR_io_pgetevents_time64,416,__PNR_io_pgetevents_time64,__PNR_io_pgetevents_time64,416,__PNR_io_pgetevents_time64},
#line 457 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str425,426,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,256,256,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext,__PNR_sys_debug_setcontext},
#line 499 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str426,468,412,__PNR_utimensat_time64,__PNR_utimensat_time64,412,__PNR_utimensat_time64,412,__PNR_utimensat_time64,412,412,__PNR_utimensat_time64,412,__PNR_utimensat_time64,__PNR_utimensat_time64,412,__PNR_utimensat_time64},
#line 511 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str427,480,146,20,516,146,66,146,19,19,146,146,146,146,66,146,146},
#line 64 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str428,33,407,__PNR_clock_nanosleep_time64,__PNR_clock_nanosleep_time64,407,__PNR_clock_nanosleep_time64,407,__PNR_clock_nanosleep_time64,407,407,__PNR_clock_nanosleep_time64,407,__PNR_clock_nanosleep_time64,__PNR_clock_nanosleep_time64,407,__PNR_clock_nanosleep_time64},
#line 52 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str429,21,12,80,80,12,49,12,78,78,12,12,12,12,49,12,12},
#line 57 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str430,26,343,305,305,372,266,341,300,305,324,324,347,347,266,337,337},
#line 132 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str431,101,299,261,261,326,__PNR_futimesat,292,251,255,279,279,290,290,__PNR_futimesat,292,292},
#line 140 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str432,109,201,__PNR_geteuid32,__PNR_geteuid32,201,__PNR_geteuid32,__PNR_geteuid32,__PNR_geteuid32,__PNR_geteuid32,__PNR_geteuid32,__PNR_geteuid32,__PNR_geteuid32,__PNR_geteuid32,__PNR_geteuid32,201,__PNR_geteuid32},
#line 119 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str433,88,433,433,433,433,433,433,433,433,433,433,433,433,433,433,433},
#line 179 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str434,148,293,255,255,318,28,286,245,249,271,271,277,277,28,286,286},
#line 95 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str435,64,250,221,221,__PNR_fadvise64,223,254,215,216,__PNR_fadvise64,__PNR_fadvise64,233,233,223,253,253},
#line 96 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str436,65,272,__PNR_fadvise64_64,__PNR_fadvise64_64,__PNR_fadvise64_64,__PNR_fadvise64_64,__PNR_fadvise64_64,__PNR_fadvise64_64,__PNR_fadvise64_64,236,236,254,__PNR_fadvise64_64,__PNR_fadvise64_64,264,__PNR_fadvise64_64},
#line 130 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str437,99,422,__PNR_futex_time64,__PNR_futex_time64,422,__PNR_futex_time64,422,__PNR_futex_time64,422,422,__PNR_futex_time64,422,__PNR_futex_time64,__PNR_futex_time64,422,__PNR_futex_time64},
#line 352 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str438,321,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,__PNR_s390_sthyi,380,380},
#line 54 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str439,23,182,92,92,182,__PNR_chown,202,90,90,180,180,181,181,__PNR_chown,182,212},
#line 107 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str440,76,221,__PNR_fcntl64,__PNR_fcntl64,221,__PNR_fcntl64,220,__PNR_fcntl64,212,202,202,204,__PNR_fcntl64,__PNR_fcntl64,221,__PNR_fcntl64},
#line 502 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str441,471,111,153,153,111,58,111,150,150,111,111,111,111,58,111,111},
#line 249 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str442,218,418,__PNR_mq_timedsend_time64,__PNR_mq_timedsend_time64,418,__PNR_mq_timedsend_time64,418,__PNR_mq_timedsend_time64,418,418,__PNR_mq_timedsend_time64,418,__PNR_mq_timedsend_time64,__PNR_mq_timedsend_time64,418,__PNR_mq_timedsend_time64},
#line 441 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str443,410,383,332,332,397,291,366,326,330,349,349,383,383,291,379,379},
#line 121 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str444,90,197,__PNR_fstat64,__PNR_fstat64,197,__PNR_fstat64,215,__PNR_fstat64,__PNR_fstat64,112,112,197,__PNR_fstat64,__PNR_fstat64,197,__PNR_fstat64},
#line 129 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str445,98,240,202,202,240,98,238,194,194,210,210,221,221,98,238,238},
#line 247 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str446,216,419,__PNR_mq_timedreceive_time64,__PNR_mq_timedreceive_time64,419,__PNR_mq_timedreceive_time64,419,__PNR_mq_timedreceive_time64,419,419,__PNR_mq_timedreceive_time64,419,__PNR_mq_timedreceive_time64,__PNR_mq_timedreceive_time64,419,__PNR_mq_timedreceive_time64},
#line 124 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str447,93,269,__PNR_fstatfs64,__PNR_fstatfs64,267,__PNR_fstatfs64,256,__PNR_fstatfs64,218,299,299,253,253,__PNR_fstatfs64,266,266},
#line 68 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str448,37,435,435,435,435,435,435,435,435,435,435,435,435,435,435,435},
#line 78 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str449,47,330,292,292,358,24,327,286,290,312,312,316,316,24,326,326},
#line 386 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str450,355,214,__PNR_setgid32,__PNR_setgid32,214,__PNR_setgid32,__PNR_setgid32,__PNR_setgid32,__PNR_setgid32,__PNR_setgid32,__PNR_setgid32,__PNR_setgid32,__PNR_setgid32,__PNR_setgid32,214,__PNR_setgid32},
#line 142 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str451,111,200,__PNR_getgid32,__PNR_getgid32,200,__PNR_getgid32,__PNR_getgid32,__PNR_getgid32,__PNR_getgid32,__PNR_getgid32,__PNR_getgid32,__PNR_getgid32,__PNR_getgid32,__PNR_getgid32,200,__PNR_getgid32},
#line 92 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str452,61,252,231,231,248,94,246,205,205,222,222,234,234,94,248,248},
#line 176 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str453,145,292,254,254,317,27,285,244,248,270,270,276,276,27,285,285},
#line 275 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str454,244,342,304,304,371,265,340,299,304,326,326,346,346,265,336,336},
#line 296 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str455,265,180,17,17,180,67,200,16,16,108,108,179,179,67,180,180},
#line 104 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str456,73,207,__PNR_fchown32,__PNR_fchown32,207,__PNR_fchown32,__PNR_fchown32,__PNR_fchown32,__PNR_fchown32,__PNR_fchown32,__PNR_fchown32,__PNR_fchown32,__PNR_fchown32,__PNR_fchown32,207,__PNR_fchown32},
#line 219 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str457,188,196,__PNR_lstat64,__PNR_lstat64,196,__PNR_lstat64,214,__PNR_lstat64,__PNR_lstat64,198,198,196,__PNR_lstat64,__PNR_lstat64,196,__PNR_lstat64},
#line 122 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str458,91,300,__PNR_fstatat64,__PNR_fstatat64,327,__PNR_fstatat64,293,__PNR_fstatat64,__PNR_fstatat64,280,280,291,__PNR_fstatat64,__PNR_fstatat64,293,__PNR_fstatat64},
#line 492 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str459,461,86,134,__PNR_uselib,86,__PNR_uselib,86,__PNR_uselib,__PNR_uselib,86,86,86,86,__PNR_uselib,86,86},
#line 257 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str460,226,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer,201,201,__PNR_multiplexer,__PNR_multiplexer,__PNR_multiplexer},
#line 36 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str461,5,124,159,159,124,171,124,154,154,124,124,124,124,171,124,124},
#line 315 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str462,284,443,443,443,443,443,443,443,443,443,443,443,443,443,443,443},
#line 205 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str463,174,198,__PNR_lchown32,__PNR_lchown32,198,__PNR_lchown32,__PNR_lchown32,__PNR_lchown32,__PNR_lchown32,__PNR_lchown32,__PNR_lchown32,__PNR_lchown32,__PNR_lchown32,__PNR_lchown32,198,__PNR_lchown32},
#line 448 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str464,417,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian,363,363,__PNR_switch_endian,__PNR_switch_endian,__PNR_switch_endian},
#line 420 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str465,389,373,48,48,293,210,182,47,47,117,117,338,338,210,373,373},
#line 131 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str466,100,449,449,449,449,449,449,449,449,449,449,449,449,449,449,449},
#line 45 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str467,14,17,__PNR_break,__PNR_break,__PNR_break,__PNR_break,17,__PNR_break,__PNR_break,__PNR_break,__PNR_break,17,17,__PNR_break,__PNR_break,__PNR_break},
#line 46 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str468,15,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,983041,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint,__PNR_breakpoint},
#line 507 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str469,476,114,61,61,114,260,114,59,59,114,114,114,114,260,114,114},
#line 314 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str470,283,131,179,179,131,60,131,172,172,131,131,131,131,60,131,131},
#line 311 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str471,280,334,296,535,362,70,331,290,294,316,316,321,321,70,329,329},
#line 299 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str472,268,340,302,302,369,261,338,297,302,321,321,325,325,261,334,334},
#line 312 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str473,281,379,328,547,393,287,362,322,326,348,348,381,381,287,377,377},
#line 58 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str474,27,405,__PNR_clock_adjtime64,__PNR_clock_adjtime64,405,__PNR_clock_adjtime64,405,__PNR_clock_adjtime64,405,405,__PNR_clock_adjtime64,405,__PNR_clock_adjtime64,__PNR_clock_adjtime64,405,__PNR_clock_adjtime64},
#line 250 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str475,219,278,241,241,275,181,272,231,235,230,230,263,263,181,272,272},
#line 490 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str476,459,301,263,263,328,35,294,253,257,281,281,292,292,35,294,294},
#line 310 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str477,279,181,18,18,181,68,201,17,17,109,109,180,180,68,181,181},
#line 489 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str478,458,10,87,87,10,__PNR_unlink,10,85,85,10,10,10,10,__PNR_unlink,10,10},
#line 42 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str479,11,134,__PNR_bdflush,__PNR_bdflush,134,__PNR_bdflush,134,__PNR_bdflush,__PNR_bdflush,134,134,134,134,__PNR_bdflush,134,134},
#line 55 "syscalls.perf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str480,24,212,__PNR_chown32,__PNR_chown32,212,__PNR_chown32,__PNR_chown32,__PNR_chown32,__PNR_chown32,__PNR_chown32,__PNR_chown32,__PNR_chown32,__PNR_chown32,__PNR_chown32,212,__PNR_chown32}
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

          switch (key - 13)
            {
              case 0:
                resword = &wordlist[0];
                goto compare;
              case 3:
                resword = &wordlist[1];
                goto compare;
              case 4:
                resword = &wordlist[2];
                goto compare;
              case 5:
                resword = &wordlist[3];
                goto compare;
              case 6:
                resword = &wordlist[4];
                goto compare;
              case 7:
                resword = &wordlist[5];
                goto compare;
              case 10:
                resword = &wordlist[6];
                goto compare;
              case 11:
                resword = &wordlist[7];
                goto compare;
              case 12:
                resword = &wordlist[8];
                goto compare;
              case 16:
                resword = &wordlist[9];
                goto compare;
              case 17:
                resword = &wordlist[10];
                goto compare;
              case 18:
                resword = &wordlist[11];
                goto compare;
              case 20:
                resword = &wordlist[12];
                goto compare;
              case 21:
                resword = &wordlist[13];
                goto compare;
              case 23:
                resword = &wordlist[14];
                goto compare;
              case 24:
                resword = &wordlist[15];
                goto compare;
              case 28:
                resword = &wordlist[16];
                goto compare;
              case 34:
                resword = &wordlist[17];
                goto compare;
              case 35:
                resword = &wordlist[18];
                goto compare;
              case 36:
                resword = &wordlist[19];
                goto compare;
              case 37:
                resword = &wordlist[20];
                goto compare;
              case 38:
                resword = &wordlist[21];
                goto compare;
              case 39:
                resword = &wordlist[22];
                goto compare;
              case 40:
                resword = &wordlist[23];
                goto compare;
              case 46:
                resword = &wordlist[24];
                goto compare;
              case 47:
                resword = &wordlist[25];
                goto compare;
              case 49:
                resword = &wordlist[26];
                goto compare;
              case 50:
                resword = &wordlist[27];
                goto compare;
              case 51:
                resword = &wordlist[28];
                goto compare;
              case 52:
                resword = &wordlist[29];
                goto compare;
              case 54:
                resword = &wordlist[30];
                goto compare;
              case 57:
                resword = &wordlist[31];
                goto compare;
              case 58:
                resword = &wordlist[32];
                goto compare;
              case 59:
                resword = &wordlist[33];
                goto compare;
              case 60:
                resword = &wordlist[34];
                goto compare;
              case 61:
                resword = &wordlist[35];
                goto compare;
              case 62:
                resword = &wordlist[36];
                goto compare;
              case 67:
                resword = &wordlist[37];
                goto compare;
              case 70:
                resword = &wordlist[38];
                goto compare;
              case 71:
                resword = &wordlist[39];
                goto compare;
              case 76:
                resword = &wordlist[40];
                goto compare;
              case 80:
                resword = &wordlist[41];
                goto compare;
              case 81:
                resword = &wordlist[42];
                goto compare;
              case 84:
                resword = &wordlist[43];
                goto compare;
              case 87:
                resword = &wordlist[44];
                goto compare;
              case 88:
                resword = &wordlist[45];
                goto compare;
              case 92:
                resword = &wordlist[46];
                goto compare;
              case 96:
                resword = &wordlist[47];
                goto compare;
              case 97:
                resword = &wordlist[48];
                goto compare;
              case 104:
                resword = &wordlist[49];
                goto compare;
              case 105:
                resword = &wordlist[50];
                goto compare;
              case 106:
                resword = &wordlist[51];
                goto compare;
              case 111:
                resword = &wordlist[52];
                goto compare;
              case 112:
                resword = &wordlist[53];
                goto compare;
              case 114:
                resword = &wordlist[54];
                goto compare;
              case 118:
                resword = &wordlist[55];
                goto compare;
              case 119:
                resword = &wordlist[56];
                goto compare;
              case 122:
                resword = &wordlist[57];
                goto compare;
              case 125:
                resword = &wordlist[58];
                goto compare;
              case 127:
                resword = &wordlist[59];
                goto compare;
              case 128:
                resword = &wordlist[60];
                goto compare;
              case 129:
                resword = &wordlist[61];
                goto compare;
              case 130:
                resword = &wordlist[62];
                goto compare;
              case 131:
                resword = &wordlist[63];
                goto compare;
              case 133:
                resword = &wordlist[64];
                goto compare;
              case 134:
                resword = &wordlist[65];
                goto compare;
              case 136:
                resword = &wordlist[66];
                goto compare;
              case 141:
                resword = &wordlist[67];
                goto compare;
              case 143:
                resword = &wordlist[68];
                goto compare;
              case 145:
                resword = &wordlist[69];
                goto compare;
              case 147:
                resword = &wordlist[70];
                goto compare;
              case 148:
                resword = &wordlist[71];
                goto compare;
              case 151:
                resword = &wordlist[72];
                goto compare;
              case 152:
                resword = &wordlist[73];
                goto compare;
              case 154:
                resword = &wordlist[74];
                goto compare;
              case 156:
                resword = &wordlist[75];
                goto compare;
              case 157:
                resword = &wordlist[76];
                goto compare;
              case 160:
                resword = &wordlist[77];
                goto compare;
              case 162:
                resword = &wordlist[78];
                goto compare;
              case 164:
                resword = &wordlist[79];
                goto compare;
              case 165:
                resword = &wordlist[80];
                goto compare;
              case 168:
                resword = &wordlist[81];
                goto compare;
              case 172:
                resword = &wordlist[82];
                goto compare;
              case 173:
                resword = &wordlist[83];
                goto compare;
              case 174:
                resword = &wordlist[84];
                goto compare;
              case 177:
                resword = &wordlist[85];
                goto compare;
              case 178:
                resword = &wordlist[86];
                goto compare;
              case 181:
                resword = &wordlist[87];
                goto compare;
              case 182:
                resword = &wordlist[88];
                goto compare;
              case 185:
                resword = &wordlist[89];
                goto compare;
              case 189:
                resword = &wordlist[90];
                goto compare;
              case 192:
                resword = &wordlist[91];
                goto compare;
              case 193:
                resword = &wordlist[92];
                goto compare;
              case 194:
                resword = &wordlist[93];
                goto compare;
              case 196:
                resword = &wordlist[94];
                goto compare;
              case 197:
                resword = &wordlist[95];
                goto compare;
              case 200:
                resword = &wordlist[96];
                goto compare;
              case 201:
                resword = &wordlist[97];
                goto compare;
              case 204:
                resword = &wordlist[98];
                goto compare;
              case 207:
                resword = &wordlist[99];
                goto compare;
              case 209:
                resword = &wordlist[100];
                goto compare;
              case 210:
                resword = &wordlist[101];
                goto compare;
              case 211:
                resword = &wordlist[102];
                goto compare;
              case 212:
                resword = &wordlist[103];
                goto compare;
              case 214:
                resword = &wordlist[104];
                goto compare;
              case 215:
                resword = &wordlist[105];
                goto compare;
              case 217:
                resword = &wordlist[106];
                goto compare;
              case 218:
                resword = &wordlist[107];
                goto compare;
              case 219:
                resword = &wordlist[108];
                goto compare;
              case 220:
                resword = &wordlist[109];
                goto compare;
              case 225:
                resword = &wordlist[110];
                goto compare;
              case 229:
                resword = &wordlist[111];
                goto compare;
              case 231:
                resword = &wordlist[112];
                goto compare;
              case 232:
                resword = &wordlist[113];
                goto compare;
              case 234:
                resword = &wordlist[114];
                goto compare;
              case 241:
                resword = &wordlist[115];
                goto compare;
              case 243:
                resword = &wordlist[116];
                goto compare;
              case 245:
                resword = &wordlist[117];
                goto compare;
              case 246:
                resword = &wordlist[118];
                goto compare;
              case 247:
                resword = &wordlist[119];
                goto compare;
              case 248:
                resword = &wordlist[120];
                goto compare;
              case 252:
                resword = &wordlist[121];
                goto compare;
              case 254:
                resword = &wordlist[122];
                goto compare;
              case 258:
                resword = &wordlist[123];
                goto compare;
              case 261:
                resword = &wordlist[124];
                goto compare;
              case 263:
                resword = &wordlist[125];
                goto compare;
              case 264:
                resword = &wordlist[126];
                goto compare;
              case 269:
                resword = &wordlist[127];
                goto compare;
              case 270:
                resword = &wordlist[128];
                goto compare;
              case 272:
                resword = &wordlist[129];
                goto compare;
              case 273:
                resword = &wordlist[130];
                goto compare;
              case 278:
                resword = &wordlist[131];
                goto compare;
              case 279:
                resword = &wordlist[132];
                goto compare;
              case 283:
                resword = &wordlist[133];
                goto compare;
              case 284:
                resword = &wordlist[134];
                goto compare;
              case 285:
                resword = &wordlist[135];
                goto compare;
              case 288:
                resword = &wordlist[136];
                goto compare;
              case 291:
                resword = &wordlist[137];
                goto compare;
              case 292:
                resword = &wordlist[138];
                goto compare;
              case 293:
                resword = &wordlist[139];
                goto compare;
              case 297:
                resword = &wordlist[140];
                goto compare;
              case 298:
                resword = &wordlist[141];
                goto compare;
              case 302:
                resword = &wordlist[142];
                goto compare;
              case 304:
                resword = &wordlist[143];
                goto compare;
              case 306:
                resword = &wordlist[144];
                goto compare;
              case 307:
                resword = &wordlist[145];
                goto compare;
              case 308:
                resword = &wordlist[146];
                goto compare;
              case 315:
                resword = &wordlist[147];
                goto compare;
              case 316:
                resword = &wordlist[148];
                goto compare;
              case 320:
                resword = &wordlist[149];
                goto compare;
              case 322:
                resword = &wordlist[150];
                goto compare;
              case 324:
                resword = &wordlist[151];
                goto compare;
              case 325:
                resword = &wordlist[152];
                goto compare;
              case 327:
                resword = &wordlist[153];
                goto compare;
              case 329:
                resword = &wordlist[154];
                goto compare;
              case 330:
                resword = &wordlist[155];
                goto compare;
              case 331:
                resword = &wordlist[156];
                goto compare;
              case 334:
                resword = &wordlist[157];
                goto compare;
              case 335:
                resword = &wordlist[158];
                goto compare;
              case 336:
                resword = &wordlist[159];
                goto compare;
              case 338:
                resword = &wordlist[160];
                goto compare;
              case 339:
                resword = &wordlist[161];
                goto compare;
              case 340:
                resword = &wordlist[162];
                goto compare;
              case 342:
                resword = &wordlist[163];
                goto compare;
              case 344:
                resword = &wordlist[164];
                goto compare;
              case 345:
                resword = &wordlist[165];
                goto compare;
              case 346:
                resword = &wordlist[166];
                goto compare;
              case 347:
                resword = &wordlist[167];
                goto compare;
              case 350:
                resword = &wordlist[168];
                goto compare;
              case 351:
                resword = &wordlist[169];
                goto compare;
              case 356:
                resword = &wordlist[170];
                goto compare;
              case 357:
                resword = &wordlist[171];
                goto compare;
              case 358:
                resword = &wordlist[172];
                goto compare;
              case 359:
                resword = &wordlist[173];
                goto compare;
              case 360:
                resword = &wordlist[174];
                goto compare;
              case 361:
                resword = &wordlist[175];
                goto compare;
              case 362:
                resword = &wordlist[176];
                goto compare;
              case 366:
                resword = &wordlist[177];
                goto compare;
              case 370:
                resword = &wordlist[178];
                goto compare;
              case 372:
                resword = &wordlist[179];
                goto compare;
              case 375:
                resword = &wordlist[180];
                goto compare;
              case 376:
                resword = &wordlist[181];
                goto compare;
              case 377:
                resword = &wordlist[182];
                goto compare;
              case 378:
                resword = &wordlist[183];
                goto compare;
              case 379:
                resword = &wordlist[184];
                goto compare;
              case 380:
                resword = &wordlist[185];
                goto compare;
              case 382:
                resword = &wordlist[186];
                goto compare;
              case 383:
                resword = &wordlist[187];
                goto compare;
              case 390:
                resword = &wordlist[188];
                goto compare;
              case 400:
                resword = &wordlist[189];
                goto compare;
              case 401:
                resword = &wordlist[190];
                goto compare;
              case 404:
                resword = &wordlist[191];
                goto compare;
              case 405:
                resword = &wordlist[192];
                goto compare;
              case 406:
                resword = &wordlist[193];
                goto compare;
              case 407:
                resword = &wordlist[194];
                goto compare;
              case 409:
                resword = &wordlist[195];
                goto compare;
              case 410:
                resword = &wordlist[196];
                goto compare;
              case 412:
                resword = &wordlist[197];
                goto compare;
              case 415:
                resword = &wordlist[198];
                goto compare;
              case 418:
                resword = &wordlist[199];
                goto compare;
              case 421:
                resword = &wordlist[200];
                goto compare;
              case 422:
                resword = &wordlist[201];
                goto compare;
              case 426:
                resword = &wordlist[202];
                goto compare;
              case 428:
                resword = &wordlist[203];
                goto compare;
              case 429:
                resword = &wordlist[204];
                goto compare;
              case 430:
                resword = &wordlist[205];
                goto compare;
              case 432:
                resword = &wordlist[206];
                goto compare;
              case 433:
                resword = &wordlist[207];
                goto compare;
              case 434:
                resword = &wordlist[208];
                goto compare;
              case 435:
                resword = &wordlist[209];
                goto compare;
              case 436:
                resword = &wordlist[210];
                goto compare;
              case 437:
                resword = &wordlist[211];
                goto compare;
              case 439:
                resword = &wordlist[212];
                goto compare;
              case 441:
                resword = &wordlist[213];
                goto compare;
              case 442:
                resword = &wordlist[214];
                goto compare;
              case 443:
                resword = &wordlist[215];
                goto compare;
              case 447:
                resword = &wordlist[216];
                goto compare;
              case 448:
                resword = &wordlist[217];
                goto compare;
              case 449:
                resword = &wordlist[218];
                goto compare;
              case 450:
                resword = &wordlist[219];
                goto compare;
              case 451:
                resword = &wordlist[220];
                goto compare;
              case 452:
                resword = &wordlist[221];
                goto compare;
              case 453:
                resword = &wordlist[222];
                goto compare;
              case 454:
                resword = &wordlist[223];
                goto compare;
              case 455:
                resword = &wordlist[224];
                goto compare;
              case 458:
                resword = &wordlist[225];
                goto compare;
              case 460:
                resword = &wordlist[226];
                goto compare;
              case 461:
                resword = &wordlist[227];
                goto compare;
              case 464:
                resword = &wordlist[228];
                goto compare;
              case 465:
                resword = &wordlist[229];
                goto compare;
              case 466:
                resword = &wordlist[230];
                goto compare;
              case 469:
                resword = &wordlist[231];
                goto compare;
              case 471:
                resword = &wordlist[232];
                goto compare;
              case 476:
                resword = &wordlist[233];
                goto compare;
              case 477:
                resword = &wordlist[234];
                goto compare;
              case 480:
                resword = &wordlist[235];
                goto compare;
              case 481:
                resword = &wordlist[236];
                goto compare;
              case 483:
                resword = &wordlist[237];
                goto compare;
              case 485:
                resword = &wordlist[238];
                goto compare;
              case 487:
                resword = &wordlist[239];
                goto compare;
              case 488:
                resword = &wordlist[240];
                goto compare;
              case 489:
                resword = &wordlist[241];
                goto compare;
              case 492:
                resword = &wordlist[242];
                goto compare;
              case 493:
                resword = &wordlist[243];
                goto compare;
              case 498:
                resword = &wordlist[244];
                goto compare;
              case 502:
                resword = &wordlist[245];
                goto compare;
              case 503:
                resword = &wordlist[246];
                goto compare;
              case 510:
                resword = &wordlist[247];
                goto compare;
              case 511:
                resword = &wordlist[248];
                goto compare;
              case 514:
                resword = &wordlist[249];
                goto compare;
              case 515:
                resword = &wordlist[250];
                goto compare;
              case 516:
                resword = &wordlist[251];
                goto compare;
              case 519:
                resword = &wordlist[252];
                goto compare;
              case 520:
                resword = &wordlist[253];
                goto compare;
              case 521:
                resword = &wordlist[254];
                goto compare;
              case 523:
                resword = &wordlist[255];
                goto compare;
              case 524:
                resword = &wordlist[256];
                goto compare;
              case 527:
                resword = &wordlist[257];
                goto compare;
              case 530:
                resword = &wordlist[258];
                goto compare;
              case 532:
                resword = &wordlist[259];
                goto compare;
              case 534:
                resword = &wordlist[260];
                goto compare;
              case 535:
                resword = &wordlist[261];
                goto compare;
              case 537:
                resword = &wordlist[262];
                goto compare;
              case 540:
                resword = &wordlist[263];
                goto compare;
              case 542:
                resword = &wordlist[264];
                goto compare;
              case 543:
                resword = &wordlist[265];
                goto compare;
              case 544:
                resword = &wordlist[266];
                goto compare;
              case 546:
                resword = &wordlist[267];
                goto compare;
              case 547:
                resword = &wordlist[268];
                goto compare;
              case 548:
                resword = &wordlist[269];
                goto compare;
              case 549:
                resword = &wordlist[270];
                goto compare;
              case 550:
                resword = &wordlist[271];
                goto compare;
              case 551:
                resword = &wordlist[272];
                goto compare;
              case 553:
                resword = &wordlist[273];
                goto compare;
              case 557:
                resword = &wordlist[274];
                goto compare;
              case 559:
                resword = &wordlist[275];
                goto compare;
              case 562:
                resword = &wordlist[276];
                goto compare;
              case 564:
                resword = &wordlist[277];
                goto compare;
              case 566:
                resword = &wordlist[278];
                goto compare;
              case 572:
                resword = &wordlist[279];
                goto compare;
              case 574:
                resword = &wordlist[280];
                goto compare;
              case 577:
                resword = &wordlist[281];
                goto compare;
              case 578:
                resword = &wordlist[282];
                goto compare;
              case 579:
                resword = &wordlist[283];
                goto compare;
              case 580:
                resword = &wordlist[284];
                goto compare;
              case 582:
                resword = &wordlist[285];
                goto compare;
              case 583:
                resword = &wordlist[286];
                goto compare;
              case 584:
                resword = &wordlist[287];
                goto compare;
              case 593:
                resword = &wordlist[288];
                goto compare;
              case 597:
                resword = &wordlist[289];
                goto compare;
              case 598:
                resword = &wordlist[290];
                goto compare;
              case 600:
                resword = &wordlist[291];
                goto compare;
              case 601:
                resword = &wordlist[292];
                goto compare;
              case 602:
                resword = &wordlist[293];
                goto compare;
              case 603:
                resword = &wordlist[294];
                goto compare;
              case 605:
                resword = &wordlist[295];
                goto compare;
              case 607:
                resword = &wordlist[296];
                goto compare;
              case 610:
                resword = &wordlist[297];
                goto compare;
              case 611:
                resword = &wordlist[298];
                goto compare;
              case 614:
                resword = &wordlist[299];
                goto compare;
              case 615:
                resword = &wordlist[300];
                goto compare;
              case 616:
                resword = &wordlist[301];
                goto compare;
              case 617:
                resword = &wordlist[302];
                goto compare;
              case 619:
                resword = &wordlist[303];
                goto compare;
              case 623:
                resword = &wordlist[304];
                goto compare;
              case 626:
                resword = &wordlist[305];
                goto compare;
              case 628:
                resword = &wordlist[306];
                goto compare;
              case 630:
                resword = &wordlist[307];
                goto compare;
              case 632:
                resword = &wordlist[308];
                goto compare;
              case 635:
                resword = &wordlist[309];
                goto compare;
              case 636:
                resword = &wordlist[310];
                goto compare;
              case 644:
                resword = &wordlist[311];
                goto compare;
              case 645:
                resword = &wordlist[312];
                goto compare;
              case 649:
                resword = &wordlist[313];
                goto compare;
              case 651:
                resword = &wordlist[314];
                goto compare;
              case 652:
                resword = &wordlist[315];
                goto compare;
              case 655:
                resword = &wordlist[316];
                goto compare;
              case 656:
                resword = &wordlist[317];
                goto compare;
              case 659:
                resword = &wordlist[318];
                goto compare;
              case 660:
                resword = &wordlist[319];
                goto compare;
              case 663:
                resword = &wordlist[320];
                goto compare;
              case 664:
                resword = &wordlist[321];
                goto compare;
              case 667:
                resword = &wordlist[322];
                goto compare;
              case 671:
                resword = &wordlist[323];
                goto compare;
              case 672:
                resword = &wordlist[324];
                goto compare;
              case 673:
                resword = &wordlist[325];
                goto compare;
              case 674:
                resword = &wordlist[326];
                goto compare;
              case 676:
                resword = &wordlist[327];
                goto compare;
              case 677:
                resword = &wordlist[328];
                goto compare;
              case 678:
                resword = &wordlist[329];
                goto compare;
              case 681:
                resword = &wordlist[330];
                goto compare;
              case 685:
                resword = &wordlist[331];
                goto compare;
              case 687:
                resword = &wordlist[332];
                goto compare;
              case 688:
                resword = &wordlist[333];
                goto compare;
              case 689:
                resword = &wordlist[334];
                goto compare;
              case 692:
                resword = &wordlist[335];
                goto compare;
              case 693:
                resword = &wordlist[336];
                goto compare;
              case 694:
                resword = &wordlist[337];
                goto compare;
              case 699:
                resword = &wordlist[338];
                goto compare;
              case 700:
                resword = &wordlist[339];
                goto compare;
              case 705:
                resword = &wordlist[340];
                goto compare;
              case 710:
                resword = &wordlist[341];
                goto compare;
              case 711:
                resword = &wordlist[342];
                goto compare;
              case 713:
                resword = &wordlist[343];
                goto compare;
              case 714:
                resword = &wordlist[344];
                goto compare;
              case 717:
                resword = &wordlist[345];
                goto compare;
              case 719:
                resword = &wordlist[346];
                goto compare;
              case 722:
                resword = &wordlist[347];
                goto compare;
              case 727:
                resword = &wordlist[348];
                goto compare;
              case 728:
                resword = &wordlist[349];
                goto compare;
              case 730:
                resword = &wordlist[350];
                goto compare;
              case 731:
                resword = &wordlist[351];
                goto compare;
              case 742:
                resword = &wordlist[352];
                goto compare;
              case 744:
                resword = &wordlist[353];
                goto compare;
              case 745:
                resword = &wordlist[354];
                goto compare;
              case 753:
                resword = &wordlist[355];
                goto compare;
              case 758:
                resword = &wordlist[356];
                goto compare;
              case 759:
                resword = &wordlist[357];
                goto compare;
              case 761:
                resword = &wordlist[358];
                goto compare;
              case 770:
                resword = &wordlist[359];
                goto compare;
              case 772:
                resword = &wordlist[360];
                goto compare;
              case 778:
                resword = &wordlist[361];
                goto compare;
              case 779:
                resword = &wordlist[362];
                goto compare;
              case 780:
                resword = &wordlist[363];
                goto compare;
              case 783:
                resword = &wordlist[364];
                goto compare;
              case 785:
                resword = &wordlist[365];
                goto compare;
              case 789:
                resword = &wordlist[366];
                goto compare;
              case 791:
                resword = &wordlist[367];
                goto compare;
              case 792:
                resword = &wordlist[368];
                goto compare;
              case 793:
                resword = &wordlist[369];
                goto compare;
              case 802:
                resword = &wordlist[370];
                goto compare;
              case 808:
                resword = &wordlist[371];
                goto compare;
              case 822:
                resword = &wordlist[372];
                goto compare;
              case 826:
                resword = &wordlist[373];
                goto compare;
              case 833:
                resword = &wordlist[374];
                goto compare;
              case 835:
                resword = &wordlist[375];
                goto compare;
              case 836:
                resword = &wordlist[376];
                goto compare;
              case 837:
                resword = &wordlist[377];
                goto compare;
              case 841:
                resword = &wordlist[378];
                goto compare;
              case 842:
                resword = &wordlist[379];
                goto compare;
              case 843:
                resword = &wordlist[380];
                goto compare;
              case 845:
                resword = &wordlist[381];
                goto compare;
              case 846:
                resword = &wordlist[382];
                goto compare;
              case 847:
                resword = &wordlist[383];
                goto compare;
              case 849:
                resword = &wordlist[384];
                goto compare;
              case 853:
                resword = &wordlist[385];
                goto compare;
              case 854:
                resword = &wordlist[386];
                goto compare;
              case 856:
                resword = &wordlist[387];
                goto compare;
              case 857:
                resword = &wordlist[388];
                goto compare;
              case 858:
                resword = &wordlist[389];
                goto compare;
              case 859:
                resword = &wordlist[390];
                goto compare;
              case 865:
                resword = &wordlist[391];
                goto compare;
              case 883:
                resword = &wordlist[392];
                goto compare;
              case 884:
                resword = &wordlist[393];
                goto compare;
              case 885:
                resword = &wordlist[394];
                goto compare;
              case 888:
                resword = &wordlist[395];
                goto compare;
              case 895:
                resword = &wordlist[396];
                goto compare;
              case 896:
                resword = &wordlist[397];
                goto compare;
              case 898:
                resword = &wordlist[398];
                goto compare;
              case 899:
                resword = &wordlist[399];
                goto compare;
              case 900:
                resword = &wordlist[400];
                goto compare;
              case 916:
                resword = &wordlist[401];
                goto compare;
              case 917:
                resword = &wordlist[402];
                goto compare;
              case 918:
                resword = &wordlist[403];
                goto compare;
              case 922:
                resword = &wordlist[404];
                goto compare;
              case 923:
                resword = &wordlist[405];
                goto compare;
              case 926:
                resword = &wordlist[406];
                goto compare;
              case 928:
                resword = &wordlist[407];
                goto compare;
              case 930:
                resword = &wordlist[408];
                goto compare;
              case 931:
                resword = &wordlist[409];
                goto compare;
              case 935:
                resword = &wordlist[410];
                goto compare;
              case 936:
                resword = &wordlist[411];
                goto compare;
              case 937:
                resword = &wordlist[412];
                goto compare;
              case 938:
                resword = &wordlist[413];
                goto compare;
              case 943:
                resword = &wordlist[414];
                goto compare;
              case 947:
                resword = &wordlist[415];
                goto compare;
              case 957:
                resword = &wordlist[416];
                goto compare;
              case 958:
                resword = &wordlist[417];
                goto compare;
              case 961:
                resword = &wordlist[418];
                goto compare;
              case 964:
                resword = &wordlist[419];
                goto compare;
              case 978:
                resword = &wordlist[420];
                goto compare;
              case 982:
                resword = &wordlist[421];
                goto compare;
              case 984:
                resword = &wordlist[422];
                goto compare;
              case 991:
                resword = &wordlist[423];
                goto compare;
              case 992:
                resword = &wordlist[424];
                goto compare;
              case 1013:
                resword = &wordlist[425];
                goto compare;
              case 1016:
                resword = &wordlist[426];
                goto compare;
              case 1020:
                resword = &wordlist[427];
                goto compare;
              case 1032:
                resword = &wordlist[428];
                goto compare;
              case 1033:
                resword = &wordlist[429];
                goto compare;
              case 1034:
                resword = &wordlist[430];
                goto compare;
              case 1037:
                resword = &wordlist[431];
                goto compare;
              case 1046:
                resword = &wordlist[432];
                goto compare;
              case 1047:
                resword = &wordlist[433];
                goto compare;
              case 1051:
                resword = &wordlist[434];
                goto compare;
              case 1054:
                resword = &wordlist[435];
                goto compare;
              case 1057:
                resword = &wordlist[436];
                goto compare;
              case 1075:
                resword = &wordlist[437];
                goto compare;
              case 1081:
                resword = &wordlist[438];
                goto compare;
              case 1083:
                resword = &wordlist[439];
                goto compare;
              case 1095:
                resword = &wordlist[440];
                goto compare;
              case 1098:
                resword = &wordlist[441];
                goto compare;
              case 1099:
                resword = &wordlist[442];
                goto compare;
              case 1101:
                resword = &wordlist[443];
                goto compare;
              case 1105:
                resword = &wordlist[444];
                goto compare;
              case 1106:
                resword = &wordlist[445];
                goto compare;
              case 1109:
                resword = &wordlist[446];
                goto compare;
              case 1112:
                resword = &wordlist[447];
                goto compare;
              case 1128:
                resword = &wordlist[448];
                goto compare;
              case 1129:
                resword = &wordlist[449];
                goto compare;
              case 1140:
                resword = &wordlist[450];
                goto compare;
              case 1141:
                resword = &wordlist[451];
                goto compare;
              case 1144:
                resword = &wordlist[452];
                goto compare;
              case 1145:
                resword = &wordlist[453];
                goto compare;
              case 1156:
                resword = &wordlist[454];
                goto compare;
              case 1166:
                resword = &wordlist[455];
                goto compare;
              case 1191:
                resword = &wordlist[456];
                goto compare;
              case 1192:
                resword = &wordlist[457];
                goto compare;
              case 1210:
                resword = &wordlist[458];
                goto compare;
              case 1211:
                resword = &wordlist[459];
                goto compare;
              case 1216:
                resword = &wordlist[460];
                goto compare;
              case 1234:
                resword = &wordlist[461];
                goto compare;
              case 1255:
                resword = &wordlist[462];
                goto compare;
              case 1278:
                resword = &wordlist[463];
                goto compare;
              case 1303:
                resword = &wordlist[464];
                goto compare;
              case 1312:
                resword = &wordlist[465];
                goto compare;
              case 1315:
                resword = &wordlist[466];
                goto compare;
              case 1316:
                resword = &wordlist[467];
                goto compare;
              case 1331:
                resword = &wordlist[468];
                goto compare;
              case 1334:
                resword = &wordlist[469];
                goto compare;
              case 1335:
                resword = &wordlist[470];
                goto compare;
              case 1339:
                resword = &wordlist[471];
                goto compare;
              case 1343:
                resword = &wordlist[472];
                goto compare;
              case 1350:
                resword = &wordlist[473];
                goto compare;
              case 1355:
                resword = &wordlist[474];
                goto compare;
              case 1423:
                resword = &wordlist[475];
                goto compare;
              case 1489:
                resword = &wordlist[476];
                goto compare;
              case 1588:
                resword = &wordlist[477];
                goto compare;
              case 1604:
                resword = &wordlist[478];
                goto compare;
              case 1614:
                resword = &wordlist[479];
                goto compare;
              case 1667:
                resword = &wordlist[480];
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
#line 512 "syscalls.perf"


static int syscall_get_offset_value(const struct arch_syscall_table *s,
				    int offset)
{
	return *(int *)((char *)s + offset);
}

int syscall_resolve_name(const char *name, int offset)
{
	const struct arch_syscall_table *s;

	s = in_word_set(name, strlen(name));
	if (s == NULL)
		return __NR_SCMP_ERROR;

	return syscall_get_offset_value(s, offset);
}

const char *syscall_resolve_num(int num, int offset)
{
	unsigned int iter;

	for (iter = 0; iter < sizeof(wordlist)/sizeof(wordlist[0]); iter++) {
		if (syscall_get_offset_value(&wordlist[iter], offset) == num)
			return (stringpool + wordlist[iter].name);
	}

	return NULL;
}

const struct arch_syscall_def *syscall_iterate(unsigned int spot, int offset)
{
	unsigned int iter;
        /* this is thread-unsafe, only use for testing */
	static struct arch_syscall_def arch_def;

	arch_def.name = NULL;
	arch_def.num = __NR_SCMP_ERROR;

	for (iter = 0; iter < sizeof(wordlist)/sizeof(wordlist[0]); iter++) {
		if (wordlist[iter].index == spot) {
			arch_def.name = stringpool + wordlist[iter].name;
			arch_def.num = syscall_get_offset_value(&wordlist[iter],
								offset);
			return &arch_def;
		}
	}

	return &arch_def;
}
