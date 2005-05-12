/*
MySecureShell permit to add restriction to modified sftp-server
when using MySecureShell as shell.
Copyright (C) 2004 Sebastien Tardif

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "defines.h"
#include "ip.h"
#include "parsing.h"
#include "string.h"
#include "user.h"

void	load_config(char verbose)
{
  if (!init_user_info())
    {
      if (verbose) printf("[ERROR]Error when fetching user informations\n");
      exit (2);
    }
  hash_set("SERVER_PORT", (void *)get_port_server(), 0);
  hash_set("SERVER_IP", get_ip_server(), 1);
  if (!load_config_file(CONFIG_FILE, verbose, 10))
    if (!load_config_file(CONFIG_FILE2, verbose, 10) && verbose)
      {
	printf("[ERROR]No valid config file were found.\nPlease correct this.\n");
	exit (2);
      }
  free_user_info();
  if (verbose)
    {
      char	*ptr;
      int	r, r2;

      printf("--- %s ---\n", (char *)hash_get("User"));
      printf("Home\t\t\t= %s\n", (char *)hash_get("Home"));
      printf("ByPassGlobalDownload\t= %s\n", (int )hash_get("ByPassGlobalDownload") == 0 ? "false" : "true");
      printf("ByPassGlobalUpload\t= %s\n", (int )hash_get("ByPassGlobalUpload") == 0 ? "false" : "true");
      printf("GlobalDownload\t\t= %i bytes/s\n", (int )hash_get("GlobalDownload"));
      printf("GlobalUpload\t\t= %i bytes/s\n", (int )hash_get("GlobalUpload"));
      printf("Download\t\t= %i bytes/s\n", (int )hash_get("Download"));
      printf("Upload\t\t\t= %i bytes/s\n", (int )hash_get("Upload"));
      printf("StayAtHome\t\t= %s\n", (int )hash_get("StayAtHome") == 0 ? "false" : "true");
      printf("VirtualChroot\t\t= %s\n", (int )hash_get("VirtualChroot") == 0 ? "false" : "true");
      printf("LimitConnection\t\t= %i\n", (int )hash_get("LimitConnection"));
      printf("LimitConnectionByUser\t= %i\n", (int )hash_get("LimitConnectionByUser"));
      printf("LimitConnectionByIP\t= %i\n", (int )hash_get("LimitConnectionByIP"));
      printf("IdleTimeOut\t\t= %is\n", (int )hash_get("IdleTimeOut"));
      printf("ResolveIP\t\t= %s\n", (int )hash_get("ResolveIP") == 0 ? "false" : "true");
      printf("DirFakeUser\t\t= %s\n", (int )hash_get("DirFakeUser") == 0 ? "false" : "true");
      printf("DirFakeGroup\t\t= %s\n", (int )hash_get("DirFakeGroup") == 0 ? "false" : "true");
      r = (int )hash_get("DirFakeMode");
      printf("DirFakeMode\t\t= %i%i%i%i\n", r / (8 * 8 * 8), (r / ( 8 * 8)) % 8, (r / 8) % 8, r % 8);
      ptr = (char *)hash_get("HideFiles");
      printf("HideFiles\t\t= %s\n", ptr ? ptr : "{nothing to hide}");
      printf("HideNoAccess\t\t= %s\n", (int )hash_get("HideNoAccess") == 0 ? "false" : "true");
      printf("MaxOpenFilesForUser\t= %i\n", (int )hash_get("MaxOpenFilesForUser"));
      printf("MaxReadFilesForUser\t= %i\n", (int )hash_get("MaxReadFilesForUser"));
      printf("MaxWriteFilesForUser\t= %i\n", (int )hash_get("MaxWriteFilesForUser"));
      printf("PathDenyFilter\t\t= %s\n", (char *)hash_get("PathDenyFilter"));
      ptr = (char *)hash_get("Shell");
      printf("Shell\t\t\t= %s\n", ptr ? ptr : "{no shell}");
      printf("ShowLinksAsLinks\t= %s\n", (int )hash_get("ShowLinksAsLinks") == 0 ? "false" : "true");
      r = (int )hash_get("DefaultRightsFile");
      r2 = (int )hash_get("DefaultRightsDirectory");
      printf("DefaultRights\t\t= %i%i%i%i %i%i%i%i\n",
	     r / (8 * 8 * 8), (r / ( 8 * 8)) % 8, (r / 8) % 8, r % 8,
	     r2 / (8 * 8 * 8), (r2 / ( 8 * 8)) % 8, (r2 / 8) % 8, r2 % 8);
    }
}

char	*convert_str_with_resolv_env_to_str(char *str)
{
  char	*env_var, *env_str, *new;
  int	beg, end;
  int	i, max;

  str = strdup(str);
  max = strlen(str);
  for (i = 0; i < max; i++)
    if (str[i] == '$')
      {
	beg = i + 1;
	while (i < max)
	  {
	    i++;
	    if (!((str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z')
		  || (str[i] >= '0' && str[i] <= '9') || (str[i] == '_')))
	      break;
	  }
	end = i;
	env_str = malloc(end - beg + 1);
	strncpy(env_str, str + beg, end - beg);
	if ((env_var = getenv(env_str)))
	  {
	    new = malloc(strlen(str) - (end - beg) + strlen(env_var) + 1);

	    strncpy(new, str, beg - 1);
	    strcat(new, env_var);
	    strcat(new, str + end);
	    free(str);
	    str = new;
	    i = 0;
	  }
	free(env_str);
      }
  return (str);
}

int	convert_boolean_to_int(char *str)
{
  if (str)
    if (!strcasecmp(str, "true") || !strcmp(str, "1"))
      return (1);
  return (0);
}

int	convert_speed_to_int(char **tb)
{
  char	*str;
  int	nb = 0;
  int	div = 0;
  int	i, j;
  int	*ptr = &nb;
  int	len = 1;

  for (j = 0; tb[j]; j++)
    {
      str = tb[j];
      for (i = 0; str[i]; i++)
	{
	  if (str[i] >= '0' && str[i] <= '9')
	    {
	      *ptr = *ptr * 10 + (str[i] - '0');
	      len *= 10;
	    }
	  else
	    switch (str[i])
	      {
	      case 'k':
	      case 'K':
		return (nb * 1024 + div * (1024 / len));
		
	      case 'm':
	      case 'M':
		return (nb * 1024 * 1024 + div * ((1024 * 1024) / len));
		
	      case '.':
		ptr = &div;
		len = 1;
		break;
	      }
	}
    }
  return (nb);
}

int     convert_mode_to_int(char *str)
{
  int	i;
  int	r;

  r = 0;
  for (i = 0; str[i]; i++)
    r = (r * 8) + (str[i] - '0');
  return (r);
}

int	load_config_file(char *file, char verbose, int max_recursive_left)
{
  FILE	*fh;
  char	buffer[1024];
  char	**tb, *str;
  int	len, line, err1, err2;

  if (!max_recursive_left)
    {
      if (verbose)
	printf("[ERROR]Too much inclusions !!!\n");
      return (0);
    }
  if ((fh = fopen(file, "r")))
    {
      line = 0;
      while (fgets(buffer, sizeof(buffer), fh))
	{
	  line++;
	  if ((str = clean_buffer(buffer)))
	    {
	      len = strlen(str) - 1;
	      if (*str == '<')
		{
		  if (str[len] == '>')
		    {
		      parse_tag(str);
		      if (parse_opened_tag < 0)
			{
			  if (verbose) printf("[ERROR]Too much tag closed at line %i in file '%s'!\n", line, file);
			  exit (2);
			}
		    }
		  else
		    {
		      if (verbose) printf("[ERROR]Error parsing line %i is not valid in file '%s'!\n", line, file);
		      exit (2);
		    }
		}
	      else if ((tb = parse_cut_string(str))) 
		{
		  if (tb[0])
		    {
		      err1 = 0;
		      if (is_for_user((char *)hash_get("USER"), verbose)
			  || is_for_group((char *)hash_get("GROUP"), verbose)
			  || is_for_rangeip((char *)hash_get("RANGEIP"), verbose)
			  || is_for_virtualhost((char *)hash_get("SERVER_IP"),
			  						(int )hash_get("SERVER_PORT"),
			  						verbose)
			  || (int)hash_get("DEFAULT") == 1)
			{
			  if (!strcmp(tb[0], "GlobalDownload") && tb[1])
			    hash_set(tb[0], (void *)convert_speed_to_int(tb + 1), 0);
			  else if (!strcmp(tb[0], "GlobalUpload") && tb[1])
			    hash_set(tb[0], (void *)convert_speed_to_int(tb + 1), 0);
			  else if (!strcmp(tb[0], "Download") && tb[1])
			    hash_set(tb[0], (void *)convert_speed_to_int(tb + 1), 0);
			  else if (!strcmp(tb[0], "Upload") && tb[1])
			    hash_set(tb[0], (void *)convert_speed_to_int(tb + 1), 0);
			  else if (!strcmp(tb[0], "StayAtHome") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "VirtualChroot") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "LimitConnection") && tb[1])
			    hash_set(tb[0], (void *)atoi(tb[1]), 0);
			  else if (!strcmp(tb[0], "LimitConnectionByUser") && tb[1])
			    hash_set(tb[0], (void *)atoi(tb[1]), 0);
			  else if (!strcmp(tb[0], "LimitConnectionByIP") && tb[1])
			    hash_set(tb[0], (void *)atoi(tb[1]), 0);
			  else if (!strcmp(tb[0], "Home") && tb[1])
			    hash_set(tb[0], (void *)convert_str_with_resolv_env_to_str(tb[1]), 1);
			  else if (!strcmp(tb[0], "Shell") && tb[1])
			    hash_set(tb[0], (void *)strdup(tb[1]), 1);
			  else if (!strcmp(tb[0], "ResolveIP") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "IdleTimeOut") && tb[1])
			    hash_set(tb[0], (void *)atoi(tb[1]), 0);
			  else if (!strcmp(tb[0], "IgnoreHidden") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "DirFakeUser") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "DirFakeGroup") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "DirFakeMode") && tb[1])
			    hash_set(tb[0], (void *)convert_mode_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "HideFiles"))
			    hash_set(tb[0], (void *)(tb[1] ? strdup(tb[1]) : 0), 1);
			  else if (!strcmp(tb[0], "HideNoAccess") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "ByPassGlobalDownload") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "ByPassGlobalUpload") && tb[1])
			    hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "MaxOpenFilesForUser") && tb[1])
			    hash_set(tb[0], (void *)atoi(tb[1]), 0);
			  else if (!strcmp(tb[0], "MaxReadFilesForUser") && tb[1])
			    hash_set(tb[0], (void *)atoi(tb[1]), 0);
			  else if (!strcmp(tb[0], "MaxWriteFilesForUser") && tb[1])
			    hash_set(tb[0], (void *)atoi(tb[1]), 0);
			  else if (!strcmp(tb[0], "ShowLinksAsLinks") && tb[1])
                            hash_set(tb[0], (void *)convert_boolean_to_int(tb[1]), 0);
			  else if (!strcmp(tb[0], "DefaultRights") && tb[1])
			    {
			      hash_set("DefaultRightsFile", (void *)convert_mode_to_int(tb[1]), 0);
			      if (tb[2])
				hash_set("DefaultRightsDirectory", (void *)convert_mode_to_int(tb[2]), 0);
			    }
			  else if (!strcmp(tb[0], "PathDenyFilter") && tb[1])
			    hash_set(tb[0], (void *)strdup(tb[1]), 1);
			  else
			    err1 = 1;
			}
		      err2 = 0;
		      if (!strcmp(tb[0], "Include") && tb[1])
			load_config_file(tb[1], verbose, max_recursive_left - 1);
		      else
			err2 = 1;

		      if (verbose && err1 && err2)
			printf("Property '%s' is not recognized !\n", tb[0]);
		    }
		  free(tb);
		}
	    }
	}
      if (parse_opened_tag != 0)
	{
	  if (verbose) printf("[ERROR]Missing %i close(s) tag(s) in file '%s'!!!\n", parse_opened_tag, file);
	  exit (2);
	}
      fclose(fh);
    }
  else
    {
      if (verbose && strcmp(file, CONFIG_FILE))
	printf("[ERROR]Couldn't load config file '%s'. Error : %s\n", file, strerror(errno));
      return (0);
    }
  return (1);
}
