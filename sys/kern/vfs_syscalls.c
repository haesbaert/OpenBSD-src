/*	$NetBSD: vfs_syscalls.c,v 1.57 1995/10/07 06:28:51 mycroft Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vfs_syscalls.c	8.28 (Berkeley) 12/10/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/dirent.h>

#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <sys/sysctl.h>

static int change_dir __P((struct nameidata *ndp, struct proc *p));

/*
 * Virtual File System System Calls
 */

/*
 * Mount a file system.
 */
/* ARGSUSED */
sys_mount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mount_args /* {
		syscallarg(char *) type;
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	register struct vnode *vp;
	register struct mount *mp;
	int error, flag;
	u_long fsindex;
	char fstypename[MFSNAMELEN];
	struct vattr va;
	struct nameidata nd;

	/*
	 * Get vnode to be covered
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (SCARG(uap, flags) & MNT_UPDATE) {
		if ((vp->v_flag & VROOT) == 0) {
			vput(vp);
			return (EINVAL);
		}
		mp = vp->v_mount;
		flag = mp->mnt_flag;
		/*
		 * We only allow the filesystem to be reloaded if it
		 * is currently mounted read-only.
		 */
		if ((SCARG(uap, flags) & MNT_RELOAD) &&
		    ((mp->mnt_flag & MNT_RDONLY) == 0)) {
			vput(vp);
			return (EOPNOTSUPP);	/* Needs translation */
		}
		mp->mnt_flag |=
		    SCARG(uap, flags) & (MNT_RELOAD | MNT_FORCE | MNT_UPDATE);
		/*
		 * Only root, or the user that did the original mount is
		 * permitted to update it.
		 */
		if (mp->mnt_stat.f_owner != p->p_ucred->cr_uid &&
		    (error = suser(p->p_ucred, &p->p_acflag))) {
			vput(vp);
			return (error);
		}
		/*
		 * Do not allow NFS export by non-root users. Silently
		 * enforce MNT_NOSUID and MNT_NODEV for non-root users.
		 */
		if (p->p_ucred->cr_uid != 0) {
			if (SCARG(uap, flags) & MNT_EXPORTED) {
				vput(vp);
				return (EPERM);
			}
			SCARG(uap, flags) |= MNT_NOSUID | MNT_NODEV;
		}
		VOP_UNLOCK(vp);
		goto update;
	}
	/*
	 * If the user is not root, ensure that they own the directory
	 * onto which we are attempting to mount.
	 */
	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) ||
	    (va.va_uid != p->p_ucred->cr_uid &&
	     (error = suser(p->p_ucred, &p->p_acflag)))) {
		vput(vp);
		return (error);
	}
	/*
	 * Do not allow NFS export by non-root users. Silently
	 * enforce MNT_NOSUID and MNT_NODEV for non-root users.
	 */
	if (p->p_ucred->cr_uid != 0) {
		if (SCARG(uap, flags) & MNT_EXPORTED) {
			vput(vp);
			return (EPERM);
		}
		SCARG(uap, flags) |= MNT_NOSUID | MNT_NODEV;
	}
	if (error = vinvalbuf(vp, V_SAVE, p->p_ucred, p, 0, 0))
		return (error);
	if (vp->v_type != VDIR) {
		vput(vp);
		return (ENOTDIR);
	}
	if (error = copyinstr(SCARG(uap, type), fstypename, MFSNAMELEN,
	    (size_t *)0)) {
#if defined(COMPAT_09) || defined(COMPAT_43)
		/*
		 * Historically filesystem types were identified by number.
		 * If we get an integer for the filesystem type instead of a
		 * string, we check to see if it matches one of the historic
		 * filesystem types.
		 */     
		fsindex = (u_long)SCARG(uap, type);
		if (fsindex >= nvfssw || vfssw[fsindex] == NULL) {
			vput(vp);
			return (ENODEV);
		}
		strncpy(fstypename, vfssw[fsindex]->vfs_name, MFSNAMELEN);
#else
		vput(vp);
		return (error);
#endif
	}
#ifdef	COMPAT_10
	/* Accept "ufs" as a synonym for "ffs" */
	if (!strncmp(fstypename, "ufs", MFSNAMELEN)) {
		strncpy( fstypename, "ffs", MFSNAMELEN);
	}
