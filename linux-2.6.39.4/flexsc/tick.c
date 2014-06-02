#include "flexsc.h"

#ifdef FLEXSC_DEBUG

void
flexsc_tick(struct flexsc_syspage *syspage) {
    unsigned long mon_wait_size[3], mon_done_size[3];
    struct mbox_struct *sendbox;
    int i, send, recv;
    
    syspage->mon_ticks ++;

    if (syspage->__type != TYPE_DEFAULT) {
        for (i = 0; i < syspage->mbox_used && i < 3; i ++) {
            sendbox = syspage->mbox_array[i].__sendbox;
            send = sendbox->send, recv = sendbox->recv;
            if (send < recv) {
                send += MAX_NFIBERS;
            }
            send = send - recv;
            syspage->mon_mbox_size[i * 2 + 0] += send;
            syspage->mon_mbox_size[i * 2 + 1] += send * send;
        }
    }

    if (syspage->mon_ticks % HZ != 0) {
        return ;
    }

    memset(mon_wait_size, 0, sizeof(mon_wait_size));
    memset(mon_done_size, 0, sizeof(mon_done_size));

    do {
        if (syspage->mon_wait_size[2] != 0) {
            mon_wait_size[2] = syspage->mon_wait_size[2];
            mon_wait_size[1] = syspage->mon_wait_size[1] / mon_wait_size[2];
            mon_wait_size[0] = syspage->mon_wait_size[0] / mon_wait_size[2];
        }
    } while (0);

    do {
        if (syspage->mon_done_size[2] != 0) {
            mon_done_size[2] = syspage->mon_done_size[2];
            mon_done_size[1] = syspage->mon_done_size[1] / mon_done_size[2];
            mon_done_size[0] = syspage->mon_done_size[0] / mon_done_size[2];
        }
    } while (0);

    printk("monitor: %2d %3d %8ld %8ld [%3ld %8ld %6ld] [%3ld %8ld %6ld : %10ld] [%3ld %6ld / %3ld %6ld / %3ld %6ld]\n", smp_processor_id(),
           syspage->worker, syspage->mon_wait_step0, syspage->mon_wait_step1,
           mon_wait_size[0], mon_wait_size[1], mon_wait_size[2],
           mon_done_size[0], mon_done_size[1], mon_done_size[2], syspage->mon_done_size[0],
           syspage->mon_mbox_size[0] / HZ, syspage->mon_mbox_size[1] / HZ,
           syspage->mon_mbox_size[2] / HZ, syspage->mon_mbox_size[3] / HZ,
           syspage->mon_mbox_size[4] / HZ, syspage->mon_mbox_size[5] / HZ);
    syspage->mon_reset = 1;
    syspage->mon_wait_reset = 1;
    memset(syspage->mon_mbox_size, 0, sizeof(syspage->mon_mbox_size));
}

#else

void
flexsc_tick(struct flexsc_syspage *syspage) {
}

#endif
