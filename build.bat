@echo off
if not exist build mkdir build
pushd build
cl ../src/meta/bob_the_builder.c -I ../src/base -I ../libs /FC /Z7 /nologo || exit /b 1
bob_the_builder.exe %*
popd
