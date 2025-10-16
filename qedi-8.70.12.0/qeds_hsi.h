#ifndef __QEDS_HSI__
#define __QEDS_HSI__ 

struct scsi_glbl_queue_entry
{
	struct regpair rq_pbl_addr /* Start physical address for the RQ (receive queue) PBL. */;
	struct regpair cq_pbl_addr /* Start physical address for the CQ (completion queue) PBL. */;
	struct regpair cmdq_pbl_addr /* Start physical address for the CMDQ (command queue) PBL. */;
};

#endif /* __QEDS_HSI__ */
