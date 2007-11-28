/*
MySecureShell permit to add restriction to modified sftp-server
when using MySecureShell as shell.
Copyright (C) 2007 Sebastien Tardif

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation (version 2)

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "../config.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "Access.h"
#include "Defines.h"
#include "Encoding.h"
#include "Log.h"
#include "Sftp.h"
#include "GetUsersInfos.h"

tGlobal	*gl_var = NULL;

#include "SftpServer.h"

static void	end_sftp()
{
  if (gl_var)
    {
      if (cVersion != SSH2_ADMIN_VERSION)
	mylog_printf(MYLOG_CONNECTION, "[%s][%s]Quit.", gl_var->who->user, gl_var->who->ip);
      mylog_close();
      if (gl_var->who != NULL)
	{
	  gl_var->who->status = SFTPWHO_EMPTY;
	  SftpWhoRelaseStruct();
	  gl_var->who = NULL;
	}
      if (gl_var->has_hide_files == MSS_TRUE)
	{
	  regfree(&gl_var->hide_files_regexp);
	  gl_var->has_hide_files = MSS_FALSE;
	}
      if (gl_var->has_deny_filter == MSS_TRUE)
	{
	  regfree(&gl_var->deny_filter_regexp);
	  gl_var->has_deny_filter = MSS_FALSE;
	}
      free(gl_var);
      gl_var = NULL;
      setCharset(NULL);
    }
  _exit(0);
}

static void	end_sftp_by_signal(int signal)
{
  gl_var->must_shutdown = 1;
}

static void	reopen_log_file(int signal)
{
  mylog_reopen();
}

void	ParseConf(tGlobal *params, int sftpProtocol)
{
  gl_var = params;
  (void )atexit(end_sftp);
  (void )signal(SIGHUP, end_sftp_by_signal);
  (void )signal(SIGINT, end_sftp_by_signal);
  (void )signal(SIGTERM, end_sftp_by_signal);
  (void )signal(SIGUSR1, reopen_log_file);
  (void )signal(SIGUSR2, reopen_log_file);
  if (sftpProtocol > 0)
    cVersion = sftpProtocol;
}

#ifdef MSSEXT_FILE_HASHING
#include <openssl/evp.h>
#endif

void	DoInitUser()
{
  mylog_printf(MYLOG_CONNECTION, "New client [%s] from [%s]", gl_var->who->user, gl_var->who->ip);
  if (chdir(gl_var->who->home) == -1)
    mylog_printf(MYLOG_ERROR, "[%s][%s]Couldn't go to home '%s' : %s",
		 gl_var->who->user, gl_var->who->ip, gl_var->who->home, strerror(errno));
#ifdef MSSEXT_FILE_HASHING
  OpenSSL_add_all_digests();
#endif
  init_usersinfos();//load users / groups into memory
  if (HAS_BIT(gl_var->who->status, SFTPWHO_VIRTUAL_CHROOT))
    {
      if (chroot(gl_var->who->home) != -1)
	{
	  gl_var->who->status &= ~SFTPWHO_STAY_AT_HOME;
	  if (chdir("/") == -1)
	    mylog_printf(MYLOG_ERROR, "[%s][%s]Couldn't change directory : %s",
			 gl_var->who->user, gl_var->who->ip, strerror(errno));
	}
      else
	{
	  mylog_printf(MYLOG_ERROR, "[%s][%s]Couldn't chroot : %s",
		     gl_var->who->user, gl_var->who->ip, strerror(errno));
	  gl_var->who->status &= ~SFTPWHO_VIRTUAL_CHROOT;
	  gl_var->who->status |= SFTPWHO_STAY_AT_HOME;
	}
    }
  if (getuid() != geteuid()) //revoke root rights in user mode !
    {
      if (seteuid(getuid()) == -1 || setegid(getgid()) == -1)
	{
	  mylog_printf(MYLOG_ERROR, "[%s][%s]Couldn't revoke root rights : %s",
		       gl_var->who->user, gl_var->who->ip, strerror(errno));
	  exit(255);
	}
    }
}

int	CheckRules(const char *pwd, int operation, const struct stat *st, int flags)
{
  const char	*str;

  if (gl_var->has_hide_files == MSS_TRUE
      && (operation >= RULES_LISTING
	  || (operation == RULES_DIRECTORY && HAS_BIT(flags, O_RDONLY))
	  || (operation == RULES_FILE && HAS_BIT(flags, O_RDONLY))))
    {
      if ((str = strrchr(pwd, '/')))
	str = &str[1];
      else //should not exist...
	str = pwd;
      if (regexec(&gl_var->hide_files_regexp, str, 0, 0, 0) != REG_NOMATCH)
	return SSH2_FX_NO_SUCH_FILE;
    }
  if (operation != RULES_LISTING && HAS_BIT(gl_var->who->status, SFTPWHO_STAY_AT_HOME))
    {
      if ((strncmp(pwd, gl_var->who->home, strlen(gl_var->who->home)) == 0 || pwd[0] != '/')
	  && strstr(pwd, "/..") == NULL)
	;
      else
	return SSH2_FX_PERMISSION_DENIED;
    }
  if (HAS_BIT(gl_var->who->status, SFTPWHO_IGNORE_HIDDEN)
      && ((operation >= RULES_DIRECTORY && HAS_BIT(flags, O_RDONLY))
	  || operation == RULES_FILE
	  || operation == RULES_LISTING))
    {
      if (strstr(pwd, "/.") != NULL)
	return SSH2_FX_NO_SUCH_FILE;
    }
  if (gl_var->has_deny_filter == MSS_TRUE
      && ((operation == RULES_FILE && (HAS_BIT(flags, O_WRONLY) || HAS_BIT(flags, O_RDWR)))
	  || (operation == RULES_DIRECTORY && HAS_BIT(flags, O_WRONLY))))
    {
      if ((str = strrchr(pwd, '/')) != NULL)
	str = &str[1];
      else
	str = pwd;
      if (regexec(&gl_var->deny_filter_regexp, str, 0, 0, 0) != REG_NOMATCH)
	return SSH2_FX_PERMISSION_DENIED;
    }
  //This code should always be at the end of this function
  if (operation == RULES_LISTING && st != NULL)
    {
      if (HAS_BIT(gl_var->who->status, SFTPWHO_LINKS_AS_LINKS))
	{
	  struct stat	localst;
	  
	  if ((st->st_mode & S_IFMT) == S_IFLNK && stat(pwd, &localst) != -1)
	    st = &localst;
	}
      if (HAS_BIT(gl_var->who->status, SFTPWHO_HIDE_NO_ACESS))
	{
	  if ((st->st_uid == getuid() && HAS_BIT(st->st_mode, S_IRUSR))
	      || (UserIsInThisGroup(st->st_gid) == 1 && HAS_BIT(st->st_mode, S_IRGRP))
	      || HAS_BIT(st->st_mode, S_IROTH))
	    return SSH2_FX_OK;
	  return SSH2_FX_NO_SUCH_FILE;
	}
    }
  return SSH2_FX_OK;
}

void	ChangeRights(struct stat *st)
{
  if (HAS_BIT(gl_var->who->status, SFTPWHO_FAKE_USER))
    st->st_uid = getuid();
  if (HAS_BIT(gl_var->who->status, SFTPWHO_FAKE_GROUP))
    st->st_gid = getgid();
  if (HAS_BIT(gl_var->who->status, SFTPWHO_FAKE_MODE))
    {
      st->st_mode = (st->st_mode & ~0x1fff) | gl_var->who->mode;
      if (HAS_BIT(st->st_mode, S_IFDIR))
	{
	  if (HAS_BIT(gl_var->who->mode, S_IRUSR))
	    st->st_mode |= S_IXUSR;
	  if (HAS_BIT(gl_var->who->mode, S_IRGRP))
	    st->st_mode |= S_IXGRP;
	  if (HAS_BIT(gl_var->who->mode, S_IROTH))
	    st->st_mode |= S_IXOTH;
	}
    }
}

int	CheckRulesAboutMaxFiles()
{
  t_sftpwho	*who;
  int		i, fileread, filewrite, fileall;

  if ((who = SftWhoGetAllStructs()) != NULL)
    {
      fileread = 0;
      filewrite = 0;
      fileall = 0;
      for (i = 0; i < SFTPWHO_MAXCLIENT; i++)
	if (strcmp(who[i].user, gl_var->who->user) == 0)
	  {
	    switch (who[i].status & SFTPWHO_STATUS_MASK)
	      {
	      case SFTPWHO_PUT:
		filewrite++;
		fileall++;
		break;
	      case SFTPWHO_GET:
		fileread++;
		fileall++;
		break;
	      }
	  }
      if ((fileread > gl_var->max_readfiles && gl_var->max_readfiles > 0) ||
	  (filewrite > gl_var->max_writefiles && gl_var->max_writefiles > 0) ||
	  (fileall > gl_var->max_openfiles && gl_var->max_openfiles > 0))
	return SSH2_FX_PERMISSION_DENIED;
    }
  return SSH2_FX_OK;
}

void	ResolvPath(const char *path, char *dst, int dstMaxSize)
{
  const char	*s = path;
  char          *ptr;
  int           i, beg, end, len;

  dst[0] = '\0';
  beg = 0;
  len = strlen(path);
  STRCPY(dst, path, dstMaxSize);
  s = dst;
  while ((ptr = strstr(s, "..")) != NULL)
    {
      beg = len - strlen(ptr);
      end = beg + 2;
      if ((dst[beg - 1] == '/' || beg == 0)
	  && (dst[end] == '\0' || dst[end] == '/'))
        {
          while (beg >= 1 && dst[beg - 1] == '/')
            beg--;
          for (i = beg - 1; i >= 0; i--)
            if (dst[i] == '/' && (i == 0 || dst[i - 1] != '/'))
              break;
          if (i < 0) i = 0;
          if (dst[end] != '\0')
            STRCPY(dst + i, dst + end, dstMaxSize);
          else
	    dst[i] = '\0';
          len = strlen(dst);
        }
      else
        s = ptr + 2;
    }
  if (dst[0] == '\0')
    {
      if (path[0] == '/')
        dst[0] = path[0];
      else
        dst[0] = '.';
      dst[1] = '\0';
    }
  len = strlen(dst);
  if (len >= 2 && dst[len - 2] == '/' && dst[len - 1] == '.')
    dst[len - 1] = '\0';
  else if (len >= 1 && dst[len - 1] != '/')
    {
      struct stat       st;

      if (stat(dst, &st) != -1 && (st.st_mode & S_IFMT) != S_IFREG)
        STRCAT(dst, "/", dstMaxSize);
    }
}
