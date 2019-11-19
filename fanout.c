
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

int
main (int argc, char *argv[])
{
  int port = 7000;
  int listen_socket;
  struct sockaddr_in tcp_sock_info;
  fd_set clients, readable;

  if (argc > 1)
    port = atoi (argv[1]);

  signal (SIGPIPE, SIG_IGN);

  tcp_sock_info.sin_family = AF_INET;
  tcp_sock_info.sin_addr.s_addr = htonl (INADDR_ANY);
  tcp_sock_info.sin_port = htons (port);

  listen_socket = socket (PF_INET, SOCK_STREAM, 0);

  int reuse = 1;
  if (setsockopt
      (listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse)) < 0)
    {
      perror ("setsockopt");
      exit (EXIT_FAILURE);
    }

  if (bind
      (listen_socket, (struct sockaddr *) &tcp_sock_info,
       sizeof (tcp_sock_info)) == -1)
    {
      perror ("socket bind");
      exit (EXIT_FAILURE);
    }

  if (listen (listen_socket, 10) == -1)
    {
      perror ("socket listen");
      exit (EXIT_FAILURE);
    }

  printf ("Listening on TCP port %u\n", port);

  FD_ZERO (&clients);

  while (1)
    {
      FD_ZERO (&readable);
      FD_SET (STDIN_FILENO, &readable);
      FD_SET (listen_socket, &readable);

      if (select (FD_SETSIZE, &readable, NULL, NULL, NULL) == -1)
	{
	  perror ("select");
	  exit (EXIT_FAILURE);
	}

      /* new connection */
      if (FD_ISSET (listen_socket, &readable))
	{
	  struct sockaddr_in client_info;
	  socklen_t addrlen = sizeof (client_info);
	  int handle;
	  handle =
	    accept (listen_socket, (struct sockaddr *) &client_info, &addrlen);
	  if (handle == -1)
	    {
	      perror ("accept failed");
	    }
	  else if (handle > FD_SETSIZE)
	    {
	      printf ("Too many connections\n");
	      close (handle);
	    }
	  else
	    {
	      int flags;
	      int send_buf_size = 0;

	      /* make socket non-blocking */
	      flags = fcntl (handle, F_GETFL, 0);
	      fcntl (handle, F_SETFL, flags | O_NONBLOCK);

	      socklen_t optlen = sizeof (send_buf_size);
	      getsockopt (handle, SOL_SOCKET, SO_SNDBUF, &send_buf_size,
			  &optlen);

	      printf ("New client %d from %s:%hu (%d byte send buffer)\n",
		      handle, inet_ntoa (client_info.sin_addr),
		      ntohs (client_info.sin_port), send_buf_size);
	      FD_SET (handle, &clients);

	    }
	}

      /* read data on stdin and send to all client sockets */
      if (FD_ISSET (STDIN_FILENO, &readable))
	{
	  int b;
	  char buf[1024];

	  b = read (STDIN_FILENO, buf, 1024);
	  if (b < 0)
	    {
	      perror ("stdio read");
	      exit (EXIT_FAILURE);
	    }
	  else if (b == 0)
	    {
	      perror ("End of stdin stream");
	      exit (EXIT_SUCCESS);
	    }
	  else
	    {
	      int i;
	      for (i = 0; i < FD_SETSIZE; i++)
		{
		  if (FD_ISSET (i, &clients))
		    {
		      if (write (i, buf, b) < 0)
			{
			  /* disconnect client on any error */
			  /* (including EWOULDBLOCK) */
			  perror ("write");
			  close (i);
			  FD_CLR (i, &clients);
			  printf ("Client %d disconnected\n", i);
			}
		    }
		}
	    }

	}
    }

  return 0;
}
