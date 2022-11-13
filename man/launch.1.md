---
title: LAUNCH
section: 1
header: User Manual
footer: helloSystem 0.8.0
date: November 13, 2022
---
# NAME
launch - Command line tool to launch applications in the helloDesktop desktop environment.

# SYNOPSIS
**launch** *application* [*arguments*]...

# DESCRIPTION
**launch** is used to launch applications from the command line, and from other applications
such as the Filer or the Menu. It determines the path of the application to be launched,
sets some environment variables, and starts a child process for the application.

**launch** tries to re-use existing application instances when possible, instead of
launching new ones.
If the application to be launched is an application bundle, no arguments have been supplied
to the application, a process is already running for an executable with the same path
as the executable of the application to be launched, and there are existing
windows for this process, then the existing windows are activated
instead of launching a new instance of the application.

If the application to be launched is an application bundle that is not already running, **launch** asks the Menu via D-Bus to show the name of the application while the application is being launched.

If the application cannot be found, cannot be launched, or exits with a return code other than 0,
**launch** displays a graphical error message on the screen.

# ARGUMENTS

The following environment variables get set on the child process:

**application** 
: the path of an executable, the filename of an executable on the $PATH, the path of an application bundle (.app, .AppDir, .AppImage), or the name of an application bundle

**arguments** 
: passed through to the launched application

# ENVIRONMENT
**LAUNCHED_EXECUTABLE** 
: The executable being executed, e.g., /System/Filer.app/Filer

**LAUNCHED_BUNDLE** 
: The executable being executed, e.g., /System/Filer.app

# EXAMPLES
**launch FeatherPad**
: Launches an application from an application bundle located at any location known to the launch database named FeatherPad that might end in .app, .AppDir, or .AppImage, or in .desktop as a fallback for legacy compatibility.

**launch FeatherPad.{app,AppDir,AppImage}**
: Launches an application from an application bundle located at any location known to the launch database named FeatherPad with the respective suffix.

**launch /Applications/FeatherPad**
: Launches an application from an application bundle located at /Applications named FeatherPad that might end in .app, .AppDir, or .AppImage, or in .desktop as a fallback for legacy compatibility.

**launch /Applications/FeatherPad.{app,AppDir,AppImage}**
: Launches an application from an application bundle located at /Applications named FeatherPad with the respective suffix.

**launch /usr/local/share/applications/featherpad.desktop**
: Launches an application from an application bundle located at /usr/local/share/applications/ named featherpad.desktop. This is provided as a fallback for legacy applications. Only a minimal subset of the XDG specifications is supported.

**launch featherpad**
: Launches an application from an executable on the $PATH given as the first argument.

**launch /usr/local/bin/featherpad**
: Launches an application from the path of an executable given as the first argument.

# AUTHORS
Written by Simon Peter for helloSystem.

# BUGS
Submit bug reports online at: <https://github.com/helloSystem/launch/issues>

# SEE ALSO
Full documentation and sources at: <https://github.com/helloSystem/launch>

