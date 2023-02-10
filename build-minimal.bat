em++ src\BatChase.cpp -o build\BatChase.html --js-library src\LibBatChase.js ^
-std=c++20 -lGL -Wall -Wextra -Wpedantic -Wshadow --closure=1 -Oz ^
-sMIN_WEBGL_VERSION=2 -sMINIMAL_RUNTIME=1 -sVERBOSE=1 -sABORTING_MALLOC=0 ^
-sGL_TRACK_ERRORS=0 -sGL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=0 -sTEXTDECODER=2 -sENVIRONMENT=web
