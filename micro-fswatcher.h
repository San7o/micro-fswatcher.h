///////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
//
// micro-fswatcher.h
// =================
//
// Listen to events in multiple files on the filesystem, as a C99
// header only library for Unix and Windows.
//
// Author:  Giovanni Santini
// Mail:    giovanni.santini@proton.me
// Github:  @San7o
//
//
// Example
// -------
//
//    FsWatcherHandle fw = fswatcher_init();
//    fswatcher_add(fw, "./LICENSE", FSWATCHER_EVENT_MODIFY);
//
//    const char* file = fswatcher_watch(fw); // blocks
//    printf("Got notification on file: %s\n", file);
//
//
// Usage
// -----
//
// Do this:
//
//   #define MICRO_FSWATCHER_IMPLEMENTATION
//
// before you include this file in *one* C or C++ file to create the
// implementation.
//
// i.e. it should look like this:
//
//   #include ...
//   #include ...
//   #include ...
//   #define MICRO_FSWATCHER_IMPLEMENTATION
//   #include "micro-fswatcher.h"
//
// You can tune the library by #defining certain values. See the
// "Config" comments under "Configuration" below.
//
//
// Code
// ----
//
// The official git repository of micro-fswatcher.h is hosted at:
//
//     https://github.com/San7o/micro-fswatcher.h
//
// This is part of a bigger collection of header-only C99 libraries
// called "micro-headers", contributions are welcome:
//
//     https://github.com/San7o/micro-headers
//

#ifndef MICRO_FSWATCHER
#define MICRO_FSWATCHER

