# cGIF
just another GIF decoder/encoder[TODO] written in C<br/>
Written with VS2013.

# Solution Structure

Folder                        | Content
------------------------------| ---
__cGIF__                      | the cGIF library (now compiled as a static lib)
__cGIFTest__                  | some unit test
__cGIF_ToString__             | a tool that print a GIF file content as string (a win32 console application)
__ViewViaGDI_Static__         | a demo program that can render a static GIF file with GDI (a win32 window application)
__ViewViaDirect3D9_Static__   | a demo program that can render a static GIF file with Direct3D 9 (a win32 window application)<br/>
__ViewViaOpenGL4_Static__     | a demo program that can render a static GIF file with OpenGL4 (a win32 window application)
__ViewViaGDI_Dynamic__        | a demo program that can render a dynamic GIF file with GDI (a win32 window application)
__GIFSample__                 | some GIF files that used as test materials

# TODO
Write some demos that render a dynamic GIF image with Direct3D 9 and OpenGL4.(Working on..)
Write the GIF encoder.

# LICENSE

__Apache 2.0__  
cGIF is licensed under the Apache 2.0 license.

   Copyright 2015 Zou Wei(zwcloud@yeah.net)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

See the LICENSE file for more information.
