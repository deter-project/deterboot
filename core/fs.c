#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "fs.h"
#include "cache.h"

/* The currently mounted filesystem */
struct fs_info *this_fs = NULL;		/* Root filesystem */
static struct inode *this_inode = NULL;	/* Current working directory */

/* Actual file structures (we don't have malloc yet...) */
struct file files[MAX_OPEN];

/*
 * Get a new inode structure
 */
struct inode *alloc_inode(struct fs_info *fs, uint32_t ino, size_t data)
{
    struct inode *inode = zalloc(sizeof(struct inode) + data);
    if (inode) {
	inode->fs = fs;
	inode->ino = ino;
    }
    return inode;
}

/*
 * Get an empty file structure
 */
static struct file *alloc_file(void)
{
    int i;
    struct file *file = files;

    for (i = 0; i < MAX_OPEN; i++) {
	if (!file->fs)
	    return file;
	file++;
    }

    return NULL;
}

/*
 * Close and free a file structure
 */
static inline void free_file(struct file *file)
{
    memset(file, 0, sizeof *file);
}

void _close_file(struct file *file)
{
    if (file->fs)
	file->fs->fs_ops->close_file(file);
    free_file(file);
}

/*
 * Convert between a 16-bit file handle and a file structure
 */

void load_config(void)
{
    int err;

    err = this_fs->fs_ops->load_config();

    if (err)
	printf("ERROR: No configuration file found\n");
}

void pm_mangle_name(com32sys_t *regs)
{
    const char *src = MK_PTR(regs->ds, regs->esi.w[0]);
    char       *dst = MK_PTR(regs->es, regs->edi.w[0]);

    mangle_name(dst, src);
}

void mangle_name(char *dst, const char *src)
{
    this_fs->fs_ops->mangle_name(dst, src);
}

/*
 * XXX: current unmangle_name() is stpcpy() on all filesystems; consider
 * eliminating it as a method.
 */
void pm_unmangle_name(com32sys_t *regs)
{
    const char *src = MK_PTR(regs->ds, regs->esi.w[0]);
    char       *dst = MK_PTR(regs->es, regs->edi.w[0]);
    
    dst = unmangle_name(dst, src);

    /* Update the di register to point to the last null char */
    regs->edi.w[0] = OFFS_WRT(dst, regs->es);
}

char *unmangle_name(char *dst, const char *src)
{
    return this_fs->fs_ops->unmangle_name(dst, src);
}

void getfssec(com32sys_t *regs)
{
    int sectors;
    bool have_more;
    uint32_t bytes_read;
    char *buf;
    struct file *file;
    uint16_t handle;
    
    sectors = regs->ecx.w[0];

    handle = regs->esi.w[0];
    file = handle_to_file(handle);
    
    buf = MK_PTR(regs->es, regs->ebx.w[0]);
    bytes_read = file->fs->fs_ops->getfssec(file, buf, sectors, &have_more);
    
    /*
     * If we reach EOF, the filesystem driver will have already closed
     * the underlying file... this really should be cleaner.
     */
    if (!have_more) {
	_close_file(file);
        regs->esi.w[0] = 0;
    }
    
    regs->ecx.l = bytes_read;
}

void pm_searchdir(com32sys_t *regs)
{
    char *name = MK_PTR(regs->ds, regs->edi.w[0]);
    int rv;

    rv = searchdir(name);
    if (rv < 0) {
	regs->esi.w[0]  = 0;
	regs->eax.l     = 0;
	regs->eflags.l |= EFLAGS_ZF;
    } else {
	regs->esi.w[0]  = rv;
	regs->eax.l     = handle_to_file(rv)->file_len;
	regs->eflags.l &= ~EFLAGS_ZF;
    }
}

