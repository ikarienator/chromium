// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/kernel_proxy.h"


#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <iterator>
#include <string>

#include "nacl_io/dbgprint.h"
#include "nacl_io/host_resolver.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_dev.h"
#include "nacl_io/mount_html5fs.h"
#include "nacl_io/mount_http.h"
#include "nacl_io/mount_mem.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/mount_node_tcp.h"
#include "nacl_io/mount_node_udp.h"
#include "nacl_io/mount_passthrough.h"
#include "nacl_io/osmman.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/osstat.h"
#include "nacl_io/path.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/typed_mount_factory.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"
#include "sdk_util/string_util.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

namespace nacl_io {

class SignalEmitter : public EventEmitter {
 public:
  // From EventEmitter.  The SignalEmitter exists in order
  // to inturrupt anything waiting in select()/poll() when kill()
  // is called.  It is an edge trigger only and therefore has no
  // persistent readable/wriable/error state.
  uint32_t GetEventStatus() {
     return 0;
  }

  int GetType() {
    // For lack of a better type, report socket to signify it can be in an
    // used to signal.
    return S_IFSOCK;
  }

  void SignalOccurred() {
    RaiseEvent(POLLERR);
  }
};

KernelProxy::KernelProxy() : dev_(0), ppapi_(NULL),
                             sigwinch_handler_(SIG_IGN),
                             signal_emitter_(new SignalEmitter) {

}

KernelProxy::~KernelProxy() {
  // Clean up the MountFactories.
  for (MountFactoryMap_t::iterator i = factories_.begin();
       i != factories_.end();
       ++i) {
    delete i->second;
  }

  delete ppapi_;
}

Error KernelProxy::Init(PepperInterface* ppapi) {
  Error rtn = 0;
  ppapi_ = ppapi;
  dev_ = 1;

  factories_["memfs"] = new TypedMountFactory<MountMem>;
  factories_["dev"] = new TypedMountFactory<MountDev>;
  factories_["html5fs"] = new TypedMountFactory<MountHtml5Fs>;
  factories_["httpfs"] = new TypedMountFactory<MountHttp>;
  factories_["passthroughfs"] = new TypedMountFactory<MountPassthrough>;

  int result;
  result = mount("", "/", "passthroughfs", 0, NULL);
  if (result != 0) {
    assert(false);
    rtn = errno;
  }

  result = mount("", "/dev", "dev", 0, NULL);
  if (result != 0) {
    assert(false);
    rtn = errno;
  }

  // Open the first three in order to get STDIN, STDOUT, STDERR
  int fd;
  fd = open("/dev/stdin", O_RDONLY);
  assert(fd == 0);
  if (fd < 0)
    rtn = errno;

  fd = open("/dev/stdout", O_WRONLY);
  assert(fd == 1);
  if (fd < 0)
    rtn = errno;

  fd = open("/dev/stderr", O_WRONLY);
  assert(fd == 2);
  if (fd < 0)
    rtn = errno;

#ifdef PROVIDES_SOCKET_API
  host_resolver_.Init(ppapi_);
#endif

  StringMap_t args;
  socket_mount_.reset(new MountSocket());
  result = socket_mount_->Init(0, args, ppapi);
  if (result != 0) {
    assert(false);
    rtn = result;
  }

  return rtn;
}

int KernelProxy::open_resource(const char* path) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  ScopedMountNode node;
  error = mnt->OpenResource(rel, &node);
  if (error) {
    // OpenResource failed, try Open().
    error = mnt->Open(rel, O_RDONLY, &node);
    if (error) {
      errno = error;
      return -1;
    }
  }

  ScopedKernelHandle handle(new KernelHandle(mnt, node));
  error = handle->Init(O_RDONLY);
  if (error) {
    errno = error;
    return -1;
  }

  return AllocateFD(handle);
}

int KernelProxy::open(const char* path, int oflags) {
  ScopedMount mnt;
  ScopedMountNode node;

  Error error = AcquireMountAndNode(path, oflags, &mnt, &node);
  if (error) {
    errno = error;
    return -1;
  }

  ScopedKernelHandle handle(new KernelHandle(mnt, node));
  error = handle->Init(oflags);
  if (error) {
    errno = error;
    return -1;
  }

  return AllocateFD(handle);
}

