-- XMOS MPlayer Modification--

MPlayer is an open source movie player availiable under the GNU General Public License version 2.

The homepage of the project is http://www.mplayerhq.hu.

XMOS have modified this project to add support for streaming of raw video over the UDP protocol.

--Download and Patch Instructions--

The MPlayer directory contains a snapshot of the MPlayer SVN source tree which has been patched with the XMOS UDP modification.

To update and patch a newer version of player, follow these instructions:
- Download the MPlayer source from a snapshot on www.mplayerhq.hu or using SVN:
- "svn checkout svn://svn.mplayerhq.hu/mplayer/trunk mplayer"
- Patch the MPlayer source by changing directory to the newly created 'mplayer' directory, and executing "patch -p1 -i ../patch/xudp.patch".

--Build Instructions--

-Windows-

To build MPlayer on Windows, MinGW is required. Download and install MinGW and MSYS from "http://www.mingw.org".

-All Platforms-

cd src
./configure --enable-xudp
make -j 3

--Directory Structure--

->bin               
-->macos            MPlayer binary for MAC OS
-->microsoft        MPlayer binary for Windows
->patch             Patch file for MPlayer source tree
->src               Patched MPlayer Snapshot