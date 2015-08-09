// cp_vt.c 
//
// Copyright (c) 2006, Mike Acton <macton@cellperformance.com>
//
// Modified for compilation on SPU by Jonathan Adamczewski <jonathan@brnz.org>
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without
// limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial
// portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
// EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.

// NOTES:
// From http://www.linuxjournal.com/article/2597
//
//     "Console ttys are used when the keyboard and monitor are directly connected to the system without running
//     the X Window System. Since you can have several virtual consoles, the devices are tty0 through tty63. In
//     theory you can have 64 virtual consoles, but most people use only a few. The device /dev/console is
//     identical to tty0 and is needed for historical reasons. If your system lets you log in on consoles 1
//     through 6, then when you run X Windows System, X uses console 7, so you'll need /dev/tty1 through /dev/
//     tty7 on your system. I recommend having files up through /dev/tty12. For more information on using
//     virtual consoles, see the article Keyboards, Consoles and VT Cruising by John Fisk in the November 1996
//     issue of Linux Journal"

// suppress a warning...
#define __BIG_ENDIAN__

#ifdef DEBUG
#include <stdio.h>
#include <stdint.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include "cp_vt.h"

#include <spu_mfcio.h>

#ifdef DEBUG
static inline const char*
select_error_str( int existing_error, const char* const existing_error_str, int new_error, const char* const new_error_str )
{
  // Only report the first error found - any error that follows is probably just a cascading effect.
  const char* error_str = (char*)( (~(intptr_t)existing_error & (intptr_t)new_error & (intptr_t)new_error_str)
                                 |  ((intptr_t)existing_error & (intptr_t)existing_error_str) );

  return (error_str);
}
#endif

int ioctl_eaddr(unsigned int d, unsigned int request, void* lsa, void* ea, int size);

