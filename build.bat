@echo off
SETLOCAL
SET VisualStudio=cl
SET VSFlags=/W1 /O2 /Zi /FeTetra.exe
SET VSIncludes=-I..\Tetrahedron\Src\Dependencies\include
SET VSLinkerFlags=/link /Profile

SET GlobalLibs=user32.lib glu32.lib opengl32.lib gdi32.lib xinput9_1_0.lib
SET LocalLibsInclude=-LIBPATH:..\Tetrahedron\Src\Dependencies\lib\OpenGL\
SET LocalLibs=glew32s.lib

pushd ..\build
	call %VisualStudio% %VSFlags% %VSIncludes% ..\Tetrahedron\Config.cpp %GlobalLibs% %VSLinkerFlags% -LIBPATH:..\Tetrahedron\Src\Dependencies\lib\OpenGL\ %LocalLibs%
popd
xcopy ..\build\Tetra.exe Tetra.exe /y /f