#ifdef __cplusplus
extern "C" {
#endif

//
// Configuration
//

// Config: Prefix for all functions
// For function inlining, set this to `static inline` and then define
// the implementation in all the files
#ifndef MICRO_FSWATCHER_DEF
  #define MICRO_FSWATCHER_DEF extern
#endif
  
// Config: Define memory allocation function
// Notes: This is expected to be used like malloc(3)
#ifndef MICRO_FSWATCHER_MALLOC
  #include <stdlib.h>
  #define MICRO_FSWATCHER_MALLOC malloc
#endif

// Config: Define memory free function
// Notes: This is expected to be used like free(3)
#ifndef MICRO_FSWATCHER_FREE
  #include <stdlib.h>
  #define MICRO_FSWATCHER_FREE free
#endif

//
// Types
//

// Events you can listen to (can be masked together)
typedef enum {
  FSWATCHER_EVENT_MODIFY,
} FsWatcherEvent;

  typedef void* FsWatcherHandle;

//
// Function declarations
//


MICRO_FSWATCHER_DEF FsWatcherHandle fswatcher_init(void); // returns NULL on failure
MICRO_FSWATCHER_DEF void fswatcher_destroy(FsWatcherHandle fw);

MICRO_FSWATCHER_DEF int fswatcher_add(FsWatcherHandle fw, const char* path,
                                    FsWatcherEvent event_mask);
MICRO_FSWATCHER_DEF int fswatcher_rm(FsWatcherHandle fw, const char* path);

MICRO_FSWATCHER_DEF const char* fswatcher_watch(FsWatcherHandle fw); // blocks

//
// Implementation
//
  
#ifdef MICRO_FSWATCHER_IMPLEMENTATION

// Posix implementation

#ifndef _WIN32

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>

#define _FSWATCHER_BUFFER_LEN  (1024 * (sizeof(struct inotify_event) + 16))

typedef struct FsWatcherUnixWdList FsWatcherUnixWdList;

struct FsWatcherUnixWdList {
  int wd;
  uint32_t mask;
  const char* pathname;
  FsWatcherUnixWdList *next;
};

typedef struct FsWatcherUnix {
  int fd;
  FsWatcherUnixWdList *wd_list;
} FsWatcherUnix;

FsWatcherHandle fswatcher_init(void)
{
  FsWatcherUnix* fw = MICRO_FSWATCHER_MALLOC(sizeof(FsWatcherUnix));
  fw->fd = inotify_init();
  if (fw->fd == -1)
    return NULL;

  return fw;
}

MICRO_FSWATCHER_DEF void fswatcher_destroy(FsWatcherHandle fw)
{
  if (!fw) return;
  FsWatcherUnix *fw_unix = (FsWatcherUnix*) fw;
  
  close(fw_unix->fd);

  FsWatcherUnixWdList *it = fw_unix->wd_list;
  FsWatcherUnixWdList *next = NULL;
  while (it)
  {
    next = it->next;
    MICRO_FSWATCHER_FREE(it);
    it = next;
  }
}

MICRO_FSWATCHER_DEF int fswatcher_add(FsWatcherHandle fw, const char* path,
                                      FsWatcherEvent event)
{
  if (!fw) return -1;
  FsWatcherUnix *fw_unix = (FsWatcherUnix*) fw;
  uint32_t mask = 0;
  
  switch(event)
  {
  case FSWATCHER_EVENT_MODIFY: mask = IN_MODIFY; break;
  default:
    break;
  }
  int wd = inotify_add_watch(fw_unix->fd, path, mask);
  if (wd == -1)
    return -2;

  FsWatcherUnixWdList* wd_item = MICRO_FSWATCHER_MALLOC(sizeof(FsWatcherUnixWdList));
  wd_item->wd       = wd;
  wd_item->pathname = path;
  wd_item->mask     = mask;
  wd_item->next     = NULL;

  if (!fw_unix->wd_list)
  {
    fw_unix->wd_list = wd_item;
    return 0;
  }
  
  FsWatcherUnixWdList *it = fw_unix->wd_list;
  while(it->next) { it = it->next; }
  it->next = wd_item;
  return 0;
}

MICRO_FSWATCHER_DEF int fswatcher_rm(FsWatcherHandle fw,
                                     const char* path)
{
  if (!fw) return 0;
  FsWatcherUnix *fw_unix = (FsWatcherUnix*) fw;

  FsWatcherUnixWdList *prev = NULL;
  FsWatcherUnixWdList *it = fw_unix->wd_list;
  while(it && strcmp(it->pathname, path) != 0)
  { prev = it; it = it->next; }

  if (!it)
    return 0;
  
  int ret = inotify_rm_watch(fw_unix->fd, it->wd);
  if (ret == -1)
    return -1;

  if (!prev)
    fw_unix->wd_list = it->next;
  else
    prev->next = it->next;
  MICRO_FSWATCHER_FREE(it);
  return 0;
}

MICRO_FSWATCHER_DEF const char* fswatcher_watch(FsWatcherHandle fw)
{
  if (!fw) return 0;
  FsWatcherUnix *fw_unix = (FsWatcherUnix*) fw;

  char buff[_FSWATCHER_BUFFER_LEN] = {0};
  ssize_t bytes = read(fw_unix->fd, &buff, sizeof(buff));
  if (bytes == -1 || bytes == 0)
    return NULL;

  for (char *ptr = buff; ptr < buff + bytes; )
  {
    struct inotify_event *ev = (struct inotify_event *) ptr;

    FsWatcherUnixWdList *it = fw_unix->wd_list;
    while (it && ev->wd != it->wd) { it = it->next; }
    if (it)
      return it->pathname;
    
    ptr += sizeof(struct inotify_event) + ev->len;
  }

  return NULL;
}

#else

// Windows implementation

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct FsWatcherWindowsDirList FsWatcherWindowsDirList;

struct FsWatcherWindowsDirList {
  HANDLE     hDir;
  WCHAR      pathW[MAX_PATH];
  char       path[MAX_PATH];
  BYTE       buffer[1024 * 64];
  DWORD      filter;
  OVERLAPPED overlapped;
  FsWatcherWindowsDirList *next;
};

typedef struct FsWatcherWindowsHandle {
  HANDLE iocp;
  FsWatcherWindowsDirList *dir_list;
} FsWatcherWindowsHandle;

MICRO_FSWATCHER_DEF FsWatcherHandle fswatcher_init(void)
{
  FsWatcherWindowsHandle *fw_windows = MICRO_FSWATCHER_MALLOC(sizeof(FsWatcherWindowsHandle));
  memset(fw_windows, 0, sizeof(FsWatcherWindowsHandle));

  fw_windows->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                            NULL,
                                            0,
                                            0);
  if (!fw_windows->iocp)
  {
    MICRO_FSWATCHER_FREE(fw_windows);
    return NULL;
  }
  
  return fw_windows;
}

