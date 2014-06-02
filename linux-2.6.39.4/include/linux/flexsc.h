#ifndef __INC_LINUX_FLEXSC_H__
#define __INC_LINUX_FLEXSC_H__

struct task_struct;

struct flexsc_kstruct;
struct flexsc_syspage;

struct flexsc_kstruct *copy_flexsc_kstruct(struct task_struct *task);
void exit_flexsc_kstruct(struct task_struct *task, int exit_code);
void flexsc_tick(struct flexsc_syspage *syspage);

#endif /* !__INC_LINUX_FLEXSC_H__ */
