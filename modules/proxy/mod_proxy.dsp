# Microsoft Developer Studio Project File - Name="ApacheModuleProxy" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=ApacheModuleProxy - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ApacheModuleProxy.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ApacheModuleProxy.mak" CFG="ApacheModuleProxy - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ApacheModuleProxy - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ApacheModuleProxy - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ApacheModuleProxy - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\ApacheMo"
# PROP BASE Intermediate_Dir ".\ApacheMo"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\lib\apr\include" /I "..\..\include" /I "..\..\os\win32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "SHARED_MODULE" /D "WIN32_LEAN_AND_MEAN" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 ApacheCore.lib aprlib.lib kernel32.lib ws2_32.lib mswsock.lib /nologo /subsystem:windows /dll /map /machine:I386 /libpath:"..\..\CoreR" /libpath:"..\..\lib\apr\Release" /base:@BaseAddr.ref,mod_proxy
# ADD LINK32 ApacheCore.lib aprlib.lib kernel32.lib ws2_32.lib mswsock.lib /nologo /subsystem:windows /dll /map /machine:I386 /libpath:"..\..\CoreR" /libpath:"..\..\lib\apr\Release" /base:@BaseAddr.ref,mod_proxy

!ELSEIF  "$(CFG)" == "ApacheModuleProxy - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\ApacheM0"
# PROP BASE Intermediate_Dir ".\ApacheM0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\..\lib\apr\include" /I "..\..\include" /I "..\..\os\win32" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "SHARED_MODULE" /D "WIN32_LEAN_AND_MEAN" /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 ApacheCore.lib aprlib.lib kernel32.lib ws2_32.lib mswsock.lib /nologo /subsystem:windows /dll /map /debug /machine:I386 /libpath:"..\..\CoreD" /libpath:"..\..\lib\apr\Debug" /base:@BaseAddr.ref,mod_proxy
# ADD LINK32 ApacheCore.lib aprlib.lib kernel32.lib ws2_32.lib mswsock.lib /nologo /subsystem:windows /dll /debug /machine:I386 /libpath:"..\..\CoreD" /libpath:"..\..\lib\apr\Debug" /base:@BaseAddr.ref,mod_proxy
# SUBTRACT LINK32 /incremental:no /map

!ENDIF 

# Begin Target

# Name "ApacheModuleProxy - Win32 Release"
# Name "ApacheModuleProxy - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\mod_proxy.c
# End Source File
# Begin Source File

SOURCE=.\proxy_cache.c
# End Source File
# Begin Source File

SOURCE=.\proxy_connect.c
# End Source File
# Begin Source File

SOURCE=.\proxy_ftp.c
# End Source File
# Begin Source File

SOURCE=.\proxy_http.c
# End Source File
# Begin Source File

SOURCE=.\proxy_util.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\mod_proxy.h
# End Source File
# End Group
# End Target
# End Project