#endif
	for (fsindex = 0; fsindex < nvfssw; fsindex++)
		if (vfssw[fsindex] != NULL &&
		    !strncmp(vfssw[fsindex]->vfs_name, fstypename, MFSNAMELEN))
			break;
	if (fsindex >= nvfssw) {
		vput(vp);
		return (ENODEV);
	}
	if (vp->v_mountedhere != NULL) {
		vput(vp);
		return (EBUSY);
	}

	/*
	 * Allocate and initialize the file system.
	 */
	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
		M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op = vfssw[fsindex];
	if (error = vfs_lock(mp)) {
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
		return (error);
	}
	/* Do this early in case we block later. */
	vfssw[fsindex]->vfs_refcount++;
	vp->v_mountedhere = mp;
	mp->mnt_vnodecovered = vp;
	mp->mnt_stat.f_owner = p->p_ucred->cr_uid;
update:
	/*
	 * Set the mount level flags.
	 */
	if (SCARG(uap, flags) & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	else if (mp->mnt_flag & MNT_RDONLY)
		mp->mnt_flag |= MNT_WANTRDWR;
	mp->mnt_flag &=~ (MNT_NOSUID | MNT_NOEXEC | MNT_NODEV |
	    MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC);
	mp->mnt_flag |= SCARG(uap, flags) & (MNT_NOSUID | MNT_NOEXEC |
	    MNT_NODEV | MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC);
	/*
	 * Mount the filesystem.
	 */
	error = VFS_MOUNT(mp, SCARG(uap, path), SCARG(uap, data), &nd, p);
	if (mp->mnt_flag & MNT_UPDATE) {
		vrele(vp);
		if (mp->mnt_flag & MNT_WANTRDWR)
			mp->mnt_flag &= ~MNT_RDONLY;
		mp->mnt_flag &=~
		    (MNT_UPDATE | MNT_RELOAD | MNT_FORCE | MNT_WANTRDWR);
		if (error)
			mp->mnt_flag = flag;
		return (error);
	}
	/*
	 * Put the new filesystem on the mount list after root.
	 */
	cache_purge(vp);
	if (!error) {
		CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
		checkdirs(vp);
		VOP_UNLOCK(vp);
		vfs_unlock(mp);
		(void) VFS_STATFS(mp, &mp->mnt_stat, p);
		error = VFS_START(mp, 0, p);
	} else {
		mp->mnt_vnodecovered->v_mountedhere = (struct mount *)0;
		vfssw[fsindex]->vfs_refcount--;
		vfs_unlock(mp);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
	}
	return (error);
}

/*
 * Scan all active processes to see if any of them have a current
 * or root directory onto which the new filesystem has just been
 * mounted. If so, replace them with the new mount point.
 */
checkdirs(olddp)
	struct vnode *olddp;
{
	struct filedesc *fdp;
	struct vnode *newdp;
	struct proc *p;

	if (olddp->v_usecount == 1)
		return;
	if (VFS_ROOT(olddp->v_mountedhere, &newdp))
		panic("mount: lost mount");
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		fdp = p->p_fd;
		if (fdp->fd_cdir == olddp) {
			vrele(fdp->fd_cdir);
			VREF(newdp);
			fdp->fd_cdir = newdp;
		}
		if (fdp->fd_rdir == olddp) {
			vrele(fdp->fd_rdir);
			VREF(newdp);
			fdp->fd_rdir = newdp;
		}
	}
	if (rootvnode == olddp) {
		vrele(rootvnode);
		VREF(newdp);
		rootvnode = newdp;
	}
	vput(newdp);
}

/*
 * Unmount a file system.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
/* ARGSUSED */
sys_unmount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_unmount_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	register struct vnode *vp;
	struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	mp = vp->v_mount;

	/*
	 * Only root, or the user that did the original mount is
	 * permitted to unmount this filesystem.
	 */
	if ((mp->mnt_stat.f_owner != p->p_ucred->cr_uid) &&
	    (error = suser(p->p_ucred, &p->p_acflag))) {
		vput(vp);
		return (error);
	}

	/*
	 * Don't allow unmounting the root file system.
	 */
	if (mp->mnt_flag & MNT_ROOTFS) {
		vput(vp);
		return (EINVAL);
	}

	/*
	 * Must be the root of the filesystem
	 */
	if ((vp->v_flag & VROOT) == 0) {
		vput(vp);
		return (EINVAL);
	}
	vput(vp);
	return (dounmount(mp, SCARG(uap, flags), p));
}

/*
 * Do the actual file system unmount.
 */
dounmount(mp, flags, p)
	register struct mount *mp;
	int flags;
	struct proc *p;
{
	struct vnode *coveredvp;
	int error;

