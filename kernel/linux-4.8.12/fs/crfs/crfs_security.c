#include <linux/fs.h>
#include <linux/devfs.h>
#include <linux/file.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vfio.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>
#include <linux/nvme.h>
#include <linux/syscalls.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/fsnotify.h>
#include <linux/mount.h>
#include <linux/kernel.h>

/* Credential Table in device-level file systems */
#define CRED_TABLE_BITS 5
DEFINE_HASHTABLE(crfs_cred_table, CRED_TABLE_BITS);

static const struct cred *get_const_curr_cred(void){

	const struct cred *cred = current_cred();

	if (unlikely(!cred)) {
    		printk(KERN_ALERT "%s:%d fs NULL \n",__FUNCTION__,__LINE__);
	    	return ERR_PTR(-ENOMEM);
	}
	return get_cred(cred);
}

/*Set the credential for current file structure*/
int crfss_set_cred(struct crfss_fstruct *fs) {

	if(!fs) {
		printk(KERN_ALERT "%s:%d fs NULL \n",__FUNCTION__,__LINE__);
		return -EINVAL;
	}
	fs->f_cred = get_const_curr_cred();

#if defined(_DEVFS_DEBUG)
	printk(KERN_ALERT "%s:%d Setting credentials for fs \n",
		__FUNCTION__,__LINE__);
#endif
	return 0;
}
EXPORT_SYMBOL(crfss_set_cred);

/*Get the credential from current file structure*/
const struct cred *crfss_get_cred(struct crfss_fstruct *fs) {

	if(!fs) {
		printk(KERN_ALERT "%s:%d fs NULL \n",__FUNCTION__,__LINE__);
		return ERR_PTR(-ENOMEM);
	}
	return fs->f_cred;
}
EXPORT_SYMBOL(crfss_get_cred);


/* Check the credential if the file structure and current process 
*  credential match. If not return error.  
*  DevFs.c will take care of handling it.
*/
int crfss_check_fs_cred(struct crfss_fstruct *fs) {
	
	int retval = 0;

	if (unlikely(!fs)) {
   		printk(KERN_ALERT "%s:%d fs NULL \n",__FUNCTION__,__LINE__);
       		retval = -EINVAL;
   	}

	if (unlikely(!fs-> f_cred)) {
   		printk(KERN_ALERT "%s:%d fs NULL \n",__FUNCTION__,__LINE__);
		retval = -EINVAL;
	}

	if (fs-> f_cred != get_const_curr_cred()) {
		retval = -EINVAL;
		printk(KERN_ALERT "%s:%d perm mismatch\n",__FUNCTION__,__LINE__);	
	}
	return retval;
}
EXPORT_SYMBOL(crfss_check_fs_cred);

/*
 * Add a cred id in credential table
 */
int crfss_add_cred_table(u8 *cred_id) {
	int retval = 0;

	/* Create and allocate a new credential table struct */
	cred_node_t *cred_node = kmalloc(sizeof(cred_node_t), GFP_KERNEL);
	if (!cred_node) {
		printk(KERN_ALERT "failed to allocate cred table entry");
		retval = -EFAULT;
		goto err_add_cred_table;
	}

	/* Add cred_id to the credential table */
	memcpy(cred_node->cred_id, cred_id, CRED_ID_BYTES);
	hash_add(crfs_cred_table, &cred_node->hash_node, *(u32*)cred_id);

err_add_cred_table:
	return retval;
}
EXPORT_SYMBOL(crfss_add_cred_table);

/*
 * Delete a cred id in credential table
 */
int crfss_del_cred_table(u8 *cred_id) {
	cred_node_t *cred_node = NULL;

	/* Lookup the node of cred_id in the credential table */
	hash_for_each_possible(crfs_cred_table, cred_node, hash_node, *(u32*)cred_id) {
		if (cred_node && !memcmp(cred_id, cred_node->cred_id, CRED_ID_BYTES))
			break;
	}

	/* Remove cred_id from the credential table */
	if (cred_node)
		kfree(cred_node);

	return 0;
}
EXPORT_SYMBOL(crfss_del_cred_table);

/*
 * Check 128-bit cred id in credential table
 */
int crfss_check_cred_table(u8 *cred_id) {
	cred_node_t *cred_node = NULL;

	/* Lookup the node of cred_id in the credential table */
	hash_for_each_possible(crfs_cred_table, cred_node, hash_node, *(u32*)cred_id) {
		if (cred_node && !memcmp(cred_id, cred_node->cred_id, CRED_ID_BYTES))
			return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(crfss_check_cred_table);
