
Debian
====================
This directory contains files used to package zyrkd/zyrk-qt
for Debian-based Linux systems. If you compile zyrkd/zyrk-qt yourself, there are some useful files here.

## zyrk: URI support ##


zyrk-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install zyrk-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your zyrk-qt binary to `/usr/bin`
and the `../../share/pixmaps/zyrk128.png` to `/usr/share/pixmaps`

zyrk-qt.protocol (KDE)