	coveredvp = mp->mnt_vnodecovered;
	if (vfs_busy(mp))
		return (EBUSY);
	mp->mnt_flag |= MNT_UNMOUNT;
	if (error = vfs_lock(mp))
		return (error);

	mp->mnt_flag &=~ MNT_ASYNC;
	vnode_pager_umount(mp);	/* release cached vnodes */
	cache_purgevfs(mp);	/* remove cache entries for this file sys */
	if ((error = VFS_SYNC(mp, MNT_WAIT, p->p_ucred, p)) == 0 ||
	    (flags & MNT_FORCE))
		error = VFS_UNMOUNT(mp, flags, p);
	mp->mnt_flag &= ~MNT_UNMOUNT;
	vfs_unbusy(mp);
	if (error) {
		vfs_unlock(mp);
	} else {
		CIRCLEQ_REMOVE(&mountlist, mp, mnt_list);
		if (coveredvp != NULLVP) {
			vrele(coveredvp);
			coveredvp->v_mountedhere = (struct mount *)0;
		}
		mp->mnt_op->vfs_refcount--;
		vfs_unlock(mp);
		if (mp->mnt_vnodelist.lh_first != NULL)
			panic("unmount: dangling vnode");
		free((caddr_t)mp, M_MOUNT);
	}
	return (error);
}

/*
 * Sync each mounted filesystem.
 */
#ifdef DEBUG
int syncprt = 0;
struct ctldebug debug0 = { "syncprt", &syncprt };
#endif

/* ARGSUSED */
sys_sync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct mount *mp, *nmp;
	int asyncflag;

	for (mp = mountlist.cqh_first; mp != (void *)&mountlist; mp = nmp) {
		/*
		 * Get the next pointer in case we hang on vfs_busy
		 * while we are being unmounted.
		 */
		nmp = mp->mnt_list.cqe_next;
		/*
		 * The lock check below is to avoid races with mount
		 * and unmount.
		 */
		if ((mp->mnt_flag & (MNT_MLOCK|MNT_RDONLY|MNT_MPBUSY)) == 0 &&
		    !vfs_busy(mp)) {
			asyncflag = mp->mnt_flag & MNT_ASYNC;
			mp->mnt_flag &= ~MNT_ASYNC;
			VFS_SYNC(mp, MNT_NOWAIT, p->p_ucred, p);
			if (asyncflag)
				mp->mnt_flag |= MNT_ASYNC;
			/*
			 * Get the next pointer again, as the next filesystem
			 * might have been unmounted while we were sync'ing.
			 */
			nmp = mp->mnt_list.cqe_next;
			vfs_unbusy(mp);
		}
	}
#ifdef DEBUG
	if (syncprt)
		vfs_bufstats();
#endif /* DEBUG */
	return (0);
}

/*
 * Change filesystem quotas.
 */
/* ARGSUSED */
sys_quotactl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_quotactl_args /* {
		syscallarg(char *) path;
		syscallarg(int) cmd;
		syscallarg(int) uid;
		syscallarg(caddr_t) arg;
	} */ *uap = v;
	register struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	mp = nd.ni_vp->v_mount;
	vrele(nd.ni_vp);
	return (VFS_QUOTACTL(mp, SCARG(uap, cmd), SCARG(uap, uid),
	    SCARG(uap, arg), p));
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_statfs_args /* {
		syscallarg(char *) path;
		syscallarg(struct statfs *) buf;
	} */ *uap = v;
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return (copyout((caddr_t)sp, (caddr_t)SCARG(uap, buf), sizeof(*sp)));
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
sys_fstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_fstatfs_args /* {
		syscallarg(int) fd;
		syscallarg(struct statfs *) buf;
	} */ *uap = v;
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return (copyout((caddr_t)sp, (caddr_t)SCARG(uap, buf), sizeof(*sp)));
}

/*
 * Get statistics on all filesystems.
 */