int
cp_vt_open_graphics(cp_vt* restrict vt, void* space)
{
  // structure some space to use for ioctl results
  struct cp_vt_store {
	struct vt_stat vts __attribute__((aligned(16)));
	int tty_ndx __attribute__((aligned(16)));
	int prev_kdmode __attribute__((aligned(16)));
  } __attribute__((aligned(16)));

  // commit to a limit of 128 bytes of space
  { int asserter[128-sizeof(struct cp_vt_store)]; (void)asserter; }

  struct cp_vt_store *store = (struct cp_vt_store*)space;

#ifdef DEBUG
  const char*    error_str      = NULL;
  int            error          = 0;
#endif

  // Open the current tty
 
  // From http://tldp.org/HOWTO/Text-Terminal-HOWTO-6.html#ss6.3
  // (An excellent overview by David S. Lawyer)
  //
  //     "In Linux the PC monitor is usually called the console and has several device special files associated
  //     with it: vc/0 (tty0), vc/1 (tty1), vc/2 (tty2), etc. When you log in you are on vc/1. To go to vc/2
  //     (on the same screen) press down the 2 keys Alt(left)-F3. For vc/3 use Left Alt-F3, etc. These (vc/1,
  //     vc/2, vc/3, etc.) are called "virtual terminals". vc/0 (tty0) is just an alias for the current virtual
  //     terminal and it's where messages from the system are sent. Thus messages from the system will be seen
  //     on the console (monitor) regardless of which virtual terminal it is displaying."

  const int   cur_tty                = open( "/dev/tty0", O_RDWR );
#ifdef DEBUG
  const int   open_cur_tty_error     = (cur_tty >> ((sizeof(int)*8)-1));
  const char* open_cur_tty_error_str = "Could not open /dev/tty0. Check permissions.";

  error_str = select_error_str( error, error_str, open_cur_tty_error, open_cur_tty_error_str );
  error     = error | open_cur_tty_error;
#endif

  // From: http://www.linuxjournal.com/article/2783
  // (A little out of date, but a nice primer.)
  //
  //     "VT_GETSTATE returns the state of all VT's in the kernel in the structure:
  //
  //         struct vt_stat {
  //               ushort v_active; 
  //               ushort v_signal; 
  //               ushort v_state;
  //          };
  //
  //      v_active        the currently active VT 
  //      v_state         mask of all the opened VT's
  //
  //      v_active holds the number of the active VT (starting from 1), while v_state 
  //      holds a mask where there is a 1 for each VT that has been opened by some process. 
  //      Note that VT 0 is always opened in this scenario, since it refers to the current VT.
  //
  //      Bugs:
  //      The v_signal member is unsupported."

  struct vt_stat  vts;

  const int   get_state_error     = ioctl_eaddr( cur_tty, VT_GETSTATE, &vts, &store->vts, (sizeof(vts)+15)&~0xf);

#ifdef DEBUG
  const char* get_state_error_str = "VT_GETSTATE failed on /dev/tty0";

  error_str = select_error_str( error, error_str, get_state_error, get_state_error_str );
  error     = error | get_state_error;
#endif

  vt->prev_tty_ndx = vts.v_active;

  // From: http://opensolaris.org/os/project/vconsole/vt.7i.txt
  // (Close enough to Linux and a pretty good source of documentation.)
  //
  // "VT_OPENQRY
  //     This call is used to find an available VT.    The argu-
  //     ment to the    ioctl is a pointer to an integer.  The integer
  //     will be filled in with the number of the first avail-
  //     able VT that no other process has open (and    hence, is
  //     available to be opened).  If there are no available
  //     VTs, then -1 will be filled in."

  const int   open_query_error     = ioctl_eaddr( cur_tty, VT_OPENQRY, &vt->tty_ndx, &store->tty_ndx, 4);
  
#ifdef DEBUG
  const char* open_query_error_str = "No open ttys available";

  error_str = select_error_str( error, error_str, open_query_error, open_query_error_str );
  error     = error | open_query_error;
#endif

  const int   close_cur_tty_error     = close( cur_tty );
#ifdef DEBUG
  const char* close_cur_tty_error_str = "Could not close parent tty";

  error_str = select_error_str( error, error_str, close_cur_tty_error, close_cur_tty_error_str );
  error     = error | close_cur_tty_error;
#endif

  char tty_file_name[11] = "/dev/tty\0\0";
  tty_file_name[8] = '0' + (vt->tty_ndx >= 10 ? 1 : vt->tty_ndx);
  tty_file_name[9] = '0' + vt->tty_ndx % 10;
  
  const int   tty                = open( tty_file_name, O_RDWR );
#ifdef DEBUG
  const int   open_tty_error     = (cur_tty >> ((sizeof(int)*8)-1));
  const char* open_tty_error_str = "Could not open tty";

  error_str = select_error_str( error, error_str, open_tty_error, open_tty_error_str );
  error     = error | open_tty_error;
#endif

  vt->tty = tty;

  // From: http://opensolaris.org/os/project/vconsole/vt.7i.txt
  // (Close enough to Linux and a pretty good source of documentation.)
  //
  // "VT_ACTIVATE
  //    This call has the effect of making the VT specified in
  //    the argument the active VT. The VT manager will cause
  //    a switch to occur in the same manner as if a hotkey had
  //    initiated the switch.  If the specified VT is not open
  //    or does not exist the call will fail and errno will be
  //    set to ENXIO."
  //
  // "VT_WAITACTIVE
  //    If the specified VT is already active, this call
  //    returns immediately. Otherwise, it will sleep until
  //    the specified VT becomes active, at which point it will
  //    return."


  const int   activate_tty_error     = ioctl( vt->tty, VT_ACTIVATE, vt->tty_ndx );
#ifdef DEBUG
  const char* activate_tty_error_str = "Could not activate tty";

  error_str = select_error_str( error, error_str, activate_tty_error, activate_tty_error_str );
  error     = error | activate_tty_error;
#endif

  const int   waitactive_tty_error     = ioctl( vt->tty, VT_WAITACTIVE, vt->tty_ndx );
#ifdef DEBUG
  const char* waitactive_tty_error_str = "Could not switch to tty";

  error_str = select_error_str( error, error_str, waitactive_tty_error, waitactive_tty_error_str );
  error     = error | waitactive_tty_error;
#endif

  // From: http://opensolaris.org/os/project/vconsole/vt.7i.txt
  // (Close enough to Linux and a pretty good source of documentation.)
  //
  //  "KDSETMODE
  //   This call is used to set the text/graphics mode to the VT.
  //
  //      KD_TEXT indicates that console text will be displayed on the screen
  //      with this VT. Normally KD_TEXT is combined with VT_AUTO mode for
  //      text console terminals, so that the console text display will
  //      automatically be saved and restored on the hot key screen switches.
  //
  //      KD_GRAPHICS indicates that the user/application, usually Xserver,
  //      will have direct control of the display for this VT in graphics
  //      mode. Normally KD_GRAPHICS is combined with VT_PROCESS mode for
  //      this VT indicating direct control of the display in graphics mode.
  //      In this mode, all writes to this VT using the write system call are
  //      ignored, and the user is responsible for saving and restoring the
  //      display on the hot key screen switches."

  // Save the current VT mode. This is most likely KD_TEXT.

  const int   kdgetmode_error     = ioctl_eaddr( vt->tty, KDGETMODE, &vt->prev_kdmode, &store->prev_kdmode, 4 );

#ifdef DEBUG
  const char* kdgetmode_error_str = "Could not get mode for tty";

  error_str = select_error_str( error, error_str, kdgetmode_error, kdgetmode_error_str );
  error     = error | kdgetmode_error;
#endif

  // Set VT to GRAPHICS (user draw) mode

  const int   kdsetmode_graphics_error     = ioctl( vt->tty, KDSETMODE, KD_GRAPHICS );
#ifdef DEBUG
  const char* kdsetmode_graphics_error_str = "Could not set graphics mode for tty";

  error_str = select_error_str( error, error_str, kdsetmode_graphics_error, kdsetmode_graphics_error_str );
  error     = error | kdsetmode_graphics_error;
#endif

  //
  // Not bothering with VT_PROCESS, VT_AUTO is fine for our purposes.
  //
 
  // If vt blanking is active, for example when running this program from a remote terminal, 
  // setting KD_GRAPHICS will not disable the blanking. Reset to KD_TEXT from KD_GRAPHICS will
  // force disable blanking. Then return to KD_GRAPHICS for drawing.
  //
  // Note: KD_TEXT (default) to KD_TEXT will do nothing, so blanking will not be disable unless
  // the mode is changing. i.e. the initial set to KD_GRAPHICS above is useful.

  const int   kdsetmode_text_error     = ioctl( vt->tty, KDSETMODE, KD_TEXT );
#ifdef DEBUG
  const char* kdsetmode_text_error_str = "Could not set text mode for tty";

  error_str = select_error_str( error, error_str, kdsetmode_text_error, kdsetmode_text_error_str );
  error     = error | kdsetmode_text_error;
#endif

  const int   kdsetmode_graphics_reset_error     = ioctl( vt->tty, KDSETMODE, KD_GRAPHICS );
#ifdef DEBUG
  const char* kdsetmode_graphics_reset_error_str = "Could not reset graphics mode for tty";

  error_str = select_error_str( error, error_str, kdsetmode_graphics_reset_error, kdsetmode_graphics_reset_error_str );
  error     = error | kdsetmode_graphics_reset_error;

  if ( error == -1 )
  {
      printf("ERROR: vt_graphics_open: %s\n",error_str);
      return (-1);
  }
#endif

  return (0);
}