int searchdir(const char *name)
{
    struct inode *inode;
    struct inode *parent;
    struct file *file;
    char part[256];
    char *p;
    int symlink_count = 6;
    bool got_parent;
    
#if 0
    printf("filename: %s\n", name);
#endif

    if (!(file = alloc_file()))
	goto err_no_close;
    file->fs = this_fs;

    /* if we have ->searchdir method, call it */
    if (file->fs->fs_ops->searchdir) {
	file->fs->fs_ops->searchdir(name, file);
	
	if (file->open_file)
	    return file_to_handle(file);
	else
	    goto err;
    }

    /* else, try the generic-path-lookup method */
    if (*name == '/') {
	inode = this_fs->fs_ops->iget_root(this_fs);
	while (*name == '/')
	    name++;
    } else {
	inode = this_fs->cwd;
    }
    parent = inode;
    got_parent = false;
    
    while (*name) {
	p = part;
	while (*name && *name != '/')
	    *p++ = *name++;
	*p = '\0';
	if (strcmp(part, ".")) {
	    inode = this_fs->fs_ops->iget(part, parent);
	    if (!inode)
		goto err;
	    if (inode->mode == I_SYMLINK) {
		if (!this_fs->fs_ops->follow_symlink || 
		    --symlink_count == 0             ||      /* limit check */
		    inode->size >= BLOCK_SIZE(this_fs))
		    goto err;
		name = this_fs->fs_ops->follow_symlink(inode, name);
		free_inode(inode);
		continue;
	    }
	    if (got_parent)
		free_inode(parent);
	    parent = inode;
	    got_parent = true;
	}
	if (!*name)
	    break;
	while (*name == '/')
	    name++;
    }
    
    if (got_parent)
	free_inode(parent);

    file->inode  = inode;
    file->offset = 0;
    file->file_len  = inode->size;
    
    return file_to_handle(file);
    
err:
    _close_file(file);
err_no_close:
    return -1;
}

void close_file(com32sys_t *regs)
{
    uint16_t handle = regs->esi.w[0];
    struct file *file;
    
    if (handle) {
	file = handle_to_file(handle);
	_close_file(file);
    }
}

/*
 * it will do:
 *    initialize the memory management function;
 *    set up the vfs fs structure;
 *    initialize the device structure;
 *    invoke the fs-specific init function;
 *    initialize the cache if we need one;
 *    finally, get the current inode for relative path looking.
 *
 */
void fs_init(com32sys_t *regs)
{
    static struct fs_info fs;	/* The actual filesystem buffer */
    uint8_t disk_devno = regs->edx.b[0];
    uint8_t disk_cdrom = regs->edx.b[1];
    sector_t disk_offset = regs->ecx.l | ((sector_t)regs->ebx.l << 32);
    uint16_t disk_heads = regs->esi.w[0];
    uint16_t disk_sectors = regs->edi.w[0];
    int blk_shift = -1;
    struct device *dev = NULL;
    /* ops is a ptr list for several fs_ops */
    const struct fs_ops **ops = (const struct fs_ops **)regs->eax.l;
    
    /* Initialize malloc() */
    mem_init();
    
    /* Default name for the root directory */
    fs.cwd_name[0] = '/';

    while ((blk_shift < 0) && *ops) {
	/* set up the fs stucture */
	fs.fs_ops = *ops;

	/*
	 * This boldly assumes that we don't mix FS_NODEV filesystems
	 * with FS_DEV filesystems...
	 */
	if (fs.fs_ops->fs_flags & FS_NODEV) {
	    fs.fs_dev = NULL;
	} else {
	    if (!dev)
		dev = device_init(disk_devno, disk_cdrom, disk_offset,
				  disk_heads, disk_sectors);
	    fs.fs_dev = dev;
	}
	/* invoke the fs-specific init code */
	blk_shift = fs.fs_ops->fs_init(&fs);
	ops++;
    }
    if (blk_shift < 0) {
	printf("No valid file system found!\n");
	while (1)
		;
    }
    this_fs = &fs;

    /* initialize the cache */
    if (fs.fs_dev && fs.fs_dev->cache_data)
        cache_init(fs.fs_dev, blk_shift);

    /* start out in the root directory */
    if (fs.fs_ops->iget_root)
	fs.cwd = fs.fs_ops->iget_root(&fs);
}