MICRO_FSWATCHER_DEF void fswatcher_destroy(FsWatcherHandle fw)
{
  if (!fw) return;
  
  // Close all directories
  FsWatcherWindowsHandle *fw_windows = (FsWatcherWindowsHandle*) fw;
  FsWatcherWindowsDirList *it = fw_windows->dir_list;
  FsWatcherWindowsDirList *next = NULL;
  while (it)
  {
    next = it->next;
    CloseHandle(it->hDir);
    // Send exit signal
    PostQueuedCompletionStatus(fw_windows->iocp, 0, (ULONG_PTR)NULL, NULL);
    it = next;
  }

  CloseHandle(fw_windows->iocp);
}

MICRO_FSWATCHER_DEF int fswatcher_add(FsWatcherHandle fw, const char* path,
                                      FsWatcherEvent event)
{
  if (!fw) return -1;

  FsWatcherWindowsHandle *fw_windows = (FsWatcherWindowsHandle*) fw;

  DWORD filter = 0;
  switch(event)
  {
  case FSWATCHER_EVENT_MODIFY:
    filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    break;
  default:
    break;
  }

  // Convert to wide char string
  int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
  WCHAR pathW[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, pathW, len);
  
  wchar_t full_path[MAX_PATH];
  wchar_t *file_part = NULL;

  if (GetFullPathNameW(pathW,
                       MAX_PATH,
                       full_path,
                       &file_part) == 0)
    return -1;

  FsWatcherWindowsDirList *fw_dir =
    MICRO_FSWATCHER_MALLOC(sizeof(FsWatcherWindowsDirList));
  memset(fw_dir, 0, sizeof(FsWatcherWindowsDirList));
  fw_dir->filter = filter;
  strcpy(fw_dir->path, path);
  
  if (file_part != NULL)
  {
    wcscpy(fw_dir->pathW, file_part);
    *file_part = L'\0';
  }
  
  HANDLE hDir = CreateFileW(full_path,
                            FILE_LIST_DIRECTORY,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                            NULL);
  if (hDir == INVALID_HANDLE_VALUE)
  {
    MICRO_FSWATCHER_FREE(fw_dir);
    return -2;
  }

  fw_dir->hDir = hDir;

  // Associate the handle with the port
  if (CreateIoCompletionPort(fw_dir->hDir,
                             fw_windows->iocp,
                             (ULONG_PTR) fw_dir,
                             0) == NULL)
  {
    MICRO_FSWATCHER_FREE(fw_dir);
    return -3;
  }

  // Tell windows to listen for changes on this directory
  if (ReadDirectoryChangesW(fw_dir->hDir,
                            fw_dir->buffer,
                            sizeof(fw_dir->buffer),
                            FALSE,
                            filter,
                            NULL,
                            &fw_dir->overlapped, NULL) == 0)
  {
    MICRO_FSWATCHER_FREE(fw_dir);
    return -4;
  }

  // Add dir to list of watched files
  FsWatcherWindowsDirList *it = fw_windows->dir_list;
  if (it == NULL)
  {
    fw_windows->dir_list = fw_dir;
    return 0;
  }
  while(it->next) { it = it->next; }
  it->next = fw_dir;
  return 0;
}