sys_getfsstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getfsstat_args /* {
		syscallarg(struct statfs *) buf;
		syscallarg(long) bufsize;
		syscallarg(int) flags;
	} */ *uap = v;
	register struct mount *mp, *nmp;
	register struct statfs *sp;
	caddr_t sfsp;
	long count, maxcount, error;

	maxcount = SCARG(uap, bufsize) / sizeof(struct statfs);
	sfsp = (caddr_t)SCARG(uap, buf);
	for (count = 0,
	     mp = mountlist.cqh_first; mp != (void *)&mountlist; mp = nmp) {
		nmp = mp->mnt_list.cqe_next;
		if (sfsp && count < maxcount &&
		    ((mp->mnt_flag & MNT_MLOCK) == 0)) {
			sp = &mp->mnt_stat;
			/*
			 * If MNT_NOWAIT is specified, do not refresh the
			 * fsstat cache. MNT_WAIT overrides MNT_NOWAIT.
			 */
			if (((SCARG(uap, flags) & MNT_NOWAIT) == 0 ||
			    (SCARG(uap, flags) & MNT_WAIT)) &&
			    (error = VFS_STATFS(mp, sp, p)))
				continue;
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
			if (error = copyout((caddr_t)sp, sfsp, sizeof(*sp)))
				return (error);
			sfsp += sizeof(*sp);
		}
		count++;
	}
	if (sfsp && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;
	return (0);
}

/*
 * Change current working directory to a given file descriptor.
 */
/* ARGSUSED */
sys_fchdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fchdir_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	struct vnode *vp, *tdp;
	struct mount *mp;
	struct file *fp;
	int error;

	if (error = getvnode(fdp, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VREF(vp);
	VOP_LOCK(vp);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	while (!error && (mp = vp->v_mountedhere) != NULL) {
		if (mp->mnt_flag & MNT_MLOCK) {
			mp->mnt_flag |= MNT_MWAIT;
			sleep((caddr_t)mp, PVFS);
			continue;
		}
		if (error = VFS_ROOT(mp, &tdp))
			break;
		vput(vp);
		vp = tdp;
	}
	VOP_UNLOCK(vp);
	if (error) {
		vrele(vp);
		return (error);
	}
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = vp;
	return (0);
}

/*
 * Change current working directory (``.'').
 */
/* ARGSUSED */
sys_chdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_chdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = change_dir(&nd, p))
		return (error);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = nd.ni_vp;
	return (0);
}

/*
 * Change notion of root (``/'') directory.
 */
/* ARGSUSED */
sys_chroot(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_chroot_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = change_dir(&nd, p))
		return (error);
	if (fdp->fd_rdir != NULL)
		vrele(fdp->fd_rdir);
	fdp->fd_rdir = nd.ni_vp;
	return (0);
}

/*
 * Common routine for chroot and chdir.
 */
static int
change_dir(ndp, p)
	register struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *vp;
	int error;

	if (error = namei(ndp))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	VOP_UNLOCK(vp);
	if (error)
		vrele(vp);
	return (error);
}

/*
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	register struct vnode *vp;
	int flags, cmode;
	struct file *nfp;
	int type, indx, error;
	struct flock lf;
	struct nameidata nd;
	extern struct fileops vnops;

	if (error = falloc(p, &nfp, &indx))
		return (error);
	fp = nfp;
	flags = FFLAGS(SCARG(uap, flags));
	cmode = ((SCARG(uap, mode) &~ fdp->fd_cmask) & ALLPERMS) &~ S_ISTXT;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	p->p_dupfd = -indx - 1;			/* XXX check for fdopen */
	if (error = vn_open(&nd, flags, cmode)) {
		ffree(fp);
		if ((error == ENODEV || error == ENXIO) &&
		    p->p_dupfd >= 0 &&			/* XXX from fdopen */
		    (error =
			dupfdopen(fdp, indx, p->p_dupfd, flags, error)) == 0) {
			*retval = indx;
			return (0);
		}
		if (error == ERESTART)
			error = EINTR;
		fdp->fd_ofiles[indx] = NULL;
		return (error);
	}
	p->p_dupfd = 0;
	vp = nd.ni_vp;
	fp->f_flag = flags & FMASK;
	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnops;
	fp->f_data = (caddr_t)vp;
	if (flags & (O_EXLOCK | O_SHLOCK)) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		if (flags & O_EXLOCK)
			lf.l_type = F_WRLCK;
		else
			lf.l_type = F_RDLCK;
		type = F_FLOCK;
		if ((flags & FNONBLOCK) == 0)
			type |= F_WAIT;
		VOP_UNLOCK(vp);
		if (error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, type)) {
			(void) vn_close(vp, fp->f_flag, fp->f_cred, p);
			ffree(fp);
			fdp->fd_ofiles[indx] = NULL;
			return (error);
		}
		VOP_LOCK(vp);
		fp->f_flag |= FHASLOCK;
	}
	VOP_UNLOCK(vp);
	*retval = indx;
	return (0);
}

