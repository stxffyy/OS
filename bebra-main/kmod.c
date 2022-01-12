#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/pid.h>
#include <asm/pgtable.h>
#include <linux/fd.h>
#include <linux/path.h>
#include <linux/mount.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#define BUFFER_SIZE 1024
static DEFINE_MUTEX(kmod_mutex);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("1.0");

static struct dentry *kmod_root;
static struct dentry *kmod_args_file;
static struct dentry *kmod_result_file;
static struct task_struct* task;
static struct vfsmount* struct_vfsmount;

static int kmod_result_open(struct seq_file *sf, void *data);
static int kmod_release(struct inode *inode, struct file *file);
struct task_struct * kmod_get_task_struct(void);
struct vfsmount * kmod_get_vfsmount(void);
void set_result(void);

int kmod_open(struct inode *inode, struct file *file) {
    if (!mutex_trylock(&kmod_mutex)) {
        printk(KERN_INFO "can't lock file");
        return -EBUSY;
    }
    printk(KERN_INFO "file is locked by module");
    return single_open(file, kmod_result_open, inode->i_private);
}

static int kmod_release(struct inode *inode, struct file *file) {
    mutex_unlock(&kmod_mutex);
    printk(KERN_INFO "file is unlocked by module");
    return 0;
}

static ssize_t kmod_args_write( struct file* ptr_file, const char __user* buffer, size_t length, loff_t* ptr_offset );

static struct file_operations kmod_args_ops = {
        .owner   = THIS_MODULE,
        .read    = seq_read,
        .write   = kmod_args_write,
        .release = kmod_release,
        .open = kmod_open,
};

static int fdesc = 0;
static int pid = 1;

struct task_struct * kmod_get_task_struct() {
    return get_pid_task(find_get_pid(pid), PIDTYPE_PID);
}

struct vfsmount * kmod_get_vfsmount() {
    struct fd f = fdget(fdesc);
    if(!f.file) {
        printk(KERN_INFO "kmod: error opening file by descriptor\n");
        return NULL;
    }
    struct vfsmount *vfs = f.file->f_path.mnt;
    return vfs;
}

void set_result() {
    task = kmod_get_task_struct();
    struct_vfsmount = kmod_get_vfsmount();
}

static int kmod_result_open(struct seq_file *sf, void *data) {
    set_result();
    if (task == NULL) {
        seq_printf(sf, "failed to retrieve TASK_STRUCT structure\n");
        printk(KERN_INFO "kmod: failed to retrieve TASK_STRUCT structure\n");
    } else {
        seq_printf(sf, "-----TASK_STRUCT-----\npid: %d\n", task->pid);
        seq_printf(sf, "name: %s\n", task->comm);
        printk(KERN_INFO "kmod: printed TASK_STRUCT structure\n");
    }
    if (struct_vfsmount == NULL) {
        seq_printf(sf, "failed to retrieve VFSMOUNT structure\n");
        printk(KERN_INFO "kmod: failed to retrieve VFSMOUNT structure\n");
    } else {
        seq_printf(sf, "-----VFSMOUNT-----\n");
        seq_printf(sf, "mnt_flags: %uc\n", struct_vfsmount->mnt_flags);
        seq_printf(sf, "superblock:{\n   blocksize in bits: %u\n", struct_vfsmount->mnt_sb->s_blocksize_bits);
        seq_printf(sf, "   blocksize in bytes: %lu\n", struct_vfsmount->mnt_sb->s_blocksize);
        seq_printf(sf, "   ref count: %d\n", struct_vfsmount->mnt_sb->s_count);
        seq_printf(sf, "   maximum files size: %ld\n", struct_vfsmount->mnt_sb->s_maxbytes);
        seq_printf(sf, "}\n");
        printk(KERN_INFO "kmod: printed VFSMOUNT structure\n");
    }
    return 0;
}

static ssize_t kmod_args_write( struct file* ptr_file, const char __user* buffer, size_t length, loff_t* ptr_offset) {
    printk(KERN_INFO "kmod: get params\n");
    char kbuf[BUFFER_SIZE];

    if (*ptr_offset > 0 || length > BUFFER_SIZE) {
        return -EFAULT;
    }

    if ( copy_from_user(kbuf, buffer, length) ) {
        return -EFAULT;
    }
    int a = sscanf(kbuf, "%d %d", &fdesc, &pid);
    printk(KERN_INFO "kmod: fdesc %d, pid %d.", fdesc, pid);
    ssize_t count = strlen(kbuf);
    *ptr_offset = count;
    return count;
}

static int __init kmod_init(void) {
    printk(KERN_INFO "kmod: module loaded\n");
    kmod_root = debugfs_create_dir("kmod", NULL);
    kmod_args_file = debugfs_create_file( "kmod_args", 0666, kmod_root, NULL, &kmod_args_ops );
    kmod_result_file = debugfs_create_file( "kmod_result", 0666, kmod_root, NULL, &kmod_args_ops );
    return 0;
}

static void __exit kmod_exit(void) {
    debugfs_remove_recursive(kmod_root);
    printk(KERN_INFO "kmod: module unloaded\n");
    mutex_destroy(&kmod_mutex);
}
module_init(kmod_init);
module_exit(kmod_exit);