int KernelProxy::close(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  // Remove the FD from the process open file descriptor map
  FreeFD(fd);
  return 0;
}

int KernelProxy::dup(int oldfd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(oldfd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  return AllocateFD(handle);
}

int KernelProxy::dup2(int oldfd, int newfd) {
  // If it's the same file handle, just return
  if (oldfd == newfd)
    return newfd;

  ScopedKernelHandle old_handle;
  Error error = AcquireHandle(oldfd, &old_handle);
  if (error) {
    errno = error;
    return -1;
  }

  FreeAndReassignFD(newfd, old_handle);
  return newfd;
}

int KernelProxy::chdir(const char* path) {
  Error error = SetCWD(path);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

char* KernelProxy::getcwd(char* buf, size_t size) {
  std::string cwd = GetCWD();

  if (size <= 0) {
    errno = EINVAL;
    return NULL;
  }

  // If size is 0, allocate as much as we need.
  if (size == 0) {
    size = cwd.size() + 1;
  }

  // Verify the buffer is large enough
  if (size <= cwd.size()) {
    errno = ERANGE;
    return NULL;
  }

  // Allocate the buffer if needed
  if (buf == NULL) {
    buf = static_cast<char*>(malloc(size));
  }

  strcpy(buf, cwd.c_str());
  return buf;
}

char* KernelProxy::getwd(char* buf) {
  if (NULL == buf) {
    errno = EFAULT;
    return NULL;
  }
  return getcwd(buf, MAXPATHLEN);
}

int KernelProxy::chmod(const char* path, mode_t mode) {
  int fd = KernelProxy::open(path, O_RDONLY);
  if (-1 == fd)
    return -1;

  int result = fchmod(fd, mode);
  close(fd);
  return result;
}

int KernelProxy::chown(const char* path, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::fchown(int fd, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::lchown(const char* path, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::utime(const char* filename, const struct utimbuf* times) {
  return 0;
}

int KernelProxy::mkdir(const char* path, mode_t mode) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Mkdir(rel, mode);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::rmdir(const char* path) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Rmdir(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::stat(const char* path, struct stat* buf) {
  int fd = open(path, O_RDONLY);
  if (-1 == fd)
    return -1;

  int result = fstat(fd, buf);
  close(fd);
  return result;
}


int KernelProxy::mount(const char* source,
                       const char* target,
                       const char* filesystemtype,
                       unsigned long mountflags,
                       const void* data) {
  std::string abs_path = GetAbsParts(target).Join();

  // Find a factory of that type
  MountFactoryMap_t::iterator factory = factories_.find(filesystemtype);
  if (factory == factories_.end()) {
    errno = ENODEV;
    return -1;
  }

  // Create a map of settings
  StringMap_t smap;
  smap["SOURCE"] = source;
  smap["TARGET"] = abs_path;

  if (data) {
    std::vector<std::string> elements;
    sdk_util::SplitString(static_cast<const char*>(data), ',', &elements);

    for (std::vector<std::string>::const_iterator it = elements.begin();
         it != elements.end(); ++it) {
      size_t location = it->find('=');
      if (location != std::string::npos) {
        std::string key = it->substr(0, location);
        std::string val = it->substr(location + 1);
        smap[key] = val;
      } else {
        smap[*it] = "TRUE";
      }
    }
  }

  ScopedMount mnt;
  Error error = factory->second->CreateMount(dev_++, smap, ppapi_, &mnt);
  if (error) {
    errno = error;
    return -1;
  }

  error = AttachMountAtPath(mnt, abs_path);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::umount(const char* path) {
  Error error = DetachMountAtPath(path);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

ssize_t KernelProxy::read(int fd, void* buf, size_t nbytes) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int cnt = 0;
  error = handle->Read(buf, nbytes, &cnt);
  if (error) {
    errno = error;
    return -1;
  }

  return cnt;
}

ssize_t KernelProxy::write(int fd, const void* buf, size_t nbytes) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int cnt = 0;
  error = handle->Write(buf, nbytes, &cnt);
  if (error) {
    errno = error;
    return -1;
  }

  return cnt;
}

int KernelProxy::fstat(int fd, struct stat* buf) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->GetStat(buf);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::getdents(int fd, void* buf, unsigned int count) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int cnt = 0;
  error = handle->GetDents(static_cast<dirent*>(buf), count, &cnt);
  if (error)
    errno = error;

  return cnt;
}

int KernelProxy::ftruncate(int fd, off_t length) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->FTruncate(length);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::fsync(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->FSync();
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::isatty(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->IsaTTY();
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::ioctl(int fd, int request, char* argp) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Ioctl(request, argp);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

off_t KernelProxy::lseek(int fd, off_t offset, int whence) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  off_t new_offset;
  error = handle->Seek(offset, whence, &new_offset);
  if (error) {
    errno = error;
    return -1;
  }

  return new_offset;
}

int KernelProxy::unlink(const char* path) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Unlink(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::remove(const char* path) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Remove(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

// TODO(noelallen): Needs implementation.
int KernelProxy::fchmod(int fd, int mode) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::access(const char* path, int amode) {
  ScopedMount mnt;
  Path rel;

  Error error = AcquireMountAndRelPath(path, &mnt, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = mnt->Access(rel, amode);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

// TODO(noelallen): Needs implementation.
int KernelProxy::link(const char* oldpath, const char* newpath) {
  errno = EINVAL;
  return -1;
}

int KernelProxy::symlink(const char* oldpath, const char* newpath) {
  errno = EINVAL;
  return -1;
}

void* KernelProxy::mmap(void* addr,
                        size_t length,
                        int prot,
                        int flags,
                        int fd,
                        size_t offset) {
  // We shouldn't be getting anonymous mmaps here.
  assert((flags & MAP_ANONYMOUS) == 0);
  assert(fd != -1);

  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return MAP_FAILED;
  }

  void* new_addr;
  error = handle->node()->MMap(addr, length, prot, flags, offset, &new_addr);
  if (error) {
    errno = error;
    return MAP_FAILED;
  }

  return new_addr;
}

int KernelProxy::munmap(void* addr, size_t length) {
  // NOTE: The comment below is from a previous discarded implementation that
  // tracks mmap'd regions. For simplicity, we no longer do this; because we
  // "snapshot" the contents of the file in mmap(), and don't support
  // write-back or updating the mapped region when the file is written, holding
  // on to the KernelHandle is pointless.
  //
  // If we ever do, these threading issues should be considered.

  //
  // WARNING: this function may be called by free().
  //
  // There is a potential deadlock scenario:
  // Thread 1: open() -> takes lock1 -> free() -> takes lock2
  // Thread 2: free() -> takes lock2 -> munmap() -> takes lock1
  //
  // Note that open() above could be any function that takes a lock that is
  // shared with munmap (this includes munmap!)
  //
  // To prevent this, we avoid taking locks in munmap() that are used by other
  // nacl_io functions that may call free. Specifically, we only take the
  // mmap_lock, which is only shared with mmap() above. There is still a
  // possibility of deadlock if mmap() or munmap() calls free(), so this is not
  // allowed.
  //
  // Unfortunately, munmap still needs to acquire other locks; see the call to
  // ReleaseHandle below which takes the process lock. This is safe as long as
  // this is never executed from free() -- we can be reasonably sure this is
  // true, because malloc only makes anonymous mmap() requests, and should only
  // be munmapping those allocations. We never add to mmap_info_list_ for
  // anonymous maps, so the unmap_list should always be empty when called from
  // free().
  return 0;
}

int KernelProxy::tcflush(int fd, int queue_selector) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Tcflush(queue_selector);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::tcgetattr(int fd, struct termios* termios_p) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Tcgetattr(termios_p);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::tcsetattr(int fd, int optional_actions,
                           const struct termios *termios_p) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Tcsetattr(optional_actions, termios_p);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::kill(pid_t pid, int sig) {
  // Currently we don't even pretend that other processes exist
  // so we can only send a signal to outselves.  For kill(2)
  // pid 0 means the current process group and -1 means all the
  // processes we have permission to send signals to.
  if (pid != getpid() && pid != -1 && pid != 0) {
    errno = ESRCH;
    return -1;
  }

  // Raise an event so that select/poll get interrupted.
  signal_emitter_->SignalOccurred();
  switch (sig) {
    case SIGWINCH:
      if (sigwinch_handler_ != SIG_IGN)
        sigwinch_handler_(SIGWINCH);
      break;

    case SIGUSR1:
    case SIGUSR2:
      break;

    default:
      errno = EINVAL;
      return -1;
  }

  return 0;
}

sighandler_t KernelProxy::sigset(int signum, sighandler_t handler) {
  switch (signum) {
    // Handled signals.
    case SIGWINCH: {
      sighandler_t old_value = sigwinch_handler_;
      if (handler == SIG_DFL)
        handler = SIG_IGN;
      sigwinch_handler_ = handler;
      return old_value;
    }

    // Known signals
    case SIGHUP:
    case SIGINT:
    case SIGKILL:
    case SIGPIPE:
    case SIGPOLL:
    case SIGPROF:
    case SIGTERM:
    case SIGCHLD:
    case SIGURG:
    case SIGFPE:
    case SIGILL:
    case SIGQUIT:
    case SIGSEGV:
    case SIGTRAP:
      if (handler == SIG_DFL)
        return SIG_DFL;
      break;
  }

  errno = EINVAL;
  return SIG_ERR;
}

#ifdef PROVIDES_SOCKET_API

int KernelProxy::select(int nfds, fd_set* readfds, fd_set* writefds,
                        fd_set* exceptfds, struct timeval* timeout) {
  ScopedEventListener listener(new EventListener);

  std::vector<struct pollfd> fds;

  fd_set readout, writeout, exceptout;

  FD_ZERO(&readout);
  FD_ZERO(&writeout);
  FD_ZERO(&exceptout);

  int fd;
  size_t event_cnt = 0;
  int event_track = 0;
  for (fd = 0; fd < nfds; fd++) {
    int events = 0;

    if (readfds != NULL && FD_ISSET(fd, readfds))
      events |= POLLIN;

    if (writefds != NULL && FD_ISSET(fd, writefds))
      events |= POLLOUT;

    if (exceptfds != NULL && FD_ISSET(fd, exceptfds))
      events |= POLLERR | POLLHUP;

    // If we are not interested in this FD, skip it
    if (0 == events) continue;

    ScopedKernelHandle handle;
    Error err = AcquireHandle(fd, &handle);

    // Select will return immediately if there are bad FDs.
    if (err != 0) {
      errno = EBADF;
      return -1;
    }

    int status = handle->node()->GetEventStatus() & events;
    if (status & POLLIN) {
      FD_SET(fd, &readout);
      event_cnt++;
    }

    if (status & POLLOUT) {
      FD_SET(fd, &writeout);
      event_cnt++;
    }

    if (status & (POLLERR | POLLHUP)) {
      FD_SET(fd, &exceptout);
      event_cnt++;
    }

    // Otherwise track it.
    if (0 == status) {
      err = listener->Track(fd, handle->node(), events, fd);
      if (err != 0) {
        errno = EBADF;
        return -1;
      }
      event_track++;
    }
  }

  // If nothing is signaled, then we must wait.
  if (event_cnt == 0) {
    std::vector<EventData> events;
    int ready_cnt;
    int ms_timeout;

    // NULL timeout signals wait forever.
    if (timeout == NULL) {
      ms_timeout = -1;
    } else {
      int64_t ms = timeout->tv_sec * 1000 + ((timeout->tv_usec + 500) / 1000);

      // If the timeout is invalid or too long (larger than signed 32 bit).
      if ((timeout->tv_sec < 0) || (timeout->tv_sec >= (INT_MAX / 1000)) ||
          (timeout->tv_usec < 0) || (timeout->tv_usec >= 1000000) ||
          (ms < 0) || (ms >= INT_MAX)) {
        errno = EINVAL;
        return -1;
      }

      ms_timeout = static_cast<int>(ms);
    }

    // Add a special node to listen for events
    // coming from the KernelProxy itself (kill will
    // generated a SIGERR event).
    listener->Track(-1, signal_emitter_, POLLERR, -1);
    event_track += 1;

    events.resize(event_track);

    bool interrupted = false;
    listener->Wait(events.data(), event_track, ms_timeout, &ready_cnt);
    for (fd = 0; static_cast<int>(fd) < ready_cnt; fd++) {
      if (events[fd].user_data == static_cast<uint64_t>(-1)) {
        if (events[fd].events & POLLERR) {
          interrupted = true;
        }
        continue;
      }

      if (events[fd].events & POLLIN) {
        FD_SET(events[fd].user_data, &readout);
        event_cnt++;
      }

      if (events[fd].events & POLLOUT) {
        FD_SET(events[fd].user_data, &writeout);
        event_cnt++;
      }

      if (events[fd].events & (POLLERR | POLLHUP)) {
        FD_SET(events[fd].user_data, &exceptout);
        event_cnt++;
      }
    }

    if (0 == event_cnt && interrupted) {
      errno = EINTR;
      return -1;
    }
  }

  // Copy out the results
  if (readfds != NULL)
    *readfds = readout;

  if (writefds != NULL)
    *writefds = writeout;

  if (exceptfds != NULL)
    *exceptfds = exceptout;

  return event_cnt;
}

int KernelProxy::poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  ScopedEventListener listener(new EventListener);
  listener->Track(-1, signal_emitter_, POLLERR, 0);

  int index;
  size_t event_cnt = 0;
  size_t event_track = 1;
  for (index = 0; static_cast<nfds_t>(index) < nfds; index++) {
    ScopedKernelHandle handle;
    struct pollfd* info = &fds[index];
    Error err = AcquireHandle(info->fd, &handle);

    // If the node isn't open, or somehow invalid, mark it so.
    if (err != 0) {
      info->revents = POLLNVAL;
      event_cnt++;
      continue;
    }

    // If it's already signaled, then just capture the event
    if (handle->node()->GetEventStatus() & info->events) {
      info->revents = info->events & handle->node()->GetEventStatus();
      event_cnt++;
      continue;
    }

    // Otherwise try to track it.
    err = listener->Track(info->fd, handle->node(), info->events, index);
    if (err != 0) {
      info->revents = POLLNVAL;
      event_cnt++;
      continue;
    }
    event_track++;
  }

  // If nothing is signaled, then we must wait.
  if (0 == event_cnt) {
    std::vector<EventData> events;
    int ready_cnt;

    bool interrupted = false;
    events.resize(event_track);
    listener->Wait(events.data(), event_track, timeout, &ready_cnt);
    for (index = 0; index < ready_cnt; index++) {
      struct pollfd* info = &fds[events[index].user_data];
      if (!info) {
        interrupted = true;
        continue;
      }

      info->revents = events[index].events;
      event_cnt++;
    }
    if (0 == event_cnt && interrupted) {
      errno = EINTR;
      return -1;
    }
  }

  return event_cnt;
}



// Socket Functions
int KernelProxy::accept(int fd, struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  errno = EINVAL;
  return -1;
}

int KernelProxy::bind(int fd, const struct sockaddr* addr, socklen_t len) {
  if (NULL == addr) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->Bind(addr, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::connect(int fd, const struct sockaddr* addr, socklen_t len) {
  if (NULL == addr) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->Connect(addr, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

struct hostent* KernelProxy::gethostbyname(const char* name) {
  return host_resolver_.gethostbyname(name);
}

int KernelProxy::getpeername(int fd, struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->GetPeerName(addr, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::getsockname(int fd, struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->GetSockName(addr, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::getsockopt(int fd,
                         int lvl,
                         int optname,
                         void* optval,
                         socklen_t* len) {
  if (NULL == optval || NULL == len) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  errno = EINVAL;
  return -1;
}

int KernelProxy::listen(int fd, int backlog) {
  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  errno = EOPNOTSUPP;
  return -1;
}

ssize_t KernelProxy::recv(int fd,
                          void* buf,
                          size_t len,
                          int flags) {
  if (NULL == buf) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  int out_len = 0;
  Error err = handle->socket_node()->Recv(buf, len, flags, &out_len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return static_cast<ssize_t>(out_len);
}

ssize_t KernelProxy::recvfrom(int fd,
                              void* buf,
                              size_t len,
                              int flags,
                              struct sockaddr* addr,
                              socklen_t* addrlen) {
  // According to the manpage, recvfrom with a null addr is identical to recv.
  if (NULL == addr) {
    return recv(fd, buf, len, flags);
  }

  if (NULL == buf || NULL == addrlen) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  int out_len = 0;
  Error err = handle->socket_node()->RecvFrom(buf,
                                              len,
                                              flags,
                                              addr,
                                              addrlen,
                                              &out_len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return static_cast<ssize_t>(out_len);
}

ssize_t KernelProxy::recvmsg(int fd, struct msghdr* msg, int flags) {
  if (NULL == msg ) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  errno = EOPNOTSUPP;
  return -1;
}

ssize_t KernelProxy::send(int fd, const void* buf, size_t len, int flags) {
  if (NULL == buf) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  int out_len = 0;
  Error err = handle->socket_node()->Send(buf, len, flags, &out_len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return static_cast<ssize_t>(out_len);
}

ssize_t KernelProxy::sendto(int fd,
                            const void* buf,
                            size_t len,
                            int flags,
                            const struct sockaddr* addr,
                            socklen_t addrlen) {
  // According to the manpage, sendto with a null addr is identical to send.
  if (NULL == addr) {
    return send(fd, buf, len, flags);
  }

  if (NULL == buf) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  int out_len = 0;
  Error err =
      handle->socket_node()->SendTo(buf, len, flags, addr, addrlen, &out_len);

  if (err != 0) {
    errno = err;
    return -1;
  }

  return static_cast<ssize_t>(out_len);
}

ssize_t KernelProxy::sendmsg(int fd, const struct msghdr* msg, int flags) {
  if (NULL == msg) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  errno = EOPNOTSUPP;
  return -1;
}

int KernelProxy::setsockopt(int fd,
                            int lvl,
                            int optname,
                            const void* optval,
                            socklen_t len) {
  if (NULL == optval) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  errno = EINVAL;
  return -1;
}

int KernelProxy::shutdown(int fd, int how) {
  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->Shutdown(how);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::socket(int domain, int type, int protocol) {
  if (AF_INET != domain && AF_INET6 != domain) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  MountNodeSocket* sock = NULL;
  switch (type) {
    case SOCK_DGRAM:
      sock = new MountNodeUDP(socket_mount_.get());
      break;

    case SOCK_STREAM:
      sock = new MountNodeTCP(socket_mount_.get());
      break;

    default:
      errno = EPROTONOSUPPORT;
      return -1;
  }

  ScopedMountNode node(sock);
  if (sock->Init(S_IREAD | S_IWRITE) == 0) {
    ScopedKernelHandle handle(new KernelHandle(socket_mount_, node));
    return AllocateFD(handle);
  }

  // If we failed to init, assume we don't have access.
  return EACCES;
}

int KernelProxy::socketpair(int domain, int type, int protocol, int* sv) {
  if (NULL == sv) {
    errno = EFAULT;
    return -1;
  }

  // Catch-22: We don't support AF_UNIX, but any other AF doesn't support
  // socket pairs. Thus, this function always fails.
  if (AF_UNIX != domain) {
    errno = EPROTONOSUPPORT;
    return -1;
  }

  if (AF_INET != domain && AF_INET6 != domain) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  // We cannot reach this point.
  errno = ENOSYS;
  return -1;
}

int KernelProxy::AcquireSocketHandle(int fd, ScopedKernelHandle* handle) {
  Error error = AcquireHandle(fd, handle);

  if (error) {
    errno = error;
    return -1;
  }

  if ((handle->get()->node_->GetType() & S_IFSOCK) == 0) {
    errno = ENOTSOCK;
    return -1;
  }

  return 0;
}

#endif  // PROVIDES_SOCKET_API

}  // namespace_nacl_io