/*
 * Create a special file.
 */
/* ARGSUSED */
sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mknod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
		syscallarg(int) dev;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	int whiteout;
	struct nameidata nd;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL)
		error = EEXIST;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = (SCARG(uap, mode) & ALLPERMS) &~ p->p_fd->fd_cmask;
		vattr.va_rdev = SCARG(uap, dev);
		whiteout = 0;

		switch (SCARG(uap, mode) & S_IFMT) {
		case S_IFMT:	/* used by badsect to flag bad sectors */
			vattr.va_type = VBAD;
			break;
		case S_IFCHR:
			vattr.va_type = VCHR;
			break;
		case S_IFBLK:
			vattr.va_type = VBLK;
			break;
		case S_IFWHT:
			whiteout = 1;
			break;
		default:
			error = EINVAL;
			break;
		}
	}
	if (!error) {
		VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		if (whiteout) {
			error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, CREATE);
			if (error)
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			vput(nd.ni_dvp);
		} else {
			error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp,
						&nd.ni_cnd, &vattr);
		}
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp)
			vrele(vp);
	}
	return (error);
}

/*
 * Create a named pipe.
 */
/* ARGSUSED */
sys_mkfifo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mkfifo_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	struct vattr vattr;
	int error;
	struct nameidata nd;

#ifndef FIFO
	return (EOPNOTSUPP);
#else
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	if (nd.ni_vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		return (EEXIST);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VFIFO;
	vattr.va_mode = (SCARG(uap, mode) & ALLPERMS) &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	return (VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr));
#endif /* FIFO */
}

/*
 * Make a hard file link.
 */
/* ARGSUSED */
sys_link(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_link_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap = v;
	register struct vnode *vp;
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VDIR ||
	    (error = suser(p->p_ucred, &p->p_acflag)) == 0) {
		nd.ni_cnd.cn_nameiop = CREATE;
		nd.ni_cnd.cn_flags = LOCKPARENT;
		nd.ni_dirp = SCARG(uap, link);
		if ((error = namei(&nd)) == 0) {
			if (nd.ni_vp != NULL)
				error = EEXIST;
			if (!error) {
				VOP_LEASE(nd.ni_dvp, p, p->p_ucred,
				    LEASE_WRITE);
				VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
				error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
			} else {
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				if (nd.ni_dvp == nd.ni_vp)
					vrele(nd.ni_dvp);
				else
					vput(nd.ni_dvp);
				if (nd.ni_vp)
					vrele(nd.ni_vp);
			}
		}
	}
	vrele(vp);
	return (error);
}

/*
 * Make a symbolic link.
 */
/* ARGSUSED */
sys_symlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_symlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap = v;
	struct vattr vattr;
	char *path;
	int error;
	struct nameidata nd;

	MALLOC(path, char *, MAXPATHLEN, M_NAMEI, M_WAITOK);
	if (error = copyinstr(SCARG(uap, path), path, MAXPATHLEN, (size_t *)0))
		goto out;
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, link), p);
	if (error = namei(&nd))
		goto out;
	if (nd.ni_vp) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		error = EEXIST;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_mode = ACCESSPERMS &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr, path);
out:
	FREE(path, M_NAMEI);
	return (error);
}

/*
 * Delete a whiteout from the filesystem.
 */
/* ARGSUSED */
sys_undelete(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_undelete_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT|DOWHITEOUT, UIO_USERSPACE,
	    SCARG(uap, path), p);
	error = namei(&nd);
	if (error)
		return (error);

	if (nd.ni_vp != NULLVP || !(nd.ni_cnd.cn_flags & ISWHITEOUT)) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		return (EEXIST);
	}

	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	if (error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, DELETE))
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	vput(nd.ni_dvp);
	return (error);
}

/*
 * Delete a name from the filesystem.
 */
/* ARGSUSED */
sys_unlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_unlink_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);

	if (vp->v_type != VDIR ||
	    (error = suser(p->p_ucred, &p->p_acflag)) == 0) {
		/*
		 * The root of a mounted filesystem cannot be deleted.
		 */
		if (vp->v_flag & VROOT)
			error = EBUSY;
		else
			(void)vnode_pager_uncache(vp);
	}

	if (!error) {
		VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp != NULLVP)
			vput(vp);
	}
	return (error);
}

/*
 * Reposition read/write file offset.
 */
