#	from: @(#)Makefile	5.7 (Berkeley) 4/1/91
#	$OpenBSD: Makefile,v 1.53 2013/08/07 22:07:32 dlg Exp $

.include <bsd.own.mk>

SUBDIR=	comsat fingerd ftpd getty ld.so lockspool \
	mail.local makewhatis rpc.rquotad rpc.rstatd rpc.rusersd \
	rpc.rwalld rpc.sprayd security spamd spamlogd spamd-setup rshd \
	talkd tcpd uucpd

SUBDIR+=login_passwd login_skey login_reject login_chpass \
	login_lchpass login_token login_radius login_tis login_yubikey

.if (${YP:L} == "yes")
SUBDIR+=rpc.yppasswdd
.endif

.if (${KERBEROS5:L} == "yes")
SUBDIR+=login_krb5 login_krb5-or-pwd
.endif

.include <bsd.subdir.mk>
