<$if indicator.file.get$>static char*
indicator_file_<$indicator.file.name$>_get_str_real(<$indicatorStateDeclaration$>)
{
<$indicatorVarsUse$><$indicator.file.get$>
<$indicatorVarsUnuse$>}

static char*
indicator_file_<$indicator.file.name$>_get_str(struct inode* inode)
{
	<$indicatorStateDeclaration$>;
	char *str;
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        return NULL;
    }
    <$indicatorStateName$> = inode->i_private;
    if(<$indicatorStateName$>)
    {
        str = indicator_file_<$indicator.file.name$>_get_str_real(<$indicatorStateName$>);
    }
    else
    {
        str = NULL;//'device', corresponed to file, is not exist
    }
    mutex_unlock(&indicator_mutex);
    
    return str;
}<$endif$>

<$if indicator.file.set$>static int
indicator_file_<$indicator.file.name$>_set_str_real(const char* str, <$indicatorStateDeclaration$>)
{
<$indicatorVarsUse$><$indicator.file.set$>
<$indicatorVarsUnuse$>}

static int
indicator_file_<$indicator.file.name$>_set_str(const char* str, struct inode* inode)
{
	int error;
    <$indicatorStateDeclaration$>;
   
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        return -EINTR;
    }

    <$indicatorStateName$> = inode->i_private;
    if(<$indicatorStateName$>)
    {
        error = indicator_file_<$indicator.file.name$>_set_str_real(str, <$indicatorStateName$>);
    }
    else
    {
        error = -EINVAL;
    }
    mutex_unlock(&indicator_mutex);
    
    return error;
}<$endif$>

CONTROL_FILE_OPS(indicator_file_<$indicator.file.name$>_operations,
    <$if indicator.file.get$>indicator_file_<$indicator.file.name$>_get_str<$else$>NULL<$endif$>,
    <$if indicator.file.set$>indicator_file_<$indicator.file.name$>_set_str<$else$>NULL<$endif$>);

static int
indicator_file_<$indicator.file.name$>_create(<$indicatorStateDeclaration$>, struct dentry* dir)
{
    <$indicatorStateName$>->file_<$indicator.file.name$> = debugfs_create_file("<$indicator.file.fs_name$>",
        S_IRUGO<$if indicator.file.set$> | S_IWUSR | S_IWGRP<$endif$>,
        dir,
        <$indicatorStateName$>, &indicator_file_<$indicator.file.name$>_operations);
    if(<$indicatorStateName$>->file_<$indicator.file.name$> == NULL)
    {
        pr_err("Cannot create file '<$indicator.file.fs_name$>'.");
        return -1;
    }
    return 0;
}

static void
indicator_file_<$indicator.file.name$>_destroy(<$indicatorStateDeclaration$>)
{
    if(<$indicatorStateName$>->file_<$indicator.file.name$>)
    {
        mutex_lock(&indicator_mutex);
        <$indicatorStateName$>->file_<$indicator.file.name$>->d_inode->i_private = NULL;
        mutex_unlock(&indicator_mutex);
        debugfs_remove(<$indicatorStateName$>->file_<$indicator.file.name$>);
    }
}