sys_lseek(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_lseek_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
		syscallarg(int) whence;
	} */ *uap = v;
	struct ucred *cred = p->p_ucred;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vattr vattr;
	int error;

	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (ESPIPE);
	switch (SCARG(uap, whence)) {
	case L_INCR:
		fp->f_offset += SCARG(uap, offset);
		break;
	case L_XTND:
		if (error =
		    VOP_GETATTR((struct vnode *)fp->f_data, &vattr, cred, p))
			return (error);
		fp->f_offset = SCARG(uap, offset) + vattr.va_size;
		break;
	case L_SET:
		fp->f_offset = SCARG(uap, offset);
		break;
	default:
		return (EINVAL);
	}
	*(off_t *)retval = fp->f_offset;
	return (0);
}

/*
 * Check access permissions.
 */
sys_access(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_access_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	register struct ucred *cred = p->p_ucred;
	register struct vnode *vp;
	int error, flags, t_gid, t_uid;
	struct nameidata nd;

	t_uid = cred->cr_uid;
	t_gid = cred->cr_gid;
	cred->cr_uid = p->p_cred->p_ruid;
	cred->cr_gid = p->p_cred->p_rgid;
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		goto out1;
	vp = nd.ni_vp;

	/* Flags == 0 means only check for existence. */
	if (SCARG(uap, flags)) {
		flags = 0;
		if (SCARG(uap, flags) & R_OK)
			flags |= VREAD;
		if (SCARG(uap, flags) & W_OK)
			flags |= VWRITE;
		if (SCARG(uap, flags) & X_OK)
			flags |= VEXEC;
		if ((flags & VWRITE) == 0 || (error = vn_writechk(vp)) == 0)
			error = VOP_ACCESS(vp, flags, cred, p);
	}
	vput(vp);
out1:
	cred->cr_uid = t_uid;
	cred->cr_gid = t_gid;
	return (error);
}

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap = v;
	struct stat sb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&sb, (caddr_t)SCARG(uap, ub), sizeof (sb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap = v;
	int error;
	struct vnode *vp, *dvp;
	struct stat sb, sb1;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | LOCKPARENT, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	/*
	 * For symbolic links, always return the attributes of its
	 * containing directory, except for mode, size, and links.
	 */
	vp = nd.ni_vp;
	dvp = nd.ni_dvp;
	if (vp->v_type != VLNK) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
		error = vn_stat(vp, &sb, p);
		vput(vp);
		if (error)
			return (error);
	} else {
		error = vn_stat(dvp, &sb, p);
		vput(dvp);
		if (error) {
			vput(vp);
			return (error);
		}
		error = vn_stat(vp, &sb1, p);
		vput(vp);
		if (error)
			return (error);
		sb.st_mode &= ~S_IFDIR;
		sb.st_mode |= S_IFLNK;
		sb.st_nlink = sb1.st_nlink;
		sb.st_size = sb1.st_size;
		sb.st_blocks = sb1.st_blocks;
	}
	error = copyout((caddr_t)&sb, (caddr_t)SCARG(uap, ub), sizeof (sb));
	return (error);
}

/*
 * Get configurable pathname variables.
 */
/* ARGSUSED */
sys_pathconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_pathconf_args /* {
		syscallarg(char *) path;
		syscallarg(int) name;
	} */ *uap = v;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	error = VOP_PATHCONF(nd.ni_vp, SCARG(uap, name), retval);
	vput(nd.ni_vp);
	return (error);
}

/*
 * Return target name of a symbolic link.
 */
/* ARGSUSED */
sys_readlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_readlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) buf;
		syscallarg(int) count;
	} */ *uap = v;
	register struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VLNK)
		error = EINVAL;
	else {
		aiov.iov_base = SCARG(uap, buf);
		aiov.iov_len = SCARG(uap, count);
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_procp = p;
		auio.uio_resid = SCARG(uap, count);
		error = VOP_READLINK(vp, &auio, p->p_ucred);
	}
	vput(vp);
	*retval = SCARG(uap, count) - auio.uio_resid;
	return (error);
}

/*
 * Change flags of a file given a path name.
 */
