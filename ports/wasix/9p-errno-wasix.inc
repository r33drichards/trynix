/* WASIX errno values are not Linux errno values. Generated from QEMU generic target_errno_defs.h. */
if (err == 0) { return 0; }
#ifdef EPERM
if (err == EPERM) { return 1; }
#endif
#ifdef ENOENT
if (err == ENOENT) { return 2; }
#endif
#ifdef ESRCH
if (err == ESRCH) { return 3; }
#endif
#ifdef EINTR
if (err == EINTR) { return 4; }
#endif
#ifdef EIO
if (err == EIO) { return 5; }
#endif
#ifdef ENXIO
if (err == ENXIO) { return 6; }
#endif
#ifdef E2BIG
if (err == E2BIG) { return 7; }
#endif
#ifdef ENOEXEC
if (err == ENOEXEC) { return 8; }
#endif
#ifdef EBADF
if (err == EBADF) { return 9; }
#endif
#ifdef ECHILD
if (err == ECHILD) { return 10; }
#endif
#ifdef EAGAIN
if (err == EAGAIN) { return 11; }
#endif
#ifdef ENOMEM
if (err == ENOMEM) { return 12; }
#endif
#ifdef EACCES
if (err == EACCES) { return 13; }
#endif
#ifdef EFAULT
if (err == EFAULT) { return 14; }
#endif
#ifdef ENOTBLK
if (err == ENOTBLK) { return 15; }
#endif
#ifdef EBUSY
if (err == EBUSY) { return 16; }
#endif
#ifdef EEXIST
if (err == EEXIST) { return 17; }
#endif
#ifdef EXDEV
if (err == EXDEV) { return 18; }
#endif
#ifdef ENODEV
if (err == ENODEV) { return 19; }
#endif
#ifdef ENOTDIR
if (err == ENOTDIR) { return 20; }
#endif
#ifdef EISDIR
if (err == EISDIR) { return 21; }
#endif
#ifdef EINVAL
if (err == EINVAL) { return 22; }
#endif
#ifdef ENFILE
if (err == ENFILE) { return 23; }
#endif
#ifdef EMFILE
if (err == EMFILE) { return 24; }
#endif
#ifdef ENOTTY
if (err == ENOTTY) { return 25; }
#endif
#ifdef ETXTBSY
if (err == ETXTBSY) { return 26; }
#endif
#ifdef EFBIG
if (err == EFBIG) { return 27; }
#endif
#ifdef ENOSPC
if (err == ENOSPC) { return 28; }
#endif
#ifdef ESPIPE
if (err == ESPIPE) { return 29; }
#endif
#ifdef EROFS
if (err == EROFS) { return 30; }
#endif
#ifdef EMLINK
if (err == EMLINK) { return 31; }
#endif
#ifdef EPIPE
if (err == EPIPE) { return 32; }
#endif
#ifdef EDOM
if (err == EDOM) { return 33; }
#endif
#ifdef ERANGE
if (err == ERANGE) { return 34; }
#endif
#ifdef EDEADLK
if (err == EDEADLK) { return 35; }
#endif
#ifdef ENAMETOOLONG
if (err == ENAMETOOLONG) { return 36; }
#endif
#ifdef ENOLCK
if (err == ENOLCK) { return 37; }
#endif
#ifdef ENOSYS
if (err == ENOSYS) { return 38; }
#endif
#ifdef ENOTEMPTY
if (err == ENOTEMPTY) { return 39; }
#endif
#ifdef ELOOP
if (err == ELOOP) { return 40; }
#endif
#ifdef ENOMSG
if (err == ENOMSG) { return 42; }
#endif
#ifdef EIDRM
if (err == EIDRM) { return 43; }
#endif
#ifdef ECHRNG
if (err == ECHRNG) { return 44; }
#endif
#ifdef EL2NSYNC
if (err == EL2NSYNC) { return 45; }
#endif
#ifdef EL3HLT
if (err == EL3HLT) { return 46; }
#endif
#ifdef EL3RST
if (err == EL3RST) { return 47; }
#endif
#ifdef ELNRNG
if (err == ELNRNG) { return 48; }
#endif
#ifdef EUNATCH
if (err == EUNATCH) { return 49; }
#endif
#ifdef ENOCSI
if (err == ENOCSI) { return 50; }
#endif
#ifdef EL2HLT
if (err == EL2HLT) { return 51; }
#endif
#ifdef EBADE
if (err == EBADE) { return 52; }
#endif
#ifdef EBADR
if (err == EBADR) { return 53; }
#endif
#ifdef EXFULL
if (err == EXFULL) { return 54; }
#endif
#ifdef ENOANO
if (err == ENOANO) { return 55; }
#endif
#ifdef EBADRQC
if (err == EBADRQC) { return 56; }
#endif
#ifdef EBADSLT
if (err == EBADSLT) { return 57; }
#endif
#ifdef EBFONT
if (err == EBFONT) { return 59; }
#endif
#ifdef ENOSTR
if (err == ENOSTR) { return 60; }
#endif
#ifdef ENODATA
if (err == ENODATA) { return 61; }
#endif
#ifdef ETIME
if (err == ETIME) { return 62; }
#endif
#ifdef ENOSR
if (err == ENOSR) { return 63; }
#endif
#ifdef ENONET
if (err == ENONET) { return 64; }
#endif
#ifdef ENOPKG
if (err == ENOPKG) { return 65; }
#endif
#ifdef EREMOTE
if (err == EREMOTE) { return 66; }
#endif
#ifdef ENOLINK
if (err == ENOLINK) { return 67; }
#endif
#ifdef EADV
if (err == EADV) { return 68; }
#endif
#ifdef ESRMNT
if (err == ESRMNT) { return 69; }
#endif
#ifdef ECOMM
if (err == ECOMM) { return 70; }
#endif
#ifdef EPROTO
if (err == EPROTO) { return 71; }
#endif
#ifdef EMULTIHOP
if (err == EMULTIHOP) { return 72; }
#endif
#ifdef EDOTDOT
if (err == EDOTDOT) { return 73; }
#endif
#ifdef EBADMSG
if (err == EBADMSG) { return 74; }
#endif
#ifdef EOVERFLOW
if (err == EOVERFLOW) { return 75; }
#endif
#ifdef ENOTUNIQ
if (err == ENOTUNIQ) { return 76; }
#endif
#ifdef EBADFD
if (err == EBADFD) { return 77; }
#endif
#ifdef EREMCHG
if (err == EREMCHG) { return 78; }
#endif
#ifdef ELIBACC
if (err == ELIBACC) { return 79; }
#endif
#ifdef ELIBBAD
if (err == ELIBBAD) { return 80; }
#endif
#ifdef ELIBSCN
if (err == ELIBSCN) { return 81; }
#endif
#ifdef ELIBMAX
if (err == ELIBMAX) { return 82; }
#endif
#ifdef ELIBEXEC
if (err == ELIBEXEC) { return 83; }
#endif
#ifdef EILSEQ
if (err == EILSEQ) { return 84; }
#endif
#ifdef ERESTART
if (err == ERESTART) { return 85; }
#endif
#ifdef ESTRPIPE
if (err == ESTRPIPE) { return 86; }
#endif
#ifdef EUSERS
if (err == EUSERS) { return 87; }
#endif
#ifdef ENOTSOCK
if (err == ENOTSOCK) { return 88; }
#endif
#ifdef EDESTADDRREQ
if (err == EDESTADDRREQ) { return 89; }
#endif
#ifdef EMSGSIZE
if (err == EMSGSIZE) { return 90; }
#endif
#ifdef EPROTOTYPE
if (err == EPROTOTYPE) { return 91; }
#endif
#ifdef ENOPROTOOPT
if (err == ENOPROTOOPT) { return 92; }
#endif
#ifdef EPROTONOSUPPORT
if (err == EPROTONOSUPPORT) { return 93; }
#endif
#ifdef ESOCKTNOSUPPORT
if (err == ESOCKTNOSUPPORT) { return 94; }
#endif
#ifdef EOPNOTSUPP
if (err == EOPNOTSUPP) { return 95; }
#endif
#ifdef EPFNOSUPPORT
if (err == EPFNOSUPPORT) { return 96; }
#endif
#ifdef EAFNOSUPPORT
if (err == EAFNOSUPPORT) { return 97; }
#endif
#ifdef EADDRINUSE
if (err == EADDRINUSE) { return 98; }
#endif
#ifdef EADDRNOTAVAIL
if (err == EADDRNOTAVAIL) { return 99; }
#endif
#ifdef ENETDOWN
if (err == ENETDOWN) { return 100; }
#endif
#ifdef ENETUNREACH
if (err == ENETUNREACH) { return 101; }
#endif
#ifdef ENETRESET
if (err == ENETRESET) { return 102; }
#endif
#ifdef ECONNABORTED
if (err == ECONNABORTED) { return 103; }
#endif
#ifdef ECONNRESET
if (err == ECONNRESET) { return 104; }
#endif
#ifdef ENOBUFS
if (err == ENOBUFS) { return 105; }
#endif
#ifdef EISCONN
if (err == EISCONN) { return 106; }
#endif
#ifdef ENOTCONN
if (err == ENOTCONN) { return 107; }
#endif
#ifdef ESHUTDOWN
if (err == ESHUTDOWN) { return 108; }
#endif
#ifdef ETOOMANYREFS
if (err == ETOOMANYREFS) { return 109; }
#endif
#ifdef ETIMEDOUT
if (err == ETIMEDOUT) { return 110; }
#endif
#ifdef ECONNREFUSED
if (err == ECONNREFUSED) { return 111; }
#endif
#ifdef EHOSTDOWN
if (err == EHOSTDOWN) { return 112; }
#endif
#ifdef EHOSTUNREACH
if (err == EHOSTUNREACH) { return 113; }
#endif
#ifdef EALREADY
if (err == EALREADY) { return 114; }
#endif
#ifdef EINPROGRESS
if (err == EINPROGRESS) { return 115; }
#endif
#ifdef ESTALE
if (err == ESTALE) { return 116; }
#endif
#ifdef EUCLEAN
if (err == EUCLEAN) { return 117; }
#endif
#ifdef ENOTNAM
if (err == ENOTNAM) { return 118; }
#endif
#ifdef ENAVAIL
if (err == ENAVAIL) { return 119; }
#endif
#ifdef EISNAM
if (err == EISNAM) { return 120; }
#endif
#ifdef EREMOTEIO
if (err == EREMOTEIO) { return 121; }
#endif
#ifdef EDQUOT
if (err == EDQUOT) { return 122; }
#endif
#ifdef ENOMEDIUM
if (err == ENOMEDIUM) { return 123; }
#endif
#ifdef EMEDIUMTYPE
if (err == EMEDIUMTYPE) { return 124; }
#endif
#ifdef ECANCELED
if (err == ECANCELED) { return 125; }
#endif
#ifdef ENOKEY
if (err == ENOKEY) { return 126; }
#endif
#ifdef EKEYEXPIRED
if (err == EKEYEXPIRED) { return 127; }
#endif
#ifdef EKEYREVOKED
if (err == EKEYREVOKED) { return 128; }
#endif
#ifdef EKEYREJECTED
if (err == EKEYREJECTED) { return 129; }
#endif
#ifdef EOWNERDEAD
if (err == EOWNERDEAD) { return 130; }
#endif
#ifdef ENOTRECOVERABLE
if (err == ENOTRECOVERABLE) { return 131; }
#endif
#ifdef ERFKILL
if (err == ERFKILL) { return 132; }
#endif
#ifdef EHWPOISON
if (err == EHWPOISON) { return 133; }
#endif
return 5; /* Unknown host error: Linux EIO, never forward an unrelated errno. */
