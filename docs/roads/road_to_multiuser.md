# Road to Multi-User Support on meniOS

**Goal**: Transform meniOS from a single-user system into a proper multi-user operating system with authentication, permissions, and user isolation.

## Current Status

**Single-User System:**
- No user/group concept
- No file permissions beyond basic read/write
- No authentication or login
- All processes run with full privileges
- No resource isolation between users

### Recent Foundations
- **Process lifecycle plumbing** is in place: `waitpid`/zombie reparenting (#145/#146) mean a login manager can supervise user shells without leaking PIDs
- **`init` now runs as PID 1** (#149/#150), giving us a natural parent for login/getty daemons and future service managers
- **Signal delivery primitives** (#103) allow clean termination of user sessions, a prerequisite for secure logouts
- `getcwd`/`chdir` (#147) now keeps per-process working directories; environment handling (#148) supplies HOME/PATH defaults for login shells
- **TTY subsystem** (#169) delivers canonical input and echo, paving the way for login prompts on virtual terminals

## Multi-User System Requirements

### Phase 1: User and Group Infrastructure

#### User/Group Identification
**Core Concepts:**
- User ID (UID) - numeric identifier for users (0 = root, 1-999 = system, 1000+ = regular users)
- Group ID (GID) - numeric identifier for groups
- Username - human-readable name
- Home directory - user's personal directory
- Login shell - default shell for user

**What's Needed:**
1. **User database** (`/etc/passwd`)
   - Format: `username:x:uid:gid:comment:home:shell`
   - Example: `root:x:0:0:root:/root:/bin/mosh`
   - Example: `alice:x:1000:1000:Alice:/home/alice:/bin/mosh`

2. **Group database** (`/etc/group`)
   - Format: `groupname:x:gid:member1,member2`
   - Example: `root:x:0:`
   - Example: `users:x:100:alice,bob`

3. **Shadow passwords** (`/etc/shadow`)
   - Format: `username:password_hash:last_change:min:max:warn:inactive:expire`
   - Example: `root:$6$rounds=5000$....:18000:0:99999:7:::`
   - Readable only by root for security

**Estimated Effort:** 1 week

#### Process Credentials
**What's Needed:**
1. Add UID/GID fields to process structure
2. Real UID/GID (actual user who created process)
3. Effective UID/GID (used for permission checks)
4. Saved UID/GID (for setuid programs)
5. Supplementary groups (additional group memberships)

**Changes Required:**
- Extend `process_t` structure with credential fields
- Inherit credentials on fork()
- Update credentials on exec() (handle setuid binaries)

**Estimated Effort:** 3-5 days

### Phase 2: Authentication System

#### Login Program
**What's Needed:**
1. Login program (`/bin/login`)
   - Prompt for username and password
   - Verify credentials against `/etc/shadow`
   - Set up user session (UID, GID, HOME, shell)
   - Execute user's login shell
   - Log successful/failed attempts

2. Password hashing
   - Support for bcrypt, scrypt, or PBKDF2
   - Salt generation
   - Secure password storage

3. Getty/Terminal management
   - Spawn login on TTY devices from PID 1 `init`
   - Handle multiple simultaneous logins
   - TTY allocation and management

**Estimated Effort:** 1-2 weeks

#### Session Management
**What's Needed:**
1. Session ID (SID) per login session
2. Process groups and session leaders
3. Controlling terminal association
4. Login accounting (utmp/wtmp)

**Estimated Effort:** 1 week

### Phase 3: File System Permissions

#### Permission Bits
**What's Needed:**
1. Extend inode structure with:
   - Owner UID
   - Owner GID
   - Permission bits (rwxrwxrwx)
   - Special bits (setuid, setgid, sticky)

2. Permission format: `-rwxr-xr-x`
   - Owner: rwx (read, write, execute)
   - Group: r-x (read, execute)
   - Other: r-x (read, execute)

3. Special permission bits:
   - **setuid** (4000) - Execute as file owner
   - **setgid** (2000) - Execute as file group
   - **sticky** (1000) - Only owner can delete (for /tmp)

**Estimated Effort:** 1 week

#### Permission Checking
**What's Needed:**
1. VFS layer permission checks:
   - Check on open/read/write/execute
   - Owner check: if UID matches, use owner permissions
   - Group check: if GID matches, use group permissions
   - Other check: use other permissions

2. Root bypass: UID 0 bypasses most checks

3. Syscalls for permission management:
   - `chmod()` - change permissions
   - `chown()` - change owner/group
   - `access()` - test file accessibility

**Estimated Effort:** 1-2 weeks

#### FAT32 Limitations
**Challenge:** FAT32 has no native permission support

**Solutions:**
1. **Mount-time options:** Apply blanket UID/GID/permissions to all files
2. **Extended attributes:** Store permissions in alternate data streams (complex)
3. **Overlay filesystem:** Map permissions in memory (lost on unmount)
4. **Recommend ext2:** Native permission support (#148)

**Estimated Effort:** 3-5 days (mount options approach)

### Phase 4: Security Syscalls

#### User/Group Syscalls
**What's Needed:**
1. **Query syscalls:**
   - `getuid()` - get real UID
   - `geteuid()` - get effective UID
   - `getgid()` - get real GID
   - `getegid()` - get effective GID
   - `getgroups()` - get supplementary groups

2. **Modification syscalls:**
   - `setuid(uid)` - set UID (root only, or to current RUID)
   - `seteuid(uid)` - set effective UID
   - `setreuid(ruid, euid)` - set real and effective UID
   - `setgid(gid)` - set GID
   - `setegid(gid)` - set effective GID
   - `setregid(rgid, egid)` - set real and effective GID
   - `setgroups(groups)` - set supplementary groups (root only)

3. **Security rules:**
   - Unprivileged processes can only switch between real, effective, and saved IDs
   - Root (UID 0) can set IDs to any value
   - Effective UID is used for permission checks

**Estimated Effort:** 1 week

#### Permission Check Updates
**What's Needed:**
1. Update all syscalls to check effective UID/GID:
   - File operations (open, read, write, unlink, etc.)
   - Process operations (kill, ptrace, etc.)
   - System operations (reboot, mount, etc.)

2. Special cases:
   - Root (UID 0) bypasses most checks
   - Some operations require specific capabilities
   - Owner-only operations (e.g., chmod own file)

**Estimated Effort:** 2-3 weeks (comprehensive audit of all syscalls)

### Phase 5: User Management Tools

#### Essential Utilities
**What's Needed:**
1. **User management:**
   - `useradd` - add new user
   - `userdel` - delete user
   - `usermod` - modify user
   - `passwd` - change password
   - `id` - display user/group IDs
   - `whoami` - display current username
   - `who` - show logged in users

2. **Group management:**
   - `groupadd` - add new group
   - `groupdel` - delete group
   - `groupmod` - modify group
   - `groups` - show user's groups

3. **File permissions:**
   - `chmod` - change file permissions
   - `chown` - change file owner
   - `chgrp` - change file group
   - `ls -l` - list with permissions (extend existing ls)

4. **Authentication:**
   - `su` - switch user
   - `sudo` - execute command as another user

**Estimated Effort:** 2-3 weeks

### Phase 6: Home Directories and User Environment

#### Home Directory Setup
**What's Needed:**
1. Create `/home` directory
2. Create user home directories (`/home/username`)
3. Set proper ownership and permissions (700 or 755)
4. Copy skeleton files from `/etc/skel`
   - `.profile` - shell configuration
   - `.moshrc` - shell startup script

**Estimated Effort:** 2-3 days

#### Environment Variables
**Already Planned:** #148 - Environment variables

**Additional for Multi-User:**
- `USER` - current username
- `HOME` - user's home directory
- `LOGNAME` - login name
- Set automatically on login

**Estimated Effort:** 1 day (extends #148)

### Phase 7: Resource Limits and Quotas

#### Resource Limits (ulimit)
**What's Needed:**
1. Per-process resource limits:
   - Max CPU time
   - Max file size
   - Max open files
   - Max processes
   - Max memory
   - Max stack size

2. Syscalls:
   - `getrlimit()` - query resource limit
   - `setrlimit()` - set resource limit
   - `prlimit()` - get/set limit for another process

3. Enforcement:
   - Check limits before allocating resources
   - Return EPERM or EDQUOT on limit exceeded
   - Inherit limits on fork()

**Estimated Effort:** 1-2 weeks

#### Disk Quotas (Optional)
**What's Needed:**
1. Track disk usage per user/group
2. Hard limits (cannot exceed)
3. Soft limits (warning, grace period)
4. Quota database
5. `quota`, `quotaon`, `quotaoff` utilities

**Estimated Effort:** 2-3 weeks
**Priority:** Low - nice to have

### Phase 8: Advanced Security Features

#### Capabilities (Optional)
**Purpose:** Fine-grained privileges instead of all-or-nothing root

**Examples:**
- `CAP_NET_BIND_SERVICE` - bind to ports < 1024
- `CAP_SYS_ADMIN` - mount filesystems
- `CAP_KILL` - send signals to any process
- `CAP_CHOWN` - change file ownership

**What's Needed:**
1. Capability bits per process
2. Capability-aware permission checks
3. Syscalls: `capget()`, `capset()`
4. File capabilities (extended attributes)

**Estimated Effort:** 3-4 weeks
**Priority:** Low - advanced feature

#### SELinux/AppArmor (Optional)
**Purpose:** Mandatory Access Control (MAC)

**Too Complex:** Not recommended for initial implementation
**Priority:** Very Low

### Phase 9: Testing and Hardening

#### Security Testing
**What's Needed:**
1. Test unprivileged user cannot:
   - Read other users' files
   - Kill other users' processes
   - Modify system files
   - Escalate privileges

2. Test setuid programs work correctly:
   - `su`, `sudo`, `passwd` run with elevated privileges
   - Proper credential switching

3. Test edge cases:
   - Root to unprivileged transitions
   - Multiple group memberships
   - Permission inheritance on fork/exec

**Estimated Effort:** 1-2 weeks

#### Audit and Fixes
**What's Needed:**
1. Comprehensive security audit of all syscalls
2. Fix any privilege escalation vulnerabilities
3. Harden credential handling
4. Add logging for security events

**Estimated Effort:** 2-3 weeks

## Implementation Roadmap

### Month 1: Foundation
- **Week 1-2:** User/Group infrastructure (3 issues TBD)
  - User/group databases
  - Process credentials
  - Basic UID/GID support
- **Week 3-4:** Authentication system (2 issues TBD)
  - Login program
  - Password hashing
  - Session management

### Month 2: Permissions
- **Week 1-2:** File system permissions (2 issues TBD)
  - Inode permission bits
  - VFS permission checking
  - FAT32 mount options
- **Week 3-4:** Security syscalls (2 issues TBD)
  - getuid/setuid family
  - Permission check updates

### Month 3: User Tools
- **Week 1-2:** User management utilities (1 issue TBD)
  - useradd, passwd, id, etc.
  - chmod, chown utilities
- **Week 3-4:** su/sudo implementation (1 issue TBD)
  - Switch user mechanism
  - Privilege elevation

### Month 4: Polish and Testing
- **Week 1-2:** Resource limits (1 issue TBD)
  - ulimit support
  - Quota system (optional)
- **Week 3-4:** Security testing and hardening (1 issue TBD)
  - Comprehensive testing
  - Security audit
  - Bug fixes

## Dependencies

### Existing Issues
- **#60** - File I/O syscalls (CLOSED) - needed for reading user databases
- **#65** - VFS layer (CLOSED) - needs permission checking added
- **#93** - fork/exec (CLOSED) - needs credential inheritance
- **#148** - ext2 filesystem - native permission support
- **#148** - Environment variables - USER, HOME, etc.
- **#153** - init program (CLOSED) - needs to spawn getty/login

### New Issues Needed (Not Yet Created)
- **TBD** - User/group database infrastructure
- **TBD** - Process credentials (UID/GID)
- **TBD** - User database parsing (/etc/passwd, /etc/group, /etc/shadow)
- **TBD** - Login program and authentication
- **TBD** - Session management and getty
- **TBD** - File system permission bits
- **TBD** - VFS permission checking
- **TBD** - Security syscalls (getuid/setuid family)
- **TBD** - Update all syscalls for permission checks
- **TBD** - User management utilities (useradd, passwd, chmod, etc.)
- **TBD** - su and sudo implementation
- **TBD** - Resource limits (ulimit/getrlimit/setrlimit)
- **TBD** - Security testing and audit

**Note**: These issues will be created when multi-user development begins. Issue numbers #166-#170 are currently used for shell terminal integration.

## Success Criteria

meniOS will be a proper multi-user system when:
- ✅ Multiple users can log in simultaneously
- ✅ Each user has their own home directory
- ✅ File permissions prevent unauthorized access
- ✅ Users cannot interfere with each other's processes
- ✅ Root user has administrative privileges
- ✅ Regular users have restricted privileges
- ✅ Authentication prevents unauthorized access
- ✅ setuid programs work correctly (su, sudo, passwd)
- ✅ Resource limits prevent resource exhaustion
- ✅ System remains secure under adversarial testing

## Comparison: Single-User vs Multi-User

| Feature | Single-User (Current) | Multi-User (Target) |
|---------|----------------------|---------------------|
| Users | All processes run as "root" | Multiple users with different privileges |
| Authentication | None | Login with username/password |
| File Permissions | Basic or none | Owner/Group/Other with rwx bits |
| Process Isolation | None | Users cannot kill others' processes |
| Home Directories | Single /root or none | Per-user /home/username |
| Resource Limits | None | Per-user limits on CPU, memory, files |
| Security | No privilege separation | Root vs. unprivileged users |
| Privacy | All files readable by all | Users' files private by default |

## Timeline Estimate

**Total Effort:** 4-6 months for full multi-user support

**Minimum Viable Multi-User (2-3 months):**
- Basic user/group infrastructure
- Simple authentication (no password hashing)
- File permissions
- Login program
- Essential utilities (useradd, passwd, chmod)

**Production-Ready Multi-User (4-6 months):**
- All features listed above
- Secure password hashing
- Resource limits
- Comprehensive testing
- Security hardening

## Current Priority

**Status:** LOW PRIORITY

Multi-user support is not needed for:
- Initial system bring-up
- Running Doom
- Basic application development
- Single-developer use case

**When to Implement:**
- After shell is complete (#147-#165)
- After filesystem infrastructure is solid (#145-#148)
- After core applications work
- When preparing for production deployment
- When multiple users need system access

## Future Enhancements

Beyond basic multi-user support:
1. **SSH server** - Remote login
2. **X11/Wayland** - Multi-user graphical interface
3. **Containers** - User namespace isolation
4. **LDAP/Active Directory** - Centralized user management
5. **Two-factor authentication** - Enhanced security
6. **Audit logging** - Security event tracking
7. **Mandatory Access Control** - SELinux/AppArmor

## Notes

- Multi-user support is a significant undertaking
- Requires careful security design
- Every syscall needs permission checking
- Testing is critical to prevent vulnerabilities
- Consider consulting security experts before production deployment
- Start with minimal implementation, iterate based on needs