/* ARGSUSED */
sys_chflags(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_chflags_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_flags = SCARG(uap, flags);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Change flags of a file given a file descriptor.
 */
/* ARGSUSED */
sys_fchflags(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_fchflags_args /* {
		syscallarg(int) fd;
		syscallarg(int) flags;
	} */ *uap = v;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_flags = SCARG(uap, flags);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Change mode of a file given path name.
 */
/* ARGSUSED */
sys_chmod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_chmod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = SCARG(uap, mode) & ALLPERMS;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Change mode of a file given a file descriptor.
 */
/* ARGSUSED */
sys_fchmod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_fchmod_args /* {
		syscallarg(int) fd;
		syscallarg(int) mode;
	} */ *uap = v;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = SCARG(uap, mode) & ALLPERMS;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Set ownership given a path name.
 */
/* ARGSUSED */
sys_chown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_chown_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_uid = SCARG(uap, uid);
		vattr.va_gid = SCARG(uap, gid);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Set ownership given a file descriptor.
 */
/* ARGSUSED */
sys_fchown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_fchown_args /* {
		syscallarg(int) fd;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_uid = SCARG(uap, uid);
		vattr.va_gid = SCARG(uap, gid);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Set the access and modification times of a file.
 */
/* ARGSUSED */
sys_utimes(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_utimes_args /* {
		syscallarg(char *) path;
		syscallarg(struct timeval *) tptr;
	} */ *uap = v;
	register struct vnode *vp;
	struct timeval tv[2];
	struct vattr vattr;
	int error;
	struct nameidata nd;

	VATTR_NULL(&vattr);
	if (SCARG(uap, tptr) == NULL) {
		microtime(&tv[0]);
		tv[1] = tv[0];
		vattr.va_vaflags |= VA_UTIMES_NULL;
	} else if (error = copyin((caddr_t)SCARG(uap, tptr), (caddr_t)tv,
	    sizeof (tv)))
  		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		vattr.va_atime.ts_sec = tv[0].tv_sec;
		vattr.va_atime.ts_nsec = tv[0].tv_usec * 1000;
		vattr.va_mtime.ts_sec = tv[1].tv_sec;
		vattr.va_mtime.ts_nsec = tv[1].tv_usec * 1000;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Truncate a file given its path name.
 */
/* ARGSUSED */
sys_truncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0 &&
	    (error = VOP_ACCESS(vp, VWRITE, p->p_ucred, p)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = SCARG(uap, length);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Truncate a file given a file descriptor.
 */
/* ARGSUSED */
sys_ftruncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_ftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ *uap = v;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	if ((fp->f_flag & FWRITE) == 0)
		return (EINVAL);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = SCARG(uap, length);
		error = VOP_SETATTR(vp, &vattr, fp->f_cred, p);
	}
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Sync an open file.
 */
/* ARGSUSED */
sys_fsync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fsync_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	register struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	error = VOP_FSYNC(vp, fp->f_cred, MNT_WAIT, p);
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Rename files.  Source and destination must either both be directories,
 * or both not be directories.  If target is a directory, it must be empty.
 */
/* ARGSUSED */
sys_rename(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_rename_args /* {
		syscallarg(char *) from;
		syscallarg(char *) to;
	} */ *uap = v;
	register struct vnode *tvp, *fvp, *tdvp;
	struct nameidata fromnd, tond;
	int error;

	NDINIT(&fromnd, DELETE, WANTPARENT | SAVESTART, UIO_USERSPACE,
	    SCARG(uap, from), p);
	if (error = namei(&fromnd))
		return (error);
	fvp = fromnd.ni_vp;
	NDINIT(&tond, RENAME, LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART,
	    UIO_USERSPACE, SCARG(uap, to), p);
	if (error = namei(&tond)) {
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}
	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = ENOTDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		}
	}
	if (fvp == tdvp)
		error = EINVAL;
	/*
	 * If source is the same as the destination (that is the
	 * same inode number with the same name in the same directory),
	 * then there is nothing to do.
	 */
	if (fvp == tvp && fromnd.ni_dvp == tdvp &&
	    fromnd.ni_cnd.cn_namelen == tond.ni_cnd.cn_namelen &&
	    !bcmp(fromnd.ni_cnd.cn_nameptr, tond.ni_cnd.cn_nameptr,
	      fromnd.ni_cnd.cn_namelen))
		error = -1;
out:
	if (!error) {
		VOP_LEASE(tdvp, p, p->p_ucred, LEASE_WRITE);
		if (fromnd.ni_dvp != tdvp)
			VOP_LEASE(fromnd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		if (tvp)
			VOP_LEASE(tvp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_RENAME(fromnd.ni_dvp, fromnd.ni_vp, &fromnd.ni_cnd,
				   tond.ni_dvp, tond.ni_vp, &tond.ni_cnd);
	} else {
		VOP_ABORTOP(tond.ni_dvp, &tond.ni_cnd);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	vrele(tond.ni_startdir);
	FREE(tond.ni_cnd.cn_pnbuf, M_NAMEI);
out1:
	if (fromnd.ni_startdir)
		vrele(fromnd.ni_startdir);
	FREE(fromnd.ni_cnd.cn_pnbuf, M_NAMEI);
	if (error == -1)
		return (0);
	return (error);
}

/*
 * Make a directory file.
 */
/* ARGSUSED */
sys_mkdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mkdir_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		return (EEXIST);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VDIR;
	vattr.va_mode = (SCARG(uap, mode) & ACCESSPERMS) &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (!error)
		vput(nd.ni_vp);
	return (error);
}

/*
 * Remove a directory file.
 */
/* ARGSUSED */
sys_rmdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_rmdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (nd.ni_dvp == vp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT)
		error = EBUSY;
out:
	if (!error) {
		VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	return (error);
}

/*
 * Read a block of directory entries in a file system independent format.
 */
sys_getdirentries(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getdirentries_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(u_int) count;
		syscallarg(long *) basep;
	} */ *uap = v;
	register struct vnode *vp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	long loff;
	int error, eofflag;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;
unionread:
	if (vp->v_type != VDIR)
		return (EINVAL);
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, count);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = SCARG(uap, count);
	VOP_LOCK(vp);
	loff = auio.uio_offset = fp->f_offset;
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, (u_long *)0, 0);
	fp->f_offset = auio.uio_offset;
	VOP_UNLOCK(vp);
	if (error)
		return (error);

#ifdef UNION
{
	extern int (**union_vnodeop_p)();
	extern struct vnode *union_dircache __P((struct vnode *));

	if ((SCARG(uap, count) == auio.uio_resid) &&
	    (vp->v_op == union_vnodeop_p)) {
		struct vnode *lvp;

		lvp = union_dircache(vp);
		if (lvp != NULLVP) {
			struct vattr va;

			/*
			 * If the directory is opaque,
			 * then don't show lower entries
			 */
			error = VOP_GETATTR(vp, &va, fp->f_cred, p);
			if (va.va_flags & OPAQUE) {
				vput(lvp);
				lvp = NULL;
			}
		}
		
		if (lvp != NULLVP) {
			error = VOP_OPEN(lvp, FREAD, fp->f_cred, p);
			VOP_UNLOCK(lvp);

			if (error) {
				vrele(lvp);
				return (error);
			}
			fp->f_data = (caddr_t) lvp;
			fp->f_offset = 0;
			error = vn_close(vp, FREAD, fp->f_cred, p);
			if (error)
				return (error);
			vp = lvp;
			goto unionread;
		}
	}
}
#endif /* UNION */

	if ((SCARG(uap, count) == auio.uio_resid) &&
	    (vp->v_flag & VROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_data = (caddr_t) vp;
		fp->f_offset = 0;
		vrele(tvp);
		goto unionread;
	}
	error = copyout((caddr_t)&loff, (caddr_t)SCARG(uap, basep),
	    sizeof(long));
	*retval = SCARG(uap, count) - auio.uio_resid;
	return (error);
}

/*
 * Set the mode mask for creation of filesystem nodes.
 */
int
sys_umask(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_umask_args /* {
		syscallarg(int) newmask;
	} */ *uap = v;
	register struct filedesc *fdp;

	fdp = p->p_fd;
	*retval = fdp->fd_cmask;
	fdp->fd_cmask = SCARG(uap, newmask) & ALLPERMS;
	return (0);
}

/*
 * Void all references to file by ripping underlying filesystem
 * away from vnode.
 */
/* ARGSUSED */
sys_revoke(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_revoke_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VCHR && vp->v_type != VBLK) {
		error = EINVAL;
		goto out;
	}
	if (error = VOP_GETATTR(vp, &vattr, p->p_ucred, p))
		goto out;
	if (p->p_ucred->cr_uid != vattr.va_uid &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		goto out;
	if (vp->v_usecount > 1 || (vp->v_flag & VALIASED))
		vgoneall(vp);
out:
	vrele(vp);
	return (error);
}

/*
 * Convert a user file descriptor to a kernel file entry.
 */
getvnode(fdp, fd, fpp)
	struct filedesc *fdp;
	struct file **fpp;
	int fd;
{
	struct file *fp;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EINVAL);
	*fpp = fp;
	return (0);
}
