#ifndef _SYS_ITHREAD_H_
#define _SYS_ITHREAD_H_

struct intrsource;

void	ithread(void *);
int	ithread_handler(int);
void	ithread_create(struct intrsource *);
void	ithread_create2(void *);

#endif /* _SYS_ITHREAD_H_ */
