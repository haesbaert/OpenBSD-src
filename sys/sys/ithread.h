#ifndef _SYS_ITHREAD_H_
#define _SYS_ITHREAD_H_

struct intrsource;

void	ithread(void *);
int	ithread_handler(struct intrsource *);
void	ithread_register(struct intrsource *);
void	ithread_forkall(void);

#endif /* _SYS_ITHREAD_H_ */
