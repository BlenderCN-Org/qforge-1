dnl ==================================================================
dnl Checks for networking
dnl ==================================================================

LIBCURL_CHECK_CONFIG([], [], [CURL=yes], [])

AC_ARG_WITH(ipv6,
[  --with-ipv6=DIR         enable IPv6 support. Optional argument specifies
                          location of inet6 libraries.],
	if test "x$withval" = xno ; then
		NETTYPE_IPV6=no
	else
		AC_DEFINE(HAVE_IPV6, 1, [Define this if you want IPv6 support])
		NETTYPE_IPV6=yes
		if test "x$withval" != xyes ; then
			LIBS="$LIBS -L${withval}"
		fi
	fi
	,
	NETTYPE_IPV6=no
)
AM_CONDITIONAL(NETTYPE_IPV6, test "x$NETTYPE_IPV6" = "xyes")

if test "x$ac_cv_func_connect" != "xyes"; then
	AC_CHECK_LIB(socket, connect,
					NET_LIBS="$NET_LIBS -lsocket"
					ac_cv_func_connect=yes
				)
fi
if test "x$ac_cv_func_gethostbyname" != "xyes"; then
	AC_CHECK_LIB(nsl, gethostbyname,
					NET_LIBS="$NET_LIBS -lnsl"
					ac_cv_func_gethostbyname=yes
				)
fi

AC_MSG_CHECKING([for connect in -lwsock32])
SAVELIBS="$LIBS"
LIBS="$LIBS -lwsock32"
AC_TRY_LINK([
#include <winsock.h>
],
[
connect(0, NULL, 42);
],
	NET_LIBS="$NET_LIBS -lwsock32 -lwinmm"
	ac_cv_func_connect=yes
	ac_cv_func_gethostbyname=yes
	HAVE_WSOCK=yes
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)
LIBS="$SAVELIBS"

AC_MSG_CHECKING(for UDP support)
if test "x$ac_cv_func_connect" = "xyes" -a "x$ac_cv_func_gethostbyname" = "xyes"; then
	HAVE_UDP=yes
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if test "x$ac_cv_func_connect" != "xyes"; then
	AC_MSG_CHECKING([for connect in -lwsock32])
	SAVELIBS="$LIBS"
	LIBS="$LIBS -lwsock32"
	AC_TRY_LINK([
#include <winsock.h>
		],
		[
connect (0, NULL, 42);
		],
	    NET_LIBS="$NET_LIBS -lwsock32 -lwinmm"
	    AC_MSG_RESULT(yes),
	    AC_MSG_RESULT(no)
	)
	LIBS="$SAVELIBS"
fi
AC_SUBST(NET_LIBS)