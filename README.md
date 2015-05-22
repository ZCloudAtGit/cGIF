# cGIF
just another GIF decoder/encoder[TODO] written in C<br/>
Written with VS2013.

#Solution Structure
Folder                        | Content
--------                      | ---
__cGIF__                      | the cGIF library (now compiled as a static lib)
__cGIFTest__                  | some unit test
__cGIF_ToString__             | a tool that print a GIF file content as string (a win32 console application)
__ViewViaGDI_Static__         | a demo program that can render a static GIF file with GDI (a win32 window application)
__ViewViaDirect3D9_Static__   | a demo program that can render a static GIF file with Direct3D 9 (a win32 window application)<br/>
__ViewViaOpenGL4_Static__     | a demo program that can render a static GIF file with OpenGL4 (a win32 window application)
__ViewViaGDI_Dynamic__        | a demo program that can render a dynamic GIF file with GDI (a win32 window application)
__GIFSample__                 | some GIF files that used as test materials

#TODO
Write some demos that render a dynamic GIF image with Direct3D 9 and OpenGL4.(Working on..)
Write the GIF encoder.

#LICENSE
   __GPLv3__  
   cGIF, a GIF decoder/encoder  
   Copyright (C) 2015  Zou Wei(zwcloud@yeah.net)
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

See the LICENSE file for more information.