int
cp_vt_close(cp_vt* restrict vt)
{
#ifdef DEBUG
  const char*    error_str      = NULL;
  int            error          = 0;
#endif

  // Reset previous mode on tty (likely KD_TEXT)

  const int   kdsetmode_error     = ioctl( vt->tty, KDSETMODE, vt->prev_kdmode );
#ifdef DEBUG
  const char* kdsetmode_error_str = "Could not reset previous mode for tty";

  error_str = select_error_str( error, error_str, kdsetmode_error, kdsetmode_error_str );
  error     = error | kdsetmode_error;
#endif

  // Restore previous tty

  const int   activate_tty_error     = ioctl( vt->tty, VT_ACTIVATE, vt->prev_tty_ndx );
#ifdef DEBUG
  const char* activate_tty_error_str = "Could not activate previous tty";

  error_str = select_error_str( error, error_str, activate_tty_error, activate_tty_error_str );
  error     = error | activate_tty_error;
#endif

  const int   waitactive_tty_error     = ioctl( vt->tty, VT_WAITACTIVE, vt->prev_tty_ndx );
#ifdef DEBUG
  const char* waitactive_tty_error_str = "Could not switch to previous tty";

  error_str = select_error_str( error, error_str, waitactive_tty_error, waitactive_tty_error_str );
  error     = error | waitactive_tty_error;
#endif

  // Close tty

  const int   close_tty_error     = close( vt->tty );
#ifdef DEBUG
  const char* close_tty_error_str = "Could not close tty";

  error_str = select_error_str( error, error_str, close_tty_error, close_tty_error_str );
  error     = error | close_tty_error;

  if ( error == -1 )
  {
      printf("ERROR: vt_close: %s\n",error_str);
      return (-1);
  }
#endif

  return (0);
}
