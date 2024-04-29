#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "cache.h"
#include <stdbool.h>
#include "directory.h"
#include "filesys.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* User implemented helper functions */


/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  size_t total_sectors = block_size (fs_device);
  set_total_sectors (total_sectors);

  /* initialize inode cache */
  cache_init ();
  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void filesys_done (void)
{
  write_back (true);
  free_map_close ();  
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  // block_sector_t inode_sector = 0;
  // struct dir *dir = dir_open_root ();
  // bool success = (dir != NULL && free_map_allocate (1, &inode_sector) &&
  //                 inode_create (inode_sector, initial_size, is_dir, false) &&
  //                 dir_add (dir, name, inode_sector));

  // if (!success && inode_sector != 0)
  //   free_map_release (inode_sector, 1);

  // dir_close (dir);

  // return success;
  block_sector_t inode_sector = 0;
  struct dir *dir = resolve_dir_from_path (name);
  char *filename = resolve_name_from_path (name);
  bool success = false;

  if ((0 != strcmp (filename, ".")) &&
      (0 != strcmp (filename, "..")))
  {
    success = ((dir != NULL) &&
              (free_map_allocate (1, &inode_sector)) &&
              (inode_create (inode_sector, initial_size, is_dir, false)) &&
              (dir_add (dir, filename, inode_sector)));
  }

  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);

  dir_close (dir);
  free (filename);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *filesys_open (const char *name)
{
  if (0 == strlen (name))
    return NULL;

  struct dir *dir = resolve_dir_from_path (name);
  char *filename  = resolve_name_from_path (name);
  struct inode *inode = NULL;

  if (dir)
  {
    if (0 == strcmp (filename, ".."))
    {
      inode = dir_get_parent_inode (dir);
      if (NULL == inode)
      {
        free (filename);
        return NULL;
      }
    }
    else if (((dir_is_root (dir)) && (0 == strlen (filename))) ||
            (0 == strcmp (filename, ".")))
    {
      free (filename);
      return (struct file *) dir;
    }
    else
      dir_lookup (dir, filename, &inode);
  }

  free (filename);
  dir_close (dir);

  if (NULL == inode)
    return NULL;

  if (inode_get_symlink (inode))
  {
    char target[15];
    inode_read_at (inode, target, NAME_MAX + 1, 0);
    struct dir *root = dir_open_root ();
    if (!dir_lookup (root, target, &inode))
      return NULL;
    dir_close (root);
  }

  if (inode_is_dir (inode))
    return (struct file *) dir_open (inode);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool filesys_remove (const char *name)
{
  // struct dir *dir = dir_open_root ();
  // bool success = dir != NULL && dir_remove (dir, name);
  // dir_close (dir);

  // return success;
  struct dir *dir = resolve_dir_from_path(name);
  char *filename = resolve_name_from_path(name);
  bool success = NULL != dir && dir_remove (dir, filename);
  dir_close (dir);
  free (filename);

  return success;
}

/* Creates symbolic link LINKPATH to target file TARGET
   Returns true if symbolic link created successfully,
   false otherwise. */
bool filesys_symlink (char *target, char *linkpath)
{
  ASSERT (target != NULL && linkpath != NULL);
  bool success = filesys_create (linkpath, 15, false);
  struct file *symlink = filesys_open (linkpath);
  inode_set_symlink (file_get_inode (symlink), true);
  inode_write_at (file_get_inode (symlink), target, NAME_MAX + 1, 0);
  file_close (symlink);
  return success;
}

/* Formats the file system. */
static void do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* User implemented filesys_chdir */
bool filesys_chdir (const char *pathname)
{
  ASSERT (NULL != pathname);
  
  struct dir *dir = resolve_dir_from_path (pathname);
  char *filename = resolve_name_from_path (pathname);
  struct inode *inode = NULL;

  if (NULL == dir)
  {
    free (filename);
    return false;
  }
  /* go to parent dir */
  else if (0 == strcmp (filename, ".."))
  {
    inode = dir_get_parent_inode (dir);
    if (NULL == inode)
    {
      free (filename);
      return false;
    }
  }
  /* current working dir */
  else if ((0 == strcmp (filename, ".")) ||
          ((0 == strlen (filename)) && (dir_is_root (dir))))
  {
    thread_current ()->dir = dir;
    free (filename);
    return true;
  }
  else
    dir_lookup(dir, filename, &inode);

  dir_close (dir);

  /* Open up target directory */
  dir = dir_open (inode);
  if (NULL == dir)
  {
    free (filename);
    return false;
  }
  else
  {
    dir_close (thread_current ()->dir);
    thread_current ()->dir = dir;
    free (filename);
    return true;
  }
  
  return false;
}

int filesys_stat (const char *pathname, struct stat *st)
{
  if (NULL == pathname)
    return false;

  struct dir *dir = resolve_dir_from_path (pathname);
  struct inode *inode = NULL;

  if (dir)
  {
    if (dir_lookup (dir, pathname, &inode))
    {
      st->logical_size  = inode_length (inode);
      st->physical_size = inode_get_physical_size (inode);
      st->inode_number  = inode_get_inumber (inode);
      st->blocks        = inode_get_block_cnt (inode);
      st->is_dir        = inode_is_dir (inode);

      inode_close (inode);
      dir_close (dir);
      return true;
    }

    dir_close (dir);
  }

  return false;
}

/* User implemented helper functions */
char *resolve_name_from_path (const char *pathname)
{
  if (NULL == pathname)
    return NULL;

  // PANIC ("PATHNAME PASSED: %s\n", pathname);
  size_t len = strlen (pathname);
  char path[len + 1]; // NULL terminate
  memcpy (path, pathname, len + 1);

  char *cur   = NULL;
  char *ptr   = NULL;
  char *prev  = "";

  for (cur = strtok_r (path, "/", &ptr); NULL != cur; cur = strtok_r (NULL, "/", &ptr))
    prev = cur;

  if (NULL == prev)
    return NULL;

  char *name = malloc (strlen (prev) + 1);
  memcpy (name, prev, strlen (prev) + 1);
  return name;
}

struct dir *resolve_dir_from_path (const char *pathname)
{
  if (NULL == pathname)
    return NULL;

  size_t len = strlen (pathname);
  char path[len + 1]; // NULL terminate
  memcpy (path, pathname, len + 1);

  struct dir *dir = NULL;
  if (('/' == path[0]) || (NULL == thread_current ()->dir))
    dir = dir_open_root ();
  else
    dir = dir_reopen (thread_current ()->dir);

  char *cur   = NULL;
  char *ptr   = NULL;
  char *prev  = NULL;

  prev = strtok_r (path, "/", &ptr);
  for (cur = strtok_r (NULL, "/", &ptr);
        NULL != cur;
        prev = cur, cur = strtok_r (NULL, "/", &ptr))
  {
    struct inode *inode;
    if (0 == strcmp (prev, "."))
      continue;
    else if (0 == strcmp (prev, ".."))
    {
      inode = dir_get_parent_inode (dir);
      if (NULL == inode)
        return NULL;
    }
    else if (!dir_lookup (dir, prev, &inode))
      return NULL;

    if (inode_is_dir (inode))
    {
      dir_close (dir);
      dir = dir_open (inode);
    }
    else
      inode_close (inode);
  }

  return dir;
}