MICRO_FSWATCHER_DEF int fswatcher_rm(FsWatcherHandle fw,
                                     const char* path)
{
  FsWatcherWindowsHandle *fw_win = (FsWatcherWindowsHandle*)fw;
  FsWatcherWindowsDirList **curr = &fw_win->dir_list;

  while (*curr)
  {
    if (strcmp((*curr)->path, path) == 0)
    {
      FsWatcherWindowsDirList *to_remove = *curr;
      
      *curr = to_remove->next;

      CancelIoEx(to_remove->hDir, &to_remove->overlapped);
      CloseHandle(to_remove->hDir);
            
      return 0;
    }
    curr = &((*curr)->next);
  }
  return -1;
}

MICRO_FSWATCHER_DEF const char* fswatcher_watch(FsWatcherHandle fw)
{
  if (!fw) return NULL;
  
  FsWatcherWindowsHandle *fw_windows = (FsWatcherWindowsHandle*) fw;
  DWORD bytes;
  ULONG_PTR key;
  LPOVERLAPPED overlapped;

  while(GetQueuedCompletionStatus(fw_windows->iocp,
                                  &bytes,
                                  &key,
                                  &overlapped,
                                  INFINITE))
  {
    if (key == 0) return NULL; // Global exit signal

    FsWatcherWindowsDirList* fw_dir = (FsWatcherWindowsDirList*)key;

    if (bytes == 0)
    {
      MICRO_FSWATCHER_FREE(fw_dir);
      continue;
    }

    BYTE* pRaw = fw_dir->buffer;
    FILE_NOTIFY_INFORMATION* info = NULL;
    
    do
    {
      info = (FILE_NOTIFY_INFORMATION*)pRaw;
            
      if (wcsncmp(info->FileName, 
                  fw_dir->pathW, 
                  info->FileNameLength / sizeof(WCHAR)) == 0) 
        {
          ReadDirectoryChangesW(fw_dir->hDir,
                                fw_dir->buffer,
                                sizeof(fw_dir->buffer),
                                FALSE,
                                fw_dir->filter,
                                NULL, 
                                &fw_dir->overlapped,
                                NULL);
          
          return fw_dir->path;
        }

      pRaw += info->NextEntryOffset;

    } while (info->NextEntryOffset != 0);

    ReadDirectoryChangesW(fw_dir->hDir,
                          fw_dir->buffer,
                          sizeof(fw_dir->buffer),
                          FALSE,
                          fw_dir->filter, NULL, 
                          &fw_dir->overlapped,
                          NULL);
  }
  return NULL;
}

#endif // WIN32

#endif // MICRO_FSWATCHER_IMPLEMENTATION

//
// Examples
//
  
#ifdef MICRO_FSWATCHER_EXAMPLE_MAIN

//#define MICRO_FSWATCHER_IMPLEMENTATION
//#include "micro-fswatcher.h"

#include <stdio.h>

int main(void)
{
  FsWatcherHandle fw = fswatcher_init();
  if (fw == NULL)
  {
    fprintf(stderr, "Error initilizing filesystem watcher\n");
    return 1;
  }

  if (fswatcher_add(fw, "./LICENSE", FSWATCHER_EVENT_MODIFY) < 0)
  {
    fprintf(stderr, "Error adding file to filesystem watcher\n");
    return 1;
  }

  const char* file = fswatcher_watch(fw); // blocks
  if (file == NULL)
  {
    fprintf(stderr, "Error watching files\n");
    return 1;
  }
  printf("Got notification on file: %s\n", file);

  if (fswatcher_rm(fw, "./LICENSE") < 0 )
  {
    fprintf(stderr, "Error removing notification on file\n");
    return 1;
  }

  fswatcher_destroy(fw);

  return 0;
}
  
#endif // MICRO_FSWATCHER_EXAMPLE_MAIN
  
#ifdef __cplusplus
}
#endif

#endif // MICRO_FSWATCHER
